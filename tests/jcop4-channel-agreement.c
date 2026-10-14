/* Public ECDH fixture: RFC5903 section8.3 (P-521), not an IKE implementation.
 * https://www.rfc-editor.org/rfc/rfc5903.html#section-8.3
 * KDF expectations separately computed with Python hashlib on the published
 * 66-byte shared X and SHA1 counters00000001/00000002. No actual card data. */
#include "../tools/jcop4-channel-agreement.h"
#include <openssl/param_build.h>
#include <openssl/err.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static const char *private_a =
	"0037ADE9319A89F4DABDB3EF411AACCCA5123C61ACAB57B5393DCE47608172A0"
	"95AA85A30FE1C2952C6771D937BA9777F5957B2639BAB072462F68C27A57382D4A52";
static const char *private_b =
	"0145BA99A847AF43793FDD0E872E7CDFA16BE30FDC780F97BCCC3F078380201E"
	"9C677D600B343757A3BDBF2A3163E4C2F869CCA7458AA4A4EFFC311F5CB151685EB9";
static const char *public_a = "04"
	"0015417E84DBF28C0AD3C278713349DC7DF153C897A1891BD98BAB4357C9ECBE"
	"E1E3BF42E00B8E380AEAE57C2D107564941885942AF5A7F4601723C4195D176CED3E"
	"017CAE20B6641D2EEB695786D8C946146239D099E18E1D5A514C739D7CB4A10A"
	"D8A788015AC405D7799DC75E7B7D5B6CF2261A6A7F1507438BF01BEB6CA3926F9582";
static const char *public_b = "04"
	"00D0B3975AC4B799F5BEA16D5E13E9AF971D5E9B984C9F39728B5E5739735A21"
	"9B97C356436ADC6E95BB0352F6BE64A6C2912D4EF2D0433CED2B6171640012D9460F"
	"015C68226383956E3BD066E797B623C27CE0EAC2F551A10C2C724D9852077B87"
	"220B6536C5C408A1D2AEBB8E86D678AE49CB57091F4732296579AB44FCD17F0FC56A";
static const char *generator = "04"
	"00C6858E06B70404E9CD9E3ECB662395B4429C648139053FB521F828AF606B4D3D"
	"BAA14B5E77EFE75928FE1DC127A2FFA8DE3348B3C1856A429BF97E7E31C2E5BD66"
	"011839296A789A3BC0045C8A5FB42C7D1BD998F54449579B446817AFBD17273E"
	"662C97EE72995EF42640C550B9013FAD0761353C7086A272C24088BE94769FD16650";

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

/* Test-only import. All private scalars are published or synthetic fixtures. */
static EVP_PKEY *fixture(const char *private_hex, const unsigned char *public_point)
{
	EVP_PKEY *key = NULL;
	EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_from_name(NULL, "EC", NULL);
	OSSL_PARAM_BLD *builder = OSSL_PARAM_BLD_new(); OSSL_PARAM *params;
	BIGNUM *priv = NULL;
	assert(ctx && builder);
	assert(OSSL_PARAM_BLD_push_utf8_string(builder, OSSL_PKEY_PARAM_GROUP_NAME, "secp521r1", 0) == 1);
	assert(OSSL_PARAM_BLD_push_octet_string(builder, OSSL_PKEY_PARAM_PUB_KEY, public_point, 133) == 1);
	if (private_hex) {
		assert(BN_hex2bn(&priv, private_hex));
		assert(OSSL_PARAM_BLD_push_BN(builder, OSSL_PKEY_PARAM_PRIV_KEY, priv) == 1);
	}
	params = OSSL_PARAM_BLD_to_param(builder); assert(params);
	assert(EVP_PKEY_fromdata_init(ctx) == 1);
	assert(EVP_PKEY_fromdata(ctx, &key, private_hex ? EVP_PKEY_KEYPAIR : EVP_PKEY_PUBLIC_KEY, params) == 1 && key);
	OSSL_PARAM_free(params); OSSL_PARAM_BLD_free(builder); BN_clear_free(priv); EVP_PKEY_CTX_free(ctx);
	return key;
}

static void cleared(const struct jcop4_ca_pending *pending)
{
	const unsigned char *p = (const unsigned char *)pending;
	for (size_t i = 0; i < sizeof(*pending); i++) assert(!p[i]);
}

