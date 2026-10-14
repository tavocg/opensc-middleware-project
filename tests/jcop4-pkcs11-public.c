/* Production diagnostic and real module, native context/reader are synthetic. */
#define IASECC_CHIPDOC_PKCS11_FIXTURE
#define sc_context_create fixture_context_create
#define sc_release_context fixture_release_context
#define sc_ctx_detect_readers fixture_detect_readers
#define sc_ctx_get_reader_count fixture_reader_count
#define sc_ctx_get_reader fixture_reader_get
#define sc_connect_card fixture_card_connect
#define sc_disconnect_card fixture_card_disconnect
#include "iasecc-chipdoc-pkcs11.c"
#undef sc_context_create
#undef sc_release_context
#undef sc_ctx_detect_readers
#undef sc_ctx_get_reader_count
#undef sc_ctx_get_reader
#undef sc_connect_card
#undef sc_disconnect_card
#define JCOP4_PKCS11_PUBLIC_FIXTURE
#define main public_probe_main
#include "../tools/jcop4-pkcs11-public.c"
#undef main
#undef printf
#include <sys/wait.h>
static sc_reader_t other_reader;
static struct sc_reader_driver fake_pcsc = {.short_name="pcsc"};
static unsigned int test_physical;
static int test_failure;
static int test_enumerate;
static int fake_reader_connect(sc_reader_t *r) { assert(r == &test_reader); return 0; }
static int fake_reader_disconnect(sc_reader_t *r) { assert(r == &test_reader); return test_failure == 5 ? -7 : 0; }
static int fake_release(sc_context_t *ctx) { int rv = fixture_release_context(ctx); return test_failure == 6 ? -7 : rv; }
static int fake_transmit(sc_reader_t *r,sc_apdu_t *a)
{
	assert((a->flags & (SC_APDU_FLAGS_NO_RETRY_WL|SC_APDU_FLAGS_NO_GET_RESP|SC_APDU_FLAGS_NO_SM)) ==
		(SC_APDU_FLAGS_NO_RETRY_WL|SC_APDU_FLAGS_NO_GET_RESP|SC_APDU_FLAGS_NO_SM));
	test_physical++; if (pub_report) puts("PRIVATE_BACKEND_RESPONSE_SHOULD_BE_DISCARDED");
	if (test_failure && test_failure <= 4) {
		a->sw1 = test_failure == 4 ? 0x61 : 0x6A; a->sw2 = 0x82;
		if (test_failure == 3) a->resplen++;
		return test_failure == 2 ? -7 : 0;
	}
	return metadata_backend(r,a);
}
static unsigned int two_readers(sc_context_t *ctx) { assert(ctx == &test_ctx); return 2; }
static sc_reader_t *two_get(sc_context_t *ctx,unsigned int i) { assert(ctx == &test_ctx && i < 2); return i ? &test_reader : &other_reader; }
static int native_connect(sc_reader_t *r,sc_card_t **out)
{ int rv = r->ops->connect(r); if (!rv) rv = fixture_card_connect(r,out); if (!rv && test_enumerate) (*out)->app_count = -1; return rv; }
static int native_disconnect(sc_card_t *card)
{ int rv = fixture_card_disconnect(card); if (test_enumerate) sc_free_apps(card); return rv; }
static void public_files(void)
{
	certificate_files(); files[4].fid = 0; files[6].fid = 0xB001;
	struct sc_pkcs15_cert_info info = {0}; info.id = ((struct sc_pkcs15_prkey_info *)native_key.data)->id;
	sc_format_path("B001",&info.path); struct sc_pkcs15_object obj = {0}; obj.type = SC_PKCS15_TYPE_CERT_X509; obj.data = &info;
	free(files[5].data); assert(!sc_pkcs15_encode_cdf_entry(&test_ctx,&obj,&files[5].data,&files[5].size));
}
static void setup(void)
{
	init_defaults(); public_files(); test_reader.name = "PRIVATE_READER_NAME"; test_reader.ctx = &test_ctx;
	test_ctx.card_drivers[0] = &driver;
	test_ctx.reader_driver = &fake_pcsc;
	reader_ops.connect = fake_reader_connect; reader_ops.disconnect = fake_reader_disconnect; reader_ops.transmit = fake_transmit;
	other_reader = test_reader; other_reader.name = "OTHER_PRIVATE_READER";
	pub_real.create = fixture_context_create; pub_real.release = fake_release;
	pub_real.count = two_readers; pub_real.reader = two_get;
	pub_real.detect = fixture_detect_readers; pub_real.presence = sc_detect_card_presence;
	pub_real.connect = native_connect; pub_real.disconnect = native_disconnect;
	creates = connects = releases = disconnects = discoveries = test_physical = 0; connected = expected_finish_failure = 0;
	test_failure = test_enumerate = 0;
}
static int public_run(const char *index,char output[8192])
{
	FILE *capture = tmpfile(); assert(capture); int out = dup(1), err = dup(2); assert(out >= 0 && err >= 0);
	fflush(stdout); assert(dup2(fileno(capture),1) >= 0);
	char *argv[] = {"public-probe",(char *)index,JCOP4_LOCAL_MODULE_PATH,NULL};
	int rv = public_probe_main(3,argv);
	assert(dup2(out,1) >= 0 && dup2(err,2) >= 0); close(out); close(err);
	fseek(capture,0,SEEK_SET); size_t n = fread(output,1,8191,capture); output[n] = 0; fclose(capture); return rv;
}
#ifndef JCOP4_PKCS11_PUBLIC_TEST_FIXTURE
int main(void)
{
	(void)thread_ops; (void)slot; (void)session; (void)second;
	test_provider.libctx = OSSL_LIB_CTX_new(); alternate_provider.libctx = OSSL_LIB_CTX_new(); assert(test_provider.libctx && alternate_provider.libctx);
	test_provider.defprov = OSSL_PROVIDER_load(test_provider.libctx,"default"); alternate_provider.defprov = OSSL_PROVIDER_load(alternate_provider.libctx,"default");
	assert(test_provider.defprov && alternate_provider.defprov); init(&peer); memcpy(initial_saved,peer.initial,137);
	EVP_PKEY_CTX *rsa = EVP_PKEY_CTX_new_from_name(test_provider.libctx,"RSA",NULL); assert(rsa);
	assert(EVP_PKEY_keygen_init(rsa) > 0 && EVP_PKEY_CTX_set_rsa_keygen_bits(rsa,2048) > 0 && EVP_PKEY_keygen(rsa,&rsa_key) > 0); EVP_PKEY_CTX_free(rsa);
	char output[8192]; setup(); int rv = public_run("1",output);
	if (rv) fprintf(stderr,"fixture report:\n%s",output);
	assert(!rv && creates == 1 && connects == 1 && releases == 1 && disconnects == 1 && !credential_attempts && !physical && !resets && test_physical > 0);
	assert(strstr(output,"validated_public_only_no_login_or_signature") && !strstr(output,"PRIVATE_READER") && !strstr(output,"Synthetic") && !strstr(output,"PRIVATE_QUERY"));
	puts("PASS actual diagnostic/module/native bind/CDF/DER/token/mechanisms/public IDs and reader namespace operate through production guard; exactly one selected reader, no login/VERIFY/channel/reset or private report values");
	setup(); test_enumerate = 1; files[7].fid = 0x2F00; files[7].size = 4+app.aid.len; files[7].data = malloc(files[7].size); assert(files[7].data);
	files[7].data[0] = 0x61; files[7].data[1] = 2+app.aid.len; files[7].data[2] = 0x4F; files[7].data[3] = app.aid.len;
	memcpy(files[7].data+4,app.aid.value,app.aid.len);
	rv = public_run("1",output); if (rv) fprintf(stderr,"fixture directory report:\n%s",output);
	assert(!rv && !credential_attempts && !physical && test_card.app_count == -1);
	puts("PASS real sc_enum_apps/EF.DIR decoder handles absolute abbreviated2F00 SELECT through production guard before native module binding, and frees enumerated application metadata on fake disconnect");
	for (int variant = 0; variant < 5; variant++) {
		setup(); if (variant == 0) test_reader.atr.value[1] ^= 1;
		if (variant == 1) test_reader.active_protocol = SC_PROTO_T0;
		if (variant == 2) test_reader.max_recv_size = 256;
		if (variant == 3) test_ctx.debug = 1;
		assert(public_run(variant == 4 ? "2" : "1",output) && !test_physical && !credential_attempts && !physical && !connected && !pub_guard.ctx);
		test_ctx.debug = 0;
	}
	puts("PASS wrong ATR/protocol/receive ceiling/debug and nonexistent reader stop before APDU and release context; another reader is never connected");
	for (int fault = 1; fault <= 6; fault++) {
		setup(); test_failure = fault;
		assert(public_run("1",output) && !credential_attempts && !physical && !resets && !connected && !pub_guard.ctx);
		assert(strstr(output,"incomplete_stop_no_pin") && !strstr(output,"PRIVATE_BACKEND"));
		if (fault <= 4) assert(test_physical == 1 && pub_guard.blocked);
	}
	puts("PASS first SW/transport/oversize/61 response stops before continuation; disconnect/context release failures prevent successful result even if Finalize returns OK; backend stdout is discarded");
	setup(); pub_guard.reader = &test_reader; pub_guard.original = &reader_ops; pub_guard.connected = 1;
	for (int variant = 0; variant < 14; variant++) {
		memset(&pub_guard,0,sizeof(pub_guard)); pub_guard.reader = &test_reader; pub_guard.original = &reader_ops; pub_guard.connected = 1;
		pub_guard.app = 1; pub_guard.selected = 0x7001; pub_guard.read_budget[3] = 600;
		unsigned char response[256], data[2] = {0x70,1}; sc_apdu_t a = {0}; a.ins = 0xB0; a.le = a.resplen = 256; a.resp = response; a.cse = SC_APDU_CASE_2_SHORT;
		switch (variant) {
		case 0: a.ins = 0x20; break; case 1: a.ins = 0x22; break; case 2: a.ins = 0x86; break; case 3: a.ins = 0x2A; break;
		case 4: a.ins = 0xD6; break; case 5: a.cla = 0x0C; break; case 6: a.cse = SC_APDU_CASE_2_EXT; break;
		case 7: a.p1 = 0x80; break; case 8: a.p1 = 1; a.p2 = 44; break; case 9: pub_guard.read_budget[3] = 255; break;
		case 10: a.control = 1; break; case 11: a.flags = SC_APDU_FLAGS_CHAINING; break;
		case 12: a.ins = 0xA4; a.p1 = 8; a.cse = SC_APDU_CASE_3_SHORT; a.lc = a.datalen = 2; a.data = data; a.le = 0; break;
		case 13: pub_guard.selected = 0xD004; break;
		}
		unsigned int before = test_physical;
		assert(pub_transmit(&test_reader,&a) < 0 && pub_guard.blocked && test_physical == before);
		a.ins = 0xB0; assert(pub_transmit(&test_reader,&a) < 0 && test_physical == before);
	}
	assert(pub_reset(&test_reader,0) < 0 && !resets);
	puts("PASS guard blocks empty VERIFY/MSE/GA/PSO/writes/SM/extended/SFI/offset/budget/control/chaining/absolute app-relative file/D004 before backend; first failure is permanent and reset is forbidden");
	pub_guard.connected = 0; memset(&pub_guard,0,sizeof(pub_guard));
	free_files(); free_native(); scconf_free(test_ctx.conf); test_ctx.conf = NULL;
	EVP_PKEY_free(rsa_key); EVP_PKEY_free(peer.peer); jcop4_ca_pending_clear(&peer.derived);
	OSSL_PROVIDER_unload(test_provider.defprov); OSSL_LIB_CTX_free(test_provider.libctx);
	OSSL_PROVIDER_unload(alternate_provider.defprov); OSSL_LIB_CTX_free(alternate_provider.libctx); return 0;
}
#endif
