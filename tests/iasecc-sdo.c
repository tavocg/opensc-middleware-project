/* Replay non-secret CHV metadata with the actual IAS/ECC SDO parser. */
#include <stdio.h>
#include <string.h>

#include "libopensc/iasecc-sdo.c"

static int failures;

static void check(const char *name, int valid)
{
	if (!valid) {
		fprintf(stderr, "FAIL %s\n", name);
		failures++;
	} else {
		printf("PASS %s\n", name);
	}
}

static void check_chipdoc(void)
{
	struct sc_context ctx = {0};
	struct sc_card card = {0};
	struct iasecc_sdo sdo = {0}, copy = {0};
	unsigned char captured[] = {
		0x70,0x2B,0xBF,0x81,0x01,0x27,0xA0,0x25,0xC9,0x01,0x01,
		0x9A,0x01,0x07,0x9B,0x01,0x07,0x80,0x02,0x00,0x0E,
		0xA1,0x16,0x8C,0x09,0xF3,0xE7,0x00,0xE7,0xFF,0x00,0xD0,0xD3,0xD3,
		0x9C,0x09,0xF3,0xE7,0x00,0xE7,0xFF,0x00,0xD0,0xD3,0xD3
	};
	unsigned char changed[sizeof(captured)];
	int result;

	ctx.app_name = "iasecc-chipdoc-sdo-regression";
	card.ctx = &ctx;
	card.type = SC_CARD_TYPE_IASECC_AMOS;
	sdo.sdo_class = IASECC_SDO_CLASS_CHV;
	sdo.sdo_ref = 1;
	result = iasecc_sdo_parse(&card, captured, sizeof(captured), &sdo);
	check("captured ChipDoc DOCP parses with its opaque C9 extension", result == SC_SUCCESS &&
			sdo.docp.chipdoc_c9.size == 1 && sdo.docp.chipdoc_c9.value &&
			*sdo.docp.chipdoc_c9.value == 1);
	check("captured ChipDoc retry counts and storage size survive", result == SC_SUCCESS &&
			sdo.docp.tries_maximum.value && *sdo.docp.tries_maximum.value == 7 &&
			sdo.docp.tries_remaining.value && *sdo.docp.tries_remaining.value == 7 &&
			sdo.docp.size.size == 2 && sdo.docp.size.value && sdo.docp.size.value[1] == 14);
	check("ChipDoc supports only the observed VERIFY and GET DATA conditions", result == SC_SUCCESS &&
			sdo.docp.amb == 0x21 && sdo.docp.scbs[1] == 0 && sdo.docp.scbs[6] == 0 &&
			sdo.docp.scbs[0] == 0xFF && sdo.docp.scbs[2] == 0xFF &&
			sdo.docp.scbs[3] == 0xFF && sdo.docp.scbs[4] == 0xFF && sdo.docp.scbs[5] == 0xFF);
	check("raw contact and contactless ACLs survive", result == SC_SUCCESS &&
			sdo.docp.acls_contact.size == 9 && sdo.docp.acls_contactless.size == 9 &&
			!memcmp(sdo.docp.acls_contact.value, captured+25, 9) &&
			!memcmp(sdo.docp.acls_contactless.value, captured+36, 9));
	result = iasecc_docp_copy(&ctx, &sdo.docp, &copy.docp);
	check("DOCP copy retains an independent C9 field", result == SC_SUCCESS &&
			copy.docp.chipdoc_c9.value && copy.docp.chipdoc_c9.value != sdo.docp.chipdoc_c9.value &&
			copy.docp.chipdoc_c9.size == 1 && *copy.docp.chipdoc_c9.value == 1);
	iasecc_sdo_free_fields(&card, &copy);
	result = iasecc_parse_acls(&card, &sdo.docp, 1);
	check("ChipDoc contactless policy remains unsupported", result == SC_ERROR_NOT_SUPPORTED);
	iasecc_sdo_free_fields(&card, &sdo);

	for (int variant = 0; variant < 4; variant++) {
		memcpy(changed, captured, sizeof(changed));
		card.type = SC_CARD_TYPE_IASECC_AMOS;
		sdo.sdo_class = IASECC_SDO_CLASS_CHV;
		sdo.sdo_ref = 1;
		if (variant == 0) changed[27] = 0x11; /* Change VERIFY protection. */
		if (variant == 1) card.type = SC_CARD_TYPE_IASECC_GEMALTO;
		if (variant == 2) sdo.sdo_ref = changed[4] = 2;
		if (variant == 3) changed[10] = 2;
		result = iasecc_sdo_parse(&card, changed, sizeof(changed), &sdo);
		check(variant == 0 ? "different ChipDoc ACL is rejected" :
				variant == 1 ? "C9 on other card types remains rejected" :
				variant == 2 ? "C9 on other CHV references remains rejected" :
				"other C9 values remain rejected",
				result == (variant == 0 ? SC_ERROR_NOT_SUPPORTED : SC_ERROR_UNKNOWN_DATA_RECEIVED));
		iasecc_sdo_free_fields(&card, &sdo);
	}
}

