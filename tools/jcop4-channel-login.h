/* Separate experimental login diagnostic. Included only by its own binary.
 * Public PIN metadata: global1, UTF8/no padding, bounds6..14. Conservatively
 * accept printable ASCII (a UTF8 subset), never convert/hash/pad credentials.
 * SM method2 padding is encryption framing, not credential formatting. */
#ifndef JCOP4_CHANNEL_LOGIN_H
#define JCOP4_CHANNEL_LOGIN_H
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

#ifndef JCOP4_LOGIN_READ_PIN
/* Uses the controlling terminal, independent of the private stdout report.
 * Do not replace with argv/env/stdin pipelines or log input/length/hash. */
static int jcop4_login_read_pin(unsigned char pin[14], size_t *size)
{
	struct termios original, hidden; unsigned char byte = 0;
	int fd = -1, changed = 0, result = 1, overflow = 0; size_t count = 0;
	*size = 0; memset(pin, 0, 14);
	fd = open("/dev/tty", O_RDWR | O_NOCTTY | O_CLOEXEC);
	if (fd < 0 || !isatty(fd) || tcgetattr(fd, &original)) goto out;
	hidden = original; hidden.c_lflag &= ~(ECHO | ECHONL);
	if (tcsetattr(fd, TCSAFLUSH, &hidden)) goto out;
	changed = 1;
	const char prompt[] = "PIN de la JCOP4 (sin eco; un único intento, sin firma): ";
	if (write(fd, prompt, sizeof(prompt) - 1) != (ssize_t)(sizeof(prompt) - 1)) goto out;
	while (!probe_interrupted) {
		ssize_t got = read(fd, &byte, 1);
		if (got != 1) goto out;
		if (byte == '\n') {
			if (!overflow && count >= 6) { *size = count; result = 0; }
			break;
		}
		if (byte < 0x20 || byte > 0x7E || count == 14) overflow = 1;
		if (!overflow) pin[count++] = byte;
	}
out:
	if (changed) {
		if (tcsetattr(fd, TCSAFLUSH, &original)) result = 1;
		if (write(fd, "\n", 1) != 1) result = 1;
	}
	if (fd >= 0) close(fd);
	OPENSSL_cleanse(&byte, sizeof(byte));
	if (result) { OPENSSL_cleanse(pin, 14); *size = 0; }
	return result;
}
#define JCOP4_LOGIN_READ_PIN jcop4_login_read_pin
#endif

static int probe_login_once(sc_card_t *card, struct jcop4_sm_channel *channel)
{
	const unsigned char verify[4] = {0, 0x20, 0, 1};
	unsigned char pin[14] = {0}, header[4] = {0}, body[29] = {0}, wire[35] = {0};
	unsigned char response[258] = {0}, plain[256] = {0}, expected_ssc[16] = {0};
	size_t pin_size = 0, body_size = 0, received = 0, plain_size = 0;
	unsigned int sw = 0; int rv, result = 1, attempted = 0;
	struct jcop4_sm_decode_report decoded;
	/* Refuse below the last observed availability7: this is a diagnostic
	 * precondition, not a claim that seven is the configured maximum. */
	expected_ssc[15] = 6;
	if (!probe_guard.login_mode || !probe_guard.context_selected || probe_guard.context_query_sw != 0x63C7 ||
		probe_guard.phase != PROBE_PIN_QUERY || !probe_guard.called || probe_guard.blocked || probe_guard.commands >= 24 || probe_interrupted ||
		!probe_transport_ready(card) || channel->phase != JCOP4_SM_IDLE || CRYPTO_memcmp(channel->keys.ssc, expected_ssc, 16)) {
		printf("login_precondition=stop_no_pin\n"); goto out;
	}
	fflush(stdout);
	if (JCOP4_LOGIN_READ_PIN(pin, &pin_size) || pin_size < 6 || pin_size > 14 || probe_interrupted) {
		printf("login_input=rejected_no_verify\n"); goto out;
	}
	for (size_t i = 0; i < pin_size; i++) if (pin[i] < 0x20 || pin[i] > 0x7E) {
		printf("login_input=rejected_no_verify\n"); goto out;
	}
	if (jcop4_sm_seal(channel, verify, pin, pin_size, 0, 0, header, body, sizeof(body), &body_size) || body_size != 29) goto out;
	OPENSSL_cleanse(pin, sizeof(pin)); /* Clear credential before transport. */
	memcpy(wire, header, 4); wire[4] = 29; memcpy(wire + 5, body, 29); wire[34] = 0;
	probe_phase_set(PROBE_LOGIN_VERIFY); memcpy(probe_guard.expected, wire, sizeof(wire)); probe_guard.expected_size = sizeof(wire);
	unsigned int before = probe_guard.commands;
	rv = probe_exchange(card, wire, sizeof(wire), response, sizeof(response), &received);
	attempted = probe_guard.commands == before + 1;
	OPENSSL_cleanse(probe_guard.expected, sizeof(probe_guard.expected)); probe_guard.expected_size = 0;
	printf("login_transport=%d\n", rv); if (rv || received > sizeof(response)) goto out;
	/* Warning family can authenticate a failed attempt, never makes it
	 * successful or triggers a retry. All other exterior errors stop. */
	rv = jcop4_sm_open_report_policy(channel, response, received, plain, sizeof(plain), &plain_size, &sw,
		JCOP4_SM_OUTER_9000_OR_MATCHING_63CX, &decoded);
	printf("login_decode_result=%s\nlogin_mac_check=%s\n", jcop4_sm_decode_name(decoded.result),
		decoded.mac == JCOP4_SM_MAC_VERIFIED ? "verified" : decoded.mac == JCOP4_SM_MAC_FAILED ? "failed" : "not_checked");
	if (rv) goto out;
	printf("login_response=authenticated\nlogin_sw=%04X\n", sw);
	if (sw != 0x9000 || plain_size) goto out;
	result = 0;
out:
	printf("login_attempted=%d\nlogin_result=%s\n", attempted, result ? "not_confirmed_stop" : "pin_verified_no_signature");
	jcop4_sm_invalidate(channel); OPENSSL_cleanse(pin, sizeof(pin)); OPENSSL_cleanse(header, sizeof(header));
	OPENSSL_cleanse(body, sizeof(body)); OPENSSL_cleanse(wire, sizeof(wire)); OPENSSL_cleanse(response, sizeof(response));
	OPENSSL_cleanse(plain, sizeof(plain)); OPENSSL_cleanse(probe_guard.expected, sizeof(probe_guard.expected)); probe_guard.expected_size = 0;
	return result;
}
#endif
