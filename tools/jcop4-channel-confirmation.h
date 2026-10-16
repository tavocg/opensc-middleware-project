#ifndef MIDDLEWARE_JCOP4_CHANNEL_CONFIRMATION_H
#define MIDDLEWARE_JCOP4_CHANNEL_CONFIRMATION_H
#include "jcop4-channel-agreement.h"
#include "jcop4-channel-messages.h"
/* Shared implementation owned by the local OpenSC submodule. */
#include "../vendor/opensc/src/libopensc/jcop4-channel-confirmation.h"

/* Standalone diagnostic adapters; production passes an explicit libctx. */
static inline int
jcop4_ca_confirm_d004_session(const unsigned char *initial, size_t initial_size,
		enum jcop4_ca_outer_le outer_le, jcop4_ca_exchange exchange, void *context,
		struct jcop4_ca_probe_report *report, struct jcop4_sm_channel *confirmed)
{
	return jcop4_ca_confirm_d004_session_ex(NULL, initial, initial_size, outer_le, exchange, context, report, confirmed);
}

static inline int
jcop4_ca_confirm_d004(const unsigned char *initial, size_t initial_size,
		enum jcop4_ca_outer_le outer_le, jcop4_ca_exchange exchange, void *context,
		struct jcop4_ca_probe_report *report)
{
	return jcop4_ca_confirm_d004_session(initial, initial_size, outer_le, exchange, context, report, NULL);
}

#endif