static void check_rsa_policy(void)
{
	struct sc_context ctx = {0};
	struct sc_card card = {0};
	struct iasecc_sdo sdo = {0};
	/* Captured DOCP only: no RSA private components. Both references have
	 * identical metadata apart from the object reference byte.
	 */
	unsigned char captured[] = {
		0x70,0x2B,0xBF,0x90,0x02,0x27,0xA0,0x25,0x9E,0x01,0x00,
		0x80,0x02,0x01,0x00,0xA1,0x1C,
		0x8C,0x0C,0xC0,0x13,0x90,0x13,0xA0,0x13,0x88,0x13,0x82,0xD3,0x81,0x00,
		0x9C,0x0C,0xC0,0x13,0x90,0x13,0xA0,0x13,0x88,0x13,0x82,0xD3,0x81,0x00
	};
	static const unsigned char expected[] = {0x13,0x13,0x13,0x13,0xFF,0xD3,0x00};
	unsigned char duplicate[] = {0x40,0x13,0x40,0x13};
	unsigned char conflicting[] = {0x40,0x13,0x40,0x00};
	unsigned char truncated[] = {0x40,0x13,0x03,0x00};
	unsigned method, reference;
	int result;

	ctx.app_name = "iasecc-rsa-policy-regression";
	card.ctx = &ctx;
	card.type = SC_CARD_TYPE_IASECC_AMOS;
	for (int key_ref = 2; key_ref <= 3; key_ref++) {
		sdo.sdo_class = IASECC_SDO_CLASS_RSA_PRIVATE;
		sdo.sdo_ref = captured[4] = key_ref;
		result = iasecc_sdo_parse(&card, captured, sizeof(captured), &sdo);
		check(key_ref == 2 ? "captured RSA reference 2 retains all ACL groups" :
				"captured RSA reference 3 retains all ACL groups",
				result == SC_SUCCESS && sdo.sdo_class == IASECC_SDO_CLASS_RSA_PRIVATE &&
				sdo.sdo_ref == key_ref && sdo.docp.amb == 0x7B &&
				!memcmp(sdo.docp.scbs, expected, sizeof(expected)) &&
				sdo.docp.size.size == 2 && sdo.docp.size.value[0] == 1 &&
				sdo.docp.size.value[1] == 0);
		if (key_ref == 2)
			iasecc_sdo_free_fields(&card, &sdo);
	}
	result = iasecc_sdo_convert_acl(&card, &sdo, SC_AC_OP_PSO_COMPUTE_SIGNATURE, &method, &reference);
	check("RSA PSO signature requires user authentication through SE 3",
			result == SC_SUCCESS && method == SC_AC_SEN && reference == 3);
	result = iasecc_sdo_convert_acl(&card, &sdo, SC_AC_OP_INTERNAL_AUTHENTICATE, &method, &reference);
	check("RSA internal authentication retains its later SE 3 rule",
			result == SC_SUCCESS && method == SC_AC_SEN && reference == 3);
	result = iasecc_sdo_convert_acl(&card, &sdo, SC_AC_OP_READ, &method, &reference);
	check("RSA metadata GET DATA retains its later unrestricted rule",
			result == SC_SUCCESS && method == SC_AC_NONE);
	result = iasecc_sdo_convert_acl(&card, &sdo, SC_AC_OP_PSO_DECRYPT, &method, &reference);
	check("RSA decipher retains its later SE 3 authentication rule",
			result == SC_SUCCESS && method == SC_AC_SEN && reference == 3);
	result = iasecc_sdo_convert_acl(&card, &sdo, SC_AC_OP_UPDATE, &method, &reference);
	check("RSA PUT DATA preserves its combined protection byte",
			result == SC_SUCCESS && method == SC_AC_SCB && reference == 0xD3);
	result = iasecc_parse_acls(&card, &sdo.docp, 1);
	check("RSA contactless metadata is parsed without performing contactless operations",
			result == SC_SUCCESS && sdo.docp.amb == 0x7B &&
			!memcmp(sdo.docp.scbs, expected, sizeof(expected)));
	iasecc_sdo_free_fields(&card, &sdo);

	/* Direct ACL fixtures also exercise malformed later groups, which the
	 * former parser silently ignored. These buffers are not heap-owned.
	 */
	sdo.docp.acls_contact.value = duplicate;
	sdo.docp.acls_contact.size = sizeof(duplicate);
	result = iasecc_parse_acls(&card, &sdo.docp, 0);
	check("identical repeated SDO conditions are accepted", result == SC_SUCCESS &&
			sdo.docp.amb == 0x40 && sdo.docp.scbs[0] == 0x13);
	sdo.docp.acls_contact.value = conflicting;
	sdo.docp.acls_contact.size = sizeof(conflicting);
	result = iasecc_parse_acls(&card, &sdo.docp, 0);
	check("different alternative SDO conditions are rejected", result == SC_ERROR_NOT_SUPPORTED);
	sdo.docp.acls_contact.value = truncated;
	sdo.docp.acls_contact.size = sizeof(truncated);
	result = iasecc_parse_acls(&card, &sdo.docp, 0);
	check("a truncated later SDO group is rejected", result == SC_ERROR_INVALID_DATA);
}

