/* Bounded experimental anonymous-channel diagnostic. Default binary never
 * accepts credentials; only the separate login binary compiles that path.
 * Uses the local OpenSC default driver and owns one connection/transaction. */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libopensc/opensc.h"
#include "libopensc/asn1.h"
#include "jcop4-dh-control.h"
#include "jcop4-channel-confirmation.h"
#ifdef JCOP4_SIGN_DIAGNOSTIC
#include "jcop4-signature.h"
static struct jcop4_signature probe_signature = {.directory_fd = -1};
#endif

static const unsigned char probe_atr[] = {0x3B,0xD5,0x18,0xFF,0x81,0x91,0xFE,0x1F,0xC3,0x80,0x73,0xC8,0x21,0x10,0x0A};
static const unsigned char probe_aid[] = {0xE8,0x28,0xBD,0x08,0x0F,0xA0,0,0,3,0x63,0x64,0x79,0x6E,0x69,0x64};
static volatile sig_atomic_t probe_interrupted;
enum probe_phase { PROBE_STOP, PROBE_DIR, PROBE_APP, PROBE_D004, PROBE_INITIAL, PROBE_MSE, PROBE_GA, PROBE_READ, PROBE_CONTEXT_MF, PROBE_PIN_QUERY,
	PROBE_LOGIN_VERIFY, PROBE_SIGNATURE_APP, PROBE_SIGNATURE_MSE, PROBE_SIGNATURE_PSO, PROBE_SIGNATURE_GET, PROBE_TRANSPORT_MF };
static struct {
	const struct sc_reader_operations *original;
	sc_reader_t *reader;
	enum probe_phase phase;
	unsigned int commands, binary_reads, next_record;
	int root, directory, blocked, called, ended;
	int context_mode, context_selected;
	int login_mode; unsigned int context_query_sw;
	int sign_mode, login_authenticated, signature_mse_verified;
	int signature_application_mode, signature_application_verified;
	int transport_mode, transport_channel_verified;
	unsigned int signature_pending_sw, signature_gets;
	unsigned char expected[70]; size_t expected_size;
} probe_guard;

