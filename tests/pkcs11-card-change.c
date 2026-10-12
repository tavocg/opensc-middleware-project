/* Real slot detection and session cleanup with a simulated reader/framework.
 * Reproduce a changed card when the independent event wait reports no event.
 * No PIN, APDU, reader or Java process is used.
 */
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
#include "pkcs11/slot.c"
#include "pkcs11/pkcs11-session.c"
#include "pkcs11/misc.c"
#include "common/simclist.c"

struct sc_context *context;
struct sc_pkcs11_config sc_pkcs11_conf;
list_t virtual_slots, sessions;
static struct sc_pkcs11_slot fixture_slot;
static sc_reader_t fixture_reader, other_reader;
static int scenario, presence_calls, connects, disconnects, binds, failures, changed = 1;

CK_RV sc_pkcs11_lock(void) { return CKR_OK; }
void sc_pkcs11_unlock(void) {}
const char *lookup_enum(CK_ULONG type, CK_ULONG value) { return NULL; }
void sc_pkcs11_release_operation(sc_pkcs11_operation_t **op) { *op = NULL; }
int slot_get_logged_in_state(struct sc_pkcs11_slot *slot) { return SC_PIN_STATE_UNKNOWN; }
int slot_get_card_state(struct sc_pkcs11_slot *slot) { return sc_detect_card_presence(slot->reader); }
void sc_pkcs11_api_trace(enum sc_api_trace_event event, CK_SESSION_HANDLE session,
		CK_RV rv, CK_ULONG a, CK_ULONG b) {}

int sc_wait_for_event(sc_context_t *ctx, unsigned int mask, sc_reader_t **reader,
		unsigned int *events, int timeout, void **states)
{
	*reader = scenario == 1 ? &other_reader : &fixture_reader;
	*events = SC_EVENT_CARD_INSERTED;
	return scenario == 2 ? SC_SUCCESS : (scenario == 1 ? SC_SUCCESS : SC_ERROR_EVENT_TIMEOUT);
}

int sc_detect_card_presence(sc_reader_t *reader)
{
	presence_calls++;
	if (scenario == 4) return 0;
	if (scenario == 5) return SC_ERROR_CARD_REMOVED;
	if (scenario == 7 || (scenario != 3 && changed))
		return SC_READER_CARD_PRESENT | SC_READER_CARD_CHANGED;
	return SC_READER_CARD_PRESENT;
}

int sc_connect_card(sc_reader_t *reader, struct sc_card **card)
{
	connects++;
	if (scenario == 6) return SC_ERROR_CARD_REMOVED;
	*card = calloc(1, sizeof(**card));
	if (!*card) return SC_ERROR_OUT_OF_MEMORY;
	(*card)->ctx = context;
	(*card)->reader = reader;
	return SC_SUCCESS;
}

int sc_disconnect_card(struct sc_card *card)
{
	if (card) {
		disconnects++;
		changed = 0;
	}
	free(card);
	return SC_SUCCESS;
}

static CK_RV fixture_bind(struct sc_pkcs11_card *card, struct sc_app_info *app)
{
	binds++;
	return CKR_OK;
}

static CK_RV fixture_create_tokens(struct sc_pkcs11_card *card, struct sc_app_info *app)
{
	struct sc_pkcs11_slot *slot = NULL;
	CK_RV rv = slot_allocate(&slot, card);
	if (rv == CKR_OK) {
		slot->slot_info.flags |= CKF_TOKEN_PRESENT;
		slot->token_info.flags = CKF_LOGIN_REQUIRED;
	}
	return rv;
}

static CK_RV fixture_logout(struct sc_pkcs11_slot *slot) { return CKR_OK; }
struct sc_pkcs11_framework_ops framework_pkcs15 = {
	.bind = fixture_bind, .create_tokens = fixture_create_tokens, .logout = fixture_logout
};
#ifdef USE_PKCS15_INIT
struct sc_pkcs11_framework_ops framework_pkcs15init;
#endif

static int slot_seeker(const void *entry, const void *key)
{
	return ((const struct sc_pkcs11_slot *)entry)->id == *(const CK_SLOT_ID *)key;
}
static int session_seeker(const void *entry, const void *key)
{
	return ((const struct sc_pkcs11_session *)entry)->handle == *(const CK_SESSION_HANDLE *)key;
}

static void check(const char *name, int valid)
{
	printf("%s %s\n", valid ? "PASS" : "FAIL", name);
	if (!valid) failures++;
}

