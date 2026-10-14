/* Native driver/channel plus real OpenSC dispatcher, software reader only. */
#include "libopensc/card-iasecc.c"
#include "libopensc/iasecc-sdo.c"
#include "libopensc/iasecc-chipdoc-sm.c"
#define main software_peer_main
#include "jcop4-channel-confirmation.c"
#undef main

static struct sc_context test_ctx;
static ossl3ctx_t test_provider, alternate_provider;
static struct sc_card test_card;
static struct sc_reader test_reader;
static struct iasecc_private_data test_prv;
static struct fixture peer;
static unsigned char initial_saved[137];
static unsigned int physical, locks, unlocks;
static int mf_error, mf_bit, reset_on_command, swap_provider_on_command, restrict_on_command;
static struct sc_reader_operations reader_ops;
static struct sc_card_operations card_ops;

#ifndef IASECC_CHIPDOC_REAL_LOCKS
int sc_lock(struct sc_card *card)
{ assert(card == &test_card && card->lock_count == 1); card->lock_count++; locks++; return SC_SUCCESS; }
int sc_unlock(struct sc_card *card)
{ assert(card == &test_card && card->lock_count == 2); card->lock_count--; unlocks++; return SC_SUCCESS; }
#endif

static void response_iv(unsigned int counter, unsigned char iv[16])
{
	unsigned char ssc[16] = {0}; ssc[15] = counter; int n, last;
	EVP_CIPHER_CTX *c = EVP_CIPHER_CTX_new(); assert(c);
	assert(EVP_EncryptInit_ex(c, EVP_aes_128_ecb(), NULL, peer.derived.keys.encryption, NULL) == 1);
	assert(EVP_CIPHER_CTX_set_padding(c, 0) == 1 && EVP_EncryptUpdate(c, iv, &n, ssc, 16) == 1);
	assert(EVP_EncryptFinal_ex(c, iv + n, &last) == 1 && n + last == 16); EVP_CIPHER_CTX_free(c);
}

