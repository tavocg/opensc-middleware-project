/* Observe bounded public discovery. Default log contains headers only.
 * An opt-in pcsc-spy stream captures only permitted SELECT 4141 control
 * information; never authentication buffers or that EF's contents.
 * A separate opt-in comparison permits bounded READ 4141 to Idopte only,
 * without recording its contents or forwarding authentication. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <winscard.h>
#include <reader.h>

static int trace_fd = -1, stopped, selected_public_file, selected_mf, selected_app, handle_set, pending_response;
static int select_spy_fd = -1;
static int compare_read4141;
static int comparison_atr_ok;
static unsigned int read4141_budget, read4141_offset, read4141_calls;
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
		dprintf(trace_fd, "format=public_discovery_headers_v6\n");
	path = getenv("JCOP4_DIAGNOSTIC_READ4141");
	if (path) {
		if (strcmp(path, "1") || trace_fd < 0) stopped = 1;
		else {
			compare_read4141 = 1;
			if (dprintf(trace_fd, "comparison=bounded_read4141_contents_omitted\n") < 0) stopped = 1;
		}
	}
	path = getenv("JCOP4_PUBLIC_SELECT_SPY_TRACE");
	if (path) {
		select_spy_fd = open(path, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
		if (select_spy_fd < 0 || trace_fd < 0 || compare_read4141) stopped = 1;
	}
}

/* A narrow, opt-in SCardTransmit stream accepted by pcsc-spy. Only the
 * already permitted SELECT 4141 is captured, never EF contents or blocked
 * commands. Handles and PCI fields are normalized placeholders. */
static int capture_select(const unsigned char *cmd, DWORD len,
		const unsigned char *response, DWORD response_len, LONG rv,
		const struct timeval *start)
{
	char entry[2048];
	size_t used = 0;
	struct timeval end;
	if (select_spy_fd < 0) return 0;
	if (!cmd || (len != 7 && len != 8) || memcmp(cmd, "\x00\xA4\x02\x00\x02\x41\x41", 7)) return 0;
	/* A control response is bounded separately from the caller's capacity. */
	if (rv != SCARD_S_SUCCESS || !response || response_len > 258) response_len = 0;
	gettimeofday(&end, NULL);
	used += snprintf(entry + used, sizeof(entry) - used,
		"0@>|%ld|%ld|SCardTransmit\n0@0x00000000\n0@0xFFFFFFFF\n0@0xFFFFFFFF\n0@0x%08lX\n0@",
		(long)start->tv_sec, (long)start->tv_usec, (unsigned long)len);
	for (DWORD i = 0; i < len; i++)
		used += snprintf(entry + used, sizeof(entry) - used, "%s%02X", i ? " " : "", cmd[i]);
	used += snprintf(entry + used, sizeof(entry) - used,
		"\n0@0xFFFFFFFF\n0@0xFFFFFFFF\n0@0x%08lX\n0@", (unsigned long)response_len);
	if (!response_len)
		used += snprintf(entry + used, sizeof(entry) - used, "NULL");
	for (DWORD i = 0; i < response_len; i++)
		used += snprintf(entry + used, sizeof(entry) - used, "%s%02X", i ? " " : "", response[i]);
	used += snprintf(entry + used, sizeof(entry) - used, "\n0@<|%ld|%ld|SCardTransmit|0x%08lX\n",
		(long)end.tv_sec, (long)end.tv_usec, (unsigned long)rv);
	return used >= sizeof(entry) || write(select_spy_fd, entry, used) != (ssize_t)used;
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
	selected_public_file = selected_mf = selected_app = pending_response = 0;
	comparison_atr_ok = 0;
	/* Do not continue an unknown-file read across a connection/reset boundary. */
	if (read4141_calls) {
		if (!stopped && trace_fd >= 0) dprintf(trace_fd, "blocked=read4141_connection_boundary\n");
		stopped = 1;
	}
	pthread_mutex_unlock(&trace_mutex);
}