static void probe_signal(int sig) { (void)sig; probe_interrupted = 1; }
static int probe_atr_matches(const struct sc_atr *atr)
{
	return atr->len == sizeof(probe_atr) && !memcmp(atr->value, probe_atr, sizeof(probe_atr));
}
static int probe_transport_ready(sc_card_t *card)
{
	return card && card->reader && card->reader == probe_guard.reader &&
		probe_atr_matches(&card->atr) && probe_atr_matches(&card->reader->atr) &&
		card->reader->active_protocol == SC_PROTO_T1 && card->reader->max_send_size >= 139 &&
		card->reader->max_recv_size >= 256 && card->max_send_size >= 139 && card->max_recv_size >= 256;
}
static int probe_apdu_allowed(sc_apdu_t *a)
{
	static const unsigned char mse[] = {0x80,0x0A,4,0,0x7F,0,7,2,2,3,2,2};
	int extended_mf = 0;
#ifdef JCOP4_TRANSPORT_DIAGNOSTIC
	extended_mf = probe_guard.transport_mode && probe_guard.phase == PROBE_TRANSPORT_MF;
#endif
	if (a->control || a->lc != a->datalen || (a->datalen && !a->data) || a->le > (extended_mf ? 291u : 256u) ||
		(a->flags & (SC_APDU_FLAGS_CHAINING | SC_APDU_FLAGS_SM_CHAINING))) return 0;
#ifdef JCOP4_TRANSPORT_DIAGNOSTIC
	if (extended_mf) {
		return probe_guard.transport_channel_verified && probe_guard.reader->max_recv_size >= 291 &&
			probe_guard.expected_size == 42 && a->cla == 0x0C && a->ins == 0xA4 && !a->p1 && !a->p2 &&
			a->cse == (SC_APDU_CASE_4_EXT) && a->lc == 33 && a->le == 291 && a->resp && a->resplen == 291 &&
			!memcmp(probe_guard.expected, "\x0c\xa4\0\0\0\0\x21", 7) &&
			!memcmp(probe_guard.expected + 40, "\x01\x23", 2) &&
			!memcmp(a->data, "\x87\x11\x01", 3) && !memcmp(a->data + 19, "\x97\x02\x01\0\x8e\x08", 6) &&
			!memcmp(a->data, probe_guard.expected + 7, 33);
	}
#endif
	if (probe_guard.phase == PROBE_DIR && a->cla == 0) {
		if (a->ins == 0xA4 && (a->cse == SC_APDU_CASE_3_SHORT || a->cse == SC_APDU_CASE_4_SHORT) &&
			(a->p2 == 0 || a->p2 == 0x0C)) {
			if (a->lc == 2 && !memcmp(a->data, "\x3f\0", 2) && a->p1 == 0) return 1;
			if (a->lc == 2 && !memcmp(a->data, "\x2f\0", 2) && a->p1 == 0 && probe_guard.root) return 1;
			if (a->lc == 2 && !memcmp(a->data, "\x2f\0", 2) && a->p1 == 2 && probe_guard.root) return 1;
			/* ISO default removes the leading MF from an absolute path.
			 * P1=08 still addresses exactly EF.DIR from MF, not a relative EF. */
			if (a->lc == 2 && !memcmp(a->data, "\x2f\0", 2) && a->p1 == 8) return 1;
			if (a->lc == 4 && !memcmp(a->data, "\x3f\0\x2f\0", 4) && a->p1 == 8) return 1;
		}
		if (probe_guard.directory && !probe_guard.ended && !a->lc && a->cse == SC_APDU_CASE_2_SHORT && a->le) {
			if (a->ins == 0xB0 && !a->p1 && !a->p2 && !probe_guard.binary_reads) return 1;
			if (a->ins == 0xB2 && a->p2 == 4 && a->p1 == probe_guard.next_record && a->p1 <= 15) return 1;
		}
	} else if (probe_guard.phase == PROBE_APP)
		return a->cla == 0 && a->ins == 0xA4 && a->p1 == 4 && a->p2 == 0x0C && a->le == 0 &&
			a->cse == SC_APDU_CASE_3_SHORT && a->lc == sizeof(probe_aid) && !memcmp(a->data, probe_aid, sizeof(probe_aid));
	else if (probe_guard.phase == PROBE_D004)
		return a->cla == 0 && a->ins == 0xA4 && a->p1 == 2 && a->p2 == 0 && a->le == 256 &&
			a->cse == SC_APDU_CASE_4_SHORT && a->lc == 2 && !memcmp(a->data, "\xd0\x04", 2);
	else if (probe_guard.phase == PROBE_INITIAL)
		return a->cla == 0 && a->ins == 0xB0 && !a->p1 && !a->p2 && !a->lc &&
			a->cse == SC_APDU_CASE_2_SHORT && a->le == 137;
	else if (probe_guard.phase == PROBE_MSE)
		return a->cla == 0 && a->ins == 0x22 && a->p1 == 0x41 && a->p2 == 0xA4 && !a->le &&
			a->cse == SC_APDU_CASE_3_SHORT && a->lc == sizeof(mse) && !memcmp(a->data, mse, sizeof(mse));
	else if (probe_guard.phase == PROBE_GA)
		return a->cla == 0x10 && a->ins == 0x86 && !a->p1 && !a->p2 && !a->le &&
			a->cse == SC_APDU_CASE_3_SHORT && a->lc == 139 && !memcmp(a->data, "\x7c\x81\x88\x80\x81\x85", 6) &&
			jcop4_p521_public_valid(a->data + 6, 133);
	else if (probe_guard.phase == PROBE_READ)
		return a->cla == 0x0C && a->ins == 0xB0 && !a->p1 && !a->p2 && a->le == (probe_guard.transport_mode ? 172u : 256u) &&
			a->cse == SC_APDU_CASE_4_SHORT && a->lc == 13 && !memcmp(a->data, "\x97\x01\x89\x8e\x08", 5);
	else if (probe_guard.context_mode && !probe_guard.transport_mode && (probe_guard.phase == PROBE_CONTEXT_MF || probe_guard.phase == PROBE_PIN_QUERY)) {
		/* Match the whole locally built frame, including ciphertext/MAC.
		 * The only plaintexts used by this mode are MF3F00 and empty VERIFY1. */
		int query = probe_guard.phase == PROBE_PIN_QUERY;
		if (!probe_guard.expected_size || (query && !probe_guard.context_selected) || a->cla != 0x0C ||
			a->ins != (query ? 0x20 : 0xA4) || a->p1 || a->p2 != (query ? 1 : 0) ||
			a->cse != SC_APDU_CASE_4_SHORT || a->le != 256 || a->lc != (query ? 10 : 32)) return 0;
		if (query ? memcmp(a->data, "\x8e\x08", 2) :
			(memcmp(a->data, "\x87\x11\x01", 3) || memcmp(a->data + 19, "\x97\x01\0\x8e\x08", 5))) return 0;
		return probe_guard.expected_size == a->lc + 6 &&
			!memcmp(probe_guard.expected, query ? "\x0c\x20\0\x01\x0a" : "\x0c\xa4\0\0\x20", 5) &&
			!memcmp(a->data, probe_guard.expected + 5, a->lc);
	}
#ifdef JCOP4_LOGIN_DIAGNOSTIC
	else if (probe_guard.login_mode && probe_guard.phase == PROBE_LOGIN_VERIFY) {
		return probe_guard.context_selected && probe_guard.context_query_sw == 0x63C7 && probe_guard.expected_size == 35 &&
			a->cla == 0x0C && a->ins == 0x20 && !a->p1 && a->p2 == 1 && a->cse == SC_APDU_CASE_4_SHORT && a->le == 256 &&
			a->lc == 29 && !memcmp(a->data, "\x87\x11\x01", 3) && !memcmp(a->data + 19, "\x8e\x08", 2) &&
			!memcmp(probe_guard.expected, "\x0c\x20\0\x01\x1d", 5) && !memcmp(a->data, probe_guard.expected + 5, 29);
	}
#endif
#ifdef JCOP4_SIGN_DIAGNOSTIC
	else if (probe_guard.sign_mode && probe_guard.signature_application_mode && probe_guard.login_authenticated &&
		probe_guard.phase == PROBE_SIGNATURE_APP) {
		return !probe_guard.signature_application_verified && probe_guard.expected_size == 35 &&
			!memcmp(probe_guard.expected, "\x0c\xa4\x04\x0c\x1d", 5) &&
			a->cla == 0x0C && a->ins == 0xA4 && a->p1 == 4 && a->p2 == 0x0C &&
			a->cse == SC_APDU_CASE_4_SHORT && a->le == 256 && a->lc == 29 &&
			!memcmp(a->data, "\x87\x11\x01", 3) && !memcmp(a->data + 19, "\x8e\x08", 2) &&
			!memcmp(probe_guard.expected + 5, a->data, 29);
	}
	else if (probe_guard.sign_mode && probe_guard.login_authenticated &&
		(probe_guard.phase == PROBE_SIGNATURE_MSE || probe_guard.phase == PROBE_SIGNATURE_PSO)) {
		int pso = probe_guard.phase == PROBE_SIGNATURE_PSO;
		return (!probe_guard.signature_application_mode || probe_guard.signature_application_verified) &&
			(!pso || probe_guard.signature_mse_verified) && probe_guard.expected_size == (pso ? 70u : 35u) &&
			!memcmp(probe_guard.expected, pso ? "\x0c\x2a\x9e\x9a\x40" : "\x0c\x22\x81\xb6\x1d", 5) &&
			a->cla == 0x0C && a->ins == (pso ? 0x2A : 0x22) &&
			a->p1 == (pso ? 0x9E : 0x81) && a->p2 == (pso ? 0x9A : 0xB6) && a->cse == SC_APDU_CASE_4_SHORT && a->le == 256 &&
			a->lc == (pso ? 64u : 29u) && !memcmp(a->data, pso ? "\x87\x31\x01" : "\x87\x11\x01", 3) &&
			(pso ? !memcmp(a->data + 51, "\x97\x01\0\x8e\x08", 5) : !memcmp(a->data + 19, "\x8e\x08", 2)) &&
			!memcmp(probe_guard.expected + 5, a->data, a->lc);
	} else if (probe_guard.sign_mode && probe_guard.login_authenticated && probe_guard.signature_mse_verified && probe_guard.phase == PROBE_SIGNATURE_GET) {
		unsigned int le = probe_guard.signature_pending_sw & 255; if (!le) le = 256;
		return (probe_guard.signature_pending_sw & 0xFF00) == 0x6100 && probe_guard.signature_gets < 3 &&
			probe_guard.expected_size == 5 && !a->cla && a->ins == 0xC0 && !a->p1 && !a->p2 &&
			a->cse == SC_APDU_CASE_2_SHORT && !a->lc && a->le == le &&
			!memcmp(probe_guard.expected, "\0\xc0\0\0", 4) && probe_guard.expected[4] == (unsigned char)le;
	}
#endif
	return 0;
}
static int probe_guard_transmit(sc_reader_t *reader, sc_apdu_t *apdu)
{
	int rv;
	if (reader != probe_guard.reader || probe_interrupted || probe_guard.blocked || probe_guard.commands >= 24 ||
		!probe_atr_matches(&reader->atr) || reader->active_protocol != SC_PROTO_T1 ||
		(probe_guard.phase != PROBE_DIR && probe_guard.called) || !probe_apdu_allowed(apdu)) {
		probe_guard.blocked = 1; return SC_ERROR_NOT_ALLOWED;
	}
	apdu->flags |= SC_APDU_FLAGS_NO_RETRY_WL | SC_APDU_FLAGS_NO_GET_RESP | SC_APDU_FLAGS_NO_SM;
	probe_guard.called = 1; probe_guard.commands++;
#ifdef JCOP4_SIGN_DIAGNOSTIC
	if (probe_guard.phase == PROBE_SIGNATURE_GET) probe_guard.signature_gets++;
#endif
	rv = probe_guard.original->transmit(reader, apdu);
	if (rv < 0) { probe_guard.blocked = 1; return rv; }
#ifdef JCOP4_SIGN_DIAGNOSTIC
	if (probe_guard.phase == PROBE_SIGNATURE_PSO || probe_guard.phase == PROBE_SIGNATURE_GET)
		probe_guard.signature_pending_sw = apdu->sw1 * 256 + apdu->sw2;
#endif
	if (probe_guard.phase == PROBE_DIR) {
		if (apdu->ins == 0xA4) {
			probe_guard.directory = 0;
			if (apdu->sw1 == 0x90 && apdu->sw2 == 0) {
				if (apdu->lc == 2 && apdu->data[0] == 0x3F) probe_guard.root = 1;
				else probe_guard.directory = 1;
			}
		} else if (apdu->ins == 0xB0) probe_guard.binary_reads++;
		else if (apdu->ins == 0xB2) probe_guard.next_record++;
		if ((apdu->ins == 0xB0 && apdu->sw1 == 0x62 && apdu->sw2 == 0x82) ||
			(apdu->ins == 0xB2 && apdu->sw1 == 0x6A && apdu->sw2 == 0x83)) probe_guard.ended = 1;
		else if (apdu->sw1 != 0x90 || apdu->sw2 != 0) probe_guard.blocked = 1;
	}
	return rv;
}
static void probe_phase_set(enum probe_phase phase) { probe_guard.phase = phase; probe_guard.called = 0; }
static int probe_exchange(void *context, const unsigned char *wire, size_t size,
	unsigned char *response, size_t capacity, size_t *received)
{
	sc_card_t *card = context; sc_apdu_t a; int rv, cse;
	if (!received) return SC_ERROR_INVALID_ARGUMENTS;
	*received = 0;
	int big_response = 0;
#ifdef JCOP4_SIGN_DIAGNOSTIC
	big_response = probe_guard.sign_mode && probe_guard.phase == PROBE_SIGNATURE_PSO && capacity == 293;
#endif
	if (!wire || !response || (capacity != 258 && !big_response) || !probe_transport_ready(card) || probe_guard.blocked || probe_interrupted)
		return SC_ERROR_NOT_ALLOWED;
	if (size == 17 && probe_guard.phase == PROBE_INITIAL) { probe_phase_set(PROBE_MSE); cse = SC_APDU_CASE_3_SHORT; }
	else if (size == 144 && probe_guard.phase == PROBE_MSE) { probe_phase_set(PROBE_GA); cse = SC_APDU_CASE_3_SHORT; }
	else if (size == 19 && probe_guard.phase == PROBE_GA) { probe_phase_set(PROBE_READ); cse = SC_APDU_CASE_4_SHORT; }
	else if (probe_guard.context_mode &&
		((probe_guard.phase == PROBE_CONTEXT_MF && size == 38) || (probe_guard.phase == PROBE_PIN_QUERY && size == 16)) &&
		probe_guard.expected_size == size && !memcmp(wire, probe_guard.expected, size)) cse = SC_APDU_CASE_4_SHORT;
#ifdef JCOP4_LOGIN_DIAGNOSTIC
	else if (probe_guard.login_mode && probe_guard.phase == PROBE_LOGIN_VERIFY && size == 35 &&
		probe_guard.expected_size == size && !memcmp(wire, probe_guard.expected, size)) cse = SC_APDU_CASE_4_SHORT;
#endif
#ifdef JCOP4_SIGN_DIAGNOSTIC
	else if (probe_guard.sign_mode &&
		((probe_guard.phase == PROBE_SIGNATURE_APP && size == 35) ||
		 (probe_guard.phase == PROBE_SIGNATURE_MSE && size == 35) || (probe_guard.phase == PROBE_SIGNATURE_PSO && size == 70)) &&
		probe_guard.expected_size == size && !memcmp(wire, probe_guard.expected, size)) cse = SC_APDU_CASE_4_SHORT;
	else if (probe_guard.sign_mode && probe_guard.phase == PROBE_SIGNATURE_GET && size == 5 &&
		probe_guard.expected_size == size && !memcmp(wire, probe_guard.expected, size)) cse = SC_APDU_CASE_2_SHORT;
#endif
	else { probe_guard.blocked = 1; return SC_ERROR_NOT_ALLOWED; }
	sc_format_apdu(card, &a, cse, wire[1], wire[2], wire[3]); a.cla = wire[0];
	if (cse != SC_APDU_CASE_2_SHORT) { a.data = wire + 5; a.lc = a.datalen = wire[4]; }
	if ((cse == SC_APDU_CASE_3_SHORT && size != a.lc + 5) ||
		(cse == SC_APDU_CASE_4_SHORT && (size != a.lc + 6 || wire[size - 1] !=
		 (probe_guard.transport_mode && probe_guard.phase == PROBE_READ ? 172 : 0)))) return SC_ERROR_NOT_ALLOWED;
	a.le = cse == SC_APDU_CASE_2_SHORT ? (wire[4] ? wire[4] : 256) : cse == SC_APDU_CASE_4_SHORT ?
		(probe_guard.transport_mode && probe_guard.phase == PROBE_READ ? 172 : 256) : 0;
	if (probe_guard.transport_mode && probe_guard.phase == PROBE_READ && wire[size-1] != 172) return SC_ERROR_NOT_ALLOWED;
	a.resp = response; a.resplen = cse == SC_APDU_CASE_2_SHORT ? a.le : capacity - 2;
	a.flags |= SC_APDU_FLAGS_NO_RETRY_WL | SC_APDU_FLAGS_NO_GET_RESP | SC_APDU_FLAGS_NO_SM;
	rv = sc_transmit_apdu(card, &a);
	if (rv < 0 || a.resplen > capacity - 2) { probe_guard.blocked = 1; return rv < 0 ? rv : SC_ERROR_INVALID_DATA; }
	response[a.resplen] = (unsigned char)a.sw1; response[a.resplen + 1] = (unsigned char)a.sw2;
	*received = a.resplen + 2; return 0;
}

