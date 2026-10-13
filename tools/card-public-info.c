/* Public discovery and bounded temporary MSE probe; no authentication. */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libopensc/opensc.h"
#include "libopensc/pkcs15.h"

static void print_hex(const char *name, const unsigned char *value, size_t len)
{
	size_t i;
	printf("%s=", name);
	for (i = 0; i < len; i++)
		printf("%02X", value[i]);
	putchar('\n');
}

static void print_result(const char *name, int rv)
{
	printf("%s=%d\n", name, rv);
	if (rv < 0)
		printf("error=%s\n", sc_strerror(rv));
}

static int probe_sign_setup(sc_card_t *card, const struct sc_aid *aid, unsigned char reference)
{
	sc_path_t path;
	sc_apdu_t apdu;
	unsigned char data[] = { 0x91, 0x02, 0x28, reference };
	int rv;

	/* CPS V4 guide, section 6.6.1: a hypothesis for this personalization.
	 * Do not translate the observed reference's high bit or attempt PSO. */
	memset(&path, 0, sizeof(path));
	path.type = SC_PATH_TYPE_DF_NAME;
	path.len = aid->len;
	memcpy(path.value, aid->value, aid->len);
	rv = sc_select_file(card, &path, NULL);
	print_result("sign_setup_select_result", rv);
	if (rv < 0)
		return 1;
	sc_format_apdu(card, &apdu, SC_APDU_CASE_3_SHORT, 0x22, 0x81, 0xB6);
	apdu.cla = 0;
	apdu.flags |= SC_APDU_FLAGS_NO_RETRY_WL | SC_APDU_FLAGS_NO_GET_RESP;
	apdu.data = data;
	apdu.datalen = apdu.lc = sizeof(data);
	print_hex("mse_data", data, sizeof(data));
	rv = sc_transmit_apdu(card, &apdu);
	print_result("mse_transport_result", rv);
	if (rv < 0)
		return 1;
	printf("mse_sw=%02X%02X\n", apdu.sw1, apdu.sw2);
	if (apdu.sw1 != 0x90 || apdu.sw2 != 0)
		return 1;
	printf("sign_setup_result=accepted_not_a_signature\n");
	return 0;
}