static int run_scenario(int variant)
{
	static const char *names[] = {
		"timeout cannot hide changed card", "another reader event cannot hide changed card",
		"matching event reconnects changed card", "unchanged card retains sessions",
		"absent card invalidates sessions despite timeout", "presence error is propagated",
		"failed reconnect leaves no token or old sessions", "persistent change has bounded retries",
		"repeated replacements reconnect"
	};
	struct sc_context ctx = {0};
	struct sc_pkcs11_card *old_card = calloc(1, sizeof(*old_card));
	struct sc_card *card = calloc(1, sizeof(*card));
	CK_SESSION_HANDLE old_session = 0, new_session = 0;
	CK_SESSION_INFO info;
	CK_RV rv;
	int expect_reconnect = variant <= 2 || variant == 8;

	if (!old_card || !card) return 1;
	scenario = variant;
	context = &ctx;
	fixture_reader.ctx = context;
	fixture_reader.name = "simulated reader";
	fixture_reader.flags = SC_READER_CARD_PRESENT;
	card->ctx = context;
	card->reader = &fixture_reader;
	old_card->card = card;
	old_card->reader = &fixture_reader;
	old_card->framework = &framework_pkcs15;
	fixture_slot.reader = &fixture_reader;
	fixture_slot.p11card = old_card;
	fixture_slot.slot_info.flags = CKF_TOKEN_PRESENT;
	fixture_slot.token_info.flags = CKF_LOGIN_REQUIRED;
	fixture_slot.login_user = CKU_USER;
	if (list_init(&virtual_slots) || list_init(&sessions) || list_init(&fixture_slot.objects) ||
			list_init(&fixture_slot.logins)) return 1;
	list_attributes_seeker(&virtual_slots, slot_seeker);
	list_attributes_seeker(&sessions, session_seeker);
	list_append(&virtual_slots, &fixture_slot);
	if (C_OpenSession(0, CKF_SERIAL_SESSION | CKF_RW_SESSION, NULL, NULL, &old_session) != CKR_OK)
		return 1;

	/* Non-null state enables the event wait that incorrectly hid CARD_CHANGED. */
	rv = card_detect(&fixture_reader, &scenario);
	if (expect_reconnect) {
		check(names[variant], rv == CKR_OK && disconnects == 1 && connects == 1 && binds == 1 &&
				presence_calls == 2 && fixture_slot.p11card &&
				(fixture_slot.slot_info.flags & CKF_TOKEN_PRESENT) && fixture_slot.login_user == -1);
		check("old session invalid; new session is public and usable",
				C_GetSessionInfo(old_session, &info) == CKR_SESSION_HANDLE_INVALID &&
				C_OpenSession(0, CKF_SERIAL_SESSION | CKF_RW_SESSION, NULL, NULL, &new_session) == CKR_OK &&
				C_GetSessionInfo(new_session, &info) == CKR_OK && info.state == CKS_RW_PUBLIC_SESSION);
		if (variant == 8) {
			int valid = 1;
			for (int replacement = 0; replacement < 4; replacement++) {
				changed = 1;
				rv = card_detect(&fixture_reader, &scenario);
				valid &= rv == CKR_OK && fixture_slot.p11card != NULL &&
					(fixture_slot.slot_info.flags & CKF_TOKEN_PRESENT) && fixture_slot.login_user == -1;
			}
			check("retry budget renews on every detection without reusing old sessions",
				valid && connects == 5 && disconnects == 5 && binds == 5 &&
				C_GetSessionInfo(new_session, &info) == CKR_SESSION_HANDLE_INVALID);
		}
	} else if (variant == 3 || variant == 5) {
		check(names[variant], rv == (variant == 3 ? CKR_OK : CKR_DEVICE_REMOVED) &&
				!connects && !disconnects && !binds && presence_calls == 1 &&
				fixture_slot.p11card == old_card && fixture_slot.login_user == CKU_USER);
		/* Presence-error case checks the preserved session list without a new reader query. */
		check("unchanged/error path preserves the existing session", list_seek(&sessions, &old_session) != NULL);
	} else {
		check(names[variant], rv == (variant == 6 ? CKR_DEVICE_REMOVED : CKR_TOKEN_NOT_PRESENT) &&
				disconnects == 1 && connects == (variant == 6 ? 1 : 0) && !binds &&
				presence_calls == (variant == 7 ? 4 : (variant == 6 ? 2 : 1)) &&
				!fixture_slot.p11card && !(fixture_slot.slot_info.flags & CKF_TOKEN_PRESENT) &&
				fixture_slot.login_user == -1);
		check("removed/failed reconnect rejects the previous session",
				C_GetSessionInfo(old_session, &info) == CKR_SESSION_HANDLE_INVALID && !list_size(&sessions));
	}
	card_removed(&fixture_reader);
	list_destroy(&fixture_slot.objects);
	list_destroy(&fixture_slot.logins);
	list_destroy(&virtual_slots);
	list_destroy(&sessions);
	return failures ? 1 : 0;
}

int main(void)
{
	/* Isolate simulated reader state and lists between scenarios. */
	for (int variant = 0; variant < 9; variant++) {
		int status;
		pid_t child = fork();
		if (child < 0) return 1;
		if (!child) {
			int result = run_scenario(variant);
			fflush(stdout);
			_exit(result);
		}
		if (waitpid(child, &status, 0) != child || !WIFEXITED(status) || WEXITSTATUS(status))
			failures++;
	}
	return failures ? 1 : 0;
}
