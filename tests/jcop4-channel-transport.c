/* No hardware: software channel peer and mocked OpenSC reader only. */
#include "libopensc/opensc.h"
#include "libopensc/internal.h"
static int test_transport_reply(sc_apdu_t *apdu);
#define JCOP4_TRANSPORT_DIAGNOSTIC
#define JCOP4_TRANSPORT_FIXTURE
#include "jcop4-channel-probe.c"

static int mf_calls, mf_error, mf_template;
static unsigned char mf_fixture[64]; static size_t mf_size;
static void fixture_mf(int shape)
{
	const unsigned char base[] = {0x62,7,0x83,2,0x3F,0,0x82,1,0x38};
	memcpy(mf_fixture, base, sizeof(base)); mf_size = sizeof(base);
	if (shape == 1) mf_fixture[0] = 0x6F;
	if (shape == 2) { memmove(mf_fixture + 2, mf_fixture, mf_size); mf_fixture[0] = 0x6F; mf_fixture[1] = mf_size; mf_size += 2; }
}
static void independent_iv(unsigned int counter, unsigned char iv[16])
{
	unsigned char ssc[16] = {0}; ssc[15] = counter; int n, last;
	EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new(); assert(ctx);
	assert(EVP_EncryptInit_ex(ctx, EVP_aes_128_ecb(), NULL, test_flow.derived.keys.encryption, NULL) == 1);
	assert(EVP_CIPHER_CTX_set_padding(ctx, 0) == 1 && EVP_EncryptUpdate(ctx, iv, &n, ssc, 16) == 1);
	assert(EVP_EncryptFinal_ex(ctx, iv + n, &last) == 1 && n + last == 16); EVP_CIPHER_CTX_free(ctx);
}
static int test_transport_reply(sc_apdu_t *a)
{
	unsigned char iv[16], plain[32], auth[96] = {0}, mac[16]; int n, last;
	assert(++mf_calls == 1 && test_flow.calls == 3 && !test_state.followups && test_card.caps == 0);
	assert(a->cse == (SC_APDU_CASE_4_EXT) && a->cla == 0x0C && a->ins == 0xA4 && !a->p1 && !a->p2);
	assert(a->lc == 33 && a->le == 291 && a->resplen == 291 && !memcmp(a->data + 19, "\x97\x02\x01\0\x8e\x08", 6));
	/* Real OpenSC serializer, even in the native fixture: no invented EXT flag. */
	unsigned char *serialized = NULL; size_t serialized_size;
	assert(sc_apdu_get_octets(&test_ctx, a, &serialized, &serialized_size, SC_PROTO_T1) == 0);
	assert(serialized_size == 42 && !memcmp(serialized, probe_guard.expected, 42)); free(serialized);
	independent_iv(3, iv); EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new(); assert(ctx);
	assert(EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, test_flow.derived.keys.encryption, iv) == 1);
	assert(EVP_CIPHER_CTX_set_padding(ctx, 0) == 1 && EVP_DecryptUpdate(ctx, plain, &n, a->data + 3, 16) == 1);
	assert(EVP_DecryptFinal_ex(ctx, plain + n, &last) == 1 && n + last == 16); EVP_CIPHER_CTX_free(ctx);
	assert(!memcmp(plain, "\x3f\0\x80", 3)); for (int i = 3; i < 16; i++) assert(!plain[i]);
	auth[15] = 3; memcpy(auth + 16, "\x0c\xa4\0\0\x80", 5); memcpy(auth + 32, a->data, 23); auth[55] = 0x80;
	cmac(test_flow.derived.keys.mac, auth, 64, mac); assert(!memcmp(mac, a->data + 25, 8));
	if (mf_error == 1) return -7;
	if (mf_error == 5) { a->resplen = 0; return 0; }
	if (mf_error == 6) { a->resplen = 292; return 0; }
	if (mf_error == 7) { a->resplen = 0; a->sw1 = 0x61; return 0; }
	fixture_mf(mf_template); if (mf_error == 4) mf_fixture[mf_template == 2 ? 6 : 4] ^= 1;
	memset(plain, 0, sizeof(plain)); memcpy(plain, mf_fixture, mf_size); plain[mf_size] = 0x80;
	independent_iv(4, iv); ctx = EVP_CIPHER_CTX_new(); assert(ctx); memcpy(a->resp, "\x87\x11\x01", 3);
	assert(EVP_EncryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, test_flow.derived.keys.encryption, iv) == 1);
	assert(EVP_CIPHER_CTX_set_padding(ctx, 0) == 1 && EVP_EncryptUpdate(ctx, a->resp + 3, &n, plain, 16) == 1);
	assert(EVP_EncryptFinal_ex(ctx, a->resp + 3 + n, &last) == 1 && n + last == 16); EVP_CIPHER_CTX_free(ctx);
	memcpy(a->resp + 19, mf_error == 3 ? "\x99\x02\x69\x82" : "\x99\x02\x90\0", 4);
	memset(auth, 0, sizeof(auth)); auth[15] = mf_error == 8 ? 2 : 4; memcpy(auth + 16, a->resp, 23); auth[39] = 0x80;
	cmac(test_flow.derived.keys.mac, auth, 48, mac); if (mf_error == 2) mac[0] ^= 1;
	memcpy(a->resp + 23, "\x8e\x08", 2); memcpy(a->resp + 25, mac, 8); a->resplen = 33;
	OPENSSL_cleanse(plain, sizeof(plain)); OPENSSL_cleanse(iv, sizeof(iv)); OPENSSL_cleanse(mac, sizeof(mac)); return 0;
}
static void transport_defaults(void)
{
	defaults(); test_reader.max_recv_size = 65536; test_flow.le = JCOP4_CA_LE_BOUND;
	mf_calls = mf_error = mf_template = 0;
}
static int invoke_transport(void)
{
	char *args[] = {"jcop4-channel-transport", "0", "--transport-extended"};
	int saved = dup(STDOUT_FILENO); FILE *capture = tmpfile(); assert(saved >= 0 && capture);
	fflush(stdout); assert(dup2(fileno(capture), STDOUT_FILENO) >= 0); int rv = channel_probe_main(3, args);
	fflush(stdout); rewind(capture); size_t size = fread(output, 1, sizeof(output)-1, capture); output[size] = 0;
	assert(dup2(saved, STDOUT_FILENO) >= 0); close(saved); fclose(capture);
	assert(!strstr(output, "PRIVATE_IDENTIFIER") && !strstr(output, "Sending:") && !strstr(output, "pin_query") && !strstr(output, "login_") && !strstr(output, "signature_"));
	assert(!test_card.caps && !test_state.followups && !test_lock_depth);
	if (test_state.locks) assert(test_state.resets == 2 && test_state.unlocks == 1 && test_state.disconnects == 1);
	return rv;
}
int main(void)
{
	init(&test_flow); memcpy(test_initial, test_flow.initial, 137); setenv("OPENSC_CONF", "/tmp/synthetic", 1); setenv("OPENSC_DEBUG", "0", 1);
	for (int shape = 0; shape < 3; shape++) {
		transport_defaults(); mf_template = shape; assert(invoke_transport() == 0 && mf_calls == 1);
		assert(strstr(output, "transport_mf_mac_check=verified") && strstr(output, "transport_mf_sw=9000"));
		assert(strstr(output, "transport_mf_result=extended_frame_authenticated_mf_no_pin"));
	}
	puts("PASS no-PIN READ172/channel confirmation followed by exact42-byte extended MF/Le291/DO97020100, independently checked AES/CMAC/SSC3/4 and real OpenSC serialization, unchanged card capability flags and complete closure");
	for (int error = 1; error <= 8; error++) {
		transport_defaults(); mf_error = error; assert(invoke_transport() == 1 && mf_calls == 1);
		assert(strstr(output, "transport_mf_result=not_confirmed_stop"));
		if (error == 2 || error == 8) assert(strstr(output, "transport_mf_mac_check=failed") && !strstr(output, "transport_mf_sw="));
	}
	puts("PASS transport/oversize/bare9000/61/replay/bad MAC/authenticated error/wrong MF stop after one exchange without fallback, GET RESPONSE or credential paths");
	transport_defaults(); test_reader.max_recv_size = 290; assert(invoke_transport() == 1 && !test_state.physical && !test_state.locks);
	transport_defaults(); test_flow.mode = BAD_MAC; assert(invoke_transport() == 1 && !mf_calls);
	transport_defaults(); test_state.reset_error = 2; assert(invoke_transport() == 1 && mf_calls == 1);
	puts("PASS insufficient reader limit stops before bootstrap and missing channel confirmation prevents EXT probe; failed final reset prevents process success");
	fixture_mf(0); assert(!transport_mf_control(mf_fixture, mf_size));
	for (size_t i = 0; i < mf_size; i++) assert(transport_mf_control(mf_fixture, i));
	for (int mutation = 0; mutation < 7; mutation++) {
		fixture_mf(0);
		if (mutation == 0) mf_fixture[4] = 0;
		if (mutation == 1) mf_fixture[8] = 1;
		if (mutation == 2) mf_fixture[8] = 0xB8;
		if (mutation == 3) { memcpy(mf_fixture + mf_size, mf_fixture + 2, 4); mf_size += 4; mf_fixture[1] += 4; }
		if (mutation == 4) mf_fixture[3] = 3;
		if (mutation == 5) mf_fixture[1] = 0x80;
		if (mutation == 6) mf_fixture[mf_size++] = 0;
		assert(transport_mf_control(mf_fixture, mf_size));
	}
	puts("PASS MF semantic validation admits bounded canonical62/6F/direct/nested control only with unique3F00 and DF descriptor, rejecting other FID/type, duplicates, truncation and trailing data");
	for (int mutation = 0; mutation < 13; mutation++) {
		transport_defaults(); memset(&probe_guard, 0, sizeof(probe_guard)); probe_guard.reader = &test_reader; probe_guard.original = &test_ops;
		probe_guard.phase = PROBE_TRANSPORT_MF; probe_guard.transport_mode = 1; probe_guard.transport_channel_verified = 1;
		unsigned char wire[42] = {0}, reply[291]; memcpy(wire, "\x0c\xa4\0\0\0\0\x21\x87\x11\x01", 10);
		memcpy(wire + 26, "\x97\x02\x01\0\x8e\x08", 6); wire[40] = 1; wire[41] = 0x23;
		memcpy(probe_guard.expected, wire, 42); probe_guard.expected_size = 42;
		sc_apdu_t a; sc_format_apdu(&test_card, &a, SC_APDU_CASE_4_EXT, 0xA4, 0, 0); a.cla = 0x0C;
		a.data = wire + 7; a.lc = a.datalen = 33; a.le = a.resplen = 291; a.resp = reply;
		if (mutation == 0) probe_guard.transport_mode = 0;
		if (mutation == 1) a.cla = 0;
		if (mutation == 2) a.p1 = 4;
		if (mutation == 3) a.p2 = 0x0C;
		if (mutation == 4) a.le = 256;
		if (mutation == 5) a.cse = SC_APDU_CASE_4_SHORT;
		if (mutation == 6) wire[11] ^= 1;
		if (mutation == 7) a.resplen = 290;
		if (mutation == 8) probe_guard.called = 1;
		if (mutation == 9) a.ins = 0x20;
		if (mutation == 10) a.control = 1;
		if (mutation == 11) probe_guard.transport_channel_verified = 0;
		if (mutation == 12) test_reader.max_recv_size = 290;
		assert(probe_guard_transmit(&test_reader, &a) < 0 && probe_guard.blocked && !test_state.physical);
	}
	puts("PASS raw-reader EXT dispatch remains guarded against missing opt-in, changed ciphertext/headers/Le/case/capacity, repetition, PIN and control commands before backend");
	jcop4_ca_pending_clear(&test_flow.derived); EVP_PKEY_free(test_flow.peer); return 0;
}
