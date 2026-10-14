/* Native driver/channel plus real OpenSC dispatcher, software reader only. */
#include "libopensc/card-iasecc.c"
#include "libopensc/iasecc-sdo.c"
#include "libopensc/iasecc-chipdoc-sm.c"
#define main software_peer_main
#include "jcop4-channel-confirmation.c"
#undef main

static struct sc_context test_ctx;
static struct sc_card test_card;
static struct sc_reader test_reader;
static struct iasecc_private_data test_prv;
static struct fixture peer;
static unsigned char initial_saved[137];
static unsigned int physical, locks, unlocks;
static int mf_error, mf_bit, reset_on_command;
static struct sc_reader_operations reader_ops;
static struct sc_card_operations card_ops;

int sc_lock(struct sc_card *card)
{ assert(card == &test_card && card->lock_count == 1); card->lock_count++; locks++; return SC_SUCCESS; }
int sc_unlock(struct sc_card *card)
{ assert(card == &test_card && card->lock_count == 2); card->lock_count--; unlocks++; return SC_SUCCESS; }

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

static int backend(struct sc_reader *reader, struct sc_apdu *a)
{
	unsigned char *wire = NULL, response[258]; size_t size = 0, received = 0;
	const unsigned long flags = SC_APDU_FLAGS_NO_SM | SC_APDU_FLAGS_NO_RETRY_WL | SC_APDU_FLAGS_NO_GET_RESP;
	assert(reader == &test_reader && test_card.lock_count == 2 && (a->flags & flags) == flags);
	assert(!a->control && !(a->flags & (SC_APDU_FLAGS_CHAINING | SC_APDU_FLAGS_SM_CHAINING)));
	assert(sc_apdu_get_octets(&test_ctx, a, &wire, &size, SC_PROTO_T1) == SC_SUCCESS);
	physical++; a->sw1 = 0x90; a->sw2 = 0;
	if (peer.calls == 3) {
		assert(size == (a->cse == (SC_APDU_CASE_4_EXT) ? 42u : 38u));
		if (a->cse == (SC_APDU_CASE_4_EXT)) assert(!memcmp(wire + 4, "\0\0\x21", 3) && !memcmp(wire + 40, "\x01\x23", 2));
		free(wire); int rv = mf_reply(a);
		if ((int)physical == reset_on_command) iasecc_card_reader_lock_obtained(&test_card, 1);
		return rv;
	}
	int rv = exchange(&peer, wire, size, response, sizeof(response), &received); free(wire);
	if (!rv && received != SIZE_MAX) {
		assert(received >= 2 && received - 2 <= a->resplen); memcpy(a->resp, response, received - 2);
		a->resplen = received - 2; a->sw1 = response[received-2]; a->sw2 = response[received-1];
	} else if (!rv) a->resplen = SIZE_MAX;
	if ((int)physical == reset_on_command) iasecc_card_reader_lock_obtained(&test_card, 1);
	return rv;
}

static void defaults(void)
{
	iasecc_chipdoc_invalidate_card(&test_card);
	memset(&test_ctx, 0, sizeof(test_ctx)); memset(&test_card, 0, sizeof(test_card));
	memset(&test_reader, 0, sizeof(test_reader)); memset(&test_prv, 0, sizeof(test_prv));
	const struct iasecc_chipdoc_profile *p = iasecc_chipdoc_profile(IASECC_CHIPDOC_CR_RSA2048);
	test_card.ctx = &test_ctx; test_card.reader = &test_reader; test_card.drv_data = &test_prv; test_card.lock_count = 1;
	test_card.atr.len = p->atr_len; memcpy(test_card.atr.value, p->atr, p->atr_len); test_reader.atr = test_card.atr;
	test_card.caps = SC_CARD_CAP_APDU_EXT; test_card.max_send_size = 255; test_card.max_recv_size = 291;
	test_reader.active_protocol = SC_PROTO_T1; test_reader.max_send_size = 255; test_reader.max_recv_size = 65536;
	memset(&reader_ops, 0, sizeof(reader_ops)); reader_ops.transmit = backend; test_reader.ops = &reader_ops;
	memset(&card_ops, 0, sizeof(card_ops)); card_ops.card_reader_lock_obtained = iasecc_card_reader_lock_obtained; test_card.ops = &card_ops;
	test_prv.chipdoc_profile = p->id;
	struct sc_aid aid = { {0}, 0 }; aid.len = p->aid_len; memcpy(aid.value, p->aid, p->aid_len);
	struct iasecc_chipdoc_metadata metadata = {SC_ALGORITHM_RSA,0x91,2048,1,0xD004,137,0x18,1,1};
	assert(iasecc_chipdoc_prepare(&test_prv.chipdoc_session, &test_card, &aid, &metadata) == SC_SUCCESS);
	physical = locks = unlocks = 0; mf_error = mf_bit = reset_on_command = 0;
	peer.calls = peer.fail_step = peer.bit = peer.ga_template = 0; peer.mode = NORMAL; peer.le = JCOP4_CA_LE_SHORT_MAX;
	memcpy(peer.initial, initial_saved, sizeof(initial_saved)); jcop4_ca_pending_clear(&peer.derived);
}

int main(void)
{
	struct iasecc_chipdoc_channel_report report;
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
	iasecc_chipdoc_invalidate_card(&test_card); EVP_PKEY_free(peer.peer); jcop4_ca_pending_clear(&peer.derived);
	puts("PASS invalid profile/lock/debug/SM/capability/limits/point/length/null inputs stop before transport; local duplicate/invalid encoding never replaces a live channel");
	return 0;
}
