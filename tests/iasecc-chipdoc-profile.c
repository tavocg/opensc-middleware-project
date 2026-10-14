/* Profile/state and real IAS lifecycle with synthetic card/context only.
 * No PCSC context, transport, credential, keys or real card metadata. */
#include <assert.h>
#include <stdio.h>
#include "libopensc/card-iasecc.c"
#include "libopensc/iasecc-sdo.c"

static int released, physical;
struct dummy_channel { unsigned char bytes[32]; struct iasecc_chipdoc_session *owner; int recurse; };

static void clear_channel(void *opaque)
{
	struct dummy_channel *p = opaque;
	assert(p && p->owner && !p->owner->channel && !p->owner->clear_channel);
	released++;
	sc_mem_clear(p->bytes, sizeof(p->bytes));
	if (p->recurse) iasecc_chipdoc_invalidate(p->owner);
}

int sc_transmit_apdu(struct sc_card *card, struct sc_apdu *apdu)
{ (void)card; (void)apdu; physical++; return SC_ERROR_NOT_ALLOWED; }

static struct sc_context ctx;
static struct sc_card card;
static struct sc_reader reader;
static struct sc_aid aid;
static struct iasecc_private_data prv;
static struct dummy_channel dummy;
static struct iasecc_chipdoc_metadata metadata;

static void defaults(void)
{
	const struct iasecc_chipdoc_profile *p = iasecc_chipdoc_profile(IASECC_CHIPDOC_CR_RSA2048);
	memset(&ctx, 0, sizeof(ctx)); memset(&card, 0, sizeof(card)); memset(&reader, 0, sizeof(reader));
	memset(&prv, 0, sizeof(prv)); memset(&dummy, 0, sizeof(dummy)); memset(&aid, 0, sizeof(aid));
	card.ctx = &ctx; card.reader = &reader; card.drv_data = &prv; card.type = SC_CARD_TYPE_IASECC_AMOS;
	card.atr.len = p->atr_len; memcpy(card.atr.value, p->atr, p->atr_len); reader.atr = card.atr;
	reader.active_protocol = SC_PROTO_T1; reader.max_send_size = 255; reader.max_recv_size = 65536;
	card.max_send_size = 255; card.max_recv_size = 291; card.caps = SC_CARD_CAP_APDU_EXT;
	aid.len = p->aid_len; memcpy(aid.value, p->aid, p->aid_len);
	dummy.owner = &prv.chipdoc_session; memset(dummy.bytes, 0xA5, sizeof(dummy.bytes));
	metadata = (struct iasecc_chipdoc_metadata){SC_ALGORITHM_RSA, 0x91, 2048, 1, 0xD004, 137, 18, 1, 1};
	released = physical = 0;
}

static void attach(void)
{
	assert(iasecc_chipdoc_prepare(&prv.chipdoc_session, &card, &aid, &metadata) == SC_SUCCESS);
	assert(iasecc_chipdoc_attach(&prv.chipdoc_session, prv.chipdoc_session.generation, &dummy, clear_channel) == SC_SUCCESS);
}

static void advance(enum iasecc_chipdoc_response step)
{
	assert(iasecc_chipdoc_record(&prv.chipdoc_session, prv.chipdoc_session.generation, step,
		step == IASECC_CHIPDOC_RESPONSE_QUERY ? 0x63C7 : 0x9000,
		step == IASECC_CHIPDOC_RESPONSE_MSE ? 0x91 : 0, step == IASECC_CHIPDOC_RESPONSE_MSE ? 2048 : 0) == SC_SUCCESS);
}

static void authenticated(void)
{
	attach(); advance(IASECC_CHIPDOC_RESPONSE_MF); advance(IASECC_CHIPDOC_RESPONSE_QUERY);
	advance(IASECC_CHIPDOC_RESPONSE_LOGIN); advance(IASECC_CHIPDOC_RESPONSE_APPLICATION);
}

