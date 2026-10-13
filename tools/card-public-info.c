/* Public discovery, temporary MSE and empty VERIFY; no authentication. */
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

enum inspect_mode { INSPECT_PKCS15, INSPECT_SIGN_SETUP, INSPECT_PIN_STATUS, INSPECT_SECURITY_INFO };

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

static int select_application(sc_card_t *card, const struct sc_aid *aid)
{
	sc_path_t path;
	int rv;
	memset(&path, 0, sizeof(path));
	path.type = SC_PATH_TYPE_DF_NAME;
	path.len = aid->len;
	memcpy(path.value, aid->value, aid->len);
	rv = sc_select_file(card, &path, NULL);
	print_result("application_select_result", rv);
	if (rv < 0)
		return rv;
	return 0;
}

static int probe_pin_status(sc_card_t *card, const struct sc_aid *aid, unsigned char reference)
{
	sc_apdu_t apdu;
	int rv = select_application(card, aid);
	if (rv < 0)
		return 1;
	/* Same case-1 VERIFY used by iso7816 SC_PIN_CMD_GET_INFO: no PIN data. */
	sc_format_apdu(card, &apdu, SC_APDU_CASE_1, 0x20, 0, reference);
	apdu.cla = 0;
	apdu.flags |= SC_APDU_FLAGS_NO_RETRY_WL | SC_APDU_FLAGS_NO_GET_RESP;
	rv = sc_transmit_apdu(card, &apdu);
	print_result("pin_status_transport_result", rv);
	if (rv < 0)
		return 1;
	printf("pin_status_sw=%02X%02X\n", apdu.sw1, apdu.sw2);
	if (apdu.sw1 == 0x90 && apdu.sw2 == 0) {
		printf("pin_status_result=already_verified\n");
		return 0;
	}
	if (apdu.sw1 == 0x63 && (apdu.sw2 & 0xF0) == 0xC0) {
		printf("pin_status_tries_left=%u\n", apdu.sw2 & 0x0F);
		if (apdu.sw2 & 0x0F) {
			printf("pin_status_result=not_verified\n");
			return 0;
		}
	}
	printf("pin_status_result=blocked_or_unexpected_stop\n");
	return 1;
}

static int optional_public_status(unsigned int sw1, unsigned int sw2)
{
	return (sw1 == 0x6A && (sw2 == 0x81 || sw2 == 0x82 || sw2 == 0x88)) ||
		(sw1 == 0x6D && sw2 == 0);
}

static int probe_security_info(sc_card_t *card, const struct sc_aid *aid)
{
	sc_apdu_t apdu;
	sc_path_t mf;
	unsigned char version[6], block[256];
	int rv = select_application(card, aid);
	if (rv < 0)
		return 1;
	/* Public version query used by eoi_init; no interpretation assumed for CR. */
	sc_format_apdu(card, &apdu, SC_APDU_CASE_2_SHORT, 0xCA, 0x01, 0x16);
	apdu.cla = 0;
	apdu.flags |= SC_APDU_FLAGS_NO_RETRY_WL | SC_APDU_FLAGS_NO_GET_RESP;
	apdu.resp = version;
	apdu.resplen = apdu.le = sizeof(version);
	rv = sc_transmit_apdu(card, &apdu);
	print_result("version_transport_result", rv);
	if (rv < 0)
		return 1;
	printf("version_sw=%02X%02X\n", apdu.sw1, apdu.sw2);
	if (apdu.sw1 == 0x90 && apdu.sw2 == 0) {
		if (apdu.resplen != sizeof(version))
			return 1;
		print_hex("version_bytes", version, apdu.resplen);
	} else if (optional_public_status(apdu.sw1, apdu.sw2)) {
		printf("version_result=unavailable_in_this_query\n");
	} else {
		return 1;
	}
	memset(&mf, 0, sizeof(mf));
	mf.type = SC_PATH_TYPE_PATH;
	mf.value[0] = 0x3F; mf.value[1] = 0;
	mf.len = 2;
	rv = sc_select_file(card, &mf, NULL);
	print_result("cardaccess_mf_select_result", rv);
	if (rv < 0)
		return 1;
	/* sm-eac.c uses SFI 1C for EF.CardAccess. Read one bounded block only. */
	sc_format_apdu(card, &apdu, SC_APDU_CASE_2_SHORT, 0xB0, 0x9C, 0);
	apdu.cla = 0;
	apdu.flags |= SC_APDU_FLAGS_NO_RETRY_WL | SC_APDU_FLAGS_NO_GET_RESP;
	apdu.resp = block;
	apdu.resplen = apdu.le = sizeof(block);
	rv = sc_transmit_apdu(card, &apdu);
	print_result("cardaccess_transport_result", rv);
	if (rv < 0)
		return 1;
	printf("cardaccess_sw=%02X%02X\n", apdu.sw1, apdu.sw2);
	if ((apdu.sw1 == 0x90 && apdu.sw2 == 0) || (apdu.sw1 == 0x62 && apdu.sw2 == 0x82)) {
		if (apdu.resplen > sizeof(block))
			return 1;
		print_hex("cardaccess_first_block", block, apdu.resplen);
		printf("cardaccess_result=block_read_completeness_unverified\n");
		return 0;
	}
	if (optional_public_status(apdu.sw1, apdu.sw2) || (apdu.sw1 == 0x6B && apdu.sw2 == 0)) {
		printf("cardaccess_result=unavailable_in_this_query\n");
		return 0;
	}
	return 1;
}