static int mf_reply(sc_apdu_t *a)
{
	unsigned char iv[16], decrypted[16], auth[64] = {0}, mac[16], padded[16] = {0};
	const unsigned char mf_control[] = {0x6F,9,0x62,7,0x82,1,0x38,0x83,2,0x3F,0};
	int n, last, extended = a->cse == (SC_APDU_CASE_4_EXT);
	assert(a->cla == 0x0C && a->ins == 0xA4 && !a->p1 && !a->p2 && a->lc == (extended ? 33u : 32u));
	assert(a->le == (extended ? 291u : 256u) && a->resplen == a->le);
	assert(!memcmp(a->data, "\x87\x11\x01", 3));
	assert(!memcmp(a->data + 19, extended ? "\x97\x02\x01\0\x8e\x08" : "\x97\x01\0\x8e\x08", extended ? 6 : 5));
	response_iv(3, iv); EVP_CIPHER_CTX *c = EVP_CIPHER_CTX_new(); assert(c);
	assert(EVP_DecryptInit_ex(c, EVP_aes_128_cbc(), NULL, peer.derived.keys.encryption, iv) == 1);
	assert(EVP_CIPHER_CTX_set_padding(c, 0) == 1 && EVP_DecryptUpdate(c, decrypted, &n, a->data + 3, 16) == 1);
	assert(EVP_DecryptFinal_ex(c, decrypted + n, &last) == 1 && n + last == 16);
	assert(!memcmp(decrypted, "\x3f\0\x80", 3)); for (int i = 3; i < 16; i++) assert(!decrypted[i]);
	auth[15] = 3; memcpy(auth + 16, "\x0c\xa4\0\0\x80", 5);
	memcpy(auth + 32, a->data, extended ? 23 : 22); auth[extended ? 55 : 54] = 0x80;
	cmac(peer.derived.keys.mac, auth, sizeof(auth), mac);
	assert(!memcmp(mac, a->data + (extended ? 25 : 24), 8));
	if (mf_error == 1) { EVP_CIPHER_CTX_free(c); return -7; }
	memcpy(padded, mf_control, sizeof(mf_control)); padded[sizeof(mf_control)] = mf_error == 3 ? 0 : 0x80;
	if (mf_error == 5) padded[9] = 0x2F;
	response_iv(mf_error == 10 ? 2 : 4, iv);
	assert(EVP_EncryptInit_ex(c, EVP_aes_128_cbc(), NULL, peer.derived.keys.encryption, iv) == 1);
	memcpy(a->resp, "\x87\x11\x01", 3);
	assert(EVP_CIPHER_CTX_set_padding(c, 0) == 1 && EVP_EncryptUpdate(c, a->resp + 3, &n, padded, 16) == 1);
	assert(EVP_EncryptFinal_ex(c, a->resp + 3 + n, &last) == 1 && n + last == 16); EVP_CIPHER_CTX_free(c);
	memcpy(a->resp + 19, "\x99\x02\x90\0", 4); if (mf_error == 4) { a->resp[21] = 0x69; a->resp[22] = 0x82; }
	memset(auth, 0, sizeof(auth)); auth[15] = mf_error == 10 ? 2 : 4;
	memcpy(auth + 16, a->resp, 23); auth[39] = 0x80; cmac(peer.derived.keys.mac, auth, 48, mac);
	if (mf_error == 2) mac[mf_bit/8] ^= 1 << (mf_bit%8);
	memcpy(a->resp + 23, "\x8e\x08", 2); memcpy(a->resp + 25, mac, 8); a->resplen = 33;
	if (mf_error == 6) a->resplen = 0;
	if (mf_error == 7) { a->resplen = 0; a->sw1 = 0x61; a->sw2 = 0; }
	if (mf_error == 8) { a->resplen = 0; a->sw1 = 0x6C; a->sw2 = 0; }
	if (mf_error == 9) a->resplen = a->le + 1;
	if (mf_error == 11) return SC_ERROR_CARD_RESET;
	sc_mem_clear(decrypted, sizeof(decrypted)); sc_mem_clear(padded, sizeof(padded)); return SC_SUCCESS;
}

static void change_provider_after_io(void)
{
	if ((int)physical == swap_provider_on_command) test_ctx.ossl3ctx = &alternate_provider;
	if ((int)physical == restrict_on_command)
		assert(EVP_set_default_properties(test_provider.libctx, "provider=unavailable") == 1);
}

static int backend(struct sc_reader *reader, struct sc_apdu *a)
{
	unsigned char *wire = NULL, response[258]; size_t size = 0, received = 0;
	const unsigned long flags = SC_APDU_FLAGS_NO_SM | SC_APDU_FLAGS_NO_RETRY_WL | SC_APDU_FLAGS_NO_GET_RESP;
	assert(reader == &test_reader && test_card.lock_count >= 2 && (a->flags & flags) == flags);
	assert(!a->control && !(a->flags & (SC_APDU_FLAGS_CHAINING | SC_APDU_FLAGS_SM_CHAINING)));
	assert(sc_apdu_get_octets(&test_ctx, a, &wire, &size, SC_PROTO_T1) == SC_SUCCESS);
	physical++; a->sw1 = 0x90; a->sw2 = 0;
	if (peer.calls == 3) {
		assert(size == (a->cse == (SC_APDU_CASE_4_EXT) ? 42u : 38u));
		if (a->cse == (SC_APDU_CASE_4_EXT)) assert(!memcmp(wire + 4, "\0\0\x21", 3) && !memcmp(wire + 40, "\x01\x23", 2));
		free(wire); int rv = mf_reply(a);
		change_provider_after_io();
		if ((int)physical == reset_on_command) iasecc_card_reader_lock_obtained(&test_card, 1);
		return rv;
	}
	int rv = exchange(&peer, wire, size, response, sizeof(response), &received); free(wire);
	if (!rv && received != SIZE_MAX) {
		assert(received >= 2 && received - 2 <= a->resplen); memcpy(a->resp, response, received - 2);
		a->resplen = received - 2; a->sw1 = response[received-2]; a->sw2 = response[received-1];
	} else if (!rv) a->resplen = SIZE_MAX;
	change_provider_after_io();
	if ((int)physical == reset_on_command) iasecc_card_reader_lock_obtained(&test_card, 1);
	return rv;
}

