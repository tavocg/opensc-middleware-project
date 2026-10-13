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
#include "libopensc/asn1.h"

enum inspect_mode { INSPECT_PKCS15, INSPECT_SIGN_SETUP, INSPECT_PIN_STATUS, INSPECT_SECURITY_INFO, INSPECT_PIN_POLICY, INSPECT_ODF_MAP, INSPECT_FILE4141, INSPECT_FILE4109, INSPECT_FILE7001, INSPECT_FILE7002, INSPECT_FILE7004, INSPECT_FILE7005, INSPECT_FILEDF0F, INSPECT_FILEB001, INSPECT_FILEB002, INSPECT_PUBLIC_KEYS, INSPECT_CERTIFICATE_PATHS };

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

/* Decode references only. Never bind PKCS#15 or follow the paths found here. */
static int print_odf_map(sc_context_t *ctx, const unsigned char *buf, size_t size)
{
	const char *names[] = { "private_keys", "public_keys", "trusted_public_keys", "secret_keys",
		"certificates", "trusted_certificates", "useful_certificates", "data_objects", "auth_objects" };
	struct { unsigned int kind; sc_path_t path; } records[32];
	const unsigned char *p = buf, *value, *tail;
	size_t left = size, value_len, tail_len, n = 0, i;
	unsigned int cla, tag;
	while (left) {
		if (*p == 0 || *p == 0xFF) {
			for (i = 0; i < left; i++) if (p[i] != *p) return 1;
			break; /* Uniform EF padding, not another record. */
		}
		value = p;
		if (n == 32 || sc_asn1_read_tag(&value, left, &cla, &tag, &value_len) < 0 ||
				!value || cla != 0xA0 || tag > 8 || !value_len)
			return 1;
		memset(&records[n], 0, sizeof(records[n]));
		{
			struct sc_asn1_entry path[] = {
				{ "path", SC_ASN1_PATH, SC_ASN1_CONS | SC_ASN1_SEQUENCE, 0, &records[n].path, NULL },
				{ NULL, 0, 0, 0, NULL, NULL }
			};
			if (sc_asn1_decode(ctx, path, value, value_len, &tail, &tail_len) < 0 || tail_len ||
					!records[n].path.len || records[n].path.len % 2 || records[n].path.aid.len ||
					(records[n].path.type != SC_PATH_TYPE_PATH && records[n].path.type != SC_PATH_TYPE_FILE_ID))
				return 1;
		}
		records[n++].kind = tag;
		left -= (size_t)(value - p) + value_len;
		p = value + value_len;
	}
	if (!n) return 1;
	/* Output only after all records validate; no raw DER or inline objects. */
	printf("odf_directory_count=%zu\n", n);
	for (i = 0; i < n; i++) {
		printf("directory_kind=%s\n", names[records[i].kind]);
		print_hex("directory_path", records[i].path.value, records[i].path.len);
	}
	printf("odf_result=paths_from_first_block_no_follow\n");
	return 0;
}

static int inspect_odf_map(sc_card_t *card, const struct sc_aid *aid)
{
	const unsigned char fid[] = { 0x50, 0x31 };
	unsigned char block[256];
	sc_apdu_t apdu;
	int i, found = 0, rv;
	for (i = 0; i < card->app_count; i++)
		if (card->app[i]->aid.len == aid->len && !memcmp(card->app[i]->aid.value, aid->value, aid->len))
			found = 1;
	if (!found) {
		printf("odf_result=aid_not_announced_no_select\n");
		return 1;
	}
	if (select_application(card, aid) < 0) return 1;
	/* Fixed public bootstrap EF only, no FID obtained from untrusted metadata. */
	sc_format_apdu(card, &apdu, SC_APDU_CASE_3_SHORT, 0xA4, 2, 0x0C);
	apdu.cla = 0;
	apdu.flags |= SC_APDU_FLAGS_NO_RETRY_WL | SC_APDU_FLAGS_NO_GET_RESP;
	apdu.data = fid; apdu.lc = apdu.datalen = sizeof(fid);
	rv = sc_transmit_apdu(card, &apdu);
	print_result("odf_select_transport", rv);
	if (rv < 0) return 1;
	printf("odf_select_sw=%02X%02X\n", apdu.sw1, apdu.sw2);
	if (apdu.sw1 != 0x90 || apdu.sw2 != 0) return 1;
	sc_format_apdu(card, &apdu, SC_APDU_CASE_2_SHORT, 0xB0, 0, 0);
	apdu.cla = 0;
	apdu.flags |= SC_APDU_FLAGS_NO_RETRY_WL | SC_APDU_FLAGS_NO_GET_RESP;
	apdu.resp = block; apdu.le = apdu.resplen = sizeof(block);
	rv = sc_transmit_apdu(card, &apdu);
	print_result("odf_read_transport", rv);
	if (rv < 0) return 1;
	printf("odf_read_sw=%02X%02X\n", apdu.sw1, apdu.sw2);
	if (!((apdu.sw1 == 0x90 && apdu.sw2 == 0) || (apdu.sw1 == 0x62 && apdu.sw2 == 0x82)) ||
			!apdu.resplen || apdu.resplen > sizeof(block))
		return 1;
	if (print_odf_map(card->ctx, block, apdu.resplen)) {
		printf("odf_result=unsupported_or_incomplete_block_no_follow\n");
		return 1;
	}
	return 0;
}