int main(void)
{
	struct sc_context ctx = {0};
	struct sc_card card = {0};
	struct iasecc_sdo sdo = {0};
	/* Captured at MF: maximum length 14, minimum length 4; no PIN value. */
	unsigned char chv[] = {
		0x70, 0x0D, 0xBF, 0x81, 0x01, 0x09,
		0x7F, 0x41, 0x06, 0x80, 0x01, 0x0E, 0x81, 0x01, 0x04
	};
	/* Standard DOCP fixture for the same object, with a single compact ACL
	 * group. Used to test accumulation across the two GET DATA responses.
	 */
	unsigned char docp[] = {
		0x70, 0x16, 0xBF, 0x81, 0x01, 0x12, 0xA0, 0x10,
		0x9A, 0x01, 0x07, 0x9B, 0x01, 0x07, 0x80, 0x02, 0x00, 0x0E,
		0xA1, 0x04, 0x8C, 0x02, 0x20, 0x00
	};
	/* Valid TLV syntax with an unknown CHV field: this must still fail. */
	unsigned char unknown[] = {
		0x70, 0x0A, 0xBF, 0x81, 0x01, 0x06, 0x7F, 0x41, 0x03, 0x83, 0x01, 0x00
	};
	int result;

	ctx.app_name = "iasecc-sdo-regression";
	card.ctx = &ctx;
	sdo.sdo_class = IASECC_SDO_CLASS_CHV;
	sdo.sdo_ref = 1;
	result = iasecc_sdo_parse(&card, chv, sizeof(chv), &sdo);
	check("captured CHV metadata retains class and reference", result == SC_SUCCESS &&
			sdo.sdo_class == IASECC_SDO_CLASS_CHV && sdo.sdo_ref == 1);
	check("captured CHV metadata retains length limits", sdo.data.chv.size_min.size == 1 &&
			sdo.data.chv.size_min.value && *sdo.data.chv.size_min.value == 4 &&
			sdo.data.chv.size_max.size == 1 && sdo.data.chv.size_max.value &&
			*sdo.data.chv.size_max.value == 14);

	result = iasecc_sdo_parse(&card, docp, sizeof(docp), &sdo);
	check("DOCP response preserves previously read CHV limits", result == SC_SUCCESS &&
			sdo.data.chv.size_min.value && *sdo.data.chv.size_min.value == 4 &&
			sdo.data.chv.size_max.value && *sdo.data.chv.size_max.value == 14);
	check("DOCP response retains retry counts and VERIFY condition", result == SC_SUCCESS &&
			sdo.docp.tries_maximum.value && *sdo.docp.tries_maximum.value == 7 &&
			sdo.docp.tries_remaining.value && *sdo.docp.tries_remaining.value == 7 &&
			sdo.docp.scbs[IASECC_ACLS_CHV_VERIFY] == 0 &&
			sdo.docp.scbs[IASECC_ACLS_CHV_CHANGE] == 0xFF);
	iasecc_sdo_free_fields(&card, &sdo);

	sdo.sdo_class = IASECC_SDO_CLASS_CHV;
	sdo.sdo_ref = 1;
	result = iasecc_sdo_parse(&card, unknown, sizeof(unknown), &sdo);
	check("unknown CHV data still returns an error", result == SC_ERROR_UNKNOWN_DATA_RECEIVED);
	iasecc_sdo_free_fields(&card, &sdo);
	check_chipdoc();
	check_rsa_policy();
	return failures ? 1 : 0;
}
