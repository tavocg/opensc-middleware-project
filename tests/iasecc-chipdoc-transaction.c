/* Real sc_lock/sc_reset/sc_unlock and dispatcher; software reader only. */
#define IASECC_CHIPDOC_REAL_LOCKS
#define IASECC_CHIPDOC_BOOTSTRAP_FIXTURE
#include "iasecc-chipdoc-bootstrap.c"
static int reader_reserved, lock_fail, unlock_fail, reset_fail_at;
static unsigned int reader_locks, reader_unlocks, resets;
static int query_mode, query_bit;
static unsigned int query_sw, query_outer;
static int mutex_unlock_fail;
static int mutex_lock(void *opaque) { (void)opaque; return 0; }
static int mutex_unlock(void *opaque) { (void)opaque; return mutex_unlock_fail ? -7 : 0; }
static sc_thread_context_t thread_ops = {.lock_mutex=mutex_lock,.unlock_mutex=mutex_unlock};
static int reader_lock(struct sc_reader *reader)
{
	assert(reader == &test_reader && !reader_reserved);
	reader_locks++; if (lock_fail) return -7;
	reader_reserved = 1; return 0;
}
static int reader_unlock(struct sc_reader *reader)
{
	assert(reader == &test_reader && reader_reserved);
	reader_unlocks++; reader_reserved = 0; return unlock_fail ? -7 : 0;
}
static int reader_reset(struct sc_reader *reader, int cold)
{
	struct iasecc_private_data *prv = test_card.drv_data;
	assert(reader == &test_reader && reader_reserved && !cold && test_card.lock_count > 0);
	assert(prv && !prv->chipdoc_session.channel && !prv->key_size);
	assert(!prv->cache.valid && !prv->security_env.algorithm_flags);
	resets++; return resets == (unsigned)reset_fail_at ? -7 : 0;
}
static int transaction_backend(struct sc_reader *reader, struct sc_apdu *a)
{
	assert(reader_reserved);
	if (physical < 7) return bootstrap_backend(reader,a);
	unsigned char *wire; size_t size; unsigned char auth[64] = {0}, mac[16];
	assert(reader == &test_reader && test_card.lock_count >= 2);
	assert(!sc_apdu_get_octets(&test_ctx,a,&wire,&size,SC_PROTO_T1));
	const unsigned long flags = SC_APDU_FLAGS_NO_SM | SC_APDU_FLAGS_NO_RETRY_WL | SC_APDU_FLAGS_NO_GET_RESP;
	assert((a->flags & flags) == flags && !a->control && !(a->flags & SC_APDU_FLAGS_CHAINING));
	assert(a->cse == SC_APDU_CASE_4_SHORT && a->cla == 0x0C && a->ins == 0x20 && !a->p1 && a->p2 == 1);
	assert(a->lc == 10 && a->le == 256 && a->resplen == 256 && size == 16);
	assert(!memcmp(wire,"\x0c\x20\0\x01\x0a\x8e\x08",7) && wire[15] == 0);
	auth[15] = 5; memcpy(auth+16,"\x0c\x20\0\x01\x80",5);
	cmac(peer.derived.keys.mac,auth,32,mac); assert(!memcmp(mac,a->data+2,8)); free(wire); physical++;
	if (query_mode == 1) return -7;
	if (query_mode == 2) { a->resplen = 0; a->sw1 = 0x90; a->sw2 = 0; return 0; }
	if (query_mode == 3) { a->resplen = 0; a->sw1 = 0x61; a->sw2 = 0; return 0; }
	if (query_mode == 4) { a->resplen = 0; a->sw1 = 0x6C; a->sw2 = 0; return 0; }
	if (query_mode == 5) { a->resplen = 257; return 0; }
	size_t offset = 0;
	if (query_mode == 8) {
		unsigned char iv[16], padded[16] = {0xAA,0x80}; int n, last;
		response_iv(6,iv); EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new(); assert(ctx);
		assert(EVP_EncryptInit_ex(ctx,EVP_aes_128_cbc(),NULL,peer.derived.keys.encryption,iv));
		assert(EVP_CIPHER_CTX_set_padding(ctx,0) && EVP_EncryptUpdate(ctx,a->resp+3,&n,padded,16));
		assert(EVP_EncryptFinal_ex(ctx,a->resp+3+n,&last) && n+last == 16); EVP_CIPHER_CTX_free(ctx);
		memcpy(a->resp,"\x87\x11\x01",3); offset = 19;
	}
	a->resp[offset++] = 0x99; a->resp[offset++] = 2;
	a->resp[offset++] = query_sw >> 8; a->resp[offset++] = query_sw & 255;
	memset(auth,0,sizeof(auth)); auth[15] = query_mode == 7 ? 4 : 6;
	memcpy(auth+16,a->resp,offset); auth[16+offset] = 0x80;
	cmac(peer.derived.keys.mac,auth,query_mode == 8 ? 48 : 32,mac);
	if (query_mode == 6) mac[query_bit/8] ^= 1 << (query_bit%8);
	memcpy(a->resp+offset,"\x8e\x08",2); offset += 2; memcpy(a->resp+offset,mac,8); offset += 8;
	a->resplen = offset; a->sw1 = query_outer >> 8; a->sw2 = query_outer & 255;
	change_provider_after_io(); return 0;
}
static void transaction_defaults(void)
{
	assert(!reader_reserved); bootstrap_defaults(); test_card.lock_count = 0;
	reader_ops.lock = reader_lock; reader_ops.unlock = reader_unlock; reader_ops.reset = reader_reset;
	reader_ops.transmit = transaction_backend;
	reader_locks = reader_unlocks = resets = 0; lock_fail = unlock_fail = reset_fail_at = 0;
	query_mode = query_bit = 0; query_sw = 0x63C7; query_outer = 0x9000;
}
static void begin(void)
{
	struct iasecc_chipdoc_transaction_report report;
	assert(!iasecc_chipdoc_transaction_begin(&p15,&key_object,&report));
	assert(report.stage == IASECC_CHIPDOC_TX_READY && report.commands == 7 && report.initial_reset_attempted);
	assert(report.lock_held && report.reset_required && !report.complete && !report.close_attempted);
	assert(reader_reserved && resets == 1 && reader_locks == 1 && !reader_unlocks);
	assert(test_prv.chipdoc_session.phase == IASECC_CHIPDOC_MF);
}
static void close_transaction(void)
{
	struct iasecc_chipdoc_transaction_report report;
	assert(!iasecc_chipdoc_transaction_close(&test_card,&report));
	assert(report.close_attempted && report.final_reset_attempted && report.unlock_attempted && report.complete && !report.reset_required);
	assert(!test_prv.chipdoc_session.channel && !test_prv.chipdoc_transaction.active && !test_card.lock_count && !reader_reserved);
	assert(resets == 2 && reader_unlocks == 1);
}
int main(void)
{
	test_provider.libctx = OSSL_LIB_CTX_new(); assert(test_provider.libctx);
	test_provider.defprov = OSSL_PROVIDER_load(test_provider.libctx,"default"); assert(test_provider.defprov);
	alternate_provider.libctx = OSSL_LIB_CTX_new(); assert(alternate_provider.libctx);
	alternate_provider.defprov = OSSL_PROVIDER_load(alternate_provider.libctx,"default"); assert(alternate_provider.defprov);
	init(&peer); memcpy(initial_saved,peer.initial,sizeof(initial_saved));
	struct iasecc_chipdoc_transaction_report transaction, closed;
	struct iasecc_chipdoc_channel_report report; int tries;
	for (int nested = 0; nested < 2; nested++) {
		transaction_defaults(); if (nested) assert(!sc_lock(&test_card));
		begin(); assert(test_card.lock_count == nested+1);
		assert(!iasecc_chipdoc_channel_query_pin(&test_card,1,&tries,&report));
		assert(tries == 7 && report.authenticated && report.sw == 0x63C7 && report.commands == 1 && physical == 8);
		struct chipdoc_crypto *crypto = test_prv.chipdoc_session.channel;
		assert(crypto->sm.keys.ssc[15] == 6 && test_prv.chipdoc_session.phase == IASECC_CHIPDOC_PIN_AVAILABLE);
		assert(!iasecc_chipdoc_transaction_close(&test_card,&closed) && closed.complete && resets == 2);
		assert(test_card.lock_count == nested && reader_reserved == nested && reader_unlocks == (nested ? 0u : 1u));
		assert(!iasecc_chipdoc_transaction_close(&test_card,&closed) && closed.complete && resets == 2);
		if (nested) assert(!sc_unlock(&test_card));
	}
	puts("PASS real recursive sc_lock/reset/unlock reserves one extra lock through native bootstrap/MF/empty VERIFY, SSC6/auth63C7; close wipes crypto before reset and restores caller depth exactly once");
	for (int variant = 0; variant < 10; variant++) {
		transaction_defaults();
		if (variant == 0) key_info.modulus_length = 3072;
		if (variant == 1) reader_ops.lock = NULL;
		if (variant == 2) reader_ops.unlock = NULL;
		if (variant == 3) reader_ops.reset = NULL;
		if (variant == 4) card_ops.card_reader_lock_obtained = NULL;
		if (variant == 5) test_ctx.debug = 1;
		if (variant == 6) test_prv.chipdoc_recovery_required = 1;
		if (variant == 7) test_card.lock_count = INT_MAX;
		if (variant == 8) key_object.next = NULL;
		if (variant == 9) test_card.driver = NULL;
		assert(iasecc_chipdoc_transaction_begin(&p15,&key_object,&transaction) != 0);
		assert(!reader_locks && !resets && !physical && !test_prv.chipdoc_transaction.active);
	}
	transaction_defaults(); lock_fail = 1;
	assert(iasecc_chipdoc_transaction_begin(&p15,&key_object,&transaction) == -7);
	assert(reader_locks == 1 && !resets && !reader_unlocks && !reader_reserved && !test_card.lock_count);
	transaction_defaults(); reset_fail_at = 1;
	assert(iasecc_chipdoc_transaction_begin(&p15,&key_object,&transaction) == -7);
	assert(resets == 1 && !physical && reader_unlocks == 1 && transaction.reset_required && !transaction.complete && !transaction.final_reset_attempted);
	assert(iasecc_chipdoc_transaction_begin(&p15,&key_object,&transaction) != 0 && reader_locks == 1);
	transaction_defaults(); test_ctx.thread_ctx = &thread_ops; test_card.mutex = &mutex_unlock_fail; mutex_unlock_fail = 1;
	assert(iasecc_chipdoc_transaction_begin(&p15,&key_object,&transaction) == -7);
	assert(test_card.lock_count == 1 && reader_reserved && transaction.reset_required && transaction.lock_held && !resets && !physical);
	assert(iasecc_chipdoc_transaction_close(&test_card,&closed) != 0 && !reader_unlocks && !resets);
	mutex_unlock_fail = 0; assert(!sc_unlock(&test_card)); test_ctx.thread_ctx = NULL; test_card.mutex = NULL;
	puts("PASS unsupported native metadata/reader callbacks/debug/recovery/overflow stop before acquisition; initial reset failure releases our lock without reset retry and quarantines instance");
	for (int step = 1; step <= 3; step++) {
		transaction_defaults(); failure_step = step; failure = 1;
		assert(iasecc_chipdoc_transaction_begin(&p15,&key_object,&transaction) != 0);
		assert(physical == (unsigned)step && transaction.complete && resets == 2 && !reader_reserved && !test_card.lock_count);
	}
	for (int step = 1; step <= 3; step++) {
		transaction_defaults(); peer.mode = TRANSPORT_ERROR; peer.fail_step = step;
		assert(iasecc_chipdoc_transaction_begin(&p15,&key_object,&transaction) != 0);
		assert(physical == (unsigned)(step+3) && transaction.complete && resets == 2 && !reader_reserved);
	}
	transaction_defaults(); mf_error = 2;
	assert(iasecc_chipdoc_transaction_begin(&p15,&key_object,&transaction) != 0 && physical == 7 && transaction.complete && !reader_reserved);
	puts("PASS startup failure in every plain/CA/MF stage destroys state, resets under the original reservation and releases it, with no PIN/GET/fallback or automatic second startup");
	for (int count = 0; count < 16; count++) for (int warning = 0; warning < 2; warning++) {
		transaction_defaults(); begin(); query_sw = 0x63C0+count; query_outer = warning ? query_sw : 0x9000;
		int rv = iasecc_chipdoc_channel_query_pin(&test_card,1,&tries,&report);
		assert((rv == 0) == (count == 7) && tries == count && report.authenticated && report.sw == query_sw);
		if (!count) assert(rv == SC_ERROR_AUTH_METHOD_BLOCKED);
		if (count != 7) assert(!test_prv.chipdoc_session.channel);
		close_transaction();
	}
	for (int mode = 1; mode <= 8; mode++) {
		int bits = mode == 6 ? 64 : 1;
		for (int bit = 0; bit < bits; bit++) {
			transaction_defaults(); begin(); query_mode = mode; query_bit = bit;
			assert(iasecc_chipdoc_channel_query_pin(&test_card,1,&tries,&report) != 0 && tries == -1 && physical == 8);
			assert(!test_prv.chipdoc_session.channel);
			if (mode != 8) assert(!report.authenticated && !report.sw);
			close_transaction();
		}
	}
	transaction_defaults(); begin(); query_outer = 0x63C6;
	assert(iasecc_chipdoc_channel_query_pin(&test_card,1,&tries,&report) != 0 && !report.authenticated && !report.sw && tries == -1);
	close_transaction();
	transaction_defaults(); begin(); query_sw = 0x9000;
	assert(iasecc_chipdoc_channel_query_pin(&test_card,1,&tries,&report) != 0 && report.authenticated && report.sw == 0x9000 && tries == -1);
	close_transaction();
	puts("PASS EMPTY VERIFY accepts only MAC-verified status; all64 altered MAC bits/replay/bare9000/61/6C/transport/size/payload/outer mismatch fail; authenticated0..15 are reported, only observed7 advances, no max-tries/login inference");
	transaction_defaults(); begin(); void *owner = test_prv.chipdoc_session.channel;
	assert(iasecc_chipdoc_transaction_begin(&p15,&key_object,&transaction) != 0 && physical == 7 && owner == test_prv.chipdoc_session.channel);
	assert(iasecc_chipdoc_channel_query_pin(&test_card,2,&tries,&report) != 0 && physical == 7 && owner == test_prv.chipdoc_session.channel);
	assert(iasecc_chipdoc_channel_query_pin(&test_card,1,NULL,&report) != 0 && physical == 7);
	assert(!iasecc_chipdoc_channel_query_pin(&test_card,1,&tries,&report));
	assert(iasecc_chipdoc_channel_query_pin(&test_card,1,&tries,&report) != 0 && physical == 8 && owner == test_prv.chipdoc_session.channel);
	close_transaction();
	transaction_defaults(); begin(); swap_provider_on_command = 8;
	assert(iasecc_chipdoc_channel_query_pin(&test_card,1,&tries,&report) != 0 && tries == -1 && !test_prv.chipdoc_session.channel);
	close_transaction();
	puts("PASS duplicate/invalid local calls preserve an owner without I/O; repeated query cannot replay; provider replacement during response drops crypto before publishing status");
	for (int reacquire = 0; reacquire < 2; reacquire++) {
		transaction_defaults(); begin(); assert(!sc_unlock(&test_card));
		if (reacquire) assert(!sc_lock(&test_card));
		assert(iasecc_chipdoc_channel_query_pin(&test_card,1,&tries,&report) != 0 && physical == 7 && !test_prv.chipdoc_session.channel);
		assert(iasecc_chipdoc_transaction_close(&test_card,&closed) != 0 && closed.reset_required && !closed.complete);
		assert(!closed.final_reset_attempted && !closed.unlock_attempted && resets == 1 && reader_unlocks == 1);
		assert(test_card.lock_count == reacquire && reader_reserved == reacquire && test_prv.chipdoc_recovery_required);
		assert(iasecc_chipdoc_transaction_begin(&p15,&key_object,&transaction) != 0 && physical == 7);
		if (reacquire) assert(!sc_unlock(&test_card)); /* foreign caller's lock */
	}
	for (int cleanup = 0; cleanup < 2; cleanup++) {
		transaction_defaults(); begin(); if (cleanup) unlock_fail = 1; else reset_fail_at = 2;
		assert(iasecc_chipdoc_transaction_close(&test_card,&closed) == -7 && !closed.complete && closed.reset_required);
		assert(!reader_reserved && !test_card.lock_count && resets == 2 && reader_unlocks == 1 && !test_prv.chipdoc_session.channel);
		assert(iasecc_chipdoc_transaction_close(&test_card,&closed) != 0 && resets == 2 && reader_unlocks == 1);
		assert(iasecc_chipdoc_transaction_begin(&p15,&key_object,&transaction) != 0 && reader_locks == 1);
	}
	puts("PASS loss and actual physical reacquisition invalidate reservation even without reset; close never resets/unlocks the foreign lock, cleanup failures are visible and never retried on this instance");
	transaction_defaults(); begin();
	assert(!iasecc_logout(&test_card) && !reader_reserved && resets == 2 && !test_prv.chipdoc_transaction.active);
	struct sc_ef_atr ef = {0}; ef.aid = app.aid; test_card.ef_atr = &ef;
	assert(!iasecc_logout(&test_card) && physical == 7 && resets == 2); test_card.ef_atr = NULL;
	for (int failure = 0; failure < 2; failure++) {
		transaction_defaults(); begin(); if (failure) reset_fail_at = 2;
		struct iasecc_private_data *heap = malloc(sizeof(*heap)); assert(heap); *heap = test_prv;
		memset(&test_prv,0,sizeof(test_prv)); test_card.drv_data = heap;
		assert(iasecc_finish(&test_card) == (failure ? -7 : 0));
		assert(!test_card.drv_data && !test_card.lock_count && !reader_reserved && reader_unlocks == 1 && resets == 2);
	}
	puts("PASS real IAS logout/finish close live reservation without plaintext root selection, release/free state and propagate cleanup failure; legacy inactive lifecycle remains covered separately");
	EVP_PKEY_free(peer.peer); jcop4_ca_pending_clear(&peer.derived);
	OSSL_PROVIDER_unload(test_provider.defprov); OSSL_LIB_CTX_free(test_provider.libctx);
	OSSL_PROVIDER_unload(alternate_provider.defprov); OSSL_LIB_CTX_free(alternate_provider.libctx);
	return 0;
}
