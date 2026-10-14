/* Actual native PKCS15 frontend; explicit software routing, no public binding. */
#define IASECC_CHIPDOC_SIGN_FIXTURE
#include "iasecc-chipdoc-sign.c"
#if defined(ENABLE_NOTIFY)
#error Native software frontend requires notifications disabled
#endif
static struct sc_pkcs15_object native_key, native_pin;
static struct sc_pkcs15_tokeninfo tokeninfo;
static struct sc_algorithm_info rsa_algorithm;
static unsigned int frontend_pins, frontend_selects, frontend_mses, frontend_psos;
static int decoded;
static struct sc_security_env frontend_environment;
static struct iasecc_chipdoc_signature_report frontend_signature;

static int frontend_pin(struct sc_card *card, struct sc_pin_cmd_data *data)
{
	struct iasecc_chipdoc_transaction_report tx; struct iasecc_chipdoc_channel_report report; int rv, tries = -1;
	assert(card == &test_card && data && data->cmd == SC_PIN_CMD_VERIFY && data->pin_type == SC_AC_CHV && data->pin_reference == 1);
	assert(data->pin1.encoding == 0 && data->pin1.min_length == 6 && data->pin1.max_length == 14 && !data->flags);
	frontend_pins++; assert(frontend_pins == 1 && !p15.opts.use_pin_cache);
	rv = iasecc_chipdoc_transaction_begin(&p15,&native_key,&tx);
	if (!rv) rv = iasecc_chipdoc_channel_query_pin(card,1,&tries,&report);
	if (!rv) rv = iasecc_chipdoc_channel_verify_pin(card,data,&tries,&report);
	data->pin1.tries_left = tries;
	if (rv && test_prv.chipdoc_transaction.active) (void)iasecc_chipdoc_transaction_close(card,&tx);
	return rv;
}
static int frontend_select(struct sc_card *card, const struct sc_path *path, struct sc_file **file)
{
	struct iasecc_chipdoc_channel_report report;
	assert(card == &test_card && path && !file && !path->len && path->type == SC_PATH_TYPE_PATH);
	assert(path->aid.len == app.aid.len && !memcmp(path->aid.value,app.aid.value,app.aid.len));
	frontend_selects++; return iasecc_chipdoc_channel_select_application(card,&report);
}
static int frontend_mse(struct sc_card *card, const struct sc_security_env *env, int number)
{
	struct iasecc_chipdoc_channel_report report;
	assert(card == &test_card && env && !number); frontend_mses++; frontend_environment = *env;
	return iasecc_chipdoc_channel_set_security_env(card,env,number,&report);
}
static int frontend_pso(struct sc_card *card, const unsigned char *in, size_t in_len, unsigned char *out, size_t out_len)
{
	assert(card == &test_card && in_len == 32 && !memcmp(in,digest,32)); frontend_psos++;
	int rv = iasecc_chipdoc_channel_compute_signature(card,in,in_len,out,out_len,&frontend_signature);
	/* Owned transaction is closed before output; the PKCS15 outer lock remains
	 * the frontend's responsibility and is released after this callback. */
	watched_output = NULL; return rv;
}
static void free_native(void)
{
	if (!decoded) return;
	assert(!native_pin.content.value && !native_pin.content.len);
	struct sc_pkcs15_prkey_info *info = native_key.data;
	assert(!info->subject.value && !info->params.data && !native_key.content.value);
	free(native_key.data); free(native_pin.data); decoded = 0;
}
static void frontend_defaults(int with_aid)
{
	free_native(); verify_defaults();
	reader_ops.transmit = signature_backend; reader_ops.reset = signing_reset; reader_ops.unlock = signing_unlock;
	failure_stage = failure_mode = failure_bit = 0; signing_calls = 0; watched_output = NULL;
	memset(&native_key,0,sizeof(native_key)); memset(&native_pin,0,sizeof(native_pin));
	unsigned char *der; const unsigned char *cursor; size_t size, left;
	assert(!sc_pkcs15_encode_prkdf_entry(&test_ctx,&key_object,&der,&size)); cursor = der; left = size;
	assert(!sc_pkcs15_decode_prkdf_entry(&p15,&native_key,&cursor,&left) && !left); free(der);
	assert(!sc_pkcs15_encode_aodf_entry(&test_ctx,&pin_object,&der,&size)); cursor = der; left = size;
	assert(!sc_pkcs15_decode_aodf_entry(&p15,&native_pin,&cursor,&left) && !left); free(der);
	decoded = 1; native_key.next = &native_pin; p15.obj_list = &native_key; p15.magic = SC_PKCS15_CARD_MAGIC;
	struct sc_pkcs15_prkey_info *key = native_key.data;
	assert(!key->path.len && key->path.aid.len == app.aid.len);
	if (!with_aid) key->path.aid.len = 0;
	struct sc_pkcs15_auth_info *pin = native_pin.data;
	assert(pin->auth_method == SC_AC_CHV && !pin->path.len && !pin->path.aid.len);
	p15.opts.use_pin_cache = p15.opts.pin_cache_ignore_user_consent = 1; p15.opts.pin_cache_counter = 10;
	memset(&tokeninfo,0,sizeof(tokeninfo)); p15.tokeninfo = &tokeninfo;
	memset(&rsa_algorithm,0,sizeof(rsa_algorithm)); rsa_algorithm.algorithm = SC_ALGORITHM_RSA; rsa_algorithm.key_length = 2048;
	rsa_algorithm.flags = SC_ALGORITHM_RSA_PAD_PKCS1_TYPE_01|SC_ALGORITHM_RSA_HASH_SHA256;
	test_card.algorithms = &rsa_algorithm; test_card.algorithm_count = 1;
	card_ops.pin_cmd = frontend_pin; card_ops.select_file = frontend_select; card_ops.set_security_env = frontend_mse; card_ops.compute_signature = frontend_pso;
	frontend_pins = frontend_selects = frontend_mses = frontend_psos = 0;
	memset(&frontend_environment,0,sizeof(frontend_environment)); memset(&frontend_signature,0,sizeof(frontend_signature));
}
static void enable(void)
{
	struct iasecc_chipdoc_pkcs15_operation operation;
	assert(!iasecc_chipdoc_pkcs15_disable_pin_cache(&p15,&native_key,&operation));
	assert(operation.pin == &native_pin && operation.algorithm_flags == rsa_algorithm.flags && !p15.opts.use_pin_cache);
	assert(!physical && !reader_locks && !resets); /* No binding/IO/algorithm changes. */
}
static void authenticate(void)
{
	assert(!sc_pkcs15_verify_pin(&p15,&native_pin,fictitious_pin,6));
	assert(frontend_pins == 1 && physical == 9 && credential_attempts == 1 && test_card.lock_count == 1 && reader_reserved);
	assert(!native_pin.content.value && !native_pin.content.len && !p15.opts.use_pin_cache);
}
static void verify_rsa(const unsigned char output[256])
{
	EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(rsa_key,NULL); assert(ctx);
	assert(EVP_PKEY_verify_init(ctx) > 0 && EVP_PKEY_CTX_set_rsa_padding(ctx,RSA_PKCS1_PADDING) > 0);
	assert(EVP_PKEY_CTX_set_signature_md(ctx,EVP_sha256()) > 0 && EVP_PKEY_verify(ctx,output,256,digest,32) == 1); EVP_PKEY_CTX_free(ctx);
}
int main(void)
{
	(void)thread_ops;
	test_provider.libctx = OSSL_LIB_CTX_new(); alternate_provider.libctx = OSSL_LIB_CTX_new(); assert(test_provider.libctx && alternate_provider.libctx);
	test_provider.defprov = OSSL_PROVIDER_load(test_provider.libctx,"default"); alternate_provider.defprov = OSSL_PROVIDER_load(alternate_provider.libctx,"default");
	assert(test_provider.defprov && alternate_provider.defprov); init(&peer); memcpy(initial_saved,peer.initial,137);
	EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_from_name(test_provider.libctx,"RSA",NULL); assert(ctx);
	assert(EVP_PKEY_keygen_init(ctx) > 0 && EVP_PKEY_CTX_set_rsa_keygen_bits(ctx,2048) > 0 && EVP_PKEY_keygen(ctx,&rsa_key) > 0); EVP_PKEY_CTX_free(ctx);
	for (size_t i = 0; i < 32; i++) digest[i] = i;
	memset(untouched,0xA5,256); unsigned char output[256], input[51];
	const unsigned char prefix[19] = {0x30,0x31,0x30,0x0D,6,9,0x60,0x86,0x48,1,0x65,3,4,2,1,5,0,4,0x20};
	memcpy(input,prefix,19); memcpy(input+19,digest,32);
	for (int with_aid = 0; with_aid < 2; with_aid++) for (int digest_info = 0; digest_info < 2; digest_info++) {
		frontend_defaults(with_aid); enable(); authenticate(); memcpy(output,untouched,256); watched_output = output;
		unsigned long flags = SC_ALGORITHM_RSA_PAD_PKCS1_TYPE_01|(digest_info ? SC_ALGORITHM_RSA_HASH_NONE : SC_ALGORITHM_RSA_HASH_SHA256);
		assert(sc_pkcs15_compute_signature(&p15,&native_key,flags,digest_info ? input : digest,digest_info ? 51 : 32,output,256,NULL) == 256);
		assert(frontend_selects == (unsigned int)with_aid && frontend_mses == 1 && frontend_psos == 1 && signing_calls == 3 && physical == 12);
		assert(frontend_environment.flags == (SC_SEC_ENV_ALG_PRESENT|SC_SEC_ENV_KEY_REF_PRESENT) && frontend_environment.key_ref[0] == 0x91);
		assert(frontend_environment.algorithm_flags == rsa_algorithm.flags && frontend_environment.key_size_bits == 2048);
		assert(frontend_signature.close.complete && !test_card.lock_count && !reader_reserved && resets == 2 && reader_unlocks == 1);
		assert(!native_pin.content.value && !native_pin.content.len && !memcmp(input,prefix,19) && !memcmp(input+19,digest,32)); verify_rsa(output);
	}
	puts("PASS real native DER/PKCS15 VERIFY and compute_signature generate canonical CHV/environment/digest32 through explicit software routing; SHA256 digest and SHA256 DigestInfo both verify RSA, optional AID selected protected exactly once, no PIN cache");
	for (int variant = 0; variant < 12; variant++) {
		frontend_defaults(1); struct sc_pkcs15_auth_info *pin = native_pin.data; struct sc_pkcs15_prkey_info *key = native_key.data;
		switch (variant) {
		case 0: pin->auth_method = SC_AC_AUT; break; case 1: pin->path.len = 2; break; case 2: pin->path.aid = app.aid; break;
		case 3: key->path.len = 2; break; case 4: key->path.type = SC_PATH_TYPE_FILE_ID; break;
		case 5: pin->attrs.pin.min_length = 5; break; case 6: pin->attrs.pin.max_length = 15; break;
		case 7: pin->attrs.pin.type = SC_PKCS15_PIN_TYPE_BCD; break;
		case 8: pin->attrs.pin.flags |= SC_PKCS15_PIN_FLAG_NEEDS_PADDING; break;
		case 9: native_pin.content.value = (unsigned char *)1; break; case 10: native_pin.content.len = 6; break;
		case 11: key->path.aid.value[0] ^= 1; break;
		}
		struct iasecc_chipdoc_pkcs15_operation operation, empty = {0}; memset(&operation,0xA5,sizeof(operation));
		assert(iasecc_chipdoc_pkcs15_disable_pin_cache(&p15,&native_key,&operation) < 0 && p15.opts.use_pin_cache == 1);
		assert(!memcmp(&operation,&empty,sizeof(operation)) && !physical && !reader_locks);
		native_pin.content.value = NULL; native_pin.content.len = 0;
	}
	frontend_defaults(1); struct iasecc_chipdoc_transaction_report tx;
	assert(iasecc_chipdoc_transaction_begin(&p15,&native_key,&tx) < 0 && !physical && !reader_locks && p15.opts.use_pin_cache);
	puts("PASS strict native operation policy rejects AUT8/paths/formats/padding/cached credential presence before mutation/IO; enabled cache blocks managed startup, no secret read/clear or native metadata rewrite");
	for (int step = 1; step <= 3; step++) {
		frontend_defaults(1); enable(); authenticate(); failure_stage = step; failure_mode = 3;
		memcpy(output,untouched,256);
		assert(sc_pkcs15_compute_signature(&p15,&native_key,rsa_algorithm.flags,digest,32,output,256,NULL) == SC_ERROR_SECURITY_STATUS_NOT_SATISFIED);
		assert(frontend_pins == 1 && credential_attempts == 1 && signing_calls == (unsigned int)step && physical == (unsigned int)(9+step));
		assert(!memcmp(output,untouched,256) && !native_pin.content.value && !test_prv.chipdoc_session.channel);
		release_failed();
	}
	puts("PASS actual PKCS15 use_key security-status revalidation sends no cached PIN or repeat AID/MSE/PSO when policy disables cache, each failing stage remains error/output untouched");
	for (int blocked = 0; blocked < 2; blocked++) {
		frontend_defaults(1); enable(); verify_sw = blocked ? 0x63C0 : 0x63C6;
		assert(sc_pkcs15_verify_pin(&p15,&native_pin,fictitious_pin,6) == (blocked ? SC_ERROR_AUTH_METHOD_BLOCKED : SC_ERROR_PIN_CODE_INCORRECT));
		assert(frontend_pins == 1 && credential_attempts == 1 && physical == 9 && !native_pin.content.value && !test_card.lock_count && !reader_reserved);
		struct sc_pkcs15_auth_info *pin = native_pin.data; assert(pin->tries_left == (blocked ? 0 : 6));
	}
	puts("PASS actual PKCS15 authenticated PIN errors propagate counts without caching/retry/signing, startup/query/VERIFY closed with outer frontend lock released");
	for (int variant = 0; variant < 4; variant++) {
		frontend_defaults(1); enable(); authenticate(); unsigned char bad[51]; memcpy(bad,input,51);
		unsigned long flags = SC_ALGORITHM_RSA_PAD_PKCS1_TYPE_01|SC_ALGORITHM_RSA_HASH_NONE; size_t size = 51;
		if (variant == 0) bad[14] = 2; /* SHA384 OID with SHA256-sized content. */
		if (variant == 1) size = 50;
		if (variant == 2) flags = SC_ALGORITHM_RSA_PAD_PSS|SC_ALGORITHM_RSA_HASH_SHA256;
		if (variant == 3) flags = SC_ALGORITHM_RSA_PAD_PKCS1_TYPE_01|SC_ALGORITHM_RSA_HASH_SHA1;
		memcpy(output,untouched,256);
		assert(sc_pkcs15_compute_signature(&p15,&native_key,flags,bad,size,output,256,NULL) < 0 && physical == 9 && !frontend_mses && !frontend_psos);
		assert(!memcmp(output,untouched,256) && credential_attempts == 1); release_failed();
	}
	frontend_defaults(1); enable(); authenticate(); reset_fail_at = 2; memcpy(output,untouched,256);
	assert(sc_pkcs15_compute_signature(&p15,&native_key,rsa_algorithm.flags,digest,32,output,256,NULL) < 0 && !memcmp(output,untouched,256));
	assert(frontend_pins == 1 && credential_attempts == 1 && !frontend_signature.close.complete && !test_card.lock_count && !reader_reserved);
	puts("PASS real PKCS15 rejects unsupported/malformed DigestInfo/hash/PSS before device signing; final managed reset failure withholds output and never replays authentication");
	free_native(); EVP_PKEY_free(rsa_key); EVP_PKEY_free(peer.peer); jcop4_ca_pending_clear(&peer.derived);
	OSSL_PROVIDER_unload(test_provider.defprov); OSSL_LIB_CTX_free(test_provider.libctx);
	OSSL_PROVIDER_unload(alternate_provider.defprov); OSSL_LIB_CTX_free(alternate_provider.libctx); return 0;
}
