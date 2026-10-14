/* Native metadata -> bootstrap -> confirmed CA/MF with a software reader. */
#define IASECC_CHIPDOC_CHANNEL_FIXTURE
#include "iasecc-chipdoc-channel.c"
static struct sc_pkcs15_card p15;
static struct sc_pkcs15_object key_object, pin_object, duplicate;
static struct sc_pkcs15_prkey_info key_info;
static struct sc_pkcs15_auth_info pin_info;
static struct sc_app_info app;
static struct sc_card_driver driver = {.name="Software IAS", .short_name="iasecc"};
static unsigned int plain_calls;
static int failure_step, failure, control_variant, read_variant;
static unsigned int plain_sw;

static int bootstrap_backend(struct sc_reader *reader, struct sc_apdu *a)
{
	if (plain_calls == 3) return backend(reader,a);
	unsigned char *wire = NULL; size_t size = 0;
	const unsigned long flags = SC_APDU_FLAGS_NO_SM | SC_APDU_FLAGS_NO_RETRY_WL | SC_APDU_FLAGS_NO_GET_RESP;
	assert(reader == &test_reader && test_card.lock_count >= 2 && (a->flags & flags) == flags);
	assert(!a->control && !(a->flags & (SC_APDU_FLAGS_CHAINING | SC_APDU_FLAGS_SM_CHAINING)));
	assert(!sc_apdu_get_octets(&test_ctx,a,&wire,&size,SC_PROTO_T1));
	assert(a->cla == 0); physical++; plain_calls++;
	if (plain_calls == 1) {
		assert(a->cse == SC_APDU_CASE_3_SHORT && a->ins == 0xA4 && a->p1 == 4 && a->p2 == 0x0C);
		assert(a->lc == app.aid.len && !a->le && size == app.aid.len+5 && !memcmp(wire+5,app.aid.value,app.aid.len));
		a->resplen = 0;
	} else if (plain_calls == 2) {
		assert(a->cse == SC_APDU_CASE_4_SHORT && size == 8 && !memcmp(wire,"\0\xa4\x02\0\x02\xd0\x04\0",8));
		assert(a->le == 256 && a->resplen == 256);
		const unsigned char control[] = {0x62,11,0x82,1,1,0x83,2,0xD0,4,0x80,2,0,137};
		memcpy(a->resp,control,sizeof(control)); a->resplen = sizeof(control);
		if (control_variant == 1) a->resp[8] = 5;
		if (control_variant == 2) a->resp[4] = 0x38;
		if (control_variant == 3) a->resp[12] = 138;
		if (control_variant == 4) { a->resp[1] -= 4; a->resplen -= 4; }
		if (control_variant == 5) { memcpy(a->resp+13,a->resp+5,4); a->resp[1] += 4; a->resplen += 4; }
		if (control_variant == 6) a->resplen = 257;
		if (control_variant == 7) a->resp[0] = 0x30;
		if (control_variant == 8) a->resp[a->resplen++] = 0;
		if (control_variant == 9) { a->resp[1] = 0x81; a->resp[2] = 0xFF; }
	} else {
		assert(a->cse == SC_APDU_CASE_2_SHORT && size == 5 && !memcmp(wire,"\0\xb0\0\0\x89",5));
		assert(a->le == 137 && a->resplen == 137);
		memcpy(a->resp,peer.initial,137); a->resplen = 137;
		if (read_variant == 1) a->resplen = 136;
		if (read_variant == 2) a->resplen = 138;
		if (read_variant == 3) a->resp[2] = 17;
		if (read_variant == 4) a->resp[5] = 2;
		if (read_variant == 5) a->resplen = 0;
		if (read_variant == 6) a->resp[2] = 0x18; /* decimal24 must not select P521. */
	}
	free(wire); a->sw1 = 0x90; a->sw2 = 0;
	if ((int)plain_calls == failure_step) {
		if (failure == 1) return -7;
		if (failure == 2) { a->sw1 = plain_sw >> 8; a->sw2 = plain_sw & 255; }
		if (failure == 3) { a->resp[0] = 0xAA; a->resplen = 1; }
	}
	change_provider_after_io();
	if ((int)physical == reset_on_command) iasecc_card_reader_lock_obtained(&test_card,1);
	return SC_SUCCESS;
}