static void defaults(void)
{
	iasecc_chipdoc_invalidate_card(&test_card);
	memset(&test_ctx, 0, sizeof(test_ctx)); memset(&test_card, 0, sizeof(test_card));
	memset(&test_reader, 0, sizeof(test_reader)); memset(&test_prv, 0, sizeof(test_prv));
	const struct iasecc_chipdoc_profile *p = iasecc_chipdoc_profile(IASECC_CHIPDOC_CR_RSA2048);
	test_ctx.ossl3ctx = &test_provider;
	test_card.ctx = &test_ctx; test_card.reader = &test_reader; test_card.drv_data = &test_prv; test_card.lock_count = 1;
	test_card.atr.len = p->atr_len; memcpy(test_card.atr.value, p->atr, p->atr_len); test_reader.atr = test_card.atr;
	test_card.caps = SC_CARD_CAP_APDU_EXT; test_card.max_send_size = 255; test_card.max_recv_size = 291;
	test_reader.active_protocol = SC_PROTO_T1; test_reader.max_send_size = 255; test_reader.max_recv_size = 65536;
	memset(&reader_ops, 0, sizeof(reader_ops)); reader_ops.transmit = backend; test_reader.ops = &reader_ops;
	memset(&card_ops, 0, sizeof(card_ops)); card_ops.card_reader_lock_obtained = iasecc_card_reader_lock_obtained; test_card.ops = &card_ops;
	test_prv.chipdoc_profile = p->id;
	struct sc_aid aid = { {0}, 0 }; aid.len = p->aid_len; memcpy(aid.value, p->aid, p->aid_len);
	struct iasecc_chipdoc_metadata metadata = {SC_ALGORITHM_RSA,0x91,2048,1,0xD004,137,18,1,1};
	assert(iasecc_chipdoc_prepare(&test_prv.chipdoc_session, &test_card, &aid, &metadata) == SC_SUCCESS);
	physical = locks = unlocks = 0; mf_error = mf_bit = reset_on_command = swap_provider_on_command = restrict_on_command = 0;
	peer.calls = peer.fail_step = peer.bit = peer.ga_template = 0; peer.mode = NORMAL; peer.le = JCOP4_CA_LE_SHORT_MAX;
	memcpy(peer.initial, initial_saved, sizeof(initial_saved)); jcop4_ca_pending_clear(&peer.derived);
}

