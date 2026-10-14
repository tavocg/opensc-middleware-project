/* Typed OpenSC environment and digest, real dispatcher, software reader/RSA. */
#define IASECC_CHIPDOC_VERIFY_FIXTURE
#include "iasecc-chipdoc-verify.c"
static EVP_PKEY *rsa_key;
static unsigned char digest[32], expected_signature[256], untouched[256];
static unsigned char *watched_output;
static int failure_stage, failure_mode, failure_bit;
static unsigned int signing_calls;

static int signing_reset(struct sc_reader *reader, int cold)
{
	if (watched_output) assert(!memcmp(watched_output,untouched,256));
	return reader_reset(reader,cold);
}
static int signing_unlock(struct sc_reader *reader)
{
	if (watched_output) assert(!memcmp(watched_output,untouched,256));
	return reader_unlock(reader);
}
static int signature_backend(struct sc_reader *reader, struct sc_apdu *a)
{
	if (physical < 9) return verify_backend(reader,a);
	unsigned char *wire = NULL, iv[16], decrypted[48], auth[320] = {0}, mac[16], padded[272] = {0};
	size_t size, cipher_size, before_mac, auth_size; int n, last;
	unsigned int step = physical-8, ssc = 7+2*step;
	int mode = (int)step == failure_stage ? failure_mode : 0;
	assert(reader == &test_reader && reader_reserved && test_card.lock_count >= 2 && credential_attempts == 1 && step <= 3);
	assert(!sc_apdu_get_octets(&test_ctx,a,&wire,&size,SC_PROTO_T1));
	const unsigned long flags = SC_APDU_FLAGS_NO_SM|SC_APDU_FLAGS_NO_RETRY_WL|SC_APDU_FLAGS_NO_GET_RESP;
	assert((a->flags & flags) == flags && !a->control && !(a->flags & (SC_APDU_FLAGS_CHAINING|SC_APDU_FLAGS_SM_CHAINING)));
	assert(a->cla == 0x0C && a->lc == (step == 3 ? 65u : 29u));
	if (step == 1) assert(a->ins == 0xA4 && a->p1 == 4 && a->p2 == 0x0C);
	if (step == 2) assert(a->ins == 0x22 && a->p1 == 0x81 && a->p2 == 0xB6);
	if (step == 3) {
		assert(a->ins == 0x2A && a->p1 == 0x9E && a->p2 == 0x9A && a->cse == (SC_APDU_CASE_4_EXT));
		assert(a->le == 291 && a->resplen == 291 && size == 74);
		assert(!memcmp(wire,"\x0c\x2a\x9e\x9a\0\0\x41",7) && !memcmp(wire+72,"\x01\x23",2));
		assert(!memcmp(a->data+51,"\x97\x02\x01\0\x8e\x08",6));
	} else {
		assert(a->cse == SC_APDU_CASE_4_SHORT && a->le == 256 && a->resplen == 256 && size == 35);
		assert(!wire[34] && !memcmp(a->data+19,"\x8e\x08",2));
	}
	free(wire); physical++; signing_calls++;
	cipher_size = step == 3 ? 48 : 16; response_iv(ssc,iv);
	EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new(); assert(ctx);
	assert(EVP_DecryptInit_ex(ctx,EVP_aes_128_cbc(),NULL,peer.derived.keys.encryption,iv));
	assert(EVP_CIPHER_CTX_set_padding(ctx,0) && EVP_DecryptUpdate(ctx,decrypted,&n,a->data+3,(int)cipher_size));
	assert(EVP_DecryptFinal_ex(ctx,decrypted+n,&last) && n+last == (int)cipher_size);
	size_t input_size = step == 1 ? 15 : step == 2 ? 4 : 32;
	const unsigned char *input = step == 1 ? iasecc_chipdoc_profile(IASECC_CHIPDOC_CR_RSA2048)->aid :
		step == 2 ? (const unsigned char *)"\x91\x02\x28\x91" : digest;
	assert(!memcmp(decrypted,input,input_size) && decrypted[input_size] == 0x80);
	for (size_t i = input_size+1; i < cipher_size; i++) assert(!decrypted[i]);
	before_mac = step == 3 ? 55 : 19; auth_size = step == 3 ? 96 : 64;
	auth[15] = ssc; auth[16] = 0x0C; auth[17] = a->ins; auth[18] = a->p1; auth[19] = a->p2; auth[20] = 0x80;
	memcpy(auth+32,a->data,before_mac); auth[32+before_mac] = 0x80;
	cmac(peer.derived.keys.mac,auth,auth_size,mac); assert(!memcmp(mac,a->data+before_mac+2,8));
	if (mode == 1 || (mode >= 11 && mode <= 13)) {
		const int errors[] = {SC_ERROR_CARD_RESET,SC_ERROR_READER_REATTACHED,SC_ERROR_CARD_REMOVED};
		EVP_CIPHER_CTX_free(ctx); return mode == 1 ? -7 : errors[mode-11];
	}
	if (mode >= 5 && mode <= 8) {
		a->resplen = mode == 8 ? a->le+1 : 0;
		a->sw1 = mode == 6 ? 0x61 : mode == 7 ? 0x6C : 0x90; a->sw2 = 0;
		EVP_CIPHER_CTX_free(ctx); return 0;
	}
	size_t plain_size = step == 3 ? 256 : 0;
	if (mode == 4) plain_size = step == 3 ? 255 : 1;
	if (mode == 10 && step != 3) plain_size = 1;
	if (mode == 3 || mode == 14 || mode == 15 || mode == 16) plain_size = 0;
	if (plain_size) {
		if (step == 3) {
			EVP_PKEY_CTX *rsa = EVP_PKEY_CTX_new(rsa_key,NULL); assert(rsa); size_t len = 256;
			assert(EVP_PKEY_sign_init(rsa) > 0 && EVP_PKEY_CTX_set_rsa_padding(rsa,RSA_PKCS1_PADDING) > 0);
			assert(EVP_PKEY_CTX_set_signature_md(rsa,EVP_sha256()) > 0 && EVP_PKEY_sign(rsa,expected_signature,&len,digest,32) > 0 && len == 256);
			EVP_PKEY_CTX_free(rsa); memcpy(padded,expected_signature,plain_size);
		} else padded[0] = 0xAA;
		padded[plain_size] = mode == 10 ? 0 : 0x80; cipher_size = (plain_size/16+1)*16;
		response_iv(ssc+1,iv);
		assert(EVP_EncryptInit_ex(ctx,EVP_aes_128_cbc(),NULL,peer.derived.keys.encryption,iv));
		assert(EVP_CIPHER_CTX_set_padding(ctx,0));
	}
	size_t offset = 0;
	if (plain_size) {
		a->resp[offset++] = 0x87;
		if (cipher_size+1 > 255) { a->resp[offset++] = 0x82; a->resp[offset++] = (cipher_size+1)>>8; a->resp[offset++] = (cipher_size+1)&255; }
		else a->resp[offset++] = cipher_size+1;
		a->resp[offset++] = 1;
		assert(EVP_EncryptUpdate(ctx,a->resp+offset,&n,padded,(int)cipher_size));
		assert(EVP_EncryptFinal_ex(ctx,a->resp+offset+n,&last) && n+last == (int)cipher_size); offset += cipher_size;
	}
	EVP_CIPHER_CTX_free(ctx);
	unsigned int sw = mode == 3 ? 0x6982 : mode == 14 || mode == 16 ? 0x6A82 : 0x9000;
	a->resp[offset++] = 0x99; a->resp[offset++] = 2; a->resp[offset++] = sw>>8; a->resp[offset++] = sw&255;
	memset(auth,0,sizeof(auth)); auth[15] = mode == 9 ? ssc-1 : ssc+1;
	memcpy(auth+16,a->resp,offset); auth[16+offset] = 0x80; auth_size = ((16+offset)/16+1)*16;
	cmac(peer.derived.keys.mac,auth,auth_size,mac); if (mode == 2) mac[failure_bit/8] ^= 1 << (failure_bit%8);
	memcpy(a->resp+offset,"\x8e\x08",2); offset += 2; memcpy(a->resp+offset,mac,8); offset += 8;
	a->resplen = offset; a->sw1 = mode == 14 || mode == 16 ? 0x6A : 0x90; a->sw2 = mode == 14 ? 0x82 : mode == 16 ? 0x83 : 0;
	if (mode == 17) test_ctx.ossl3ctx = &alternate_provider;
	if (mode == 18) assert(EVP_set_default_properties(test_provider.libctx,"provider=unavailable"));
	OPENSSL_cleanse(decrypted,sizeof(decrypted)); OPENSSL_cleanse(padded,sizeof(padded)); OPENSSL_cleanse(auth,sizeof(auth));
	return 0;
}
static struct sc_security_env environment(void)
{
	struct sc_security_env env = {0}; env.flags = SC_SEC_ENV_ALG_PRESENT|SC_SEC_ENV_KEY_REF_PRESENT;
	env.operation = SC_SEC_OPERATION_SIGN; env.algorithm = SC_ALGORITHM_RSA;
	env.algorithm_flags = SC_ALGORITHM_RSA_PAD_PKCS1_TYPE_01|SC_ALGORITHM_RSA_HASH_SHA256;
	env.key_ref_len = 1; env.key_ref[0] = 0x91; env.key_size_bits = 2048; return env;
}
static void logged_in(void)
{
	verify_defaults(); reader_ops.transmit = signature_backend; reader_ops.reset = signing_reset; reader_ops.unlock = signing_unlock;
	failure_stage = failure_mode = failure_bit = 0; signing_calls = 0; watched_output = NULL;
	available(); struct sc_pin_cmd_data d = arguments(); int tries; struct iasecc_chipdoc_channel_report report;
	assert(!iasecc_chipdoc_channel_verify_pin(&test_card,&d,&tries,&report) && physical == 9);
}
static void prepared(void)
{
	struct sc_security_env env = environment(); struct iasecc_chipdoc_channel_report report;
	assert(!iasecc_chipdoc_channel_set_security_env(&test_card,&env,0,&report) && report.commands == 2 && report.authenticated);
	assert(physical == 11 && test_prv.chipdoc_session.phase == IASECC_CHIPDOC_MSE);
}
static void release_failed(void)
{
	struct iasecc_chipdoc_transaction_report report;
	if (test_prv.chipdoc_transaction.active) (void)iasecc_chipdoc_transaction_close(&test_card,&report);
	if (reader_reserved) { watched_output = NULL; assert(!sc_unlock(&test_card)); } /* Fake teardown only. */
	assert(!reader_reserved && !test_prv.chipdoc_session.channel);
}
#ifndef IASECC_CHIPDOC_SIGN_FIXTURE
int main(void)
{
	(void)thread_ops;
	test_provider.libctx = OSSL_LIB_CTX_new(); alternate_provider.libctx = OSSL_LIB_CTX_new(); assert(test_provider.libctx && alternate_provider.libctx);
	test_provider.defprov = OSSL_PROVIDER_load(test_provider.libctx,"default"); alternate_provider.defprov = OSSL_PROVIDER_load(alternate_provider.libctx,"default");
	assert(test_provider.defprov && alternate_provider.defprov); init(&peer); memcpy(initial_saved,peer.initial,137);
	EVP_PKEY_CTX *rsa = EVP_PKEY_CTX_new_from_name(test_provider.libctx,"RSA",NULL); assert(rsa);
	assert(EVP_PKEY_keygen_init(rsa) > 0 && EVP_PKEY_CTX_set_rsa_keygen_bits(rsa,2048) > 0 && EVP_PKEY_keygen(rsa,&rsa_key) > 0); EVP_PKEY_CTX_free(rsa);
	for (size_t i = 0; i < 32; i++) digest[i] = i;
	memset(untouched,0xA5,256);
	unsigned char output[256], saved_digest[32]; memcpy(saved_digest,digest,32);
	struct iasecc_chipdoc_channel_report report; struct iasecc_chipdoc_signature_report signed_report;
	struct sc_security_env env;
	for (int nested = 0; nested < 2; nested++) {
		logged_in(); if (nested) assert(!sc_lock(&test_card));
		env = environment(); struct sc_security_env saved = env;
		assert(!iasecc_chipdoc_channel_set_security_env(&test_card,&env,0,&report) && !memcmp(&env,&saved,sizeof(env)));
		assert(report.commands == 2 && report.sw == 0x9000 && test_prv.chipdoc_session.key_reference == 0x91);
		assert(iasecc_chipdoc_channel_set_security_env(&test_card,&env,0,&report) != 0 && physical == 11);
		memcpy(output,untouched,256); watched_output = output;
		assert(iasecc_chipdoc_channel_compute_signature(&test_card,digest,32,output,256,&signed_report) == 256); watched_output = NULL;
		assert(signed_report.channel.commands == 1 && signed_report.channel.authenticated && signed_report.channel.sw == 0x9000);
		assert(signed_report.close.complete && !signed_report.channel.reset_required && signing_calls == 3 && physical == 12 && credential_attempts == 1);
		assert(!memcmp(output,expected_signature,256) && !memcmp(digest,saved_digest,32) && !test_prv.chipdoc_session.channel);
		rsa = EVP_PKEY_CTX_new(rsa_key,NULL); assert(rsa); assert(EVP_PKEY_verify_init(rsa) > 0 && EVP_PKEY_CTX_set_rsa_padding(rsa,RSA_PKCS1_PADDING) > 0);
		assert(EVP_PKEY_CTX_set_signature_md(rsa,EVP_sha256()) > 0 && EVP_PKEY_verify(rsa,output,256,digest,32) == 1);
		output[0] ^= 1; assert(EVP_PKEY_verify(rsa,output,256,digest,32) != 1); EVP_PKEY_CTX_free(rsa);
		assert(test_card.lock_count == nested && reader_reserved == nested && resets == 2);
		if (nested) assert(!sc_unlock(&test_card));
		assert(iasecc_chipdoc_channel_compute_signature(&test_card,digest,32,output,256,&signed_report) != 256 && physical == 12);
	}
	puts("PASS typed RSA2048/SHA256 environment selects bound AID/MSE once, independent encrypted frames35/35/74 and SSC9..14; verified synthetic RSA signature published only after complete close, nested locks preserved");
	for (int variant = 0; variant < 20; variant++) {
		logged_in(); env = environment();
		switch (variant) {
		case 0: env.flags = 0; break; case 1: env.flags |= SC_SEC_ENV_ALG_REF_PRESENT; break;
		case 2: env.operation = SC_SEC_OPERATION_DECIPHER; break; case 3: env.operation = SC_SEC_OPERATION_AUTHENTICATE; break;
		case 4: env.algorithm = SC_ALGORITHM_EC; break; case 5: env.key_ref_len = 0; break; case 6: env.key_ref_len = 2; break;
		case 7: env.key_ref[0] = 0x11; break; case 8: env.key_ref[0] = 0x92; break; case 9: env.key_size_bits = 3072; break;
		case 10: env.key_size_bits = 0; break; case 11: env.algorithm_flags |= SC_ALGORITHM_RSA_HASH_NONE; break;
		case 12: env.algorithm_flags = SC_ALGORITHM_RSA_RAW; break; case 13: env.algorithm_flags = SC_ALGORITHM_RSA_PAD_PKCS1_TYPE_01|SC_ALGORITHM_RSA_HASH_SHA1; break;
		case 14: env.file_ref.len = 2; break; case 15: env.file_ref.aid.len = 1; break; case 16: env.target_file_ref.len = 2; break;
		case 17: env.params[0].param_type = SC_SEC_ENV_PARAM_IV; break; case 18: env.params[9].value = (void *)1; break; case 19: env.params[0].value_len = 1; break;
		}
		void *owner = test_prv.chipdoc_session.channel;
		assert(iasecc_chipdoc_channel_set_security_env(&test_card,&env,0,&report) != 0 && physical == 9 && !signing_calls);
		assert(owner == test_prv.chipdoc_session.channel && test_prv.chipdoc_session.phase == IASECC_CHIPDOC_LOGGED_IN); close_transaction();
	}
	logged_in(); env = environment();
	assert(iasecc_chipdoc_channel_set_security_env(&test_card,NULL,0,&report) != 0 && physical == 9);
	assert(iasecc_chipdoc_channel_set_security_env(&test_card,&env,1,&report) != 0 && physical == 9);
	assert(iasecc_chipdoc_channel_set_security_env(&test_card,&env,0,NULL) != 0 && physical == 9); close_transaction();
	puts("PASS unsupported key/hash/RSA3072/flags/files/parameters/operation/environment fail before AID/MSE without losing owner or reading optional pointers");
	for (int variant = 0; variant < 8; variant++) {
		logged_in(); prepared(); memcpy(output,untouched,256); void *owner = test_prv.chipdoc_session.channel;
		const unsigned char *in = digest; size_t in_len = 32, out_len = 256; unsigned char *out = output;
		if (variant == 0) in = NULL;
		if (variant == 1) in_len = 0;
		if (variant == 2) in_len = 31;
		if (variant == 3) in_len = 33;
		if (variant == 4) in_len = 51;
		if (variant == 5) out = NULL;
		if (variant == 6) out_len = 255;
		if (variant == 7) { in = (unsigned char *)1; in_len = SIZE_MAX; }
		assert(iasecc_chipdoc_channel_compute_signature(&test_card,in,in_len,out,out_len,&signed_report) != 256 && physical == 11);
		assert(owner == test_prv.chipdoc_session.channel && !memcmp(output,untouched,256)); close_transaction();
	}
	puts("PASS only digest32/output256 accepted, no DigestInfo/message/hash conversion or PSO on local errors; output and MSE owner unchanged");
	for (int step = 1; step <= 3; step++) for (int mode = 1; mode <= 18; mode++) for (int bit = 0; bit < (mode == 2 ? 64 : 1); bit++) {
		if (step != 3 && mode >= 14 && mode <= 16) continue;
		logged_in(); env = environment(); failure_stage = step; failure_mode = mode; failure_bit = bit; memcpy(output,untouched,256);
		int rv = iasecc_chipdoc_channel_set_security_env(&test_card,&env,0,&report);
		if (step == 3) {
			assert(!rv); watched_output = output;
			rv = iasecc_chipdoc_channel_compute_signature(&test_card,digest,32,output,256,&signed_report); watched_output = NULL;
			if (mode == 14) assert(signed_report.channel.authenticated && signed_report.channel.sw == 0x6A82);
		} else assert(!report.commands || report.commands == (unsigned int)step);
		assert(rv < 0 && physical == (unsigned int)(9+step) && signing_calls == (unsigned int)step && credential_attempts == 1);
		assert(!test_prv.chipdoc_session.channel && !memcmp(output,untouched,256));
		if (mode == 18) assert(EVP_set_default_properties(test_provider.libctx,NULL));
		assert(iasecc_chipdoc_channel_compute_signature(&test_card,digest,32,output,256,&signed_report) < 0 && physical == (unsigned int)(9+step));
		release_failed();
	}
	puts("PASS first transport/MAC/all64 bits/replay/padding/payload/size/61/6C/status/reset/reattach/removal/provider failure stops each signing stage, closes accepted PSO, never retries or returns partial output; authenticated status-only6A82 stays error");
	for (int cleanup = 0; cleanup < 2; cleanup++) {
		logged_in(); prepared(); memcpy(output,untouched,256); watched_output = output;
		if (cleanup) unlock_fail = 1; else reset_fail_at = 2;
		assert(iasecc_chipdoc_channel_compute_signature(&test_card,digest,32,output,256,&signed_report) < 0); watched_output = NULL;
		assert(signed_report.channel.authenticated && !signed_report.close.complete && signed_report.close.reset_required);
		assert(!memcmp(output,untouched,256) && !test_prv.chipdoc_session.channel && !reader_reserved && credential_attempts == 1);
		struct iasecc_chipdoc_transaction_report tx;
		assert(iasecc_chipdoc_transaction_begin(&p15,&key_object,&tx) < 0 && physical == 12);
	}
	puts("PASS valid authenticated signature withheld when reset/unlock fail; recovery quarantine and cleanup metadata retained, no automatic restart or PIN replay");
	logged_in(); env = environment(); struct chipdoc_crypto *crypto = test_prv.chipdoc_session.channel; crypto->sm.keys.ssc[15] = 6;
	assert(iasecc_chipdoc_channel_set_security_env(&test_card,&env,0,&report) < 0 && physical == 9 && !test_prv.chipdoc_session.channel); release_failed();
	logged_in(); prepared(); crypto = test_prv.chipdoc_session.channel; crypto->sm.keys.ssc[15] = 10;
	assert(iasecc_chipdoc_channel_compute_signature(&test_card,(unsigned char *)1,32,(unsigned char *)1,256,&signed_report) < 0 && physical == 11); release_failed();
	logged_in(); prepared(); assert(EVP_set_default_properties(test_provider.libctx,"provider=unavailable"));
	memcpy(output,untouched,256); assert(iasecc_chipdoc_channel_compute_signature(&test_card,digest,32,output,256,&signed_report) < 0 && physical == 11);
	assert(EVP_set_default_properties(test_provider.libctx,NULL)); assert(!test_prv.chipdoc_transaction.active && !memcmp(output,untouched,256));
	puts("PASS stale SSC stops before digest/output access; denied private provider sends no PSO and still closes, without global fallback");
	EVP_PKEY_free(rsa_key); EVP_PKEY_free(peer.peer); jcop4_ca_pending_clear(&peer.derived);
	OSSL_PROVIDER_unload(test_provider.defprov); OSSL_LIB_CTX_free(test_provider.libctx);
	OSSL_PROVIDER_unload(alternate_provider.defprov); OSSL_LIB_CTX_free(alternate_provider.libctx);
	return 0;
}
#endif
