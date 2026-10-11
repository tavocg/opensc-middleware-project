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
	return failures ? 1 : 0;
}