static void bootstrap_defaults(void)
{
	defaults(); iasecc_chipdoc_invalidate_card(&test_card); test_prv.chipdoc_profile = IASECC_CHIPDOC_NONE;
	test_card.driver = &driver; card_ops.check_sw = sc_get_iso7816_driver()->ops->check_sw;
	reader_ops.transmit = bootstrap_backend;
	memset(&p15,0,sizeof(p15)); memset(&key_object,0,sizeof(key_object)); memset(&pin_object,0,sizeof(pin_object));
	memset(&duplicate,0,sizeof(duplicate)); memset(&key_info,0,sizeof(key_info)); memset(&pin_info,0,sizeof(pin_info));
	memset(&app,0,sizeof(app));
	const struct iasecc_chipdoc_profile *profile = iasecc_chipdoc_profile(IASECC_CHIPDOC_CR_RSA2048);
	app.aid.len = profile->aid_len; memcpy(app.aid.value,profile->aid,profile->aid_len); app.ddo.aid = app.aid;
	test_card.app_count = 1; test_card.app[0] = &app;
	p15.card = &test_card; p15.app = &app; p15.obj_list = &key_object;
	key_object.type = SC_PKCS15_TYPE_PRKEY_RSA; key_object.flags = SC_PKCS15_CO_FLAG_PRIVATE;
	key_object.data = &key_info; key_object.auth_id.len = 1; key_object.auth_id.value[0] = 0x42;
	key_object.next = &pin_object;
	key_info.native = 1; key_info.modulus_length = 2048; key_info.key_reference = 0x91;
	key_info.usage = SC_PKCS15_PRKEY_USAGE_NONREPUDIATION;
	key_info.id.len = 1; key_info.id.value[0] = 1; sc_format_path("3F00",&key_info.path);
	pin_object.type = SC_PKCS15_TYPE_AUTH_PIN; pin_object.data = &pin_info;
	pin_info.auth_id = key_object.auth_id; pin_info.attrs.pin.reference = 1;
	pin_info.attrs.pin.flags = SC_PKCS15_PIN_FLAG_INITIALIZED; pin_info.attrs.pin.type = SC_PKCS15_PIN_TYPE_UTF8;
	pin_info.attrs.pin.min_length = 4; pin_info.attrs.pin.stored_length = pin_info.attrs.pin.max_length = 12;
	plain_calls = 0; failure_step = failure = control_variant = read_variant = 0; plain_sw = 0x6982;
}

