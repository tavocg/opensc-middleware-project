/* Real config/native bind/post-bind activation, ISO metadata + software CA. */
#define IASECC_CHIPDOC_ROUTING_FIXTURE
#include "iasecc-chipdoc-routing.c"
static struct { unsigned int fid; unsigned char *data; size_t size; } files[8];
static unsigned int selected_fid, metadata_commands, metadata_reads;
static int missing_pin;
static int metadata_backend(struct sc_reader *reader, struct sc_apdu *a)
{
	struct iasecc_private_data *prv = test_card.drv_data;
	if (prv->chipdoc_transaction.active) return signature_backend(reader,a);
	assert(reader == &test_reader && reader_reserved && a->cla == 0);
	metadata_commands++;
	if (a->ins == 0xA4) {
		if (a->p1 == 4) {
			assert(a->datalen == app.aid.len && !memcmp(a->data,app.aid.value,app.aid.len)); selected_fid = 0x5015;
		} else {
			assert(a->datalen >= 2); selected_fid = a->data[a->datalen-2]*256+a->data[a->datalen-1];
		}
		size_t size = 0;
		for (size_t i = 0; i < sizeof(files)/sizeof(files[0]); i++) if (files[i].fid == selected_fid) size = files[i].size;
		if (missing_pin && selected_fid == 0x7001) { a->resplen = 0; a->sw1 = 0x6A; a->sw2 = 0x82; return 0; }
		unsigned char fcp[] = {0x62,11,0x82,1,1,0x83,2,0,0,0x80,2,0,0};
		fcp[4] = selected_fid == 0x5015 || selected_fid == 0x3F00 ? 0x38 : 1;
		fcp[7] = selected_fid>>8; fcp[8] = selected_fid&255; fcp[11] = size>>8; fcp[12] = size&255;
		if (a->resp) { assert(a->resplen >= sizeof(fcp)); memcpy(a->resp,fcp,sizeof(fcp)); a->resplen = sizeof(fcp); }
		else a->resplen = 0;
	} else {
		assert(a->ins == 0xB0 && a->cse == SC_APDU_CASE_2_SHORT && a->le <= 256 && !a->lc); metadata_reads++;
		const unsigned char *data = NULL; size_t size = 0, offset = a->p1*256+a->p2;
		for (size_t i = 0; i < sizeof(files)/sizeof(files[0]); i++) if (files[i].fid == selected_fid) { data = files[i].data; size = files[i].size; }
		assert(data && offset <= size); size -= offset; if (size > a->resplen) size = a->resplen;
		memcpy(a->resp,data+offset,size); a->resplen = size;
	}
	a->sw1 = 0x90; a->sw2 = 0; return 0;
}
static void config(const char *value)
{
	if (test_ctx.conf) scconf_free(test_ctx.conf);
	memset(test_ctx.conf_blocks,0,sizeof(test_ctx.conf_blocks));
	test_ctx.conf = scconf_new(NULL); assert(test_ctx.conf);
	char text[512]; assert(snprintf(text,sizeof(text),
		"app default { card_driver iasecc { chipdoc_profile = %s; } framework pkcs15 { use_file_caching = yes; use_pin_caching = yes; try_emulation_first = yes; } }",value) > 0);
	assert(scconf_parse_string(test_ctx.conf,text) == 1);
	scconf_block **blocks = scconf_find_blocks(test_ctx.conf,NULL,"app","default"); assert(blocks && blocks[0]);
	test_ctx.conf_blocks[0] = blocks[0]; free(blocks);
}
static void free_files(void)
{ for (size_t i = 0; i < sizeof(files)/sizeof(files[0]); i++) { free(files[i].data); memset(&files[i],0,sizeof(files[i])); } }
static void prepare_files(void)
{
	free_files();
	files[0].fid = 0x5032; tokeninfo.version = 0; tokeninfo.serial_number = "00"; tokeninfo.label = "Synthetic";
	tokeninfo.flags = SC_PKCS15_TOKEN_READONLY; sc_init_oid(&tokeninfo.profile_indication.oid);
	assert(!sc_pkcs15_encode_tokeninfo(&test_ctx,&tokeninfo,&files[0].data,&files[0].size));
	files[1].fid = 0x5031;
	struct sc_pkcs15_df key_df = {0}, pin_df = {0}; key_df.type = SC_PKCS15_PRKDF; pin_df.type = SC_PKCS15_AODF;
	sc_format_path("7002",&key_df.path); sc_format_path("7001",&pin_df.path); key_df.next = &pin_df; p15.df_list = &key_df;
	assert(!sc_pkcs15_encode_odf(&test_ctx,&p15,&files[1].data,&files[1].size)); p15.df_list = NULL;
	files[2].fid = 0x7002; assert(!sc_pkcs15_encode_prkdf_entry(&test_ctx,&native_key,&files[2].data,&files[2].size));
	files[3].fid = 0x7001; assert(!sc_pkcs15_encode_aodf_entry(&test_ctx,&native_pin,&files[3].data,&files[3].size));
	files[4].fid = 0xB001; files[4].size = 500; files[4].data = malloc(500); assert(files[4].data); memset(files[4].data,0x5A,500);
}
static void init_defaults(void)
{
	if (test_ctx.conf) { scconf_free(test_ctx.conf); test_ctx.conf = NULL; }
	assert(!test_prv.chipdoc_routing); frontend_defaults(1);
	reader_ops.transmit = metadata_backend; selected_fid = metadata_commands = metadata_reads = 0; missing_pin = 0;
	test_card.drv_data = NULL; test_card.algorithms = NULL; test_card.algorithm_count = 0;
	test_card.max_send_size = test_card.max_recv_size = test_card.caps = 0;
	test_card.type = SC_CARD_TYPE_IASECC_CHIPDOC;
	sc_format_path("",&app.path); app.path.type = SC_PATH_TYPE_DF_NAME;
	app.path.len = app.aid.len; memcpy(app.path.value,app.aid.value,app.aid.len);
	prepare_files(); config("cr-rsa2048");
}
static int initialize(void)
{ return iasecc_chipdoc_initialize(&test_card,sc_get_iso7816_driver()->ops,iasecc_finish,iasecc_card_reader_lock_obtained); }
static void finish_initialized(void)
{
	assert(!card_ops.finish(&test_card) && !test_card.drv_data);
	free(test_card.algorithms); test_card.algorithms = NULL; test_card.algorithm_count = 0;
	test_card.drv_data = &test_prv; assert(!reader_reserved);
}
#ifndef IASECC_CHIPDOC_INIT_FIXTURE
int main(void)
{
	(void)thread_ops;
	test_provider.libctx = OSSL_LIB_CTX_new(); alternate_provider.libctx = OSSL_LIB_CTX_new(); assert(test_provider.libctx && alternate_provider.libctx);
	test_provider.defprov = OSSL_PROVIDER_load(test_provider.libctx,"default"); alternate_provider.defprov = OSSL_PROVIDER_load(alternate_provider.libctx,"default");
	assert(test_provider.defprov && alternate_provider.defprov); init(&peer); memcpy(initial_saved,peer.initial,137);
	EVP_PKEY_CTX *rsa = EVP_PKEY_CTX_new_from_name(test_provider.libctx,"RSA",NULL); assert(rsa);
	assert(EVP_PKEY_keygen_init(rsa) > 0 && EVP_PKEY_CTX_set_rsa_keygen_bits(rsa,2048) > 0 && EVP_PKEY_keygen(rsa,&rsa_key) > 0); EVP_PKEY_CTX_free(rsa);
	for (size_t i = 0; i < 32; i++) digest[i] = i;
	memset(untouched,0xA5,256); unsigned char output[256];
	for (int variant = 0; variant < 7; variant++) {
		init_defaults(); struct sc_card_operations before = card_ops;
		if (variant == 0) test_card.type = SC_CARD_TYPE_IASECC_AMOS;
		if (variant == 1) test_card.type = 0;
		if (variant == 2) test_card.atr.value[1] ^= 1;
		if (variant == 3) test_ctx.debug = 1;
		if (variant == 4) test_reader.max_recv_size = 256;
		if (variant == 5) test_card.max_recv_size = 256;
		if (variant == 6) test_reader.active_protocol = SC_PROTO_T0;
		assert(initialize() < 0 && !test_card.drv_data && !test_card.algorithms && !metadata_commands && !resets);
		assert(!memcmp(&card_ops,&before,sizeof(before))); test_ctx.debug = 0;
	}
	puts("PASS matched card initialization requires exact ATR/type, quiet supported provider/T1/reader and declared limits; failed initializer sends no IO and leaves operations/private state/algorithms unchanged");
	init_defaults(); assert(iasecc_chipdoc_match(&test_card, test_card.driver) && !initialize());
	assert(iasecc_chipdoc_configured(&test_card) && !metadata_commands && !resets && !physical);
	assert(test_card.max_send_size == 139 && test_card.max_recv_size == 291 && test_card.algorithm_count == 1 && test_card.algorithms[0].flags == rsa_algorithm.flags);
	assert(test_card.algorithms[0].algorithm == SC_ALGORITHM_RSA && test_card.algorithms[0].key_length == 2048 && !test_card.algorithms[0].u._rsa.exponent);
	assert(test_card.caps == (SC_CARD_CAP_APDU_EXT|SC_CARD_CAP_ISO7816_PIN_INFO) && !card_ops.compute_signature && !card_ops.get_challenge && !card_ops.update_binary);
	struct sc_pin_cmd_data pin = {0}; pin.cmd = SC_PIN_CMD_VERIFY; pin.pin_type = SC_AC_CHV; pin.pin_reference = 1; pin.pin1.data = fictitious_pin; pin.pin1.len = 6;
	assert(sc_pin_cmd(&test_card,&pin) < 0 && !metadata_commands && !credential_attempts); finish_initialized();
	puts("PASS initializer installs narrow ISO readonly pre-bind table and measured139/291 operation ceilings, no legacy IAS/EFATR/serial/PIN/init APDU or RNG/generation/raw hashes; PIN/signing blocked before native activation");
	for (int bad = 0; bad < 2; bad++) {
		init_defaults(); if (bad) missing_pin = 1; assert(!initialize());
		struct sc_pkcs15_card *owner = NULL;
		int rv = sc_pkcs15_bind(&test_card,&app.aid,&owner);
		if (bad) {
			assert(rv < 0 && !owner && !((struct iasecc_private_data *)test_card.drv_data)->chipdoc_routing && !credential_attempts && !physical);
			assert(sc_pin_cmd(&test_card,&pin) < 0); finish_initialized(); continue;
		}
		if (rv) fprintf(stderr,"fixture native bind error=%d metadata_commands=%u\n",rv,metadata_commands);
		assert(!rv && owner && !(owner->flags & SC_PKCS15_CARD_FLAG_EMULATED) && !owner->opts.use_pin_cache && owner->opts.use_file_cache == SC_PKCS15_OPTS_CACHE_NO_FILES);
		assert(((struct iasecc_private_data *)test_card.drv_data)->chipdoc_routing && !physical && !credential_attempts && !resets);
		struct sc_pkcs15_object *keys[4], *pins[4]; assert(sc_pkcs15_get_objects(owner,SC_PKCS15_TYPE_PRKEY_RSA,keys,4) == 1);
		assert(sc_pkcs15_get_objects(owner,SC_PKCS15_TYPE_AUTH_PIN,pins,4) == 1);
		/* Real native bind/post-bind hook installed production routing without
		 * calling any installer or driver callback from this fixture. */
		assert(!sc_pkcs15_verify_pin(owner,pins[0],fictitious_pin,6) && physical == 9 && credential_attempts == 1);
		memcpy(output,untouched,256); assert(sc_pkcs15_compute_signature(owner,keys[0],rsa_algorithm.flags,digest,32,output,256,NULL) == 256); verify_rsa(output);
		assert(physical == 12 && credential_attempts == 1 && resets == 2 && !reader_reserved && !pins[0]->content.value);
		assert(!sc_pkcs15_unbind(owner)); finish_initialized();
	}
	puts("PASS real sc_pkcs15_bind reads native ISO TokenInfo/ODF/PrKDF/AODF and post-bind automatically installs production routing only for configured candidate; actual native login/RSA verified, emulation-first ignored for this profile, invalid metadata stays pre-bind blocked");
	init_defaults(); assert(!initialize()); sc_path_t path; sc_format_path("B001",&path);
	assert(!sc_select_file(&test_card,&path,NULL)); unsigned int before = metadata_reads; unsigned char bytes[500];
	assert(sc_read_binary(&test_card,0,bytes,500,NULL) == 500 && metadata_reads-before == 2);
	for (size_t i = 0; i < 500; i++) assert(bytes[i] == 0x5A);
	assert(!credential_attempts && !physical); finish_initialized();
	puts("PASS public metadata READ500 uses short256+244 via real ISO serializer/core loop despite protected PSO291 ceiling, without truncation or enabling extended public READ");
	free_files(); free_native(); scconf_free(test_ctx.conf); test_ctx.conf = NULL; memset(test_ctx.conf_blocks,0,sizeof(test_ctx.conf_blocks));
	EVP_PKEY_free(rsa_key); EVP_PKEY_free(peer.peer); jcop4_ca_pending_clear(&peer.derived);
	OSSL_PROVIDER_unload(test_provider.defprov); OSSL_LIB_CTX_free(test_provider.libctx);
	OSSL_PROVIDER_unload(alternate_provider.defprov); OSSL_LIB_CTX_free(alternate_provider.libctx); return 0;
}
#endif