static int inspect_native_pkcs15(sc_card_t *card, struct sc_aid *aid, int sign_setup)
{
	sc_pkcs15_card_t *p15;
	sc_pkcs15_object_t *objects[32];
	const unsigned int types[] = { SC_PKCS15_TYPE_CERT_X509, SC_PKCS15_TYPE_PRKEY, SC_PKCS15_TYPE_AUTH_PIN };
	const char *names[] = { "certificate_count", "private_key_metadata_count", "pin_metadata_count" };
	int found = 0, status = 1, rv, count, i, sign_matches = 0, sign_valid = 0, sign_reference = 0;
	size_t cls;

	for (i = 0; i < card->app_count; i++) {
		if (card->app[i]->aid.len == aid->len &&
				!memcmp(card->app[i]->aid.value, aid->value, aid->len))
			found = 1;
	}
	if (!found) {
		printf("pkcs15_result=aid_not_announced_no_bind\n");
		return 1;
	}
	p15 = sc_pkcs15_card_new();
	if (!p15)
		return 1;
	p15->card = card;
	p15->opts.use_file_cache = SC_PKCS15_OPTS_CACHE_NO_FILES;
	p15->opts.use_pin_cache = 0;
	p15->opts.pin_protected_certificate = SC_PKCS15_CARD_OPTS_PRIV_CERT_PROTECT;
	/* Native binding only: no emulators or PIN-status operations. */
	rv = sc_pkcs15_bind_internal(p15, aid);
	print_result("native_pkcs15_bind_result", rv);
	if (rv < 0)
		goto out;
	if (p15->tokeninfo) {
		for (i = 0; i < SC_MAX_SUPPORTED_ALGORITHMS && p15->tokeninfo->supported_algos[i].reference; i++) {
			const struct sc_supported_algo_info *algo = &p15->tokeninfo->supported_algos[i];
			printf("algorithm_reference=%u\nalgorithm_mechanism=%u\nalgorithm_operations=%u\nalgorithm_card_ref=%u\n",
					algo->reference, algo->mechanism, algo->operations, algo->algo_ref);
		}
		printf("algorithm_metadata_count=%d\n", i);
	}
	for (cls = 0; cls < sizeof(types) / sizeof(types[0]); cls++) {
		count = sc_pkcs15_get_objects(p15, types[cls], objects, 32);
		print_result(names[cls], count);
		if (count < 0 || count >= 32)
			goto out;
		for (i = 0; i < count; i++) {
			sc_pkcs15_object_t *obj = objects[i];
			printf("object_type=%u\nobject_label=%s\nobject_flags=%u\n", obj->type, obj->label, obj->flags);
			print_hex("object_auth_id", obj->auth_id.value, obj->auth_id.len);
			if (types[cls] == SC_PKCS15_TYPE_CERT_X509) {
				sc_pkcs15_cert_info_t *info = obj->data;
				sc_pkcs15_cert_t *cert = NULL;
				print_hex("certificate_id", info->id.value, info->id.len);
				if (sign_setup) {
					printf("certificate_read=skipped_setup_metadata_only\n");
					continue;
				}
				if (obj->flags & SC_PKCS15_CO_FLAG_PRIVATE) {
					printf("certificate_read=skipped_private\n");
					continue;
				}
				rv = sc_pkcs15_read_certificate(p15, info, 0, &cert);
				print_result("certificate_read_result", rv);
				if (rv < 0)
					goto out;
				print_hex("certificate_der", cert->data.value, cert->data.len);
				sc_pkcs15_free_certificate(cert);
			} else if (types[cls] == SC_PKCS15_TYPE_PRKEY) {
				sc_pkcs15_prkey_info_t *info = obj->data;
				print_hex("key_id", info->id.value, info->id.len);
				printf("key_reference=%d\nkey_native=%d\nkey_usage=%u\n", info->key_reference, info->native, info->usage);
				if (obj->type == SC_PKCS15_TYPE_PRKEY_RSA)
					printf("rsa_modulus_bits=%zu\n", info->modulus_length);
				print_hex("key_path", info->path.value, info->path.len);
				{
					unsigned int n;
					for (n = 0; n < SC_MAX_SUPPORTED_ALGORITHMS && info->algo_refs[n]; n++)
						printf("key_algorithm_reference=%u\n", info->algo_refs[n]);
				}
				if (info->id.len == sizeof("LlaveDeFirma") - 1 &&
						!memcmp(info->id.value, "LlaveDeFirma", info->id.len)) {
					sign_matches++;
					sign_reference = info->key_reference;
					/* Diagnostic limited to the metadata observed on this card. */
					sign_valid = obj->type == SC_PKCS15_TYPE_PRKEY_RSA && info->native &&
							(info->usage & SC_PKCS15_PRKEY_USAGE_SIGN) &&
							info->key_reference == 0x91 && info->modulus_length == 2048;
				}
			} else {
				sc_pkcs15_auth_info_t *info = obj->data;
				print_hex("pin_auth_id", info->auth_id.value, info->auth_id.len);
				printf("pin_reference=%d\npin_flags=%u\npin_min_length=%zu\npin_max_length=%zu\n",
						info->attrs.pin.reference, info->attrs.pin.flags,
						info->attrs.pin.min_length, info->attrs.pin.max_length);
				print_hex("pin_path", info->path.value, info->path.len);
			}
		}
	}
	if (sign_setup) {
		if (sign_matches != 1 || !sign_valid) {
			printf("sign_setup_result=metadata_mismatch_no_mse\n");
			goto out;
		}
		status = probe_sign_setup(card, aid, (unsigned char)sign_reference);
	} else {
		status = 0;
	}
out:
	sc_pkcs15_card_free(p15);
	return status;
}