static void real_pkcs15_decode(void)
{
	bootstrap_defaults();
	struct sc_pkcs15_object decoded_key = {0}, decoded_pin = {0};
	unsigned char *der; const unsigned char *cursor; size_t size, left;
	assert(!sc_pkcs15_encode_prkdf_entry(&test_ctx,&key_object,&der,&size)); cursor = der; left = size;
	assert(!sc_pkcs15_decode_prkdf_entry(&p15,&decoded_key,&cursor,&left) && !left); free(der);
	assert(!sc_pkcs15_encode_aodf_entry(&test_ctx,&pin_object,&der,&size)); cursor = der; left = size;
	assert(!sc_pkcs15_decode_aodf_entry(&p15,&decoded_pin,&cursor,&left) && !left); free(der);
	decoded_key.next = &decoded_pin; p15.obj_list = &decoded_key;
	struct sc_aid aid; struct iasecc_chipdoc_key_metadata metadata;
	assert(!iasecc_chipdoc_pkcs15_key(&p15,&decoded_key,&aid,&metadata));
	assert(metadata.reference == 0x91 && metadata.bits == 2048 && metadata.pin_reference == 1);
	assert(aid.len == app.aid.len && !memcmp(aid.value,app.aid.value,aid.len));
	struct iasecc_chipdoc_bootstrap_report report;
	int rv = iasecc_chipdoc_channel_bootstrap(&p15,&decoded_key,&report);
	if (rv) fprintf(stderr,"fixture bootstrap result=%d stage=%d commands=%u transport=%d\n",rv,report.stage,report.channel.commands,report.channel.transport);
	assert(!rv);
	assert(report.stage == IASECC_CHIPDOC_BOOTSTRAP_CHANNEL && report.channel.commands == 6 && report.channel.authenticated);
	assert(report.channel.sw == 0x9000 && report.channel.reset_required && physical == 6 && locks == unlocks);
	assert(test_prv.chipdoc_profile == IASECC_CHIPDOC_CR_RSA2048 && test_prv.chipdoc_session.phase == IASECC_CHIPDOC_CHANNEL);
	struct chipdoc_crypto *crypto = test_prv.chipdoc_session.channel; assert(crypto && crypto->sm.keys.ssc[15] == 2);
	struct iasecc_chipdoc_channel_report mf;
	assert(!iasecc_chipdoc_channel_select_mf(&test_card,IASECC_CHIPDOC_MF_SHORT,&mf) && physical == 7);
	assert(crypto->sm.keys.ssc[15] == 4 && test_prv.chipdoc_session.phase == IASECC_CHIPDOC_MF);
	iasecc_chipdoc_invalidate_card(&test_card);
	/* Fixture has no subject/params/content allocations beyond decoded info. */
	struct sc_pkcs15_prkey_info *decoded_info = decoded_key.data;
	assert(!decoded_info->subject.value && !decoded_info->params.data && !decoded_key.content.value && !decoded_pin.content.value);
	free(decoded_key.data); free(decoded_pin.data);
	puts("PASS real native PKCS15 DER key/PIN decoding drives exact bootstrap then real dispatcher CA/MF, six/seven APDU, SSC2/4, no PIN or discovery");
}

