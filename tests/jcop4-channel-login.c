/* Only fake input and a software reader/card. Never opens /dev/tty or PCSC. */
#include "libopensc/opensc.h"
#include <stddef.h>
static int fake_pin(unsigned char pin[14], size_t *size);
static int test_login_reply(sc_apdu_t *a);
#define JCOP4_LOGIN_READ_PIN fake_pin
#define JCOP4_LOGIN_DIAGNOSTIC
#define JCOP4_LOGIN_FIXTURE
#include "jcop4-channel-probe.c"

static int pin_reads, verifies, input_error, reply_error;
static size_t input_size = 8;
static int fake_pin(unsigned char pin[14], size_t *size)
{
	pin_reads++; memset(pin, '7', 14); *size = input_size;
	if (input_error == 2) pin[0] = 0;
	if (input_error == 3) pin[0] = 0xC3;
	return input_error == 1;
}
static int test_login_reply(sc_apdu_t *a)
{
	unsigned char ssc[16] = {0}, iv[32] = {0}, plain[32] = {0}, auth[64] = {0}, tag[16] = {0};
	EVP_CIPHER_CTX *cipher = EVP_CIPHER_CTX_new(); int count, final; assert(cipher);
	assert(++verifies == 1 && pin_reads == 1 && test_state.followups == 2);
	assert(a->cla == 0x0C && a->ins == 0x20 && !a->p1 && a->p2 == 1 && a->lc == 29 && a->le == 256);
	assert(!memcmp(a->data, "\x87\x11\x01", 3) && !memcmp(a->data + 19, "\x8e\x08", 2));
	ssc[15] = 7;
	assert(EVP_EncryptInit_ex(cipher, EVP_aes_128_ecb(), NULL, test_flow.derived.keys.encryption, NULL) == 1);
	assert(EVP_CIPHER_CTX_set_padding(cipher, 0) == 1 && EVP_EncryptUpdate(cipher, iv, &count, ssc, 16) == 1);
	assert(EVP_EncryptFinal_ex(cipher, iv + count, &final) == 1 && count + final == 16);
	assert(EVP_DecryptInit_ex(cipher, EVP_aes_128_cbc(), NULL, test_flow.derived.keys.encryption, iv) == 1);
	assert(EVP_CIPHER_CTX_set_padding(cipher, 0) == 1 && EVP_DecryptUpdate(cipher, plain, &count, a->data + 3, 16) == 1);
	assert(EVP_DecryptFinal_ex(cipher, plain + count, &final) == 1 && count + final == 16);
	for (size_t i = 0; i < input_size; i++) assert(plain[i] == '7');
	assert(plain[input_size] == 0x80); for (size_t i = input_size + 1; i < 16; i++) assert(!plain[i]);
	memcpy(auth, ssc, 16); memcpy(auth + 16, "\x0c\x20\0\x01\x80", 5); memcpy(auth + 32, a->data, 19); auth[51] = 0x80;
	cmac(test_flow.derived.keys.mac, auth, 64, tag); assert(!memcmp(tag, a->data + 21, 8));
	EVP_CIPHER_CTX_free(cipher); OPENSSL_cleanse(iv, sizeof(iv)); OPENSSL_cleanse(plain, sizeof(plain));
	if (reply_error == 1) return -7;
	unsigned int sw = reply_error == 3 ? 0x63C6 : reply_error == 4 ? 0x6982 : 0x9000;
	memset(auth, 0, sizeof(auth)); auth[15] = reply_error == 5 ? 6 : 8;
	auth[16] = 0x99; auth[17] = 2; auth[18] = sw >> 8; auth[19] = sw & 255; auth[20] = 0x80;
	cmac(test_flow.derived.keys.mac, auth, 32, tag); if (reply_error == 2) tag[0] ^= 1;
	assert(a->resplen >= 14); memcpy(a->resp, auth + 16, 4); a->resp[4] = 0x8E; a->resp[5] = 8;
	memcpy(a->resp + 6, tag, 8); a->resplen = 14;
	if (reply_error == 3) { a->sw1 = 0x63; a->sw2 = 0xC6; }
	OPENSSL_cleanse(tag, sizeof(tag)); return 0;
}
static void login_defaults(void)
{
	defaults(); pin_reads = verifies = input_error = reply_error = 0; input_size = 8;
	test_state.query_outer_sw = 0x63C7;
}
static int invoke_login(void)
{
	char *args[] = {"jcop4-channel-login", "0", "--login-once"};
	int saved = dup(STDOUT_FILENO); FILE *capture = tmpfile(); assert(saved >= 0 && capture);
	fflush(stdout); assert(dup2(fileno(capture), STDOUT_FILENO) >= 0);
	int rv = channel_probe_main(3, args); fflush(stdout); rewind(capture);
	size_t size = fread(output, 1, sizeof(output) - 1, capture); output[size] = 0;
	assert(!strstr(output, "777777") && !strstr(output, "PRIVATE_") && !strstr(output, "Sending:"));
	assert(dup2(saved, STDOUT_FILENO) >= 0); close(saved); fclose(capture);
	assert(!test_lock_depth && test_state.resets == 2 && test_state.unlocks == 1 && test_state.disconnects == 1);
	return rv;
}
int main(void)
{
	init(&test_flow); memcpy(test_initial, test_flow.initial, 137);
	setenv("OPENSC_CONF", "/tmp/synthetic-config", 1); setenv("OPENSC_DEBUG", "0", 1);
	for (size_t size = 6; size <= 14; size++) {
		login_defaults(); input_size = size; assert(invoke_login() == 0 && pin_reads == 1 && verifies == 1);
		assert(strstr(output, "login_result=pin_verified_no_signature") && strstr(output, "login_mac_check=verified"));
		assert(test_state.physical ==
#ifdef TEST_REAL_BOOTSTRAP
			11
#else
			12
#endif
		);
	}
	puts("PASS synthetic ASCII PIN6..14 is encrypted once at SSC7, independently decrypted/MAC checked, responseSSC8 verified and session closed");
	for (int failure = 1; failure <= 5; failure++) {
		login_defaults(); reply_error = failure;
		assert(invoke_login() == 1 && pin_reads == 1 && verifies == 1 && !strstr(output, "login_result=pin_verified"));
		if (failure == 2 || failure == 5) assert(!strstr(output, "login_sw="));
		if (failure == 3) assert(strstr(output, "login_sw=63C6"));
	}
	puts("PASS transport/MAC/replay/authenticated warning/error cause one attempted VERIFY only, no signature/retry, and full closure");
	for (int scenario = 0; scenario < 5; scenario++) {
		login_defaults();
		if (scenario == 0) { test_state.query_sw = test_state.query_outer_sw = 0x63C6; }
		if (scenario == 1) { test_state.query_sw = 0x9000; test_state.query_outer_sw = 0; }
		if (scenario == 2) test_flow.mode = BAD_MAC;
		if (scenario == 3) test_state.context_fail = 2;
		if (scenario == 4) test_state.context_fail = 5;
		assert(invoke_login() == 1 && !pin_reads && !verifies);
	}
	puts("PASS channel/MF/query authentication and exact observed availability63C7 gate all credential collection and VERIFY");
	for (int scenario = 0; scenario < 5; scenario++) {
		login_defaults();
		if (scenario < 3) input_error = scenario + 1;
		else input_size = scenario == 3 ? 5 : 15;
		assert(invoke_login() == 1 && pin_reads == 1 && !verifies);
		assert(strstr(output, "login_input=rejected_no_verify"));
	}
	puts("PASS cancellation/control/non-ASCII/invalid bounds reject before VERIFY; credential value/length is never logged");
	{
		for (int mutation = 0; mutation < 9; mutation++) {
			login_defaults(); test_state.locks = 1; memset(&probe_guard, 0, sizeof(probe_guard));
			probe_guard.reader = &test_reader; probe_guard.original = &test_ops; probe_guard.login_mode = 1;
			probe_guard.context_selected = 1; probe_guard.context_query_sw = 0x63C7; probe_guard.phase = PROBE_LOGIN_VERIFY;
			unsigned char wire[35] = {0x0C,0x20,0,1,29,0x87,0x11,1}, reply[256]; memcpy(wire + 24, "\x8e\x08", 2);
			memcpy(probe_guard.expected, wire, 35); probe_guard.expected_size = 35;
			sc_apdu_t a; sc_format_apdu(&test_card, &a, SC_APDU_CASE_4_SHORT, 0x20, 0, 1);
			a.cla = 0x0C; a.data = wire + 5; a.lc = a.datalen = 29; a.le = a.resplen = 256; a.resp = reply;
			if (mutation == 0) a.cla = 0;
			if (mutation == 1) a.p2 = 3;
			if (mutation == 2) a.ins = 0x24;
			if (mutation == 3) wire[8] ^= 1;
			if (mutation == 4) probe_guard.login_mode = 0;
			if (mutation == 5) probe_guard.context_selected = 0;
			if (mutation == 6) probe_guard.context_query_sw = 0x63C6;
			if (mutation == 7) probe_guard.called = 1;
			if (mutation == 8) a.le = 172;
			assert(probe_guard_transmit(&test_reader, &a) < 0 && probe_guard.blocked && !test_state.physical);
		}
	}
	puts("PASS login guard excludes plaintext/change/ref/Le/ciphertext/unconfirmed context/repeated VERIFY before backend");
	jcop4_ca_pending_clear(&test_flow.derived); EVP_PKEY_free(test_flow.peer); return 0;
}
