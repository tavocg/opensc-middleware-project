/* Exercise the real PKCS#15 signing path with a public SHA-256 DigestInfo.
 * No hardware or PIN: SELECT, transport and locking are simulated.
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define sc_pkcs15_decipher fixture_decipher
#define sc_pkcs15_derive fixture_derive
#define sc_pkcs15_unwrap fixture_unwrap
#define sc_pkcs15_wrap fixture_wrap
#define sc_pkcs15_compute_signature fixture_compute_signature
#define sc_pkcs15_encrypt_sym fixture_encrypt_sym
#define sc_pkcs15_decrypt_sym fixture_decrypt_sym
#include "libopensc/pkcs15-sec.c"
#undef sc_pkcs15_decipher
#undef sc_pkcs15_derive
#undef sc_pkcs15_unwrap
#undef sc_pkcs15_wrap
#undef sc_pkcs15_compute_signature
#undef sc_pkcs15_encrypt_sym
#undef sc_pkcs15_decrypt_sym
#include "libopensc/card-iasecc.c"
#include "libopensc/iasecc-sdo.c"
#include "libopensc/padding.c"
#include "libopensc/pkcs15-iasecc.c"

/* SHA-256 DigestInfo for the standard vector "abc". */
static const unsigned char digest_info[] = {
	0x30,0x31,0x30,0x0D,0x06,0x09,0x60,0x86,0x48,0x01,0x65,0x03,0x04,0x02,0x01,0x05,0x00,0x04,0x20,
	0xBA,0x78,0x16,0xBF,0x8F,0x01,0xCF,0xEA,0x41,0x41,0x40,0xDE,0x5D,0xAE,0x22,0x23,
	0xB0,0x03,0x61,0xA3,0x96,0x17,0x7A,0x9C,0xB4,0x10,0xFF,0x61,0xF2,0x00,0x15,0xAD
};
static int failures, selections, metadata_queries, mse_queries, signature_queries, forbidden_commands;
static int reject_mse, reject_signature, reject_select, reject_security_status;
static int revalidation_queries;

static void check(const char *name, int valid)
{
	if (valid)
		printf("PASS %s\n", name);
	else {
		fprintf(stderr, "FAIL %s\n", name);
		failures++;
	}
}

int sc_lock(struct sc_card *card) { (void)card; return SC_SUCCESS; }
int sc_unlock(struct sc_card *card) { (void)card; return SC_SUCCESS; }

int sc_pkcs15_pincache_revalidate(struct sc_pkcs15_card *card, const struct sc_pkcs15_object *obj)
{
	(void)card;
	(void)obj;
	revalidation_queries++;
	return SC_ERROR_SECURITY_STATUS_NOT_SATISFIED;
}

/* Algorithm lookup is internal to libopensc; emulate the card's published
 * list while retaining the actual PKCS#15 encoding and driver operations.
 */
sc_algorithm_info_t *sc_card_find_alg(sc_card_t *card, unsigned int algorithm,
		size_t key_length, void *curve_name)
{
	(void)curve_name;
	for (int i = 0; i < card->algorithm_count; i++)
		if (card->algorithms[i].algorithm == algorithm && card->algorithms[i].key_length == key_length)
			return &card->algorithms[i];
	return NULL;
}

sc_algorithm_info_t *sc_card_find_rsa_alg(sc_card_t *card, size_t size)
{ return sc_card_find_alg(card, SC_ALGORITHM_RSA, size, NULL); }
sc_algorithm_info_t *sc_card_find_gostr3410_alg(sc_card_t *card, size_t size)
{ return sc_card_find_alg(card, SC_ALGORITHM_GOSTR3410, size, NULL); }
sc_algorithm_info_t *sc_card_find_ec_alg(sc_card_t *card, size_t size, struct sc_object_id *curve)
{ return sc_card_find_alg(card, SC_ALGORITHM_EC, size, curve); }
sc_algorithm_info_t *sc_card_find_eddsa_alg(sc_card_t *card, size_t size, struct sc_object_id *curve)
{ return sc_card_find_alg(card, SC_ALGORITHM_EDDSA, size, curve); }
sc_algorithm_info_t *sc_card_find_xeddsa_alg(sc_card_t *card, size_t size, struct sc_object_id *curve)
{ return sc_card_find_alg(card, SC_ALGORITHM_XEDDSA, size, curve); }

