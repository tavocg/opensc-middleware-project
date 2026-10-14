/* Replace OpenSC I/O with a stateful software peer. Never uses hardware. */
#define main channel_probe_main
#include "../tools/jcop4-channel-probe.c"
#undef main
/* Reuse published peer and independent response construction, not its main. */
#define main confirmation_fixture_main
#include "jcop4-channel-confirmation.c"
#undef main
#include <unistd.h>

static sc_context_t test_ctx;
static sc_reader_t test_reader;
static sc_card_t test_card;
static sc_app_info_t test_app;
static struct fixture test_flow;
static unsigned char test_initial[137];
static struct {
	int missing_reader, bad_atr, bad_protocol, short_send, short_receive, missing_app;
	int connect_error, lock_error, reset_error, select_error, bad_control, bad_initial, bootstrap_attack, bad_app_count;
	int connects, locks, unlocks, resets, disconnects, releases, physical;
} test_state;
static char output[8192];
static int test_lock_depth;

static int backend_reset(sc_reader_t *r, int cold) { assert(r == &test_reader && !cold); return 0; }
static int backend_transmit(sc_reader_t *r, sc_apdu_t *a)
{
	const unsigned long flags = SC_APDU_FLAGS_NO_RETRY_WL | SC_APDU_FLAGS_NO_GET_RESP | SC_APDU_FLAGS_NO_SM;
	assert(r == &test_reader && (a->flags & flags) == flags && test_state.locks == 1 && !test_state.unlocks);
	test_state.physical++; a->sw1 = 0x90; a->sw2 = 0;
	if (probe_guard.phase >= PROBE_MSE) {
		unsigned char wire[264], reply[258]; size_t size = 5 + a->lc, received = 0;
		wire[0] = (unsigned char)a->cla; wire[1] = (unsigned char)a->ins;
		wire[2] = (unsigned char)a->p1; wire[3] = (unsigned char)a->p2; wire[4] = (unsigned char)a->lc;
		memcpy(wire + 5, a->data, a->lc);
		if (a->cse == SC_APDU_CASE_4_SHORT) wire[size++] = (unsigned char)a->le;
		int rv = exchange(&test_flow, wire, size, reply, sizeof(reply), &received);
		if (rv) return rv;
		if (received > sizeof(reply)) { a->resplen = received; return 0; }
		assert(received >= 2 && received - 2 <= a->resplen);
		memcpy(a->resp, reply, received - 2); a->resplen = received - 2;
		a->sw1 = reply[received - 2]; a->sw2 = reply[received - 1];
	} else if (probe_guard.phase == PROBE_D004) {
		static const unsigned char control[] = {0x62,11,0x82,1,1,0x83,2,0xD0,4,0x80,2,0,137};
		assert(a->resplen >= sizeof(control)); memcpy(a->resp, control, sizeof(control)); a->resplen = sizeof(control);
		if (test_state.bad_control) a->resp[8] = 5;
	} else if (probe_guard.phase == PROBE_INITIAL) {
		assert(a->resplen == 137); memcpy(a->resp, test_flow.initial, 137); a->resplen = test_state.bad_initial ? 136 : 137;
	} else {
#ifdef TEST_REAL_BOOTSTRAP
		if (probe_guard.phase == PROBE_DIR) {
			/* Only the reader backend is synthetic; OpenSC builds/parses
			 * the absolute EF.DIR SELECT, file control and directory. */
			if (a->ins == 0xA4) {
				static const unsigned char fci[] = {0x62,11,0x82,1,1,0x83,2,0x2F,0,0x80,2,0,19};
				assert(a->p1 == 8 && a->lc == 2 && !memcmp(a->data, "\x2f\0", 2));
				assert(a->resplen >= sizeof(fci)); memcpy(a->resp, fci, sizeof(fci)); a->resplen = sizeof(fci);
			} else {
				assert(a->ins == 0xB0 && !a->p1 && !a->p2 && a->le == 19 && a->resplen >= 19);
				a->resp[0] = 0x61; a->resp[1] = 17; a->resp[2] = 0x4F; a->resp[3] = sizeof(probe_aid);
				memcpy(a->resp + 4, probe_aid, sizeof(probe_aid)); a->resplen = 19;
			}
			return 0;
		}
#endif
		a->resplen = 0;
		if (probe_guard.phase == PROBE_APP && test_state.select_error) { a->sw1 = 0x69; a->sw2 = 0x82; }
	}
	return 0;
}
static const struct sc_reader_operations test_ops = {.transmit = backend_transmit, .reset = backend_reset};
int sc_context_create(sc_context_t **ctx, const sc_context_param_t *params)
{
	assert(params->debug == 0 && !strcmp(params->app_name, "jcop4-channel-probe")); *ctx = &test_ctx; return 0;
}
int sc_release_context(sc_context_t *ctx)
{
	assert(ctx == &test_ctx && test_reader.ops == &test_ops); test_state.releases++; return 0;
}
int sc_set_card_driver(sc_context_t *ctx, const char *name)
{
	assert(ctx->debug == 0 && (ctx->flags & SC_CTX_FLAG_ENABLE_DEFAULT_DRIVER) && !strcmp(name, "default")); return 0;
}
sc_reader_t *sc_ctx_get_reader(sc_context_t *ctx, unsigned int index)
{
	assert(ctx == &test_ctx); return test_state.missing_reader || index != 0 ? NULL : &test_reader;
}
int sc_connect_card(sc_reader_t *reader, sc_card_t **card)
{
	assert(reader == &test_reader); test_state.connects++;
	if (test_state.connect_error) return SC_ERROR_CARD_NOT_PRESENT;
	*card = &test_card; return 0;
}
int sc_lock(sc_card_t *card)
{
	assert(card == &test_card);
	if (!test_lock_depth) test_state.locks++;
	if (test_state.lock_error) return -7;
	test_lock_depth++; return 0;
}
int sc_unlock(sc_card_t *card)
{
	assert(card == &test_card && test_lock_depth > 0);
	if (!--test_lock_depth) test_state.unlocks++;
	return 0;
}
int sc_disconnect_card(sc_card_t *card)
{
	assert(card == &test_card); test_state.disconnects++;
#ifdef TEST_REAL_BOOTSTRAP
	sc_free_apps(card);
#endif
	return 0;
}
int sc_reset(sc_card_t *card, int cold)
{
	assert(card == &test_card && !cold && test_state.locks == 1 && !test_state.unlocks);
	test_state.resets++; return test_state.reset_error == test_state.resets ? -7 : card->reader->ops->reset(card->reader, cold);
}
#ifndef TEST_REAL_BOOTSTRAP
void sc_format_apdu(sc_card_t *card, sc_apdu_t *a, int cse, int ins, int p1, int p2)
{
	assert(card == &test_card); memset(a, 0, sizeof(*a)); a->cse = cse; a->ins = ins; a->p1 = p1; a->p2 = p2;
}
int sc_transmit_apdu(sc_card_t *card, sc_apdu_t *a)
{
	assert(card == &test_card); return card->reader->ops->transmit(card->reader, a);
}
#endif
#ifndef TEST_REAL_BOOTSTRAP
int sc_select_file(sc_card_t *card, const sc_path_t *path, sc_file_t **file)
{
	sc_apdu_t a; assert(!file && path->type == SC_PATH_TYPE_DF_NAME && path->len == sizeof(probe_aid));
	sc_format_apdu(card, &a, SC_APDU_CASE_3_SHORT, 0xA4, 4, 0x0C); a.data = path->value; a.lc = a.datalen = path->len;
	int rv = sc_transmit_apdu(card, &a); return rv < 0 ? rv : a.sw1 == 0x90 && !a.sw2 ? 0 : SC_ERROR_SECURITY_STATUS_NOT_SATISFIED;
}
int sc_enum_apps(sc_card_t *card)
{
	sc_apdu_t a; unsigned char response[256]; int rv;
	sc_format_apdu(card, &a, SC_APDU_CASE_4_SHORT, 0xA4, 0, 0);
	a.data = (const unsigned char *)"\x3f\0"; a.lc = a.datalen = 2; a.le = a.resplen = 256; a.resp = response;
	rv = sc_transmit_apdu(card, &a); if (rv < 0) return rv;
	sc_format_apdu(card, &a, SC_APDU_CASE_4_SHORT, 0xA4, 2, 0);
	a.data = (const unsigned char *)"\x2f\0"; a.lc = a.datalen = 2; a.le = a.resplen = 256; a.resp = response;
	rv = sc_transmit_apdu(card, &a); if (rv < 0) return rv;
	sc_format_apdu(card, &a, SC_APDU_CASE_2_SHORT, test_state.bootstrap_attack ? 0x20 : 0xB0, 0, 0);
	a.le = a.resplen = 128; a.resp = response;
	rv = sc_transmit_apdu(card, &a); if (rv < 0) return rv;
	card->app_count = test_state.bad_app_count ? SC_MAX_CARD_APPS + 1 : test_state.missing_app ? 0 : 1;
	card->app[0] = &test_app; return 0;
}
#endif
static void defaults(void)
{
	memset(&test_state, 0, sizeof(test_state)); memset(&test_ctx, 0, sizeof(test_ctx));
	test_lock_depth = 0;
	memset(&test_reader, 0, sizeof(test_reader)); memset(&test_card, 0, sizeof(test_card)); memset(&test_app, 0, sizeof(test_app));
	test_reader.ops = &test_ops; test_reader.ctx = &test_ctx; test_reader.name = "PRIVATE_IDENTIFIER_DO_NOT_LOG";
	test_reader.active_protocol = SC_PROTO_T1; test_reader.max_send_size = 255; test_reader.max_recv_size = 256;
	test_reader.atr.len = sizeof(probe_atr); memcpy(test_reader.atr.value, probe_atr, sizeof(probe_atr));
	test_card.ctx = &test_ctx; test_card.reader = &test_reader; test_card.atr = test_reader.atr;
	test_card.max_send_size = 255; test_card.max_recv_size = 256;
#ifdef TEST_REAL_BOOTSTRAP
	test_card.ops = sc_get_iso7816_driver()->ops;
#endif
	test_app.aid.len = sizeof(probe_aid); memcpy(test_app.aid.value, probe_aid, sizeof(probe_aid)); test_app.label = "PRIVATE_IDENTIFIER_DO_NOT_LOG";
	test_flow.mode = NORMAL; test_flow.fail_step = test_flow.calls = test_flow.ga_template = 0; test_flow.le = JCOP4_CA_LE_SHORT_MAX;
	memcpy(test_flow.initial, test_initial, 137); jcop4_ca_pending_clear(&test_flow.derived);
}
static int invoke(void)
{
	char *args[] = {"jcop4-channel-probe", "0"}; int result, saved = dup(STDOUT_FILENO); FILE *capture = tmpfile();
	assert(saved >= 0 && capture); fflush(stdout); assert(dup2(fileno(capture), STDOUT_FILENO) >= 0);
	result = channel_probe_main(2, args); fflush(stdout); rewind(capture);
	size_t len = fread(output, 1, sizeof(output)-1, capture); output[len] = 0;
	assert(!strstr(output, "PRIVATE_IDENTIFIER") && !strstr(output, "Sending:") && !strstr(output, "Received"));
	assert(dup2(saved, STDOUT_FILENO) >= 0); close(saved); fclose(capture); return result;
}
int main(void)
{
	init(&test_flow); memcpy(test_initial, test_flow.initial, 137); setenv("OPENSC_CONF", "/tmp/synthetic-config", 1); setenv("OPENSC_DEBUG", "0", 1);
#ifdef TEST_REAL_BOOTSTRAP
	defaults(); int result = invoke();
	if (result) fputs(output, stderr); /* Synthetic metadata only. */
	assert(result == 0 && test_flow.calls == 3 && test_state.physical == 8 && test_state.resets == 2);
	assert(test_state.unlocks == 1 && test_state.disconnects == 1 && test_state.releases == 1 && !test_lock_depth);
	assert(strstr(output, "directory_result=0") && strstr(output, "application_announced=1"));
	puts("PASS real OpenSC enum/select/read/FCI/ASN.1 and APDU formatting reach channel confirmation with a synthetic reader only");
	for (int mutation = 0; mutation < 5; mutation++) {
		sc_apdu_t a; unsigned char response[256]; defaults(); test_state.locks = 1;
		memset(&probe_guard, 0, sizeof(probe_guard)); probe_guard.reader = &test_reader;
		probe_guard.original = &test_ops; probe_guard.phase = PROBE_DIR;
		sc_format_apdu(&test_card, &a, SC_APDU_CASE_4_SHORT, 0xA4, 8, 0);
		a.data = (const unsigned char *)"\x2f\0"; a.lc = a.datalen = 2; a.le = a.resplen = 256; a.resp = response;
		if (mutation == 0) a.data = (const unsigned char *)"\x41\x41";
		if (mutation == 1) a.p1 = 9;
		if (mutation == 2) a.cla = 1;
		if (mutation == 3) a.lc = a.datalen = 1;
		if (mutation == 4) a.p2 = 4;
		assert(probe_guard_transmit(&test_reader, &a) < 0 && !test_state.physical && probe_guard.blocked);
	}
	puts("PASS absolute EF.DIR allowance does not allow other files, relative paths, channels, lengths or P2");
#else
	defaults(); assert(invoke() == 0 && test_flow.calls == 3 && test_state.physical == 9 && test_state.resets == 2);
	assert(test_state.unlocks == 1 && test_state.disconnects == 1 && test_state.releases == 1);
	assert(strstr(output, "channel_result=protected_d004_read_confirmed_no_login") && strstr(output, "final_reset_result=0"));
	puts("PASS native adapter verifies the first protected read and resets/unlocks/disconnects without identifiers or raw APDU logs");
	for (int scenario = 0; scenario < 7; scenario++) {
		defaults();
		if (scenario == 0) test_state.missing_reader = 1;
		if (scenario == 1) test_state.connect_error = 1;
		if (scenario == 2) test_card.atr.value[0] ^= 1;
		if (scenario == 3) test_reader.atr.value[0] ^= 1;
		if (scenario == 4) test_reader.active_protocol = SC_PROTO_T0;
		if (scenario == 5) test_reader.max_send_size = 138;
		if (scenario == 6) test_card.max_recv_size = 255;
		assert(invoke() == 1 && !test_flow.calls && !test_state.physical && !test_state.resets && test_state.releases == 1);
	}
	puts("PASS missing reader/card, mismatched ATR, T0 and insufficient declared limits prevent all card APDU and channel operations");
	for (int scenario = 0; scenario < 8; scenario++) {
		defaults();
		if (scenario == 0) test_state.lock_error = 1;
		if (scenario == 1) test_state.reset_error = 1;
		if (scenario == 2) test_state.missing_app = 1;
		if (scenario == 3) test_state.select_error = 1;
		if (scenario == 4) test_state.bad_control = 1;
		if (scenario == 5) test_state.bad_initial = 1;
		if (scenario == 6) test_state.bootstrap_attack = 1;
		if (scenario == 7) test_state.bad_app_count = 1;
		assert(invoke() == 1 && !test_flow.calls && test_state.disconnects == 1 && test_state.releases == 1);
		assert(test_state.resets == (scenario == 0 ? 0 : scenario == 1 ? 1 : 2));
	}
	puts("PASS lock/reset/application/control/initial-read failures and bootstrap VERIFY attempt stop before MSE and clean up");
	for (int step = 1; step <= 3; step++) {
		defaults(); test_flow.mode = TRANSPORT_ERROR; test_flow.fail_step = step;
		assert(invoke() == 1 && test_flow.calls == step && test_state.resets == 2 && test_state.unlocks == 1 && test_state.disconnects == 1);
	}
	defaults(); test_flow.mode = BAD_MAC; test_flow.bit = 10;
	assert(invoke() == 1 && test_flow.calls == 3 && strstr(output, "channel_authenticated_response=0"));
	defaults(); test_state.reset_error = 2;
	assert(invoke() == 1 && test_flow.calls == 3 && strstr(output, "final_reset_result=-7") && test_state.disconnects == 1);
	puts("PASS setup/read/MAC failures and final-reset failure never report a successful process or skip cleanup");
	defaults(); setenv("LIBPCSCLITE_DELEGATE", "/tmp/synthetic-spy", 1);
	assert(invoke() == 2 && !test_state.connects && !test_state.physical); unsetenv("LIBPCSCLITE_DELEGATE");
	setenv("OPENSC_DEBUG", "9", 1); assert(invoke() == 2 && !test_state.connects); setenv("OPENSC_DEBUG", "0", 1);
	puts("PASS direct helper refuses inherited tracing/delegates or nonzero debug before connection");
	{
		/* Exercise the physical guard independently of the high-level sequence. */
		for (int mutation = 0; mutation < 11; mutation++) {
			sc_apdu_t a; unsigned char response[137]; defaults(); test_state.locks = 1;
			memset(&probe_guard, 0, sizeof(probe_guard)); probe_guard.reader = &test_reader;
			probe_guard.original = &test_ops; probe_guard.phase = PROBE_INITIAL;
			sc_format_apdu(&test_card, &a, SC_APDU_CASE_2_SHORT, 0xB0, 0, 0);
			a.le = a.resplen = 137; a.resp = response;
			if (mutation == 0) a.ins = 0x20;
			if (mutation == 1) a.ins = 0x24;
			if (mutation == 2) a.ins = 0xD6;
			if (mutation == 3) a.cla = 0x80;
			if (mutation == 4) a.p1 = 0x80;
			if (mutation == 5) a.p2 = 1;
			if (mutation == 6) a.le = 256;
			if (mutation == 7) a.control = 1;
			if (mutation == 8) a.flags = SC_APDU_FLAGS_CHAINING;
			if (mutation == 9) a.cse = SC_APDU_CASE_2_EXT;
			if (mutation == 10) probe_guard.commands = 24;
			assert(probe_guard_transmit(&test_reader, &a) < 0 && !test_state.physical && probe_guard.blocked);
			a.ins = 0xB0; assert(probe_guard_transmit(&test_reader, &a) < 0 && !test_state.physical);
		}
		defaults(); test_state.locks = 1; memset(&probe_guard, 0, sizeof(probe_guard));
		probe_guard.reader = &test_reader; probe_guard.original = &test_ops; probe_guard.phase = PROBE_INITIAL;
		sc_apdu_t a; unsigned char response[137]; sc_format_apdu(&test_card, &a, SC_APDU_CASE_2_SHORT, 0xB0, 0, 0);
		a.le = a.resplen = 137; a.resp = response;
		assert(probe_guard_transmit(&test_reader, &a) == 0 && test_state.physical == 1);
		assert(probe_guard_transmit(&test_reader, &a) < 0 && test_state.physical == 1);
	}
	puts("PASS guard rejects PIN/change/write/control/SFI/offset/length/chaining/extended/budget/repeated commands before physical transport");
#endif
	EVP_PKEY_free(test_flow.peer); jcop4_ca_pending_clear(&test_flow.derived); return 0;
}
