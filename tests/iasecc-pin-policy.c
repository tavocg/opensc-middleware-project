/* Exercise the driver's limited ChipDoc PIN policy with simulated metadata.
 * The transport rejects every command except GET DATA and empty PIN status.
 */
#include <stdio.h>
#include <string.h>

#include "libopensc/card-iasecc.c"
#include "libopensc/iasecc-sdo.c"

static int forbidden_commands;
static int status_queries, metadata_queries, status_error;
static int status_state = SC_PIN_STATE_LOGGED_OUT, status_tries = 7;

static int mock_pin_cmd(struct sc_card *card, struct sc_pin_cmd_data *data)
{
	(void)card;
	if (data->cmd != SC_PIN_CMD_GET_INFO || data->pin_reference != 1) {
		forbidden_commands++;
		return SC_ERROR_NOT_SUPPORTED;
	}
	/* Result of the captured empty VERIFY / 63 C7 status query. */
	status_queries++;
	if (status_error) return status_error;
	data->pin1.max_tries = -1;
	data->pin1.tries_left = status_tries;
	data->pin1.logged_in = status_state;
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
	if (apdu->ins == 0x20 && !apdu->datalen && apdu->p2 == 1) {
		apdu->sw1 = 0x63;
		apdu->sw2 = 0xC7;
		apdu->resplen = 0;
		return SC_SUCCESS;
	}
	if (apdu->ins != 0xCB || apdu->p1 != 0x3F || apdu->p2 != 0xFF ||
			apdu->datalen < 10 || apdu->data[5] != 0x81 || apdu->data[6] != 1) {
		forbidden_commands++;
		return SC_ERROR_NOT_SUPPORTED;
	}
	if (apdu->datalen == 11 && apdu->data[8] == 0x7F && apdu->data[9] == 0x41) {
		response = chv;
		length = sizeof(chv);
	} else if (apdu->datalen == 10 && apdu->data[8] == 0xA0) {
		response = docp;
		length = sizeof(docp);
	} else {
		forbidden_commands++;
		return SC_ERROR_NOT_SUPPORTED;
	}
	if (apdu->resplen < length)
		return SC_ERROR_BUFFER_TOO_SMALL;
	memcpy(apdu->resp, response, length);
	metadata_queries++;
	apdu->resplen = length;
	apdu->sw1 = 0x90;
	apdu->sw2 = 0;
	return SC_SUCCESS;
}