static LONG check_comparison_atr(SCARDHANDLE card)
{
	typedef LONG (*attrib_fn)(SCARDHANDLE, DWORD, unsigned char *, DWORD *);
	const unsigned char known[] = { 0x3B, 0xD5, 0x18, 0xFF, 0x81, 0x91, 0xFE, 0x1F, 0xC3, 0x80, 0x73, 0xC8, 0x21, 0x10, 0x0A };
	unsigned char atr[64];
	DWORD size = sizeof(atr);
	attrib_fn get = (attrib_fn)dlsym(RTLD_NEXT, "SCardGetAttrib");
	LONG rv = get ? get(card, SCARD_ATTR_ATR_STRING, atr, &size) : SCARD_F_INTERNAL_ERROR;
	pthread_mutex_lock(&trace_mutex);
	comparison_atr_ok = rv == SCARD_S_SUCCESS && size == sizeof(known) && !memcmp(atr, known, sizeof(known));
	if (trace_fd >= 0) dprintf(trace_fd, "comparison_atr=%s\n", comparison_atr_ok ? "known_jcop4" : "unavailable_or_mismatch");
	if (!comparison_atr_ok) { stopped = 1; rv = SCARD_E_NOT_TRANSACTED; }
	pthread_mutex_unlock(&trace_mutex);
	return rv;
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
	if (compare_read4141 && !stopped && rv == SCARD_S_SUCCESS)
		rv = card ? check_comparison_atr(*card) : SCARD_F_INTERNAL_ERROR;
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
	if (compare_read4141 && !stopped && rv == SCARD_S_SUCCESS) rv = check_comparison_atr(card);
	log_lifecycle("SCardReconnect", rv);
	return rv;
}

static int public_file(unsigned int fid)
{
	/* Bootstrap files. D003 is handled separately with confirmed MF context. */
	return fid == 0x2F00 || fid == 0x2F01 || fid == 0x5031 || fid == 0x5032;
}

static unsigned int read4141_size(const unsigned char *cmd, DWORD len)
{
	if (!cmd || len < 5 || cmd[0] != 0 || cmd[1] != 0xB0 || cmd[2] & 0x80)
		return 0;
	if (len == 5) return cmd[4] ? cmd[4] : 256;
	if (len == 7 && !cmd[4]) return ((unsigned int)cmd[5] << 8) | cmd[6];
	return 0;
}

