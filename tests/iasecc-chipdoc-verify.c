/* Canonical OpenSC software-PIN arguments, synthetic bytes, no hardware. */
#define IASECC_CHIPDOC_TRANSACTION_FIXTURE
#include "iasecc-chipdoc-transaction.c"
static unsigned char fictitious_pin[14];
static size_t expected_pin_size;
static int verify_mode, verify_bit;
static unsigned int verify_sw, verify_outer, credential_attempts;

static int verify_backend(struct sc_reader *reader, struct sc_apdu *a)
{
	if (physical < 8) return transaction_backend(reader,a);
	unsigned char *wire = NULL; size_t size; unsigned char auth[64] = {0}, mac[16], iv[16], padded[16];
	assert(reader == &test_reader && reader_reserved && test_card.lock_count >= 2 && !credential_attempts);
	assert(!sc_apdu_get_octets(&test_ctx,a,&wire,&size,SC_PROTO_T1));
	const unsigned long flags = SC_APDU_FLAGS_NO_SM | SC_APDU_FLAGS_NO_RETRY_WL | SC_APDU_FLAGS_NO_GET_RESP;
	assert((a->flags & flags) == flags && !a->control && !(a->flags & (SC_APDU_FLAGS_CHAINING|SC_APDU_FLAGS_SM_CHAINING)));
	assert(size == 35 && a->cse == SC_APDU_CASE_4_SHORT && a->le == 256 && a->resplen == 256);
	assert(!memcmp(wire,"\x0c\x20\0\x01\x1d\x87\x11\x01",8) && !wire[34]);
	assert(!memcmp(a->data+19,"\x8e\x08",2));
	/* Independent IV ECB, data CBC and CMAC; no production unseal helpers. */
	response_iv(7,iv); EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new(); assert(ctx); int n, last;
	assert(EVP_DecryptInit_ex(ctx,EVP_aes_128_cbc(),NULL,peer.derived.keys.encryption,iv));
	assert(EVP_CIPHER_CTX_set_padding(ctx,0) && EVP_DecryptUpdate(ctx,padded,&n,a->data+3,16));
	assert(EVP_DecryptFinal_ex(ctx,padded+n,&last) && n+last == 16);
	assert(expected_pin_size >= 6 && expected_pin_size <= 14 && !memcmp(padded,fictitious_pin,expected_pin_size));
	assert(padded[expected_pin_size] == 0x80);
	for (size_t i = expected_pin_size+1; i < 16; i++) assert(!padded[i]);
	auth[15] = 7; memcpy(auth+16,"\x0c\x20\0\x01\x80",5); memcpy(auth+32,a->data,19); auth[51] = 0x80;
	cmac(peer.derived.keys.mac,auth,64,mac); assert(!memcmp(mac,a->data+21,8));
	physical++; credential_attempts++; free(wire);
	if (verify_mode == 1) { EVP_CIPHER_CTX_free(ctx); return -7; }
	if (verify_mode == 2) { a->resplen = 0; a->sw1 = 0x90; a->sw2 = 0; EVP_CIPHER_CTX_free(ctx); return 0; }
	if (verify_mode == 3 || verify_mode == 4) { a->resplen = 0; a->sw1 = verify_mode == 3 ? 0x61 : 0x6C; a->sw2 = 0; EVP_CIPHER_CTX_free(ctx); return 0; }
	if (verify_mode == 5) { a->resplen = 257; EVP_CIPHER_CTX_free(ctx); return 0; }
	if (verify_mode >= 9 && verify_mode <= 11) {
		const int errors[] = {SC_ERROR_CARD_RESET,SC_ERROR_READER_REATTACHED,SC_ERROR_CARD_REMOVED};
		EVP_CIPHER_CTX_free(ctx); return errors[verify_mode-9];
	}
	size_t offset = 0;
	if (verify_mode == 8) {
		memset(padded,0,16); padded[0] = 0xAA; padded[1] = 0x80; response_iv(8,iv);
		assert(EVP_EncryptInit_ex(ctx,EVP_aes_128_cbc(),NULL,peer.derived.keys.encryption,iv));
		assert(EVP_CIPHER_CTX_set_padding(ctx,0) && EVP_EncryptUpdate(ctx,a->resp+3,&n,padded,16));
		assert(EVP_EncryptFinal_ex(ctx,a->resp+3+n,&last) && n+last == 16);
		memcpy(a->resp,"\x87\x11\x01",3); offset = 19;
	}
	EVP_CIPHER_CTX_free(ctx);
	a->resp[offset++] = 0x99; a->resp[offset++] = 2; a->resp[offset++] = verify_sw >> 8; a->resp[offset++] = verify_sw & 255;
	memset(auth,0,64); auth[15] = verify_mode == 7 ? 6 : 8;
	memcpy(auth+16,a->resp,offset); auth[16+offset] = 0x80;
	cmac(peer.derived.keys.mac,auth,verify_mode == 8 ? 48 : 32,mac);
	if (verify_mode == 6) mac[verify_bit/8] ^= 1 << (verify_bit%8);
	memcpy(a->resp+offset,"\x8e\x08",2); offset += 2; memcpy(a->resp+offset,mac,8); offset += 8;
	a->resplen = offset; a->sw1 = verify_outer >> 8; a->sw2 = verify_outer & 255;
	OPENSSL_cleanse(padded,16); OPENSSL_cleanse(auth,64); OPENSSL_cleanse(mac,16); change_provider_after_io();
	return 0;
}
static void verify_defaults(void)
{
	transaction_defaults(); reader_ops.transmit = verify_backend;
	for (size_t i = 0; i < sizeof(fictitious_pin); i++) fictitious_pin[i] = 0x20+i;
	expected_pin_size = 6; verify_mode = verify_bit = 0; verify_sw = verify_outer = 0x9000; credential_attempts = 0;
}
static void available(void)
{
	begin(); int tries; struct iasecc_chipdoc_channel_report report;
	assert(!iasecc_chipdoc_channel_query_pin(&test_card,1,&tries,&report) && tries == 7 && physical == 8);
}
static struct sc_pin_cmd_data arguments(void)
{
	struct sc_pin_cmd_data d = {0}; d.cmd = SC_PIN_CMD_VERIFY; d.pin_type = SC_AC_CHV; d.pin_reference = 1;
	d.pin1.data = fictitious_pin; d.pin1.len = expected_pin_size; d.pin1.min_length = 6; d.pin1.max_length = 14;
	d.pin1.pad_length = 14; /* PKCS15 supplies stored_length even when padding flag is clear. */
	d.pin1.max_tries = d.pin1.tries_left = 99; d.pin1.logged_in = SC_PIN_STATE_LOGGED_IN;
	d.pin1.prompt = (const char *)1; /* Driver must not prompt or read this. */
	return d;
}
#ifndef IASECC_CHIPDOC_VERIFY_FIXTURE
int main(void)
{
	(void)thread_ops; /* Mutex-error variants remain in the shared transaction suite. */
	test_provider.libctx = OSSL_LIB_CTX_new(); assert(test_provider.libctx);
	test_provider.defprov = OSSL_PROVIDER_load(test_provider.libctx,"default"); assert(test_provider.defprov);
	alternate_provider.libctx = OSSL_LIB_CTX_new(); assert(alternate_provider.libctx);
	alternate_provider.defprov = OSSL_PROVIDER_load(alternate_provider.libctx,"default"); assert(alternate_provider.defprov);
	init(&peer); memcpy(initial_saved,peer.initial,sizeof(initial_saved));
	struct iasecc_chipdoc_channel_report report; struct iasecc_chipdoc_transaction_report transaction; int tries;
	for (size_t length = 6; length <= 14; length++) {
		verify_defaults(); available(); expected_pin_size = length;
		struct sc_pin_cmd_data d = arguments(), saved = d; unsigned char saved_pin[14]; memcpy(saved_pin,fictitious_pin,14);
		assert(!iasecc_chipdoc_channel_verify_pin(&test_card,&d,&tries,&report) && tries == -1);
		assert(report.authenticated && report.sw == 0x9000 && report.commands == 1 && report.reset_required && credential_attempts == 1);
		assert(!memcmp(&d,&saved,sizeof(d)) && !memcmp(saved_pin,fictitious_pin,14));
		struct chipdoc_crypto *crypto = test_prv.chipdoc_session.channel;
		assert(crypto && crypto->sm.keys.ssc[15] == 8 && test_prv.chipdoc_session.phase == IASECC_CHIPDOC_LOGGED_IN);
		assert(iasecc_chipdoc_channel_verify_pin(&test_card,&d,&tries,&report) != 0 && physical == 9 && credential_attempts == 1);
		assert(test_prv.chipdoc_session.phase == IASECC_CHIPDOC_LOGGED_IN); close_transaction();
	}
	puts("PASS typed OpenSC PIN6..14 is encrypted unmodified once in measured35-byte VERIFY, independent AES/CMAC/SSC7/8, no cache/prompt/input mutation; only authenticated empty9000 logs in");
	for (int variant = 0; variant < 5; variant++) {
		verify_defaults();
		if (variant == 0) pin_info.attrs.pin.min_length = 5;
		if (variant == 1) pin_info.attrs.pin.max_length = 15;
		if (variant == 2) pin_info.attrs.pin.type = SC_PKCS15_PIN_TYPE_BCD;
		if (variant == 3) pin_info.attrs.pin.flags |= SC_PKCS15_PIN_FLAG_NEEDS_PADDING;
		if (variant == 4) pin_info.attrs.pin.flags &= ~SC_PKCS15_PIN_FLAG_CASE_SENSITIVE;
		assert(iasecc_chipdoc_transaction_begin(&p15,&key_object,&transaction) != 0 && !reader_locks && !physical && !resets);
	}
	for (int variant = 0; variant < 22; variant++) {
		verify_defaults(); available(); struct sc_pin_cmd_data d = arguments(); void *owner = test_prv.chipdoc_session.channel;
		switch (variant) {
		case 0: d.cmd = SC_PIN_CMD_CHANGE; break;
		case 1: d.cmd = SC_PIN_CMD_UNBLOCK; break;
		case 2: d.cmd = SC_PIN_CMD_GET_INFO; break;
		case 3: d.flags = SC_PIN_CMD_USE_PINPAD; break;
		case 4: d.flags = SC_PIN_CMD_NEED_PADDING; break;
		case 5: d.flags = IASECC_PIN_STATUS_ONLY; break;
		case 6: d.pin_reference = 2; break;
		case 7: d.pin_type = SC_AC_AUT; break; /* Never reinterpret key authentication8. */
		case 8: d.puk_reference = 1; break;
		case 9: d.pin2.data = fictitious_pin; break;
		case 10: d.pin2.len = 1; break;
		case 11: d.apdu = (struct sc_apdu *)1; break;
		case 12: d.pin1.encoding = SC_PIN_ENCODING_BCD; break;
		case 13: d.pin1.min_length = 5; break;
		case 14: d.pin1.max_length = 15; break;
		case 15: d.pin1.data = NULL; break;
		case 16: d.pin1.len = 0; break;
		case 17: d.pin1.len = 5; break;
		case 18: d.pin1.len = 15; break;
		case 19: d.pin1.len = SIZE_MAX; d.pin1.data = (unsigned char *)1; break;
		case 20: fictitious_pin[0] = 0; break;
		case 21: fictitious_pin[0] = 0x80; break;
		}
		assert(iasecc_chipdoc_channel_verify_pin(&test_card,&d,&tries,&report) != 0 && !credential_attempts && physical == 8 && tries == -1);
		assert(owner == test_prv.chipdoc_session.channel && test_prv.chipdoc_session.phase == IASECC_CHIPDOC_PIN_AVAILABLE);
		close_transaction();
	}
	puts("PASS measured PIN policy is validated before lock; unsupported operations/PIN-pad/padding/AUT8/ref/encoding/multiple PIN/custom APDU/oversized or nonprintable input fail locally without attempts or losing owner");
	for (int count = 0; count < 16; count++) for (int warning = 0; warning < 2; warning++) {
		verify_defaults(); available(); struct sc_pin_cmd_data d = arguments();
		verify_sw = 0x63C0+count; verify_outer = warning ? verify_sw : 0x9000;
		int rv = iasecc_chipdoc_channel_verify_pin(&test_card,&d,&tries,&report);
		assert(rv == (count ? SC_ERROR_PIN_CODE_INCORRECT : SC_ERROR_AUTH_METHOD_BLOCKED));
		assert(tries == count && report.authenticated && report.sw == verify_sw && credential_attempts == 1 && !test_prv.chipdoc_session.channel);
		assert(iasecc_chipdoc_channel_verify_pin(&test_card,&d,&tries,&report) != 0 && physical == 9 && credential_attempts == 1);
		close_transaction();
	}
	const unsigned int statuses[] = {0x6982,0x6983,0x6A82};
	const int errors[] = {SC_ERROR_SECURITY_STATUS_NOT_SATISFIED,SC_ERROR_AUTH_METHOD_BLOCKED,SC_ERROR_CARD_CMD_FAILED};
	for (int i = 0; i < 3; i++) {
		verify_defaults(); available(); struct sc_pin_cmd_data d = arguments(); verify_sw = statuses[i];
		assert(iasecc_chipdoc_channel_verify_pin(&test_card,&d,&tries,&report) == errors[i]);
		assert(report.authenticated && report.sw == verify_sw && tries == -1 && !test_prv.chipdoc_session.channel); close_transaction();
	}
	puts("PASS MAC-authenticated incorrect/blocked/security statuses remain errors, counts0..15 reported only when encoded, no max/restart inference or retransmission after failure");
	for (int mode = 1; mode <= 8; mode++) for (int bit = 0; bit < (mode == 6 ? 64 : 1); bit++) {
		verify_defaults(); available(); struct sc_pin_cmd_data d = arguments(); verify_mode = mode; verify_bit = bit;
		assert(iasecc_chipdoc_channel_verify_pin(&test_card,&d,&tries,&report) != 0 && tries == -1 && credential_attempts == 1);
		assert(!test_prv.chipdoc_session.channel && physical == 9);
		if (mode != 8) assert(!report.authenticated && !report.sw);
		assert(iasecc_chipdoc_channel_verify_pin(&test_card,&d,&tries,&report) != 0 && credential_attempts == 1 && physical == 9);
		close_transaction();
	}
	verify_defaults(); available(); struct sc_pin_cmd_data d = arguments(); verify_sw = 0x63C6; verify_outer = 0x63C5;
	assert(iasecc_chipdoc_channel_verify_pin(&test_card,&d,&tries,&report) != 0 && !report.authenticated && !report.sw && tries == -1); close_transaction();
	for (int mode = 9; mode <= 11; mode++) {
		verify_defaults(); available(); d = arguments(); verify_mode = mode;
		assert(iasecc_chipdoc_channel_verify_pin(&test_card,&d,&tries,&report) != 0 && tries == -1 && credential_attempts == 1);
		assert(!report.authenticated && !report.sw && !test_prv.chipdoc_session.channel && physical == 9);
		assert(iasecc_chipdoc_channel_verify_pin(&test_card,&d,&tries,&report) != 0 && physical == 9);
		if (mode == 11) close_transaction();
		else {
			assert(iasecc_chipdoc_transaction_close(&test_card,&transaction) == SC_ERROR_CARD_RESET);
			assert(!transaction.final_reset_attempted && !transaction.unlock_attempted && transaction.reset_required);
			assert(resets == 1 && !reader_unlocks && test_prv.chipdoc_recovery_required);
			assert(!sc_unlock(&test_card)); /* Harness releases its fake reader after simulating teardown. */
		}
	}
	puts("PASS transport/61/6C/oversize/bare9000/all64 MAC bits/replay/payload/outer mismatch invalidate channel without publishing unverified PIN status or retrying credential");
	verify_defaults(); begin(); d = arguments(); d.pin1.data = (unsigned char *)1;
	assert(iasecc_chipdoc_channel_verify_pin(&test_card,&d,&tries,&report) != 0 && physical == 7 && !credential_attempts); close_transaction();
	verify_defaults(); available(); d = arguments();
	struct chipdoc_crypto *crypto = test_prv.chipdoc_session.channel; crypto->sm.keys.ssc[15] = 4;
	d.pin1.data = (unsigned char *)1;
	assert(iasecc_chipdoc_channel_verify_pin(&test_card,&d,&tries,&report) != 0 && physical == 8 && !credential_attempts && !test_prv.chipdoc_session.channel); close_transaction();
	verify_defaults(); available(); d = arguments(); swap_provider_on_command = 9;
	assert(iasecc_chipdoc_channel_verify_pin(&test_card,&d,&tries,&report) != 0 && !report.authenticated && !report.sw && credential_attempts == 1); close_transaction();
	verify_defaults(); available(); d = arguments();
	assert(EVP_set_default_properties(test_provider.libctx,"provider=unavailable"));
	assert(iasecc_chipdoc_channel_verify_pin(&test_card,&d,&tries,&report) != 0 && !credential_attempts && physical == 8 && !test_prv.chipdoc_session.channel);
	assert(EVP_set_default_properties(test_provider.libctx,NULL)); close_transaction();
	puts("PASS missing query/stale SSC stop before reading credential; provider changes/failures drop channel with zero or one attempt, no global fallback");
	verify_defaults(); available(); d = arguments();
	assert(iasecc_chipdoc_channel_verify_pin(&test_card,NULL,&tries,&report) != 0 && physical == 8);
	assert(iasecc_chipdoc_channel_verify_pin(&test_card,&d,NULL,&report) != 0 && physical == 8);
	assert(iasecc_chipdoc_channel_verify_pin(&test_card,&d,&tries,NULL) != 0 && physical == 8);
	d.pin1.min_length = d.pin1.max_length = 0; /* Optional caller bounds cannot override stored policy. */
	assert(!iasecc_chipdoc_channel_verify_pin(&test_card,&d,&tries,&report));
	reset_fail_at = 2;
	assert(iasecc_chipdoc_transaction_close(&test_card,&transaction) != 0 && transaction.reset_required && !transaction.complete);
	assert(!test_prv.chipdoc_session.channel && !reader_reserved && credential_attempts == 1);
	assert(iasecc_chipdoc_transaction_begin(&p15,&key_object,&transaction) != 0 && credential_attempts == 1);
	puts("PASS null/local bounds handling cannot send an attempt; failed cleanup after successful VERIFY remains visible and blocks fresh startup without resetting/inferencing PIN counter");
	EVP_PKEY_free(peer.peer); jcop4_ca_pending_clear(&peer.derived); OPENSSL_cleanse(fictitious_pin,14);
	OSSL_PROVIDER_unload(test_provider.defprov); OSSL_LIB_CTX_free(test_provider.libctx);
	OSSL_PROVIDER_unload(alternate_provider.defprov); OSSL_LIB_CTX_free(alternate_provider.libctx);
	return 0;
}
#endif
