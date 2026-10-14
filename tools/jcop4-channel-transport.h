#ifndef JCOP4_CHANNEL_TRANSPORT_H
#define JCOP4_CHANNEL_TRANSPORT_H
#include "jcop4-channel-controls.h"
static int probe_transport_mf(sc_card_t *card, struct jcop4_sm_channel *channel)
{
	const unsigned char select[4] = {0,0xA4,0,0}, mf[2] = {0x3F,0};
	unsigned char ssc[16] = {0}, header[4], body[33], wire[42], response[291], plain[256];
	size_t size, written; unsigned int sw; int rv, result = 1; sc_apdu_t apdu;
	struct jcop4_sm_decode_report decoded;
	ssc[15] = 2;
	if (!probe_guard.transport_mode || !probe_guard.transport_channel_verified || probe_guard.phase != PROBE_READ || !probe_guard.called ||
		probe_guard.blocked || probe_interrupted || !probe_transport_ready(card) || card->reader->max_recv_size < 291 ||
		channel->phase != JCOP4_SM_IDLE || CRYPTO_memcmp(channel->keys.ssc, ssc, 16)) goto out;
	if (jcop4_sm_seal_policy(channel, select, mf, 2, 1, 256, header, body, sizeof(body), &size,
		JCOP4_SM_LE_TWO_BYTE_256) || size != 33) goto out;
	memcpy(wire, header, 4); memcpy(wire + 4, "\0\0\x21", 3); memcpy(wire + 7, body, 33);
	wire[40] = 1; wire[41] = 0x23;
	probe_phase_set(PROBE_TRANSPORT_MF); memcpy(probe_guard.expected, wire, 42); probe_guard.expected_size = 42;
	sc_format_apdu(card, &apdu, SC_APDU_CASE_4_EXT, 0xA4, 0, 0); apdu.cla = 0x0C;
	apdu.data = body; apdu.lc = apdu.datalen = 33; apdu.le = apdu.resplen = 291; apdu.resp = response;
	apdu.flags |= SC_APDU_FLAGS_NO_RETRY_WL | SC_APDU_FLAGS_NO_GET_RESP | SC_APDU_FLAGS_NO_SM;
	/* Experimental exact frame, not a declared capability of the card.
	 * Dispatch through our guarded reader operation; do not manufacture
	 * SC_CARD_CAP_APDU_EXT or modify generic driver's limits/cache/SM. The
	 * guard validates case/Le/Lc/header/body before the real backend formats
	 * this single extended APDU. No higher-layer auto retry/GET RESPONSE. */
	printf("transport_card_capability=not_inferred_or_modified\n");
	rv = card->reader->ops->transmit(card->reader, &apdu);
	OPENSSL_cleanse(probe_guard.expected, sizeof(probe_guard.expected)); probe_guard.expected_size = 0;
	printf("transport_mf_transport=%d\n", rv); if (rv || apdu.resplen > sizeof(response)) goto out;
	/* Keep raw physical envelope in memory only, capacity includes SW. */
	unsigned char envelope[293]; memcpy(envelope, response, apdu.resplen);
	envelope[apdu.resplen] = apdu.sw1; envelope[apdu.resplen + 1] = apdu.sw2;
	rv = jcop4_sm_open_report(channel, envelope, apdu.resplen + 2, plain, sizeof(plain), &written, &sw, &decoded);
	OPENSSL_cleanse(envelope, sizeof(envelope));
	printf("transport_mf_decode_result=%s\ntransport_mf_mac_check=%s\n", jcop4_sm_decode_name(decoded.result),
		decoded.mac == JCOP4_SM_MAC_VERIFIED ? "verified" : decoded.mac == JCOP4_SM_MAC_FAILED ? "failed" : "not_checked");
	if (rv) goto out;
	printf("transport_mf_sw=%04X\n", sw); if (sw != 0x9000) goto out;
	int control_ok = !jcop4_sm_mf_control(plain, written);
	printf("transport_mf_control_template=%s\ntransport_mf_control_result=%s\n", !written ? "absent" :
		plain[0] == 0x62 ? "fcp62" : plain[0] == 0x6F ? "fci6f" : "other_uninterpreted",
		control_ok ? "matches_mf_df" : "unrecognized_control_stop");
	if (!control_ok) goto out;
	result = 0;
out:
	printf("transport_mf_result=%s\n", result ? "not_confirmed_stop" : "extended_frame_authenticated_mf_no_pin");
	jcop4_sm_invalidate(channel); OPENSSL_cleanse(header, sizeof(header)); OPENSSL_cleanse(body, sizeof(body));
	OPENSSL_cleanse(wire, sizeof(wire)); OPENSSL_cleanse(response, sizeof(response)); OPENSSL_cleanse(plain, sizeof(plain));
	OPENSSL_cleanse(probe_guard.expected, sizeof(probe_guard.expected)); probe_guard.expected_size = 0;
	return result;
}
#endif
