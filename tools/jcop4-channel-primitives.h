#ifndef MIDDLEWARE_JCOP4_CHANNEL_PRIMITIVES_H
#define MIDDLEWARE_JCOP4_CHANNEL_PRIMITIVES_H
/* Shared implementation owned by the local OpenSC submodule. */
#include "../vendor/opensc/src/libopensc/jcop4-channel-primitives.h"

/* Standalone diagnostic adapters; production passes an explicit libctx. */
static inline int
jcop4_sm_aes128_cbc(int encrypt, const unsigned char key[16], const unsigned char iv[16],
		const unsigned char *input, size_t size, unsigned char *output, size_t capacity)
{
	return jcop4_sm_aes128_cbc_ex(NULL, encrypt, key, iv, input, size, output, capacity);
}

static inline int
jcop4_sm_cmac(const unsigned char key[16], const unsigned char *input,
		size_t size, unsigned char output[16])
{
	return jcop4_sm_cmac_ex(NULL, key, input, size, output);
}

static inline int
jcop4_sm_verify_mac8(const unsigned char key[16], const unsigned char *input,
		size_t size, const unsigned char received[8])
{
	return jcop4_sm_verify_mac8_ex(NULL, key, input, size, received);
}

#endif
