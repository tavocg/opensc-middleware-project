/* Test the opt-in session/token-info experiment in the real PKCS#15 framework.
 * No reader, login, PIN or APDU: the physical status query is a stub.
 */
#include <stdio.h>
#include <string.h>
#include "pkcs11/framework-pkcs15.c"

struct sc_context *context;
static int failures, status_queries, session_state_events, query_error, token_events, token_cached_events;
static CK_RV token_pin_rv;
static struct sc_pkcs11_slot *fixture_slot;

CK_RV sc_pkcs11_lock(void) { return CKR_OK; }
void sc_pkcs11_unlock(void) {}
const char *lookup_enum(CK_ULONG type, CK_ULONG value) { return NULL; }
CK_RV sc_to_cryptoki_error(int rc, const char *ctx) { return CKR_GENERAL_ERROR; }
CK_RV slot_get_token(CK_SLOT_ID id, struct sc_pkcs11_slot **slot)
{
	if (id != 0) return CKR_SLOT_ID_INVALID;
	*slot = fixture_slot;
	return CKR_OK;
}

int sc_pkcs15_get_pin_info(struct sc_pkcs15_card *p15card, struct sc_pkcs15_object *obj)
{
	(void)p15card;
	status_queries++;
	if (query_error) return SC_ERROR_CARD_RESET;
	((struct sc_pkcs15_auth_info *)obj->data)->logged_in = SC_PIN_STATE_LOGGED_OUT;
	return SC_SUCCESS;
}

void sc_pkcs11_api_trace(enum sc_api_trace_event event, CK_SESSION_HANDLE session,
		CK_RV rv, CK_ULONG a, CK_ULONG b)
{
	(void)session;
	if (event == SC_API_TOKEN_INFO_BEGIN || event == SC_API_TOKEN_INFO)
		token_events++;
	if (event == SC_API_TOKEN_PIN_STATUS)
		token_pin_rv = rv;
	if (event == SC_API_TOKEN_PIN_CACHED && rv == CKR_OK &&
			a == SC_PIN_STATE_UNKNOWN && b == CKU_USER)
		token_cached_events++;
	if (event == SC_API_SESSION_LOGIN_STATE && rv == CKR_OK &&
			a == SC_PIN_STATE_UNKNOWN && b == CKU_USER)
		session_state_events++;
}

static void check(const char *name, int valid)
{
	printf("%s %s\n", valid ? "PASS" : "FAIL", name);
	if (!valid) failures++;
}

