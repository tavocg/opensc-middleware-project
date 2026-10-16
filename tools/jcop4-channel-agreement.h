#ifndef MIDDLEWARE_JCOP4_CHANNEL_AGREEMENT_H
#define MIDDLEWARE_JCOP4_CHANNEL_AGREEMENT_H
#include "jcop4-channel-kdf.h"
/* Shared implementation owned by the local OpenSC submodule. */
#include "../vendor/opensc/src/libopensc/jcop4-channel-agreement.h"

/* Standalone diagnostic adapters; production passes an explicit libctx. */
static inline int
jcop4_ca_prepare_key(EVP_PKEY *local, const unsigned char *peer,
		size_t size, struct jcop4_ca_pending *pending)
{
	return jcop4_ca_prepare_key_ex(NULL, local, peer, size, pending);
}

static inline int
jcop4_ca_prepare(const unsigned char *peer, size_t size, struct jcop4_ca_pending *pending)
{
	return jcop4_ca_prepare_ex(NULL, peer, size, pending);
}

#endif
