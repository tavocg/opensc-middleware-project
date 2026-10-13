/* Observe bounded public discovery. No response or authentication data;
 * the only command-field exception is the FID of a blocked ISO SELECT EF. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <winscard.h>

static int trace_fd = -1, stopped, selected_public_file, handle_set, pending_response;
static unsigned int events;
static unsigned int lifecycle_events;
static SCARDHANDLE trace_handle;
static pthread_mutex_t trace_mutex = PTHREAD_MUTEX_INITIALIZER;

__attribute__((constructor)) static void observer_init(void)
{
	const char *path = getenv("JCOP4_PUBLIC_DISCOVERY_TRACE");
	if (path)
		trace_fd = open(path, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
	if (trace_fd >= 0)
		dprintf(trace_fd, "format=public_discovery_headers_v3\n");
}

/* Dependency lookup works when this library is loaded as a PC/SC delegate.
 * Do not log reader names, handles, ATRs, input buffers or returned data. */
static void log_lifecycle(const char *name, LONG rv)
{
	pthread_mutex_lock(&trace_mutex);
	if (trace_fd >= 0 && lifecycle_events++ < 128)
		dprintf(trace_fd, "pcsc_call=%s\npcsc_result=%ld\n", name, (long)rv);
	pthread_mutex_unlock(&trace_mutex);
}

static void drop_card_context(void)
{
	pthread_mutex_lock(&trace_mutex);
	selected_public_file = pending_response = 0;
	pthread_mutex_unlock(&trace_mutex);
}

LONG SCardEstablishContext(DWORD scope, const void *reserved1, const void *reserved2,
		SCARDCONTEXT *context)
{
	typedef LONG (*fn)(DWORD, const void *, const void *, SCARDCONTEXT *);
	fn real = (fn)dlsym(RTLD_NEXT, "SCardEstablishContext");
	LONG rv = real ? real(scope, reserved1, reserved2, context) : SCARD_F_INTERNAL_ERROR;
	log_lifecycle("SCardEstablishContext", rv);
	return rv;
}

LONG SCardConnect(SCARDCONTEXT context, const char *reader, DWORD share,
		DWORD protocols, SCARDHANDLE *card, DWORD *protocol)
{
	typedef LONG (*fn)(SCARDCONTEXT, const char *, DWORD, DWORD, SCARDHANDLE *, DWORD *);
	fn real = (fn)dlsym(RTLD_NEXT, "SCardConnect");
	drop_card_context();
	LONG rv = real ? real(context, reader, share, protocols, card, protocol) : SCARD_F_INTERNAL_ERROR;
	log_lifecycle("SCardConnect", rv);
	return rv;
}

LONG SCardReconnect(SCARDHANDLE card, DWORD share, DWORD protocols, DWORD action,
		DWORD *protocol)
{
	typedef LONG (*fn)(SCARDHANDLE, DWORD, DWORD, DWORD, DWORD *);
	fn real = (fn)dlsym(RTLD_NEXT, "SCardReconnect");
	drop_card_context();
	LONG rv = real ? real(card, share, protocols, action, protocol) : SCARD_F_INTERNAL_ERROR;
	log_lifecycle("SCardReconnect", rv);
	return rv;
}

static int public_file(unsigned int fid)
{
	/* Public bootstrap files only; no certificates, credentials or key files. */
	return fid == 0x2F00 || fid == 0x2F01 || fid == 0x5031 || fid == 0x5032;
}

static int allowed_command(const unsigned char *cmd, DWORD len, int *next_file)
{
	size_t lc;
	*next_file = -1;
	if (!cmd || len < 4 || cmd[0] > 3)
		return 0; /* No secure messaging or proprietary CLA. */
	if (cmd[1] == 0xA4 && len >= 5) {
		lc = cmd[4];
		if (!lc || lc > 16 || (len != lc + 5 && len != lc + 6))
			return 0;
		if (cmd[2] == 4) {
			const unsigned char root_aid[] = { 0xA0, 0, 0, 0, 0, 0x3F, 0 };
			const unsigned char app_aid[] = { 0xE8, 0x28, 0xBD, 8, 0x0F, 0xA0, 0, 0, 3, 0x63, 0x64, 0x79, 0x6E, 0x69, 0x64 };
			if (!((lc == sizeof(root_aid) && !memcmp(cmd + 5, root_aid, lc)) ||
					(lc == sizeof(app_aid) && !memcmp(cmd + 5, app_aid, lc))))
				return 0;
			*next_file = 0; /* ISO DF-name selection; the AID is not logged. */
			return 1;
		}
		if ((cmd[2] == 0 || cmd[2] == 1 || cmd[2] == 2 || cmd[2] == 8 || cmd[2] == 9) && !(lc % 2)) {
			unsigned int fid = ((unsigned int)cmd[5 + lc - 2] << 8) | cmd[5 + lc - 1];
			if (!((lc == 2) || (lc == 4 && (cmd[2] == 8 || cmd[2] == 9) && cmd[5] == 0x3F && cmd[6] == 0)))
				return 0;
			if (fid != 0x3F00 && !public_file(fid))
				return 0;
			if ((cmd[2] == 1 && fid != 0x3F00) || (cmd[2] == 2 && fid == 0x3F00))
				return 0;
			*next_file = public_file(fid);
			return 1;
		}
		return 0;
	}
	if (cmd[1] == 0xCA)
		/* 0100 is ChipDoc applet-name discovery (ANSSI-CC-2018/07,
		 * section 1.2.4). Only the observed basic-channel case-2 command;
		 * its response remains private and is never logged. */
		return len == 5 && cmd[2] == 1 &&
				(cmd[3] == 0x16 || (cmd[0] == 0 && cmd[3] == 0));
	if (cmd[1] == 0xB0)
		return selected_public_file && len == 5 && !(cmd[2] & 0x80);
	if (cmd[1] == 0xB2)
		return selected_public_file && len == 5 && !(cmd[3] & 0xF8);
	if (cmd[1] == 0xC0)
		return pending_response && len == 5 && cmd[2] == 0 && cmd[3] == 0;
	return 0; /* Includes VERIFY, MSE, PSO, GENERAL AUTHENTICATE and writes. */
}

