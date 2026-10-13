/* Experimental contact-only operation, after validating the observed profile. */
#include "tools/util.h"

static int jcop4_set_signature_env(sc_card_t *card, FILE *report)
{
	sc_apdu_t apdu;
	const unsigned char mse[] = { 0x91, 0x02, 0x28, 0x91 };
	int rv;
	sc_format_apdu(card, &apdu, SC_APDU_CASE_3_SHORT, 0x22, 0x81, 0xB6);
	apdu.cla = 0;
	apdu.flags |= SC_APDU_FLAGS_NO_RETRY_WL | SC_APDU_FLAGS_NO_GET_RESP;
	apdu.data = mse;
	apdu.lc = apdu.datalen = sizeof(mse);
	rv = sc_transmit_apdu(card, &apdu);
	fprintf(report, "mse_transport=%d\nmse_sw=%02X%02X\n", rv, apdu.sw1, apdu.sw2);
	return rv < 0 || apdu.sw1 != 0x90 || apdu.sw2;
}

static int jcop4_sign_once_order(sc_card_t *card, const unsigned char digest[32],
		unsigned char signature[256], FILE *report, int verify_first)
{
	sc_apdu_t apdu;
	char *credential = NULL;
	size_t capacity = 0;
	int length, rv, result = 1, attempted = 0;
	unsigned int sw1, sw2;
	fprintf(report, "authentication_order=%s\n", verify_first ? "verify_before_mse" : "mse_before_verify");

	/* Query current availability without a credential before prompting. */
	sc_format_apdu(card, &apdu, SC_APDU_CASE_1, 0x20, 0, 1);
	apdu.cla = 0;
	apdu.flags |= SC_APDU_FLAGS_NO_RETRY_WL | SC_APDU_FLAGS_NO_GET_RESP;
	rv = sc_transmit_apdu(card, &apdu);
	fprintf(report, "status_transport=%d\nstatus_sw=%02X%02X\n", rv, apdu.sw1, apdu.sw2);
	if (rv < 0 || !((apdu.sw1 == 0x90 && !apdu.sw2) ||
			(apdu.sw1 == 0x63 && (apdu.sw2 & 0xF0) == 0xC0 && (apdu.sw2 & 0x0F))))
		goto out;
	if (!verify_first && jcop4_set_signature_env(card, report))
		goto out;
	fflush(report);
	printf("PIN de la JCOP4 (sin eco; un único intento): ");
	fflush(stdout);
	length = util_getpass(&credential, &capacity, stdin);
	/* The observed profile declares UTF-8, no padding, public bounds 6..14. */
	if (length < 6 || length > 14 || !credential) {
		fprintf(report, "verify_result=input_rejected_before_transmission\n");
		goto out;
	}
	sc_format_apdu(card, &apdu, SC_APDU_CASE_3_SHORT, 0x20, 0, 1);
	apdu.cla = 0;
	apdu.flags |= SC_APDU_FLAGS_NO_RETRY_WL | SC_APDU_FLAGS_NO_GET_RESP;
	apdu.data = (unsigned char *)credential;
	apdu.lc = apdu.datalen = (size_t)length;
	attempted = 1;
	rv = sc_transmit_apdu(card, &apdu);
	sw1 = apdu.sw1;
	sw2 = apdu.sw2;
	sc_mem_clear(credential, capacity);
	free(credential);
	credential = NULL;
	sc_mem_clear(&apdu, sizeof(apdu));
	/* Only fixed result metadata, never credential, actual length or hash. */
	fprintf(report, "verify_transport=%d\nverify_sw=%02X%02X\n", rv, sw1, sw2);
	if (rv < 0 || sw1 != 0x90 || sw2)
		goto out;
	if (verify_first && jcop4_set_signature_env(card, report))
		goto out;
	/* No file selection or metadata reads after successful VERIFY. */
	sc_format_apdu(card, &apdu, SC_APDU_CASE_4_SHORT, 0x2A, 0x9E, 0x9A);
	apdu.cla = 0;
	apdu.flags |= SC_APDU_FLAGS_NO_RETRY_WL;
	apdu.data = digest;
	apdu.lc = apdu.datalen = 32;
	apdu.resp = signature;
	apdu.resplen = apdu.le = 256;
	rv = sc_transmit_apdu(card, &apdu);
	fprintf(report, "pso_transport=%d\npso_sw=%02X%02X\nsignature_bytes=%zu\n", rv, apdu.sw1, apdu.sw2, apdu.resplen);
	if (rv == 0 && apdu.sw1 == 0x90 && !apdu.sw2 && apdu.resplen == 256)
		result = 0;
out:
	if (credential) {
		sc_mem_clear(credential, capacity);
		free(credential);
	}
	/* End authentication even on VERIFY/PSO failure; never replay the PIN. */
	if (attempted) {
		rv = sc_reset(card, 0);
		fprintf(report, "reset_result=%d\n", rv);
		if (rv < 0)
			result = 1;
	}
	fflush(report);
	return result;
}

static int jcop4_sign_once(sc_card_t *card, const unsigned char digest[32],
		unsigned char signature[256], FILE *report)
{
	return jcop4_sign_once_order(card, digest, signature, report, 0);
}
