/* Test the actual PKCS#15 fixup against the observed non-secret PIN metadata.
 * No reader, APDU transport or PIN value is used.
 */
#include <stdio.h>
#include <string.h>

#include "libopensc/pkcs15-iasecc.c"

static int failures;

enum variant {
	OBSERVED, LOCAL, OTHER_REFERENCE, OTHER_PATH, OTHER_APP,
	OTHER_CARD, AUTH_KEY, OTHER_PATH_AID, APP_PATH_AID
};

static void check_path(const char *name, enum variant variant, int expect_fixup)
{
	struct sc_context ctx = {0};
	struct sc_card card = {0};
	struct sc_app_info app = {0};
	struct sc_pkcs15_card p15card = {0};
	struct sc_pkcs15_object obj = {0};
	struct sc_pkcs15_auth_info info = {0};
	struct sc_path original;
	const unsigned char aid[] = {
		0xE8, 0x28, 0xBD, 0x08, 0x0F, 0xD2, 0x50,
		0x47, 0x65, 0x6E, 0x65, 0x72, 0x69, 0x63
	};
	int valid;

	ctx.app_name = "iasecc-pin-path-regression";
	card.ctx = &ctx;
	card.type = SC_CARD_TYPE_IASECC_AMOS;
	p15card.card = &card;
	p15card.app = &app;
	p15card.obj_list = &obj;
	app.aid.len = sizeof(aid);
	memcpy(app.aid.value, aid, sizeof(aid));
	obj.type = SC_PKCS15_TYPE_AUTH_PIN;
	obj.data = &info;
	info.auth_type = SC_PKCS15_PIN_AUTH_TYPE_PIN;
	info.attrs.pin.reference = 1;
	info.attrs.pin.flags = SC_PKCS15_PIN_FLAG_CASE_SENSITIVE | SC_PKCS15_PIN_FLAG_INITIALIZED;
	sc_format_path("0001", &info.path);

	switch (variant) {
	case LOCAL: info.attrs.pin.flags |= SC_PKCS15_PIN_FLAG_LOCAL; break;
	case OTHER_REFERENCE: info.attrs.pin.reference = 2; break;
	case OTHER_PATH: info.path.value[1] = 2; break;
	case OTHER_APP: app.aid.value[0] ^= 1; break;
	case OTHER_CARD: card.type = SC_CARD_TYPE_IASECC_GEMALTO; break;
	case AUTH_KEY:
		obj.type = SC_PKCS15_TYPE_AUTH_AUTHKEY;
		info.auth_type = SC_PKCS15_PIN_AUTH_TYPE_AUTH_KEY;
		break;
	case OTHER_PATH_AID:
	case APP_PATH_AID:
		info.path.aid = app.aid;
		if (variant == OTHER_PATH_AID)
			info.path.aid.value[0] ^= 1;
		break;
	default: break;
	}
	original = info.path;
	_iasecc_fixup_global_pin_paths(&p15card);
	if (expect_fixup) {
		valid = info.path.type == SC_PATH_TYPE_FILE_ID && info.path.len == 2 &&
			info.path.value[0] == 0x3F && info.path.value[1] == 0 && !info.path.aid.len;
	} else {
		valid = !memcmp(&info.path, &original, sizeof(original));
	}
	if (!valid) {
		fprintf(stderr, "FAIL %s\n", name);
		failures++;
	} else {
		printf("PASS %s\n", name);
	}
}

int main(void)
{
	check_path("observed global CHV 1 uses MF", OBSERVED, 1);
	check_path("global CHV 1 with application AID uses MF", APP_PATH_AID, 1);
	check_path("local PIN keeps its path", LOCAL, 0);
	check_path("other CHV reference keeps its path", OTHER_REFERENCE, 0);
	check_path("ordinary file path stays unchanged", OTHER_PATH, 0);
	check_path("other application stays unchanged", OTHER_APP, 0);
	check_path("other card profile stays unchanged", OTHER_CARD, 0);
	check_path("authentication key stays unchanged", AUTH_KEY, 0);
	check_path("other path AID stays unchanged", OTHER_PATH_AID, 0);
	return failures ? 1 : 0;
}