int main(void)
{
	struct sc_context ctx = {0};
	struct sc_card card = {0};
	struct sc_card_operations operations = {0};
	struct iasecc_private_data private_data = {0};
	struct sc_pin_cmd_data data = {0};
	int result, failures = 0;

	ctx.app_name = "iasecc-pin-policy-regression";
	card.ctx = &ctx;
	struct sc_card_driver driver = {.short_name = "iasecc"};
	struct sc_app_info app = {0};
	card.driver = &driver; card.app_count = 1; card.app[0] = &app;
	app.aid.len = IASECC_CHIPDOC_GENERIC_AID_LEN;
	memcpy(app.aid.value, IASECC_CHIPDOC_GENERIC_AID, app.aid.len);
	card.type = SC_CARD_TYPE_IASECC_AMOS;
	card.drv_data = &private_data;
	card.ops = &operations;
	iso_ops = sc_get_iso7816_driver()->ops;
	operations.check_sw = iso_ops->check_sw;
	operations.pin_cmd = mock_pin_cmd;
	iso_ops = &operations;
	data.pin_type = SC_AC_CHV;
	data.pin_reference = 1;
	data.cmd = SC_PIN_CMD_GET_INFO;
	data.flags = IASECC_PIN_STATUS_ONLY;
	result = iasecc_pin_get_info(&card, &data);
	if (result != SC_ERROR_NOT_SUPPORTED || status_queries || metadata_queries) {
		fprintf(stderr, "FAIL uninitialized policy refuses status-only before card commands\n");
		failures++;
	} else puts("PASS uninitialized policy refuses status-only before card commands");
	data.flags = 0;
	result = iasecc_pin_get_info(&card, &data);
	if (result || data.pin1.max_tries != 7 || data.pin1.tries_left != 7) {
		fprintf(stderr, "FAIL driver reads ChipDoc retry counts: %d\n", result);
		failures++;
	} else {
		puts("PASS driver reads ChipDoc retry counts");
	}
	data.flags = IASECC_PIN_STATUS_ONLY;
	status_state = SC_PIN_STATE_LOGGED_IN;
	status_tries = -1;
	result = iasecc_pin_get_info(&card, &data);
	if (result || data.pin1.max_tries != 7 || data.pin1.tries_left != -1 ||
			data.pin1.logged_in != SC_PIN_STATE_LOGGED_IN || status_queries != 2 || metadata_queries != 2) {
		fprintf(stderr, "FAIL status-only preserves unknown remaining count without policy reads\n");
		failures++;
	} else puts("PASS status-only preserves unknown remaining count without policy reads");
	status_state = SC_PIN_STATE_LOGGED_OUT;
	status_tries = 2;
	result = iasecc_pin_get_info(&card, &data);
	if (result || data.pin1.tries_left != 2 || data.pin1.logged_in != SC_PIN_STATE_LOGGED_OUT ||
			status_queries != 3 || metadata_queries != 2) {
		fprintf(stderr, "FAIL status-only detects changed authentication and remaining tries\n");
		failures++;
	} else puts("PASS status-only detects changed authentication and remaining tries");
	status_error = SC_ERROR_TRANSMIT_FAILED;
	result = iasecc_pin_get_info(&card, &data);
	if (result != status_error || status_queries != 4 || metadata_queries != 2) {
		fprintf(stderr, "FAIL status-only transport error does not retry or select\n");
		failures++;
	} else puts("PASS status-only transport error does not retry or select");
	status_error = 0;
	card.type = SC_CARD_TYPE_IASECC_SAGEM;
	result = iasecc_pin_get_info(&card, &data);
	if (result != SC_ERROR_NOT_SUPPORTED || status_queries != 4) failures++;
	else puts("PASS status-only excludes other card types");
	card.type = SC_CARD_TYPE_IASECC_AMOS;
	data.pin_reference = 1 | IASECC_OBJECT_REF_LOCAL;
	result = iasecc_pin_get_info(&card, &data);
	if (result != SC_ERROR_NOT_SUPPORTED || status_queries != 4) failures++;
	else puts("PASS status-only excludes local CHVs");
	data.pin_reference = 2;
	result = iasecc_pin_get_info(&card, &data);
	if (result != SC_ERROR_NOT_SUPPORTED || status_queries != 4) failures++;
	else puts("PASS status-only excludes other CHV references");
	data.pin_reference = 1;
	data.pin_type = SC_AC_AUT;
	result = iasecc_pin_get_info(&card, &data);
	if (result != SC_ERROR_NOT_SUPPORTED || status_queries != 4) failures++;
	else puts("PASS status-only excludes other authentication methods");
	data.pin_type = SC_AC_CHV;
	data.flags = 0;
	data.cmd = SC_PIN_CMD_CHANGE;
	result = iasecc_pin_change(&card, &data);
	if (result != SC_ERROR_NOT_SUPPORTED || forbidden_commands) {
		fprintf(stderr, "FAIL ChipDoc PIN change stops before authentication: %d\n", result);
		failures++;
	} else {
		puts("PASS ChipDoc PIN change stops before authentication");
	}
	data.cmd = SC_PIN_CMD_UNBLOCK;
	result = iasecc_pin_reset(&card, &data);
	if (result != SC_ERROR_NOT_SUPPORTED || forbidden_commands) {
		fprintf(stderr, "FAIL ChipDoc PIN unblock stops before authentication: %d\n", result);
		failures++;
	} else {
		puts("PASS ChipDoc PIN unblock stops before authentication");
	}
	return failures ? 1 : 0;
}
