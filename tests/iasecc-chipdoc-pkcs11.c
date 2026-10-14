/* Real built opensc-pkcs11 module; context/discovery/reader are software only.
 * No real PCSC context is ever created. Certificate/key/PIN are synthetic. */
#include "pkcs11/pkcs11.h"
#define IASECC_CHIPDOC_INIT_FIXTURE
#include "iasecc-chipdoc-init.c"
#include <openssl/x509.h>
#include <dlfcn.h>
static unsigned int creates, connects, disconnects, releases, discoveries;
static int event_state, connected, expected_finish_failure, presence = SC_READER_CARD_PRESENT;
int sc_context_create(struct sc_context **out, const sc_context_param_t *options)
{
	assert(options && out && !strcmp(options->app_name,"opensc-pkcs11")); creates++;
	*out = &test_ctx; return 0;
}
int sc_release_context(struct sc_context *ctx) { assert(ctx == &test_ctx && !connected); releases++; return 0; }
int sc_ctx_detect_readers(struct sc_context *ctx) { assert(ctx == &test_ctx); discoveries++; return 0; }
unsigned int sc_ctx_get_reader_count(struct sc_context *ctx) { assert(ctx == &test_ctx); return 1; }
struct sc_reader *sc_ctx_get_reader(struct sc_context *ctx, unsigned int index) { assert(ctx == &test_ctx && !index); return &test_reader; }
int sc_detect_card_presence(struct sc_reader *reader) { assert(reader == &test_reader); return presence; }
int sc_wait_for_event(struct sc_context *ctx, unsigned int mask, struct sc_reader **reader, unsigned int *events, int timeout, void **state)
{
	assert(ctx == &test_ctx && timeout == 0); if (state) *state = mask ? &event_state : NULL;
	if (reader) *reader = &test_reader;
	if (events) *events = 0;
	return SC_ERROR_EVENT_TIMEOUT;
}
int sc_cancel(struct sc_context *ctx) { assert(ctx == &test_ctx); return 0; }
int sc_connect_card(struct sc_reader *reader, struct sc_card **out)
{
	assert(reader == &test_reader && !connected); connects++; int rv = initialize();
	if (!rv) { connected = 1; *out = &test_card; } return rv;
}
int sc_disconnect_card(struct sc_card *card)
{
	assert(card == &test_card && connected); disconnects++;
	if (!expected_finish_failure) { finish_initialized(); connected = 0; return 0; }
	int rv = card_ops.finish(card); assert(rv && !card->drv_data && !reader_reserved);
	free(card->algorithms); card->algorithms = NULL; card->algorithm_count = 0; card->drv_data = &test_prv;
	connected = 0; return rv;
}
static void certificate_files(void)
{
	/* Additional native authentication key must not become a signable
	 * PKCS11 private key merely because it is also RSA2048. */
	struct sc_pkcs15_prkey_info authkey = *(struct sc_pkcs15_prkey_info *)native_key.data;
	authkey.key_reference = 0x90; authkey.id.value[0] = 2;
	struct sc_pkcs15_object authobj = native_key; authobj.data = &authkey;
	unsigned char *der; size_t der_size;
	assert(!sc_pkcs15_encode_prkdf_entry(&test_ctx,&authobj,&der,&der_size));
	unsigned char *both = realloc(files[2].data,files[2].size+der_size); assert(both);
	files[2].data = both; memcpy(both+files[2].size,der,der_size); files[2].size += der_size; free(der);
	/* The profile must stay readonly even without TokenInfo's READONLY bit. */
	tokeninfo.flags = SC_PKCS15_TOKEN_LOGIN_REQUIRED; free(files[0].data);
	assert(!sc_pkcs15_encode_tokeninfo(&test_ctx,&tokeninfo,&files[0].data,&files[0].size));
	X509 *cert = X509_new(); assert(cert);
	assert(X509_set_version(cert,2) && ASN1_INTEGER_set(X509_get_serialNumber(cert),1));
	assert(X509_gmtime_adj(X509_getm_notBefore(cert),0) && X509_gmtime_adj(X509_getm_notAfter(cert),3600));
	X509_NAME *name = X509_get_subject_name(cert);
	assert(X509_NAME_add_entry_by_txt(name,"CN",MBSTRING_ASC,(const unsigned char *)"Synthetic",-1,-1,0));
	assert(X509_set_issuer_name(cert,name) && X509_set_pubkey(cert,rsa_key) && X509_sign(cert,rsa_key,EVP_sha256()) > 0);
	files[6].fid = 0xD100; int len = i2d_X509(cert,NULL); assert(len > 0);
	files[6].size = len; files[6].data = malloc(len); assert(files[6].data); unsigned char *cursor = files[6].data;
	assert(i2d_X509(cert,&cursor) == len); X509_free(cert);
	struct sc_pkcs15_cert_info info = {0}; info.id = ((struct sc_pkcs15_prkey_info *)native_key.data)->id;
	sc_format_path("D100",&info.path); struct sc_pkcs15_object obj = {0}; obj.type = SC_PKCS15_TYPE_CERT_X509; obj.data = &info;
	strcpy(obj.label,"Synthetic signature certificate"); files[5].fid = 0x7005;
	assert(!sc_pkcs15_encode_cdf_entry(&test_ctx,&obj,&files[5].data,&files[5].size));
	struct sc_pkcs15_df keydf = {0}, pindf = {0}, certdf = {0};
	keydf.type = SC_PKCS15_PRKDF; pindf.type = SC_PKCS15_AODF; certdf.type = SC_PKCS15_CDF;
	sc_format_path("7002",&keydf.path); sc_format_path("7001",&pindf.path); sc_format_path("7005",&certdf.path);
	keydf.next = &pindf; pindf.next = &certdf; p15.df_list = &keydf; free(files[1].data);
	assert(!sc_pkcs15_encode_odf(&test_ctx,&p15,&files[1].data,&files[1].size)); p15.df_list = NULL;
}
static CK_FUNCTION_LIST_PTR api;
static void fresh_peer(void)
{
    assert(!reader_reserved && !test_card.lock_count);
    EVP_PKEY_free(peer.peer); jcop4_ca_pending_clear(&peer.derived); init(&peer); peer.le = JCOP4_CA_LE_SHORT_MAX;
    memcpy(initial_saved,peer.initial,137);
    physical = plain_calls = credential_attempts = signing_calls = reader_locks = reader_unlocks = resets = 0;
    failure_stage = failure_mode = failure_bit = verify_mode = verify_bit = 0;
    verify_sw = verify_outer = 0x9000; query_mode = query_bit = 0; query_sw = 0x63C7; query_outer = 0x9000;
}
static void state(CK_SESSION_HANDLE h, CK_STATE expected)
{
    unsigned int before = physical, metadata = metadata_commands, attempts = credential_attempts;
    CK_SESSION_INFO info; CK_RV rv = api->C_GetSessionInfo(h,&info);
    if (rv || info.state != expected) fprintf(stderr,"fixture state rv=%lu actual=%lu expected=%lu physical=%u reserved=%d\n",rv,info.state,expected,physical,reader_reserved);
    assert(rv == CKR_OK && info.state == expected);
    assert(physical == before && metadata_commands == metadata && credential_attempts == attempts);
}
static void attributes(CK_SESSION_HANDLE h, CK_OBJECT_HANDLE obj, CK_ATTRIBUTE_TYPE type, void *out, CK_ULONG size)
{
    CK_ATTRIBUTE attr = {type,out,size}; assert(api->C_GetAttributeValue(h,obj,&attr,1) == CKR_OK && attr.ulValueLen == size);
}
static void login(CK_SESSION_HANDLE h)
{
    fresh_peer(); assert(api->C_Login(h,CKU_USER,fictitious_pin,6) == CKR_OK);
    assert(physical == 9 && credential_attempts == 1 && reader_reserved && resets == 1);
    state(h,CKS_RO_USER_FUNCTIONS);
}
static void digest_info(unsigned char info[51])
{ memcpy(info,"\x30\x31\x30\x0d\x06\x09\x60\x86\x48\x01\x65\x03\x04\x02\x01\x05\x00\x04\x20",19); memcpy(info+19,digest,32); }

