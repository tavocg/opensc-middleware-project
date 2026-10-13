/* Simulate OpenSC discovery APIs: no reader, card, PIN or PC/SC calls. */
#define main card_public_info_main
#include "../tools/card-public-info.c"
#undef main
#include <assert.h>

static sc_context_t fixture_ctx;
static sc_reader_t fixture_reader;
static sc_card_t fixture_card;
static struct sc_ef_atr fixture_ef_atr;
static sc_app_info_t fixture_app;
static sc_pkcs15_card_t fixture_p15;
static sc_pkcs15_tokeninfo_t fixture_tokeninfo;
static sc_pkcs15_object_t fixture_cert_obj, fixture_key_obj, fixture_pin_obj;
static sc_pkcs15_cert_info_t fixture_cert_info;
static sc_pkcs15_prkey_info_t fixture_key_info;
static sc_pkcs15_auth_info_t fixture_pin_info;
static sc_pkcs15_cert_t fixture_cert;
static int atr_calls, dir_calls, lock_calls, unlock_calls, connect_calls, release_calls;
static int atr_result, dir_result, lock_result, driver_result, mismatch;
static int bind_calls, object_calls, cert_calls, p15_frees, cert_frees;
static int bind_result, objects_result, cert_result;
static int select_calls, mse_calls, select_result, mse_result, mse_sw;
static char fixture_atr[] = "3b:d5:18:ff:81:91:fe:1f:c3:80:73:c8:21:10:0a";
static char fixture_aid[] = "E828BD080FA00000036364796E6964";

