/* Fake chunks only. No card, PIN, signature or actual certificate data. */
#include "../tools/jcop4-response-collector.h"
#include "../tools/jcop4-channel-messages.h"
#include <assert.h>
#include <stdio.h>

struct fixture { unsigned char data[419]; size_t total, offset, chunk; int calls, error, malformed; unsigned char cla; };
static int exchange(void *context, const unsigned char *wire, size_t size, unsigned char *response, size_t capacity, size_t *written)
{
	struct fixture *f = context; f->calls++;
	assert(size == 5 && wire[0] == f->cla && wire[1] == 0xC0 && !wire[2] && !wire[3] && capacity == 258);
	size_t le = wire[4] ? wire[4] : 256, remaining = f->total - f->offset;
	assert(le <= remaining && le == (remaining >= 256 ? 256 : remaining));
	if (f->error) return -7;
	size_t chunk = f->chunk && f->chunk < le ? f->chunk : le;
	if (f->malformed == 1) chunk = 0;
	memcpy(response, f->data + f->offset, chunk); f->offset += chunk;
	remaining = f->total - f->offset;
	response[chunk] = remaining ? 0x61 : 0x90; response[chunk + 1] = remaining >= 256 ? 0 : (unsigned char)remaining;
	if (f->malformed == 2) { response[chunk] = 0x6C; response[chunk + 1] = 1; }
	*written = f->malformed == 3 ? 259 : f->malformed == 4 ? 1 : chunk + 2;
	return 0;
}
static void init(struct fixture *f, size_t size, unsigned char cla)
{
	memset(f, 0, sizeof(*f)); f->total = size; f->cla = cla;
	for (size_t i = 0; i < size; i++) f->data[i] = (unsigned char)(i * 7 + 3);
}
static void error(struct fixture *f, const unsigned char *first, size_t size, enum jcop4_response_cla policy, size_t limit,
		enum jcop4_response_result reason)
{
	unsigned char output[421]; size_t written = 123; struct jcop4_response_report report;
	memset(output, 0xAA, sizeof(output));
	assert(jcop4_response_collect(first, size, policy, limit, exchange, f, output, sizeof(output), &written, &report) == 1);
	assert(!written && report.result == reason);
	for (size_t i = 0; i < sizeof(output); i++) assert(output[i] == 0xAA);
}
int main(void)
{
	struct fixture f; struct jcop4_response_report report; unsigned char first[258], out[421]; size_t written;
	for (int key_size = 0; key_size < 2; key_size++) for (int cla = 0; cla < 2; cla++) {
		size_t limit = key_size ? 421 : 293; init(&f, limit - 2, cla ? 0x0C : 0);
		assert(jcop4_response_collect((unsigned char *)"\x61\0", 2, cla, limit, exchange, &f, out, sizeof(out), &written, &report) == 0);
		assert(f.calls == 2 && written == limit && !memcmp(out, f.data, limit - 2) && out[limit - 2] == 0x90 && !out[limit - 1]);
		assert(report.result == JCOP4_RESPONSE_COMPLETE_UNAUTHENTICATED);
	}
	puts("PASS synthetic293/421-byte opaque frames collect with explicit0/0C policy, bounded short GET RESPONSE and no replay of the initial command");
	init(&f, 291, 0x0C); memcpy(first, f.data, 256); first[256] = 0x61; first[257] = 35; f.offset = 256;
	assert(jcop4_response_collect(first, sizeof(first), JCOP4_RESPONSE_CLA_PROTECTED, 293, exchange, &f, out, sizeof(out), &written, &report) == 0);
	assert(f.calls == 1 && written == 293 && !memcmp(out, f.data, 291));
	init(&f, 14, 0); memcpy(first, f.data, 14); first[14] = 0x90; first[15] = 0;
	assert(jcop4_response_collect(first, 16, JCOP4_RESPONSE_CLA_ZERO, 293, exchange, &f, out, sizeof(out), &written, &report) == 0 && !f.calls);
	puts("PASS initial data/61xx and complete9000 frames are handled without losing bytes; complete transport is explicitly not MAC authentication");
	for (int bad = 0; bad < 5; bad++) {
		init(&f, 291, 0); f.error = bad == 0; f.malformed = bad;
		error(&f, (unsigned char *)"\x61\0", 2, JCOP4_RESPONSE_CLA_ZERO, 293,
			bad == 0 ? JCOP4_RESPONSE_TRANSPORT : bad == 1 ? JCOP4_RESPONSE_NO_PROGRESS : bad == 2 ? JCOP4_RESPONSE_STATUS : JCOP4_RESPONSE_SIZE);
		assert(f.calls == 1);
	}
	init(&f, 291, 0); f.chunk = 1;
	error(&f, (unsigned char *)"\x61\0", 2, JCOP4_RESPONSE_CLA_ZERO, 293, JCOP4_RESPONSE_BUDGET); assert(f.calls == 3);
	puts("PASS transport/truncation/oversize/6C/no-progress/endless61xx stop within three continuations and leave output untouched");
	const unsigned char *statuses[] = {(unsigned char *)"\x6c\0", (unsigned char *)"\x69\x82", (unsigned char *)"\x63\xc7", (unsigned char *)"\x90\0"};
	for (int i = 0; i < 4; i++) { init(&f, 291, 0); error(&f, statuses[i], 2, JCOP4_RESPONSE_CLA_ZERO, 293, i == 3 ? JCOP4_RESPONSE_SIZE : JCOP4_RESPONSE_STATUS); assert(!f.calls); }
	init(&f, 291, 0); memset(first, 0, 258); first[256] = 0x61;
	error(&f, first, 258, JCOP4_RESPONSE_CLA_ZERO, 293, JCOP4_RESPONSE_CAPACITY); assert(!f.calls);
	init(&f, 291, 0); error(&f, NULL, 2, JCOP4_RESPONSE_CLA_ZERO, 293, JCOP4_RESPONSE_ARGUMENT);
	init(&f, 291, 0); error(&f, (unsigned char *)"\x61\0", 2, (enum jcop4_response_cla)-1, 293, JCOP4_RESPONSE_ARGUMENT);
	init(&f, 291, 0); error(&f, (unsigned char *)"\x61\0", 2, JCOP4_RESPONSE_CLA_ZERO, 292, JCOP4_RESPONSE_ARGUMENT);
	puts("PASS unknown statuses/invalid policy/size/null/announced overflow are rejected before GET RESPONSE, without expanding operation or SW permissions");
	{
		/* OpenSSL CLI independently assembled: synthetic Kenc00..0F/Kmac10..1F,
		 * SSCresponse2, payload00..FF, IV=AES(Kenc,SSC), CBC and CMAC8.
		 * Payload is NOT an RSA signature, only a256-byte codec fixture. */
		const char *vector = "8782011101a959cb9ce3b93919547c3b6de833413d017b06eaea916ebaa313ed98e0dd5d453c021f999e5e33c58823046853f52d0f9646c85bd6c272c47b3dd31ba67942669ef4dae8aae71ee45e65365647c8388d64f4edea2df346bc2c82e5a9bcc4912c629c2985529ffe1b94727bd05d74423365372c3397b27b3023381badd995363f4f4a698ac3a327b73886e55e85c5187a7895bb00c2dcea656d352e60f1e803b0c9d949f32420a67dc7303895e40b342fb8e3f18a8d2aa7789b86ce34aae10df4e520b7cad8b02622bf3a6a63178ecf0ecf23b82913d81903daeb283fa4abcd8897d39d8db2ae3689b53880ccb517920b72850e050a907313bf2bd2624763bcd2ff8284577beaf23db4d4d964cbbd397b990290008e088b9ad0d8b32668d39000";
		unsigned char response[293], plain[256], header[4], body[32], synthetic_z[66] = {0};
		struct jcop4_channel_keys keys; struct jcop4_sm_channel channel; size_t size; unsigned int sw;
		assert(jcop4_ca_aes128_kdf(synthetic_z, sizeof(synthetic_z), &keys) == 0);
		for (int i = 0; i < 16; i++) { keys.encryption[i] = (unsigned char)i; keys.mac[i] = (unsigned char)(i + 16); }
		assert(strlen(vector) == 586);
		for (size_t i = 0; i < sizeof(response); i++) { unsigned int byte; assert(sscanf(vector + 2*i, "%2x", &byte) == 1); response[i] = byte; }
		init(&f, 291, 0);
		assert(jcop4_response_collect(response, sizeof(response), JCOP4_RESPONSE_CLA_ZERO, 293, exchange, &f,
			out, sizeof(out), &written, &report) == 0 && !f.calls && written == 293 && !memcmp(out, response, 293));
		for (size_t changed = 0; changed <= 291; changed++) {
			init(&f, 291, 0x0C); memcpy(f.data, response, 291);
			if (changed < 291) f.data[changed] ^= 1;
			assert(jcop4_sm_init(&channel, &keys) == 0);
			assert(jcop4_sm_seal(&channel, (unsigned char *)"\0\xb0\0\0", NULL, 0, 1, 256, header, body, sizeof(body), &size) == 0);
			assert(jcop4_response_collect((unsigned char *)"\x61\0", 2, JCOP4_RESPONSE_CLA_PROTECTED, 293,
				exchange, &f, out, sizeof(out), &written, &report) == 0 && f.calls == 2);
			assert(channel.phase == JCOP4_SM_WAITING && channel.keys.ssc[15] == 1); /* GET did not consume SSC. */
			memset(plain, 0xAA, sizeof(plain));
			int rv = jcop4_sm_open(&channel, out, written, plain, sizeof(plain), &size, &sw);
			if (changed == 291) {
				assert(rv == 0 && sw == 0x9000 && size == 256 && channel.keys.ssc[15] == 2);
				for (size_t i = 0; i < 256; i++) assert(plain[i] == (unsigned char)i);
			} else {
				assert(rv == 1 && !size && !sw && channel.phase == JCOP4_SM_INVALID);
				for (size_t i = 0; i < sizeof(plain); i++) assert(plain[i] == 0xAA);
			}
			jcop4_sm_invalidate(&channel);
		}
		OPENSSL_cleanse(&keys, sizeof(keys));
		puts("PASS independently generated293-byte fixture verifies one final MAC/SSC increment after collection; every payload byte altered is rejected before plaintext/status");
	}
	return 0;
}
