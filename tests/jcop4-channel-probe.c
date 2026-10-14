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
	int context_fail, followups; unsigned int query_sw, query_outer_sw, mf_outer_sw;
} test_state;
static char output[8192];
static int test_lock_depth;

static int backend_reset(sc_reader_t *r, int cold) { assert(r == &test_reader && !cold); return 0; }
static int context_reply(sc_apdu_t *a)
{
	unsigned char auth[64] = {0}, tag[16], reply[64] = {0}, iv[32] = {0}, plain[32] = {0}, ssc[16] = {0};
	int query = probe_guard.phase == PROBE_PIN_QUERY, count, final;
	unsigned int sw = query ? test_state.query_sw : test_state.context_fail == 3 ? 0x6982 : 0x9000;
	size_t body_size = 0, auth_size; EVP_CIPHER_CTX *cipher = EVP_CIPHER_CTX_new(); assert(cipher);
	test_state.followups++;
	unsigned int outer = query ? test_state.query_outer_sw : test_state.mf_outer_sw;
	if (outer) { a->sw1 = outer >> 8; a->sw2 = outer & 255; }
	assert(test_state.followups == (query ? 2 : 1) && a->cla == 0x0C && !a->p1 && a->p2 == (query ? 1 : 0));
	assert(a->ins == (query ? 0x20 : 0xA4) && a->le == 256 && a->lc == (query ? 10 : 32));
	auth[15] = (unsigned char)(query ? 5 : 3); auth[16] = 0x0C; auth[17] = (unsigned char)a->ins;
	auth[19] = (unsigned char)a->p2; auth[20] = 0x80;
	if (query) {
		assert(!memcmp(a->data, "\x8e\x08", 2));
		cmac(test_flow.derived.keys.mac, auth, 32, tag); assert(!memcmp(tag, a->data + 2, 8));
	} else {
		assert(!memcmp(a->data, "\x87\x11\x01", 3) && !memcmp(a->data + 19, "\x97\x01\0\x8e\x08", 5));
		memcpy(auth + 32, a->data, 22); auth[54] = 0x80;
		cmac(test_flow.derived.keys.mac, auth, 64, tag); assert(!memcmp(tag, a->data + 24, 8));
		ssc[15] = 3;
		assert(EVP_EncryptInit_ex(cipher, EVP_aes_128_ecb(), NULL, test_flow.derived.keys.encryption, NULL) == 1);
		assert(EVP_CIPHER_CTX_set_padding(cipher, 0) == 1 && EVP_EncryptUpdate(cipher, iv, &count, ssc, 16) == 1);
		assert(EVP_EncryptFinal_ex(cipher, iv + count, &final) == 1 && count + final == 16);
		assert(EVP_DecryptInit_ex(cipher, EVP_aes_128_cbc(), NULL, test_flow.derived.keys.encryption, iv) == 1);
		assert(EVP_CIPHER_CTX_set_padding(cipher, 0) == 1 && EVP_DecryptUpdate(cipher, plain, &count, a->data + 3, 16) == 1);
		assert(EVP_DecryptFinal_ex(cipher, plain + count, &final) == 1 && count + final == 16);
		assert(!memcmp(plain, "\x3f\0\x80", 3)); for (int i = 3; i < 16; i++) assert(!plain[i]);
	}
	if (test_state.context_fail == (query ? 4 : 1)) { EVP_CIPHER_CTX_free(cipher); return -7; }
	if (query && (test_state.context_fail == 6 || test_state.context_fail == 12)) {
		/* Correctly authenticated unexpected query data must remain unlogged/rejected. */
		memset(plain, 0, sizeof(plain)); memcpy(plain, "PRIVATE_QUERY", 13);
		if (test_state.context_fail == 6) plain[13] = 0x80;
		ssc[15] = 6;
		assert(EVP_EncryptInit_ex(cipher, EVP_aes_128_ecb(), NULL, test_flow.derived.keys.encryption, NULL) == 1);
		assert(EVP_CIPHER_CTX_set_padding(cipher, 0) == 1 && EVP_EncryptUpdate(cipher, iv, &count, ssc, 16) == 1);
		assert(EVP_EncryptFinal_ex(cipher, iv + count, &final) == 1 && count + final == 16);
		memcpy(reply, "\x87\x11\x01", 3);
		assert(EVP_EncryptInit_ex(cipher, EVP_aes_128_cbc(), NULL, test_flow.derived.keys.encryption, iv) == 1);
		assert(EVP_CIPHER_CTX_set_padding(cipher, 0) == 1 && EVP_EncryptUpdate(cipher, reply + 3, &count, plain, 16) == 1);
		assert(EVP_EncryptFinal_ex(cipher, reply + 3 + count, &final) == 1 && count + final == 16); body_size = 19;
	}
	reply[body_size++] = 0x99; reply[body_size++] = 2;
	reply[body_size++] = (unsigned char)(sw >> 8); reply[body_size++] = (unsigned char)sw;
	memset(auth, 0, sizeof(auth)); auth[15] = (unsigned char)(query ? 6 : 4);
	memcpy(auth + 16, reply, body_size); auth[16 + body_size] = 0x80; auth_size = (16 + body_size + 16) / 16 * 16;
	cmac(test_flow.derived.keys.mac, auth, auth_size, tag);
	if (test_state.context_fail == (query ? 5 : 2)) tag[0] ^= 1;
	reply[body_size++] = 0x8E; reply[body_size++] = 8; memcpy(reply + body_size, tag, 8); body_size += 8;
	if (query) {
		if (test_state.context_fail == 11) { assert(body_size >= 9); reply[body_size - 9] = 7; }
		if (test_state.context_fail == 7 || test_state.context_fail == 8) {
			body_size = 0; a->sw1 = test_state.context_fail == 7 ? 0x69 : 0x90;
			a->sw2 = test_state.context_fail == 7 ? 0x82 : 0;
		}
		if (test_state.context_fail == 9) reply[0] = 0x85;
		if (test_state.context_fail == 10) reply[1] = 3;
	}
	assert(a->resplen >= body_size); memcpy(a->resp, reply, body_size); a->resplen = body_size;
	EVP_CIPHER_CTX_free(cipher); OPENSSL_cleanse(iv, sizeof(iv)); OPENSSL_cleanse(plain, sizeof(plain));
	OPENSSL_cleanse(tag, sizeof(tag)); return 0;
}
static int backend_transmit(sc_reader_t *r, sc_apdu_t *a)
{
	const unsigned long flags = SC_APDU_FLAGS_NO_RETRY_WL | SC_APDU_FLAGS_NO_GET_RESP | SC_APDU_FLAGS_NO_SM;
	assert(r == &test_reader && (a->flags & flags) == flags && test_state.locks == 1 && !test_state.unlocks);
	test_state.physical++; a->sw1 = 0x90; a->sw2 = 0;
#ifdef JCOP4_TRANSPORT_FIXTURE
	if (probe_guard.phase == PROBE_TRANSPORT_MF) return test_transport_reply(a);
#endif
#ifdef JCOP4_SIGN_FIXTURE
	if (probe_guard.phase == PROBE_SIGNATURE_APP || probe_guard.phase == PROBE_SIGNATURE_MSE || probe_guard.phase == PROBE_SIGNATURE_PSO || probe_guard.phase == PROBE_SIGNATURE_GET)
		return test_signature_reply(a);
#endif
#ifdef JCOP4_LOGIN_FIXTURE
	if (probe_guard.phase == PROBE_LOGIN_VERIFY) return test_login_reply(a);
#endif
	if (probe_guard.phase == PROBE_CONTEXT_MF || probe_guard.phase == PROBE_PIN_QUERY) return context_reply(a);
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
	test_state.query_sw = 0x63C7;
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
static int invoke_mode(int context_mode)
{
	char *args[] = {"jcop4-channel-probe", "0", "--pin-context"}; int result, saved = dup(STDOUT_FILENO); FILE *capture = tmpfile();
	assert(saved >= 0 && capture); fflush(stdout); assert(dup2(fileno(capture), STDOUT_FILENO) >= 0);
	result = channel_probe_main(context_mode ? 3 : 2, args); fflush(stdout); rewind(capture);
	size_t len = fread(output, 1, sizeof(output)-1, capture); output[len] = 0;
	assert(!strstr(output, "PRIVATE_IDENTIFIER") && !strstr(output, "PRIVATE_QUERY") && !strstr(output, "Sending:") && !strstr(output, "Received"));
	assert(dup2(saved, STDOUT_FILENO) >= 0); close(saved); fclose(capture); return result;
}
static int invoke(void) { return invoke_mode(0); }
#if defined(JCOP4_LOGIN_FIXTURE) || defined(JCOP4_TRANSPORT_FIXTURE)
int no_pin_fixture_main(void)
#else
int main(void)
#endif
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
	defaults(); test_state.query_outer_sw = 0x63C7;
	assert(invoke_mode(1) == 0 && test_state.physical == 10 && test_state.followups == 2 && test_state.resets == 2);
	assert(strstr(output, "pin_query_outer_sw=63C7") && strstr(output, "pin_query_mac_check=verified"));
	assert(strstr(output, "pin_query_sw=63C7") && strstr(output, "pin_context_result=status_observed_no_pin_no_login"));
	puts("PASS context mode also uses real OpenSC bootstrap/transmit and software MF/PIN status peer without reader access");
#else
	defaults();
	char *login_args[] = {"jcop4-channel-probe", "0", "--login-once"};
	assert(channel_probe_main(3, login_args) == 2 && !test_state.connects);
	defaults(); assert(invoke() == 0 && test_flow.calls == 3 && test_state.physical == 9 && test_state.resets == 2);
	assert(test_state.unlocks == 1 && test_state.disconnects == 1 && test_state.releases == 1);
	assert(strstr(output, "channel_result=protected_d004_read_confirmed_no_login") && strstr(output, "final_reset_result=0"));
	assert(strstr(output, "channel_content_size=137") && strstr(output, "channel_content_first_block_equal=1") &&
		strstr(output, "channel_content_tail_equal=1"));
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
	assert(!strstr(output, "channel_content_"));
	defaults(); test_flow.mode = INNER_ERROR;
	assert(invoke() == 1 && strstr(output, "channel_sw=6982") && !strstr(output, "channel_content_"));
	defaults(); test_flow.mode = ZERO_DATA_IV;
	assert(invoke() == 1 && strstr(output, "channel_content_size_equal=1") &&
		strstr(output, "channel_content_first_block_equal=0") && strstr(output, "channel_content_tail_equal=1"));
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
	defaults(); assert(invoke_mode(1) == 0 && test_state.physical == 11 && test_state.followups == 2 && test_state.resets == 2);
	assert(strstr(output, "context_select_result=mf_selected_under_sm_no_login") && strstr(output, "pin_query_sw=63C7"));
	assert(strstr(output, "pin_context_result=status_observed_no_pin_no_login") && test_state.unlocks == 1 && test_state.disconnects == 1);
	puts("PASS opt-in context mode confirms D004 first, encrypts MF SELECT at SSC3/4, queries empty VERIFY1 at SSC5/6 and closes");
	for (int fail = 1; fail <= 6; fail++) {
		defaults(); test_state.context_fail = fail;
		assert(invoke_mode(1) == 1 && test_state.followups == (fail <= 3 ? 1 : 2) && test_state.resets == 2);
		assert(test_state.unlocks == 1 && test_state.disconnects == 1 && !strstr(output, "pin_context_result=status_observed"));
		if (fail <= 3) assert(!strstr(output, "pin_query_"));
	}
	for (int which = 0; which < 4; which++) {
		defaults(); test_state.query_sw = which == 0 ? 0x63C0 : which == 1 ? 0x6983 : which == 2 ? 0x6982 : 0x6100;
		assert(invoke_mode(1) == 1 && test_state.followups == 2 && test_state.resets == 2);
	}
	defaults(); test_state.query_sw = 0x9000; assert(invoke_mode(1) == 0 && strstr(output, "pin_query_result=status_9000_no_pin_supplied"));
	defaults(); test_state.query_sw = 0x63CF; assert(invoke_mode(1) == 0 && strstr(output, "pin_query_result=available_unverified"));
	puts("PASS context transport/MAC/SW/unexpected data failures stop; counter zero/block/unknown SW fail and no maximum or reset is inferred");
	{
		const char *reasons[] = {"outer_sw_not_9000", "response_size_invalid", "status_object_invalid_or_unsupported",
			"status_object_invalid_or_unsupported", "mac_object_invalid_or_trailing_data", "padding_invalid"};
		for (int fail = 7; fail <= 12; fail++) {
			char expected[128]; defaults(); test_state.context_fail = fail;
			assert(invoke_mode(1) == 1 && test_state.followups == 2 && test_state.resets == 2);
			assert(test_state.unlocks == 1 && test_state.disconnects == 1 && !strstr(output, "pin_query_sw="));
			snprintf(expected, sizeof(expected), "pin_query_decode_result=%s", reasons[fail - 7]);
			assert(strstr(output, expected) && strstr(output, "pin_query_response=invalid_stop"));
			assert(strstr(output, fail == 12 ? "pin_query_mac_check=verified" : "pin_query_mac_check=not_checked"));
			assert(!strstr(output, "PRIVATE_QUERY") && !strstr(output, "pin_context_result=status_observed"));
			if (fail == 7) assert(strstr(output, "pin_query_outer_sw=6982") && strstr(output, "pin_query_response_size=2"));
			if (fail == 9) assert(strstr(output, "pin_query_leading_object=unsupported85"));
		}
		defaults(); test_state.context_fail = 5; assert(invoke_mode(1) == 1);
		assert(strstr(output, "pin_query_decode_result=mac_verification_failed") && strstr(output, "pin_query_mac_check=failed"));
		assert(!strstr(output, "pin_query_sw="));
		defaults(); assert(invoke_mode(1) == 0);
		assert(strstr(output, "pin_query_decode_result=accepted") && strstr(output, "pin_query_mac_check=verified"));
		puts("PASS bounded envelope/reason metadata distinguishes rejected query responses without inner SW/data, retries or extra APDUs");
	}
	{
		defaults(); test_state.query_outer_sw = 0x63C7;
		assert(invoke_mode(1) == 0 && test_state.followups == 2 && test_state.resets == 2 && test_state.physical == 11);
		assert(strstr(output, "pin_query_outer_sw=63C7") && strstr(output, "pin_query_sw=63C7") &&
			strstr(output, "pin_query_response=authenticated") && strstr(output, "pin_query_mac_check=verified"));
		for (int which = 0; which < 5; which++) {
			defaults(); test_state.query_outer_sw = which == 0 ? 0x63C0 : which == 1 ? 0x63C6 : 0x63C7;
			if (which == 0) test_state.query_sw = 0x63C0;
			if (which >= 2) test_state.context_fail = which == 2 ? 5 : which == 3 ? 11 : 6;
			assert(invoke_mode(1) == 1 && test_state.followups == 2 && test_state.resets == 2);
			assert(test_state.unlocks == 1 && test_state.disconnects == 1 && !strstr(output, "pin_context_result=status_observed"));
			if (which == 1) assert(strstr(output, "pin_query_decode_result=authenticated_status_outer_mismatch"));
			if (which >= 1 && which <= 3) assert(!strstr(output, "pin_query_sw="));
		}
		defaults(); test_state.mf_outer_sw = 0x63C7;
		assert(invoke_mode(1) == 1 && test_state.followups == 1 && !strstr(output, "pin_query_"));
		assert(strstr(output, "context_select_decode_result=outer_sw_not_9000"));
		puts("PASS outer63Cx policy is query-only with MAC/status equality; zero counter, mismatches, malformed MAC, data and MF warnings stop and close");
	}
	defaults(); test_flow.mode = BAD_MAC; assert(invoke_mode(1) == 1 && !test_state.followups && test_state.resets == 2);
	defaults(); test_flow.mode = WRONG_CONTENT; assert(invoke_mode(1) == 1 && !test_state.followups);
	defaults(); assert(invoke() == 0 && !test_state.followups && !strstr(output, "pin_query_"));
	puts("PASS no context/PIN query before full D004 confirmation; legacy mode still blocks the entire follow-up sequence");
	{
		for (int mutation = 0; mutation < 8; mutation++) {
			sc_apdu_t a; unsigned char reply[256], wire[16] = {0x0C,0x20,0,1,10,0x8E,8};
			defaults(); test_state.locks = 1; memset(&probe_guard, 0, sizeof(probe_guard));
			probe_guard.reader = &test_reader; probe_guard.original = &test_ops; probe_guard.context_mode = 1;
			probe_guard.context_selected = 1; probe_guard.phase = PROBE_PIN_QUERY;
			memcpy(probe_guard.expected, wire, sizeof(wire)); probe_guard.expected_size = sizeof(wire);
			sc_format_apdu(&test_card, &a, SC_APDU_CASE_4_SHORT, 0x20, 0, 1); a.cla = 0x0C;
			a.data = wire + 5; a.lc = a.datalen = 10; a.le = a.resplen = 256; a.resp = reply;
			if (mutation == 0) a.p2 = 3;
			if (mutation == 1) a.ins = 0x24;
			if (mutation == 2) a.cla = 0;
			if (mutation == 3) wire[5] = 0x87;
			if (mutation == 4) wire[7] ^= 1;
			if (mutation == 5) probe_guard.context_selected = 0;
			if (mutation == 6) probe_guard.context_mode = 0;
			if (mutation == 7) a.le = 172;
			assert(probe_guard_transmit(&test_reader, &a) < 0 && !test_state.physical && probe_guard.blocked);
		}
	}
	puts("PASS context guard refuses PIN data/DO87, changed MAC/header/ref/Le, missing authenticated MF or missing opt-in before backend");
	{
		for (int mutation = 0; mutation < 7; mutation++) {
			sc_apdu_t a; unsigned char reply[256], wire[38] = {0x0C,0xA4,0,0,32,0x87,0x11,1};
			memcpy(wire + 24, "\x97\x01\0\x8e\x08", 5);
			defaults(); test_state.locks = 1; memset(&probe_guard, 0, sizeof(probe_guard));
			probe_guard.reader = &test_reader; probe_guard.original = &test_ops; probe_guard.context_mode = 1;
			probe_guard.phase = PROBE_CONTEXT_MF;
			memcpy(probe_guard.expected, wire, sizeof(wire)); probe_guard.expected_size = sizeof(wire);
			sc_format_apdu(&test_card, &a, SC_APDU_CASE_4_SHORT, 0xA4, 0, 0); a.cla = 0x0C;
			a.data = wire + 5; a.lc = a.datalen = 32; a.le = a.resplen = 256; a.resp = reply;
			if (mutation == 0) a.p2 = 4;
			if (mutation == 1) a.p1 = 4;
			if (mutation == 2) wire[8] ^= 1;
			if (mutation == 3) wire[24] = 0x87;
			if (mutation == 4) a.cla = 0;
			if (mutation == 5) probe_guard.expected_size = 0;
			if (mutation == 6) probe_guard.context_mode = 0;
			assert(probe_guard_transmit(&test_reader, &a) < 0 && !test_state.physical && probe_guard.blocked);
		}
	}
	puts("PASS MF guard binds exact encrypted frame and excludes alternative P1/P2/ciphertext/tags/plaintext or unarmed/non-opt-in transport");
#endif
	EVP_PKEY_free(test_flow.peer); jcop4_ca_pending_clear(&test_flow.derived); return 0;
}