int main(void)
{
	unsigned char a[133], b[133], g[133], expected[16], altered[256], output[256];
	struct jcop4_ca_pending pending, reverse, second;
	EVP_PKEY *key_a, *key_b; size_t written;
	assert(hex(a, public_a) == 133 && hex(b, public_b) == 133 && hex(g, generator) == 133);
	key_a = fixture(private_a, a); key_b = fixture(private_b, b);
	{
		assert(jcop4_ca_prepare_key(key_a, b, sizeof(b), &pending) == 0);
		assert(!memcmp(pending.terminal_public, a, sizeof(a)));
		assert(hex(expected, "9bd06cdd776262b2bdde7a42a81e2c16") == 16 && !memcmp(pending.keys.encryption, expected, 16));
		assert(hex(expected, "f4ef4dc315b5d9f1d8207309688cdce6") == 16 && !memcmp(pending.keys.mac, expected, 16));
		for (int i = 0; i < 16; i++) assert(!pending.keys.ssc[i]);
		assert(jcop4_ca_prepare_key(key_b, a, sizeof(a), &reverse) == 0);
		assert(!memcmp(reverse.terminal_public, b, sizeof(b)) && !memcmp(&pending.keys, &reverse.keys, sizeof(pending.keys)));
		puts("PASS RFC5903 P-521 keypairs derive both independent CA key fixtures symmetrically and start SSC at zero");
	}
	{
		EVP_PKEY *one = fixture("01", g);
		assert(jcop4_ca_prepare_key(one, g, sizeof(g), &pending) == 0);
		assert(!memcmp(pending.terminal_public, g, sizeof(g)));
		assert(hex(expected, "685908e4b1b7b8108b8d585e0afac152") == 16 && !memcmp(pending.keys.encryption, expected, 16));
		assert(hex(expected, "7f6f7b295d2afac49c24fbfc71243801") == 16 && !memcmp(pending.keys.mac, expected, 16));
		EVP_PKEY_free(one);
		puts("PASS shared X with a leading zero retains the exact66-byte width before KDF");
	}
	{
		assert(jcop4_ca_prepare(b, sizeof(b), &pending) == 0);
		assert(jcop4_p521_public_valid(pending.terminal_public, sizeof(pending.terminal_public)));
		assert(jcop4_ca_prepare_key(key_b, pending.terminal_public, sizeof(pending.terminal_public), &reverse) == 0);
		assert(!memcmp(&pending.keys, &reverse.keys, sizeof(pending.keys)));
		assert(jcop4_ca_prepare(b, sizeof(b), &second) == 0);
		assert(memcmp(pending.terminal_public, second.terminal_public, sizeof(pending.terminal_public)));
		assert(jcop4_ca_prepare_key(key_b, second.terminal_public, sizeof(second.terminal_public), &reverse) == 0);
		assert(!memcmp(&second.keys, &reverse.keys, sizeof(second.keys)));
		puts("PASS internally generated ephemeral keys are distinct and agree with a synthetic software peer without exporting private scalars");
	}
	{
		for (size_t byte = 0; byte < sizeof(b); byte++) for (int bit = 0; bit < 8; bit++) {
			memcpy(altered, b, sizeof(b)); altered[byte] ^= (unsigned char)(1 << bit);
			assert(!jcop4_p521_public_valid(altered, sizeof(b)));
			memset(&pending, 0xAA, sizeof(pending));
			assert(jcop4_ca_prepare_key(key_a, altered, sizeof(b), &pending) == 1); cleared(&pending);
		}
		for (size_t size = 0; size <= 134; size++) if (size != 133) {
			memset(&pending, 0xAA, sizeof(pending));
			assert(jcop4_ca_prepare_key(key_a, b, size, &pending) == 1); cleared(&pending);
		}
		memcpy(altered, b, 133); altered[0] = 2; /* Compressed/hybrid/infinity encodings are outside this subset. */
		assert(jcop4_ca_prepare(altered, 133, &pending) == 1); cleared(&pending);
		assert(jcop4_ca_prepare(NULL, 133, &pending) == 1); cleared(&pending);
		assert(jcop4_ca_prepare(b, SIZE_MAX, &pending) == 1); cleared(&pending);
		assert(jcop4_ca_prepare_key(NULL, b, 133, &pending) == 1); cleared(&pending);
		assert(jcop4_ca_prepare(b, 133, NULL) == 1 && jcop4_ca_prepare_key(key_a, b, 133, NULL) == 1);
		puts("PASS invalid/off-curve point bits, lengths, encodings and pointers fail and clear the pending output");
	}
	{
		EC_GROUP *group = EC_GROUP_new_by_curve_name(NID_secp521r1);
		BIGNUM *prime = BN_new(), *value = BN_bin2bn(g + 1, 66, NULL);
		assert(group && prime && value && EC_GROUP_get_curve(group, prime, NULL, NULL, NULL) == 1);
		memcpy(altered, g, 133); assert(BN_bn2binpad(prime, altered + 1, 66) == 66);
		assert(!jcop4_p521_public_valid(altered, 133));
		assert(BN_add(value, value, prime) == 1 && BN_bn2binpad(value, altered + 1, 66) == 66);
		/* x+p maps to the same field element but is not a canonical coordinate. */
		assert(jcop4_ca_prepare_key(key_a, altered, 133, &pending) == 1); cleared(&pending);
		BN_clear_free(value); value = BN_bin2bn(g + 67, 66, NULL); assert(value);
		memcpy(altered, g, 133); assert(BN_add(value, value, prime) == 1 && BN_bn2binpad(value, altered + 67, 66) == 66);
		assert(jcop4_ca_prepare_key(key_a, altered, 133, &pending) == 1); cleared(&pending);
		memset(altered, 0, 133); altered[0] = 4;
		assert(jcop4_ca_prepare_key(key_a, altered, 133, &pending) == 1); cleared(&pending);
		BN_clear_free(value); BN_clear_free(prime); EC_GROUP_free(group);
		puts("PASS coordinates at/above field prime and normalized aliases are rejected before provider import");
	}
	{
		EVP_PKEY *bad_keys[5];
		bad_keys[0] = fixture(NULL, a); /* Public-only local key. */
		bad_keys[1] = fixture(private_a, b); /* Mismatched private/public. */
		bad_keys[2] = fixture("00", a); /* Invalid zero scalar. */
		bad_keys[3] = EVP_EC_gen("prime256v1");
		bad_keys[4] = EVP_PKEY_Q_keygen(NULL, NULL, "ED25519");
		for (size_t i = 0; i < 5; i++) {
			assert(bad_keys[i]); memset(&pending, 0xAA, sizeof(pending));
			assert(jcop4_ca_prepare_key(bad_keys[i], b, 133, &pending) == 1); cleared(&pending);
			EVP_PKEY_free(bad_keys[i]);
		}
		ERR_clear_error();
		assert(EVP_set_default_properties(NULL, "provider=unavailable_for_jcop4_test") == 1);
		memset(&pending, 0xAA, sizeof(pending));
		assert(jcop4_ca_prepare(b, 133, &pending) == 1); cleared(&pending);
		assert(EVP_set_default_properties(NULL, NULL) == 1); ERR_clear_error();
		puts("PASS public-only/mismatched/zero-scalar/wrong-curve/type keys and unavailable provider fail without fallback");
	}
	{
		const unsigned char prefix[] = {0x10, 0x86, 0, 0, 139, 0x7C, 0x81, 136, 0x80, 0x81, 133};
		assert(jcop4_ca_build_mse(output, sizeof(output), &written) == 0 && written == 17);
		assert(!memcmp(output, "\x00\x22\x41\xa4\x0c\x80\x0a\x04\x00\x7f\x00\x07\x02\x02\x03\x02\x02", 17));
		memset(output, 0xAA, sizeof(output));
		assert(jcop4_ca_build_ga(a, sizeof(a), output, sizeof(output), &written) == 0 && written == 144);
		assert(!memcmp(output, prefix, sizeof(prefix)) && !memcmp(output + 11, a, sizeof(a)) && output[144] == 0xAA);
		puts("PASS candidate case3 MSE17/GA144 bytes preserve CLA10, exact P-521 TLV lengths and absent Le without transmitting anything");
	}
	{
		memset(output, 0xAA, sizeof(output)); written = 123;
		assert(jcop4_ca_build_mse(output, 16, &written) == 1 && !written);
		assert(jcop4_ca_build_mse(NULL, 17, &written) == 1 && !written);
		assert(jcop4_ca_build_mse(output, 17, NULL) == 1);
		assert(jcop4_ca_build_ga(a, 133, output, 143, &written) == 1 && !written);
		assert(jcop4_ca_build_ga(NULL, 133, output, 256, &written) == 1 && !written);
		assert(jcop4_ca_build_ga(a, 132, output, 256, &written) == 1 && !written);
		assert(jcop4_ca_build_ga(a, 133, NULL, 256, &written) == 1 && !written);
		assert(jcop4_ca_build_ga(a, 133, output, 256, NULL) == 1);
		for (size_t i = 0; i < sizeof(output); i++) assert(output[i] == 0xAA);
		jcop4_ca_pending_clear(&pending); cleared(&pending);
		jcop4_ca_pending_clear(&reverse); cleared(&reverse);
		jcop4_ca_pending_clear(&second); cleared(&second); jcop4_ca_pending_clear(NULL);
		puts("PASS candidate builders reject invalid input/capacity without partial output and pending keys can be explicitly erased");
	}
	EVP_PKEY_free(key_a); EVP_PKEY_free(key_b);
	return 0;
}