/* Policy isolation is observed with real provider fetches, not mocked crypto. */
static void provider_checks(void)
{
	struct iasecc_chipdoc_channel_report report;
	OSSL_LIB_CTX *blocked = OSSL_LIB_CTX_new(); assert(blocked);
	OSSL_PROVIDER *null_provider = OSSL_PROVIDER_load(blocked, "null"); assert(null_provider);
	alternate_provider.libctx = OSSL_LIB_CTX_new(); assert(alternate_provider.libctx);
	alternate_provider.defprov = OSSL_PROVIDER_load(alternate_provider.libctx, "default"); assert(alternate_provider.defprov);
	unsigned char secret[66] = {0}, iv[16] = {0}, input[16] = {1}, output[16], expected[16], tag[16], pub[133];
	struct jcop4_channel_keys keys, expected_keys;
	struct jcop4_ca_pending pending;
	struct jcop4_sm_channel sm = {0}; size_t written = 0;
	unsigned char header[4], body[64], response[258], plain[137]; unsigned int sw = 0;
	pub[0] = 4; memcpy(pub + 1, initial_saved + 5, 132);
	assert(!jcop4_ca_aes128_kdf(secret, 66, &expected_keys));
	assert(!jcop4_sm_aes128_cbc(1, expected_keys.encryption, iv, input, 16, expected, 16));
	assert(!jcop4_sm_cmac(expected_keys.mac, input, 16, tag));
	/* A valid independent response is built before disabling global fetches. */
	defaults(); assert(!iasecc_chipdoc_channel_open(&test_card, peer.initial, 137, &report));
	memset(peer.derived.keys.ssc, 0, sizeof(peer.derived.keys.ssc));
	unsigned char command[19] = {0x0c,0xb0,0,0,13,0x97,1,0x89,0x8e,8};
	unsigned char auth[48] = {0}, mac[16]; auth[15] = 1;
	memcpy(auth + 16, "\x0c\xb0\0\0\x80", 5); memcpy(auth + 32, "\x97\x01\x89\x80", 4);
	cmac(peer.derived.keys.mac, auth, 48, mac); memcpy(command + 10, mac, 8);
	peer.calls = 2; size_t received = 0;
	assert(!exchange(&peer, command, sizeof(command), response, sizeof(response), &received));
	iasecc_chipdoc_invalidate_card(&test_card);
	assert(EVP_set_default_properties(NULL, "provider=unavailable") == 1);
	assert(jcop4_ca_aes128_kdf(secret, 66, &keys));
	assert(!jcop4_ca_aes128_kdf_ex(test_provider.libctx, secret, 66, &keys) && !memcmp(&keys, &expected_keys, sizeof(keys)));
	assert(!jcop4_sm_aes128_cbc_ex(test_provider.libctx, 1, keys.encryption, iv, input, 16, output, 16) && !memcmp(output, expected, 16));
	assert(!jcop4_sm_cmac_ex(test_provider.libctx, keys.mac, input, 16, output) && !memcmp(output, tag, 16));
	assert(!jcop4_sm_verify_mac8_ex(test_provider.libctx, keys.mac, input, 16, tag));
	assert(!jcop4_ca_prepare_ex(test_provider.libctx, pub, 133, &pending)); jcop4_ca_pending_clear(&pending);
	assert(!jcop4_sm_init(&sm, &peer.derived.keys));
	assert(!jcop4_sm_seal_policy_ex(test_provider.libctx, &sm, (unsigned char[]){0,0xb0,0,0}, NULL, 0, 1, 137,
		header, body, sizeof(body), &written, JCOP4_SM_LE_SHORT) && written == 13);
	assert(!jcop4_sm_open_report_policy_ex(test_provider.libctx, &sm, response, received, plain, sizeof(plain), &written, &sw, JCOP4_SM_OUTER_9000, NULL));
	assert(written == 137 && sw == 0x9000 && !memcmp(plain, initial_saved, 137)); jcop4_sm_invalidate(&sm);
	assert(EVP_set_default_properties(NULL, NULL) == 1);
	puts("PASS private libctx performs EC/ECDH/KDF/AES/CMAC and protected decode while global provider policy denies algorithms; independent ciphertext/MAC remains valid");
	for (int policy = 0; policy < 2; policy++) {
		OSSL_LIB_CTX *ctx = policy ? test_provider.libctx : blocked;
		if (policy) assert(EVP_set_default_properties(ctx, "provider=unavailable") == 1);
		memset(&keys, 0xA5, sizeof(keys)); assert(jcop4_ca_aes128_kdf_ex(ctx, secret, 66, &keys));
		assert(!memcmp(&keys, &(struct jcop4_channel_keys){0}, sizeof(keys)));
		memset(output, 0xA5, 16); assert(jcop4_sm_aes128_cbc_ex(ctx, 1, expected_keys.encryption, iv, input, 16, output, 16));
		for (int i = 0; i < 16; i++) assert(output[i] == 0xA5);
		assert(jcop4_sm_cmac_ex(ctx, expected_keys.mac, input, 16, output));
		for (int i = 0; i < 16; i++) assert(!output[i]);
		assert(jcop4_sm_verify_mac8_ex(ctx, expected_keys.mac, input, 16, tag));
		assert(jcop4_ca_prepare_ex(ctx, pub, 133, &pending));
		assert(!memcmp(&pending, &(struct jcop4_ca_pending){0}, sizeof(pending)));
		assert(jcop4_ca_prepare_key_ex(ctx, peer.peer, pub, 133, &pending));
		defaults(); test_ctx.ossl3ctx = policy ? &test_provider : &(ossl3ctx_t){.libctx = blocked};
		assert(iasecc_chipdoc_channel_open(&test_card, peer.initial, 137, &report) != 0 && !physical && !report.reset_required);
		assert(!test_prv.chipdoc_session.channel && test_prv.chipdoc_profile == IASECC_CHIPDOC_CR_RSA2048);
		if (policy) assert(EVP_set_default_properties(ctx, NULL) == 1);
		test_ctx.ossl3ctx = &test_provider;
	}
	for (int variant = 0; variant < 2; variant++) {
		defaults(); test_ctx.ossl3ctx = variant ? &(ossl3ctx_t){0} : NULL;
		assert(iasecc_chipdoc_channel_open(&test_card, peer.initial, 137, &report) != 0 && !physical && !report.reset_required);
		test_ctx.ossl3ctx = &test_provider;
	}
	puts("PASS absent OpenSC libctx, null provider and default-property restrictions stop before setup I/O with no global fallback or partial keys");
	for (int variant = 0; variant < 4; variant++) {
		defaults(); assert(!iasecc_chipdoc_channel_open(&test_card, peer.initial, 137, &report));
		OSSL_LIB_CTX *saved = test_provider.libctx;
		if (variant == 0) test_ctx.ossl3ctx = &alternate_provider;
		if (variant == 1) test_provider.libctx = alternate_provider.libctx;
		if (variant == 2) { static struct sc_context other; other = test_ctx; test_card.ctx = &other; }
		if (variant == 3) assert(EVP_set_default_properties(saved, "provider=unavailable") == 1);
		assert(iasecc_chipdoc_channel_select_mf(&test_card, IASECC_CHIPDOC_MF_EXTENDED, &report) != 0 && physical == 3 && report.reset_required);
		assert(!test_prv.chipdoc_session.channel && !report.authenticated && !report.sw);
		test_provider.libctx = saved; assert(EVP_set_default_properties(saved, NULL) == 1);
	}
	for (int step = 1; step <= 4; step++) for (int restrict_policy = 0; restrict_policy < 2; restrict_policy++) {
		defaults(); if (restrict_policy) restrict_on_command = step; else swap_provider_on_command = step;
		int rv = iasecc_chipdoc_channel_open(&test_card, peer.initial, 137, &report);
		if (step == 4) { assert(!rv); rv = iasecc_chipdoc_channel_select_mf(&test_card, IASECC_CHIPDOC_MF_EXTENDED, &report); }
		assert(rv != 0 && !test_prv.chipdoc_session.channel && report.reset_required && !report.authenticated && !report.sw);
		assert(physical == (unsigned)(restrict_policy && step == 1 ? 2 : step));
		assert(EVP_set_default_properties(test_provider.libctx, NULL) == 1);
	}
	puts("PASS context/owner/libctx replacement and changed policy before/during exchange discard the channel without further protected I/O, stale SW or global retry");
	defaults(); iasecc_chipdoc_invalidate_card(&test_card);
	OSSL_PROVIDER_unload(null_provider); OSSL_LIB_CTX_free(blocked);
	OSSL_PROVIDER_unload(alternate_provider.defprov); OSSL_LIB_CTX_free(alternate_provider.libctx);
	OPENSSL_cleanse(&keys, sizeof(keys)); OPENSSL_cleanse(&expected_keys, sizeof(expected_keys));
	OPENSSL_cleanse(plain, sizeof(plain)); OPENSSL_cleanse(mac, sizeof(mac)); OPENSSL_cleanse(tag, sizeof(tag));
}