int sc_context_create(sc_context_t **ctx, const sc_context_param_t *params)
{
	assert(params->debug == 0);
	*ctx = &fixture_ctx;
	return 0;
}
int sc_release_context(sc_context_t *ctx) { assert(ctx == &fixture_ctx); release_calls++; return 0; }
int sc_set_card_driver(sc_context_t *ctx, const char *name)
{
	assert(ctx == &fixture_ctx && !strcmp(name, "default") &&
			(ctx->flags & SC_CTX_FLAG_ENABLE_DEFAULT_DRIVER));
	return driver_result;
}
sc_reader_t *sc_ctx_get_reader(sc_context_t *ctx, unsigned int index)
{
	assert(ctx == &fixture_ctx && index == 2);
	return &fixture_reader;
}
int sc_connect_card(sc_reader_t *reader, sc_card_t **card)
{
	assert(reader == &fixture_reader);
	connect_calls++;
	*card = &fixture_card;
	fixture_card.atr.len = sizeof(fixture_card.atr.value);
	assert(sc_hex_to_bin(fixture_atr, fixture_card.atr.value, &fixture_card.atr.len) == 0);
	if (mismatch) fixture_card.atr.value[0] ^= 1;
	return 0;
}
int sc_disconnect_card(sc_card_t *card) { assert(card == &fixture_card); return 0; }
int sc_lock(sc_card_t *card) { assert(card == &fixture_card); lock_calls++; return lock_result; }
int sc_unlock(sc_card_t *card) { assert(card == &fixture_card); unlock_calls++; return 0; }
int sc_parse_ef_atr(sc_card_t *card)
{
	assert(card == &fixture_card && lock_calls == 1 && unlock_calls == 0);
	atr_calls++;
	card->ef_atr = &fixture_ef_atr;
	return atr_result;
}
int sc_enum_apps(sc_card_t *card)
{
	assert(card == &fixture_card && lock_calls == 1 && unlock_calls == 0);
	dir_calls++;
	if (dir_result < 0) { card->app_count = 0; return dir_result; }
	card->app_count = 1;
	card->app[0] = &fixture_app;
	return dir_result;
}
sc_pkcs15_card_t *sc_pkcs15_card_new(void) { return &fixture_p15; }
void sc_pkcs15_card_free(sc_pkcs15_card_t *p15) { assert(p15 == &fixture_p15); p15_frees++; }
int sc_pkcs15_bind_internal(sc_pkcs15_card_t *p15, struct sc_aid *aid)
{
	assert(p15 == &fixture_p15 && p15->card == &fixture_card &&
			!p15->opts.use_file_cache && !p15->opts.use_pin_cache &&
			p15->opts.pin_protected_certificate == SC_PKCS15_CARD_OPTS_PRIV_CERT_PROTECT &&
			aid->len == fixture_app.aid.len);
	assert(lock_calls == 1 && unlock_calls == 0);
	bind_calls++;
	return bind_result;
}
int sc_pkcs15_get_objects(sc_pkcs15_card_t *p15, unsigned int type, sc_pkcs15_object_t **ret, size_t count)
{
	assert(p15 == &fixture_p15 && count == 32);
	object_calls++;
	if (objects_result < 0) return objects_result;
	switch (type) {
	case SC_PKCS15_TYPE_CERT_X509: ret[0] = &fixture_cert_obj; break;
	case SC_PKCS15_TYPE_PRKEY: ret[0] = &fixture_key_obj; break;
	case SC_PKCS15_TYPE_AUTH_PIN: ret[0] = &fixture_pin_obj; break;
	default: assert(0);
	}
	return 1;
}
int sc_pkcs15_read_certificate(sc_pkcs15_card_t *p15, const sc_pkcs15_cert_info_t *info,
		int private_obj, sc_pkcs15_cert_t **cert)
{
	assert(p15 == &fixture_p15 && info == &fixture_cert_info && private_obj == 0);
	cert_calls++;
	if (cert_result < 0) return cert_result;
	*cert = &fixture_cert;
	return 0;
}
void sc_pkcs15_free_certificate(sc_pkcs15_cert_t *cert) { assert(cert == &fixture_cert); cert_frees++; }
int sc_select_file(sc_card_t *card, const sc_path_t *path, sc_file_t **file)
{
	assert(card == &fixture_card && lock_calls == 1 && !unlock_calls && !file);
	assert(path->type == SC_PATH_TYPE_DF_NAME && path->len == fixture_app.aid.len &&
			!memcmp(path->value, fixture_app.aid.value, path->len));
	select_calls++;
	return select_result;
}
int sc_transmit_apdu(sc_card_t *card, sc_apdu_t *apdu)
{
	const unsigned char expected[] = { 0x91, 0x02, 0x28, 0x91 };
	assert(card == &fixture_card && lock_calls == 1 && !unlock_calls && select_calls == 1);
	/* Any VERIFY, PSO, different key/algorithm, or second MSE aborts the test. */
	assert(!mse_calls && apdu->cla == 0 && apdu->ins == 0x22 && apdu->p1 == 0x81 && apdu->p2 == 0xB6 &&
			(apdu->flags & (SC_APDU_FLAGS_NO_RETRY_WL | SC_APDU_FLAGS_NO_GET_RESP)) ==
			(SC_APDU_FLAGS_NO_RETRY_WL | SC_APDU_FLAGS_NO_GET_RESP) &&
			apdu->cse == SC_APDU_CASE_3_SHORT && apdu->lc == sizeof(expected) &&
			apdu->datalen == sizeof(expected) && !apdu->le && !apdu->resplen &&
			!memcmp(apdu->data, expected, sizeof(expected)));
	mse_calls++;
	apdu->sw1 = mse_sw >> 8;
	apdu->sw2 = mse_sw & 0xFF;
	return mse_result;
}
static void reset_fixture(void)
{
	memset(&fixture_ctx, 0, sizeof(fixture_ctx));
	memset(&fixture_card, 0, sizeof(fixture_card));
	memset(&fixture_ef_atr, 0, sizeof(fixture_ef_atr));
	memset(&fixture_app, 0, sizeof(fixture_app));
	fixture_app.aid.len = sizeof(fixture_app.aid.value);
	assert(sc_hex_to_bin(fixture_aid, fixture_app.aid.value, &fixture_app.aid.len) == 0);
	fixture_app.label = "fixture";
	memset(&fixture_p15, 0, sizeof(fixture_p15));
	memset(&fixture_tokeninfo, 0, sizeof(fixture_tokeninfo));
	fixture_p15.tokeninfo = &fixture_tokeninfo;
	memset(&fixture_cert_obj, 0, sizeof(fixture_cert_obj));
	memset(&fixture_key_obj, 0, sizeof(fixture_key_obj));
	memset(&fixture_pin_obj, 0, sizeof(fixture_pin_obj));
	memset(&fixture_cert_info, 0, sizeof(fixture_cert_info));
	memset(&fixture_key_info, 0, sizeof(fixture_key_info));
	memset(&fixture_pin_info, 0, sizeof(fixture_pin_info));
	memset(&fixture_cert, 0, sizeof(fixture_cert));
	fixture_cert_obj.type = SC_PKCS15_TYPE_CERT_X509;
	fixture_cert_obj.data = &fixture_cert_info;
	fixture_key_obj.type = SC_PKCS15_TYPE_PRKEY_RSA;
	fixture_key_obj.data = &fixture_key_info;
	fixture_key_info.modulus_length = 3072;
	fixture_key_info.key_reference = 7;
	fixture_pin_obj.type = SC_PKCS15_TYPE_AUTH_PIN;
	fixture_pin_obj.data = &fixture_pin_info;
	fixture_cert.data.value = (unsigned char *)"fixture DER";
	fixture_cert.data.len = 11;
	atr_calls = dir_calls = lock_calls = unlock_calls = connect_calls = release_calls = 0;
	atr_result = dir_result = lock_result = driver_result = mismatch = 0;
	bind_calls = object_calls = cert_calls = p15_frees = cert_frees = 0;
	bind_result = objects_result = cert_result = 0;
	select_calls = mse_calls = select_result = mse_result = 0;
	mse_sw = 0x9000;
}
static void setup_fixture(void)
{
	reset_fixture();
	fixture_key_info.id.len = sizeof("LlaveDeFirma") - 1;
	memcpy(fixture_key_info.id.value, "LlaveDeFirma", fixture_key_info.id.len);
	fixture_key_info.key_reference = 0x91;
	fixture_key_info.modulus_length = 2048;
	fixture_key_info.native = 1;
	fixture_key_info.usage = SC_PKCS15_PRKEY_USAGE_SIGN;
	fixture_tokeninfo.supported_algos[0].reference = 1;
	fixture_tokeninfo.supported_algos[0].algo_ref = 0x28;
	fixture_key_info.algo_refs[0] = 1;
}
static int run_fixture(void)
{
	char *args[] = { "card-public-info", "2", fixture_atr };
	return card_public_info_main(3, args);
}
int main(void)
{
	char *bad_args[] = { "card-public-info", "-1", fixture_atr };
	reset_fixture();
	assert(run_fixture() == 0 && atr_calls == 1 && dir_calls == 1 && unlock_calls == 1 && release_calls == 1);
	puts("PASS public discovery uses the default driver and queries both files under one lock");
	reset_fixture(); mismatch = 1;
	assert(run_fixture() == 1 && !atr_calls && !dir_calls && !lock_calls && release_calls == 1);
	puts("PASS public discovery checks the connected card ATR before file queries");
	reset_fixture();
	assert(card_public_info_main(3, bad_args) == 2 && !release_calls && !connect_calls);
	bad_args[1] = "4294967296";
	assert(card_public_info_main(3, bad_args) == 2 && !connect_calls);
	bad_args[1] = "2"; bad_args[2] = "invalid";
	assert(card_public_info_main(3, bad_args) == 2 && !connect_calls);
	puts("PASS public discovery validates reader bounds and ATR before opening a context");
	reset_fixture(); atr_result = SC_ERROR_FILE_NOT_FOUND;
	assert(run_fixture() == 0 && atr_calls == 1 && dir_calls == 1 && unlock_calls == 1);
	puts("PASS public discovery can inspect EF.DIR when EF.ATR is absent");
	reset_fixture(); atr_result = SC_ERROR_SECURITY_STATUS_NOT_SATISFIED;
	assert(run_fixture() == 1 && atr_calls == 1 && !dir_calls && unlock_calls == 1);
	puts("PASS public discovery stops on security rejection without authentication or retries");
	reset_fixture(); dir_result = SC_ERROR_SECURITY_STATUS_NOT_SATISFIED;
	assert(run_fixture() == 1 && atr_calls == 1 && dir_calls == 1 && unlock_calls == 1 && release_calls == 1);
	puts("PASS public discovery releases the lock on directory failure");
	reset_fixture(); lock_result = SC_ERROR_READER;
	assert(run_fixture() == 1 && !atr_calls && !dir_calls && !unlock_calls && release_calls == 1);
	puts("PASS public discovery sends no file queries if locking fails");
	reset_fixture(); driver_result = SC_ERROR_NOT_SUPPORTED;
	assert(run_fixture() == 1 && !connect_calls && !atr_calls && !dir_calls && release_calls == 1);
	puts("PASS public discovery stops if the default driver cannot be selected");
	{
		char *args[] = { "card-public-info", "2", fixture_atr, "--pkcs15", fixture_aid };
		reset_fixture();
		assert(card_public_info_main(5, args) == 0 && bind_calls == 1 && object_calls == 3 &&
				cert_calls == 1 && cert_frees == 1 && p15_frees == 1 && unlock_calls == 1);
		puts("PASS native discovery reads public certificate DER and metadata without a fixed RSA size or key reference");
		reset_fixture(); args[4] = "A001";
		assert(card_public_info_main(5, args) == 1 && !bind_calls && !object_calls && unlock_calls == 1);
		puts("PASS native discovery rejects an AID not announced in EF.DIR before binding");
		reset_fixture(); args[4] = fixture_aid; bind_result = SC_ERROR_INVALID_ASN1_OBJECT;
		assert(card_public_info_main(5, args) == 1 && bind_calls == 1 && !object_calls && p15_frees == 1);
		puts("PASS native discovery stops on binding error without emulation fallback");
		reset_fixture(); fixture_cert_obj.flags = SC_PKCS15_CO_FLAG_PRIVATE;
		assert(card_public_info_main(5, args) == 0 && object_calls == 3 && !cert_calls && p15_frees == 1);
		puts("PASS native discovery skips private certificates without attempting authentication");
		reset_fixture(); cert_result = SC_ERROR_SECURITY_STATUS_NOT_SATISFIED;
		assert(card_public_info_main(5, args) == 1 && cert_calls == 1 && object_calls == 1 &&
				p15_frees == 1 && unlock_calls == 1);
		puts("PASS native discovery stops on certificate security rejection without retry");
		reset_fixture(); objects_result = SC_ERROR_SECURITY_STATUS_NOT_SATISFIED;
		assert(card_public_info_main(5, args) == 1 && object_calls == 1 && !cert_calls && p15_frees == 1);
		puts("PASS native discovery stops on metadata security rejection without reading other directories");
		reset_fixture(); args[3] = "--login";
		assert(card_public_info_main(5, args) == 2 && !connect_calls && !bind_calls);
		puts("PASS native discovery accepts only its public PKCS15 operation");
	}
	{
		char *args[] = { "card-public-info", "2", fixture_atr, "--sign-setup", fixture_aid };
		setup_fixture();
		assert(card_public_info_main(5, args) == 0 && !cert_calls && select_calls == 1 && mse_calls == 1 && unlock_calls == 1);
		puts("PASS sign setup preserves the observed key byte and sends one temporary MSE without certificate reads or authentication");
		setup_fixture(); mse_sw = 0x6982;
		assert(card_public_info_main(5, args) == 1 && mse_calls == 1 && unlock_calls == 1 && p15_frees == 1);
		setup_fixture(); mse_sw = 0x6A80;
		assert(card_public_info_main(5, args) == 1 && mse_calls == 1 && unlock_calls == 1);
		setup_fixture(); mse_result = SC_ERROR_TRANSMIT_FAILED;
		assert(card_public_info_main(5, args) == 1 && mse_calls == 1 && unlock_calls == 1);
		puts("PASS sign setup treats card rejection and transport failure as errors without retries or PIN fallback");
		setup_fixture(); select_result = SC_ERROR_FILE_NOT_FOUND;
		assert(card_public_info_main(5, args) == 1 && select_calls == 1 && !mse_calls && unlock_calls == 1);
		puts("PASS sign setup stops before MSE if the application cannot be reselected");
		setup_fixture(); fixture_key_info.key_reference = 3;
		assert(card_public_info_main(5, args) == 1 && !select_calls && !mse_calls);
		setup_fixture(); fixture_key_info.modulus_length = 3072;
		assert(card_public_info_main(5, args) == 1 && !mse_calls);
		setup_fixture(); fixture_key_obj.type = SC_PKCS15_TYPE_PRKEY_EC;
		assert(card_public_info_main(5, args) == 1 && !mse_calls);
		setup_fixture(); fixture_key_info.id.value[0] ^= 1;
		assert(card_public_info_main(5, args) == 1 && !mse_calls);
		setup_fixture(); fixture_key_info.usage = 0;
		assert(card_public_info_main(5, args) == 1 && !mse_calls);
		puts("PASS sign setup rejects changed key identity, type, size, usage or reference before MSE");
		setup_fixture(); mismatch = 1;
		assert(card_public_info_main(5, args) == 1 && !bind_calls && !mse_calls);
		setup_fixture(); args[2] = "3b:00";
		assert(card_public_info_main(5, args) == 2 && !connect_calls);
		setup_fixture(); args[2] = fixture_atr; args[4] = "A001";
		assert(card_public_info_main(5, args) == 2 && !connect_calls);
		puts("PASS sign setup is restricted to the observed connected ATR and application");
	}
	return 0;
}
