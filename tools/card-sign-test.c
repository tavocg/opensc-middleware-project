/* Experimental APDU signing, not a PKCS#11 driver. No administrative commands. */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "libopensc/opensc.h"
#include "libopensc/pkcs15.h"
#include "jcop4-sign-operation.h"

static sc_pkcs15_object_t *unique_object(sc_pkcs15_card_t *p15, unsigned int type,
		const sc_pkcs15_id_t *id)
{
	sc_pkcs15_object_t *objects[32], *found = NULL;
	int count = sc_pkcs15_get_objects(p15, type, objects, 32), i;
	if (count < 0 || count >= 32)
		return NULL;
	for (i = 0; i < count; i++) {
		const sc_pkcs15_id_t *candidate;
		if (type == SC_PKCS15_TYPE_PRKEY)
			candidate = &((sc_pkcs15_prkey_info_t *)objects[i]->data)->id;
		else if (type == SC_PKCS15_TYPE_CERT_X509)
			candidate = &((sc_pkcs15_cert_info_t *)objects[i]->data)->id;
		else
			candidate = &((sc_pkcs15_auth_info_t *)objects[i]->data)->auth_id;
		if (candidate->len == id->len && !memcmp(candidate->value, id->value, id->len)) {
			if (found)
				return NULL;
			found = objects[i];
		}
	}
	return found;
}

static int validate_metadata(sc_pkcs15_card_t *p15, sc_pkcs15_object_t **certificate)
{
	sc_pkcs15_id_t id = { { 0 }, sizeof("LlaveDeFirma") - 1 };
	sc_pkcs15_object_t *key, *pin;
	sc_pkcs15_prkey_info_t *ki;
	sc_pkcs15_auth_info_t *pi;
	memcpy(id.value, "LlaveDeFirma", id.len);
	key = unique_object(p15, SC_PKCS15_TYPE_PRKEY, &id);
	if (!key || key->type != SC_PKCS15_TYPE_PRKEY_RSA || !key->auth_id.len)
		return 1;
	ki = key->data;
	if (!ki->native || ki->key_reference != 0x91 || ki->modulus_length != 2048 ||
			!(ki->usage & SC_PKCS15_PRKEY_USAGE_SIGN) || ki->path.len)
		return 1;
	pin = unique_object(p15, SC_PKCS15_TYPE_AUTH_PIN, &key->auth_id);
	if (!pin)
		return 1;
	pi = pin->data;
	if (pi->auth_type != SC_PKCS15_PIN_AUTH_TYPE_PIN || pi->auth_method != SC_AC_CHV ||
			pi->attrs.pin.reference != 1 || pi->attrs.pin.flags != 0x11 ||
			pi->attrs.pin.type != SC_PKCS15_PIN_TYPE_UTF8 || pi->attrs.pin.pad_char ||
			pi->attrs.pin.min_length != 6 || pi->attrs.pin.max_length != 14 || pi->path.len)
		return 1;
	*certificate = unique_object(p15, SC_PKCS15_TYPE_CERT_X509, &id);
	return !*certificate || ((*certificate)->flags & SC_PKCS15_CO_FLAG_PRIVATE);
}