/* Separate no-credential mode. Never copies the Idopte teardown/retry branch. */
#ifndef JCOP4_TRANSPORT_DIAGNOSTIC
static int probe_pin_context(sc_card_t *card, struct jcop4_sm_channel *channel)
{
	const unsigned char mf_header[4] = {0, 0xA4, 0, 0}, query_header[4] = {0, 0x20, 0, 1}, mf[2] = {0x3F, 0};
	unsigned char header[4] = {0}, body[32] = {0}, wire[38] = {0}, response[258] = {0}, plain[256] = {0};
	unsigned char expected_ssc[16] = {0}; size_t body_size, wire_size, received, plain_size;
	unsigned int sw; int rv, result = 1;
	if (!probe_guard.context_mode || probe_guard.phase != PROBE_READ || !probe_guard.called ||
		probe_guard.blocked || probe_interrupted || !probe_transport_ready(card) || channel->phase != JCOP4_SM_IDLE) goto out;
	expected_ssc[15] = 2;
	if (CRYPTO_memcmp(channel->keys.ssc, expected_ssc, 16)) goto out;
	for (int step = 0; step < 2; step++) {
		const char *label = step ? "pin_query" : "context_select";
		if (jcop4_sm_seal(channel, step ? query_header : mf_header, step ? NULL : mf, step ? 0 : 2,
			!step, step ? 0 : 256, header, body, sizeof(body), &body_size) || body_size != (step ? 10u : 32u)) goto out;
		memcpy(wire, header, 4); wire[4] = (unsigned char)body_size; memcpy(wire + 5, body, body_size);
		wire_size = body_size + 6; wire[wire_size - 1] = 0; /* Outer Le256 even for no-Le inner query. */
		probe_phase_set(step ? PROBE_PIN_QUERY : PROBE_CONTEXT_MF);
		memcpy(probe_guard.expected, wire, wire_size); probe_guard.expected_size = wire_size;
		received = 0; memset(response, 0, sizeof(response));
		rv = probe_exchange(card, wire, wire_size, response, sizeof(response), &received);
		OPENSSL_cleanse(probe_guard.expected, sizeof(probe_guard.expected)); probe_guard.expected_size = 0;
		printf("%s_transport=%d\n", label, rv); if (rv) goto out;
		plain_size = 0; sw = 0;
		if (received > sizeof(response)) { printf("%s_response=invalid_stop\n", label); goto out; }
		/* Bounded envelope metadata, explicitly unauthenticated. Never log
		 * unknown tag values, DO99 status or payload before successful decode. */
		printf("%s_response_size=%zu\n", label, received);
		if (received >= 2) printf("%s_outer_sw=%04X\n", label,
			(unsigned int)response[received - 2] * 256 + response[received - 1]);
		printf("%s_leading_object=%s\n", label, received <= 2 ? "absent" : response[0] == 0x87 ? "encrypted87" :
			response[0] == 0x85 ? "unsupported85" : response[0] == 0x99 ? "status99" : "other_uninterpreted");
		struct jcop4_sm_decode_report decoded;
		printf("%s_response_policy=%s\n", label, step ? "outer9000_or_mac_matching_63Cx" : "outer9000_only");
		rv = jcop4_sm_open_report_policy(channel, response, received, plain, sizeof(plain), &plain_size, &sw,
			step ? JCOP4_SM_OUTER_9000_OR_MATCHING_63CX : JCOP4_SM_OUTER_9000, &decoded);
		printf("%s_decode_result=%s\n%s_mac_check=%s\n", label, jcop4_sm_decode_name(decoded.result), label,
			decoded.mac == JCOP4_SM_MAC_VERIFIED ? "verified" : decoded.mac == JCOP4_SM_MAC_FAILED ? "failed" : "not_checked");
		if (rv) {
			printf("%s_response=invalid_stop\n", label); goto out;
		}
		printf("%s_response=authenticated\n%s_sw=%04X\n", label, label, sw);
		if (!step) {
			/* SELECT returns control data, which remain opaque and unlogged. */
			if (sw != 0x9000) goto out;
			probe_guard.context_selected = 1;
			printf("context_select_result=mf_selected_under_sm_no_login\n");
		} else {
			/* Empty VERIFY has no DO87/credential or inner Le. No replay, follow-up or prompt. */
			if (plain_size) { printf("pin_query_result=unexpected_data_stop\n"); goto out; }
			if (sw == 0x9000) printf("pin_query_result=status_9000_no_pin_supplied\n");
			else if ((sw & 0xFFF0) == 0x63C0 && (sw & 15)) printf("pin_query_result=available_unverified\n");
			else { printf("pin_query_result=blocked_or_unexpected_stop\n"); goto out; }
			probe_guard.context_query_sw = sw;
		}
		OPENSSL_cleanse(plain, sizeof(plain));
	}
	result = 0;
out:
	/* Only the separately compiled login diagnostic may retain SSC6. */
	if (result || !probe_guard.login_mode) jcop4_sm_invalidate(channel);
	OPENSSL_cleanse(header, sizeof(header)); OPENSSL_cleanse(body, sizeof(body));
	OPENSSL_cleanse(wire, sizeof(wire)); OPENSSL_cleanse(response, sizeof(response)); OPENSSL_cleanse(plain, sizeof(plain));
	OPENSSL_cleanse(probe_guard.expected, sizeof(probe_guard.expected)); probe_guard.expected_size = 0;
	printf("pin_context_result=%s\n", result ? "not_confirmed_stop" : "status_observed_no_pin_no_login");
	return result;
}
#endif

