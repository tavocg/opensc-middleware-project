/* Stateful software peer only: RFC5903 section8.3 public fixture.
 * Response TLV/padding/MAC assembled outside the production message codec. */
#include "../tools/jcop4-channel-confirmation.h"
#include <openssl/param_build.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
enum mode { NORMAL, TRANSPORT_ERROR, BAD_SW, TOO_LARGE, SETUP_PAYLOAD, PHYSICAL_ONLY,
	BAD_MAC, BAD_PADDING, INNER_ERROR, WRONG_CONTENT, SHORT_CONTENT, EMPTY_CONTENT, ZERO_DATA_IV };
struct fixture {
	unsigned char initial[137]; EVP_PKEY *peer; struct jcop4_ca_pending derived;
	int calls, fail_step, bit, ga_template; enum mode mode;
	enum jcop4_ca_outer_le le; unsigned int bad_sw;
};
static void init(struct fixture *f)
{
	const char *point = "0400D0B3975AC4B799F5BEA16D5E13E9AF971D5E9B984C9F39728B5E5739735A21"
		"9B97C356436ADC6E95BB0352F6BE64A6C2912D4EF2D0433CED2B6171640012D9460F"
		"015C68226383956E3BD066E797B623C27CE0EAC2F551A10C2C724D9852077B87"
		"220B6536C5C408A1D2AEBB8E86D678AE49CB57091F4732296579AB44FCD17F0FC56A";
	unsigned char pub[133]; BIGNUM *priv = NULL; OSSL_PARAM *params;
	OSSL_PARAM_BLD *builder = OSSL_PARAM_BLD_new(); EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_from_name(NULL, "EC", NULL);
	memset(f, 0, sizeof(*f)); f->le = JCOP4_CA_LE_BOUND; assert(strlen(point) == 266);
	for (size_t i = 0; i < 133; i++) { unsigned int v; assert(sscanf(point + 2*i, "%2x", &v) == 1); pub[i] = (unsigned char)v; }
	assert(BN_hex2bn(&priv, "0145BA99A847AF43793FDD0E872E7CDFA16BE30FDC780F97BCCC3F078380201E"
		"9C677D600B343757A3BDBF2A3163E4C2F869CCA7458AA4A4EFFC311F5CB151685EB9"));
	assert(builder && ctx);
	assert(OSSL_PARAM_BLD_push_utf8_string(builder, OSSL_PKEY_PARAM_GROUP_NAME, "secp521r1", 0) == 1);
	assert(OSSL_PARAM_BLD_push_octet_string(builder, OSSL_PKEY_PARAM_PUB_KEY, pub, 133) == 1);
	assert(OSSL_PARAM_BLD_push_BN(builder, OSSL_PKEY_PARAM_PRIV_KEY, priv) == 1);
	params = OSSL_PARAM_BLD_to_param(builder); assert(params);
	assert(EVP_PKEY_fromdata_init(ctx) == 1 && EVP_PKEY_fromdata(ctx, &f->peer, EVP_PKEY_KEYPAIR, params) == 1);
	OSSL_PARAM_free(params); OSSL_PARAM_BLD_free(builder); BN_clear_free(priv); EVP_PKEY_CTX_free(ctx);
	f->initial[2] = 18; f->initial[4] = 0x77; memcpy(f->initial + 5, pub + 1, 132);
}
static void cmac(const unsigned char key[16], const unsigned char *input, size_t size, unsigned char tag[16])
{
	EVP_MAC *mac = EVP_MAC_fetch(NULL, "CMAC", NULL); EVP_MAC_CTX *ctx;
	char cipher[] = "AES-128-CBC"; OSSL_PARAM params[2]; size_t actual = 0;
	assert(mac); ctx = EVP_MAC_CTX_new(mac); assert(ctx);
	params[0] = OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_CIPHER, cipher, 0); params[1] = OSSL_PARAM_construct_end();
	assert(EVP_MAC_init(ctx, key, 16, params) == 1 && EVP_MAC_update(ctx, input, size) == 1);
	assert(EVP_MAC_final(ctx, tag, &actual, 16) == 1 && actual == 16);
	EVP_MAC_CTX_free(ctx); EVP_MAC_free(mac);
}
static int exchange(void *context, const unsigned char *command, size_t size,
		unsigned char *response, size_t capacity, size_t *received)
{
	struct fixture *f = context; unsigned char auth[256] = {0}, tag[16];
	f->calls++; assert(capacity == 258 && f->calls <= 3);
	if (f->calls == 1) assert(size == 17 && !memcmp(command, "\0\x22\x41\xa4\x0c\x80\x0a\x04\0\x7f\0\x07\x02\x02\x03\x02\x02", 17));
	else if (f->calls == 2) {
		assert(size == 144 && !memcmp(command, "\x10\x86\0\0\x8b\x7c\x81\x88\x80\x81\x85", 11));
		assert(jcop4_ca_prepare_key(f->peer, command + 11, 133, &f->derived) == 0);
	} else {
		assert(size == 19 && !memcmp(command, "\x0c\xb0\0\0\x0d\x97\x01\x89\x8e\x08", 10));
		assert(command[18] == (unsigned char)f->le);
		auth[15] = 1; memcpy(auth + 16, "\x0c\xb0\0\0", 4); auth[20] = 0x80;
		memcpy(auth + 32, "\x97\x01\x89", 3); auth[35] = 0x80;
		cmac(f->derived.keys.mac, auth, 48, tag); assert(!memcmp(command + 10, tag, 8));
	}
	if (f->fail_step == f->calls && f->mode == TRANSPORT_ERROR) { *received = capacity; return -7; }
	if (f->fail_step == f->calls && f->mode == TOO_LARGE) { *received = SIZE_MAX; return 0; }
	if (f->calls < 3) {
		unsigned int sw = f->fail_step == f->calls && f->mode == BAD_SW ? f->bad_sw : 0x9000;
		*received = 2; response[0] = (unsigned char)(sw >> 8); response[1] = (unsigned char)sw;
		if (f->fail_step == f->calls && f->mode == SETUP_PAYLOAD) {
			memcpy(response, "\x7c\x03\x81\x01\0\x90\0", 7); *received = 7;
		} else if (f->calls == 2 && f->ga_template && sw == 0x9000) { memcpy(response, "\x7c\0\x90\0", 4); *received = 4; }
		return 0;
	}
	if (f->mode == PHYSICAL_ONLY) { memcpy(response, "\x90\0", 2); *received = 2; return 0; }
	{
		unsigned char padded[144] = {0}, encrypted[160] = {0}, iv[16] = {0};
		size_t offset = 0, plain_size = f->mode == SHORT_CONTENT ? 136 : 137, auth_size;
		int count = 0, final = 0; unsigned int sw = f->mode == INNER_ERROR ? 0x6982 : 0x9000;
		if (f->mode != INNER_ERROR && f->mode != EMPTY_CONTENT) {
			EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new(); assert(ctx);
			/* Independent IV construction using ECB, not the production helper. */
			unsigned char response_ssc[16] = {0}; response_ssc[15] = 2;
			if (f->mode != ZERO_DATA_IV) {
				assert(EVP_EncryptInit_ex(ctx, EVP_aes_128_ecb(), NULL, f->derived.keys.encryption, NULL) == 1);
				assert(EVP_CIPHER_CTX_set_padding(ctx, 0) == 1 && EVP_EncryptUpdate(ctx, iv, &count, response_ssc, 16) == 1);
				assert(EVP_EncryptFinal_ex(ctx, iv + count, &final) == 1 && count + final == 16);
			}
			memcpy(padded, f->initial, plain_size); padded[plain_size] = f->mode == BAD_PADDING ? 0 : 0x80;
			if (f->mode == WRONG_CONTENT) padded[0] ^= 1;
			assert(EVP_EncryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, f->derived.keys.encryption, iv) == 1);
			assert(EVP_CIPHER_CTX_set_padding(ctx, 0) == 1 && EVP_EncryptUpdate(ctx, encrypted, &count, padded, 144) == 1);
			assert(EVP_EncryptFinal_ex(ctx, encrypted + count, &final) == 1 && count + final == 144); EVP_CIPHER_CTX_free(ctx);
			memcpy(response, "\x87\x81\x91\x01", 4); memcpy(response + 4, encrypted, 144); offset = 148;
		}
		response[offset++] = 0x99; response[offset++] = 2;
		response[offset++] = (unsigned char)(sw >> 8); response[offset++] = (unsigned char)sw;
		memset(auth, 0, sizeof(auth)); auth[15] = 2; memcpy(auth + 16, response, offset);
		auth[16 + offset] = 0x80; auth_size = (16 + offset + 16) / 16 * 16;
		cmac(f->derived.keys.mac, auth, auth_size, tag);
		if (f->mode == BAD_MAC) tag[f->bit / 8] ^= (unsigned char)(1 << (f->bit % 8));
		response[offset++] = 0x8E; response[offset++] = 8; memcpy(response + offset, tag, 8); offset += 8;
		response[offset++] = 0x90; response[offset++] = 0; *received = offset;
	}
	return 0;
}
static int run(struct fixture *f, struct jcop4_ca_probe_report *report)
{
	f->calls = 0; jcop4_ca_pending_clear(&f->derived);
	return jcop4_ca_confirm_d004(f->initial, 137, f->le, exchange, f, report);
}
int main(void)
{
	struct fixture f; struct jcop4_ca_probe_report report; unsigned char saved[137]; init(&f); memcpy(saved, f.initial, 137);
	for (int policy = 0; policy < 2; policy++) for (int template = 0; template < 2; template++) {
		f.le = policy ? JCOP4_CA_LE_SHORT_MAX : JCOP4_CA_LE_BOUND; f.ga_template = template;
		assert(run(&f, &report) == 0 && f.calls == 3 && report.stage == JCOP4_CA_READ);
		assert(report.outcome == JCOP4_CA_READ_CONFIRMED && report.authenticated_response && report.sw == 0x9000);
		assert(!memcmp(f.initial, saved, 137));
	}
	puts("PASS software peer confirms MSE/GA/one protected D004 read with explicit Le172/256 and empty GA forms");
	for (int step = 1; step <= 3; step++) {
		f.fail_step = step; f.mode = TRANSPORT_ERROR;
		assert(run(&f, &report) == 1 && f.calls == step && report.transport == -7 && !report.authenticated_response && !report.sw);
		f.mode = TOO_LARGE; assert(run(&f, &report) == 1 && f.calls == step && !report.authenticated_response && !report.sw);
	}
	puts("PASS transport errors and oversized responses stop at the failing stage without retries or stale status");
	{
		const unsigned int statuses[] = {0x6982, 0x6A88, 0x6100, 0x6C00};
		for (int step = 1; step <= 2; step++) for (size_t i = 0; i < 4; i++) {
			f.fail_step = step; f.mode = BAD_SW; f.bad_sw = statuses[i];
			assert(run(&f, &report) == 1 && f.calls == step && report.outcome == JCOP4_CA_SETUP_SW_FAILED);
			assert(report.sw == statuses[i] && !report.authenticated_response);
		}
		f.mode = SETUP_PAYLOAD;
		for (int step = 1; step <= 2; step++) { f.fail_step = step; assert(run(&f, &report) == 1 && f.calls == step && report.outcome == JCOP4_CA_RESPONSE_SHAPE_FAILED); }
	}
	puts("PASS setup errors/61xx/6Cxx and nonce-like payloads block follow-up commands and GET RESPONSE");
	f.fail_step = 0; f.mode = PHYSICAL_ONLY; assert(run(&f, &report) == 1 && f.calls == 3 && !report.authenticated_response && !report.sw);
	f.mode = BAD_MAC;
	for (int bit = 0; bit < 64; bit++) { f.bit = bit; assert(run(&f, &report) == 1 && f.calls == 3 && !report.authenticated_response && !report.sw); }
	f.mode = BAD_PADDING; assert(run(&f, &report) == 1 && !report.authenticated_response && !report.sw);
	puts("PASS plain9000, all changed MAC bits and valid-MAC/bad-padding do not confirm a channel");
	f.mode = INNER_ERROR;
	assert(run(&f, &report) == 1 && report.authenticated_response && report.sw == 0x6982 && report.outcome == JCOP4_CA_INNER_SW_FAILED);
	const enum mode modes[] = {WRONG_CONTENT, SHORT_CONTENT, EMPTY_CONTENT};
	for (size_t i = 0; i < 3; i++) { f.mode = modes[i]; assert(run(&f, &report) == 1 && report.authenticated_response && report.sw == 0x9000 && report.outcome == JCOP4_CA_CONTENT_MISMATCH); }
	puts("PASS authenticated inner error and wrong/short/missing content remain failed confirmations");
	f.mode = ZERO_DATA_IV;
	assert(run(&f, &report) == 1 && report.authenticated_response && report.sw == 0x9000);
	assert(report.outcome == JCOP4_CA_CONTENT_MISMATCH && report.content_size == 137 && report.content_size_equal);
	assert(!report.content_first_block_equal && report.content_tail_equal);
	f.mode = NORMAL;
	assert(run(&f, &report) == 0 && report.content_first_block_equal && report.content_tail_equal);
	f.mode = SHORT_CONTENT; assert(run(&f, &report) == 1 && report.content_size == 136 && !report.content_size_equal);
	f.mode = BAD_MAC; assert(run(&f, &report) == 1 && !report.content_size && !report.content_first_block_equal && !report.content_tail_equal);
	puts("PASS zero-data-IV peer shows authenticated first-block mismatch; fixed comparison metadata never exposes unverified data");
	{
		f.calls = 0; const size_t sizes[] = {0, 136, 138, SIZE_MAX};
		for (size_t i = 0; i < 4; i++) {
			memset(&report, 0xAA, sizeof(report)); assert(jcop4_ca_confirm_d004(f.initial, sizes[i], f.le, exchange, &f, &report) == 1);
			assert(!f.calls && !report.authenticated_response && !report.sw && report.outcome == JCOP4_CA_INVALID_INPUT);
		}
		f.initial[2] = 17; assert(run(&f, &report) == 1 && !f.calls); f.initial[2] = 18;
		f.initial[5] = 2; assert(run(&f, &report) == 1 && !f.calls); memcpy(f.initial, saved, 137);
		assert(jcop4_ca_confirm_d004(NULL, 137, f.le, exchange, &f, &report) == 1 && !f.calls);
		assert(jcop4_ca_confirm_d004(f.initial, 137, 0, exchange, &f, &report) == 1 && !f.calls);
		assert(jcop4_ca_confirm_d004(f.initial, 137, f.le, NULL, &f, &report) == 1 && !f.calls);
		assert(jcop4_ca_confirm_d004(f.initial, 137, f.le, exchange, &f, NULL) == 1 && !f.calls);
	}
	puts("PASS invalid D004/point/length/policy/arguments clear the report and cause zero exchanges");
	EVP_PKEY_free(f.peer); jcop4_ca_pending_clear(&f.derived); return 0;
}
