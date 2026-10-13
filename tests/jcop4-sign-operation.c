/* Synthetic credentials and simulated APDUs only; no card or real PIN. */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libopensc/opensc.h"
#include "libopensc/pkcs15.h"
#define main card_sign_test_main
#include "../tools/card-sign-test.c"
#undef main

static int status_sw, mse_sw, verify_sw, pso_sw, fail_ins, reset_rv, short_signature;
static int step, prompts, verifies, psos, resets, wipes, rejected_input;
static int verify_first_fixture;
static void *credential_buffer;
static sc_pkcs15_object_t key_obj, pin_obj, cert_obj;
static sc_pkcs15_prkey_info_t key_info;
static sc_pkcs15_auth_info_t pin_info;
static sc_pkcs15_cert_info_t cert_info;
static int duplicate;

int sc_pkcs15_get_objects(sc_pkcs15_card_t *p15, unsigned int type, sc_pkcs15_object_t **objects, size_t count)
{
	(void)p15;
	assert(count == 32);
	objects[0] = type == SC_PKCS15_TYPE_PRKEY ? &key_obj : type == SC_PKCS15_TYPE_AUTH_PIN ? &pin_obj : &cert_obj;
	if (duplicate && type == SC_PKCS15_TYPE_AUTH_PIN) { objects[1] = &pin_obj; return 2; }
	return 1;
}
int util_getpass(char **value, size_t *capacity, FILE *stream)
{
	assert(stream == stdin && step == (verify_first_fixture ? 1 : 2) && !prompts);
	prompts++;
	*capacity = 128;
	*value = calloc(1, *capacity);
	assert(*value);
	strcpy(*value, rejected_input ? "x" : "fixture");
	credential_buffer = *value;
	putchar('\n');
	return rejected_input ? 1 : 7;
}
void sc_mem_clear(void *value, size_t size)
{
	if (value == credential_buffer) { assert(size == 128); wipes++; credential_buffer = NULL; }
	memset(value, 0, size);
}
int sc_reset(sc_card_t *card, int cold)
{
	(void)card;
	assert(!cold && verifies == 1 && !resets && wipes == 1);
	resets++;
	return reset_rv;
}
int sc_transmit_apdu(sc_card_t *card, sc_apdu_t *apdu)
{
	const unsigned char mse[] = { 0x91, 2, 0x28, 0x91 };
	(void)card;
	assert(apdu->cla == 0 && (apdu->flags & SC_APDU_FLAGS_NO_RETRY_WL));
	if (step == 0) {
		assert(apdu->ins == 0x20 && apdu->p1 == 0 && apdu->p2 == 1 && apdu->cse == SC_APDU_CASE_1 &&
				!apdu->data && !apdu->lc && !apdu->datalen && !apdu->le);
		apdu->sw1 = status_sw >> 8; apdu->sw2 = status_sw & 255;
	} else if (step == (verify_first_fixture ? 2 : 1)) {
		assert(apdu->ins == 0x22 && apdu->p1 == 0x81 && apdu->p2 == 0xB6 && apdu->lc == 4 &&
				apdu->datalen == 4 && !memcmp(apdu->data, mse, 4));
		assert(verify_first_fixture ? (prompts == 1 && verifies == 1 && wipes == 1) : !prompts);
		apdu->sw1 = mse_sw >> 8; apdu->sw2 = mse_sw & 255;
	} else if (step == (verify_first_fixture ? 1 : 2)) {
		assert(apdu->ins == 0x20 && apdu->p1 == 0 && apdu->p2 == 1 && apdu->lc == 7 &&
				apdu->datalen == 7 && !memcmp(apdu->data, "fixture", 7) && prompts == 1 && !verifies);
		verifies++;
		apdu->sw1 = verify_sw >> 8; apdu->sw2 = verify_sw & 255;
	} else {
		assert(step == 3 && apdu->ins == 0x2A && apdu->p1 == 0x9E && apdu->p2 == 0x9A &&
				apdu->lc == 32 && apdu->datalen == 32 && apdu->le == 256 && apdu->resplen == 256 &&
				wipes == 1 && !psos && !(apdu->flags & SC_APDU_FLAGS_NO_GET_RESP));
		psos++;
		memset(apdu->resp, 0xA5, 256);
		apdu->resplen = short_signature ? 255 : 256;
		apdu->sw1 = pso_sw >> 8; apdu->sw2 = pso_sw & 255;
	}
	if (step < 3) assert(apdu->flags & SC_APDU_FLAGS_NO_GET_RESP);
	step++;
	return fail_ins == step ? SC_ERROR_TRANSMIT_FAILED : 0;
}
static void reset_fixture(void)
{
	status_sw = 0x63C7; mse_sw = verify_sw = pso_sw = 0x9000;
	fail_ins = reset_rv = short_signature = 0;
	step = prompts = verifies = psos = resets = wipes = rejected_input = duplicate = 0;
	verify_first_fixture = 0;
	credential_buffer = NULL;
	memset(&key_obj, 0, sizeof(key_obj)); memset(&pin_obj, 0, sizeof(pin_obj)); memset(&cert_obj, 0, sizeof(cert_obj));
	memset(&key_info, 0, sizeof(key_info)); memset(&pin_info, 0, sizeof(pin_info)); memset(&cert_info, 0, sizeof(cert_info));
	key_obj.type = SC_PKCS15_TYPE_PRKEY_RSA; key_obj.data = &key_info;
	pin_obj.type = SC_PKCS15_TYPE_AUTH_PIN; pin_obj.data = &pin_info;
	cert_obj.type = SC_PKCS15_TYPE_CERT_X509; cert_obj.data = &cert_info;
	key_info.id.len = 12; memcpy(key_info.id.value, "LlaveDeFirma", 12);
	cert_info.id = key_info.id;
	key_obj.auth_id.len = 8; memcpy(key_obj.auth_id.value, "Card PIN", 8);
	pin_info.auth_id = key_obj.auth_id;
	key_info.native = 1; key_info.key_reference = 0x91; key_info.modulus_length = 2048; key_info.usage = SC_PKCS15_PRKEY_USAGE_SIGN;
	pin_info.auth_type = SC_PKCS15_PIN_AUTH_TYPE_PIN; pin_info.auth_method = SC_AC_CHV;
	pin_info.attrs.pin.reference = 1; pin_info.attrs.pin.flags = 0x11; pin_info.attrs.pin.type = SC_PKCS15_PIN_TYPE_UTF8;
	pin_info.attrs.pin.min_length = 6; pin_info.attrs.pin.max_length = 14;
}
static int run_operation(void)
{
	sc_card_t card = { 0 };
	unsigned char digest[32] = { 0 }, signature[256];
	char text[2048];
	FILE *report = tmpfile();
	int result;
	size_t size;
	assert(report);
	result = verify_first_fixture ? jcop4_sign_once_order(&card, digest, signature, report, 1) :
		jcop4_sign_once(&card, digest, signature, report);
	rewind(report);
	size = fread(text, 1, sizeof(text) - 1, report); text[size] = 0;
	assert(!strstr(text, "fixture") && !strstr(text, "pin_length") && !strstr(text, "credential") && !strstr(text, "hash"));
	assert(strstr(text, verify_first_fixture ? "authentication_order=verify_before_mse" : "authentication_order=mse_before_verify"));
	fclose(report);
	return result;
}
int main(void)
{
	sc_pkcs15_object_t *certificate;
	reset_fixture(); assert(!validate_metadata(NULL, &certificate) && certificate == &cert_obj);
	key_info.modulus_length = 3072; assert(validate_metadata(NULL, &certificate));
	reset_fixture(); pin_info.attrs.pin.type = SC_PKCS15_PIN_TYPE_BCD; assert(validate_metadata(NULL, &certificate));
	reset_fixture(); pin_info.attrs.pin.flags |= SC_PKCS15_PIN_FLAG_NEEDS_PADDING; assert(validate_metadata(NULL, &certificate));
	reset_fixture(); pin_info.auth_id.value[0] ^= 1; assert(validate_metadata(NULL, &certificate));
	reset_fixture(); duplicate = 1; assert(validate_metadata(NULL, &certificate));
	reset_fixture(); cert_obj.flags = SC_PKCS15_CO_FLAG_PRIVATE; assert(validate_metadata(NULL, &certificate));
	puts("PASS authenticated diagnostic requires observed key, linked unique UTF8 PIN and public matching certificate");
	reset_fixture(); assert(!run_operation() && prompts == 1 && verifies == 1 && psos == 1 && resets == 1 && wipes == 1);
	puts("PASS single signing operation uses status-MSE-VERIFY-PSO order, raw digest and 256-byte response then reset");
	reset_fixture(); status_sw = 0x63C0; assert(run_operation() && !prompts && !verifies && !resets);
	reset_fixture(); status_sw = 0x6A88; assert(run_operation() && !prompts && step == 1);
	reset_fixture(); mse_sw = 0x6A80; assert(run_operation() && !prompts && step == 2);
	puts("PASS unavailable PIN or rejected MSE stops before collecting any credential");
	reset_fixture(); rejected_input = 1; assert(run_operation() && prompts == 1 && !verifies && !resets && wipes == 1);
	puts("PASS locally rejected synthetic input is cleared without transmitting VERIFY");
	reset_fixture(); verify_sw = 0x63C6; assert(run_operation() && verifies == 1 && !psos && resets == 1 && wipes == 1);
	reset_fixture(); fail_ins = 3; assert(run_operation() && verifies == 1 && !psos && resets == 1);
	puts("PASS VERIFY rejection or transport failure does not retry or sign and resets the session");
	reset_fixture(); pso_sw = 0x6982; assert(run_operation() && psos == 1 && resets == 1);
	reset_fixture(); short_signature = 1; assert(run_operation() && psos == 1 && resets == 1);
	reset_fixture(); fail_ins = 4; assert(run_operation() && psos == 1 && resets == 1);
	puts("PASS signing rejection, short response or transport failure does not retry or claim success");
	reset_fixture(); reset_rv = SC_ERROR_READER; assert(run_operation() && verifies == 1 && psos == 1 && resets == 1);
	puts("PASS reset failure propagates even after successful PSO");
	reset_fixture(); verify_first_fixture = 1;
	assert(!run_operation() && step == 4 && prompts == 1 && verifies == 1 && psos == 1 && resets == 1 && wipes == 1);
	puts("PASS opt-in VERIFY-first keeps the observed PIN/key/digest, clears credential before MSE and signs once");
	reset_fixture(); verify_first_fixture = 1; verify_sw = 0x6982;
	assert(run_operation() && step == 2 && verifies == 1 && !psos && resets == 1 && wipes == 1);
	reset_fixture(); verify_first_fixture = 1; verify_sw = 0x63C6;
	assert(run_operation() && step == 2 && verifies == 1 && !psos && resets == 1);
	reset_fixture(); verify_first_fixture = 1; fail_ins = 2;
	assert(run_operation() && step == 2 && verifies == 1 && !psos && resets == 1);
	reset_fixture(); verify_first_fixture = 1; verify_sw = 0x9001;
	assert(run_operation() && step == 2 && verifies == 1 && !psos && resets == 1);
	puts("PASS VERIFY-first rejects security/PIN/transport/non-9000 results without MSE, PSO or another credential");
	reset_fixture(); verify_first_fixture = 1; mse_sw = 0x6982;
	assert(run_operation() && step == 3 && verifies == 1 && !psos && resets == 1 && wipes == 1);
	reset_fixture(); verify_first_fixture = 1; fail_ins = 3;
	assert(run_operation() && step == 3 && verifies == 1 && !psos && resets == 1);
	puts("PASS MSE rejection after VERIFY resets authentication without PSO or retries");
	reset_fixture(); verify_first_fixture = 1; status_sw = 0x63C0;
	assert(run_operation() && step == 1 && !prompts && !verifies && !resets);
	reset_fixture(); verify_first_fixture = 1; rejected_input = 1;
	assert(run_operation() && step == 1 && prompts == 1 && !verifies && !resets && wipes == 1);
	puts("PASS VERIFY-first does not bypass availability or local input checks");
	reset_fixture(); verify_first_fixture = 1; pso_sw = 0x6982;
	assert(run_operation() && step == 4 && psos == 1 && resets == 1);
	reset_fixture(); verify_first_fixture = 1; short_signature = 1;
	assert(run_operation() && psos == 1 && resets == 1);
	reset_fixture(); verify_first_fixture = 1; reset_rv = SC_ERROR_READER;
	assert(run_operation() && psos == 1 && resets == 1);
	puts("PASS VERIFY-first requires complete successful PSO and cleanup before reporting success");
	return 0;
}