#ifdef JCOP4_LOGIN_DIAGNOSTIC
#include "jcop4-channel-login.h"
#endif
#ifdef JCOP4_SIGN_DIAGNOSTIC
#include "jcop4-channel-sign.h"
#endif
#ifdef JCOP4_TRANSPORT_DIAGNOSTIC
#include "jcop4-channel-transport.h"
#endif

int main(int argc, char **argv)
{
	sc_context_t *ctx = NULL; sc_reader_t *reader = NULL; sc_card_t *card = NULL;
	sc_context_param_t params; struct sc_reader_operations guarded_ops; sc_path_t path;
	unsigned char control[256] = {0}, initial[137] = {0}; sc_apdu_t a;
	struct jcop4_ca_probe_report report;
	struct jcop4_sm_channel confirmed = {{{0}, {0}, {0}}, JCOP4_SM_INVALID};
	const char *unsafe_env[] = {"OPENSC_API_TRACE_FILE", "OPENSC_DEBUG_FILE", "OPENSC_DEEPBIND", "LD_PRELOAD", "LD_AUDIT",
		"LIBPCSCLITE_DELEGATE", "LIBPCSCLITE_SPY_DELEGATE", "JCOP4_PUBLIC_DISCOVERY_TRACE", "JCOP4_PUBLIC_SELECT_SPY_TRACE"};
	unsigned long index; char *end; int rv, status = 1, locked = 0, session = 0, announced = 0;
	int context_mode = argc == 3 && !strcmp(argv[2], "--pin-context");
#ifdef JCOP4_TRANSPORT_DIAGNOSTIC
	int transport_mode = argc == 3 && !strcmp(argv[2], "--transport-extended");
	if (!transport_mode) return 2;
	context_mode = 1;
#endif
#ifdef JCOP4_LOGIN_DIAGNOSTIC
#ifdef JCOP4_SIGN_DIAGNOSTIC
	int signature_application_mode = argc == 5 && !strcmp(argv[2], "--sign-application-once");
	int sign_mode = signature_application_mode || (argc == 5 && !strcmp(argv[2], "--sign-once"));
	int login_mode = sign_mode;
#else
	int login_mode = argc == 3 && !strcmp(argv[2], "--login-once");
#endif
	if (!login_mode) return 2; /* Separate binary cannot silently run another mode. */
	context_mode = 1;
#endif
	if ((argc != 2 && !context_mode) || !argv[1][0] || strspn(argv[1], "0123456789") != strlen(argv[1])) return 2;
	errno = 0; index = strtoul(argv[1], &end, 10); if (errno || *end || index > UINT_MAX) return 2;
	for (size_t i = 0; i < sizeof(unsafe_env)/sizeof(unsafe_env[0]); i++) if (getenv(unsafe_env[i])) return 2;
	if (getenv("OPENSC_DEBUG") && strcmp(getenv("OPENSC_DEBUG"), "0")) return 2;
	if (!getenv("OPENSC_CONF")) return 2;
#ifdef JCOP4_SIGN_DIAGNOSTIC
	if (jcop4_signature_prepare(&probe_signature, argv[3], argv[4])) { printf("signature_certificate=invalid_stop\n"); return 1; }
	printf("signature_certificate=rsa2048_signing_public_key_loaded\n");
#endif
	memset(&probe_guard, 0, sizeof(probe_guard)); probe_guard.next_record = 1; probe_interrupted = 0;
	probe_guard.context_mode = context_mode;
#ifdef JCOP4_TRANSPORT_DIAGNOSTIC
	probe_guard.transport_mode = transport_mode;
#endif
#ifdef JCOP4_LOGIN_DIAGNOSTIC
	probe_guard.login_mode = login_mode;
#endif
#ifdef JCOP4_SIGN_DIAGNOSTIC
	probe_guard.sign_mode = sign_mode;
	probe_guard.signature_application_mode = signature_application_mode;
#endif
#ifdef JCOP4_LOGIN_DIAGNOSTIC
	/* Interrupt terminal read so TERM/INT can restore echo before closing. */
	struct sigaction action; memset(&action, 0, sizeof(action)); action.sa_handler = probe_signal;
	sigemptyset(&action.sa_mask);
	if (sigaction(SIGINT, &action, NULL) || sigaction(SIGTERM, &action, NULL)) goto out;
#else
	signal(SIGINT, probe_signal); signal(SIGTERM, probe_signal);
#endif
	memset(&params, 0, sizeof(params)); params.app_name = "jcop4-channel-probe";
	rv = sc_context_create(&ctx, &params); printf("context_result=%d\n", rv); if (rv < 0) goto out;
	ctx->debug = 0; ctx->flags |= SC_CTX_FLAG_ENABLE_DEFAULT_DRIVER;
	rv = sc_set_card_driver(ctx, "default"); printf("driver_result=%d\n", rv); if (rv < 0) goto out;
	reader = sc_ctx_get_reader(ctx, (unsigned int)index);
	if (!reader) { printf("reader_result=not_found\n"); goto out; }
	if (!reader->ops || !reader->ops->transmit || !reader->ops->reset) { printf("reader_result=unsupported_backend_stop\n"); goto out; }
	probe_guard.reader = reader; probe_guard.original = reader->ops; guarded_ops = *reader->ops;
	guarded_ops.transmit = probe_guard_transmit; reader->ops = &guarded_ops;
	rv = sc_connect_card(reader, &card); printf("connect_result=%d\n", rv); if (rv < 0) goto out;
	printf("atr_result=%s\n", probe_atr_matches(&card->atr) ? "known" : "mismatch_stop");
	if (!probe_transport_ready(card)) { printf("transport_policy=unsupported_stop\n"); goto out; }
	if (probe_guard.transport_mode && reader->max_recv_size < 291) { printf("transport_policy=reader_limit_below291_stop\n"); goto out; }
	printf("transport_policy=%s\n", probe_guard.transport_mode ? "candidate_t1_read172_mf_extended291" : "opensc_t1_short_max256");
	rv = sc_lock(card); printf("lock_result=%d\n", rv); if (rv < 0) goto out; locked = 1;
	rv = sc_reset(card, 0); printf("initial_reset_result=%d\n", rv); if (rv < 0) goto out; session = 1;
	if (!probe_transport_ready(card) || probe_interrupted || probe_guard.blocked) goto out;
	probe_phase_set(PROBE_DIR);
	rv = sc_enum_apps(card); printf("directory_result=%d\n", rv); if (rv < 0 || probe_guard.blocked) goto out;
	if (card->app_count < 0 || card->app_count > SC_MAX_CARD_APPS) goto out;
	for (int i = 0; i < card->app_count; i++) if (card->app[i] && card->app[i]->aid.len == sizeof(probe_aid) &&
		!memcmp(card->app[i]->aid.value, probe_aid, sizeof(probe_aid))) announced = 1;
	printf("application_announced=%d\n", announced); if (!announced) goto out;
	probe_phase_set(PROBE_APP); memset(&path, 0, sizeof(path)); path.type = SC_PATH_TYPE_DF_NAME;
	path.len = sizeof(probe_aid); memcpy(path.value, probe_aid, sizeof(probe_aid));
	rv = sc_select_file(card, &path, NULL); printf("application_select_result=%d\n", rv);
	if (rv < 0 || probe_guard.blocked || !probe_guard.called) goto out;
	probe_phase_set(PROBE_D004); sc_format_apdu(card, &a, SC_APDU_CASE_4_SHORT, 0xA4, 2, 0);
	a.cla = 0; a.data = (const unsigned char *)"\xd0\x04"; a.lc = a.datalen = 2;
	a.resp = control; a.le = a.resplen = sizeof(control);
	a.flags |= SC_APDU_FLAGS_NO_RETRY_WL | SC_APDU_FLAGS_NO_GET_RESP | SC_APDU_FLAGS_NO_SM;
	rv = sc_transmit_apdu(card, &a); printf("d004_select_transport=%d\nd004_select_sw=%02X%02X\n", rv, a.sw1, a.sw2);
	if (rv < 0 || a.sw1 != 0x90 || a.sw2 || !d004_control_matches_observation(control, a.resplen)) goto out;
	printf("d004_control_result=observed_identity_structure_size\n");
	probe_phase_set(PROBE_INITIAL); sc_format_apdu(card, &a, SC_APDU_CASE_2_SHORT, 0xB0, 0, 0);
	a.cla = 0; a.resp = initial; a.le = a.resplen = sizeof(initial);
	a.flags |= SC_APDU_FLAGS_NO_RETRY_WL | SC_APDU_FLAGS_NO_GET_RESP | SC_APDU_FLAGS_NO_SM;
	rv = sc_transmit_apdu(card, &a); printf("d004_read_transport=%d\nd004_read_sw=%02X%02X\n", rv, a.sw1, a.sw2);
	if (rv < 0 || a.sw1 != 0x90 || a.sw2 || a.resplen != sizeof(initial)) goto out;
	status = context_mode ? jcop4_ca_confirm_d004_session(initial, sizeof(initial),
		probe_guard.transport_mode ? JCOP4_CA_LE_BOUND : JCOP4_CA_LE_SHORT_MAX,
		probe_exchange, card, &report, &confirmed) :
		jcop4_ca_confirm_d004(initial, sizeof(initial), JCOP4_CA_LE_SHORT_MAX, probe_exchange, card, &report);
	printf("channel_stage=%d\nchannel_outcome=%d\nchannel_transport=%d\nchannel_sw=%04X\nchannel_authenticated_response=%d\n",
		report.stage, report.outcome, report.transport, report.sw, report.authenticated_response);
	if (report.authenticated_response && report.sw == 0x9000) {
		printf("channel_content_size=%zu\nchannel_content_size_equal=%d\n", report.content_size, report.content_size_equal);
		if (report.content_size_equal)
			printf("channel_content_first_block_equal=%d\nchannel_content_tail_equal=%d\n",
				report.content_first_block_equal, report.content_tail_equal);
	}
	printf("channel_result=%s\n", status ? "not_confirmed_stop" : "protected_d004_read_confirmed_no_login");
#ifdef JCOP4_TRANSPORT_DIAGNOSTIC
	if (!status) { probe_guard.transport_channel_verified = 1; status = probe_transport_mf(card, &confirmed); }
#else
	if (!status && context_mode) status = probe_pin_context(card, &confirmed);
#endif
#ifdef JCOP4_LOGIN_DIAGNOSTIC
	if (!status) status = probe_login_once(card, &confirmed);
#endif
#ifdef JCOP4_SIGN_DIAGNOSTIC
	if (!status) status = probe_sign_once(card, &confirmed);
#endif
out:
	probe_guard.phase = PROBE_STOP;
	jcop4_sm_invalidate(&confirmed); OPENSSL_cleanse(probe_guard.expected, sizeof(probe_guard.expected));
	probe_guard.expected_size = 0;
	OPENSSL_cleanse(control, sizeof(control)); OPENSSL_cleanse(initial, sizeof(initial));
	if (session) { rv = sc_reset(card, 0); printf("final_reset_result=%d\n", rv); if (rv < 0) status = 1; }
	if (locked) { rv = sc_unlock(card); printf("unlock_result=%d\n", rv); if (rv < 0) status = 1; }
	if (card) { rv = sc_disconnect_card(card); printf("disconnect_result=%d\n", rv); if (rv < 0) status = 1; }
	if (reader && probe_guard.original) reader->ops = probe_guard.original;
	if (ctx) sc_release_context(ctx);
#ifdef JCOP4_SIGN_DIAGNOSTIC
	if (!status && !probe_interrupted) status = jcop4_signature_save(&probe_signature);
	printf("signature_result=%s\n", status || probe_interrupted ? "not_saved_stop" : "rsa_sha256_verified_and_saved");
	jcop4_signature_clear(&probe_signature);
#endif
	printf("guard_blocked=%d\ncommand_count=%u\ninterrupted=%d\n", probe_guard.blocked, probe_guard.commands, !!probe_interrupted);
	return probe_interrupted ? 1 : status;
}
