/* Exercise the real PKCS#15 query's selection, error and metadata behavior.
 * The card command is a stub: no actual reader, PIN or APDU.
 */
#include <stdio.h>
#include <string.h>
#include "libopensc/pkcs15-pin.c"

static int failures, selections, queries, status_queries, locks, unlocks;
static int lock_error, command_error, status_unsupported;
static int state = SC_PIN_STATE_LOGGED_IN, tries = -1;

/* These hidden helpers belong to unrelated functions included with this file.
 * Any use of PIN-content caching during a status query is a test failure.
 */
void sc_pkcs15_free_object_content(struct sc_pkcs15_object *obj) { abort(); }
int sc_pkcs15_allocate_object_content(struct sc_context *ctx,
		struct sc_pkcs15_object *obj, const unsigned char *data, size_t len) { abort(); }

static void check(const char *name, int valid)
{
	printf("%s %s\n", valid ? "PASS" : "FAIL", name);
	if (!valid) failures++;
}
int sc_lock(struct sc_card *card) { locks++; return lock_error; }
int sc_unlock(struct sc_card *card) { unlocks++; return 0; }
int sc_select_file(struct sc_card *card, const struct sc_path *path, struct sc_file **file)
{
	selections++;
	return 0;
}
int sc_pin_cmd(struct sc_card *card, struct sc_pin_cmd_data *data)
{
	queries++;
	if (data->pin1.data || data->pin1.len || data->pin2.data || data->pin2.len ||
			data->cmd != SC_PIN_CMD_GET_INFO)
		return SC_ERROR_INVALID_ARGUMENTS;
	if (data->flags & SC_PIN_CMD_STATUS_ONLY) {
		status_queries++;
		if (status_unsupported) return SC_ERROR_NOT_SUPPORTED;
	}
	if (command_error) return command_error;
	data->pin1.max_tries = 7;
	data->pin1.tries_left = tries;
	data->pin1.logged_in = state;
	return 0;
}
static void clear_counts(void)
{
	selections = queries = status_queries = locks = unlocks = 0;
	lock_error = command_error = status_unsupported = 0;
}