int main(int argc, char **argv)
{
	const char *atr_hex = "3B:D5:18:FF:81:91:FE:1F:C3:80:73:C8:21:10:0A";
	const char *aid_hex = "E828BD080FA00000036364796E6964";
	sc_context_t *ctx = NULL;
	sc_card_t *card = NULL;
	sc_pkcs15_card_t *p15 = NULL;
	sc_pkcs15_cert_t *cert = NULL;
	sc_pkcs15_object_t *cert_obj;
	sc_context_param_t params;
	sc_reader_t *reader;
	sc_path_t path;
	struct sc_aid aid;
	unsigned char atr[SC_MAX_ATR_SIZE], digest[33], signature[256];
	size_t atr_len = sizeof(atr);
	unsigned long index;
	char *end;
	FILE *report = NULL, *file = NULL, *output = NULL;
	int rv = -1, result = 1, locked = 0, i, announced = 0, created_signature = 0;
	const char *stage = "input";

	if (argc != 3 || !argv[1][0] || strspn(argv[1], "0123456789") != strlen(argv[1]))
		return 2;
	errno = 0;
	index = strtoul(argv[1], &end, 10);
	if (errno || *end || index > UINT_MAX || !isatty(0) || !isatty(1))
		return 2;
	/* argv[2] is a fresh private result directory, never a credential. */
	umask(077);
	if (chdir(argv[2]) || !(report = fopen("report.txt", "a")))
		return 1;
	unsetenv("OPENSC_API_TRACE_FILE");
	unsetenv("OPENSC_DEBUG_FILE");
	if (!(file = fopen("digest.bin", "rb")) || fread(digest, 1, sizeof(digest), file) != 32)
		goto out;
	fclose(file); file = NULL;
	memset(&params, 0, sizeof(params));
	params.app_name = "card-sign-test";
	stage = "context";
	if ((rv = sc_context_create(&ctx, &params)) < 0)
		goto out;
	ctx->debug = 0;
	ctx->flags |= SC_CTX_FLAG_ENABLE_DEFAULT_DRIVER;
	stage = "reader";
	if ((rv = sc_set_card_driver(ctx, "default")) < 0 || !(reader = sc_ctx_get_reader(ctx, (unsigned int)index)))
		goto out;
	stage = "connection";
	if ((rv = sc_connect_card(reader, &card)) < 0)
		goto out;
	stage = "atr";
	if (sc_hex_to_bin(atr_hex, atr, &atr_len) < 0 || card->atr.len != atr_len || memcmp(card->atr.value, atr, atr_len))
		goto out;
	if ((rv = sc_lock(card)) < 0)
		goto out;
	locked = 1;
	stage = "directory";
	memset(&aid, 0, sizeof(aid));
	aid.len = sizeof(aid.value);
	if (sc_hex_to_bin(aid_hex, aid.value, &aid.len) < 0 || (rv = sc_enum_apps(card)) < 0)
		goto out;
	for (i = 0; i < card->app_count; i++)
		if (card->app[i]->aid.len == aid.len && !memcmp(card->app[i]->aid.value, aid.value, aid.len))
			announced = 1;
	if (!announced || !(p15 = sc_pkcs15_card_new()))
		goto out;
	p15->card = card;
	p15->opts.use_file_cache = SC_PKCS15_OPTS_CACHE_NO_FILES;
	p15->opts.use_pin_cache = 0;
	p15->opts.pin_protected_certificate = SC_PKCS15_CARD_OPTS_PRIV_CERT_PROTECT;
	stage = "native_binding";
	if ((rv = sc_pkcs15_bind_internal(p15, &aid)) < 0)
		goto out;
	stage = "metadata";
	if (validate_metadata(p15, &cert_obj))
		goto out;
	stage = "certificate";
	if ((rv = sc_pkcs15_read_certificate(p15, cert_obj->data, 0, &cert)) < 0 || !cert->key ||
			cert->key->algorithm != SC_ALGORITHM_RSA || cert->key->u.rsa.modulus.len != 256)
		goto out;
	if (!(file = fopen("certificate.der", "wx")) || fwrite(cert->data.value, 1, cert->data.len, file) != cert->data.len)
		goto out;
	if (fclose(file)) { file = NULL; goto out; }
	file = NULL;
	if (!(output = fopen("signature.bin", "wx")))
		goto out;
	created_signature = 1;
	memset(&path, 0, sizeof(path));
	path.type = SC_PATH_TYPE_DF_NAME;
	path.len = aid.len;
	memcpy(path.value, aid.value, aid.len);
	stage = "application_selection";
	if ((rv = sc_select_file(card, &path, NULL)) < 0)
		goto out;
	fprintf(report, "profile_metadata=validated\ncertificate_rsa_bits=2048\n");
	stage = "sign_operation";
	if (jcop4_sign_once(card, digest, signature, report))
		goto out;
	stage = "signature_output";
	if (fwrite(signature, 1, sizeof(signature), output) != sizeof(signature))
		goto out;
	result = 0;
out:
	if (report) fprintf(report, "last_stage=%s\nlast_library_result=%d\n", stage, rv);
	if (output && fclose(output)) result = 1;
	if (result && created_signature) unlink("signature.bin");
	if (file) fclose(file);
	if (cert) sc_pkcs15_free_certificate(cert);
	if (p15) sc_pkcs15_card_free(p15);
	if (locked) {
		rv = sc_unlock(card);
		fprintf(report, "unlock_result=%d\n", rv);
		if (rv < 0) result = 1;
	}
	if (card) sc_disconnect_card(card);
	if (ctx) sc_release_context(ctx);
	if (report) {
		fprintf(report, "helper_result=%d\n", result);
		fclose(report);
	}
	return result;
}