int main(int argc, char **argv)
{
	sc_context_t *ctx = NULL;
	sc_card_t *card = NULL;
	sc_reader_t *reader;
	sc_context_param_t params;
	unsigned char expected_atr[SC_MAX_ATR_SIZE];
	size_t expected_len = sizeof(expected_atr);
	struct sc_aid app_aid;
	unsigned long reader_index;
	char *end;
	int rv, status = 1, locked = 0, i, sign_setup = 0;

	if ((argc != 3 && argc != 5) || !argv[1][0] || strspn(argv[1], "0123456789") != strlen(argv[1]))
		return 2;
	memset(&app_aid, 0, sizeof(app_aid));
	app_aid.len = sizeof(app_aid.value);
	if (argc == 5 && ((strcmp(argv[3], "--pkcs15") && strcmp(argv[3], "--sign-setup")) ||
			sc_hex_to_bin(argv[4], app_aid.value, &app_aid.len) < 0 || !app_aid.len))
		return 2;

	errno = 0;
	reader_index = strtoul(argv[1], &end, 10);
	if (errno || *end || reader_index > UINT_MAX ||
			sc_hex_to_bin(argv[2], expected_atr, &expected_len) < 0 || !expected_len)
		return 2;

	if (argc == 5 && !strcmp(argv[3], "--sign-setup")) {
		const unsigned char known_atr[] = { 0x3B, 0xD5, 0x18, 0xFF, 0x81, 0x91, 0xFE, 0x1F, 0xC3, 0x80, 0x73, 0xC8, 0x21, 0x10, 0x0A };
		const unsigned char known_aid[] = { 0xE8, 0x28, 0xBD, 0x08, 0x0F, 0xA0, 0x00, 0x00, 0x03, 0x63, 0x64, 0x79, 0x6E, 0x69, 0x64 };
		if (expected_len != sizeof(known_atr) || memcmp(expected_atr, known_atr, expected_len) ||
				app_aid.len != sizeof(known_aid) || memcmp(app_aid.value, known_aid, app_aid.len))
			return 2;
		sign_setup = 1;
	}
	memset(&params, 0, sizeof(params));
	params.app_name = "card-public-info";
	rv = sc_context_create(&ctx, &params);
	print_result("context_result", rv);
	if (rv < 0)
		goto out;
	ctx->debug = 0;
	ctx->flags |= SC_CTX_FLAG_ENABLE_DEFAULT_DRIVER;
	rv = sc_set_card_driver(ctx, "default");
	print_result("driver_result", rv);
	if (rv < 0)
		goto out;
	reader = sc_ctx_get_reader(ctx, (unsigned int)reader_index);
	if (!reader) {
		printf("reader_result=not_found\n");
		goto out;
	}
	rv = sc_connect_card(reader, &card);
	print_result("connect_result", rv);
	if (rv < 0)
		goto out;
	print_hex("atr", card->atr.value, card->atr.len);
	if (card->atr.len != expected_len || memcmp(card->atr.value, expected_atr, expected_len)) {
		printf("atr_result=mismatch_no_file_queries\n");
		goto out;
	}
	rv = sc_lock(card);
	print_result("lock_result", rv);
	if (rv < 0)
		goto out;
	locked = 1;

	/* Only 3F00/2F01 and 3F00/2F00; library routines stop failed SELECT reads. */
	rv = sc_parse_ef_atr(card);
	print_result("ef_atr_result", rv);
	if (rv < 0 && rv != SC_ERROR_FILE_NOT_FOUND)
		goto out;
	if (rv == SC_SUCCESS && card->ef_atr) {
		print_hex("mf_aid", card->ef_atr->aid.value, card->ef_atr->aid.len);
		print_hex("pre_issuing", card->ef_atr->pre_issuing, card->ef_atr->pre_issuing_len);
		printf("max_command_apdu=%zu\nmax_response_apdu=%zu\n",
				card->ef_atr->max_command_apdu, card->ef_atr->max_response_apdu);
	}
	rv = sc_enum_apps(card);
	print_result("ef_dir_result", rv);
	if (rv == SC_ERROR_FILE_NOT_FOUND) {
		printf("directory_result=not_found_in_this_context\n");
	} else if (rv < 0) {
		goto out;
	} else {
		printf("application_count=%d\n", card->app_count);
		for (i = 0; i < card->app_count; i++) {
			printf("application_index=%d\n", i);
			print_hex("application_aid", card->app[i]->aid.value, card->app[i]->aid.len);
			printf("application_label=%s\n", card->app[i]->label ? card->app[i]->label : "");
		}
	}
	status = argc == 5 ? inspect_native_pkcs15(card, &app_aid, sign_setup) : 0;
out:
	if (locked) {
		rv = sc_unlock(card);
		print_result("unlock_result", rv);
		if (rv < 0)
			status = 1;
	}
	if (card)
		sc_disconnect_card(card);
	if (ctx)
		sc_release_context(ctx);
	return status;
}