int sc_select_file(struct sc_card *card, const struct sc_path *path, struct sc_file **file)
{
	struct iasecc_private_data *private_data = card->drv_data;
	struct sc_file *df = private_data->cache.current_df;
	(void)file;
	if (reject_select)
		return SC_ERROR_SECURITY_STATUS_NOT_SATISFIED;
	if (path->len || path->aid.len != df->path.len ||
			memcmp(path->aid.value, df->path.value, df->path.len))
		return SC_ERROR_INVALID_ARGUMENTS;
	selections++;
	return SC_SUCCESS;
}

int sc_transmit_apdu(struct sc_card *card, struct sc_apdu *apdu)
{
	static const unsigned char request[] = {0x4D,0x08,0x70,0x06,0xBF,0x90,0x03,0x02,0xA0,0x80};
	static const unsigned char docp[] = {
		0x70,0x2B,0xBF,0x90,0x03,0x27,0xA0,0x25,0x9E,0x01,0x00,
		0x80,0x02,0x01,0x00,0xA1,0x1C,
		0x8C,0x0C,0xC0,0x13,0x90,0x13,0xA0,0x13,0x88,0x13,0x82,0xD3,0x81,0x00,
		0x9C,0x0C,0xC0,0x13,0x90,0x13,0xA0,0x13,0x88,0x13,0x82,0xD3,0x81,0x00
	};
	static const unsigned char ht[] = {0x80,0x01,0x40};
	static const unsigned char dst[] = {0x84,0x01,0x83,0x80,0x01,0x42};
	static const unsigned char at[] = {0x84,0x01,0x83,0x80,0x01,0x02};
	(void)card;
	apdu->sw1 = 0x90;
	apdu->sw2 = 0;
	if (apdu->ins == 0xCB && apdu->p1 == 0x3F && apdu->p2 == 0xFF &&
			apdu->datalen == sizeof(request) && !memcmp(apdu->data, request, sizeof(request))) {
		if (apdu->resplen < sizeof(docp))
			return SC_ERROR_BUFFER_TOO_SMALL;
		memcpy(apdu->resp, docp, sizeof(docp));
		apdu->resplen = sizeof(docp);
		metadata_queries++;
	} else if (apdu->ins == 0x22 && apdu->p1 == 0x41 &&
			((apdu->p2 == 0xAA && apdu->datalen == sizeof(ht) && !memcmp(apdu->data, ht, sizeof(ht))) ||
			 (apdu->p2 == 0xB6 && apdu->datalen == sizeof(dst) && !memcmp(apdu->data, dst, sizeof(dst))) ||
			 (apdu->p2 == 0xA4 && apdu->datalen == sizeof(at) && !memcmp(apdu->data, at, sizeof(at))))) {
		mse_queries++;
		apdu->resplen = 0;
		if (reject_mse) { apdu->sw1 = 0x6A; apdu->sw2 = 0x80; }
	} else if (apdu->ins == 0x88 && !apdu->p1 && !apdu->p2 && apdu->le == 256 &&
			apdu->datalen == sizeof(digest_info) && !memcmp(apdu->data, digest_info, sizeof(digest_info))) {
		signature_queries++;
		if (reject_signature) return SC_ERROR_NOT_SUPPORTED;
		if (reject_security_status) {
			apdu->sw1 = 0x69;
			apdu->sw2 = 0x82;
			apdu->resplen = 0;
			return SC_SUCCESS;
		}
		if (apdu->resplen < 256) return SC_ERROR_BUFFER_TOO_SMALL;
		/* Synthetic response, not evidence of any real RSA signature. */
		memset(apdu->resp, 0xA5, 256);
		apdu->resplen = 256;
	} else {
		forbidden_commands++;
		return SC_ERROR_NOT_SUPPORTED;
	}
	return SC_SUCCESS;
}