/* ETSI TS 102 221 9.2.5 describes the ISO compact groups: AM followed
 * by one SC per set bit, high to low; bit 1 is READ for an EF. This is
 * diagnostic metadata, never a grant to read an unidentified file. */
enum compact_read { COMPACT_ABSENT, COMPACT_ALWAYS, COMPACT_OTHER, COMPACT_ALTERNATIVES, COMPACT_UNSUPPORTED };
static int summarize_compact_read(const unsigned char *buf, size_t size, enum compact_read *summary)
{
	size_t pos = 0;
	int seen = 0, condition = 0, alternatives = 0;
	if (!size) return 1;
	while (pos < size) {
		unsigned int am = buf[pos++], mask;
		if (am & 0x80) { *summary = COMPACT_UNSUPPORTED; return 0; }
		if (!am) return 1;
		for (mask = 0x40; mask; mask >>= 1) {
			if (!(am & mask)) continue;
			if (pos == size) return 1;
			if (mask == 1) {
				if (seen && condition != buf[pos]) alternatives = 1;
				condition = buf[pos]; seen = 1;
			}
			pos++;
		}
	}
	*summary = alternatives ? COMPACT_ALTERNATIVES : !seen ? COMPACT_ABSENT :
		condition ? COMPACT_OTHER : COMPACT_ALWAYS;
	return 0;
}

static const char *compact_read_name(enum compact_read summary)
{
	const char *names[] = { "not_declared", "always", "nonzero_uninterpreted", "alternatives_uninterpreted", "proprietary_uninterpreted" };
	return names[summary];
}

/* Only these fixed tag names may be reported. No arbitrary identifiers,
 * values, lengths, reference numbers or security-condition bytes escape. */
static unsigned int security_tag_bit(unsigned int tag)
{
	switch (tag) {
	case 0x8C: return 1;
	case 0x8B: return 2;
	case 0xAB: return 4;
	case 0x86: return 8;
	case 0xA1: return 16;
	case 0x9C: return 32;
	case 0x62: return 64;
	case 0x6F: return 128;
	default: return 0;
	}
}

static void print_security_tags(const char *key, unsigned int tags)
{
	const char *names[] = { "8C", "8B", "AB", "86", "A1", "9C", "62", "6F" };
	unsigned int i;
	int first = 1;
	printf("%s=", key);
	for (i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
		if (!(tags & (1U << i))) continue;
		printf("%s%s", first ? "" : ",", names[i]); first = 0;
	}
	printf("%s\n", first ? "none_recognized" : "");
}

struct a1_metadata {
	unsigned int tags;
	int valid_tlv, other_tags, compact_count, compact_valid;
	enum compact_read compact;
};

/* A1 is constructed in BER. Inspect only its immediate TLVs; its role in
 * this personalization is unknown. A nested 8C gets an encoding summary,
 * never the JCOP3 designation 'contact ACL' or a global READ policy. */