static int probe_sign_setup(sc_card_t *card, const struct sc_aid *aid, unsigned char reference)
{
	sc_apdu_t apdu;
	unsigned char data[] = { 0x91, 0x02, 0x28, reference };
	int rv;

	/* CPS V4 guide, section 6.6.1: a hypothesis for this personalization.
	 * Do not translate the observed reference's high bit or attempt PSO. */
	rv = select_application(card, aid);
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

static int inspect_native_pkcs15(sc_card_t *card, struct sc_aid *aid, enum inspect_mode mode)
{
	sc_pkcs15_card_t *p15;
	sc_pkcs15_object_t *objects[32];
	const unsigned int types[] = { SC_PKCS15_TYPE_CERT_X509, SC_PKCS15_TYPE_PRKEY, SC_PKCS15_TYPE_AUTH_PIN };
	const char *names[] = { "certificate_count", "private_key_metadata_count", "pin_metadata_count" };
	int found = 0, status = 1, rv, count, i, sign_matches = 0, sign_valid = 0, sign_reference = 0;
	size_t cls;
	sc_pkcs15_id_t sign_auth = { { 0 }, 0 };
	int pin_matches = 0, pin_valid = 0, pin_reference = 0;

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
		unsigned int type = mode == INSPECT_SECURITY_INFO && cls == 2 ? SC_PKCS15_TYPE_AUTH : types[cls];
		count = sc_pkcs15_get_objects(p15, type, objects, 32);
		print_result(mode == INSPECT_SECURITY_INFO && cls == 2 ? "authentication_metadata_count" : names[cls], count);
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
				if (mode != INSPECT_PKCS15) {
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
					sign_auth = obj->auth_id;
					/* Diagnostic limited to the metadata observed on this card. */
					sign_valid = obj->type == SC_PKCS15_TYPE_PRKEY_RSA && info->native &&
							(info->usage & SC_PKCS15_PRKEY_USAGE_SIGN) &&
							info->key_reference == 0x91 && info->modulus_length == 2048;
				}
			} else {
				sc_pkcs15_auth_info_t *info = obj->data;
				if (obj->type != SC_PKCS15_TYPE_AUTH_PIN) {
					print_hex("authentication_id", info->auth_id.value, info->auth_id.len);
					printf("authentication_type=%u\nauthentication_method=%u\n", info->auth_type, info->auth_method);
					print_hex("authentication_path", info->path.value, info->path.len);
					if (info->auth_type == SC_PKCS15_PIN_AUTH_TYPE_AUTH_KEY) {
						print_hex("authentication_key_id", info->attrs.authkey.skey_id.value, info->attrs.authkey.skey_id.len);
						printf("authentication_key_derived=%d\n", info->attrs.authkey.derived);
					}
					continue; /* Metadata only; never read the referenced credential. */
				}
				print_hex("pin_auth_id", info->auth_id.value, info->auth_id.len);
				printf("pin_reference=%d\npin_flags=%u\npin_min_length=%zu\npin_max_length=%zu\n",
						info->attrs.pin.reference, info->attrs.pin.flags,
						info->attrs.pin.min_length, info->attrs.pin.max_length);
				printf("pin_auth_type=%u\npin_auth_method=%u\npin_format=%u\npin_pad_char=%02X\n",
						info->auth_type, info->auth_method, info->attrs.pin.type, info->attrs.pin.pad_char);
				print_hex("pin_path", info->path.value, info->path.len);
				if (sign_auth.len && info->auth_id.len == sign_auth.len &&
						!memcmp(info->auth_id.value, sign_auth.value, sign_auth.len)) {
					pin_matches++;
					pin_reference = info->attrs.pin.reference;
					pin_valid = info->auth_type == SC_PKCS15_PIN_AUTH_TYPE_PIN &&
							info->attrs.pin.reference == 1 && info->attrs.pin.flags == 0x11 &&
							info->attrs.pin.min_length == 6 && info->attrs.pin.max_length == 14 && !info->path.len;
				}
			}
		}
	}
	if (mode == INSPECT_SECURITY_INFO) {
		status = probe_security_info(card, aid);
	} else if (mode != INSPECT_PKCS15) {
		if (sign_matches != 1 || !sign_valid) {
			printf("sign_setup_result=metadata_mismatch_no_mse\n");
			goto out;
		}
		if (mode == INSPECT_PIN_STATUS) {
			if (pin_matches != 1 || !pin_valid) {
				printf("pin_status_result=metadata_mismatch_no_verify\n");
				goto out;
			}
			status = probe_pin_status(card, aid, (unsigned char)pin_reference);
		} else {
			status = probe_sign_setup(card, aid, (unsigned char)sign_reference);
		}
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
	int rv, status = 1, locked = 0, i;
	enum inspect_mode mode = INSPECT_PKCS15;

	if ((argc != 3 && argc != 5) || !argv[1][0] || strspn(argv[1], "0123456789") != strlen(argv[1]))
		return 2;
	memset(&app_aid, 0, sizeof(app_aid));
	app_aid.len = sizeof(app_aid.value);
	if (argc == 5 && ((strcmp(argv[3], "--pkcs15") && strcmp(argv[3], "--sign-setup") &&
			strcmp(argv[3], "--pin-status") && strcmp(argv[3], "--security-info")) ||
			sc_hex_to_bin(argv[4], app_aid.value, &app_aid.len) < 0 || !app_aid.len))
		return 2;

	errno = 0;
	reader_index = strtoul(argv[1], &end, 10);
	if (errno || *end || reader_index > UINT_MAX ||
			sc_hex_to_bin(argv[2], expected_atr, &expected_len) < 0 || !expected_len)
		return 2;

	if (argc == 5 && strcmp(argv[3], "--pkcs15")) {
		const unsigned char known_atr[] = { 0x3B, 0xD5, 0x18, 0xFF, 0x81, 0x91, 0xFE, 0x1F, 0xC3, 0x80, 0x73, 0xC8, 0x21, 0x10, 0x0A };
		const unsigned char known_aid[] = { 0xE8, 0x28, 0xBD, 0x08, 0x0F, 0xA0, 0x00, 0x00, 0x03, 0x63, 0x64, 0x79, 0x6E, 0x69, 0x64 };
		if (expected_len != sizeof(known_atr) || memcmp(expected_atr, known_atr, expected_len) ||
				app_aid.len != sizeof(known_aid) || memcmp(app_aid.value, known_aid, app_aid.len))
			return 2;
		mode = !strcmp(argv[3], "--sign-setup") ? INSPECT_SIGN_SETUP :
			!strcmp(argv[3], "--pin-status") ? INSPECT_PIN_STATUS : INSPECT_SECURITY_INFO;
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
	status = argc == 5 ? inspect_native_pkcs15(card, &app_aid, mode) : 0;
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
