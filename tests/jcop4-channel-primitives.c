/* Public NIST SP800-38A F.2.1/F.2.2 and AES_CMAC.pdf AES128 examples.
 * All data/keys here are public vectors or synthetic, never card/session data. */
#include "../tools/jcop4-channel-primitives.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static void from_hex(unsigned char *out, size_t size, const char *hex)
{
	assert(strlen(hex) == size * 2);
	for (size_t i = 0; i < size; i++) {
		unsigned int value;
		assert(sscanf(hex + i * 2, "%2x", &value) == 1);
		out[i] = (unsigned char)value;
	}
}

int main(void)
{
	unsigned char key[16], iv[16], plain[64], cipher[64], actual[64], recovered[64];
	from_hex(key, 16, "2b7e151628aed2a6abf7158809cf4f3c");
	from_hex(iv, 16, "000102030405060708090a0b0c0d0e0f");
	from_hex(plain, 64, "6bc1bee22e409f96e93d7e117393172aae2d8a571e03ac9c9eb76fac45af8e51"
			"30c81c46a35ce411e5fbc1191a0a52eff69f2445df4f9b17ad2b417be66c3710");
	from_hex(cipher, 64, "7649abac8119b246cee98e9b12e9197d5086cb9b507219ee95db113a917678b2"
			"73bed6b8e3c1743b7116e69e222295163ff1caa1681fac09120eca307586e1a7");
	assert(jcop4_sm_aes128_cbc(1, key, iv, plain, 64, actual, 64) == 0);
	assert(!memcmp(actual, cipher, 64));
	assert(jcop4_sm_aes128_cbc(0, key, iv, cipher, 64, recovered, 64) == 0);
	assert(!memcmp(recovered, plain, 64));
	memcpy(actual, plain, 64);
	assert(jcop4_sm_aes128_cbc(1, key, iv, actual, 64, actual, 64) == 0);
	assert(!memcmp(actual, cipher, 64));
	puts("PASS AES128 CBC encrypt/decrypt matches four-block NIST vectors and supports in-place output");
	{
		unsigned char zero_iv[16] = {0}, expected[16];
		/* CBC's first block with IV0 equals the NIST ECB-AES128 first block. */
		from_hex(expected, 16, "3ad77bb40d7a3660a89ecaf32466ef97");
		assert(jcop4_sm_aes128_cbc(1, key, zero_iv, plain, 16, actual, 64) == 0);
		assert(!memcmp(actual, expected, 16));
		assert(memcmp(actual, cipher, 16));
		puts("PASS candidate zero-IV CBC matches public first-block vector and differs from the nonzero-IV case");
	}
	{
		const size_t sizes[] = {0, 1, 15, 4097, SIZE_MAX};
		unsigned char sentinel[64]; memset(sentinel, 0xAA, sizeof(sentinel));
		for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
			memcpy(actual, sentinel, 64);
			assert(jcop4_sm_aes128_cbc(1, key, iv, plain, sizes[i], actual, 64) == 1);
			assert(!memcmp(actual, sentinel, 64));
		}
		assert(jcop4_sm_aes128_cbc(1, key, iv, plain, 16, actual, 15) == 1);
		assert(jcop4_sm_aes128_cbc(2, key, iv, plain, 16, actual, 64) == 1);
		assert(jcop4_sm_aes128_cbc(1, NULL, iv, plain, 16, actual, 64) == 1);
		assert(jcop4_sm_aes128_cbc(1, key, NULL, plain, 16, actual, 64) == 1);
		assert(jcop4_sm_aes128_cbc(1, key, iv, NULL, 16, actual, 64) == 1);
		assert(jcop4_sm_aes128_cbc(1, key, iv, plain, 16, NULL, 64) == 1);
		assert(!memcmp(actual, sentinel, 64));
		puts("PASS CBC rejects bad sizes/capacities/modes/pointers without publishing partial plaintext or ciphertext");
	}
	{
		unsigned char large[4096], encrypted[4096], decrypted[4096], mac[16];
		for (size_t i = 0; i < sizeof(large); i++) large[i] = (unsigned char)i;
		assert(jcop4_sm_aes128_cbc(1, key, iv, large, sizeof(large), encrypted, sizeof(encrypted)) == 0);
		assert(jcop4_sm_aes128_cbc(0, key, iv, encrypted, sizeof(encrypted), decrypted, sizeof(decrypted)) == 0);
		assert(!memcmp(large, decrypted, sizeof(large)));
		assert(jcop4_sm_cmac(key, large, sizeof(large), mac) == 0);
		assert(jcop4_sm_verify_mac8(key, large, sizeof(large), mac) == 0);
		puts("PASS CBC and CMAC accept the exact local4096-byte boundary with complete output buffers");
	}
	{
		const size_t sizes[] = {0, 16, 20, 64};
		const char *tags[] = {"bb1d6929e95937287fa37d129b756746", "070a16b46b4d4144f79bdd9dd04a287c",
			"7d85449ea6ea19c823a7bf78837dfade", "51f0bebf7e3b9d92fc49741779363cfe"};
		unsigned char tag[16], expected[16], tampered[8];
		for (size_t i = 0; i < 4; i++) {
			const unsigned char *input = sizes[i] ? plain : NULL;
			from_hex(expected, 16, tags[i]);
			assert(jcop4_sm_cmac(key, input, sizes[i], tag) == 0);
			assert(!memcmp(tag, expected, 16));
			assert(jcop4_sm_verify_mac8(key, input, sizes[i], expected) == 0);
			for (int bit = 0; bit < 64; bit++) {
				memcpy(tampered, expected, 8); tampered[bit / 8] ^= (unsigned char)(1 << (bit % 8));
				assert(jcop4_sm_verify_mac8(key, input, sizes[i], tampered) == 1);
			}
		}
		from_hex(expected, 16, tags[3]);
		memcpy(recovered, plain, 64); recovered[0] ^= 1;
		assert(jcop4_sm_verify_mac8(key, recovered, 64, expected) == 1);
		assert(jcop4_sm_verify_mac8(key, plain, 64, NULL) == 1);
		memset(tag, 0xAA, 16); assert(jcop4_sm_cmac(NULL, plain, 64, tag) == 1);
		for (int i = 0; i < 16; i++) assert(!tag[i]);
		assert(jcop4_sm_cmac(key, NULL, 1, tag) == 1);
		assert(jcop4_sm_cmac(key, plain, SIZE_MAX, tag) == 1);
		puts("PASS NIST CMAC vectors cover empty/full/partial/multiple blocks and truncated MAC rejects every flipped bit or changed message");
	}
	{
		unsigned char counter[16] = {0}, next[16], sentinel[16];
		assert(jcop4_sm_next_ssc(counter, next) == 0 && next[15] == 1);
		for (int i = 0; i < 15; i++) assert(!next[i]);
		counter[15] = 0xFF;
		assert(jcop4_sm_next_ssc(counter, counter) == 0 && counter[14] == 1 && counter[15] == 0);
		memset(counter, 0xFF, 16); memset(sentinel, 0xAA, 16); memcpy(next, sentinel, 16);
		assert(jcop4_sm_next_ssc(counter, next) == 1 && !memcmp(next, sentinel, 16));
		assert(jcop4_sm_next_ssc(counter, counter) == 1);
		for (int i = 0; i < 16; i++) assert(counter[i] == 0xFF);
		assert(jcop4_sm_next_ssc(NULL, next) == 1 && jcop4_sm_next_ssc(counter, NULL) == 1);
		puts("PASS SSC increments big-endian with carry, permits in-place updates and rejects overflow without resetting the counter");
	}
	{
		unsigned char data[4096] = {0}, padded[4096] = {0};
		size_t size, unpadded;
		/* Embedded marker/zeros are plaintext, not a removable padding block. */
		data[0] = 0x80;
		for (size_t input_size = 0; input_size < 4096; input_size++) {
			assert(jcop4_sm_pad(data, input_size, padded, sizeof(padded), &size) == 0);
			assert(size % 16 == 0 && size > input_size && size <= input_size + 16);
			assert(!memcmp(data, padded, input_size) && padded[input_size] == 0x80);
			assert(jcop4_sm_unpad(padded, size, &unpadded) == 0 && unpadded == input_size);
		}
		memset(padded, 0, sizeof(padded));
		assert(jcop4_sm_unpad(padded, 16, &unpadded) == 1 && !unpadded);
		padded[0] = 0x80; assert(jcop4_sm_unpad(padded, 32, &unpadded) == 1);
		padded[31] = 1; assert(jcop4_sm_unpad(padded, 32, &unpadded) == 1);
		assert(jcop4_sm_pad(data, SIZE_MAX, padded, sizeof(padded), &size) == 1 && !size);
		assert(jcop4_sm_pad(data, 4096, padded, sizeof(padded), &size) == 1);
		assert(jcop4_sm_pad(data, 16, padded, 16, &size) == 1);
		assert(jcop4_sm_pad(NULL, 1, padded, sizeof(padded), &size) == 1);
		assert(jcop4_sm_unpad(padded, 15, &unpadded) == 1);
		puts("PASS method2 padding preserves embedded markers, covers empty/aligned/large inputs and rejects malformed final padding or overflow");
	}
	return 0;
}
