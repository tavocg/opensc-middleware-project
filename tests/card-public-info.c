/* Simulate OpenSC discovery APIs: no reader, card, PIN or PC/SC calls. */
#define main card_public_info_main
#include "../tools/card-public-info.c"
#undef main
#include <assert.h>
#include <unistd.h>

static sc_context_t fixture_ctx;
static sc_reader_t fixture_reader;
static sc_card_t fixture_card;
static struct sc_ef_atr fixture_ef_atr;
static sc_app_info_t fixture_app;
static sc_pkcs15_card_t fixture_p15;
static sc_pkcs15_tokeninfo_t fixture_tokeninfo;
static sc_pkcs15_object_t fixture_cert_obj, fixture_key_obj, fixture_pin_obj, fixture_authkey_obj;
static sc_pkcs15_cert_info_t fixture_cert_info;
static sc_pkcs15_prkey_info_t fixture_key_info;
static sc_pkcs15_auth_info_t fixture_pin_info, fixture_authkey_info;
static sc_pkcs15_cert_t fixture_cert;
static sc_pkcs15_df_t fixture_pub_df, fixture_cert_df;
static sc_pkcs15_object_t fixture_pub_rsa_obj, fixture_pub_ec_obj;
static sc_pkcs15_pubkey_info_t fixture_pub_rsa_info, fixture_pub_ec_info;
static int pub_parse_calls, pub_parse_result, pub_empty;
static int cdf_parse_calls, cdf_parse_result, cdf_empty;
static int atr_calls, dir_calls, lock_calls, unlock_calls, connect_calls, release_calls;
static int atr_result, dir_result, lock_result, driver_result, mismatch;
static int bind_calls, object_calls, cert_calls, p15_frees, cert_frees;
static int bind_result, objects_result, cert_result;
static int select_calls, mse_calls, select_result, mse_result, mse_sw;
static int pin_calls, pin_result, pin_sw;
static int version_calls, version_result, version_sw, version_len;
static int cardaccess_calls, cardaccess_result, cardaccess_sw, cardaccess_len, mf_select_result;
static int docp_mode, docp_calls, docp_result[2], docp_sw[2], docp_len[2];
static int odf_mode, odf_select_calls, odf_read_calls, odf_select_result, odf_read_result, odf_select_sw, odf_read_sw;
static size_t odf_size;
static int file4141_wrongfid, file4141_mode, file4141_calls, file4141_transport, file4141_sw;
static size_t file4141_size;
static int dh_mode, dh_calls, dh_transport, dh_sw;
static size_t dh_size;
static unsigned char fixture_dh[137];
static const unsigned char fixture_fcp[] = { 0x62, 11, 0x82, 1, 1, 0x83, 2, 0x41, 0x41, 0x80, 2, 0, 32 };
static const unsigned char fixture_odf[] = { 0xA4, 6, 0x30, 4, 4, 2, 0x70, 5, 0xA8, 6, 0x30, 4, 4, 2, 0x41, 0x41 };
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
int sc_pkcs15_parse_df(sc_pkcs15_card_t *p15, sc_pkcs15_df_t *df)
{
	if (df == &fixture_cert_df) {
		assert(p15 == &fixture_p15 && df->type == SC_PKCS15_CDF &&
				df->path.value[df->path.len - 2] == 0x70 && df->path.value[df->path.len - 1] == 5);
		assert(!object_calls && !cert_calls && !mse_calls && !pin_calls && !cdf_parse_calls && !pub_parse_calls);
		cdf_parse_calls++;
		if (cdf_parse_result < 0) return cdf_parse_result;
		p15->obj_list = cdf_empty ? NULL : &fixture_cert_obj;
		return 0;
	}
	assert(p15 == &fixture_p15 && df == &fixture_pub_df && df->type == SC_PKCS15_PUKDF &&
			df->path.len == 2 && df->path.value[0] == 0x70 && df->path.value[1] == 4);
	assert(!object_calls && !cert_calls && !mse_calls && !pin_calls && !pub_parse_calls);
	pub_parse_calls++;
	if (pub_parse_result < 0) return pub_parse_result;
	p15->obj_list = pub_empty ? NULL : &fixture_pub_rsa_obj;
	return 0;
}
int sc_pkcs15_read_pubkey(sc_pkcs15_card_t *p15, const sc_pkcs15_object_t *obj, sc_pkcs15_pubkey_t **key)
{
	(void)p15; (void)obj; (void)key;
	assert(0); return SC_ERROR_INTERNAL; /* No referenced key reads in this mode. */
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
	case SC_PKCS15_TYPE_AUTH:
		ret[0] = &fixture_pin_obj; ret[1] = &fixture_authkey_obj;
		return 2;
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
	if (path->type == SC_PATH_TYPE_PATH) {
		assert((docp_mode ? select_calls == 0 && !version_calls : select_calls == 1 && version_calls == 1) && !cardaccess_calls &&
				path->len == 2 && path->value[0] == 0x3F && path->value[1] == 0);
		select_calls++;
		return mf_select_result;
	}
	assert(select_calls == (docp_mode ? 1 : 0) && path->type == SC_PATH_TYPE_DF_NAME && path->len == fixture_app.aid.len &&
			!memcmp(path->value, fixture_app.aid.value, path->len));
	select_calls++;
	return select_result;
}
int sc_transmit_apdu(sc_card_t *card, sc_apdu_t *apdu)
{
	const unsigned char expected[] = { 0x91, 0x02, 0x28, 0x91 };
	assert(card == &fixture_card && lock_calls == 1 && !unlock_calls);
	if (dh_mode && file4141_calls == 1) {
		assert(card == &fixture_card && lock_calls == 1 && !unlock_calls && select_calls == 1 && !dh_calls &&
				!bind_calls && !object_calls && !cert_calls && !mse_calls && !pin_calls && !odf_read_calls &&
				apdu->cla == 0 && apdu->ins == 0xB0 && apdu->p1 == 0 && apdu->p2 == 0 &&
				apdu->cse == SC_APDU_CASE_2_SHORT && !apdu->lc && !apdu->datalen && !apdu->data &&
				apdu->le == 137 && apdu->resplen == 137 &&
				(apdu->flags & (SC_APDU_FLAGS_NO_RETRY_WL | SC_APDU_FLAGS_NO_GET_RESP)) ==
				(SC_APDU_FLAGS_NO_RETRY_WL | SC_APDU_FLAGS_NO_GET_RESP));
		dh_calls++;
		memcpy(apdu->resp, fixture_dh, sizeof(fixture_dh));
		apdu->resplen = dh_size;
		apdu->sw1 = dh_sw >> 8; apdu->sw2 = dh_sw & 0xFF;
		return dh_transport;
	}
	if (file4141_mode) {
		assert(select_calls == 1 && !file4141_calls && !bind_calls && !object_calls && !cert_calls && !mse_calls && !pin_calls &&
				!odf_read_calls && apdu->cla == 0 && apdu->ins == 0xA4 && apdu->p1 == 2 && apdu->p2 == 0 &&
				apdu->cse == SC_APDU_CASE_4_SHORT && apdu->lc == 2 && apdu->datalen == 2 &&
				apdu->data[0] == (file4141_mode == 10 ? 0xD0 : file4141_mode >= 8 ? 0xB0 : file4141_mode == 7 ? 0xDF : file4141_mode >= 3 ? 0x70 : 0x41) &&
				apdu->data[1] == (file4141_mode == 10 ? 4 : file4141_mode == 9 ? 2 : file4141_mode == 8 ? 1 : file4141_mode == 7 ? 0x0F : file4141_mode == 6 ? 5 : file4141_mode == 5 ? 4 : file4141_mode == 4 ? 2 : file4141_mode == 3 ? 1 : file4141_mode == 2 ? 0x09 : 0x41) && apdu->le == 256 && apdu->resplen == 256 &&
				(apdu->flags & (SC_APDU_FLAGS_NO_RETRY_WL | SC_APDU_FLAGS_NO_GET_RESP)) ==
				(SC_APDU_FLAGS_NO_RETRY_WL | SC_APDU_FLAGS_NO_GET_RESP));
		file4141_calls++;
		memcpy(apdu->resp, fixture_fcp, sizeof(fixture_fcp));
		if (!file4141_wrongfid && file4141_mode > 1) {
			apdu->resp[7] = file4141_mode == 10 ? 0xD0 : file4141_mode >= 8 ? 0xB0 : file4141_mode == 7 ? 0xDF : file4141_mode >= 3 ? 0x70 : 0x41;
			apdu->resp[8] = file4141_mode == 10 ? 4 : file4141_mode == 9 ? 2 : file4141_mode == 8 ? 1 : file4141_mode == 7 ? 0x0F : file4141_mode == 6 ? 5 : file4141_mode == 5 ? 4 : file4141_mode == 4 ? 2 : file4141_mode == 3 ? 1 : 0x09;
		}
		if (dh_mode) apdu->resp[12] = 137;
		apdu->resplen = file4141_size;
		apdu->sw1 = file4141_sw >> 8; apdu->sw2 = file4141_sw & 0xFF;
		return file4141_transport;
	}
	if (odf_mode) {
		assert(select_calls == 1 && !bind_calls && !object_calls && !cert_calls && !mse_calls && !pin_calls &&
				apdu->cla == 0 && (apdu->flags & (SC_APDU_FLAGS_NO_RETRY_WL | SC_APDU_FLAGS_NO_GET_RESP)) ==
				(SC_APDU_FLAGS_NO_RETRY_WL | SC_APDU_FLAGS_NO_GET_RESP));
		if (apdu->ins == 0xA4) {
			assert(!odf_select_calls && !odf_read_calls && apdu->p1 == 2 && apdu->p2 == 0x0C &&
					apdu->cse == SC_APDU_CASE_3_SHORT && apdu->lc == 2 && apdu->datalen == 2 &&
					apdu->data[0] == 0x50 && apdu->data[1] == 0x31 && !apdu->le && !apdu->resplen);
			odf_select_calls++;
			apdu->sw1 = odf_select_sw >> 8; apdu->sw2 = odf_select_sw & 0xFF;
			return odf_select_result;
		}
		assert(apdu->ins == 0xB0 && odf_select_calls == 1 && !odf_read_calls && !apdu->p1 && !apdu->p2 &&
				apdu->cse == SC_APDU_CASE_2_SHORT && !apdu->lc && !apdu->datalen && !apdu->data &&
				apdu->le == 256 && apdu->resplen == 256);
		odf_read_calls++;
		memcpy(apdu->resp, fixture_odf, sizeof(fixture_odf)); apdu->resplen = odf_size;
		apdu->sw1 = odf_read_sw >> 8; apdu->sw2 = odf_read_sw & 0xFF;
		return odf_read_result;
	}
	if (apdu->ins == 0xCB) {
		const unsigned char request[] = { 0x4D, 0x08, 0x70, 0x06, 0xBF, 0x81, 0x01, 0x02, 0xA0, 0x80 };
		int index = docp_calls++;
		assert(docp_mode && index < 2 && select_calls == index + 1 &&
				!pin_calls && !mse_calls && !version_calls && !cardaccess_calls &&
				apdu->cla == 0 && apdu->p1 == 0x3F && apdu->p2 == 0xFF &&
				apdu->cse == SC_APDU_CASE_4_SHORT && apdu->lc == sizeof(request) &&
				apdu->datalen == sizeof(request) && !memcmp(apdu->data, request, sizeof(request)) &&
				apdu->le == 256 && apdu->resplen == 256 &&
				(apdu->flags & (SC_APDU_FLAGS_NO_RETRY_WL | SC_APDU_FLAGS_NO_GET_RESP)) ==
				(SC_APDU_FLAGS_NO_RETRY_WL | SC_APDU_FLAGS_NO_GET_RESP));
		memset(apdu->resp, 0, 256);
		apdu->resp[0] = 0xA0; /* Synthetic public DOCP, never a credential. */
		apdu->resplen = docp_len[index];
		apdu->sw1 = docp_sw[index] >> 8; apdu->sw2 = docp_sw[index] & 0xFF;
		return docp_result[index];
	}
	if (apdu->ins == 0xCA || apdu->ins == 0xB0) {
		assert(!pin_calls && !mse_calls && apdu->cla == 0 && !apdu->data && !apdu->datalen && !apdu->lc &&
				apdu->cse == SC_APDU_CASE_2_SHORT &&
				(apdu->flags & (SC_APDU_FLAGS_NO_RETRY_WL | SC_APDU_FLAGS_NO_GET_RESP)) ==
				(SC_APDU_FLAGS_NO_RETRY_WL | SC_APDU_FLAGS_NO_GET_RESP));
		if (apdu->ins == 0xCA) {
			assert(select_calls == 1 && !version_calls && !cardaccess_calls &&
					apdu->p1 == 1 && apdu->p2 == 0x16 && apdu->le == 6 && apdu->resplen == 6);
			version_calls++;
			memset(apdu->resp, 0x01, 6);
			apdu->resplen = version_len;
			apdu->sw1 = version_sw >> 8; apdu->sw2 = version_sw & 0xFF;
			return version_result;
		}
		assert(select_calls == 2 && version_calls == 1 && !cardaccess_calls &&
				apdu->p1 == 0x9C && apdu->p2 == 0 && apdu->le == 256 && apdu->resplen == 256);
		cardaccess_calls++;
		memset(apdu->resp, 0, 256);
		apdu->resp[0] = 0x31; /* Synthetic DER SET, no real card data. */
		apdu->resplen = cardaccess_len;
		apdu->sw1 = cardaccess_sw >> 8; apdu->sw2 = cardaccess_sw & 0xFF;
		return cardaccess_result;
	}
	assert(select_calls == 1);
	if (apdu->ins == 0x20) {
		/* No PIN bytes, authentication attempt, alternate reference, or MSE. */
		assert(!pin_calls && !mse_calls && apdu->cla == 0 && apdu->p1 == 0 && apdu->p2 == 1 &&
				apdu->cse == SC_APDU_CASE_1 && !apdu->data && !apdu->datalen && !apdu->lc &&
				!apdu->le && !apdu->resplen &&
				(apdu->flags & (SC_APDU_FLAGS_NO_RETRY_WL | SC_APDU_FLAGS_NO_GET_RESP)) ==
				(SC_APDU_FLAGS_NO_RETRY_WL | SC_APDU_FLAGS_NO_GET_RESP));
		pin_calls++;
		apdu->sw1 = pin_sw >> 8;
		apdu->sw2 = pin_sw & 0xFF;
		return pin_result;
	}
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
	fixture_card.ctx = &fixture_ctx;
	memset(&fixture_ef_atr, 0, sizeof(fixture_ef_atr));
	memset(&fixture_app, 0, sizeof(fixture_app));
	fixture_app.aid.len = sizeof(fixture_app.aid.value);
	assert(sc_hex_to_bin(fixture_aid, fixture_app.aid.value, &fixture_app.aid.len) == 0);
	fixture_app.label = "fixture";
	memset(&fixture_p15, 0, sizeof(fixture_p15));
	memset(&fixture_pub_df, 0, sizeof(fixture_pub_df));
	fixture_pub_df.type = SC_PKCS15_PUKDF;
	fixture_pub_df.path.len = 2; fixture_pub_df.path.type = SC_PATH_TYPE_PATH;
	fixture_pub_df.path.value[0] = 0x70; fixture_pub_df.path.value[1] = 4;
	fixture_p15.df_list = &fixture_pub_df;
	memset(&fixture_pub_rsa_obj, 0, sizeof(fixture_pub_rsa_obj));
	memset(&fixture_pub_ec_obj, 0, sizeof(fixture_pub_ec_obj));
	memset(&fixture_pub_rsa_info, 0, sizeof(fixture_pub_rsa_info));
	memset(&fixture_pub_ec_info, 0, sizeof(fixture_pub_ec_info));
	fixture_pub_rsa_obj.type = SC_PKCS15_TYPE_PUBKEY_RSA;
	fixture_pub_ec_obj.type = SC_PKCS15_TYPE_PUBKEY_EC;
	fixture_pub_rsa_obj.data = &fixture_pub_rsa_info;
	fixture_pub_ec_obj.data = &fixture_pub_ec_info;
	fixture_pub_rsa_obj.df = fixture_pub_ec_obj.df = &fixture_pub_df;
	fixture_pub_rsa_obj.next = &fixture_pub_ec_obj;
	strcpy(fixture_pub_rsa_obj.label, "SECRET_LABEL");
	fixture_pub_rsa_info.id.len = 4; memcpy(fixture_pub_rsa_info.id.value, "SECR", 4);
	fixture_pub_rsa_info.modulus_length = 3072; fixture_pub_rsa_info.key_reference = 145;
	fixture_pub_rsa_info.native = 1;
	fixture_pub_ec_info.field_length = 521; fixture_pub_ec_info.key_reference = 7;
	pub_parse_calls = pub_parse_result = pub_empty = 0;
	memset(&fixture_tokeninfo, 0, sizeof(fixture_tokeninfo));
	fixture_p15.tokeninfo = &fixture_tokeninfo;
	memset(&fixture_cert_obj, 0, sizeof(fixture_cert_obj));
	memset(&fixture_key_obj, 0, sizeof(fixture_key_obj));
	memset(&fixture_pin_obj, 0, sizeof(fixture_pin_obj));
	memset(&fixture_authkey_obj, 0, sizeof(fixture_authkey_obj));
	memset(&fixture_cert_info, 0, sizeof(fixture_cert_info));
	memset(&fixture_key_info, 0, sizeof(fixture_key_info));
	memset(&fixture_pin_info, 0, sizeof(fixture_pin_info));
	memset(&fixture_authkey_info, 0, sizeof(fixture_authkey_info));
	memset(&fixture_cert, 0, sizeof(fixture_cert));
	fixture_cert_obj.type = SC_PKCS15_TYPE_CERT_X509;
	fixture_cert_obj.data = &fixture_cert_info;
	memset(&fixture_cert_df, 0, sizeof(fixture_cert_df));
	fixture_cert_df.type = SC_PKCS15_CDF;
	fixture_cert_df.path.type = SC_PATH_TYPE_PATH;
	fixture_cert_df.path.len = 2; memcpy(fixture_cert_df.path.value, "\x70\x05", 2);
	fixture_cert_obj.df = &fixture_cert_df;
	strcpy(fixture_cert_obj.label, "SECRET_CERTIFICATE_LABEL");
	fixture_cert_info.id.len = 4; memcpy(fixture_cert_info.id.value, "SECR", 4);
	fixture_cert_info.path.type = SC_PATH_TYPE_PATH;
	fixture_cert_info.path.len = 2; memcpy(fixture_cert_info.path.value, "\xB0\x01", 2);
	fixture_cert_info.path.count = -1;
	cdf_parse_calls = cdf_parse_result = cdf_empty = 0;
	fixture_key_obj.type = SC_PKCS15_TYPE_PRKEY_RSA;
	fixture_key_obj.data = &fixture_key_info;
	fixture_key_info.modulus_length = 3072;
	fixture_key_info.key_reference = 7;
	fixture_pin_obj.type = SC_PKCS15_TYPE_AUTH_PIN;
	fixture_pin_obj.data = &fixture_pin_info;
	fixture_authkey_obj.type = SC_PKCS15_TYPE_AUTH_AUTHKEY;
	fixture_authkey_obj.data = &fixture_authkey_info;
	fixture_authkey_info.auth_type = SC_PKCS15_PIN_AUTH_TYPE_AUTH_KEY;
	fixture_authkey_info.path.type = SC_PATH_TYPE_PATH;
	fixture_authkey_info.path.len = 4;
	memcpy(fixture_authkey_info.path.value, "\x3F\x00\xE0\x00", 4);
	fixture_authkey_info.attrs.authkey.skey_id.len = 1;
	fixture_authkey_info.attrs.authkey.skey_id.value[0] = 4;
	fixture_cert.data.value = (unsigned char *)"fixture DER";
	fixture_cert.data.len = 11;
	atr_calls = dir_calls = lock_calls = unlock_calls = connect_calls = release_calls = 0;
	atr_result = dir_result = lock_result = driver_result = mismatch = 0;
	bind_calls = object_calls = cert_calls = p15_frees = cert_frees = 0;
	bind_result = objects_result = cert_result = 0;
	select_calls = mse_calls = select_result = mse_result = 0;
	mse_sw = 0x9000;
	pin_calls = pin_result = 0;
	pin_sw = 0x63C7;
	version_calls = version_result = cardaccess_calls = cardaccess_result = mf_select_result = 0;
	version_sw = 0x9000; version_len = 6;
	cardaccess_sw = 0x6282; cardaccess_len = 2;
	docp_mode = docp_calls = docp_result[0] = docp_result[1] = 0;
	docp_sw[0] = docp_sw[1] = 0x9000;
	docp_len[0] = docp_len[1] = 4;
	odf_mode = odf_select_calls = odf_read_calls = odf_select_result = odf_read_result = 0;
	odf_select_sw = odf_read_sw = 0x9000; odf_size = sizeof(fixture_odf);
	file4141_wrongfid = file4141_mode = file4141_calls = file4141_transport = 0;
	file4141_sw = 0x9000; file4141_size = sizeof(fixture_fcp);
	dh_mode = dh_calls = dh_transport = 0; dh_sw = 0x9000; dh_size = sizeof(fixture_dh);
}
static void expect_metadata_summary(const unsigned char *buf, size_t size, const char *expected)
{
	char output[1024] = {0};
	FILE *capture = tmpfile();
	int saved = dup(STDOUT_FILENO);
	assert(capture && saved >= 0);
	fflush(stdout); assert(dup2(fileno(capture), STDOUT_FILENO) >= 0);
	assert(print_file4141_metadata(buf, size) == 0);
	fflush(stdout); rewind(capture);
	assert(fread(output, 1, sizeof(output) - 1, capture) > 0);
	assert(dup2(saved, STDOUT_FILENO) >= 0); close(saved); fclose(capture);
	assert(strstr(output, expected));
	assert(!strstr(output, "SECRET") && !strstr(output, "NAME") && !strstr(output, "ACL="));
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
static void pin_fixture(void)
{
	setup_fixture();
	fixture_key_obj.auth_id.len = sizeof("Card PIN") - 1;
	memcpy(fixture_key_obj.auth_id.value, "Card PIN", fixture_key_obj.auth_id.len);
	fixture_pin_info.auth_id = fixture_key_obj.auth_id;
	fixture_pin_info.auth_type = SC_PKCS15_PIN_AUTH_TYPE_PIN;
	fixture_pin_info.attrs.pin.reference = 1;
	fixture_pin_info.attrs.pin.flags = 0x11;
	fixture_pin_info.attrs.pin.min_length = 6;
	fixture_pin_info.attrs.pin.max_length = 14;
}
static int run_fixture(void)
{
	char *args[] = { "card-public-info", "2", fixture_atr };
	return card_public_info_main(3, args);
}

static void policy_fixture(void)
{
	pin_fixture();
	docp_mode = 1;
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
	{
		char *args[] = { "card-public-info", "2", fixture_atr, "--pin-status", fixture_aid };
		pin_fixture();
		assert(card_public_info_main(5, args) == 0 && pin_calls == 1 && !mse_calls && !cert_calls && unlock_calls == 1);
		pin_fixture(); pin_sw = 0x9000;
		assert(card_public_info_main(5, args) == 0 && pin_calls == 1 && !mse_calls && unlock_calls == 1);
		puts("PASS PIN status sends exactly one data-free VERIFY and accepts retries remaining or already verified");
		pin_fixture(); pin_sw = 0x63C0;
		assert(card_public_info_main(5, args) == 1 && pin_calls == 1 && unlock_calls == 1 && p15_frees == 1);
		pin_fixture(); pin_sw = 0x6983;
		assert(card_public_info_main(5, args) == 1 && pin_calls == 1 && unlock_calls == 1);
		pin_fixture(); pin_sw = 0x6A88;
		assert(card_public_info_main(5, args) == 1 && pin_calls == 1 && !mse_calls);
		pin_fixture(); pin_result = SC_ERROR_TRANSMIT_FAILED;
		assert(card_public_info_main(5, args) == 1 && pin_calls == 1 && unlock_calls == 1);
		puts("PASS PIN status stops on blocked, unexpected or transport failure without secret input or alternate commands");
		pin_fixture(); fixture_pin_info.attrs.pin.reference = 0x81;
		assert(card_public_info_main(5, args) == 1 && !pin_calls && !select_calls);
		pin_fixture(); fixture_pin_info.attrs.pin.flags |= SC_PKCS15_PIN_FLAG_LOCAL;
		assert(card_public_info_main(5, args) == 1 && !pin_calls);
		pin_fixture(); fixture_pin_info.auth_id.value[0] ^= 1;
		assert(card_public_info_main(5, args) == 1 && !pin_calls);
		pin_fixture(); fixture_pin_info.auth_type = SC_PKCS15_PIN_AUTH_TYPE_AUTH_KEY;
		assert(card_public_info_main(5, args) == 1 && !pin_calls);
		pin_fixture(); fixture_key_info.key_reference = 3;
		assert(card_public_info_main(5, args) == 1 && !pin_calls);
		puts("PASS PIN status requires the observed global PIN metadata linked to the observed signing key");
		pin_fixture(); select_result = SC_ERROR_FILE_NOT_FOUND;
		assert(card_public_info_main(5, args) == 1 && !pin_calls && select_calls == 1 && unlock_calls == 1);
		pin_fixture(); mismatch = 1;
		assert(card_public_info_main(5, args) == 1 && !pin_calls && !bind_calls);
		pin_fixture(); args[4] = "A001";
		assert(card_public_info_main(5, args) == 2 && !connect_calls);
		puts("PASS PIN status stops before VERIFY on selection failure or another connected ATR or application");
	}
	{
		char *args[] = { "card-public-info", "2", fixture_atr, "--security-info", fixture_aid };
		/* Different RSA metadata is allowed: this operation only reads public information. */
		reset_fixture();
		assert(card_public_info_main(5, args) == 0 && object_calls == 3 && !cert_calls &&
				version_calls == 1 && cardaccess_calls == 1 && !pin_calls && !mse_calls && unlock_calls == 1);
		reset_fixture(); cardaccess_sw = 0x9000; cardaccess_len = 256;
		assert(card_public_info_main(5, args) == 0 && cardaccess_calls == 1 && unlock_calls == 1);
		puts("PASS security discovery enumerates all authentication metadata and reads one public block without credentials or follow-up APDUs");
		{
			const int short_lengths[] = { 0, 1, 5 };
			size_t i;
			for (i = 0; i < sizeof(short_lengths) / sizeof(short_lengths[0]); i++) {
				reset_fixture(); version_len = short_lengths[i];
				assert(card_public_info_main(5, args) == 0 && version_calls == 1 &&
						cardaccess_calls == 1 && !pin_calls && !mse_calls && unlock_calls == 1);
			}
			puts("PASS security discovery accepts bounded shorter version replies without assuming the eOI format or skipping CardAccess");
		}
		reset_fixture(); version_sw = 0x6D00; cardaccess_sw = 0x6A82;
		assert(card_public_info_main(5, args) == 0 && version_calls == 1 && cardaccess_calls == 1 && unlock_calls == 1);
		reset_fixture(); cardaccess_sw = 0x6B00;
		assert(card_public_info_main(5, args) == 0 && cardaccess_calls == 1);
		puts("PASS security discovery tolerates unavailable optional public data without alternate queries");
		reset_fixture(); version_sw = 0x6982;
		assert(card_public_info_main(5, args) == 1 && version_calls == 1 && !cardaccess_calls && select_calls == 1 && unlock_calls == 1);
		reset_fixture(); version_sw = 0x6C06;
		assert(card_public_info_main(5, args) == 1 && version_calls == 1 && !cardaccess_calls);
		reset_fixture(); version_result = SC_ERROR_TRANSMIT_FAILED;
		assert(card_public_info_main(5, args) == 1 && version_calls == 1 && !cardaccess_calls && unlock_calls == 1);
		reset_fixture(); version_len = 7;
		assert(card_public_info_main(5, args) == 1 && !cardaccess_calls);
		puts("PASS security discovery stops on version security, transport, length or retry status errors");
		reset_fixture(); mf_select_result = SC_ERROR_FILE_NOT_FOUND;
		assert(card_public_info_main(5, args) == 1 && version_calls == 1 && !cardaccess_calls && unlock_calls == 1);
		reset_fixture(); cardaccess_sw = 0x6982;
		assert(card_public_info_main(5, args) == 1 && cardaccess_calls == 1 && unlock_calls == 1);
		reset_fixture(); cardaccess_result = SC_ERROR_TRANSMIT_FAILED;
		assert(card_public_info_main(5, args) == 1 && cardaccess_calls == 1 && unlock_calls == 1);
		reset_fixture(); cardaccess_len = 257;
		assert(card_public_info_main(5, args) == 1 && cardaccess_calls == 1 && unlock_calls == 1);
		puts("PASS security discovery releases the transaction on MF selection and CardAccess failures");
		reset_fixture(); objects_result = SC_ERROR_SECURITY_STATUS_NOT_SATISFIED;
		assert(card_public_info_main(5, args) == 1 && !select_calls && !version_calls && !cardaccess_calls && unlock_calls == 1);
		reset_fixture(); select_result = SC_ERROR_FILE_NOT_FOUND;
		assert(card_public_info_main(5, args) == 1 && select_calls == 1 && !version_calls && unlock_calls == 1);
		reset_fixture(); mismatch = 1;
		assert(card_public_info_main(5, args) == 1 && !bind_calls && !version_calls);
		reset_fixture(); args[4] = "A001";
		assert(card_public_info_main(5, args) == 2 && !connect_calls);
		puts("PASS security discovery checks metadata, application selection and connected ATR before public queries");
	}
	{
		char *args[] = { "card-public-info", "2", fixture_atr, "--pin-policy", fixture_aid };
		policy_fixture();
		assert(card_public_info_main(5, args) == 0 && docp_calls == 2 && select_calls == 2 &&
				!cert_calls && !pin_calls && !mse_calls && unlock_calls == 1);
		puts("PASS PIN policy sends only CHV 1 DOCP metadata queries in MF and application, without secret or component reads");
		policy_fixture(); docp_sw[0] = 0x6A88;
		assert(card_public_info_main(5, args) == 0 && docp_calls == 2 && unlock_calls == 1);
		policy_fixture(); docp_sw[0] = 0x6D00; docp_sw[1] = 0x6A82;
		assert(card_public_info_main(5, args) == 0 && docp_calls == 2);
		puts("PASS PIN policy records optional absence and probes no other references or templates");
		policy_fixture(); docp_sw[0] = 0x6982;
		assert(card_public_info_main(5, args) == 1 && docp_calls == 1 && select_calls == 1 && unlock_calls == 1);
		policy_fixture(); docp_sw[0] = 0x6C00;
		assert(card_public_info_main(5, args) == 1 && docp_calls == 1 && select_calls == 1);
		policy_fixture(); docp_result[0] = SC_ERROR_TRANSMIT_FAILED;
		assert(card_public_info_main(5, args) == 1 && docp_calls == 1 && unlock_calls == 1);
		policy_fixture(); docp_sw[1] = 0x6982;
		assert(card_public_info_main(5, args) == 1 && docp_calls == 2 && unlock_calls == 1);
		policy_fixture(); docp_len[0] = 257;
		assert(card_public_info_main(5, args) == 1 && docp_calls == 1 && unlock_calls == 1);
		puts("PASS PIN policy stops on security, retry, transport or response bounds errors and releases the transaction");
		policy_fixture(); mf_select_result = SC_ERROR_FILE_NOT_FOUND;
		assert(card_public_info_main(5, args) == 1 && !docp_calls && unlock_calls == 1);
		policy_fixture(); select_result = SC_ERROR_FILE_NOT_FOUND;
		assert(card_public_info_main(5, args) == 1 && docp_calls == 1 && unlock_calls == 1);
		puts("PASS PIN policy stops before querying a context when its SELECT fails");
		policy_fixture(); fixture_pin_info.attrs.pin.reference = 0x81;
		assert(card_public_info_main(5, args) == 1 && !select_calls && !docp_calls);
		policy_fixture(); fixture_pin_info.auth_id.value[0] ^= 1;
		assert(card_public_info_main(5, args) == 1 && !docp_calls);
		policy_fixture(); mismatch = 1;
		assert(card_public_info_main(5, args) == 1 && !bind_calls && !docp_calls);
		policy_fixture(); args[4] = "A001";
		assert(card_public_info_main(5, args) == 2 && !connect_calls);
		puts("PASS PIN policy requires the observed connected profile and linked global PIN metadata before GET DATA");
	}
	{
		char *args[] = { "card-public-info", "2", fixture_atr, "--odf-map", fixture_aid };
		reset_fixture(); odf_mode = 1;
		assert(card_public_info_main(5, args) == 0 && odf_select_calls == 1 && odf_read_calls == 1 &&
				!bind_calls && !object_calls && !cert_calls && !pin_calls && !mse_calls && unlock_calls == 1);
		reset_fixture(); odf_mode = 1; odf_read_sw = 0x6282;
		assert(card_public_info_main(5, args) == 0 && odf_read_calls == 1 && !bind_calls);
		puts("PASS ODF map reads only fixed 5031 once without binding, following paths or authenticating");
		for (int i = 0; i < 5; i++) {
			reset_fixture(); odf_mode = 1;
			if (i == 0) odf_select_result = SC_ERROR_READER;
			if (i == 1) odf_select_sw = 0x6A82;
			if (i == 2) odf_select_sw = 0x6101;
			if (i == 3) select_result = SC_ERROR_FILE_NOT_FOUND;
			if (i == 4) fixture_app.aid.value[0] ^= 1;
			assert(card_public_info_main(5, args) == 1 && !odf_read_calls && !bind_calls && unlock_calls == 1);
		}
		puts("PASS ODF map stops before READ after failed selection, pending response or absent announced application");
		for (int i = 0; i < 6; i++) {
			reset_fixture(); odf_mode = 1;
			if (i == 0) odf_read_result = SC_ERROR_READER;
			if (i == 1) odf_read_sw = 0x6982;
			if (i == 2) odf_read_sw = 0x6C10;
			if (i == 3) odf_size = 257;
			if (i == 4) odf_size = 0;
			if (i == 5) odf_size = sizeof(fixture_odf) - 1;
			assert(card_public_info_main(5, args) == 1 && odf_read_calls == 1 && !bind_calls && unlock_calls == 1);
		}
		puts("PASS ODF map rejects read errors, oversized and incomplete responses without retries or referenced reads");
		reset_fixture(); odf_mode = 1; mismatch = 1;
		assert(card_public_info_main(5, args) == 1 && !odf_select_calls && !odf_read_calls);
		reset_fixture(); odf_mode = 1; args[4] = "A001";
		assert(card_public_info_main(5, args) == 2 && !connect_calls);
		puts("PASS ODF map requires the known ATR and AID before any application query");
		{
			const unsigned char bad[][10] = {
				{ 0xA9, 6, 0x30, 4, 4, 2, 0x41, 0x41 },
				{ 0xA8, 7, 0x30, 4, 4, 2, 0x41, 0x41 },
				{ 0xA8, 6, 0xA0, 4, 4, 2, 0x41, 0x41 },
				{ 0xA8, 5, 0x30, 3, 4, 1, 0x41 },
				{ 0xA8, 6, 0x30, 4, 4, 2, 0x41, 0x41, 0, 1 }
			};
			const size_t lengths[] = { 8, 8, 8, 7, 10 };
			unsigned char padded[32] = {0}, excessive[33 * 8];
			memcpy(padded, fixture_odf, sizeof(fixture_odf));
			assert(print_odf_map(&fixture_ctx, padded, sizeof(padded)) == 0);
			for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++)
				assert(print_odf_map(&fixture_ctx, bad[i], lengths[i]) == 1);
			for (size_t i = 0; i < 33; i++) memcpy(excessive + i * 8, fixture_odf + 8, 8);
			assert(print_odf_map(&fixture_ctx, excessive, sizeof(excessive)) == 1);
			assert(print_odf_map(&fixture_ctx, padded, 0) == 1);
		}
		puts("PASS ODF parser accepts only bounded directory paths and uniform padding, rejecting inline objects and truncation");
	}
	{
		char *args[] = { "card-public-info", "2", fixture_atr, "--public-keys", fixture_aid };
		char output[4096] = {0};
		FILE *capture = tmpfile(); int saved = dup(STDOUT_FILENO);
		assert(capture && saved >= 0);
		reset_fixture(); fflush(stdout); assert(dup2(fileno(capture), STDOUT_FILENO) >= 0);
		assert(card_public_info_main(5, args) == 0 && pub_parse_calls == 1 && bind_calls == 1 &&
				p15_frees == 1 && unlock_calls == 1 && !object_calls && !cert_calls && !pin_calls && !mse_calls);
		fflush(stdout); rewind(capture); assert(fread(output, 1, sizeof(output)-1, capture) > 0);
		assert(dup2(saved, STDOUT_FILENO) >= 0); close(saved); fclose(capture);
		assert(strstr(output, "public_key_metadata_count=2") && strstr(output, "public_key_algorithm=RSA") &&
				strstr(output, "public_key_rsa_bits=3072") && strstr(output, "public_key_ec_bits=521"));
		assert(!strstr(output, "SECRET") && !strstr(output, "public_key_path") && !strstr(output, "public_key_id"));
		puts("PASS public-key inventory parses only PuKDF7004 and reports RSA/EC metadata without following key paths or exposing labels/IDs");
		reset_fixture(); pub_empty = 1;
		assert(card_public_info_main(5, args) == 0 && pub_parse_calls == 1 && !object_calls);
		reset_fixture(); pub_parse_result = SC_ERROR_READER;
		assert(card_public_info_main(5, args) == 1 && pub_parse_calls == 1 && p15_frees == 1 && unlock_calls == 1);
		puts("PASS public-key inventory accepts parsed empty directory but propagates directory errors rather than claiming empty success");
		for (int i = 0; i < 6; i++) {
			reset_fixture();
			if (i == 0) fixture_p15.df_list = NULL;
			if (i == 1) fixture_pub_df.path.value[1] = 5;
			if (i == 2) fixture_pub_df.path.len = 0;
			if (i == 3) fixture_pub_df.next = &fixture_pub_df;
			if (i == 4) fixture_pub_df.path.aid.len = 1;
			if (i == 5) mismatch = 1;
			assert(card_public_info_main(5, args) == 1 && !pub_parse_calls);
		}
		puts("PASS public-key inventory rejects absent, different, duplicate or external-AID directories and connected ATR mismatch before parsing");
		reset_fixture(); fixture_pub_ec_obj.data = NULL;
		assert(card_public_info_main(5, args) == 1 && pub_parse_calls == 1);
		reset_fixture(); fixture_pub_ec_obj.next = &fixture_pub_rsa_obj;
		assert(card_public_info_main(5, args) == 1 && pub_parse_calls == 1);
		reset_fixture(); bind_result = SC_ERROR_READER;
		assert(card_public_info_main(5, args) == 1 && !pub_parse_calls && p15_frees == 1 && unlock_calls == 1);
		puts("PASS public-key inventory bounds objects and cleans up on malformed metadata or binding error without crypto operations");
	}
	{
		char *args[] = { "card-public-info", "2", fixture_atr, "--certificate-paths", fixture_aid };
		char output[4096] = {0}; FILE *capture = tmpfile(); int saved = dup(STDOUT_FILENO);
		assert(capture && saved >= 0);
		reset_fixture(); fixture_p15.df_list = &fixture_cert_df;
		fflush(stdout); assert(dup2(fileno(capture), STDOUT_FILENO) >= 0);
		assert(card_public_info_main(5, args) == 0 && cdf_parse_calls == 1 && bind_calls == 1 &&
				p15_frees == 1 && unlock_calls == 1 && !object_calls && !cert_calls && !pin_calls && !mse_calls);
		fflush(stdout); rewind(capture); assert(fread(output, 1, sizeof(output)-1, capture) > 0);
		assert(dup2(saved, STDOUT_FILENO) >= 0); close(saved); fclose(capture);
		assert(strstr(output, "certificate_path=B001") && strstr(output, "certificate_path_metadata_count=1") &&
				strstr(output, "certificate_path_private=0") && strstr(output, "certificate_path_count=-1"));
		assert(!strstr(output, "SECRET") && !strstr(output, "certificate_id") && !strstr(output, "certificate_der"));
		puts("PASS certificate paths parse only CDF7005 and omit certificate data, labels and IDs without following references");
		reset_fixture(); fixture_p15.df_list = &fixture_cert_df; cdf_empty = 1;
		assert(card_public_info_main(5, args) == 0 && cdf_parse_calls == 1 && !object_calls);
		reset_fixture(); fixture_p15.df_list = &fixture_cert_df; cdf_parse_result = SC_ERROR_INVALID_ASN1_OBJECT;
		assert(card_public_info_main(5, args) == 1 && cdf_parse_calls == 1 && unlock_calls == 1 && p15_frees == 1);
		puts("PASS CDF paths distinguish a parsed empty directory from parse failure and clean up without fallback");
		for (int i = 0; i < 8; i++) {
			reset_fixture(); fixture_p15.df_list = &fixture_cert_df;
			if (i == 0) fixture_p15.df_list = NULL;
			if (i == 1) fixture_cert_df.path.value[1] = 4;
			if (i == 2) fixture_cert_df.path.len = 0;
			if (i == 3) fixture_cert_df.next = &fixture_cert_df;
			if (i == 4) fixture_cert_df.path.aid.len = 1;
			if (i == 5) fixture_cert_df.path.type = SC_PATH_TYPE_DF_NAME;
			if (i == 6) mismatch = 1;
			if (i == 7) bind_result = SC_ERROR_READER;
			assert(card_public_info_main(5, args) == 1 && !cdf_parse_calls && !cert_calls && !pin_calls && !mse_calls);
		}
		puts("PASS CDF paths reject absent/different/duplicate/external/invalid directories, ATR mismatch and binding errors before parsing");
		for (int i = 0; i < 9; i++) {
			reset_fixture(); fixture_p15.df_list = &fixture_cert_df;
			if (i == 0) fixture_cert_obj.data = NULL;
			if (i == 1) fixture_cert_obj.type = SC_PKCS15_TYPE_PRKEY_RSA;
			if (i == 2) fixture_cert_info.path.len = 1;
			if (i == 3) fixture_cert_info.path.len = sizeof(fixture_cert_info.path.value) + 2;
			if (i == 4) fixture_cert_info.path.type = SC_PATH_TYPE_DF_NAME;
			if (i == 5) fixture_cert_info.path.index = -1;
			if (i == 6) fixture_cert_info.path.count = -2;
			if (i == 7) fixture_cert_info.path.aid.len = 1;
			if (i == 8) fixture_cert_obj.next = &fixture_cert_obj;
			assert(card_public_info_main(5, args) == 1 && cdf_parse_calls == 1 && !cert_calls && !pin_calls && !mse_calls);
		}
		puts("PASS CDF paths validate bounded objects/routes/ranges/AID and reject malformed metadata before emitting routes");
		reset_fixture(); fixture_p15.df_list = &fixture_cert_df;
		fixture_cert_obj.flags = SC_PKCS15_CO_FLAG_PRIVATE;
		fixture_cert_info.path.aid = fixture_app.aid;
		fixture_cert_df.path.len = 4; memcpy(fixture_cert_df.path.value, "\x3F\x00\x70\x05", 4);
		assert(card_public_info_main(5, args) == 0 && cdf_parse_calls == 1 && !cert_calls);
		puts("PASS CDF paths preserve private flags and accept matching AID and MF-prefixed directory without reading certificates");
		reset_fixture(); fixture_p15.df_list = &fixture_cert_df;
		fixture_cert_info.path.len = 0;
		fixture_cert_info.value.value = (unsigned char *)"SECRET_INLINE_DER";
		fixture_cert_info.value.len = 17;
		assert(card_public_info_main(5, args) == 0 && !cert_calls);
		cdf_parse_calls = 0; fixture_cert_info.value.value = NULL;
		assert(inspect_certificate_paths(&fixture_p15, &fixture_app.aid) == 1);
		puts("PASS inline certificates are classified without printing contents and invalid inline storage is rejected");
		reset_fixture(); args[4] = "A001";
		assert(card_public_info_main(5, args) == 2 && !connect_calls);
		puts("PASS certificate paths require the known AID before connecting to hardware");
	}
	{
		char *args[] = { "card-public-info", "2", fixture_atr, "--file7001", fixture_aid };
		reset_fixture(); file4141_mode = 3;
		assert(card_public_info_main(5, args) == 0 && file4141_calls == 1 && unlock_calls == 1 &&
				!bind_calls && !object_calls && !cert_calls && !pin_calls && !mse_calls && !odf_read_calls);
		reset_fixture(); file4141_mode = 3; file4141_wrongfid = 1;
		assert(card_public_info_main(5, args) == 1 && file4141_calls == 1 && unlock_calls == 1);
		reset_fixture(); file4141_mode = 3; mismatch = 1;
		assert(card_public_info_main(5, args) == 1 && !file4141_calls);
		puts("PASS AODF7001 SELECT-only requires connected ATR, sends exact FID and rejects other returned FID without READ or binding");
	}
	{
		char *args[] = { "card-public-info", "2", fixture_atr, "--file7002", fixture_aid };
		reset_fixture(); file4141_mode = 4;
		assert(card_public_info_main(5, args) == 0 && file4141_calls == 1 && unlock_calls == 1 &&
				!bind_calls && !object_calls && !cert_calls && !pin_calls && !mse_calls && !odf_read_calls);
		reset_fixture(); file4141_mode = 4; file4141_wrongfid = 1;
		assert(card_public_info_main(5, args) == 1 && file4141_calls == 1 && unlock_calls == 1);
		reset_fixture(); file4141_mode = 4; mismatch = 1;
		assert(card_public_info_main(5, args) == 1 && !file4141_calls);
		puts("PASS PrKDF7002 SELECT-only requires connected ATR, sends exact FID and rejects other returned FID without READ or binding");
	}
	{
		char *args[] = { "card-public-info", "2", fixture_atr, "--file7004", fixture_aid };
		reset_fixture(); file4141_mode = 5;
		assert(card_public_info_main(5, args) == 0 && file4141_calls == 1 && unlock_calls == 1 &&
				!bind_calls && !object_calls && !cert_calls && !pin_calls && !mse_calls && !odf_read_calls);
		reset_fixture(); file4141_mode = 5; file4141_wrongfid = 1;
		assert(card_public_info_main(5, args) == 1 && file4141_calls == 1 && unlock_calls == 1);
		reset_fixture(); file4141_mode = 5; mismatch = 1;
		assert(card_public_info_main(5, args) == 1 && !file4141_calls);
		puts("PASS PuKDF7004 SELECT-only requires connected ATR, sends exact FID and rejects other returned FID without READ or binding");
	}
	{
		char *args[] = { "card-public-info", "2", fixture_atr, "--file7005", fixture_aid };
		reset_fixture(); file4141_mode = 6;
		assert(card_public_info_main(5, args) == 0 && file4141_calls == 1 && unlock_calls == 1 &&
				!bind_calls && !object_calls && !cert_calls && !pin_calls && !mse_calls && !odf_read_calls);
		reset_fixture(); file4141_mode = 6; file4141_wrongfid = 1;
		assert(card_public_info_main(5, args) == 1 && file4141_calls == 1 && unlock_calls == 1);
		reset_fixture(); file4141_mode = 6; mismatch = 1;
		assert(card_public_info_main(5, args) == 1 && !file4141_calls);
		puts("PASS CDF7005 SELECT-only requires connected ATR, sends exact FID and rejects other returned FID without READ or binding");
	}
	{
		char *args[] = { "card-public-info", "2", fixture_atr, "--fileb002", fixture_aid };
		reset_fixture(); file4141_mode = 9;
		assert(card_public_info_main(5, args) == 0 && file4141_calls == 1 && unlock_calls == 1 &&
				!bind_calls && !object_calls && !cert_calls && !pin_calls && !mse_calls && !odf_read_calls);
		reset_fixture(); file4141_mode = 9; file4141_wrongfid = 1;
		assert(card_public_info_main(5, args) == 1 && file4141_calls == 1 && unlock_calls == 1);
		reset_fixture(); file4141_mode = 9; mismatch = 1;
		assert(card_public_info_main(5, args) == 1 && !file4141_calls);
		puts("PASS B002 SELECT-only requires connected ATR, sends exact FID and rejects other returned FID without READ or binding");
		reset_fixture(); file4141_mode = 9; file4141_sw = 0x6A82;
		assert(card_public_info_main(5, args) == 0 && file4141_calls == 1 && unlock_calls == 1);
		for (int i = 0; i < 4; i++) {
			reset_fixture(); file4141_mode = 9;
			if (i == 0) file4141_transport = SC_ERROR_READER;
			if (i == 1) file4141_sw = 0x6982;
			if (i == 2) file4141_sw = 0x6101;
			if (i == 3) file4141_size = sizeof(fixture_fcp) - 1;
			assert(card_public_info_main(5, args) == 1 && file4141_calls == 1 && unlock_calls == 1 &&
					!bind_calls && !pin_calls && !mse_calls && !odf_read_calls);
		}
		puts("PASS B002 treats 6A82 as a context-specific diagnostic and stops on status/transport/malformed data without follow-up");
	}
	{
		char *args[] = { "card-public-info", "2", fixture_atr, "--dh-parameters", fixture_aid };
		EC_GROUP *group = EC_GROUP_new_by_curve_name(NID_secp521r1);
		assert(group);
		memset(fixture_dh, 0, sizeof(fixture_dh)); fixture_dh[2] = 18;
		assert(EC_POINT_point2oct(group, EC_GROUP_get0_generator(group), POINT_CONVERSION_UNCOMPRESSED,
				fixture_dh + 4, sizeof(fixture_dh) - 4, NULL) == 133);
		EC_GROUP_free(group);
		reset_fixture(); dh_mode = 1; file4141_mode = 10;
		assert(card_public_info_main(5, args) == 0 && file4141_calls == 1 && dh_calls == 1 && unlock_calls == 1 &&
				!pin_calls && !bind_calls && !mse_calls && !object_calls && !cert_calls);
		puts("PASS D004 parameter probe makes one exact 137-byte READ after checked SELECT, validates a synthetic P521 point without authentication");
		for (int i = 0; i < 6; i++) {
			reset_fixture(); dh_mode = 1; file4141_mode = 10;
			if (i == 0) file4141_sw = 0x6A82;
			if (i == 1) file4141_wrongfid = 1;
			if (i == 2) file4141_size = sizeof(fixture_fcp) - 1;
			if (i == 3) mismatch = 1;
			if (i == 4) select_result = SC_ERROR_READER;
			if (i == 5) file4141_transport = SC_ERROR_READER;
			assert(card_public_info_main(5, args) == 1 && !dh_calls && !pin_calls && !bind_calls && !mse_calls);
		}
		puts("PASS D004 parameter probe never reads after failed/missing/malformed selection or mismatched card");
		for (int i = 0; i < 7; i++) {
			reset_fixture(); dh_mode = 1; file4141_mode = 10;
			if (i == 0) dh_transport = SC_ERROR_READER;
			if (i == 1) dh_sw = 0x6982;
			if (i == 2) dh_sw = 0x6101;
			if (i == 3) dh_sw = 0x6282;
			if (i == 4) dh_size = 136;
			if (i == 5) dh_size = 138;
			if (i == 6) fixture_dh[2] = 17;
			assert(card_public_info_main(5, args) == 1 && dh_calls == 1 && unlock_calls == 1 &&
					!pin_calls && !bind_calls && !mse_calls);
		}
		fixture_dh[2] = 18;
		puts("PASS D004 parameter probe stops on READ errors, continuation, wrong size or unknown format without a second read");
		{
			unsigned char control[sizeof(fixture_fcp)], duplicate[sizeof(fixture_fcp) + 4];
			memcpy(control, fixture_fcp, sizeof(control));
			control[7] = 0xD0; control[8] = 4; control[12] = 137;
			assert(d004_control_matches_observation(control, sizeof(control)));
			for (int i = 0; i < 4; i++) {
				unsigned char invalid[sizeof(control)]; memcpy(invalid, control, sizeof(invalid));
				if (i == 0) invalid[4] = 0x38;
				if (i == 1) invalid[12] = 136;
				if (i == 2) invalid[7] = 0x41;
				if (i == 3) invalid[9] = 0x85;
				assert(!d004_control_matches_observation(invalid, sizeof(invalid)));
			}
			memcpy(duplicate, control, sizeof(control)); duplicate[1] += 4;
			memcpy(duplicate + sizeof(control), control + 5, 4);
			assert(!d004_control_matches_observation(duplicate, sizeof(duplicate)));
			assert(!d004_control_matches_observation(NULL, 0));
			puts("PASS D004 control matching rejects absent/duplicate identity, other structure, size or malformed bounds without interpreting security attributes");
		}
		{
			unsigned char invalid[sizeof(fixture_dh)];
			memcpy(invalid, fixture_dh, sizeof(invalid)); invalid[4] = 0;
			assert(summarize_d004_parameters(invalid, sizeof(invalid)) == 1);
			memset(invalid + 5, 0, sizeof(invalid) - 5); invalid[4] = 4;
			assert(summarize_d004_parameters(invalid, sizeof(invalid)) == 1);
			assert(summarize_d004_parameters(fixture_dh, 136) == 1);
			puts("PASS D004 parser rejects invalid point encodings, off-curve coordinates and truncated input");
		}
		{
			char output[512] = {0}, point_hex[33]; FILE *capture = tmpfile(); int saved = dup(STDOUT_FILENO);
			assert(capture && saved >= 0);
			for (int i = 0; i < 16; i++) sprintf(point_hex + i * 2, "%02X", fixture_dh[i + 4]);
			fflush(stdout); assert(dup2(fileno(capture), STDOUT_FILENO) >= 0);
			assert(summarize_d004_parameters(fixture_dh, sizeof(fixture_dh)) == 0);
			fflush(stdout); rewind(capture); assert(fread(output, 1, sizeof(output) - 1, capture) > 0);
			assert(dup2(saved, STDOUT_FILENO) >= 0); close(saved); fclose(capture);
			assert(!strcmp(output, "dh_layout_size=expected_137\ndh_curve_selector=matches_p521_candidate\ndh_point_prefix=matches_uncompressed_candidate\ndh_parameters_curve=secp521r1\ndh_parameters_point=valid_non_infinity\ndh_parameters_result=recognized_point_only_no_channel\n"));
			assert(!strstr(output, point_hex));
			puts("PASS D004 summary emits exclusively fixed curve/validation labels and no point, header, hash or identifying bytes");
		}
		for (int failure = 0; failure < 3; failure++) {
			unsigned char invalid[sizeof(fixture_dh)];
			char output[512] = {0}; FILE *capture = tmpfile(); int saved = dup(STDOUT_FILENO);
			assert(capture && saved >= 0);
			memcpy(invalid, fixture_dh, sizeof(invalid));
			if (failure != 1) invalid[2] = 0xAA;
			if (failure != 0) invalid[4] = 0xBB;
			fflush(stdout); assert(dup2(fileno(capture), STDOUT_FILENO) >= 0);
			assert(summarize_d004_parameters(invalid, sizeof(invalid)) == 1);
			fflush(stdout); rewind(capture); assert(fread(output, 1, sizeof(output) - 1, capture) > 0);
			assert(dup2(saved, STDOUT_FILENO) >= 0); close(saved); fclose(capture);
			assert(strstr(output, "dh_layout_size=expected_137\n"));
			assert(strstr(output, failure == 1 ? "dh_curve_selector=matches_p521_candidate\n" : "dh_curve_selector=different_uninterpreted\n"));
			assert(strstr(output, failure == 0 ? "dh_point_prefix=matches_uncompressed_candidate\n" : "dh_point_prefix=different_uninterpreted\n"));
			assert(strstr(output, "dh_parameters_point=not_checked_due_to_layout\n"));
			assert(strstr(output, "dh_parameters_result=unrecognized_layout_stop\n"));
			assert(!strstr(output, "secp521r1") && !strstr(output, "valid_non_infinity") && !strstr(output, "AA") && !strstr(output, "BB"));
		}
		puts("PASS D004 layout failure distinguishes either or both mismatches without exposing values or accepting another format");
		{
			char output[256] = {0}; FILE *capture = tmpfile(); int saved = dup(STDOUT_FILENO);
			assert(capture && saved >= 0);
			fflush(stdout); assert(dup2(fileno(capture), STDOUT_FILENO) >= 0);
			assert(summarize_d004_parameters(NULL, 137) == 1);
			assert(summarize_d004_parameters(fixture_dh, 2) == 1);
			fflush(stdout); rewind(capture); assert(fread(output, 1, sizeof(output) - 1, capture) > 0);
			assert(dup2(saved, STDOUT_FILENO) >= 0); close(saved); fclose(capture);
			assert(!strstr(output, "dh_curve_selector=") && !strstr(output, "dh_point_prefix="));
			assert(strstr(output, "dh_layout_size=unexpected_or_missing_stop\n"));
			puts("PASS D004 layout diagnostic avoids reading or classifying absent and truncated input");
		}
	}
	{
		char *args[] = { "card-public-info", "2", fixture_atr, "--filed004", fixture_aid };
		reset_fixture(); file4141_mode = 10;
		assert(card_public_info_main(5, args) == 0 && file4141_calls == 1 && unlock_calls == 1 &&
				!bind_calls && !object_calls && !cert_calls && !pin_calls && !mse_calls && !odf_read_calls);
		reset_fixture(); file4141_mode = 10; file4141_wrongfid = 1;
		assert(card_public_info_main(5, args) == 1 && file4141_calls == 1 && unlock_calls == 1);
		reset_fixture(); file4141_mode = 10; mismatch = 1;
		assert(card_public_info_main(5, args) == 1 && !file4141_calls);
		puts("PASS D004 SELECT-only requires connected ATR, sends exact FID and rejects other returned FID without READ or binding");
		reset_fixture(); file4141_mode = 10; file4141_sw = 0x6A82;
		assert(card_public_info_main(5, args) == 0 && file4141_calls == 1 && unlock_calls == 1);
		for (int i = 0; i < 4; i++) {
			reset_fixture(); file4141_mode = 10;
			if (i == 0) file4141_transport = SC_ERROR_READER;
			if (i == 1) file4141_sw = 0x6982;
			if (i == 2) file4141_sw = 0x6101;
			if (i == 3) file4141_size = sizeof(fixture_fcp) - 1;
			assert(card_public_info_main(5, args) == 1 && file4141_calls == 1 && unlock_calls == 1 &&
					!bind_calls && !pin_calls && !mse_calls && !odf_read_calls);
		}
		puts("PASS D004 treats 6A82 as a context-specific diagnostic and stops on status/transport/malformed data without follow-up");
		reset_fixture(); file4141_mode = 10; fixture_app.aid.value[0] ^= 1;
		assert(card_public_info_main(5, args) == 1 && !select_calls && !file4141_calls && !bind_calls && unlock_calls == 1);
		fixture_app.aid.value[0] ^= 1;
		reset_fixture(); file4141_mode = 10; select_result = SC_ERROR_READER;
		assert(card_public_info_main(5, args) == 1 && select_calls == 1 && !file4141_calls && !bind_calls && unlock_calls == 1);
		reset_fixture(); file4141_mode = 10; args[4] = "A000000001";
		assert(card_public_info_main(5, args) == 2 && !connect_calls && !file4141_calls);
		puts("PASS D004 rejects an unannounced or unknown AID and failed application selection before the EF command");
	}
	{
		char *args[] = { "card-public-info", "2", fixture_atr, "--fileb001", fixture_aid };
		reset_fixture(); file4141_mode = 8;
		assert(card_public_info_main(5, args) == 0 && file4141_calls == 1 && unlock_calls == 1 &&
				!bind_calls && !object_calls && !cert_calls && !pin_calls && !mse_calls && !odf_read_calls);
		reset_fixture(); file4141_mode = 8; file4141_wrongfid = 1;
		assert(card_public_info_main(5, args) == 1 && file4141_calls == 1 && unlock_calls == 1);
		reset_fixture(); file4141_mode = 8; mismatch = 1;
		assert(card_public_info_main(5, args) == 1 && !file4141_calls);
		puts("PASS B001 SELECT-only requires connected ATR, sends exact FID and rejects other returned FID without READ or binding");
		reset_fixture(); file4141_mode = 8; file4141_sw = 0x6A82;
		assert(card_public_info_main(5, args) == 0 && file4141_calls == 1 && unlock_calls == 1);
		for (int i = 0; i < 4; i++) {
			reset_fixture(); file4141_mode = 8;
			if (i == 0) file4141_transport = SC_ERROR_READER;
			if (i == 1) file4141_sw = 0x6982;
			if (i == 2) file4141_sw = 0x6101;
			if (i == 3) file4141_size = sizeof(fixture_fcp) - 1;
			assert(card_public_info_main(5, args) == 1 && file4141_calls == 1 && unlock_calls == 1 &&
					!bind_calls && !pin_calls && !mse_calls && !odf_read_calls);
		}
		puts("PASS B001 treats 6A82 as a context-specific diagnostic and stops on status/transport/malformed data without follow-up");
	}
	{
		char *args[] = { "card-public-info", "2", fixture_atr, "--filedf0f", fixture_aid };
		reset_fixture(); file4141_mode = 7;
		assert(card_public_info_main(5, args) == 0 && file4141_calls == 1 && unlock_calls == 1 &&
				!bind_calls && !object_calls && !cert_calls && !pin_calls && !mse_calls && !odf_read_calls);
		reset_fixture(); file4141_mode = 7; file4141_wrongfid = 1;
		assert(card_public_info_main(5, args) == 1 && file4141_calls == 1 && unlock_calls == 1);
		reset_fixture(); file4141_mode = 7; mismatch = 1;
		assert(card_public_info_main(5, args) == 1 && !file4141_calls);
		puts("PASS DF0F SELECT-only requires connected ATR, sends exact FID and rejects other returned FID without READ or binding");
		reset_fixture(); file4141_mode = 7; file4141_sw = 0x6A82;
		assert(card_public_info_main(5, args) == 0 && file4141_calls == 1 && unlock_calls == 1);
		for (int i = 0; i < 4; i++) {
			reset_fixture(); file4141_mode = 7;
			if (i == 0) file4141_transport = SC_ERROR_READER;
			if (i == 1) file4141_sw = 0x6982;
			if (i == 2) file4141_sw = 0x6101;
			if (i == 3) file4141_size = sizeof(fixture_fcp) - 1;
			assert(card_public_info_main(5, args) == 1 && file4141_calls == 1 && unlock_calls == 1 &&
					!bind_calls && !pin_calls && !mse_calls && !odf_read_calls);
		}
		puts("PASS DF0F treats 6A82 as a context-specific diagnostic and stops on status/transport/malformed data without follow-up");
	}
	{
		char *args[] = { "card-public-info", "2", fixture_atr, "--file4109", fixture_aid };
		reset_fixture(); file4141_mode = 2;
		assert(card_public_info_main(5, args) == 0 && file4141_calls == 1 && unlock_calls == 1 &&
				!bind_calls && !object_calls && !cert_calls && !pin_calls && !mse_calls && !odf_read_calls);
		reset_fixture(); file4141_mode = 2; file4141_size = 0;
		assert(card_public_info_main(5, args) == 0 && file4141_calls == 1 && unlock_calls == 1);
		reset_fixture(); file4141_mode = 2; file4141_sw = 0x6A82;
		assert(card_public_info_main(5, args) == 0 && file4141_calls == 1 && unlock_calls == 1);
		puts("PASS SELECT-only 4109 sends its exact FID and accepts empty metadata or missing file without READ or authentication");
		for (int i = 0; i < 7; i++) {
			reset_fixture(); file4141_mode = 2;
			if (i == 0) file4141_transport = SC_ERROR_READER;
			if (i == 1) file4141_sw = 0x6982;
			if (i == 2) file4141_sw = 0x6101;
			if (i == 3) file4141_sw = 0x6C10;
			if (i == 4) file4141_size = 257;
			if (i == 5) file4141_size = sizeof(fixture_fcp) - 1;
			if (i == 6) file4141_wrongfid = 1;
			assert(card_public_info_main(5, args) == 1 && file4141_calls == 1 && unlock_calls == 1 && !bind_calls);
		}
		puts("PASS SELECT-only 4109 rejects wrong returned FID, malformed data, retry requests and transport/security errors without follow-up");
		for (int i = 0; i < 3; i++) {
			reset_fixture(); file4141_mode = 2;
			if (i == 0) mismatch = 1;
			if (i == 1) fixture_app.aid.value[0] ^= 1;
			if (i == 2) select_result = SC_ERROR_FILE_NOT_FOUND;
			assert(card_public_info_main(5, args) == 1 && !file4141_calls && !bind_calls);
		}
		reset_fixture(); file4141_mode = 2; args[4] = "A001";
		assert(card_public_info_main(5, args) == 2 && !connect_calls);
		puts("PASS SELECT-only 4109 requires connected ATR and announced known application; no arbitrary-file CLI accepted");
		{
			unsigned char fci[sizeof(fixture_fcp)];
			memcpy(fci, fixture_fcp, sizeof(fci)); fci[0] = 0x6F; fci[8] = 0x09;
			assert(print_file_metadata(fci, sizeof(fci), 0x4109) == 0);
			assert(print_file_metadata(fci, sizeof(fci), 0x4141) == 1);
			assert(print_file_metadata(fci, sizeof(fci), 0x4108) == 1);
			reset_fixture(); args[3] = "--file4108"; args[4] = fixture_aid;
			assert(card_public_info_main(5, args) == 2 && !connect_calls);
		}
		puts("PASS shared metadata parser keeps fixed-file identity distinct and refuses unsupported FIDs");
	}
	{
		char *args[] = { "card-public-info", "2", fixture_atr, "--file4141", fixture_aid };
		reset_fixture(); file4141_mode = 1;
		assert(card_public_info_main(5, args) == 0 && file4141_calls == 1 && unlock_calls == 1 &&
				!bind_calls && !object_calls && !cert_calls && !pin_calls && !mse_calls && !odf_read_calls);
		reset_fixture(); file4141_mode = 1; file4141_size = 0;
		assert(card_public_info_main(5, args) == 0 && file4141_calls == 1 && unlock_calls == 1);
		reset_fixture(); file4141_mode = 1; file4141_sw = 0x6A82;
		assert(card_public_info_main(5, args) == 0 && file4141_calls == 1 && unlock_calls == 1);
		puts("PASS fixed-file query sends only SELECT 4141 and treats missing control data or 6A82 as diagnostic results without READ");
		for (int i = 0; i < 6; i++) {
			reset_fixture(); file4141_mode = 1;
			if (i == 0) file4141_transport = SC_ERROR_READER;
			if (i == 1) file4141_sw = 0x6982;
			if (i == 2) file4141_sw = 0x6101;
			if (i == 3) file4141_sw = 0x6C10;
			if (i == 4) file4141_size = 257;
			if (i == 5) file4141_size = sizeof(fixture_fcp) - 1;
			assert(card_public_info_main(5, args) == 1 && file4141_calls == 1 && unlock_calls == 1 && !bind_calls);
		}
		puts("PASS fixed-file query stops on security, transport, pending response, retry, bounds or malformed control data without follow-up");
		for (int i = 0; i < 3; i++) {
			reset_fixture(); file4141_mode = 1;
			if (i == 0) mismatch = 1;
			if (i == 1) fixture_app.aid.value[0] ^= 1;
			if (i == 2) select_result = SC_ERROR_FILE_NOT_FOUND;
			assert(card_public_info_main(5, args) == 1 && !file4141_calls && !bind_calls);
		}
		reset_fixture(); file4141_mode = 1; args[4] = "A001";
		assert(card_public_info_main(5, args) == 2 && !connect_calls);
		puts("PASS fixed-file query requires the connected ATR, announced known AID and successful application selection");
		{
			const unsigned char bad[][16] = {
				{ 0x62, 4, 0x83, 2, 0x41, 0x42 },
				{ 0x62, 3, 0x83, 1, 0x41 },
				{ 0x62, 2, 0x82, 0 },
				{ 0x62, 6, 0x82, 1, 1, 0x82, 1, 2 },
				{ 0x62, 6, 0x80, 1, 1, 0x80, 1, 2 },
				{ 0x62, 2, 0x80, 0 },
				{ 0x62, 7, 0x80, 5, 0, 0, 0, 0, 1 },
				{ 0x62, 4, 0x83, 2, 0x41 },
				{ 0x62, 0, 1 },
				{ 0x61, 0 }
			};
			const size_t lengths[] = { 6, 5, 4, 8, 8, 4, 9, 5, 3, 2 };
			unsigned char fci[sizeof(fixture_fcp)];
			memcpy(fci, fixture_fcp, sizeof(fci)); fci[0] = 0x6F;
			assert(print_file4141_metadata(fci, sizeof(fci)) == 0);
			for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++)
				assert(print_file4141_metadata(bad[i], lengths[i]) == 1);
			puts("PASS fixed-file metadata parser accepts FCP or FCI and rejects wrong FID, contradictions, truncation or trailing data");
		}
		{
			/* Synthetic proprietary/name/ACL fields must not be echoed. */
			const unsigned char private_fields[] = { 0x62, 15, 0x84, 3, 'N', 'A', 'M', 0x85, 3, 'S', 'E', 'C', 0x8C, 3, 'A', 'C', 'L' };
			char output[256] = {0};
			FILE *capture = tmpfile();
			int saved = dup(STDOUT_FILENO);
			assert(capture && saved >= 0);
			fflush(stdout); assert(dup2(fileno(capture), STDOUT_FILENO) >= 0);
			assert(print_file4141_metadata(private_fields, sizeof(private_fields)) == 0);
			fflush(stdout); rewind(capture);
			assert(fread(output, 1, sizeof(output) - 1, capture) > 0);
			assert(dup2(saved, STDOUT_FILENO) >= 0); close(saved); fclose(capture);
			assert(!strcmp(output, "file4141_security_format=compact\nfile4141_security_direct_tags=8C\nfile4141_standard_compact_read=uninterpreted\nfile4141_result=selected_metadata_only_no_read\n"));
			puts("PASS fixed-file metadata omits names, proprietary fields, access data and raw responses");
		}
		{
			enum compact_read summary;
			const unsigned char read[] = { 1, 0 }, update_read[] = { 3, 0x10, 0 };
			const unsigned char later[] = { 2, 0x10, 1, 0 }, duplicate[] = { 1, 0, 1, 0 };
			const unsigned char nonzero[] = { 1, 0xFF }, alternatives[] = { 1, 0x10, 1, 0 };
			const unsigned char proprietary[] = { 0x81, 0 }, absent[] = { 2, 0 };
			assert(!summarize_compact_read(read, sizeof(read), &summary) && summary == COMPACT_ALWAYS);
			assert(!summarize_compact_read(update_read, sizeof(update_read), &summary) && summary == COMPACT_ALWAYS);
			assert(!summarize_compact_read(later, sizeof(later), &summary) && summary == COMPACT_ALWAYS);
			assert(!summarize_compact_read(duplicate, sizeof(duplicate), &summary) && summary == COMPACT_ALWAYS);
			assert(!summarize_compact_read(nonzero, sizeof(nonzero), &summary) && summary == COMPACT_OTHER);
			assert(!summarize_compact_read(alternatives, sizeof(alternatives), &summary) && summary == COMPACT_ALTERNATIVES);
			assert(!summarize_compact_read(proprietary, sizeof(proprietary), &summary) && summary == COMPACT_UNSUPPORTED);
			assert(!summarize_compact_read(absent, sizeof(absent), &summary) && summary == COMPACT_ABSENT);
			puts("PASS compact security summary locates READ in all standard groups and does not resolve alternative or proprietary conditions");
			{
				const unsigned char truncated[] = { 3, 0x10 }, zero[] = { 0 };
				const unsigned char malformed_fcp[] = { 0x62, 6, 0x82, 1, 1, 0x8C, 1, 1 };
				assert(summarize_compact_read(read, 0, &summary));
				assert(summarize_compact_read(read, 1, &summary));
				assert(summarize_compact_read(truncated, sizeof(truncated), &summary));
				assert(summarize_compact_read(zero, sizeof(zero), &summary));
				assert(print_file4141_metadata(malformed_fcp, sizeof(malformed_fcp)) == 1);
			}
			puts("PASS compact security summary rejects empty or truncated standard groups before emitting a condition");
		}
		{
			const unsigned char compact_fcp[] = { 0x62, 8, 0x82, 1, 1, 0x8C, 3, 3, 0x10, 0 };
			const unsigned char other[][16] = {
				{ 0x62, 4, 0x8B, 2, 0x12, 0x34 },
				{ 0x62, 3, 0xAB, 1, 0 },
				{ 0x62, 4, 0xA1, 2, 0x12, 0x34 },
				{ 0x62, 11, 0x82, 1, 1, 0x8C, 2, 1, 0, 0x9C, 2, 1, 0 },
				{ 0x6F, 9, 0x62, 7, 0x82, 1, 1, 0x8C, 2, 1, 0 },
				{ 0x62, 11, 0x82, 1, 1, 0x8C, 2, 1, 0, 0x8C, 2, 1, 0 }
			};
			const size_t sizes[] = { 6, 5, 6, 13, 11, 13 };
			expect_metadata_summary(compact_fcp, sizeof(compact_fcp), "file4141_standard_compact_read=always\n");
			for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
				expect_metadata_summary(other[i], sizes[i], "file4141_standard_compact_read=uninterpreted\n");
			puts("PASS control metadata distinguishes direct standard compact security from referenced, expanded, profile, nested or multiple forms");
			{
				unsigned char descriptor[sizeof(compact_fcp)];
				const unsigned char variants[] = { 0x38, 0x21, 0x81, 2 };
				for (size_t i = 0; i < sizeof(variants); i++) {
					memcpy(descriptor, compact_fcp, sizeof(descriptor)); descriptor[4] = variants[i];
					expect_metadata_summary(descriptor, sizeof(descriptor), "file4141_standard_compact_read=uninterpreted\n");
				}
				const unsigned char no_descriptor[] = { 0x62, 4, 0x8C, 2, 1, 0 };
				const unsigned char opaque[] = { 0x62, 17, 0x82, 1, 1, 0x8C, 2, 1, 0, 0x85, 6, 'S','E','C','R','E','T', 0x84, 0 };
				expect_metadata_summary(no_descriptor, sizeof(no_descriptor), "file4141_standard_compact_read=uninterpreted\n");
				expect_metadata_summary(opaque, sizeof(opaque), "file4141_standard_compact_read=always\n");
			}
			puts("PASS READ summaries require an ordinary transparent EF and omit unrelated names, proprietary values and security bytes");
		}
		{
			const unsigned char direct86[] = { 0x62, 2, 0x86, 0 };
			const unsigned char direct9c[] = { 0x62, 2, 0x9C, 0 };
			const unsigned char nested[] = { 0x62, 14, 0x82, 1, 1, 0xA1, 9, 0x8C, 3, 3, 0x10, 0, 0x9C, 2, 1, 0xFF };
			expect_metadata_summary(direct86, sizeof(direct86), "file4141_security_direct_tags=86\n");
			expect_metadata_summary(direct9c, sizeof(direct9c), "file4141_security_direct_tags=9C\n");
			expect_metadata_summary(nested, sizeof(nested), "file4141_security_direct_tags=A1\n");
			expect_metadata_summary(nested, sizeof(nested), "file4141_a1_security_tags=8C,9C\n");
			expect_metadata_summary(nested, sizeof(nested), "file4141_a1_8c_read_encoding=always\n");
			expect_metadata_summary(nested, sizeof(nested), "file4141_standard_compact_read=uninterpreted\n");
			puts("PASS profile tags are distinguished and nested compact encoding never becomes a direct or global READ policy");
			{
				const unsigned char private_nested[] = { 0x62, 17, 0x82, 1, 1, 0xA1, 12, 0x8C, 2, 1, 0x42, 0x85, 6, 'S', 'E', 'C', 'R', 'E', 'T' };
				expect_metadata_summary(private_nested, sizeof(private_nested), "file4141_a1_other_tags=present_values_omitted\n");
				expect_metadata_summary(private_nested, sizeof(private_nested), "file4141_a1_8c_read_encoding=nonzero_uninterpreted\n");
				puts("PASS nested proprietary values and condition references are omitted from the report");
			}
			{
				const unsigned char opaque[] = { 0x62, 9, 0x82, 1, 1, 0xA1, 4, 0x8C, 2, 1, 0xA1 };
				unsigned char malformed[sizeof(opaque)];
				const unsigned char multiple[] = { 0x62, 11, 0x82, 1, 1, 0xA1, 2, 0x8C, 0, 0xA1, 2, 0x9C, 0 };
				memcpy(malformed, opaque, sizeof(malformed));
				/* Valid outer A1, but its nested TLV runs past the container. */
				malformed[8] = 3;
				expect_metadata_summary(malformed, sizeof(malformed), "file4141_a1_structure=opaque_uninterpreted\n");
				expect_metadata_summary(multiple, sizeof(multiple), "file4141_a1_structure=multiple_uninterpreted\n");
				puts("PASS malformed and repeated A1 containers do not expose partial tag or condition evidence");
			}
			{
				const unsigned char malformed_compact[] = { 0x62, 8, 0x82, 1, 1, 0xA1, 3, 0x8C, 1, 1 };
				const unsigned char repeated[] = { 0x62, 13, 0x82, 1, 1, 0xA1, 8, 0x8C, 2, 1, 0, 0x8C, 2, 1, 0 };
				const unsigned char recursive[] = { 0x62, 11, 0x82, 1, 1, 0xA1, 6, 0xA1, 4, 0x8C, 2, 1, 0 };
				expect_metadata_summary(malformed_compact, sizeof(malformed_compact), "file4141_a1_8c_read_encoding=uninterpreted\n");
				expect_metadata_summary(repeated, sizeof(repeated), "file4141_a1_8c_read_encoding=uninterpreted\n");
				expect_metadata_summary(recursive, sizeof(recursive), "file4141_a1_security_tags=A1\n");
				expect_metadata_summary(recursive, sizeof(recursive), "file4141_a1_8c_read_encoding=uninterpreted\n");
				puts("PASS malformed or repeated compact data and deeper containers stay uninterpreted");
			}
			{
				unsigned char record[sizeof(nested)];
				const unsigned char no_descriptor[] = { 0x62, 6, 0xA1, 4, 0x8C, 2, 1, 0 };
				memcpy(record, nested, sizeof(record)); record[4] = 2;
				expect_metadata_summary(record, sizeof(record), "file4141_a1_8c_read_encoding=uninterpreted\n");
				expect_metadata_summary(no_descriptor, sizeof(no_descriptor), "file4141_a1_8c_read_encoding=uninterpreted\n");
				puts("PASS nested READ encoding also requires an ordinary transparent descriptor");
			}
		}
	}
	return 0;
}
