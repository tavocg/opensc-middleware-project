/* Real ISO PIN encoding with simulated transport and synthetic credentials.
 * Distinguish local validation from a card/transport failure without logging
 * PIN bytes, PIN length or a credential-bearing APDU.
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define sc_get_iso7816_driver fixture_get_iso7816_driver
#define iso7816_fixup_transceive_length fixture_fixup_transceive_length
#include "libopensc/iso7816.c"
#undef sc_get_iso7816_driver
#undef iso7816_fixup_transceive_length

static int failures, commands, transport_error, wrong_pin;

/* Other retained ISO entry points require these hidden library helpers.
 * They are not part of the PIN path exercised by this fixture. */
size_t sc_get_max_send_size(const struct sc_card *card)
{
	(void)card;
	return 255;
}

size_t sc_get_max_recv_size(const struct sc_card *card)
{
	(void)card;
	return 256;
}

static void check(const char *name, int valid)
{
	printf("%s %s\n", valid ? "PASS" : "FAIL", name);
	if (!valid) failures++;
}

int sc_transmit_apdu(struct sc_card *card, struct sc_apdu *apdu)
{
	(void)card;
	commands++;
	if (transport_error) return transport_error;
	apdu->sw1 = wrong_pin ? 0x63 : 0x90;
	apdu->sw2 = wrong_pin ? 0xC6 : 0;
	return SC_SUCCESS;
}

static void read_trace(const char *path, char *output, size_t size)
{
	FILE *file = fopen(path, "r");
	size_t length = file ? fread(output, 1, size - 1, file) : 0;
	if (file) fclose(file);
	output[length] = 0;
}

static void clear_trace(const char *path)
{
	FILE *file = fopen(path, "w");
	if (!file) exit(1);
	fclose(file);
	commands = transport_error = wrong_pin = 0;
}

int main(void)
{
	struct sc_context ctx = {0};
	struct sc_card card = {0};
	struct sc_card_operations ops = {0};
	struct sc_pin_cmd_data pin = {0};
	const unsigned char synthetic_pin[] = "12345678";
	char path[] = "/tmp/jcop3-pin-stage-XXXXXX", trace[2048], expected[256];
	int fd = mkstemp(path), rv;
	if (fd < 0) return 1;
	close(fd);
	card.ctx = &ctx;
	card.ops = &ops;
	card.caps = SC_CARD_CAP_ISO7816_PIN_INFO;
	ops.check_sw = iso7816_check_sw;
	pin.cmd = SC_PIN_CMD_VERIFY;
	pin.pin_type = SC_AC_CHV;
	pin.pin_reference = 1;
	pin.pin1.encoding = SC_PIN_ENCODING_ASCII;
	pin.pin1.max_length = 14;
	pin.pin1.data = synthetic_pin;
	pin.pin1.len = sizeof(synthetic_pin) - 1;
	unsetenv("OPENSC_API_TRACE_FILE");
	rv = iso7816_pin_cmd(&card, &pin);
	read_trace(path, trace, sizeof(trace));
	check("disabled diagnostics preserve exactly one VERIFY with no trace",
			rv == 0 && commands == 1 && !*trace && pin.apdu == NULL);
	clear_trace(path);
	setenv("OPENSC_API_TRACE_FILE", path, 1);
	rv = iso7816_pin_cmd(&card, &pin);
	read_trace(path, trace, sizeof(trace));
	check("successful VERIFY distinguishes build and transport using public metadata only",
			rv == 0 && commands == 1 && !strcmp(trace,
			"PIN_APDU_BUILD session=0x0 rv=0x0 a=0x1 b=0x0\n"
			"PIN_TRANSMIT session=0x0 rv=0x0 a=0x1 b=0x9000\n"));
	clear_trace(path);
	/* Keep all synthetic bytes accessible; tighten the public limit instead. */
	pin.pin1.max_length = 4;
	rv = iso7816_pin_cmd(&card, &pin);
	read_trace(path, trace, sizeof(trace));
	snprintf(expected, sizeof(expected),
			"PIN_APDU_BUILD session=0x0 rv=0x%lX a=0x1 b=0x0\n",
			(unsigned long)SC_ERROR_INVALID_ARGUMENTS);
	check("over-limit synthetic input fails before transport without revealing input or length",
			rv == SC_ERROR_INVALID_ARGUMENTS && !commands && pin.apdu == NULL && !strcmp(trace, expected));
	clear_trace(path);
	pin.pin1.max_length = 14;
	transport_error = SC_ERROR_INVALID_ARGUMENTS;
	rv = iso7816_pin_cmd(&card, &pin);
	read_trace(path, trace, sizeof(trace));
	snprintf(expected, sizeof(expected),
			"PIN_TRANSMIT session=0x0 rv=0x%lX a=0x1 b=0x0\n",
			(unsigned long)SC_ERROR_INVALID_ARGUMENTS);
	check("transport argument failure is distinct from encoding rejection with no retry",
			rv == SC_ERROR_INVALID_ARGUMENTS && commands == 1 && pin.apdu == NULL &&
			strstr(trace, "PIN_APDU_BUILD session=0x0 rv=0x0") && strstr(trace, expected));
	clear_trace(path);
	wrong_pin = 1;
	rv = iso7816_pin_cmd(&card, &pin);
	read_trace(path, trace, sizeof(trace));
	check("wrong-PIN card response remains distinct and performs no retry",
			rv == SC_ERROR_PIN_CODE_INCORRECT && commands == 1 && pin.pin1.tries_left == 6 &&
			strstr(trace, "PIN_TRANSMIT session=0x0 rv=0x0 a=0x1 b=0x63C6"));
	clear_trace(path);
	pin.cmd = SC_PIN_CMD_GET_INFO;
	rv = iso7816_pin_cmd(&card, &pin);
	read_trace(path, trace, sizeof(trace));
	check("credential-free status queries do not produce VERIFY diagnostics",
			rv == 0 && commands == 1 && !*trace);
	clear_trace(path);
	pin.cmd = SC_PIN_CMD_VERIFY;
	snprintf(expected, sizeof(expected), "%s/missing", path);
	setenv("OPENSC_API_TRACE_FILE", expected, 1);
	rv = iso7816_pin_cmd(&card, &pin);
	check("unwritable diagnostic does not affect authentication or add commands",
			rv == 0 && commands == 1 && pin.apdu == NULL);
	unsetenv("OPENSC_API_TRACE_FILE");
	unlink(path);
	return failures ? 1 : 0;
}
