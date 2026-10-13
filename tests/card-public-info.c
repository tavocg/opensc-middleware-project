/* Simulate OpenSC discovery APIs: no reader, card, PIN or PC/SC calls. */
#define main card_public_info_main
#include "../tools/card-public-info.c"
#undef main
#include <assert.h>

static sc_context_t fixture_ctx;
static sc_reader_t fixture_reader;
static sc_card_t fixture_card;
static struct sc_ef_atr fixture_ef_atr;
static sc_app_info_t fixture_app;
static int atr_calls, dir_calls, lock_calls, unlock_calls, connect_calls, release_calls;
static int atr_result, dir_result, lock_result, driver_result, mismatch;
static char fixture_atr[] = "3b:d5:18:ff:81:91:fe:1f:c3:80:73:c8:21:10:0a";

int sc_context_create(sc_context_t **ctx, const sc_context_param_t *params)
{
	assert(params->debug == 0);
	*ctx = &fixture_ctx;
	return 0;
}
int sc_release_context(sc_context_t *ctx) { assert(ctx == &fixture_ctx); release_calls++; return 0; }
int sc_set_card_driver(sc_context_t *ctx, const char *name)
{
	assert(ctx == &fixture_ctx && !strcmp(name, "default") &&
			(ctx->flags & SC_CTX_FLAG_ENABLE_DEFAULT_DRIVER));
	return driver_result;
}
sc_reader_t *sc_ctx_get_reader(sc_context_t *ctx, unsigned int index)
{
	assert(ctx == &fixture_ctx && index == 2);
	return &fixture_reader;
}
int sc_connect_card(sc_reader_t *reader, sc_card_t **card)
{
	assert(reader == &fixture_reader);
	connect_calls++;
	*card = &fixture_card;
	fixture_card.atr.len = sizeof(fixture_card.atr.value);
	assert(sc_hex_to_bin(fixture_atr, fixture_card.atr.value, &fixture_card.atr.len) == 0);
	if (mismatch) fixture_card.atr.value[0] ^= 1;
	return 0;
}
int sc_disconnect_card(sc_card_t *card) { assert(card == &fixture_card); return 0; }
int sc_lock(sc_card_t *card) { assert(card == &fixture_card); lock_calls++; return lock_result; }
int sc_unlock(sc_card_t *card) { assert(card == &fixture_card); unlock_calls++; return 0; }
int sc_parse_ef_atr(sc_card_t *card)
{
	assert(card == &fixture_card && lock_calls == 1 && unlock_calls == 0);
	atr_calls++;
	card->ef_atr = &fixture_ef_atr;
	return atr_result;
}
int sc_enum_apps(sc_card_t *card)
{
	assert(card == &fixture_card && lock_calls == 1 && unlock_calls == 0);
	dir_calls++;
	card->app_count = 1;
	card->app[0] = &fixture_app;
	return dir_result;
}
static void reset_fixture(void)
{
	memset(&fixture_ctx, 0, sizeof(fixture_ctx));
	memset(&fixture_card, 0, sizeof(fixture_card));
	memset(&fixture_ef_atr, 0, sizeof(fixture_ef_atr));
	memset(&fixture_app, 0, sizeof(fixture_app));
	fixture_app.aid.value[0] = 0xA0;
	fixture_app.aid.len = 1;
	fixture_app.label = "fixture";
	atr_calls = dir_calls = lock_calls = unlock_calls = connect_calls = release_calls = 0;
	atr_result = dir_result = lock_result = driver_result = mismatch = 0;
}
static int run_fixture(void)
{
	char *args[] = { "card-public-info", "2", fixture_atr };
	return card_public_info_main(3, args);
}
int main(void)
{
	char *bad_args[] = { "card-public-info", "-1", fixture_atr };
	reset_fixture();
	assert(run_fixture() == 0 && atr_calls == 1 && dir_calls == 1 && unlock_calls == 1 && release_calls == 1);
	puts("PASS public discovery uses the default driver and queries both files under one lock");
	reset_fixture(); mismatch = 1;
	assert(run_fixture() == 1 && !atr_calls && !dir_calls && !lock_calls && release_calls == 1);
	puts("PASS public discovery checks the connected card ATR before file queries");
	reset_fixture();
	assert(card_public_info_main(3, bad_args) == 2 && !release_calls && !connect_calls);
	bad_args[1] = "4294967296";
	assert(card_public_info_main(3, bad_args) == 2 && !connect_calls);
	bad_args[1] = "2"; bad_args[2] = "invalid";
	assert(card_public_info_main(3, bad_args) == 2 && !connect_calls);
	puts("PASS public discovery validates reader bounds and ATR before opening a context");
	reset_fixture(); atr_result = SC_ERROR_FILE_NOT_FOUND;
	assert(run_fixture() == 0 && atr_calls == 1 && dir_calls == 1 && unlock_calls == 1);
	puts("PASS public discovery can inspect EF.DIR when EF.ATR is absent");
	reset_fixture(); atr_result = SC_ERROR_SECURITY_STATUS_NOT_SATISFIED;
	assert(run_fixture() == 1 && atr_calls == 1 && !dir_calls && unlock_calls == 1);
	puts("PASS public discovery stops on security rejection without authentication or retries");
	reset_fixture(); dir_result = SC_ERROR_SECURITY_STATUS_NOT_SATISFIED;
	assert(run_fixture() == 1 && atr_calls == 1 && dir_calls == 1 && unlock_calls == 1 && release_calls == 1);
	puts("PASS public discovery releases the lock on directory failure");
	reset_fixture(); lock_result = SC_ERROR_READER;
	assert(run_fixture() == 1 && !atr_calls && !dir_calls && !unlock_calls && release_calls == 1);
	puts("PASS public discovery sends no file queries if locking fails");
	reset_fixture(); driver_result = SC_ERROR_NOT_SUPPORTED;
	assert(run_fixture() == 1 && !connect_calls && !atr_calls && !dir_calls && release_calls == 1);
	puts("PASS public discovery stops if the default driver cannot be selected");
	return 0;
}
