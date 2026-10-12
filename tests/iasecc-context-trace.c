/* Real SELECT and reader locking with simulated transport. The diagnostic
 * must neither issue extra commands nor alter error propagation.
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "libopensc/card-iasecc.c"

static int failures, commands, lock_calls, reset_once, transport_error, status_error;

static void check(const char *name, int valid)
{
	printf("%s %s\n", valid ? "PASS" : "FAIL", name);
	if (!valid) failures++;
}

int sc_transmit_apdu(struct sc_card *card, struct sc_apdu *apdu)
{
	(void)card;
	commands++;
	if (apdu->ins != 0xA4) return SC_ERROR_NOT_SUPPORTED;
	if (transport_error) return SC_ERROR_TRANSMIT_FAILED;
	apdu->resplen = 0;
	apdu->sw1 = status_error ? 0x69 : 0x90;
	apdu->sw2 = status_error ? 0x82 : 0;
	return SC_SUCCESS;
}

static int mock_lock(struct sc_reader *reader)
{
	(void)reader;
	lock_calls++;
	if (reset_once) {
		reset_once = 0;
		return SC_ERROR_CARD_RESET;
	}
	return SC_SUCCESS;
}

static void read_trace(const char *path, char *output, size_t size)
{
	FILE *file = fopen(path, "r");
	size_t length = file ? fread(output, 1, size - 1, file) : 0;
	if (file) fclose(file);
	output[length] = 0;
}

int main(void)
{
	struct sc_context ctx = {0};
	struct sc_card_operations ops = {0};
	struct sc_reader_operations reader_ops = {0};
	struct sc_reader reader = {0};
	struct iasecc_private_data private_data = {0};
	struct sc_card card = {0};
	struct sc_path path;
	char trace_path[] = "/tmp/jcop3-context-trace-XXXXXX", trace[4096], expected[160];
	int fd = mkstemp(trace_path), rv;
	if (fd < 0) return 1;
	close(fd);
	card.ctx = &ctx;
	card.type = SC_CARD_TYPE_IASECC_AMOS;
	card.drv_data = &private_data;
	card.ops = &ops;
	card.reader = &reader;
	reader.ops = &reader_ops;
	reader_ops.lock = mock_lock;
	iso_ops = sc_get_iso7816_driver()->ops;
	ops.check_sw = iso_ops->check_sw;
	sc_format_path("E828BD080FD25047656E65726963", &path);
	path.type = SC_PATH_TYPE_DF_NAME;
	unsetenv("OPENSC_API_TRACE_FILE");
	rv = iasecc_select_file(&card, &path, NULL);
	read_trace(trace_path, trace, sizeof(trace));
	check("selection diagnostics are off by default and preserve the single command",
			rv == SC_SUCCESS && commands == 1 && !*trace);
	setenv("OPENSC_API_TRACE_FILE", trace_path, 1);
	rv = iasecc_select_file(&card, &path, NULL);
	read_trace(trace_path, trace, sizeof(trace));
	check("selection diagnostic identifies only the public application category and result",
			rv == SC_SUCCESS && commands == 2 && !strcmp(trace,
			"CARD_SELECT session=0x0 rv=0x0 a=0x3 b=0x9000\n"));
	status_error = 1;
	rv = iasecc_select_file(&card, &path, NULL);
	snprintf(expected, sizeof(expected), "CARD_SELECT session=0x0 rv=0x%lX a=0x3 b=0x6982\n",
			(unsigned long)SC_ERROR_SECURITY_STATUS_NOT_SATISFIED);
	read_trace(trace_path, trace, sizeof(trace));
	check("rejected selection is logged without retrying or changing its error",
			rv == SC_ERROR_SECURITY_STATUS_NOT_SATISFIED && commands == 3 && strstr(trace, expected));
	transport_error = 1;
	rv = iasecc_select_file(&card, &path, NULL);
	snprintf(expected, sizeof(expected), "CARD_SELECT session=0x0 rv=0x%lX a=0x3 b=0x0\n",
			(unsigned long)SC_ERROR_TRANSMIT_FAILED);
	read_trace(trace_path, trace, sizeof(trace));
	check("failed transport reports no invented response status and performs no retry",
			rv == SC_ERROR_TRANSMIT_FAILED && commands == 4 && strstr(trace, expected));
	reset_once = 1;
	rv = sc_lock(&card);
	read_trace(trace_path, trace, sizeof(trace));
	check("reader reset during locking is observable without changing the existing recovery",
			rv == SC_SUCCESS && lock_calls == 2 && card.lock_count == 1 &&
			strstr(trace, "CARD_LOCK session=0x0 rv=0x0 a=0x1 b=0x1\n"));
	sc_unlock(&card);
	unsetenv("OPENSC_API_TRACE_FILE");
	sc_invalidate_cache(&card);
	unlink(trace_path);
	return failures ? 1 : 0;
}
