/* One experimental RSA/SHA256 PSO after the already verified login path. */
#ifndef JCOP4_CHANNEL_SIGN_H
#define JCOP4_CHANNEL_SIGN_H
#include "jcop4-response-collector.h"

static int probe_signature_get(void *context, const unsigned char *wire, size_t size,
		unsigned char *response, size_t capacity, size_t *received)
{
	if (!probe_guard.sign_mode || !probe_guard.login_authenticated || !probe_guard.signature_mse_verified ||
		(probe_guard.phase != PROBE_SIGNATURE_PSO && probe_guard.phase != PROBE_SIGNATURE_GET) || !probe_guard.called ||
		(probe_guard.signature_pending_sw & 0xFF00) != 0x6100 || size != 5 || capacity != 258 ||
		memcmp(wire, "\0\xc0\0\0", 4)) return SC_ERROR_NOT_ALLOWED;
	probe_phase_set(PROBE_SIGNATURE_GET); memcpy(probe_guard.expected, wire, 5); probe_guard.expected_size = 5;
	int rv = probe_exchange(context, wire, size, response, capacity, received);
	OPENSSL_cleanse(probe_guard.expected, sizeof(probe_guard.expected)); probe_guard.expected_size = 0;
	return rv;
}
static int probe_sign_once(sc_card_t *card, struct jcop4_sm_channel *channel)
{
	const unsigned char mse_header[4] = {0,0x22,0x81,0xB6}, mse_data[4] = {0x91,2,0x28,0x91};
	const unsigned char pso_header[4] = {0,0x2A,0x9E,0x9A};
	unsigned char header[4], body[64], wire[70], first[293], collected[293], plain[256], expected_ssc[16] = {0};
	size_t body_size, wire_size, received, collected_size, plain_size; unsigned int sw; int rv, result = 1;
	expected_ssc[15] = 8;
	if (!probe_guard.sign_mode || !probe_guard.login_authenticated || probe_guard.phase != PROBE_LOGIN_VERIFY ||
		!probe_guard.called || probe_guard.blocked || probe_interrupted || !probe_transport_ready(card) ||
		channel->phase != JCOP4_SM_IDLE || CRYPTO_memcmp(channel->keys.ssc, expected_ssc, 16)) goto out;
	for (int step = 0; step < 2; step++) {
		const char *label = step ? "signature_pso" : "signature_mse";
		if (jcop4_sm_seal(channel, step ? pso_header : mse_header, step ? probe_signature.digest : mse_data,
			step ? 32 : 4, step, step ? 256 : 0, header, body, sizeof(body), &body_size)) goto out;
		if (body_size != (step ? 64u : 29u)) goto out;
		memcpy(wire, header, 4); wire[4] = (unsigned char)body_size; memcpy(wire + 5, body, body_size);
		wire_size = body_size + 6; wire[wire_size - 1] = 0;
		probe_phase_set(step ? PROBE_SIGNATURE_PSO : PROBE_SIGNATURE_MSE);
		memcpy(probe_guard.expected, wire, wire_size); probe_guard.expected_size = wire_size; received = 0;
		rv = probe_exchange(card, wire, wire_size, first, step ? sizeof(first) : 258, &received);
		OPENSSL_cleanse(probe_guard.expected, sizeof(probe_guard.expected)); probe_guard.expected_size = 0;
		printf("%s_transport=%d\n", label, rv); if (rv || received > sizeof(first)) goto out;
		const unsigned char *response = first; size_t response_size = received;
		if (step) {
			/* Envelope metadata only; these observations are not authenticated. */
			printf("signature_first_size=%zu\n", received);
			if (received >= 2) printf("signature_first_outer_sw=%04X\n", (unsigned int)first[received-2]*256 + first[received-1]);
			printf("signature_first_leading_object=%s\n", received <= 2 ? "absent" : first[0] == 0x87 ? "encrypted87" :
				first[0] == 0x85 ? "unsupported85" : first[0] == 0x99 ? "status99" : "other_uninterpreted");
			struct jcop4_response_report report;
			rv = jcop4_response_collect(first, received, JCOP4_RESPONSE_CLA_ZERO, 293, probe_signature_get, card,
				collected, sizeof(collected), &collected_size, &report);
			printf("signature_collection=%d\nsignature_get_count=%u\nsignature_collection_transport=%d\nsignature_outer_sw=%04X\n",
				report.result, report.get_count, report.transport, report.physical_sw);
			if (rv) goto out;
			response = collected; response_size = collected_size;
		}
		struct jcop4_sm_decode_report decoded;
		rv = jcop4_sm_open_report(channel, response, response_size, plain, sizeof(plain), &plain_size, &sw, &decoded);
		printf("%s_decode_result=%s\n%s_mac_check=%s\n", label, jcop4_sm_decode_name(decoded.result), label,
			decoded.mac == JCOP4_SM_MAC_VERIFIED ? "verified" : decoded.mac == JCOP4_SM_MAC_FAILED ? "failed" : "not_checked");
		if (rv) goto out;
		printf("%s_sw=%04X\n", label, sw);
		if (sw != 0x9000 || (!step && plain_size)) goto out;
		if (!step) probe_guard.signature_mse_verified = 1;
		if (step) {
			if (plain_size != 256 || jcop4_signature_verify(&probe_signature, plain, plain_size)) {
				printf("signature_verification=invalid_stop\n"); goto out;
			}
			printf("signature_verification=rsa_sha256_pkcs1_verified\n");
		}
		OPENSSL_cleanse(plain, sizeof(plain));
	}
	result = 0;
out:
	jcop4_sm_invalidate(channel); OPENSSL_cleanse(header, sizeof(header)); OPENSSL_cleanse(body, sizeof(body));
	OPENSSL_cleanse(wire, sizeof(wire)); OPENSSL_cleanse(first, sizeof(first)); OPENSSL_cleanse(collected, sizeof(collected));
	OPENSSL_cleanse(plain, sizeof(plain)); OPENSSL_cleanse(probe_guard.expected, sizeof(probe_guard.expected)); probe_guard.expected_size = 0;
	return result;
}
#endif