static int allowed_command(const unsigned char *cmd, DWORD len, int *next_file, int *next_mf, int *next_app)
{
	size_t lc;
	*next_file = -1;
	*next_mf = -1;
	*next_app = -1;
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
			*next_mf = cmd[0] == 0 && lc == sizeof(root_aid) && !memcmp(cmd + 5, root_aid, lc);
			*next_app = cmd[0] == 0 && lc == sizeof(app_aid) && !memcmp(cmd + 5, app_aid, lc);
			return 1;
		}
		if ((cmd[2] == 0 || cmd[2] == 1 || cmd[2] == 2 || cmd[2] == 8 || cmd[2] == 9) && !(lc % 2)) {
			unsigned int fid = ((unsigned int)cmd[5 + lc - 2] << 8) | cmd[5 + lc - 1];
			if (!((lc == 2) || (lc == 4 && (cmd[2] == 8 || cmd[2] == 9) && cmd[5] == 0x3F && cmd[6] == 0)))
				return 0;
			if (fid == 0x4141) {
				/* Hardware confirmed SELECT control data under this ADF. The
				 * EF remains unidentified. Only the distinct comparison mode
				 * grants a bounded B0 read; default stays SELECT-only. */
				if (!selected_app || cmd[0] != 0 || cmd[2] != 2 || cmd[3] != 0 || lc != 2)
					return 0;
				*next_file = compare_read4141 ? 3 : 0;
				*next_mf = 0;
				*next_app = 1;
				return 1;
			}
			if (fid == 0x4109) {
				/* Observed next SELECT after bounded READ 4141. Metadata
				 * independently confirmed; no content-read permission. */
				if (!compare_read4141 || !comparison_atr_ok || !selected_app ||
						selected_public_file != 3 || !read4141_calls ||
						cmd[0] != 0 || cmd[2] != 2 || cmd[3] != 0 || lc != 2)
					return 0;
				*next_file = 0;
				*next_mf = 0;
				*next_app = 1;
				return 1;
			}
			if (fid == 0xD003) {
				/* EF.SN precedent: ANS CPS4 section 3.1.2. Only the observed
				 * SELECT EF form under a confirmed MF; never record its data. */
				if (!selected_mf || cmd[0] != 0 || cmd[2] != 2 || cmd[3] != 0 || lc != 2)
					return 0;
				*next_file = 2; /* Transparent identification file: basic-channel B0 only. */
				*next_mf = 1;
				*next_app = 0;
				return 1;
			}
			if (fid != 0x3F00 && !public_file(fid))
				return 0;
			if ((cmd[2] == 1 && fid != 0x3F00) || (cmd[2] == 2 && fid == 0x3F00))
				return 0;
			*next_file = public_file(fid);
			*next_mf = cmd[0] == 0 && (fid == 0x3F00 || (lc == 4 && cmd[2] == 8) || selected_mf);
			*next_app = cmd[0] == 0 && lc == 2 && (cmd[2] == 0 || cmd[2] == 2) &&
					public_file(fid) && selected_app;
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
	if (cmd[1] == 0xB0 && selected_public_file == 3) {
		unsigned int size = read4141_size(cmd, len);
		unsigned int offset = ((unsigned int)cmd[2] << 8) | cmd[3];
		return compare_read4141 && comparison_atr_ok && selected_app && size && size <= 400 &&
				offset == read4141_offset && offset <= 400 && size <= 400 - offset &&
				read4141_budget <= 400 - size && read4141_calls < 2;
	}
	if (cmd[1] == 0xB0)
		return selected_public_file && len == 5 && !(cmd[2] & 0x80) &&
				(selected_public_file != 2 || cmd[0] == 0);
	if (cmd[1] == 0xB2)
		return selected_public_file == 1 && len == 5 && !(cmd[3] & 0xF8);
	if (cmd[1] == 0xC0)
		return selected_public_file != 3 && pending_response && len == 5 && cmd[2] == 0 && cmd[3] == 0 &&
				(selected_public_file != 2 || cmd[0] == 0);
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
	unsigned char bounded[402];
	DWORD bounded_size = sizeof(bounded);
	unsigned int read_size = 0;
	int diagnostic_read = 0;
	int next_file = -1, next_mf = -1, next_app = -1;
	unsigned int sw;
	struct timeval start;
	pthread_mutex_lock(&trace_mutex);
	if (trace_fd < 0 || stopped)
		goto out;
	if (!handle_set) { trace_handle = card; handle_set = 1; }
	if (card != trace_handle || ++events > 128) {
		dprintf(trace_fd, "blocked=handle_or_event_limit\n");
		stopped = 1;
		goto out;
	}
	if (!allowed_command(cmd, len, &next_file, &next_mf, &next_app)) {
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
		/* Recognize fixed public protocol identifiers without dumping an MSE
		 * buffer. Classification never authorizes authentication transport. */
		if (cmd && (len == 17 || len == 18) &&
				!memcmp(cmd, "\x00\x22\x41\xA4\x0C\x80\x0A\x04\x00\x7F\x00\x07\x02\x02\x03\x02", 16) &&
				(cmd[16] == 1 || cmd[16] == 2))
			dprintf(trace_fd, "blocked_mse_profile=%s\nblocked_mse_case=%s\n",
					cmd[16] == 1 ? "ca_ecdh_3des" : "ca_ecdh_aes128",
					len == 17 ? "short_case3" : "short_case4");
		stopped = 1;
		goto out;
	}
	diagnostic_read = selected_public_file == 3 && cmd[1] == 0xB0;
	if (diagnostic_read) {
		read_size = read4141_size(cmd, len);
		if (!response || !response_len || capacity < read_size + 2) {
			dprintf(trace_fd, "blocked=read4141_output_buffer\n");
			stopped = 1;
			goto out;
		}
		bounded_size = read_size + 2;
		read4141_budget += read_size;
		read4141_calls++;
		if (dprintf(trace_fd, "diagnostic_read=known_4141_contents_omitted\n") < 0) {
			stopped = 1;
			goto out;
		}
	}
	dprintf(trace_fd, "public_header=%02X%02X%02X%02X\n", cmd[0], cmd[1], cmd[2], cmd[3]);
	/* Fixed labels for known selections, never arbitrary payload values. */
	if (cmd[1] == 0xA4 && cmd[2] == 4 && cmd[0] == 0)
		dprintf(trace_fd, "public_select=%s\n", next_app ? "known_chipdoc_application" : "known_root_application");
	if (cmd[1] == 0xA4 && cmd[2] == 2 && cmd[4] == 2 && cmd[5] == 0x41 && cmd[6] == 0x41)
		dprintf(trace_fd, "public_select=%s\n", compare_read4141 ?
				"known_4141_bounded_read_comparison" : "known_4141_metadata_only");
	if (cmd[1] == 0xA4 && cmd[2] == 2 && cmd[4] == 2 && cmd[5] == 0x41 && cmd[6] == 0x09)
		dprintf(trace_fd, "public_select=known_4109_metadata_only\n");
	if (cmd[0] != 0) selected_app = 0;
	/* A failed/warning SELECT may still change card context: drop old permission. */
	if (next_file >= 0)
		selected_public_file = selected_mf = selected_app = 0;
	transmit = (transmit_fn)dlsym(RTLD_NEXT, "SCardTransmit");
	if (!transmit) { stopped = 1; goto out; }
	gettimeofday(&start, NULL);
	rv = transmit(card, send_pci, cmd, len, recv_pci,
			diagnostic_read ? bounded : response, diagnostic_read ? &bounded_size : response_len);
	if (diagnostic_read) {
		/* The unknown contents go to Idopte only, never a trace or spy. */
		if (rv == SCARD_S_SUCCESS && bounded_size >= 2 && bounded_size <= read_size + 2) {
			sw = ((unsigned int)bounded[bounded_size - 2] << 8) | bounded[bounded_size - 1];
			if (sw == 0x9000 || sw == 0x6282) {
				memcpy(response, bounded, bounded_size);
				*response_len = bounded_size;
				read4141_offset += bounded_size - 2;
			} else {
				dprintf(trace_fd, "read4141_transport=%ld\nread4141_sw=%04X\nblocked=read4141_status\n", (long)rv, sw);
				stopped = 1;
				rv = SCARD_E_NOT_TRANSACTED;
			}
		} else {
			stopped = 1;
			if (rv == SCARD_S_SUCCESS) rv = SCARD_E_INSUFFICIENT_BUFFER;
		}
	}
	dprintf(trace_fd, "transport=%ld\n", (long)rv);
	if (rv == SCARD_S_SUCCESS && response && response_len && *response_len >= 2 && *response_len <= capacity) {
		sw = ((unsigned int)response[*response_len - 2] << 8) | response[*response_len - 1];
		dprintf(trace_fd, "sw=%04X\n", sw);
		pending_response = (sw >> 8) == 0x61;
		if ((sw >> 8) == 0x69)
			stopped = 1;
		if (next_file >= 0 && (sw == 0x9000 || (sw >> 8) == 0x61)) {
			selected_public_file = next_file;
			selected_mf = next_mf;
			selected_app = sw == 0x9000 && next_app;
		}
	} else {
		stopped = 1;
	}
	if (capture_select(cmd, len, response,
			rv == SCARD_S_SUCCESS && response_len && *response_len <= capacity ? *response_len : 0, rv, &start)) {
		dprintf(trace_fd, "blocked=select_spy_write_failed\n");
		stopped = 1; rv = SCARD_E_NOT_TRANSACTED;
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
	typedef LONG (*control_fn)(SCARDHANDLE, DWORD, const void *, DWORD, void *, DWORD, DWORD *);
	unsigned char features[256];
	DWORD capacity = receive_len < sizeof(features) ? receive_len : sizeof(features), count = 0;
	LONG rv = SCARD_E_UNSUPPORTED_FEATURE;
	control_fn control;
	(void)send; /* The only forwarded operation always has NULL/zero input. */
	pthread_mutex_lock(&trace_mutex);
	if (returned) *returned = 0;
	if (trace_fd < 0 || stopped)
		goto out;
	if (!handle_set) { trace_handle = card; handle_set = 1; }
	if (card != trace_handle || ++events > 128) {
		dprintf(trace_fd, "blocked=handle_or_event_limit\n");
		stopped = 1;
		goto out;
	}
	/* PC/SC Part 10 capability discovery only. Returned feature IOCTLs never
	 * become permissions; PIN-pad, escape and all other controls stay blocked. */
	if (code != CM_IOCTL_GET_FEATURE_REQUEST || send_len != 0 || !receive || !capacity || !returned) {
		dprintf(trace_fd, "blocked=reader_control\nblocked_control_code=%08lX\n", (unsigned long)code);
		dprintf(trace_fd, "blocked_control_kind=%s\n", code == CM_IOCTL_GET_FEATURE_REQUEST ?
				(send_len == 0 ? "feature_request_empty" : "feature_request_unsupported") : "unknown");
		stopped = 1;
		goto out;
	}
	dprintf(trace_fd, "public_control_code=%08lX\n", (unsigned long)code);
	control = (control_fn)dlsym(RTLD_NEXT, "SCardControl");
	rv = control ? control(card, code, NULL, 0, features, capacity, &count) : SCARD_E_UNSUPPORTED_FEATURE;
	if (rv == SCARD_S_SUCCESS && count > capacity)
		rv = SCARD_E_INSUFFICIENT_BUFFER;
	dprintf(trace_fd, "control_transport=%ld\n", (long)rv);
	if (rv == SCARD_S_SUCCESS) {
		memcpy(receive, features, count);
		*returned = count;
	} else {
		stopped = 1;
	}
out:
	pthread_mutex_unlock(&trace_mutex);
	return rv;
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
