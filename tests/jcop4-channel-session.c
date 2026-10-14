/* Retention/continuation tests only; context, SELECT and peer are synthetic. */
#define main confirmation_fixture_main
#include "jcop4-channel-confirmation.c"
#undef main

static void fresh(struct jcop4_sm_channel *channel)
{
	memset(channel, 0xA5, sizeof(*channel)); channel->phase = JCOP4_SM_INVALID;
}
static void cleared(const struct jcop4_sm_channel *channel)
{
	const unsigned char *bytes = (const unsigned char *)&channel->keys;
	assert(channel->phase == JCOP4_SM_INVALID);
	for (size_t i = 0; i < sizeof(channel->keys); i++) assert(!bytes[i]);
}
static int start(struct fixture *f, struct jcop4_sm_channel *channel, struct jcop4_ca_probe_report *report)
{
	f->calls = 0; jcop4_ca_pending_clear(&f->derived);
	return jcop4_ca_confirm_d004_session(f->initial, 137, f->le, exchange, f, report, channel);
}
/* Independently decrypt the software SELECT data with ECB-derived IV. */
static void check_select(struct fixture *f, const unsigned char header[4], const unsigned char *body, size_t size)
{
	unsigned char ssc[16] = {0}, iv[32] = {0}, plain[32] = {0}, expected[16] = {0};
	unsigned char auth[64] = {0}, tag[16]; int count, final;
	EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new(); assert(ctx);
	assert(size == 32 && !memcmp(header, "\x0c\xa4\0\0", 4));
	assert(!memcmp(body, "\x87\x11\x01", 3) && !memcmp(body + 19, "\x97\x01\0\x8e\x08", 5));
	ssc[15] = 3;
	assert(EVP_EncryptInit_ex(ctx, EVP_aes_128_ecb(), NULL, f->derived.keys.encryption, NULL) == 1);
	assert(EVP_CIPHER_CTX_set_padding(ctx, 0) == 1 && EVP_EncryptUpdate(ctx, iv, &count, ssc, 16) == 1);
	assert(EVP_EncryptFinal_ex(ctx, iv + count, &final) == 1 && count + final == 16);
	assert(EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, f->derived.keys.encryption, iv) == 1);
	assert(EVP_CIPHER_CTX_set_padding(ctx, 0) == 1 && EVP_DecryptUpdate(ctx, plain, &count, body + 3, 16) == 1);
	assert(EVP_DecryptFinal_ex(ctx, plain + count, &final) == 1 && count + final == 16);
	expected[0] = 0x3F; expected[2] = 0x80; assert(!memcmp(plain, expected, 16));
	memcpy(auth, ssc, 16); memcpy(auth + 16, header, 4); auth[20] = 0x80;
	memcpy(auth + 32, body, 22); auth[54] = 0x80;
	cmac(f->derived.keys.mac, auth, sizeof(auth), tag); assert(!memcmp(tag, body + 24, 8));
	EVP_CIPHER_CTX_free(ctx); OPENSSL_cleanse(iv, sizeof(iv)); OPENSSL_cleanse(plain, sizeof(plain));
	OPENSSL_cleanse(tag, sizeof(tag));
}
static void select_reply(struct fixture *f, unsigned int counter, unsigned char response[16])
{
	unsigned char auth[32] = {0}, tag[16]; auth[15] = (unsigned char)counter;
	memcpy(response, "\x99\x02\x90\0\x8e\x08", 6); memcpy(auth + 16, response, 4); auth[20] = 0x80;
	cmac(f->derived.keys.mac, auth, sizeof(auth), tag); memcpy(response + 6, tag, 8);
	response[14] = 0x90; response[15] = 0; OPENSSL_cleanse(tag, sizeof(tag));
}
int main(void)
{
	struct fixture f; struct jcop4_ca_probe_report report;
	struct jcop4_sm_channel channel, saved;
	const unsigned char header[4] = {0, 0xA4, 0, 0}, data[2] = {0x3F, 0};
	unsigned char protected_header[4], body[64], response[16], plain[16]; size_t size, written; unsigned int sw;
	init(&f); fresh(&channel);
	assert(start(&f, &channel, &report) == 0 && f.calls == 3 && channel.phase == JCOP4_SM_IDLE);
	assert(channel.keys.ssc[15] == 2 && !memcmp(channel.keys.encryption, f.derived.keys.encryption, 16));
	assert(!memcmp(channel.keys.mac, f.derived.keys.mac, 16));
	assert(report.outcome == JCOP4_CA_READ_CONFIRMED && report.content_size_equal && report.content_first_block_equal && report.content_tail_equal);
	puts("PASS channel state is returned only after full D004 confirmation, IDLE at SSC2 with the independently derived software peer keys");
	assert(jcop4_sm_seal(&channel, header, data, 2, 1, 256, protected_header, body, sizeof(body), &size) == 0);
	assert(channel.phase == JCOP4_SM_WAITING && channel.keys.ssc[15] == 3);
	check_select(&f, protected_header, body, size);
	assert(jcop4_sm_seal(&channel, header, data, 2, 1, 256, protected_header, body, sizeof(body), &written) == 1);
	assert(channel.phase == JCOP4_SM_WAITING && channel.keys.ssc[15] == 3);
	select_reply(&f, 4, response); memset(plain, 0xAA, sizeof(plain));
	assert(jcop4_sm_open(&channel, response, sizeof(response), plain, sizeof(plain), &written, &sw) == 0);
	assert(!written && sw == 0x9000 && plain[0] == 0xAA && channel.phase == JCOP4_SM_IDLE && channel.keys.ssc[15] == 4);
	jcop4_sm_invalidate(&channel); cleared(&channel);
	puts("PASS software-only encrypted SELECT continues at SSC3/4 without resetting counters or interleaving commands; explicit close wipes state");
	fresh(&channel); assert(start(&f, &channel, &report) == 0);
	assert(jcop4_sm_seal(&channel, header, data, 2, 1, 256, protected_header, body, sizeof(body), &size) == 0);
	select_reply(&f, 2, response);
	assert(jcop4_sm_open(&channel, response, sizeof(response), plain, sizeof(plain), &written, &sw) == 1);
	assert(!written && !sw); cleared(&channel);
	puts("PASS response with stale SSC2 cannot authenticate the continuation requiring SSC4 and invalidates the retained keys");
	{
		const enum mode failures[] = {TRANSPORT_ERROR, BAD_SW, SETUP_PAYLOAD, BAD_MAC, INNER_ERROR, WRONG_CONTENT, SHORT_CONTENT, ZERO_DATA_IV, PHYSICAL_ONLY};
		for (size_t i = 0; i < sizeof(failures)/sizeof(failures[0]); i++) {
			fresh(&channel); f.mode = failures[i]; f.fail_step = 1; f.bad_sw = 0x6982;
			assert(start(&f, &channel, &report) == 1); cleared(&channel);
		}
		f.mode = NORMAL; f.fail_step = 0;
	}
	puts("PASS setup/transport/MAC/SW/length/content/IV failures never hand a usable session to the caller");
	fresh(&channel); f.calls = 0;
	assert(jcop4_ca_confirm_d004_session(f.initial, 136, f.le, exchange, &f, &report, &channel) == 1 && !f.calls); cleared(&channel);
	fresh(&channel); assert(jcop4_ca_confirm_d004_session(f.initial, 137, f.le, exchange, &f, NULL, &channel) == 1 && !f.calls); cleared(&channel);
	fresh(&channel); assert(start(&f, &channel, &report) == 0); saved = channel; f.calls = 0;
	assert(jcop4_ca_confirm_d004_session(f.initial, 137, f.le, exchange, &f, &report, &channel) == 1 && !f.calls);
	assert(!memcmp(&channel, &saved, sizeof(channel)) && report.outcome == JCOP4_CA_INVALID_INPUT);
	assert(jcop4_sm_seal(&channel, header, data, 2, 1, 256, protected_header, body, sizeof(body), &size) == 0);
	jcop4_sm_invalidate(&saved); saved = channel;
	assert(jcop4_ca_confirm_d004_session(f.initial, 137, f.le, exchange, &f, &report, &channel) == 1 && !f.calls);
	assert(channel.phase == JCOP4_SM_WAITING && !memcmp(&channel, &saved, sizeof(channel)));
	jcop4_sm_invalidate(&channel); jcop4_sm_invalidate(&saved);
	puts("PASS invalid input clears an unused destination; an active destination is refused without I/O or silent loss of the owner's existing state");
	assert(run(&f, &report) == 0 && f.calls == 3);
	jcop4_ca_pending_clear(&f.derived); EVP_PKEY_free(f.peer);
	puts("PASS existing probe API still confirms three exchanges and retains no session; no reader, PIN or real SELECT is used");
	return 0;
}
