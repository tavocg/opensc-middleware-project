/* Offline synthetic fixtures, independently assembled in Python with the
 * OpenSSL CLI (enc -aes-128-cbc -nopad; mac CMAC cipher:AES-128-CBC).
 * Kenc=00..0f, Kmac=10..1f, IV=0, SSCcmd=1, SSCresponse=2, ISO method2 padding.
 * Command MAC: SSC||pad(header), with pad(body concatenation) only when body
 * is present; response MAC: pad(SSC||87(optional)||99). First eight MAC bytes.
 * Long command fixtures use data[i]=i mod256, header00DA0000, Le256;
 * Python hashlib SHA256 summarizes their entire independently assembled body.
 * These fixtures test the candidate encoding, not its applicability to a card. */
#include "../tools/jcop4-channel-messages.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static const unsigned char empty_header[4] = {0, 0xCA, 1, 0};
static struct jcop4_channel_keys fixture_keys;

static size_t hex(unsigned char *out, const char *text)
{
	size_t size = strlen(text) / 2;
	assert(strlen(text) == size * 2);
	for (size_t i = 0; i < size; i++) {
		unsigned int value;
		assert(sscanf(text + i * 2, "%2x", &value) == 1);
		out[i] = (unsigned char)value;
	}
	return size;
}

static void waiting(struct jcop4_sm_channel *channel)
{
	unsigned char header[4], body[64]; size_t size;
	assert(jcop4_sm_init(channel, &fixture_keys) == 0);
	assert(jcop4_sm_seal(channel, empty_header, NULL, 0, 0, 0, header, body, sizeof(body), &size) == 0);
	assert(channel->phase == JCOP4_SM_WAITING && channel->keys.ssc[15] == 1);
}

static void invalid(const struct jcop4_sm_channel *channel)
{
	const unsigned char *keys = (const unsigned char *)&channel->keys;
	assert(channel->phase == JCOP4_SM_INVALID);
	for (size_t i = 0; i < sizeof(channel->keys); i++) assert(!keys[i]);
}

static void rejected(const unsigned char *response, size_t size, size_t capacity)
{
	struct jcop4_sm_channel channel;
	unsigned char plain[64], header[4], body[64]; size_t written = 123; unsigned int sw = 0x1234;
	waiting(&channel); memset(plain, 0xAA, sizeof(plain));
	assert(jcop4_sm_open(&channel, response, size, plain, capacity, &written, &sw) == 1);
	assert(!written && !sw); invalid(&channel);
	for (size_t i = 0; i < sizeof(plain); i++) assert(plain[i] == 0xAA);
	assert(jcop4_sm_seal(&channel, empty_header, NULL, 0, 0, 0, header, body, sizeof(body), &written) == 1);
}