static void reset_transport(void)
{
	selections = metadata_queries = mse_queries = signature_queries = forbidden_commands = 0;
	reject_mse = reject_signature = 0;
	reject_select = reject_security_status = revalidation_queries = 0;
}

static void read_trace(const char *path, char *output, size_t size)
{
	FILE *file = fopen(path, "r");
	size_t length = 0;
	if (file) {
		length = fread(output, 1, size - 1, file);
		fclose(file);
	}
	output[length] = 0;
}

static void clear_trace(const char *path)
{
	FILE *file = fopen(path, "w");
	if (file) fclose(file);
}

int main(void)
{
	static const unsigned char aid[] = {
		0xE8,0x28,0xBD,0x08,0x0F,0xD2,0x50,0x47,0x65,0x6E,0x65,0x72,0x69,0x63
	};
	struct sc_context ctx = {0};
	struct sc_card card = {0};
	struct sc_card_operations operations = {0};
	struct iasecc_private_data private_data = {0};
	struct sc_file df = {0};
	struct sc_pkcs15_card p15card = {0};
	struct sc_pkcs15_tokeninfo tokeninfo = {0};
	struct sc_app_info app = {0};
	struct sc_pkcs15_object key = {0};
	struct sc_pkcs15_prkey_info key_info = {0};
	struct sc_algorithm_info algorithm = {0};
	unsigned char signature[256];
	unsigned long flags = SC_ALGORITHM_RSA_PAD_PKCS1_TYPE_01 | SC_ALGORITHM_RSA_HASH_NONE;
	int result;
	ctx.app_name = "iasecc-rsa-pkcs-regression";
	card.ctx = &ctx;
	struct sc_card_driver driver = {.short_name = "iasecc"};
	card.driver = &driver;
	card.type = SC_CARD_TYPE_IASECC_AMOS;
	card.drv_data = &private_data;
	card.ops = &operations;
	card.algorithms = &algorithm;
	card.algorithm_count = 1;
	algorithm.algorithm = SC_ALGORITHM_RSA;
	algorithm.key_length = 2048;
	algorithm.flags = IASECC_CARD_DEFAULT_FLAGS;
	iso_ops = sc_get_iso7816_driver()->ops;
	operations.check_sw = iso_ops->check_sw;
	operations.set_security_env = iasecc_set_security_env;
	operations.compute_signature = iasecc_compute_signature;
	private_data.cache.valid = 1;
	private_data.cache.current_df = &df;
	df.path.type = SC_PATH_TYPE_DF_NAME;
	df.path.len = sizeof(aid);
	memcpy(df.path.value, aid, sizeof(aid));
	p15card.card = &card;
	p15card.tokeninfo = &tokeninfo;
	p15card.app = &app;
	app.aid.len = sizeof(aid);
	memcpy(app.aid.value, aid, sizeof(aid));
	p15card.opts.use_pin_cache = 0;
	key.type = SC_PKCS15_TYPE_PRKEY_RSA;
	key.data = &key_info;
	key_info.native = 1;
	key_info.modulus_length = 2048;
	key_info.key_reference = 3;
	key_info.usage = SC_PKCS15_PRKEY_USAGE_SIGN;
	p15card.obj_list = &key;
	key_info.path.type = SC_PATH_TYPE_PATH;
	key_info.path.aid.len = sizeof(aid);
	memcpy(key_info.path.aid.value, aid, sizeof(aid));
	result = fixture_compute_signature(&p15card, &key, flags, digest_info, sizeof(digest_info),
			signature, sizeof(signature), NULL);
	check("PKCS15 RSA-PKCS sends unchanged SHA-256 DigestInfo with measured AT 02", result == 256 &&
			selections == 1 && metadata_queries == 1 && mse_queries == 1 && signature_queries == 1 &&
			!forbidden_commands && private_data.security_env.operation == SC_SEC_OPERATION_AUTHENTICATE &&
			signature[0] == 0xA5 && signature[255] == 0xA5);
	reset_transport();
	reject_mse = 1;
	result = fixture_compute_signature(&p15card, &key, flags, digest_info, sizeof(digest_info),
			signature, sizeof(signature), NULL);
	check("failed AT setup stops before RSA without retry", result < 0 && selections == 1 &&
			metadata_queries == 1 && mse_queries == 1 && !signature_queries && !forbidden_commands);
	reset_transport();
	reject_signature = 1;
	result = fixture_compute_signature(&p15card, &key, flags, digest_info, sizeof(digest_info),
			signature, sizeof(signature), NULL);
	check("failed RSA operation is not retried by PKCS15", result == SC_ERROR_NOT_SUPPORTED &&
			selections == 1 && signature_queries == 1 && !forbidden_commands);
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	reset_transport();
	flags = SC_ALGORITHM_RSA_PAD_PKCS1_TYPE_01 | SC_ALGORITHM_RSA_HASH_SHA256;
	result = fixture_compute_signature(&p15card, &key, flags, digest_info + 19, 32,
			signature, sizeof(signature), NULL);
	check("unadapted combined SHA-256 route fails in host QSign with OpenSSL 3", result == SC_ERROR_NOT_SUPPORTED &&
			selections == 1 && metadata_queries == 1 && mse_queries == 2 && !signature_queries && !forbidden_commands);
#endif
	reset_transport();
	_iasecc_fixup_signature_algorithms(&p15card);
	flags = SC_ALGORITHM_RSA_PAD_PKCS1_TYPE_01 | SC_ALGORITHM_RSA_HASH_SHA256;
	result = fixture_compute_signature(&p15card, &key, flags, digest_info + 19, 32,
			signature, sizeof(signature), NULL);
	check("ChipDoc software SHA-256 digest uses the verified RSA-PKCS path", result == 256 &&
			selections == 1 && metadata_queries == 1 && mse_queries == 1 && signature_queries == 1 &&
			!forbidden_commands && private_data.security_env.operation == SC_SEC_OPERATION_AUTHENTICATE);
	reset_transport();
	result = fixture_compute_signature(&p15card, &key, flags, digest_info + 19, 31,
			signature, sizeof(signature), NULL);
	check("invalid SHA-256 digest length is rejected before card transport", result < 0 &&
			!selections && !metadata_queries && !mse_queries && !signature_queries && !forbidden_commands);
	reset_transport();
	flags = SC_ALGORITHM_RSA_PAD_PKCS1_TYPE_01 | SC_ALGORITHM_RSA_HASH_NONE;
	result = fixture_compute_signature(&p15card, &key, flags, digest_info, sizeof(digest_info),
			signature, sizeof(signature), NULL);
	check("explicit RSA-PKCS remains unchanged after the capability fixup", result == 256 &&
			selections == 1 && metadata_queries == 1 && mse_queries == 1 && signature_queries == 1 &&
			!forbidden_commands);

	/* Preserve functional failure propagation after removal of debug traces. */
	reset_transport(); reject_mse = 1;
	result = fixture_compute_signature(&p15card, &key, flags, digest_info, sizeof(digest_info), signature, sizeof(signature), NULL);
	check("MSE failure stops before signing or revalidation", result < 0 && !signature_queries && !revalidation_queries && !forbidden_commands);
	reset_transport(); reject_security_status = 1;
	result = fixture_compute_signature(&p15card, &key, flags, digest_info, sizeof(digest_info), signature, sizeof(signature), NULL);
	check("card rejection propagates without another RSA", result == SC_ERROR_SECURITY_STATUS_NOT_SATISFIED && signature_queries == 1 && revalidation_queries == 1 && !forbidden_commands);
	reset_transport(); reject_select = 1;
	result = fixture_compute_signature(&p15card, &key, flags, digest_info, sizeof(digest_info), signature, sizeof(signature), NULL);
	check("selection failure stops before metadata MSE or signing", result == SC_ERROR_SECURITY_STATUS_NOT_SATISFIED && !metadata_queries && !mse_queries && !signature_queries && revalidation_queries == 1 && !forbidden_commands);
	return failures ? 1 : 0;
}
