/* One isolated empty status query after a real successful VERIFY. No hardware
 * or actual PIN: the only credential here is a fixed synthetic fixture.
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "libopensc/card-iasecc.c"
#include "libopensc/iasecc-sdo.c"

static int failures, status_queries, verifies, metadata_queries, forbidden;
static int authenticated, verify_error, status_error;

static void check(const char *name, int valid)
{
	printf("%s %s\n", valid ? "PASS" : "FAIL", name);
	if (!valid) failures++;
}

static int mock_pin(struct sc_card *card, struct sc_pin_cmd_data *data)
{
	(void)card;
	if (data->cmd == SC_PIN_CMD_VERIFY && data->pin1.data && data->pin1.len) {
		verifies++;
		if (verify_error) return verify_error;
		authenticated = 1;
		return SC_SUCCESS;
	}
	if (data->cmd != SC_PIN_CMD_GET_INFO || data->pin_reference != 1 ||
			data->pin1.data || data->pin1.len || data->pin2.data || data->pin2.len ||
			data->flags) {
		forbidden++;
		return SC_ERROR_NOT_SUPPORTED;
	}
	status_queries++;
	if (authenticated && status_error) return status_error;
	data->pin1.max_tries = -1;
	data->pin1.tries_left = 7;
	data->pin1.logged_in = authenticated ? SC_PIN_STATE_LOGGED_IN : SC_PIN_STATE_LOGGED_OUT;
	return SC_SUCCESS;
}

int sc_transmit_apdu(struct sc_card *card, struct sc_apdu *apdu)
{
	static const unsigned char chv[] = {
		0x70,0x0D,0xBF,0x81,0x01,0x09,0x7F,0x41,0x06,0x80,0x01,0x0E,0x81,0x01,0x04
	};
	static const unsigned char docp[] = {
		0x70,0x2B,0xBF,0x81,0x01,0x27,0xA0,0x25,0xC9,0x01,0x01,
		0x9A,0x01,0x07,0x9B,0x01,0x07,0x80,0x02,0x00,0x0E,
		0xA1,0x16,0x8C,0x09,0xF3,0xE7,0x00,0xE7,0xFF,0x00,0xD0,0xD3,0xD3,
		0x9C,0x09,0xF3,0xE7,0x00,0xE7,0xFF,0x00,0xD0,0xD3,0xD3
	};
	const unsigned char *response;
	size_t length;
	(void)card;
	if (authenticated || apdu->ins != 0xCB || apdu->p1 != 0x3F || apdu->p2 != 0xFF ||
			apdu->datalen < 10 || apdu->data[5] != 0x81 || apdu->data[6] != 1) {
		forbidden++;
		return SC_ERROR_NOT_SUPPORTED;
	}
	if (apdu->datalen == 11 && apdu->data[8] == 0x7F && apdu->data[9] == 0x41) {
		response = chv;
		length = sizeof(chv);
	} else if (apdu->datalen == 10 && apdu->data[8] == 0xA0) {
		response = docp;
		length = sizeof(docp);
	} else {
		forbidden++;
		return SC_ERROR_NOT_SUPPORTED;
	}
	if (apdu->resplen < length) return SC_ERROR_BUFFER_TOO_SMALL;
	metadata_queries++;
	memcpy(apdu->resp, response, length);
	apdu->resplen = length;
	apdu->sw1 = 0x90;
	apdu->sw2 = 0;
	return SC_SUCCESS;
}

static void clear_run(struct sc_card *card, struct sc_pin_cmd_data *pin)
{
	iasecc_chv_cache_clean(card, pin);
	status_queries = verifies = metadata_queries = forbidden = 0;
	authenticated = verify_error = status_error = 0;
}

int main(void)
{
	struct sc_context ctx = {0};
	struct sc_card card = {0};
	struct sc_card_operations ops = {0};
	struct iasecc_private_data private_data = {0};
	struct sc_app_info app = {0};
	struct sc_pin_cmd_data pin = {0};
	struct iasecc_pin_policy policy = {0};
	const unsigned char aid[] = {
		0xE8,0x28,0xBD,0x08,0x0F,0xD2,0x50,0x47,0x65,0x6E,0x65,0x72,0x69,0x63
	};
	const unsigned char synthetic_pin[] = "1234";
	char path[] = "/tmp/jcop3-status-probe-XXXXXX", output[4096];
	FILE *file;
	size_t length;
	int fd = mkstemp(path), rv;
	if (fd < 0) return 1;
	close(fd);
	unsetenv("OPENSC_API_TRACE_FILE");
	unsetenv("OPENSC_JCOP3_POST_LOGIN_STATUS");
	card.ctx = &ctx;
	card.type = SC_CARD_TYPE_IASECC_AMOS;
	card.drv_data = &private_data;
	card.ops = &ops;
	card.app[0] = &app;
	card.app_count = 1;
	memcpy(app.aid.value, aid, sizeof(aid));
	app.aid.len = sizeof(aid);
	ops.check_sw = sc_get_iso7816_driver()->ops->check_sw;
	ops.pin_cmd = mock_pin;
	iso_ops = &ops;
	pin.cmd = SC_PIN_CMD_VERIFY;
	pin.pin_type = SC_AC_CHV;
	pin.pin_reference = 1;
	pin.pin1.data = synthetic_pin;
	pin.pin1.len = sizeof(synthetic_pin) - 1;
	policy.read_verify_only = 1;

	rv = iasecc_pin_verify(&card, &pin);
	check("default VERIFY adds no post-login query",
			rv == 0 && verifies == 1 && status_queries == 1 && metadata_queries == 2 && !forbidden);
	clear_run(&card, &pin);
	setenv("OPENSC_JCOP3_POST_LOGIN_STATUS", "yes", 1);
	setenv("OPENSC_API_TRACE_FILE", path, 1);
	rv = iasecc_pin_verify(&card, &pin);
	check("opt-in adds exactly one credential-free status query after successful VERIFY",
			rv == 0 && verifies == 1 && status_queries == 2 && metadata_queries == 2 && !forbidden);
	file = fopen(path, "r");
	length = file ? fread(output, 1, sizeof(output) - 1, file) : 0;
	if (file) fclose(file);
	output[length] = 0;
	check("isolated query trace contains only numeric results and no credential",
			!strcmp(output, "POST_LOGIN_STATUS_BEGIN session=0x0 rv=0x0 a=0x1 b=0x0\n"
			"POST_LOGIN_STATUS session=0x0 rv=0x0 a=0x2 b=0x0\n"));
	rv = iasecc_pin_verify(&card, &pin);
	check("existing verified-PIN shortcut does not repeat the post-VERIFY experiment",
			rv == 0 && verifies == 1 && status_queries == 3 && metadata_queries == 2 && !forbidden);
	unsetenv("OPENSC_API_TRACE_FILE");
	clear_run(&card, &pin);
	verify_error = SC_ERROR_PIN_CODE_INCORRECT;
	rv = iasecc_pin_verify(&card, &pin);
	check("failed authentication never runs the probe or retries VERIFY",
			rv == verify_error && verifies == 1 && status_queries == 1 && !forbidden);
	clear_run(&card, &pin);
	status_error = SC_ERROR_TRANSMIT_FAILED;
	rv = iasecc_pin_verify(&card, &pin);
	check("post-login status transport failure stops without retry",
			rv == status_error && verifies == 1 && status_queries == 2 && !forbidden);
	clear_run(&card, &pin);
	setenv("OPENSC_JCOP3_POST_LOGIN_STATUS", "no", 1);
	rv = iasecc_chipdoc_post_login_status(&card, &pin, &policy);
	check("probe requires exact opt-in value", rv == 0 && !status_queries);
	setenv("OPENSC_JCOP3_POST_LOGIN_STATUS", "yes", 1);
	card.type = SC_CARD_TYPE_IASECC_SAGEM;
	iasecc_chipdoc_post_login_status(&card, &pin, &policy);
	check("probe excludes other card types", !status_queries);
	card.type = SC_CARD_TYPE_IASECC_AMOS;
	policy.read_verify_only = 0;
	iasecc_chipdoc_post_login_status(&card, &pin, &policy);
	check("probe requires the observed parsed policy", !status_queries);
	policy.read_verify_only = 1;
	pin.pin_reference = 1 | IASECC_OBJECT_REF_LOCAL;
	iasecc_chipdoc_post_login_status(&card, &pin, &policy);
	check("probe excludes local PINs", !status_queries);
	pin.pin_reference = 2;
	iasecc_chipdoc_post_login_status(&card, &pin, &policy);
	check("probe excludes other PIN references", !status_queries);
	pin.pin_reference = 1;
	pin.pin_type = SC_AC_AUT;
	iasecc_chipdoc_post_login_status(&card, &pin, &policy);
	check("probe excludes other authentication types", !status_queries);
	pin.pin_type = SC_AC_CHV;
	pin.cmd = SC_PIN_CMD_CHANGE;
	iasecc_chipdoc_post_login_status(&card, &pin, &policy);
	check("probe excludes administrative operations", !status_queries);
	pin.cmd = SC_PIN_CMD_VERIFY;
	app.aid.value[0] ^= 1;
	iasecc_chipdoc_post_login_status(&card, &pin, &policy);
	check("probe excludes other application AIDs", !status_queries);
	app.aid.value[0] ^= 1;
	app.aid.len--;
	iasecc_chipdoc_post_login_status(&card, &pin, &policy);
	check("probe rejects partial AID matches", !status_queries);
	app.aid.len++;
	card.app[0] = NULL;
	iasecc_chipdoc_post_login_status(&card, &pin, &policy);
	check("probe tolerates absent application metadata", !status_queries);
	card.app_count = 0;
	iasecc_chipdoc_post_login_status(&card, &pin, &policy);
	check("probe requires an enumerated application", !status_queries);
	unsetenv("OPENSC_JCOP3_POST_LOGIN_STATUS");
	unlink(path);
	return failures ? 1 : 0;
}
