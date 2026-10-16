#ifndef MIDDLEWARE_JCOP4_CHANNEL_KDF_H
#define MIDDLEWARE_JCOP4_CHANNEL_KDF_H
/* Shared implementation owned by the local OpenSC submodule. */
#include "../vendor/opensc/src/libopensc/jcop4-channel-kdf.h"

/* Standalone diagnostic adapters; production passes an explicit libctx. */
static inline int
jcop4_ca_aes128_kdf(const unsigned char *secret, size_t size,
		struct jcop4_channel_keys *keys)
{
	return jcop4_ca_aes128_kdf_ex(NULL, secret, size, keys);
}

#endif