int main(void)
{
	const struct iasecc_chipdoc_profile *p = iasecc_chipdoc_profile(IASECC_CHIPDOC_CR_RSA2048);
	const unsigned long flags = SC_ALGORITHM_RSA_PAD_PKCS1_TYPE_01 | SC_ALGORITHM_RSA_HASH_SHA256;
	defaults(); assert(p && !iasecc_chipdoc_profile(IASECC_CHIPDOC_NONE) && !iasecc_chipdoc_profile(99));
	assert(iasecc_chipdoc_candidate(&card.atr, &aid) == p->id);
	for (size_t i = 0; i < card.atr.len; i++) {
		card.atr.value[i] ^= 1; assert(iasecc_chipdoc_candidate(&card.atr, &aid) == IASECC_CHIPDOC_NONE); card.atr.value[i] ^= 1;
	}
	for (size_t i = 0; i < aid.len; i++) {
		aid.value[i] ^= 1; assert(iasecc_chipdoc_candidate(&card.atr, &aid) == IASECC_CHIPDOC_NONE); aid.value[i] ^= 1;
	}
	assert(!iasecc_chipdoc_candidate(NULL, &aid) && !iasecc_chipdoc_candidate(&card.atr, NULL));
	aid.len--; assert(!iasecc_chipdoc_candidate(&card.atr, &aid)); aid.len++;
	card.atr.len--; assert(!iasecc_chipdoc_candidate(&card.atr, &aid)); card.atr.len++;
	/* JCOP3 application, or eOI sharing a candidate ATR, stays outside this profile. */
	memcpy(aid.value, "\xE8\x28\xBD\x08\x0F\xD2\x50\x47\x65\x6E\x65\x72\x69\x63", 14); aid.len = 14;
	assert(!iasecc_chipdoc_candidate(&card.atr, &aid));
	puts("PASS candidate requires exact ATR and announced AID, rejects all byte/length/null variants and another application on the same chip");
	for (int variation = 0; variation < 9; variation++) {
		defaults();
		if (variation == 0) card.caps = 0;
		if (variation == 1) reader.active_protocol = SC_PROTO_T0;
		if (variation == 2) reader.max_recv_size = 290;
		if (variation == 3) reader.max_send_size = 138;
		if (variation == 4) card.max_recv_size = 290;
		if (variation == 5) card.max_send_size = 138;
		if (variation == 6) reader.atr.value[0] ^= 1;
		if (variation == 7) card.reader = NULL;
		if (variation == 8) aid.value[0] ^= 1;
		unsigned long caps = card.caps;
		assert(iasecc_chipdoc_prepare(&prv.chipdoc_session, &card, &aid, &metadata) != SC_SUCCESS);
		assert(prv.chipdoc_session.phase == IASECC_CHIPDOC_INACTIVE && !prv.chipdoc_session.channel && card.caps == caps && !physical);
	}
	puts("PASS card and reader EXT/T1/limits/candidate are checked locally without manufacturing capability or calling transport");
	for (int variation = 0; variation < 11; variation++) {
		defaults();
		if (variation == 0) metadata.algorithm = SC_ALGORITHM_EC;
		if (variation == 1) metadata.reference = 0x90;
		if (variation == 2) metadata.bits = 3072;
		if (variation == 3) metadata.pin_reference = 2;
		if (variation == 4) metadata.dh_file = 0xD003;
		if (variation == 5) metadata.dh_size = 136;
		if (variation == 6) metadata.curve_selector = 0x17;
		if (variation == 7) metadata.native = 0;
		if (variation == 8) metadata.signing = 0;
		if (variation == 10) metadata.curve_selector = 0x18; /* 24, not the observed decimal18. */
		assert(iasecc_chipdoc_prepare(&prv.chipdoc_session, &card, &aid, variation == 9 ? NULL : &metadata) != SC_SUCCESS);
		assert(!prv.chipdoc_session.profile && !prv.chipdoc_session.channel && !physical);
	}
	puts("PASS shared ATR/AID are only a candidate: ECC/other key/reference/PIN/D004/size/selector/usage/native/null public metadata cannot prepare this session");
	defaults(); authenticated();
	assert(iasecc_chipdoc_signing_key(&prv.chipdoc_session, 0x91, 2048, flags) == SC_SUCCESS);
	for (int variant = 0; variant < 6; variant++) {
		struct iasecc_chipdoc_session before = prv.chipdoc_session;
		unsigned long f = variant == 3 ? SC_ALGORITHM_RSA_PAD_PKCS1_TYPE_01 | SC_ALGORITHM_RSA_HASH_SHA1 :
			variant == 4 ? SC_ALGORITHM_RSA_RAW : variant == 5 ? flags | SC_ALGORITHM_ONBOARD_KEY_GEN : flags;
		assert(iasecc_chipdoc_signing_key(&prv.chipdoc_session, variant < 2 ? (variant ? 0x11 : 0x90) : 0x91,
			variant == 2 ? 3072 : 2048, f) == SC_ERROR_NOT_SUPPORTED);
		assert(!memcmp(&before, &prv.chipdoc_session, sizeof(before)) && !physical);
	}
	advance(IASECC_CHIPDOC_RESPONSE_MSE); assert(prv.chipdoc_session.key_reference == 0x91 && prv.chipdoc_session.key_bits == 2048);
	assert(iasecc_chipdoc_signing_key(&prv.chipdoc_session, 0x91, 2048, flags) != SC_SUCCESS);
	advance(IASECC_CHIPDOC_RESPONSE_SIGNATURE);
	assert(prv.chipdoc_session.phase == IASECC_CHIPDOC_APPLICATION && !prv.chipdoc_session.key_reference && !prv.chipdoc_session.key_bits);
	iasecc_chipdoc_invalidate(&prv.chipdoc_session);
	assert(released == 1 && !physical);
	puts("PASS public key metadata restricts measured RSA2048/SHA256/ref91, rejects unsupported local requests without changing state, and consumes each MSE once");
	for (int step = 0; step <= IASECC_CHIPDOC_RESPONSE_SIGNATURE; step++) {
		for (int fault = 0; fault < 4; fault++) {
			defaults(); attach(); for (int i = 0; i < step; i++) advance((enum iasecc_chipdoc_response)i);
			uint64_t generation = prv.chipdoc_session.generation;
			assert(iasecc_chipdoc_record(&prv.chipdoc_session, generation + (fault == 0),
				fault == 1 ? (enum iasecc_chipdoc_response)99 : (enum iasecc_chipdoc_response)step,
				fault == 2 ? 0x6982 : step == IASECC_CHIPDOC_RESPONSE_QUERY ? 0x63C7 : 0x9000,
				fault == 3 ? 0x90 : step == IASECC_CHIPDOC_RESPONSE_MSE ? 0x91 : 0,
				step == IASECC_CHIPDOC_RESPONSE_MSE ? 2048 : 0) != SC_SUCCESS);
			assert(released == 1 && !prv.chipdoc_session.channel && !prv.chipdoc_session.profile && !physical);
		}
	}
	for (unsigned int sw = 0x63C0; sw <= 0x63CF; sw++) if (sw != 0x63C7) {
		defaults(); attach(); advance(IASECC_CHIPDOC_RESPONSE_MF);
		assert(iasecc_chipdoc_record(&prv.chipdoc_session, prv.chipdoc_session.generation, IASECC_CHIPDOC_RESPONSE_QUERY, sw, 0, 0) != SC_SUCCESS && released == 1);
	}
	for (int expected = 0; expected <= IASECC_CHIPDOC_RESPONSE_SIGNATURE; expected++) {
		for (int wrong = 0; wrong <= IASECC_CHIPDOC_RESPONSE_SIGNATURE; wrong++) if (wrong != expected) {
			defaults(); attach(); for (int i = 0; i < expected; i++) advance((enum iasecc_chipdoc_response)i);
			assert(iasecc_chipdoc_record(&prv.chipdoc_session, prv.chipdoc_session.generation, (enum iasecc_chipdoc_response)wrong,
				wrong == IASECC_CHIPDOC_RESPONSE_QUERY ? 0x63C7 : 0x9000,
				wrong == IASECC_CHIPDOC_RESPONSE_MSE ? 0x91 : 0, wrong == IASECC_CHIPDOC_RESPONSE_MSE ? 2048 : 0) != SC_SUCCESS);
			assert(released == 1 && !prv.chipdoc_session.channel && !physical);
		}
	}
	puts("PASS stale/out-of-order/unknown/error replies at every operation drop ownership and authorizations, query63C7 is a diagnostic precondition without storing a maximum or remaining count");
	defaults();
	assert(iasecc_chipdoc_prepare(&prv.chipdoc_session, &card, &aid, &metadata) == SC_SUCCESS);
	assert(iasecc_chipdoc_attach(&prv.chipdoc_session, prv.chipdoc_session.generation - 1, &dummy, clear_channel) != SC_SUCCESS);
	assert(!prv.chipdoc_session.channel && !released && dummy.bytes[0] == 0xA5);
	assert(iasecc_chipdoc_attach(&prv.chipdoc_session, prv.chipdoc_session.generation, &dummy, NULL) != SC_SUCCESS);
	assert(iasecc_chipdoc_attach(&prv.chipdoc_session, prv.chipdoc_session.generation, NULL, clear_channel) != SC_SUCCESS);
	assert(iasecc_chipdoc_attach(&prv.chipdoc_session, prv.chipdoc_session.generation, &dummy, clear_channel) == SC_SUCCESS);
	uint64_t old = prv.chipdoc_session.generation;
	assert(iasecc_chipdoc_prepare(&prv.chipdoc_session, &card, &aid, &metadata) != SC_SUCCESS);
	assert(iasecc_chipdoc_attach(&prv.chipdoc_session, old, &dummy, clear_channel) != SC_SUCCESS);
	dummy.recurse = 1; iasecc_chipdoc_invalidate(&prv.chipdoc_session); iasecc_chipdoc_invalidate(&prv.chipdoc_session);
	assert(released == 1); for (size_t i = 0; i < sizeof(dummy.bytes); i++) assert(!dummy.bytes[i]);
	assert(iasecc_chipdoc_prepare(&prv.chipdoc_session, &card, &aid, &metadata) == SC_SUCCESS);
	assert(prv.chipdoc_session.generation > old && iasecc_chipdoc_attach(&prv.chipdoc_session, old, &dummy, clear_channel) != SC_SUCCESS);
	iasecc_chipdoc_invalidate(&prv.chipdoc_session); prv.chipdoc_session.generation = UINT64_MAX;
	iasecc_chipdoc_invalidate(&prv.chipdoc_session); assert(prv.chipdoc_session.generation == UINT64_MAX);
	assert(iasecc_chipdoc_prepare(&prv.chipdoc_session, &card, &aid, &metadata) != SC_SUCCESS);
	puts("PASS ownership transfers only once, failed attachment stays caller-owned, recursive/repeated cleanup is safe and generation never wraps or revives stale contexts");
	defaults();
	struct iasecc_chipdoc_session other = {0}; struct dummy_channel other_channel = {{0}, &other, 0};
	attach(); assert(iasecc_chipdoc_prepare(&other, &card, &aid, &metadata) == SC_SUCCESS);
	assert(iasecc_chipdoc_attach(&other, other.generation, &other_channel, clear_channel) == SC_SUCCESS);
	advance(IASECC_CHIPDOC_RESPONSE_MF);
	assert(other.phase == IASECC_CHIPDOC_CHANNEL && other.channel == &other_channel);
	iasecc_chipdoc_invalidate(&prv.chipdoc_session);
	assert(released == 1 && other.channel == &other_channel && other.phase == IASECC_CHIPDOC_CHANNEL);
	iasecc_chipdoc_invalidate(&other); assert(released == 2 && !physical);
	puts("PASS per-card ownership and operation state are independent without global login/channel caches");
	defaults(); authenticated(); prv.cache.valid = 1; prv.key_size = 256; prv.op_method = 1; prv.op_ref = 0x91;
	prv.chipdoc_profile = IASECC_CHIPDOC_CR_RSA2048;
	memset(&prv.security_env, 0xA5, sizeof(prv.security_env));
	assert(iasecc_card_reader_lock_obtained(&card, 0) == SC_SUCCESS && !released && prv.cache.valid);
	assert(iasecc_card_reader_lock_obtained(&card, 1) == SC_SUCCESS && released == 1 && !prv.cache.valid && !prv.key_size && !prv.op_method && !prv.op_ref);
	struct sc_security_env empty = {0}; assert(!memcmp(&prv.security_env, &empty, sizeof(empty)));
	assert(iasecc_card_reader_lock_obtained(&card, 1) == SC_SUCCESS && released == 1 && !physical);
	assert(prv.chipdoc_profile == IASECC_CHIPDOC_CR_RSA2048 && !prv.chipdoc_session.profile);
	/* A session already invalidated by a crypto failure still has a binding.
 * The next reset must discard any remaining driver-level MSE/cache state. */
	prv.cache.valid = 1; prv.key_size = 256; memset(&prv.security_env, 0xA5, sizeof(prv.security_env));
	assert(iasecc_card_reader_lock_obtained(&card, 1) == SC_SUCCESS && released == 1 && !prv.cache.valid && !prv.key_size);
	assert(!memcmp(&prv.security_env, &empty, sizeof(empty)) && prv.chipdoc_profile == IASECC_CHIPDOC_CR_RSA2048);
	defaults(); prv.cache.valid = 1; prv.key_size = 256;
	assert(iasecc_card_reader_lock_obtained(&card, 1) == SC_SUCCESS && prv.cache.valid && prv.key_size == 256 && !physical);
	assert(iasecc_card_reader_lock_obtained(NULL, 1) == SC_SUCCESS);
	puts("PASS real IAS reset hook clears new-profile channel/cache/MSE and preserves legacy IAS cache/behavior, ordinary lock acquisition keeps the new session");
	defaults(); authenticated(); assert(iasecc_logout(&card) == SC_SUCCESS && released == 1 && !physical);
	assert(iasecc_logout(&card) == SC_SUCCESS && released == 1);
	defaults(); struct iasecc_private_data *allocated = calloc(1, sizeof(prv)); assert(allocated);
	card.drv_data = allocated; dummy.owner = &allocated->chipdoc_session;
	assert(iasecc_chipdoc_prepare(&allocated->chipdoc_session, &card, &aid, &metadata) == SC_SUCCESS);
	assert(iasecc_chipdoc_attach(&allocated->chipdoc_session, allocated->chipdoc_session.generation, &dummy, clear_channel) == SC_SUCCESS);
	assert(iasecc_finish(&card) == SC_SUCCESS && released == 1 && !card.drv_data && !physical);
	puts("PASS real IAS logout and finish discard channel ownership even without EF.ATR, cleanup precedes driver-data free without card APDU");
	return 0;
}