int main(void)
{
	struct jcop4_sm_channel channel;
	unsigned char header[4], body[4096], expected[4096], plain[4096], response[128], altered[128];
	size_t written, response_size; unsigned int sw;
	for (int i = 0; i < 16; i++) { fixture_keys.encryption[i] = (unsigned char)i; fixture_keys.mac[i] = (unsigned char)(i + 16); }
	{
		const unsigned char select[4] = {0, 0xA4, 2, 0}, data[2] = {0x3F, 0};
		size_t size = hex(expected, "871101b1007bb97d86ab8fd872cea2401e24e79701008e08b2c9b82ca77f4701");
		assert(jcop4_sm_init(&channel, &fixture_keys) == 0);
		assert(jcop4_sm_seal(&channel, select, data, 2, 1, 256, header, body, sizeof(body), &written) == 0);
		assert(written == size && !memcmp(body, expected, size));
		assert(!memcmp(header, "\x0c\xa4\x02\x00", 4) && channel.keys.ssc[15] == 1);
		size = hex(expected, "8e089375a542f08d13e6");
		assert(jcop4_sm_init(&channel, &fixture_keys) == 0);
		assert(jcop4_sm_seal(&channel, empty_header, NULL, 0, 0, 0, header, body, sizeof(body), &written) == 0);
		assert(written == size && !memcmp(body, expected, size));
		size = hex(expected, "9701108e08f2b7f9d0b613447e");
		assert(jcop4_sm_init(&channel, &fixture_keys) == 0);
		assert(jcop4_sm_seal(&channel, empty_header, NULL, 0, 1, 16, header, body, sizeof(body), &written) == 0);
		assert(written == size && !memcmp(body, expected, size));
		puts("PASS synthetic independent command fixtures cover encrypted87, Le97 and header-only MAC without extra padding");
	}
	{
		const unsigned char data_header[4] = {0, 0xDA, 0, 0};
		const size_t sizes[] = {128, 384, 3584}, body_sizes[] = {161, 418, 3618};
		const char *digests[] = {"1c70041ff2808ffd0e8c640ee0c1ecef0c7781ac620378c6ec9769aa92375e19",
			"1a1a04af47a3434c3a0dd6da335441823b49e1545a19ecbbd90996c4ed463444",
			"267b864592725f403aa1e4cbe9cb1c09e404b395e8ff0cc611c2c11497b11ee6"};
		for (size_t i = 0; i < sizeof(plain); i++) plain[i] = (unsigned char)i;
		for (size_t i = 0; i < 3; i++) {
			unsigned char digest[32]; unsigned int digest_size = 0;
			assert(jcop4_sm_init(&channel, &fixture_keys) == 0);
			assert(jcop4_sm_seal(&channel, data_header, plain, sizes[i], 1, 256, header, body, sizeof(body), &written) == 0);
			assert(written == body_sizes[i] && body[1] == (i ? 0x82 : 0x81));
			assert(EVP_Digest(body, written, digest, &digest_size, EVP_sha256(), NULL) == 1 && digest_size == 32);
			assert(hex(expected, digests[i]) == 32 && !memcmp(digest, expected, 32));
		}
		puts("PASS independent long-body hashes cover canonical81/82 lengths and the local3584-byte data limit");
	}
	response_size = hex(response, "871101657c8e4ab0d61d6dc57232817bffbb7f990290008e0834a63af6815184259000");
	{
		waiting(&channel); memset(plain, 0xAA, sizeof(plain));
		assert(jcop4_sm_open(&channel, response, response_size, plain, sizeof(plain), &written, &sw) == 0);
		assert(written == 14 && !memcmp(plain, "PUBLIC FIXTURE", 14) && plain[14] == 0xAA && sw == 0x9000);
		assert(channel.phase == JCOP4_SM_IDLE && channel.keys.ssc[15] == 2);
		waiting(&channel); memcpy(altered, response, response_size);
		assert(jcop4_sm_open(&channel, altered, response_size, altered, sizeof(altered), &written, &sw) == 0);
		assert(written == 14 && !memcmp(altered, "PUBLIC FIXTURE", 14) && sw == 0x9000);
		written = hex(altered, "990269828e08cb3ef064b14ca89a9000"); waiting(&channel);
		memset(plain, 0xAA, sizeof(plain));
		assert(jcop4_sm_open(&channel, altered, written, plain, 0, &written, &sw) == 0);
		assert(!written && sw == 0x6982 && plain[0] == 0xAA && channel.phase == JCOP4_SM_IDLE);
		puts("PASS authenticated responses publish plaintext or the exact inner error status; in-place decode succeeds");
	}
	{
		/* All bits, not just the MAC: ciphertext, structural bytes, inner/outer SW. */
		for (size_t i = 0; i < response_size; i++) for (int bit = 0; bit < 8; bit++) {
			memcpy(altered, response, response_size); altered[i] ^= (unsigned char)(1 << bit);
			rejected(altered, response_size, 64);
		}
		written = hex(altered, "871101c6a13b37878f5b826f4f8162a1c8d879990290008e083495a0ac9db801779000");
		rejected(altered, written, 64); /* Valid MAC, plaintext has no ISO80 padding marker. */
		puts("PASS every changed response bit and valid-MAC/invalid-padding fixture fail without exposing plaintext or SW and wipe keys");
	}
	{
		for (size_t i = 0; i < response_size; i++) rejected(response, i, 64);
		const char *bad[] = {"99810290008e0834a63af6815184259000", /* Noncanonical81. */
			"9982000290008e0834a63af6815184259000", /* Leading zero82. */
			"9980900000008e0834a63af6815184259000", /* Indefinite. */
			"998301000090008e0834a63af6815184259000", /* Three-byte length. */
			"99029000990290008e0834a63af6815184259000", /* Duplicate99. */
			"8e0834a63af681518425990290009000", /* Reordered. */
			"990290008e0834a63af68151842500009000", /* Trailing object bytes. */
			"990290009000", /* Missing MAC. */
			"850100990290008e0834a63af6815184259000"}; /* Unsupported85. */
		for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
			written = hex(altered, bad[i]); rejected(altered, written, 64);
		}
		puts("PASS truncation, noncanonical/indefinite lengths, duplicates, reordering, trailing bytes and unsupported85 are rejected");
	}
	{
		waiting(&channel); written = 123;
		assert(jcop4_sm_seal(&channel, empty_header, NULL, 0, 0, 0, header, body, sizeof(body), &written) == 1 && !written);
		assert(channel.phase == JCOP4_SM_WAITING && channel.keys.ssc[15] == 1);
		assert(jcop4_sm_open(&channel, response, response_size, plain, sizeof(plain), &written, &sw) == 0);
		assert(jcop4_sm_seal(&channel, empty_header, NULL, 0, 0, 0, header, body, sizeof(body), &written) == 0);
		assert(channel.keys.ssc[15] == 3);
		assert(jcop4_sm_open(&channel, response, response_size, plain, sizeof(plain), &written, &sw) == 1);
		invalid(&channel); /* Old MAC/SSC2 cannot satisfy SSC4. */
		assert(jcop4_sm_init(&channel, &fixture_keys) == 0);
		assert(jcop4_sm_open(&channel, response, response_size, plain, sizeof(plain), &written, &sw) == 1);
		invalid(&channel);
		puts("PASS pending commands cannot interleave; replay and unsolicited responses invalidate the local channel");
	}
	{
		struct jcop4_sm_channel saved;
		assert(jcop4_sm_init(&channel, &fixture_keys) == 0); saved = channel;
		memset(body, 0xAA, sizeof(body)); memset(header, 0xAA, sizeof(header));
		assert(jcop4_sm_seal(&channel, empty_header, NULL, 0, 0, 0, header, body, 9, &written) == 1);
		assert(!written && !memcmp(&channel, &saved, sizeof(channel)) && body[0] == 0xAA && header[0] == 0xAA);
		assert(jcop4_sm_seal(&channel, empty_header, NULL, 1, 0, 0, header, body, sizeof(body), &written) == 1);
		assert(jcop4_sm_seal(&channel, empty_header, plain, 3585, 0, 0, header, body, sizeof(body), &written) == 1);
		assert(jcop4_sm_seal(&channel, empty_header, plain, SIZE_MAX, 0, 0, header, body, sizeof(body), &written) == 1);
		assert(jcop4_sm_seal(&channel, empty_header, NULL, 0, 1, 257, header, body, sizeof(body), &written) == 1);
		assert(jcop4_sm_seal(&channel, empty_header, NULL, 0, 1, 0, header, body, sizeof(body), &written) == 1);
		assert(jcop4_sm_seal(&channel, empty_header, NULL, 0, 2, 0, header, body, sizeof(body), &written) == 1);
		assert(jcop4_sm_seal(&channel, NULL, NULL, 0, 0, 0, header, body, sizeof(body), &written) == 1);
		assert(jcop4_sm_seal(&channel, (const unsigned char *)"\x10\x86\0\0", NULL, 0, 0, 0, header, body, sizeof(body), &written) == 1);
		assert(!memcmp(&channel, &saved, sizeof(channel)) && body[0] == 0xAA && header[0] == 0xAA);
		rejected(response, response_size, 13);
		waiting(&channel);
		assert(jcop4_sm_open(&channel, response, response_size, NULL, 64, &written, &sw) == 1); invalid(&channel);
		waiting(&channel);
		assert(jcop4_sm_open(&channel, response, response_size, plain, 64, NULL, &sw) == 1 && !sw); invalid(&channel);
		waiting(&channel);
		assert(jcop4_sm_open(&channel, response, response_size, plain, 64, &written, NULL) == 1 && !written); invalid(&channel);
		puts("PASS local construction errors preserve unsent state/output; response capacity/pointer errors discard the context");
	}
	{
		assert(jcop4_sm_init(&channel, &fixture_keys) == 0); memset(channel.keys.ssc, 0xFF, 16);
		assert(jcop4_sm_seal(&channel, empty_header, NULL, 0, 0, 0, header, body, sizeof(body), &written) == 1); invalid(&channel);
		waiting(&channel); memset(channel.keys.ssc, 0xFF, 16);
		assert(jcop4_sm_open(&channel, response, response_size, plain, sizeof(plain), &written, &sw) == 1); invalid(&channel);
		waiting(&channel); jcop4_sm_invalidate(&channel); invalid(&channel); /* Caller transport failure. */
		assert(jcop4_sm_init(&channel, NULL) == 1); invalid(&channel);
		assert(jcop4_sm_init(NULL, &fixture_keys) == 1);
		{
			unsigned char synthetic_secret[66] = {0}; struct jcop4_channel_keys derived;
			assert(jcop4_ca_aes128_kdf(synthetic_secret, sizeof(synthetic_secret), &derived) == 0);
			assert(jcop4_sm_init(&channel, &derived) == 0 && !memcmp(&channel.keys, &derived, sizeof(derived)));
			jcop4_sm_invalidate(&channel); OPENSSL_cleanse(&derived, sizeof(derived));
		}
		puts("PASS SSC overflow never resets; explicit invalidation and failed initialization wipe keys; synthetic KDF keys can initialize codec");
	}
	{
		const size_t sizes[] = {0, 1, 127, 128, 255, 256, 3584};
		for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
			const unsigned char *value; size_t total = 0, offset = 0, length;
			assert(jcop4_sm_append_tlv(body, sizeof(body), &total, 0x87, plain, sizes[i]) == 0);
			assert(jcop4_sm_take_tlv(body, total, &offset, 0x87, &value, &length) == 0);
			assert(offset == total && length == sizes[i] && !memcmp(value, plain, length));
			for (size_t end = 0; end < total; end++) {
				offset = 0; assert(jcop4_sm_take_tlv(body, end, &offset, 0x87, &value, &length) == 1 && !offset);
			}
		}
		puts("PASS bounded TLV parsing covers short/81/82 boundaries and rejects every truncation without advancing offset");
	}
	jcop4_sm_invalidate(&channel);
	return 0;
}