static CK_SLOT_ID slot;
static CK_SESSION_HANDLE session, second;
static CK_OBJECT_HANDLE find(CK_SESSION_HANDLE h, CK_OBJECT_CLASS type)
{
	CK_ATTRIBUTE attr = {CKA_CLASS,&type,sizeof(type)}; CK_OBJECT_HANDLE object; CK_ULONG count;
	assert(api->C_FindObjectsInit(h,&attr,1) == CKR_OK);
	assert(api->C_FindObjects(h,&object,1,&count) == CKR_OK && count == 1);
	assert(api->C_FindObjectsFinal(h) == CKR_OK); return object;
}
#ifndef IASECC_CHIPDOC_PKCS11_FIXTURE
int main(int argc, char **argv)
{
	(void)thread_ops;
	setvbuf(stdout,NULL,_IOLBF,0);
	test_provider.libctx = OSSL_LIB_CTX_new(); alternate_provider.libctx = OSSL_LIB_CTX_new(); assert(test_provider.libctx && alternate_provider.libctx);
	test_provider.defprov = OSSL_PROVIDER_load(test_provider.libctx,"default"); alternate_provider.defprov = OSSL_PROVIDER_load(alternate_provider.libctx,"default");
	assert(test_provider.defprov && alternate_provider.defprov); init(&peer); memcpy(initial_saved,peer.initial,137);
	EVP_PKEY_CTX *rsa = EVP_PKEY_CTX_new_from_name(test_provider.libctx,"RSA",NULL); assert(rsa);
	assert(EVP_PKEY_keygen_init(rsa) > 0 && EVP_PKEY_CTX_set_rsa_keygen_bits(rsa,2048) > 0 && EVP_PKEY_keygen(rsa,&rsa_key) > 0); EVP_PKEY_CTX_free(rsa);
	for (size_t i = 0; i < 32; i++) digest[i] = i;
	memset(untouched,0xA5,256); init_defaults(); certificate_files();
	test_reader.name = "Synthetic reader"; test_reader.ctx = &test_ctx;
	test_reader.capabilities = SC_READER_CAP_PIN_PAD;
	assert(argc == 2 && C_GetFunctionList(&api) == CKR_OK && api);
	Dl_info module; assert(dladdr((void *)api->C_Initialize,&module) && !strcmp(module.dli_fname,argv[1]));
	assert(api->C_Initialize(NULL) == CKR_OK);
	CK_ULONG count = 1; assert(api->C_GetSlotList(CK_TRUE,&slot,&count) == CKR_OK && count == 1);

    assert(creates == 1 && connects == 1 && !physical && !credential_attempts && !resets);
    CK_TOKEN_INFO ti; unsigned int before = metadata_commands;
    assert(api->C_GetTokenInfo(slot,&ti) == CKR_OK && metadata_commands == before && !physical);
    assert((ti.flags & (CKF_WRITE_PROTECTED|CKF_TOKEN_INITIALIZED|CKF_LOGIN_REQUIRED|CKF_USER_PIN_INITIALIZED)) ==
        (CKF_WRITE_PROTECTED|CKF_TOKEN_INITIALIZED|CKF_LOGIN_REQUIRED|CKF_USER_PIN_INITIALIZED));
    assert(!(ti.flags & (CKF_RNG|CKF_PROTECTED_AUTHENTICATION_PATH)) && ti.ulMinPinLen == 6 && ti.ulMaxPinLen == 14);
    assert(api->C_OpenSession(slot,CKF_SERIAL_SESSION,NULL,NULL,&session) == CKR_OK);
    assert(api->C_OpenSession(slot,CKF_SERIAL_SESSION,NULL,NULL,&second) == CKR_OK);
    CK_OBJECT_HANDLE cert = find(session,CKO_CERTIFICATE), pub = find(session,CKO_PUBLIC_KEY);
    unsigned char *encoded = malloc(files[6].size); assert(encoded);
    attributes(session,cert,CKA_VALUE,encoded,files[6].size); assert(!memcmp(encoded,files[6].data,files[6].size)); free(encoded);
    CK_ULONG bits; attributes(session,pub,CKA_MODULUS_BITS,&bits,sizeof(bits)); assert(bits == 2048);
    unsigned char id; attributes(session,pub,CKA_ID,&id,1); assert(id == 1); attributes(session,cert,CKA_ID,&id,1); assert(id == 1);
    state(session,CKS_RO_PUBLIC_SESSION); state(second,CKS_RO_PUBLIC_SESSION);
    puts("PASS actual built module identity checked; C_Initialize/GetSlotList native bind and public certificate/derived RSA2048 key/IDs/token policy use synthetic ISO metadata, no PIN/status APDU or real context/reader");

    CK_MECHANISM_TYPE types[64]; count = 64; assert(api->C_GetMechanismList(slot,types,&count) == CKR_OK);
    unsigned int signs = 0;
    for (CK_ULONG i = 0; i < count; i++) {
        CK_MECHANISM_INFO info; assert(api->C_GetMechanismInfo(slot,types[i],&info) == CKR_OK);
        assert(!(info.flags & (CKF_DECRYPT|CKF_ENCRYPT|CKF_GENERATE|CKF_GENERATE_KEY_PAIR|CKF_DERIVE|CKF_WRAP|CKF_UNWRAP)));
        if (info.flags & CKF_SIGN) {
            assert(types[i] == CKM_RSA_PKCS || types[i] == CKM_SHA256_RSA_PKCS);
            assert(info.ulMinKeySize == 2048 && info.ulMaxKeySize == 2048); signs++;
        } else assert(info.flags == CKF_DIGEST);
    }
    assert(signs == 2);
    CK_MECHANISM_TYPE unsupported[] = {CKM_RSA_X_509,CKM_SHA1_RSA_PKCS,CKM_SHA384_RSA_PKCS,CKM_RSA_PKCS_PSS,CKM_RSA_PKCS_KEY_PAIR_GEN,CKM_ECDSA};
    for (size_t i = 0; i < sizeof(unsupported)/sizeof(unsupported[0]); i++) { CK_MECHANISM_INFO info; assert(api->C_GetMechanismInfo(slot,unsupported[i],&info) == CKR_MECHANISM_INVALID); }
    assert(!physical && !credential_attempts);
    puts("PASS mechanisms advertise only RSA_PKCS and SHA256_RSA_PKCS signing at2048 plus software digests/verify, no decipher/generation/PSS/raw/other RSA hashes/EC or device operation");

    assert(api->C_Login(session,CKU_USER,NULL,0) == CKR_PIN_LEN_RANGE && !credential_attempts && !physical);
    login(session); state(second,CKS_RO_USER_FUNCTIONS);
    assert(api->C_Login(second,CKU_USER,fictitious_pin,6) == CKR_USER_ALREADY_LOGGED_IN && physical == 9 && credential_attempts == 1);
    assert(api->C_Login(second,CKU_SO,NULL,0) == CKR_USER_TYPE_INVALID);
    assert(api->C_Login(second,CKU_CONTEXT_SPECIFIC,fictitious_pin,6) == CKR_USER_TYPE_INVALID && physical == 9 && credential_attempts == 1);
    CK_OBJECT_HANDLE key = find(session,CKO_PRIVATE_KEY); CK_BBOOL flag;
	CK_OBJECT_CLASS private_class = CKO_PRIVATE_KEY; CK_ATTRIBUTE only_private = {CKA_CLASS,&private_class,sizeof(private_class)};
	CK_OBJECT_HANDLE keys[3]; CK_ULONG key_count;
	assert(api->C_FindObjectsInit(session,&only_private,1) == CKR_OK && api->C_FindObjects(session,keys,3,&key_count) == CKR_OK && key_count == 1);
	assert(api->C_FindObjectsFinal(session) == CKR_OK);
    attributes(session,key,CKA_SIGN,&flag,sizeof(flag)); assert(flag);
    attributes(session,key,CKA_DECRYPT,&flag,sizeof(flag)); assert(!flag);
    attributes(session,key,CKA_EXTRACTABLE,&flag,sizeof(flag)); assert(!flag);
    attributes(session,key,CKA_ID,&id,1); assert(id == 1);
    unsigned char secret[32]; memset(secret,0xA5,sizeof(secret)); CK_ATTRIBUTE sensitive = {CKA_PRIVATE_EXPONENT,secret,sizeof(secret)};
    assert(api->C_GetAttributeValue(session,key,&sensitive,1) == CKR_ATTRIBUTE_SENSITIVE);
    for (size_t i = 0; i < sizeof(secret); i++) assert(secret[i] == 0xA5);
    CK_MECHANISM mechanism = {CKM_RSA_PKCS,NULL,0}; unsigned char info[51], output[256]; digest_info(info);
    assert(api->C_SignInit(session,&mechanism,key) == CKR_OK);
    count = 0; assert(api->C_Sign(session,info,51,NULL,&count) == CKR_OK && count == 256 && physical == 9);
    count = 255; memset(output,0xA5,256);
    assert(api->C_Sign(session,info,51,output,&count) == CKR_BUFFER_TOO_SMALL && count == 256 && physical == 9 && !memcmp(output,untouched,256));
    state(second,CKS_RO_USER_FUNCTIONS); count = 256;
    assert(api->C_Sign(session,info,51,output,&count) == CKR_OK && count == 256); verify_rsa(output);
    assert(physical == 12 && credential_attempts == 1 && resets == 2 && !reader_reserved && !test_card.lock_count);
    state(session,CKS_RO_PUBLIC_SESSION); state(second,CKS_RO_PUBLIC_SESSION);
    assert(api->C_SignInit(second,&mechanism,key) == CKR_USER_NOT_LOGGED_IN && physical == 12 && credential_attempts == 1);
    puts("PASS real C_Login/Sign with SHA256 DigestInfo verifies RSA; both sessions share live login then become public after PSO, size/small-buffer queries do not sign, unsupported login roles/private extraction do no credential attempt");

    static unsigned char message[] = "Synthetic PKCS11 message"; unsigned int digest_size;
    assert(EVP_Digest(message,sizeof(message)-1,digest,&digest_size,EVP_sha256(),NULL) && digest_size == 32);
    login(second); assert(api->C_CloseSession(session) == CKR_OK && reader_reserved && physical == 9); state(second,CKS_RO_USER_FUNCTIONS);
    mechanism.mechanism = CKM_SHA256_RSA_PKCS; key = find(second,CKO_PRIVATE_KEY);
    assert(api->C_SignInit(second,&mechanism,key) == CKR_OK); count = 256;
    assert(api->C_Sign(second,message,sizeof(message)-1,output,&count) == CKR_OK && count == 256); verify_rsa(output);
    assert(physical == 12 && credential_attempts == 1 && resets == 2 && !reader_reserved);
    state(second,CKS_RO_PUBLIC_SESSION);
    puts("PASS explicit new login on another surviving session succeeds, closing one session preserves other live login, one-part SHA256_RSA_PKCS hashes in software and verifies with one PSO/no replay");

    assert(api->C_OpenSession(slot,CKF_SERIAL_SESSION,NULL,NULL,&session) == CKR_OK); login(session); key = find(session,CKO_PRIVATE_KEY);
    assert(api->C_SignInit(session,&mechanism,key) == CKR_OK);
    assert(api->C_SignUpdate(session,message,7) == CKR_OK && api->C_SignUpdate(session,message+7,sizeof(message)-1-7) == CKR_OK);
    count = 0; assert(api->C_SignFinal(session,NULL,&count) == CKR_OK && count == 256 && physical == 9);
    count = 255; memset(output,0xA5,256); assert(api->C_SignFinal(session,output,&count) == CKR_BUFFER_TOO_SMALL && physical == 9 && !memcmp(output,untouched,256));
    count = 256; assert(api->C_SignFinal(session,output,&count) == CKR_OK); verify_rsa(output);
    assert(physical == 12 && credential_attempts == 1 && resets == 2 && !reader_reserved); state(second,CKS_RO_PUBLIC_SESSION);
    puts("PASS multipart SHA256 SignUpdate/SignFinal with non-mutating size/capacity queries verifies RSA and returns both sessions to public without repeating login");

    login(session); key = find(session,CKO_PRIVATE_KEY); mechanism.mechanism = CKM_RSA_PKCS;
    for (size_t i = 0; i < sizeof(unsupported)/sizeof(unsupported[0]); i++) {
        CK_MECHANISM bad = {unsupported[i],NULL,0}; assert(api->C_SignInit(session,&bad,key) == CKR_MECHANISM_INVALID);
    }
    assert(api->C_SignInit(session,&mechanism,key) == CKR_OK); digest_info(info); info[0] ^= 1; count = 256; memset(output,0xA5,256);
    assert(api->C_Sign(session,info,51,output,&count) != CKR_OK && physical == 9 && credential_attempts == 1 && !memcmp(output,untouched,256));
    state(second,CKS_RO_USER_FUNCTIONS);
    assert(api->C_Logout(session) == CKR_OK && !reader_reserved); state(second,CKS_RO_PUBLIC_SESSION);
    for (int step = 1; step <= 3; step++) {
        login(session); key = find(session,CKO_PRIVATE_KEY); failure_stage = step; failure_mode = 2;
        assert(api->C_SignInit(session,&mechanism,key) == CKR_OK); digest_info(info); count = 256; memset(output,0xA5,256);
        assert(api->C_Sign(session,info,51,output,&count) != CKR_OK && physical == (unsigned)(9+step) && credential_attempts == 1 && !reader_reserved && !memcmp(output,untouched,256));
        state(session,CKS_RO_PUBLIC_SESSION); state(second,CKS_RO_PUBLIC_SESSION);
    }
    fresh_peer(); verify_mode = 2; assert(api->C_Login(session,CKU_USER,fictitious_pin,6) != CKR_OK && credential_attempts == 1 && physical == 9 && !reader_reserved);
    state(session,CKS_RO_PUBLIC_SESSION);
    puts("PASS unsupported mechanisms/malformed DigestInfo stop before PSO; bad MAC at SELECT/MSE/PSO and unauthenticated VERIFY9000 fail with unchanged output, closed owned reservation, public sessions and no credential retry");

    login(session); presence = SC_READER_CARD_PRESENT|SC_READER_CARD_CHANGED;
    CK_SESSION_INFO si; assert(api->C_GetSessionInfo(session,&si) == CKR_SESSION_HANDLE_INVALID);
    assert(api->C_GetSessionInfo(second,&si) == CKR_SESSION_HANDLE_INVALID && !reader_reserved && credential_attempts == 1);
    presence = 0; count = 4; CK_SLOT_ID slots[4]; assert(api->C_GetSlotList(CK_TRUE,slots,&count) == CKR_OK && !count && !connected);
    assert(api->C_Finalize(NULL) == CKR_OK && creates == 1 && connects == 1 && disconnects == 1 && releases == 1);
    assert(!reader_reserved && !test_card.lock_count);
    puts("PASS simulated card change invalidates both sessions and closes live login, removal/finalize free native owner/routing/card state exactly once with no credential replay or real discovery");

    presence = SC_READER_CARD_PRESENT;
    for (int atomic = 0; atomic < 2; atomic++) {
        init_defaults(); certificate_files(); test_reader.name = "Synthetic reader"; test_reader.ctx = &test_ctx;
        scconf_block *block = scconf_block_add(test_ctx.conf,test_ctx.conf_blocks[0],"pkcs11",NULL); assert(block);
        assert(scconf_put_bool(block,atomic ? "atomic" : "lock_login",1));
        assert(api->C_Initialize(NULL) == CKR_OK); count = 4; assert(api->C_GetSlotList(CK_TRUE,slots,&count) == CKR_OK && !count);
        assert(!credential_attempts && !physical && !metadata_commands && !reader_reserved && !connected);
        assert(api->C_Finalize(NULL) == CKR_OK);
    }
    puts("PASS atomic and generic lock_login configurations reject profile binding before metadata/credentials, expose no usable token and finalize without cached PIN, automatic login or real PCSC context");

	init_defaults(); certificate_files(); test_reader.name = "Synthetic reader"; test_reader.ctx = &test_ctx;
	assert(api->C_Initialize(NULL) == CKR_OK); count = 1; assert(api->C_GetSlotList(CK_TRUE,&slot,&count) == CKR_OK && count == 1);
	assert(api->C_OpenSession(slot,CKF_SERIAL_SESSION,NULL,NULL,&session) == CKR_OK); login(session);
	reset_fail_at = 2;
	assert(api->C_CloseAllSessions(slot) != CKR_OK && !reader_reserved && !test_card.lock_count && resets == 2);
	assert(((struct iasecc_private_data *)test_card.drv_data)->chipdoc_recovery_required);
	assert(api->C_GetSessionInfo(session,&si) == CKR_SESSION_HANDLE_INVALID);
	assert(api->C_OpenSession(slot,CKF_SERIAL_SESSION,NULL,NULL,&session) == CKR_OK);
	assert(api->C_Login(session,CKU_USER,fictitious_pin,6) != CKR_OK && physical == 9 && credential_attempts == 1 && resets == 2);
	expected_finish_failure = 1; assert(api->C_Finalize(NULL) == CKR_OK && !connected && !reader_reserved);
	puts("PASS failed final reset propagates through CloseAllSessions after freeing handles, quarantines the same card instance and blocks another login before any VERIFY; finalize OK is not treated as proof of successful remote closure");
	free_files(); free_native(); scconf_free(test_ctx.conf); test_ctx.conf = NULL;
	EVP_PKEY_free(rsa_key); EVP_PKEY_free(peer.peer); jcop4_ca_pending_clear(&peer.derived);
	OSSL_PROVIDER_unload(test_provider.defprov); OSSL_LIB_CTX_free(test_provider.libctx);
	OSSL_PROVIDER_unload(alternate_provider.defprov); OSSL_LIB_CTX_free(alternate_provider.libctx);
	return 0;
}
#endif
