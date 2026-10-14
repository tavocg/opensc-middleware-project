/* Installed per-card production routing, actual native frontend, fake reader. */
#define IASECC_CHIPDOC_PKCS15_FIXTURE
#include "iasecc-chipdoc-pkcs15.c"
#include "libopensc/iasecc-chipdoc-routing.h"
static unsigned int metadata_reads, metadata_selects, owner_clears;
static struct sc_card_operations original_operations;
static int idle_read(struct sc_card *card, unsigned int offset, unsigned char *out, size_t size, unsigned long *flags)
{ (void)flags; assert(card == &test_card && !offset && size == 1); metadata_reads++; out[0] = 0xAA; return 1; }
static int idle_select(struct sc_card *card, const struct sc_path *path, struct sc_file **file)
{ assert(card == &test_card && path && !file); metadata_selects++; return 0; }
static void owner_clear(struct sc_pkcs15_card *owner)
{
	struct iasecc_chipdoc_routing *b = chipdoc_route(owner->card);
	assert(b && b->detached && !b->p15 && !b->key && !b->pin); owner_clears++;
}
static void routing_defaults(int aid)
{
	assert(!test_prv.chipdoc_routing);
	frontend_defaults(aid); metadata_reads = metadata_selects = owner_clears = 0;
	card_ops.read_binary = idle_read; card_ops.select_file = idle_select;
	card_ops.logout = iasecc_logout; card_ops.finish = iasecc_finish;
	original_operations = card_ops; p15.ops.clear = owner_clear;
}
static void install(void)
{
	assert(!iasecc_chipdoc_routing_install(&p15,&native_key));
	assert(test_prv.chipdoc_routing && !p15.opts.use_pin_cache && p15.ops.clear == chipdoc_route_clear);
	assert(!physical && !reader_locks && !resets && test_card.caps == (SC_CARD_CAP_APDU_EXT));
}
static void status(int expected)
{
	struct sc_pin_cmd_data data = {0}; data.cmd = SC_PIN_CMD_GET_INFO; data.flags = SC_PIN_CMD_STATUS_ONLY;
	data.pin_type = SC_AC_CHV; data.pin_reference = 1; unsigned int before = physical;
	assert(!sc_pin_cmd(&test_card,&data) && data.pin1.logged_in == expected && data.pin1.tries_left == -1 && !data.pin1.max_tries);
	assert(data.pin1.min_length == 6 && data.pin1.max_length == 14 && physical == before);
}
static void destroy_routing(void)
{
	struct iasecc_private_data *heap = calloc(1,sizeof(*heap)); assert(heap);
	int failed_close = test_prv.chipdoc_recovery_required;
	*heap = test_prv; test_card.drv_data = heap; test_prv.chipdoc_routing = NULL;
	int rv = card_ops.finish(&test_card);
	assert(!test_card.drv_data && !memcmp(&card_ops,&original_operations,sizeof(card_ops)));
	assert(failed_close ? rv < 0 : rv == 0);
	if (reader_reserved) assert(!sc_unlock(&test_card)); /* Simulated teardown of a lost reader lock. */
	assert(!reader_reserved); test_card.drv_data = &test_prv; memset(&test_prv,0,sizeof(test_prv));
}
#ifndef IASECC_CHIPDOC_ROUTING_FIXTURE
int main(void)
{
	(void)thread_ops;
	test_provider.libctx = OSSL_LIB_CTX_new(); alternate_provider.libctx = OSSL_LIB_CTX_new(); assert(test_provider.libctx && alternate_provider.libctx);
	test_provider.defprov = OSSL_PROVIDER_load(test_provider.libctx,"default"); alternate_provider.defprov = OSSL_PROVIDER_load(alternate_provider.libctx,"default");
	assert(test_provider.defprov && alternate_provider.defprov); init(&peer); memcpy(initial_saved,peer.initial,137);
	EVP_PKEY_CTX *rsa = EVP_PKEY_CTX_new_from_name(test_provider.libctx,"RSA",NULL); assert(rsa);
	assert(EVP_PKEY_keygen_init(rsa) > 0 && EVP_PKEY_CTX_set_rsa_keygen_bits(rsa,2048) > 0 && EVP_PKEY_keygen(rsa,&rsa_key) > 0); EVP_PKEY_CTX_free(rsa);
	for (size_t i = 0; i < 32; i++) digest[i] = i;
	memset(untouched,0xA5,256); unsigned char output[256], byte = 0; unsigned long flags = 0;
	struct iasecc_chipdoc_routing_report report;
	for (int aid = 0; aid < 2; aid++) {
		routing_defaults(aid); struct sc_card_operations other = card_ops, saved_other = other; install(); status(SC_PIN_STATE_LOGGED_OUT);
		assert(!memcmp(&other,&saved_other,sizeof(other)) && !driver.ops); /* No shared vector mutated. */
		assert(sc_read_binary(&test_card,0,&byte,1,&flags) == 1 && byte == 0xAA && metadata_reads == 1);
		sc_path_t path; sc_format_path("3F00",&path); assert(!sc_select_file(&test_card,&path,NULL) && metadata_selects == 1);
		assert(!sc_pkcs15_verify_pin(&p15,&native_pin,fictitious_pin,6) && credential_attempts == 1 && physical == 9);
		status(SC_PIN_STATE_LOGGED_IN); unsigned int before = physical;
		assert(sc_read_binary(&test_card,0,&byte,1,&flags) < 0 && metadata_reads == 1 && physical == before);
		assert(sc_select_file(&test_card,&path,NULL) < 0 && metadata_selects == 1 && physical == before);
		assert(!card_ops.get_challenge && !card_ops.get_response && !card_ops.decipher && !card_ops.card_ctl && !card_ops.update_binary && !card_ops.create_file);
		struct sc_pin_cmd_data wrong = {0}; wrong.cmd = SC_PIN_CMD_CHANGE; wrong.pin_type = SC_AC_CHV; wrong.pin_reference = 1;
		assert(sc_pin_cmd(&test_card,&wrong) < 0 && physical == before);
		memcpy(output,untouched,256);
		assert(sc_pkcs15_compute_signature(&p15,&native_key,rsa_algorithm.flags,digest,32,output,256,NULL) == 256); verify_rsa(output);
		assert(physical == 12 && signing_calls == 3 && !reader_reserved && !test_card.lock_count && !native_pin.content.value);
		assert(!frontend_pins && !frontend_selects && !frontend_mses && !frontend_psos); /* No fixture routing callbacks used. */
		assert(!iasecc_chipdoc_routing_last_report(&test_card,&report) && report.operation == IASECC_CHIPDOC_ROUTE_SIGN && report.result == 256 && report.close.complete);
		status(SC_PIN_STATE_LOGGED_OUT); assert(!card_ops.logout(&test_card) && resets == 2);
		memcpy(output,untouched,256);
		assert(sc_pkcs15_compute_signature(&p15,&native_key,rsa_algorithm.flags,digest,32,output,256,NULL) == SC_ERROR_SECURITY_STATUS_NOT_SATISFIED);
		assert(physical == 12 && credential_attempts == 1 && !memcmp(output,untouched,256));
		/* New caller-supplied PIN/new software peer, same installed card binding.
		 * Reset fixture counters only; do not reset production session/binding. */
		EVP_PKEY_free(peer.peer); jcop4_ca_pending_clear(&peer.derived); init(&peer); peer.le = JCOP4_CA_LE_SHORT_MAX;
		memcpy(initial_saved,peer.initial,137);
		physical = plain_calls = credential_attempts = signing_calls = reader_locks = reader_unlocks = resets = 0;
		assert(!sc_pkcs15_verify_pin(&p15,&native_pin,fictitious_pin,6));
		assert(sc_pkcs15_compute_signature(&p15,&native_key,rsa_algorithm.flags,digest,32,output,256,NULL) == 256); verify_rsa(output);
		assert(physical == 12 && credential_attempts == 1 && resets == 2 && !reader_reserved && !native_pin.content.value);
		assert(!iasecc_chipdoc_routing_remove(&test_card,&p15,&report) && report.close.complete && !test_prv.chipdoc_routing->p15);
		assert(p15.ops.clear == owner_clear && !p15.opts.use_pin_cache);
		assert(sc_read_binary(&test_card,0,&byte,1,&flags) < 0 && physical == 12 && metadata_reads == 1);
		assert(iasecc_chipdoc_routing_install(&p15,&native_key) < 0 && !iasecc_chipdoc_routing_remove(&test_card,NULL,&report));
		destroy_routing();
	}
	puts("PASS actual installed per-card callbacks drive native PKCS15 login/SHA256 RSA signing, one protected AID/MSE/PSO and no fixture routing; local GET_INFO follows live proof without counters/APDU, idle metadata only, detached tombstone blocks fallback");
	for (int variant = 0; variant < 10; variant++) {
		routing_defaults(1); struct sc_card_operations before = card_ops;
		if (variant == 0) test_ctx.debug = 1;
		if (variant == 1) test_card.caps = 0;
		if (variant == 2) test_card.max_recv_size = 256;
		if (variant == 3) driver.ops = &card_ops;
		if (variant == 4) p15.magic = 0;
		if (variant == 5) ((struct sc_pkcs15_auth_info *)native_pin.data)->auth_method = SC_AC_AUT;
		if (variant == 6) native_pin.content.value = (unsigned char *)1;
		if (variant == 7) { card_ops.finish = NULL; before = card_ops; }
		if (variant == 8) test_card.caps |= SC_CARD_CAP_RNG;
		if (variant == 9) rsa_algorithm.flags |= SC_ALGORITHM_RSA_RAW;
		assert(iasecc_chipdoc_routing_install(&p15,&native_key) < 0 && !test_prv.chipdoc_routing && p15.opts.use_pin_cache && p15.ops.clear == owner_clear);
		assert(!memcmp(&card_ops,&before,sizeof(before)) && !physical && !reader_locks);
		test_ctx.debug = 0; driver.ops = NULL; native_pin.content.value = NULL;
	}
	routing_defaults(1); install(); struct sc_pkcs15_card other_owner = p15;
	assert(iasecc_chipdoc_routing_install(&other_owner,&native_key) < 0 && iasecc_chipdoc_routing_remove(&test_card,&other_owner,&report) < 0);
	assert(test_prv.chipdoc_routing->p15 == &p15 && !physical); destroy_routing();
	puts("PASS unsupported platform/limits/shared operations table/owner/method/cache install failures do not mutate table/options/lifetime or send IO; only one explicit owner, no automatic activation");
	for (int step = 1; step <= 3; step++) {
		routing_defaults(1); install(); assert(!sc_pkcs15_verify_pin(&p15,&native_pin,fictitious_pin,6));
		failure_stage = step; failure_mode = 3; memcpy(output,untouched,256);
		assert(sc_pkcs15_compute_signature(&p15,&native_key,rsa_algorithm.flags,digest,32,output,256,NULL) == SC_ERROR_SECURITY_STATUS_NOT_SATISFIED);
		assert(credential_attempts == 1 && physical == (unsigned int)(9+step) && !reader_reserved && !test_card.lock_count && !native_pin.content.value);
		assert(!memcmp(output,untouched,256) && !frontend_pins && !iasecc_chipdoc_routing_last_report(&test_card,&report) && report.close.complete);
		destroy_routing();
	}
	routing_defaults(1); install(); verify_sw = 0x63C6;
	assert(sc_pkcs15_verify_pin(&p15,&native_pin,fictitious_pin,6) == SC_ERROR_PIN_CODE_INCORRECT);
	assert(credential_attempts == 1 && physical == 9 && !reader_reserved && !native_pin.content.value);
	assert(!iasecc_chipdoc_routing_last_report(&test_card,&report) && report.channel.authenticated && report.channel.sw == 0x63C6 && report.close.complete); destroy_routing();
	puts("PASS installed routing closes every native PIN/select/MSE/PSO failure without cached revalidation or partial output; numeric report retains authenticated error and managed cleanup");
	for (int fault = 0; fault < 3; fault++) {
		routing_defaults(1); install(); assert(!sc_pkcs15_verify_pin(&p15,&native_pin,fictitious_pin,6));
		if (fault == 0) reset_fail_at = 2;
		if (fault == 1) unlock_fail = 1;
		if (fault == 2) assert(!iasecc_card_reader_lock_obtained(&test_card,0));
		assert(iasecc_chipdoc_routing_remove(&test_card,&p15,&report) < 0 && report.close.reset_required && !report.close.complete);
		assert(!test_prv.chipdoc_routing->p15 && !test_prv.chipdoc_routing->key && !test_prv.chipdoc_session.channel);
		assert(sc_read_binary(&test_card,0,&byte,1,&flags) < 0 && physical == 9 && !metadata_reads);
		assert(iasecc_chipdoc_routing_remove(&test_card,NULL,&report) < 0); destroy_routing();
	}
	puts("PASS reset/unlock/lost-transaction detach failures wipe secrets and detach objects while retaining quarantine/filter/report; no reset/unlock of reacquired owner, no plaintext fallback or second credential");
	for (int mutation = 0; mutation < 4; mutation++) {
		routing_defaults(1); install(); assert(!sc_pkcs15_verify_pin(&p15,&native_pin,fictitious_pin,6));
		if (mutation == 0) p15.opts.use_pin_cache = 1;
		if (mutation == 1) ((struct sc_pkcs15_auth_info *)native_pin.data)->attrs.pin.reference = 2;
		if (mutation == 2) p15.obj_list = &native_pin; /* Removed key: reject before dereferencing its borrowed pointer. */
		if (mutation == 3) test_ctx.ossl3ctx = &alternate_provider;
		struct sc_pin_cmd_data data = {0}; data.cmd = SC_PIN_CMD_GET_INFO; data.pin_type = SC_AC_CHV; data.pin_reference = 1;
		assert(sc_pin_cmd(&test_card,&data) < 0 && data.pin1.logged_in == SC_PIN_STATE_UNKNOWN && physical == 9 && !reader_reserved);
		assert(!test_prv.chipdoc_session.channel && credential_attempts == 1); destroy_routing();
	}
	puts("PASS changed cache policy/metadata/object membership/provider invalidate live proof and close without publishing cached login or sending another APDU");
	for (int failure = 0; failure < 2; failure++) {
		routing_defaults(1);
		struct sc_pkcs15_card *owner = sc_pkcs15_card_new(); assert(owner);
		owner->card = &test_card; owner->app = malloc(sizeof(app)); assert(owner->app);
		assert(!app.label && !app.ddo.value); *owner->app = app; /* Synthetic app owns no dynamic fields. */
		struct sc_pkcs15_object *key = malloc(sizeof(*key)), *pin = malloc(sizeof(*pin)); assert(key && pin);
		*key = native_key; *pin = native_pin; key->next = pin; pin->next = NULL; owner->obj_list = key;
		owner->opts.use_pin_cache = 1; owner->ops.clear = owner_clear; decoded = 0; /* Ownership moved to actual framework free. */
		assert(!iasecc_chipdoc_routing_install(owner,key));
		assert(!sc_pkcs15_verify_pin(owner,pin,fictitious_pin,6));
		if (failure) reset_fail_at = 2;
		sc_pkcs15_card_free(owner); /* Real lifecycle calls our clear BEFORE freeing native objects. */
		assert(owner_clears == 1 && !test_prv.chipdoc_routing->p15 && !test_prv.chipdoc_routing->key && !test_prv.chipdoc_routing->pin);
		struct sc_pin_cmd_data info = {0}; info.cmd = SC_PIN_CMD_GET_INFO; info.pin_type = SC_AC_CHV; info.pin_reference = 1;
		assert(sc_pin_cmd(&test_card,&info) < 0 && physical == 9 && !reader_reserved); destroy_routing();
	}
	puts("PASS real sc_pkcs15_card_free chains original clear once and detaches before object free, including failed reset; later callbacks never access freed owner and finish frees card-owned routing then original private data");
	free_native(); EVP_PKEY_free(rsa_key); EVP_PKEY_free(peer.peer); jcop4_ca_pending_clear(&peer.derived);
	OSSL_PROVIDER_unload(test_provider.defprov); OSSL_LIB_CTX_free(test_provider.libctx);
	OSSL_PROVIDER_unload(alternate_provider.defprov); OSSL_LIB_CTX_free(alternate_provider.libctx); return 0;
}
#endif