LONG SCardTransmit(SCARDHANDLE card, const SCARD_IO_REQUEST *send_pci,
		const unsigned char *cmd, DWORD len, SCARD_IO_REQUEST *recv_pci,
		unsigned char *response, DWORD *response_len)
{
	typedef LONG (*transmit_fn)(SCARDHANDLE, const SCARD_IO_REQUEST *, const unsigned char *,
			DWORD, SCARD_IO_REQUEST *, unsigned char *, DWORD *);
	transmit_fn transmit;
	LONG rv = SCARD_E_NOT_TRANSACTED;
	DWORD capacity = response_len ? *response_len : 0;
	int next_file = -1;
	unsigned int sw;
	pthread_mutex_lock(&trace_mutex);
	if (trace_fd < 0 || stopped)
		goto out;
	if (!handle_set) { trace_handle = card; handle_set = 1; }
	if (card != trace_handle || ++events > 128) {
		dprintf(trace_fd, "blocked=handle_or_event_limit\n");
		stopped = 1;
		goto out;
	}
	if (!allowed_command(cmd, len, &next_file)) {
		/* Header only except a validated, blocked ISO SELECT EF identifier.
		 * Never authentication values, Lc/Le/lengths, hashes or responses. */
		if (cmd && len >= 4)
			dprintf(trace_fd, "blocked_header=%02X%02X%02X%02X\n", cmd[0], cmd[1], cmd[2], cmd[3]);
		else
			dprintf(trace_fd, "blocked=invalid_command\n");
		if (cmd && len >= 4 && cmd[0] == 0 && cmd[1] == 0xA4 && cmd[2] == 2 && cmd[3] == 0) {
			if ((len == 7 || len == 8) && cmd[4] == 2)
				dprintf(trace_fd, "blocked_select_ef=%02X%02X\n", cmd[5], cmd[6]);
			else
				dprintf(trace_fd, "blocked_select_form=unsupported\n");
		}
		stopped = 1;
		goto out;
	}
	dprintf(trace_fd, "public_header=%02X%02X%02X%02X\n", cmd[0], cmd[1], cmd[2], cmd[3]);
	/* A failed/warning SELECT may still change card context: drop old permission. */
	if (next_file >= 0)
		selected_public_file = 0;
	transmit = (transmit_fn)dlsym(RTLD_NEXT, "SCardTransmit");
	if (!transmit) { stopped = 1; goto out; }
	rv = transmit(card, send_pci, cmd, len, recv_pci, response, response_len);
	dprintf(trace_fd, "transport=%ld\n", (long)rv);
	if (rv == SCARD_S_SUCCESS && response && response_len && *response_len >= 2 && *response_len <= capacity) {
		sw = ((unsigned int)response[*response_len - 2] << 8) | response[*response_len - 1];
		dprintf(trace_fd, "sw=%04X\n", sw);
		pending_response = (sw >> 8) == 0x61;
		if ((sw >> 8) == 0x69)
			stopped = 1;
		if (next_file >= 0 && (sw == 0x9000 || (sw >> 8) == 0x61))
			selected_public_file = next_file;
	} else {
		stopped = 1;
	}
out:
	if (rv != SCARD_S_SUCCESS && response_len)
		*response_len = 0;
	pthread_mutex_unlock(&trace_mutex);
	return rv;
}

LONG SCardControl(SCARDHANDLE card, DWORD code, const void *send, DWORD send_len,
		void *receive, DWORD receive_len, DWORD *returned)
{
	/* Reader IOCTLs can perform PIN-pad authentication: none are forwarded. */
	(void)card; (void)code; (void)send; (void)send_len; (void)receive; (void)receive_len;
	pthread_mutex_lock(&trace_mutex);
	if (trace_fd >= 0 && !stopped)
		dprintf(trace_fd, "blocked=reader_control\n");
	stopped = 1;
	if (returned) *returned = 0;
	pthread_mutex_unlock(&trace_mutex);
	return SCARD_E_UNSUPPORTED_FEATURE;
}

LONG SCardSetAttrib(SCARDHANDLE card, DWORD attribute, const unsigned char *data, DWORD len)
{
	(void)card; (void)attribute; (void)data; (void)len;
	pthread_mutex_lock(&trace_mutex);
	if (trace_fd >= 0 && !stopped)
		dprintf(trace_fd, "blocked=reader_attribute\n");
	stopped = 1;
	pthread_mutex_unlock(&trace_mutex);
	return SCARD_E_UNSUPPORTED_FEATURE;
}