int main(void)
{
	static const unsigned char aid[] = {
		0xE8,0x28,0xBD,0x08,0x0F,0xD2,0x50,0x47,0x65,0x6E,0x65,0x72,0x69,0x63
	};
	struct sc_card card = {0};
	struct sc_app_info app = {0};
	struct sc_pkcs15_card p15card = {0};
	struct sc_pkcs15_auth_info pin_info = {0}, original;
	struct sc_pkcs15_object pin_obj = {0};
	struct sc_pkcs11_card p11card = {0};
	struct pkcs15_fw_data fw_data = {0};
	struct pkcs15_slot_data slot_data = {0};
	struct sc_pkcs11_slot slot = {0};
	int state;

	card.type = SC_CARD_TYPE_IASECC_AMOS;
	app.aid.len = sizeof(aid);
	memcpy(app.aid.value, aid, sizeof(aid));
	p15card.card = &card;
	p15card.app = &app;
	pin_info.auth_type = SC_PKCS15_PIN_AUTH_TYPE_PIN;
	pin_info.auth_method = SC_AC_CHV;
	pin_info.attrs.pin.reference = 1;
	pin_info.path.type = SC_PATH_TYPE_FILE_ID;
	pin_info.path.len = 2;
	pin_info.path.value[0] = 0x3F;
	pin_obj.type = SC_PKCS15_TYPE_AUTH_PIN;
	pin_obj.data = &pin_info;
	fw_data.p15_card = &p15card;
	p11card.fws_data[0] = &fw_data;
	slot_data.auth_obj = &pin_obj;
	slot.p11card = &p11card;
	slot.fw_data = &slot_data;
	slot.login_user = CKU_USER;
	fixture_slot = &slot;

	unsetenv("OPENSC_JCOP3_SESSION_STATE");
	state = slot_get_logged_in_state(&slot);
	check("default session query still consults the physical PIN", state == SC_PIN_STATE_LOGGED_OUT &&
			status_queries == 1 && !session_state_events);
	setenv("OPENSC_JCOP3_SESSION_STATE", "yes", 1);
	status_queries = 0;
	state = slot_get_logged_in_state(&slot);
	state |= slot_get_logged_in_state(&slot);
	check("opt-in authenticated ChipDoc sessions omit repeated physical queries and report UNKNOWN",
			state == SC_PIN_STATE_UNKNOWN && !status_queries && session_state_events == 2 &&
			pin_info.logged_in == SC_PIN_STATE_LOGGED_OUT && slot.login_user == CKU_USER);
	slot.login_user = -1;
	state = slot_get_logged_in_state(&slot);
	check("opt-in never creates a login for an unauthenticated session", state == SC_PIN_STATE_LOGGED_OUT &&
			status_queries == 1 && slot.login_user == -1 && session_state_events == 2);
	slot.login_user = CKU_USER;
	setenv("OPENSC_JCOP3_SESSION_STATE", "no", 1);
	status_queries = 0;
	state = slot_get_logged_in_state(&slot);
	check("other environment values do not enable the experiment", state == SC_PIN_STATE_LOGGED_OUT &&
			status_queries == 1 && session_state_events == 2);
	setenv("OPENSC_JCOP3_SESSION_STATE", "yes", 1);
	original = pin_info;
	for (int variant = 0; variant < 13; variant++) {
		const char *names[] = {
			"another card keeps physical status queries", "another AID keeps physical status queries",
			"a local PIN keeps physical status queries", "another CHV keeps physical status queries",
			"an authentication key keeps physical status queries", "another authentication method keeps physical status queries",
			"another PIN path keeps physical status queries", "a path with AID keeps physical status queries",
			"another path type keeps physical status queries", "missing application keeps physical status queries",
			"SO PIN is excluded from the experiment", "unblocking PIN is excluded from the experiment",
			"SO login is excluded from the experiment"
		};
		pin_info = original;
		card.type = SC_CARD_TYPE_IASECC_AMOS;
		p15card.app = &app;
		memcpy(app.aid.value, aid, sizeof(aid));
		slot.login_user = CKU_USER;
		switch (variant) {
		case 0: card.type = SC_CARD_TYPE_IASECC_GEMALTO; break;
		case 1: app.aid.value[0] ^= 1; break;
		case 2: pin_info.attrs.pin.flags = SC_PKCS15_PIN_FLAG_LOCAL; break;
		case 3: pin_info.attrs.pin.reference = 2; break;
		case 4: pin_info.auth_type = SC_PKCS15_PIN_AUTH_TYPE_AUTH_KEY; break;
		case 5: pin_info.auth_method = SC_AC_AUT; break;
		case 6: pin_info.path.value[1] = 1; break;
		case 7: pin_info.path.aid.len = 1; break;
		case 8: pin_info.path.type = SC_PATH_TYPE_PATH; break;
		case 9: p15card.app = NULL; break;
		case 10: pin_info.attrs.pin.flags = SC_PKCS15_PIN_FLAG_SO_PIN; break;
		case 11: pin_info.attrs.pin.flags = SC_PKCS15_PIN_FLAG_UNBLOCKING_PIN; break;
		case 12: slot.login_user = CKU_SO; break;
		}
		status_queries = 0;
		int excluded = !pkcs15_chipdoc_session_state(&p15card, &pin_info, slot.login_user);
		if (variant != 12) {
			state = slot_get_logged_in_state(&slot);
			excluded &= state == SC_PIN_STATE_LOGGED_OUT && status_queries == 1;
		}
		check(names[variant], excluded && session_state_events == 2);
		status_queries = 0;
		CK_TOKEN_INFO info;
		CK_RV token_rv = C_GetTokenInfo(0, &info);
		char name[160];
		snprintf(name, sizeof(name), "token-info: %s", names[variant]);
		check(name, token_rv == CKR_OK && status_queries == 1 && !token_cached_events);
	}
	pin_info = original;
	card.type = SC_CARD_TYPE_IASECC_AMOS;
	p15card.app = &app;
	memcpy(app.aid.value, aid, sizeof(aid));
	slot.login_user = CKU_USER;
	fixture_slot = &slot;
	CK_TOKEN_INFO token_info;
	status_queries = 0;
	token_events = 0;
	unsetenv("OPENSC_JCOP3_SESSION_STATE");
	CK_RV rv = C_GetTokenInfo(0, &token_info);
	check("default token-info still queries the physical PIN",
			rv == CKR_OK && status_queries == 1 && token_events == 2 && token_pin_rv == SC_SUCCESS);
	query_error = 1;
	rv = C_GetTokenInfo(0, &token_info);
	check("token-info diagnostic preserves its return while reporting the failed PIN query",
			rv == CKR_OK && status_queries == 2 && token_events == 4 &&
			token_pin_rv == (CK_RV)SC_ERROR_CARD_RESET);
	setenv("OPENSC_JCOP3_SESSION_STATE", "yes", 1);
	pin_info.logged_in = SC_PIN_STATE_UNKNOWN;
	pin_info.tries_left = 2;
	pin_info.max_tries = 7;
	rv = C_GetTokenInfo(0, &token_info);
	check("opt-in token-info uses cached retry metadata without querying or claiming physical login",
			rv == CKR_OK && status_queries == 2 && token_events == 6 && token_cached_events == 1 &&
			pin_info.logged_in == SC_PIN_STATE_UNKNOWN && slot.login_user == CKU_USER &&
			(token_info.flags & CKF_USER_PIN_COUNT_LOW) &&
			!(token_info.flags & (CKF_USER_PIN_LOCKED | CKF_USER_PIN_FINAL_TRY)));
	slot.login_user = -1;
	rv = C_GetTokenInfo(0, &token_info);
	check("opt-in token-info never substitutes a physical query before login",
			rv == CKR_OK && status_queries == 3 && token_cached_events == 1 && slot.login_user == -1);
	unsetenv("OPENSC_JCOP3_SESSION_STATE");
	return failures ? 1 : 0;
}