static void summarize_a1_metadata(const unsigned char *buf, size_t size, struct a1_metadata *out)
{
	const unsigned char *p = buf, *value;
	size_t left = size, len;
	unsigned int cla, tag, bit;
	memset(out, 0, sizeof(*out));
	out->valid_tlv = 1;
	while (left) {
		value = p;
		if (sc_asn1_read_tag(&value, left, &cla, &tag, &len) < 0 || !value) {
			/* Discard partial evidence if the container is opaque/malformed. */
			memset(out, 0, sizeof(*out)); return;
		}
		bit = security_tag_bit(cla | tag);
		out->tags |= bit;
		if (!bit) out->other_tags = 1;
		if ((cla | tag) == 0x8C && ++out->compact_count == 1)
			out->compact_valid = !summarize_compact_read(value, len, &out->compact);
		left -= (size_t)(value - p) + len;
		p = value + len;
	}
}

/* SELECT returns control information, not EF contents. Emit only bounded
 * metadata summaries; omit names, raw ACLs, proprietary values and responses. */
static int print_file_metadata(const unsigned char *buf, size_t size, unsigned int fid)
{
	const unsigned char *p = buf, *value;
	char direct_name[40], a1_name[40];
	size_t len, left, file_size = 0;
	unsigned int cla, tag, descriptor = 0;
	int have_descriptor = 0, have_size = 0;
	unsigned int security_forms = 0;
	unsigned int direct_tags = 0;
	int compact_count = 0, a1_count = 0;
	struct a1_metadata a1 = {0};
	enum compact_read compact = COMPACT_ABSENT;
	if (fid != 0x4141 && fid != 0x4109 && fid != 0x7001 && fid != 0x7002 && fid != 0x7004 && fid != 0x7005 && fid != 0xDF0F && fid != 0xB001 && fid != 0xB002) return 1;
	snprintf(direct_name, sizeof(direct_name), "file%04X_security_direct_tags", fid);
	snprintf(a1_name, sizeof(a1_name), "file%04X_a1_security_tags", fid);
	if (!size) {
		printf("file%04X_result=selected_without_metadata_no_read\n", fid);
		return 0;
	}
	if (sc_asn1_read_tag(&p, size, &cla, &tag, &len) < 0 || !p ||
			cla != 0x60 || (tag != 2 && tag != 15) || (size_t)(p - buf) + len != size)
		return 1;
	left = len;
	while (left) {
		value = p;
		if (sc_asn1_read_tag(&value, left, &cla, &tag, &len) < 0 || !value) return 1;
		if ((cla | tag) == 0x83 && (len != 2 || value[0] != (fid >> 8) || value[1] != (fid & 0xFF))) return 1;
		if ((cla | tag) == 0x82) {
			if (!len || (have_descriptor && descriptor != value[0])) return 1;
			descriptor = value[0]; have_descriptor = 1;
		}
		if ((cla | tag) == 0x80) {
			size_t candidate = 0, i;
			if (!len || len > 4) return 1;
			for (i = 0; i < len; i++) candidate = (candidate << 8) | value[i];
			if (have_size && candidate != file_size) return 1;
			file_size = candidate; have_size = 1;
		}
		direct_tags |= security_tag_bit(cla | tag);
		if ((cla | tag) == 0xA1 && ++a1_count == 1)
			summarize_a1_metadata(value, len, &a1);
		switch (cla | tag) {
		case 0x8C:
			security_forms |= 1;
			if (++compact_count == 1 && summarize_compact_read(value, len, &compact)) return 1;
			break;
		case 0x8B: security_forms |= 2; break;
		case 0xAB: security_forms |= 4; break;
		/* Profile-dependent forms are only reported as uninterpreted.
		 * Do not import the JCOP3 meaning of A1/9C into this profile. */
		case 0x86: case 0xA1: case 0x9C: security_forms |= 8; break;
		case 0x62: case 0x6F: security_forms |= 16; break;
		}
		left -= (size_t)(value - p) + len;
		p = value + len;
	}
	if (have_descriptor)
		printf("file%04X_structure=%s\n", fid, (descriptor & 0x38) == 0x38 ? "dedicated_file" :
				(descriptor & 0x38) > 8 ? "unknown" :
				(descriptor & 7) == 1 ? "transparent" : (descriptor & 7) >= 2 ? "record" : "unknown");
	if (have_size) printf("file%04X_size=%zu\n", fid, file_size);
	printf("file%04X_security_format=%s\n", fid, !security_forms ? "not_observed_in_direct_tags" :
		security_forms == 1 ? "compact" : security_forms == 2 ? "referenced_uninterpreted" :
		security_forms == 4 ? "expanded_uninterpreted" : security_forms == 8 ? "profile_uninterpreted" :
		security_forms == 16 ? "nested_control_uninterpreted" : "multiple_forms_uninterpreted");
	print_security_tags(direct_name, direct_tags);
	if (a1_count) {
		printf("file%04X_a1_structure=%s\n", fid, a1_count != 1 ? "multiple_uninterpreted" :
			a1.valid_tlv ? "immediate_tlv_only" : "opaque_uninterpreted");
		if (a1_count == 1 && a1.valid_tlv) {
			print_security_tags(a1_name, a1.tags);
			printf("file%04X_a1_other_tags=%s\n", fid, a1.other_tags ? "present_values_omitted" : "not_observed");
			printf("file%04X_a1_8c_read_encoding=%s\n", fid, a1.compact_count == 1 && a1.compact_valid &&
				have_descriptor && !(descriptor & 0x80) &&
				((descriptor & 0x3F) == 1 || (descriptor & 0x3F) == 9) ?
				compact_read_name(a1.compact) : "uninterpreted");
		}
	}
	if (security_forms == 1 && compact_count == 1 && have_descriptor && !(descriptor & 0x80) &&
			((descriptor & 0x3F) == 1 || (descriptor & 0x3F) == 9)) {
		printf("file%04X_standard_compact_read=%s\n", fid, compact_read_name(compact));
	} else if (security_forms)
		printf("file%04X_standard_compact_read=uninterpreted\n", fid);
	printf("file%04X_result=selected_metadata_only_no_read\n", fid);
	return 0;
}