int main(void)
{
	const unsigned char aid[] = {
		0xE8,0x28,0xBD,0x08,0x0F,0xD2,0x50,0x47,0x65,0x6E,0x65,0x72,0x69,0x63
	};
	struct sc_context ctx = {0};
	struct sc_card card = {0};
	struct sc_app_info app = {0};
	struct sc_pkcs15_card p15 = {0};
	struct sc_pkcs15_auth_info info = {0}, original;
	struct sc_pkcs15_object obj = {0};
	int rv;
	card.ctx = &ctx;
	card.type = SC_CARD_TYPE_IASECC_AMOS;
	app.aid.len = sizeof(aid);
	memcpy(app.aid.value, aid, sizeof(aid));
	p15.card = &card;
	p15.app = &app;
	info.auth_type = SC_PKCS15_PIN_AUTH_TYPE_PIN;
	info.auth_method = SC_AC_CHV;
	info.attrs.pin.reference = 1;
	info.max_tries = 7;
	info.tries_left = 2;
	sc_format_path("3F00", &info.path);
	info.path.type = SC_PATH_TYPE_FILE_ID;
	obj.data = &info;
	original = info;
	unsetenv("OPENSC_JCOP3_PIN_STATUS_IN_PLACE");
	rv = sc_pkcs15_get_pin_info(&p15, &obj);
	check("default query preserves path selection and full driver info",
			rv == 0 && selections == 1 && queries == 1 && !status_queries && locks == 1 && unlocks == 1);
	clear_counts();
	setenv("OPENSC_JCOP3_PIN_STATUS_IN_PLACE", "yes", 1);
	rv = sc_pkcs15_get_pin_info(&p15, &obj);
	check("candidate reads physical state without selecting or inventing remaining tries",
			rv == 0 && !selections && queries == 1 && status_queries == 1 &&
			info.logged_in == SC_PIN_STATE_LOGGED_IN && info.tries_left == -1 && info.max_tries == 7 &&
			locks == 1 && unlocks == 1);
	state = SC_PIN_STATE_LOGGED_OUT;
	tries = 2;
	rv = sc_pkcs15_get_pin_info(&p15, &obj);
	check("candidate repeated query detects lost login and current retry counter",
			rv == 0 && !selections && queries == 2 && info.logged_in == state && info.tries_left == 2);
	clear_counts();
	status_unsupported = 1;
	rv = sc_pkcs15_get_pin_info(&p15, &obj);
	check("unknown driver policy falls back to the original initialization query",
			rv == 0 && selections == 1 && queries == 2 && status_queries == 1 && locks == 1 && unlocks == 1);
	clear_counts();
	info.logged_in = SC_PIN_STATE_LOGGED_IN;
	command_error = SC_ERROR_CARD_RESET;
	rv = sc_pkcs15_get_pin_info(&p15, &obj);
	check("candidate query error clears stale physical state without fallback or retry",
			rv == command_error && !selections && queries == 1 && info.logged_in == SC_PIN_STATE_UNKNOWN &&
			info.tries_left == -1 && unlocks == 1);
	clear_counts();
	info.logged_in = SC_PIN_STATE_LOGGED_IN;
	info.tries_left = 2;
	lock_error = SC_ERROR_CARD_REMOVED;
	rv = sc_pkcs15_get_pin_info(&p15, &obj);
	check("failed locking invalidates candidate observations without commands or unlocking",
			rv == lock_error && !selections && !queries && !unlocks &&
			info.logged_in == SC_PIN_STATE_UNKNOWN && info.tries_left == -1);
	clear_counts();
	setenv("OPENSC_JCOP3_PIN_STATUS_IN_PLACE", "no", 1);
	rv = sc_pkcs15_get_pin_info(&p15, &obj);
	check("candidate requires exact opt-in", rv == 0 && selections == 1 && !status_queries);
	setenv("OPENSC_JCOP3_PIN_STATUS_IN_PLACE", "yes", 1);
	for (int variant = 0; variant < 12; variant++) {
		info = original;
		card.type = SC_CARD_TYPE_IASECC_AMOS;
		p15.app = &app;
		memcpy(app.aid.value, aid, sizeof(aid));
		clear_counts();
		switch (variant) {
		case 0: card.type = SC_CARD_TYPE_IASECC_SAGEM; break;
		case 1: app.aid.value[0] ^= 1; break;
		case 2: info.attrs.pin.flags = SC_PKCS15_PIN_FLAG_LOCAL; break;
		case 3: info.attrs.pin.reference = 2; break;
		case 4: info.auth_type = SC_PKCS15_PIN_AUTH_TYPE_AUTH_KEY; break;
		case 5: info.auth_method = SC_AC_AUT; break;
		case 6: info.path.value[1] = 1; break;
		case 7: info.path.aid.len = 1; break;
		case 8: info.path.type = SC_PATH_TYPE_PATH; break;
		case 9: p15.app = NULL; break;
		case 10: info.attrs.pin.flags = SC_PKCS15_PIN_FLAG_SO_PIN; break;
		case 11: info.attrs.pin.flags = SC_PKCS15_PIN_FLAG_UNBLOCKING_PIN; break;
		}
		int excluded = !chipdoc_pin_status_in_place(&p15, &info);
		rv = sc_pkcs15_get_pin_info(&p15, &obj);
		char name[96];
		snprintf(name, sizeof(name), "candidate excludes profile variant %d without status-only commands", variant);
		check(name, excluded && !status_queries &&
				(variant == 4 ? rv == SC_ERROR_INVALID_DATA : rv == 0 && selections == 1));
	}
	unsetenv("OPENSC_JCOP3_PIN_STATUS_IN_PLACE");
	return failures ? 1 : 0;
}