#ifndef IASECC_CHIPDOC_BOOTSTRAP_FIXTURE
int main(void)
{
	test_provider.libctx = OSSL_LIB_CTX_new(); assert(test_provider.libctx);
	test_provider.defprov = OSSL_PROVIDER_load(test_provider.libctx,"default"); assert(test_provider.defprov);
	alternate_provider.libctx = OSSL_LIB_CTX_new(); assert(alternate_provider.libctx);
	alternate_provider.defprov = OSSL_PROVIDER_load(alternate_provider.libctx,"default"); assert(alternate_provider.defprov);
	init(&peer); memcpy(initial_saved,peer.initial,sizeof(initial_saved)); real_pkcs15_decode();
	struct iasecc_chipdoc_bootstrap_report report;
	for (int variant = 0; variant < 32; variant++) {
		bootstrap_defaults();
		switch (variant) {
		case 0: key_info.native = 0; break;
		case 1: key_info.modulus_length = 3072; break;
		case 2: key_info.key_reference = 0x90; break;
		case 3: key_info.usage = SC_PKCS15_PRKEY_USAGE_DECRYPT; break;
		case 4: key_object.type = SC_PKCS15_TYPE_PRKEY_EC; break;
		case 5: pin_info.attrs.pin.reference = 2; break;
		case 6: pin_info.attrs.pin.flags |= SC_PKCS15_PIN_FLAG_LOCAL; break;
		case 7: pin_info.attrs.pin.flags |= SC_PKCS15_PIN_FLAG_SO_PIN; break;
		case 8: pin_info.attrs.pin.flags |= SC_PKCS15_PIN_FLAG_UNBLOCKING_PIN; break;
		case 9: pin_info.auth_id.value[0] ^= 1; break;
		case 10: key_object.auth_id.len = 0; break;
		case 11: key_object.auth_id.len = sizeof(key_object.auth_id.value)+1; break;
		case 12: key_object.session_object = 1; break;
		case 13: key_object.emulated = &key_info; break;
		case 14: p15.flags = SC_PKCS15_CARD_FLAG_EMULATED; break;
		case 15: pin_info.auth_type = SC_PKCS15_PIN_AUTH_TYPE_AUTH_KEY; break;
		case 16: key_object.next = NULL; break;
		case 17: p15.obj_list = &pin_object; break;
		case 18: duplicate = pin_object; pin_object.next = &duplicate; break;
		case 19: app.aid.value[5] ^= 1; break;
		case 20: test_card.app_count = 0; break;
		case 21: test_card.app_count = SC_MAX_CARD_APPS+1; break;
		case 22: test_card.app_count = 2; test_card.app[1] = &app; break;
		case 23: duplicate.type = SC_PKCS15_TYPE_CERT_X509; duplicate.next = &duplicate; pin_object.next = &duplicate; break;
		case 24: test_card.lock_count = 0; break;
		case 25: test_card.caps = 0; break;
		case 26: test_ctx.debug = 1; break;
		case 27: test_ctx.ossl3ctx = NULL; break;
		case 28: test_card.driver = NULL; break;
		case 29: test_prv.chipdoc_session.generation = UINT64_MAX; break;
		case 30: key_info.path.aid = app.aid; key_info.path.aid.value[0] ^= 1; break;
		case 31: app.ddo.aid.value[0] ^= 1; break;
		}
		struct sc_aid aid; struct iasecc_chipdoc_key_metadata metadata;
		if (variant < 24 || variant >= 30) {
			memset(&aid,0xA5,sizeof(aid)); memset(&metadata,0xA5,sizeof(metadata));
			assert(iasecc_chipdoc_pkcs15_key(&p15,&key_object,&aid,&metadata) != 0);
			assert(!memcmp(&aid,&(struct sc_aid){0},sizeof(aid)) && !memcmp(&metadata,&(struct iasecc_chipdoc_key_metadata){0},sizeof(metadata)));
		}
		assert(iasecc_chipdoc_channel_bootstrap(&p15,&key_object,&report) != 0 && !physical && !report.channel.authenticated);
		assert(test_prv.chipdoc_profile == IASECC_CHIPDOC_NONE && !test_prv.chipdoc_session.channel);
	}
	puts("PASS native RSA2048 key ownership/auth association/unique global PIN/AID inventory and bounded lists reject thirty-two unsupported inputs before I/O, including RSA3072");
	for (int step = 1; step <= 3; step++) {
		bootstrap_defaults(); failure_step = step; failure = 1;
		assert(iasecc_chipdoc_channel_bootstrap(&p15,&key_object,&report) != 0 && physical == (unsigned)step && report.channel.reset_required);
		const unsigned int statuses[] = {0x6982,0x6A82,0x6100,0x6C00};
		for (unsigned int i = 0; i < 4; i++) {
			bootstrap_defaults(); failure_step = step; failure = 2; plain_sw = statuses[i];
			assert(iasecc_chipdoc_channel_bootstrap(&p15,&key_object,&report) != 0 && physical == (unsigned)step);
			assert(report.channel.reset_required && !report.channel.authenticated && !report.channel.sw && !test_prv.chipdoc_session.channel);
		}
	}
	bootstrap_defaults(); failure_step = 1; failure = 3;
	assert(iasecc_chipdoc_channel_bootstrap(&p15,&key_object,&report) != 0 && physical == 1);
	for (int variant = 1; variant <= 9; variant++) {
		bootstrap_defaults(); control_variant = variant;
		assert(iasecc_chipdoc_channel_bootstrap(&p15,&key_object,&report) != 0 && physical == 2);
		assert(report.stage == IASECC_CHIPDOC_BOOTSTRAP_CONTROL && report.channel.reset_required && test_prv.chipdoc_profile == IASECC_CHIPDOC_NONE);
	}
	for (int variant = 1; variant <= 6; variant++) {
		bootstrap_defaults(); read_variant = variant;
		assert(iasecc_chipdoc_channel_bootstrap(&p15,&key_object,&report) != 0 && physical == 3 && test_prv.chipdoc_profile == IASECC_CHIPDOC_NONE);
	}
	puts("PASS plain transport/SW/61xx/6Cxx/payload and D004 control FID/type/size/duplicates/truncation/overflow/invalid P521 stop before bind without continuation or retry");
	for (int step = 1; step <= 6; step++) for (int swap = 0; swap < 2; swap++) {
		bootstrap_defaults(); if (swap) swap_provider_on_command = step; else reset_on_command = step;
		assert(iasecc_chipdoc_channel_bootstrap(&p15,&key_object,&report) != 0 && physical == (unsigned)step);
		assert(report.channel.reset_required && !report.channel.authenticated && !report.channel.sw && !test_prv.chipdoc_session.channel);
		assert(test_prv.chipdoc_session.phase == IASECC_CHIPDOC_INACTIVE && locks == unlocks && test_card.lock_count == 1);
		assert(test_prv.chipdoc_profile == (step <= 3 ? IASECC_CHIPDOC_NONE : IASECC_CHIPDOC_CR_RSA2048));
	}
	for (int mode = TRANSPORT_ERROR; mode <= ZERO_DATA_IV; mode++) {
		bootstrap_defaults(); peer.mode = mode; peer.fail_step = 2; peer.bad_sw = 0x6982;
		assert(iasecc_chipdoc_channel_bootstrap(&p15,&key_object,&report) != 0 && physical >= 4);
		assert(report.channel.reset_required && !test_prv.chipdoc_session.channel && test_prv.chipdoc_profile == IASECC_CHIPDOC_CR_RSA2048);
	}
	puts("PASS reset/context replacement during all six exchanges invalidates bootstrap; CA/MAC failures preserve recognized binding but never attach a channel, caller must reset remote state");
	bootstrap_defaults(); p15.opts.use_pin_cache = 1;
	pin_info.logged_in = SC_PIN_STATE_LOGGED_IN; pin_info.tries_left = 0; pin_info.max_tries = 99;
	key_object.content.value = pin_object.content.value = (unsigned char *)1;
	key_object.content.len = pin_object.content.len = SIZE_MAX;
	struct sc_aid aid; struct iasecc_chipdoc_key_metadata metadata;
	assert(!iasecc_chipdoc_pkcs15_key(&p15,&key_object,&aid,&metadata));
	assert(!iasecc_chipdoc_channel_bootstrap(&p15,&key_object,&report) && physical == 6);
	void *owned = test_prv.chipdoc_session.channel;
	assert(iasecc_chipdoc_channel_bootstrap(&p15,&key_object,&report) != 0 && physical == 6 && test_prv.chipdoc_session.channel == owned && report.channel.reset_required);
	assert(iasecc_chipdoc_channel_bootstrap(&p15,&key_object,NULL) != 0 && physical == 6 && test_prv.chipdoc_session.channel == owned);
	test_ctx.debug = 1;
	assert(iasecc_chipdoc_channel_bootstrap(&p15,&key_object,&report) != 0 && physical == 6 && test_prv.chipdoc_session.channel == owned && report.channel.reset_required);
	test_ctx.debug = 0;
	assert(iasecc_chipdoc_channel_bootstrap(NULL,NULL,&report) != 0);
	assert(iasecc_chipdoc_pkcs15_key(NULL,NULL,&aid,&metadata) != 0);
	assert(iasecc_chipdoc_pkcs15_key(&p15,&key_object,NULL,&metadata) != 0);
	iasecc_chipdoc_invalidate_card(&test_card);
	bootstrap_defaults(); test_prv.chipdoc_profile = IASECC_CHIPDOC_CR_RSA2048; control_variant = 1;
	assert(iasecc_chipdoc_channel_bootstrap(&p15,&key_object,&report) != 0 && physical == 2 && test_prv.chipdoc_profile == IASECC_CHIPDOC_CR_RSA2048);
	assert(!test_prv.chipdoc_session.channel && report.channel.reset_required);
	puts("PASS PIN cache/content/tries/logged-in metadata are never trusted; duplicate/null bootstrap preserves a live owner; no public binding or remote reset/unlock activated");
	EVP_PKEY_free(peer.peer); jcop4_ca_pending_clear(&peer.derived);
	OSSL_PROVIDER_unload(test_provider.defprov); OSSL_LIB_CTX_free(test_provider.libctx);
	OSSL_PROVIDER_unload(alternate_provider.defprov); OSSL_LIB_CTX_free(alternate_provider.libctx);
	return 0;
}

#endif