static int print_file4141_metadata(const unsigned char *buf, size_t size)
{
	return print_file_metadata(buf, size, 0x4141);
}

static int inspect_file_metadata(sc_card_t *card, const struct sc_aid *aid, unsigned int fid_number)
{
	const unsigned char fid[] = { fid_number >> 8, fid_number & 0xFF };
	char transport_name[40];
	unsigned char response[256];
	sc_apdu_t apdu;
	int i, found = 0, rv;
	if (fid_number != 0x4141 && fid_number != 0x4109 && fid_number != 0x7001 && fid_number != 0x7002 && fid_number != 0x7004 && fid_number != 0x7005 && fid_number != 0xDF0F && fid_number != 0xB001 && fid_number != 0xB002) return 1;
	for (i = 0; i < card->app_count; i++)
		if (card->app[i]->aid.len == aid->len && !memcmp(card->app[i]->aid.value, aid->value, aid->len)) found = 1;
	if (!found) {
		printf("file%04X_result=aid_not_announced_no_select\n", fid_number);
		return 1;
	}
	if (select_application(card, aid) < 0) return 1;
	/* The one SELECT observed blocked in Idopte. No B0/B2 or binding follows. */
	sc_format_apdu(card, &apdu, SC_APDU_CASE_4_SHORT, 0xA4, 2, 0);
	apdu.cla = 0;
	apdu.flags |= SC_APDU_FLAGS_NO_RETRY_WL | SC_APDU_FLAGS_NO_GET_RESP;
	apdu.data = fid; apdu.lc = apdu.datalen = sizeof(fid);
	apdu.resp = response; apdu.le = apdu.resplen = sizeof(response);
	rv = sc_transmit_apdu(card, &apdu);
	snprintf(transport_name, sizeof(transport_name), "file%04X_select_transport", fid_number);
	print_result(transport_name, rv);
	if (rv < 0) return 1;
	printf("file%04X_select_sw=%02X%02X\n", fid_number, apdu.sw1, apdu.sw2);
	if (apdu.sw1 == 0x6A && apdu.sw2 == 0x82) {
		printf("file%04X_result=not_found_in_known_application_no_read\n", fid_number);
		return 0;
	}
	if (apdu.sw1 != 0x90 || apdu.sw2 != 0 || apdu.resplen > sizeof(response)) return 1;
	if (fid_number == 0x4141 ? print_file4141_metadata(response, apdu.resplen) :
			print_file_metadata(response, apdu.resplen, fid_number)) {
		printf("file%04X_result=selected_metadata_unrecognized_no_read\n", fid_number);
		return 1;
	}
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

static int probe_pin_policy(sc_card_t *card, const struct sc_aid *aid)
{
	/* iasecc_sdo_get_tagged_data: CHV 1, DOCP A0 only, never CHV value 7F41. */
	const unsigned char request[] = { 0x4D, 0x08, 0x70, 0x06, 0xBF, 0x81, 0x01, 0x02, 0xA0, 0x80 };
	unsigned char response[256];
	sc_path_t mf;
	sc_apdu_t apdu;
	int context, rv;
	memset(&mf, 0, sizeof(mf));
	mf.type = SC_PATH_TYPE_PATH;
	mf.value[0] = 0x3F; mf.value[1] = 0;
	mf.len = 2;
	for (context = 0; context < 2; context++) {
		printf("pin_policy_context=%s\n", context ? "application" : "mf");
		rv = context ? select_application(card, aid) : sc_select_file(card, &mf, NULL);
		print_result("pin_policy_select_result", rv);
		if (rv < 0)
			return 1;
		sc_format_apdu(card, &apdu, SC_APDU_CASE_4_SHORT, 0xCB, 0x3F, 0xFF);
		apdu.cla = 0;
		apdu.flags |= SC_APDU_FLAGS_NO_RETRY_WL | SC_APDU_FLAGS_NO_GET_RESP;
		apdu.data = request;
		apdu.lc = apdu.datalen = sizeof(request);
		apdu.resp = response;
		apdu.le = apdu.resplen = sizeof(response);
		rv = sc_transmit_apdu(card, &apdu);
		print_result("pin_policy_transport_result", rv);
		if (rv < 0)
			return 1;
		printf("pin_policy_sw=%02X%02X\npin_policy_response_length=%zu\n", apdu.sw1, apdu.sw2, apdu.resplen);
		if (apdu.sw1 == 0x90 && apdu.sw2 == 0) {
			if (apdu.resplen > sizeof(response))
				return 1;
			print_hex("pin_policy_docp", response, apdu.resplen);
			printf("pin_policy_result=bytes_uninterpreted\n");
		} else if (optional_public_status(apdu.sw1, apdu.sw2)) {
			printf("pin_policy_result=unavailable_in_this_query\n");
		} else {
			return 1;
		}
	}
	return 0;
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
	printf("version_response_length=%zu\n", apdu.resplen);
	if (apdu.sw1 == 0x90 && apdu.sw2 == 0) {
		if (apdu.resplen > sizeof(version)) {
			printf("version_result=response_exceeds_buffer_stop\n");
			return 1;
		}
		print_hex("version_bytes", version, apdu.resplen);
		printf("version_result=%s\n", apdu.resplen == sizeof(version) ?
				"six_bytes_uninterpreted" : "different_length_uninterpreted");
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

/* Parse only the PuKDF7004 named in this application's ODF. The generic
 * get_objects API can swallow parse errors and search trusted directories;
 * propagate this directory's result and never follow any key value/path. */
static int inspect_public_key_metadata(sc_pkcs15_card_t *p15, const struct sc_aid *aid)
{
	sc_pkcs15_df_t *df, *target = NULL;
	sc_pkcs15_object_t *obj;
	unsigned int dirs = 0, seen = 0, count = 0;
	int rv;
	for (df = p15->df_list; df; df = df->next) {
		if (++dirs > 32) return 1;
		if (df->type != SC_PKCS15_PUKDF) continue;
		if (target || (df->path.len != 2 && df->path.len != 4) ||
				(df->path.len == 4 && (df->path.value[0] != 0x3F || df->path.value[1] != 0)) ||
				df->path.value[df->path.len - 2] != 0x70 || df->path.value[df->path.len - 1] != 4 ||
				(df->path.aid.len && (df->path.aid.len != aid->len ||
				memcmp(df->path.aid.value, aid->value, aid->len)))) {
			printf("public_keys_result=directory_mismatch_no_parse\n");
			return 1;
		}
		target = df;
	}
	if (!target) {
		printf("public_keys_result=directory_not_advertised_no_parse\n");
		return 1;
	}
	rv = sc_pkcs15_parse_df(p15, target);
	print_result("public_keys_parse_result", rv);
	if (rv < 0) return 1;
	/* Validate all returned objects before emitting any partial inventory. */
	for (obj = p15->obj_list; obj; obj = obj->next) {
		if (++seen > 128) return 1;
		if (obj->df != target) continue;
		if ((obj->type & SC_PKCS15_TYPE_CLASS_MASK) != SC_PKCS15_TYPE_PUBKEY ||
				!obj->data || ++count >= 32) return 1;
	}
	printf("public_key_metadata_count=%u\n", count);
	count = 0;
	for (obj = p15->obj_list; obj; obj = obj->next) {
		const sc_pkcs15_pubkey_info_t *info;
		if (obj->df != target) continue;
		info = obj->data;
		printf("public_key_index=%u\npublic_key_algorithm=%s\npublic_key_reference=%d\n",
				count++, obj->type == SC_PKCS15_TYPE_PUBKEY_RSA ? "RSA" :
				obj->type == SC_PKCS15_TYPE_PUBKEY_EC ? "EC" : "other_uninterpreted", info->key_reference);
		printf("public_key_native=%d\npublic_key_usage=%u\n", info->native, info->usage);
		if (obj->type == SC_PKCS15_TYPE_PUBKEY_RSA)
			printf("public_key_rsa_bits=%zu\n", info->modulus_length);
		if (obj->type == SC_PKCS15_TYPE_PUBKEY_EC)
			printf("public_key_ec_bits=%zu\n", info->field_length);
	}
	printf("public_keys_result=metadata_only_no_referenced_key_reads\n");
	return 0;
}

/* Only CDF7005 from this AID's native ODF; never follow certificate paths.
 * get_objects may hide parse errors or search trusted CDFs, so parse explicitly. */
static int inspect_certificate_paths(sc_pkcs15_card_t *p15, const struct sc_aid *aid)
{
	sc_pkcs15_df_t *df, *target = NULL;
	sc_pkcs15_object_t *obj;
	unsigned int dirs = 0, seen = 0, count = 0;
	int rv;
	for (df = p15->df_list; df; df = df->next) {
		if (++dirs > 32) return 1;
		if (df->type != SC_PKCS15_CDF) continue;
		if (target || (df->path.len != 2 && df->path.len != 4) ||
				(df->path.type != SC_PATH_TYPE_PATH && df->path.type != SC_PATH_TYPE_FILE_ID) ||
				(df->path.len == 4 && memcmp(df->path.value, "\x3F\x00", 2)) ||
				df->path.value[df->path.len - 2] != 0x70 || df->path.value[df->path.len - 1] != 5 ||
				(df->path.aid.len && (df->path.aid.len != aid->len ||
				memcmp(df->path.aid.value, aid->value, aid->len)))) {
			printf("certificate_paths_result=directory_mismatch_no_parse\n"); return 1;
		}
		target = df;
	}
	if (!target) {
		printf("certificate_paths_result=directory_not_advertised_no_parse\n"); return 1;
	}
	rv = sc_pkcs15_parse_df(p15, target);
	print_result("certificate_paths_parse_result", rv);
	if (rv < 0) return 1;
	/* Validate the entire result before emitting routes. No partial inventory. */
	for (obj = p15->obj_list; obj; obj = obj->next) {
		const sc_pkcs15_cert_info_t *info;
		if (++seen > 128) return 1;
		if (obj->df != target) continue;
		if (obj->type != SC_PKCS15_TYPE_CERT_X509 || !obj->data || ++count > 32) return 1;
		info = obj->data;
		if (info->value.len) {
			if (!info->value.value || info->path.len) return 1;
			continue; /* Inline DER is neither printed nor followed. */
		}
		if (info->path.len < 2 || info->path.len > sizeof(info->path.value) || info->path.len % 2 ||
				(info->path.type != SC_PATH_TYPE_PATH && info->path.type != SC_PATH_TYPE_FILE_ID) ||
				info->path.index < 0 || info->path.count < -1 ||
				(info->path.aid.len && (info->path.aid.len != aid->len ||
				memcmp(info->path.aid.value, aid->value, aid->len)))) return 1;
	}
	printf("certificate_path_metadata_count=%u\n", count);
	count = 0;
	for (obj = p15->obj_list; obj; obj = obj->next) {
		const sc_pkcs15_cert_info_t *info;
		if (obj->df != target) continue;
		info = obj->data;
		printf("certificate_path_index=%u\ncertificate_path_private=%u\n", count++,
				!!(obj->flags & SC_PKCS15_CO_FLAG_PRIVATE));
		if (info->value.len) {
			printf("certificate_storage=inline_contents_omitted\n"); continue;
		}
		printf("certificate_storage=path_no_follow\n");
		print_hex("certificate_path", info->path.value, info->path.len);
		printf("certificate_path_offset=%d\ncertificate_path_count=%d\n", info->path.index, info->path.count);
	}
	printf("certificate_paths_result=cdf7005_metadata_only_no_certificate_reads\n");
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
	if (mode == INSPECT_PUBLIC_KEYS) {
		status = inspect_public_key_metadata(p15, aid);
		goto out;
	}
	if (mode == INSPECT_CERTIFICATE_PATHS) {
		status = inspect_certificate_paths(p15, aid);
		goto out;
	}
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
		if (mode == INSPECT_PIN_STATUS || mode == INSPECT_PIN_POLICY) {
			if (pin_matches != 1 || !pin_valid) {
				printf("%s\n", mode == INSPECT_PIN_POLICY ? "pin_policy_result=metadata_mismatch_no_get_data" :
						"pin_status_result=metadata_mismatch_no_verify");
				goto out;
			}
			status = mode == INSPECT_PIN_POLICY ? probe_pin_policy(card, aid) :
					probe_pin_status(card, aid, (unsigned char)pin_reference);
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
			strcmp(argv[3], "--pin-status") && strcmp(argv[3], "--security-info") && strcmp(argv[3], "--pin-policy") && strcmp(argv[3], "--odf-map") && strcmp(argv[3], "--file4141") && strcmp(argv[3], "--file4109") && strcmp(argv[3], "--file7001") && strcmp(argv[3], "--file7002") && strcmp(argv[3], "--file7004") && strcmp(argv[3], "--file7005") && strcmp(argv[3], "--fileb001") && strcmp(argv[3], "--fileb002") && strcmp(argv[3], "--filedf0f") && strcmp(argv[3], "--public-keys") && strcmp(argv[3], "--certificate-paths")) ||
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
			!strcmp(argv[3], "--pin-status") ? INSPECT_PIN_STATUS :
			!strcmp(argv[3], "--odf-map") ? INSPECT_ODF_MAP :
			!strcmp(argv[3], "--file4141") ? INSPECT_FILE4141 :
			!strcmp(argv[3], "--file4109") ? INSPECT_FILE4109 :
			!strcmp(argv[3], "--file7001") ? INSPECT_FILE7001 :
			!strcmp(argv[3], "--file7002") ? INSPECT_FILE7002 :
			!strcmp(argv[3], "--file7004") ? INSPECT_FILE7004 :
			!strcmp(argv[3], "--file7005") ? INSPECT_FILE7005 :
			!strcmp(argv[3], "--filedf0f") ? INSPECT_FILEDF0F :
			!strcmp(argv[3], "--fileb001") ? INSPECT_FILEB001 :
			!strcmp(argv[3], "--fileb002") ? INSPECT_FILEB002 :
			!strcmp(argv[3], "--public-keys") ? INSPECT_PUBLIC_KEYS :
			!strcmp(argv[3], "--certificate-paths") ? INSPECT_CERTIFICATE_PATHS :
			!strcmp(argv[3], "--pin-policy") ? INSPECT_PIN_POLICY : INSPECT_SECURITY_INFO;
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
	status = argc == 5 ? ((mode == INSPECT_FILE4141 || mode == INSPECT_FILE4109 || mode == INSPECT_FILE7001 || mode == INSPECT_FILE7002 || mode == INSPECT_FILE7004 || mode == INSPECT_FILE7005 || mode == INSPECT_FILEDF0F || mode == INSPECT_FILEB001 || mode == INSPECT_FILEB002) ?
		inspect_file_metadata(card, &app_aid, mode == INSPECT_FILE4141 ? 0x4141 : mode == INSPECT_FILE4109 ? 0x4109 : mode == INSPECT_FILE7001 ? 0x7001 : mode == INSPECT_FILE7002 ? 0x7002 : mode == INSPECT_FILE7004 ? 0x7004 : mode == INSPECT_FILE7005 ? 0x7005 : mode == INSPECT_FILEB001 ? 0xB001 : mode == INSPECT_FILEB002 ? 0xB002 : 0xDF0F) :
		mode == INSPECT_ODF_MAP ? inspect_odf_map(card, &app_aid) :
		inspect_native_pkcs15(card, &app_aid, mode)) : 0;
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