#ifndef IASECC_CHIPDOC_CHANNEL_FIXTURE
int main(void)
{
	struct iasecc_chipdoc_channel_report report;
	test_provider.libctx = OSSL_LIB_CTX_new(); assert(test_provider.libctx);
	test_provider.defprov = OSSL_PROVIDER_load(test_provider.libctx, "default"); assert(test_provider.defprov);
	init(&peer); memcpy(initial_saved, peer.initial, sizeof(initial_saved));
	for (int encoding = 0; encoding < 2; encoding++) {
		defaults(); assert(iasecc_chipdoc_channel_open(&test_card, peer.initial, 137, &report) == 0);
		assert(report.authenticated && report.sw == 0x9000 && report.commands == 3 && report.reset_required && physical == 3);
		assert(test_prv.chipdoc_session.phase == IASECC_CHIPDOC_CHANNEL && test_prv.chipdoc_session.channel);
		struct chipdoc_crypto *crypto = test_prv.chipdoc_session.channel; assert(crypto->sm.keys.ssc[15] == 2);
		test_prv.cache.valid = 1; test_prv.cache.current_df = sc_file_new(); test_prv.cache.current_ef = sc_file_new();
		assert(test_prv.cache.current_df && test_prv.cache.current_ef);
		assert(iasecc_chipdoc_channel_select_mf(&test_card, (enum iasecc_chipdoc_mf_encoding)encoding, &report) == 0);
		assert(report.authenticated && report.sw == 0x9000 && report.commands == 1 && physical == 4 && locks == unlocks && test_card.lock_count == 1);
		assert(test_prv.chipdoc_session.phase == IASECC_CHIPDOC_MF && crypto->sm.keys.ssc[15] == 4);
		assert(!test_prv.cache.valid && !test_prv.cache.current_df && !test_prv.cache.current_ef);
		assert(iasecc_chipdoc_channel_select_mf(&test_card, (enum iasecc_chipdoc_mf_encoding)encoding, &report) != 0 && physical == 4);
	}
	puts("PASS real OpenSC validation/serializer/recursive locking confirm CA and MF short/extended using independent AES/CMAC peer, SSC2/4, no raw reader bypass or PIN");
	for (int step = 1; step <= 3; step++) for (int mode = TRANSPORT_ERROR; mode <= SETUP_PAYLOAD; mode++) {
		defaults(); peer.fail_step = step; peer.mode = (enum mode)mode; peer.bad_sw = 0x6100;
		if (step == 3 && mode == BAD_SW) continue;
		if (step == 3 && mode == SETUP_PAYLOAD) continue;
		assert(iasecc_chipdoc_channel_open(&test_card, peer.initial, 137, &report) != 0);
		assert(physical == (unsigned)step && !test_prv.chipdoc_session.channel && test_prv.chipdoc_profile == IASECC_CHIPDOC_CR_RSA2048);
		assert(report.reset_required && locks == unlocks);
	}
	for (int mode = PHYSICAL_ONLY; mode <= ZERO_DATA_IV; mode++) {
		defaults(); peer.mode = (enum mode)mode;
		assert(iasecc_chipdoc_channel_open(&test_card, peer.initial, 137, &report) != 0 && physical == 3 && !test_prv.chipdoc_session.channel);
	}
	puts("PASS setup/transport/size/61xx/payload/MAC/padding/inner error/content/IV failures never attach a context or authorize PIN and require explicit remote cleanup");
	for (int encoding = 0; encoding < 2; encoding++) for (int error = 1; error <= 11; error++) {
		defaults(); assert(iasecc_chipdoc_channel_open(&test_card, peer.initial, 137, &report) == 0); mf_error = error;
		assert(iasecc_chipdoc_channel_select_mf(&test_card, (enum iasecc_chipdoc_mf_encoding)encoding, &report) != 0);
		assert(physical == 4 && report.reset_required && !test_prv.chipdoc_session.channel && !test_prv.chipdoc_session.profile);
		assert(test_prv.chipdoc_profile == IASECC_CHIPDOC_CR_RSA2048 && locks == unlocks);
	}
	for (int bit = 0; bit < 64; bit++) {
		defaults(); assert(iasecc_chipdoc_channel_open(&test_card, peer.initial, 137, &report) == 0); mf_error = 2; mf_bit = bit;
		assert(iasecc_chipdoc_channel_select_mf(&test_card, IASECC_CHIPDOC_MF_EXTENDED, &report) != 0 && !report.authenticated && !report.sw);
	}
	puts("PASS MF transport/61xx/6Cxx/oversize/bare9000/replay/all64 MAC bits/padding/control/innerSW failures discard crypto and MSE without fallback/GET/retry");
	for (int stage = 1; stage <= 4; stage++) {
		defaults(); reset_on_command = stage;
		int rv = iasecc_chipdoc_channel_open(&test_card, peer.initial, 137, &report);
		if (stage == 4) { assert(rv == 0); rv = iasecc_chipdoc_channel_select_mf(&test_card, IASECC_CHIPDOC_MF_EXTENDED, &report); }
		assert(rv != 0 && physical == (unsigned)stage && !test_prv.chipdoc_session.channel && test_card.lock_count == 1);
	}
	puts("PASS reset hook during setup or protected exchange safely detaches/frees context before post-transport checks, with no stale ownership or use-after-free");
	for (int variant = 0; variant < 7; variant++) {
		defaults();
		if (variant == 0) test_card.lock_count = 0;
		if (variant == 1) test_ctx.debug = 1;
		if (variant == 2) test_card.sm_ctx.sm_mode = SM_MODE_TRANSMIT;
		if (variant == 3) test_card.caps = 0;
		if (variant == 4) test_prv.chipdoc_profile = IASECC_CHIPDOC_NONE;
		if (variant == 5) test_reader.max_recv_size = 290;
		if (variant == 6) peer.initial[5] = 2;
		assert(iasecc_chipdoc_channel_open(&test_card, peer.initial, 137, &report) != 0 && !physical && !report.authenticated);
	}
	defaults(); assert(iasecc_chipdoc_channel_open(&test_card, peer.initial, 136, &report) != 0 && !physical);
	defaults(); assert(iasecc_chipdoc_channel_open(&test_card, NULL, 137, &report) != 0 && !physical);
	defaults(); assert(iasecc_chipdoc_channel_open(&test_card, peer.initial, 137, NULL) != 0 && !physical);
	defaults(); assert(iasecc_chipdoc_channel_open(&test_card, peer.initial, 137, &report) == 0);
	void *owned = test_prv.chipdoc_session.channel;
	assert(iasecc_chipdoc_channel_open(&test_card, peer.initial, 137, &report) != 0 && test_prv.chipdoc_session.channel == owned && physical == 3);
	test_ctx.debug = 1;
	assert(iasecc_chipdoc_channel_open(&test_card, peer.initial, 137, &report) != 0 && test_prv.chipdoc_session.channel == owned && physical == 3 && report.reset_required);
	test_ctx.debug = 0;
	assert(iasecc_chipdoc_channel_select_mf(&test_card, 99, &report) != 0 && test_prv.chipdoc_session.channel == owned && physical == 3 && report.reset_required);
	provider_checks();
	iasecc_chipdoc_invalidate_card(&test_card); EVP_PKEY_free(peer.peer); jcop4_ca_pending_clear(&peer.derived);
	OSSL_PROVIDER_unload(test_provider.defprov); OSSL_LIB_CTX_free(test_provider.libctx);
	puts("PASS invalid profile/lock/debug/SM/capability/limits/point/length/null inputs stop before transport; local duplicate/invalid encoding never replaces a live channel");
	return 0;
}

#endif
