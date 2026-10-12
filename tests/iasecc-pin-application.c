/* Simulated peer changes the selected application without card removal/reset.
 * Replay real policy retrieval, selection, VERIFY orchestration and RSA AT.
 * The model distinguishes CHV verification from permission for ChipDoc RSA;
 * it tests command ordering, not cryptographic or hardware correctness.
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "libopensc/card-iasecc.c"
#include "libopensc/iasecc-sdo.c"

static const unsigned char chipdoc_aid[] = {
	0xE8,0x28,0xBD,0x08,0x0F,0xD2,0x50,0x47,0x65,0x6E,0x65,0x72,0x69,0x63
};
static int failures, selected_chipdoc, chv_verified, rsa_authorized;
static int selects, verifies, signatures, metadata, statuses, forbidden;
static int select_error, select_rejected, verify_error, metadata_error;

static void check(const char *name, int valid)
{
	printf("%s %s\n", valid ? "PASS" : "FAIL", name);
	if (!valid) failures++;
}

static int mock_pin(struct sc_card *card, struct sc_pin_cmd_data *data)
{
	(void)card;
	if (data->pin_reference != 1 || data->pin_type != SC_AC_CHV) {
		forbidden++;
		return SC_ERROR_NOT_SUPPORTED;
	}
	if (data->cmd == SC_PIN_CMD_GET_INFO && !data->pin1.data && !data->pin1.len) {
		statuses++;
		data->pin1.logged_in = chv_verified ? SC_PIN_STATE_LOGGED_IN : SC_PIN_STATE_LOGGED_OUT;
		data->pin1.tries_left = 7;
		return 0;
	}
	if (data->cmd == SC_PIN_CMD_VERIFY && data->pin1.data && data->pin1.len) {
		verifies++;
		if (verify_error) return verify_error;
		chv_verified = 1;
		rsa_authorized = selected_chipdoc;
		return 0;
	}
	forbidden++;
	return SC_ERROR_NOT_SUPPORTED;
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
	if (apdu->ins == 0xA4 && apdu->p1 == 0x04 && apdu->p2 == 0x04 &&
			apdu->datalen == sizeof(chipdoc_aid) &&
			!memcmp(apdu->data, chipdoc_aid, sizeof(chipdoc_aid))) {
		selects++;
		if (select_error) return select_error;
		apdu->resplen = 0;
		apdu->sw1 = select_rejected ? 0x6A : 0x90;
		apdu->sw2 = select_rejected ? 0x82 : 0;
		if (!select_rejected) selected_chipdoc = 1;
		return 0;
	}
	if (apdu->ins == 0x88) {
		signatures++;
		if (apdu->resplen < 256) return SC_ERROR_BUFFER_TOO_SMALL;
		apdu->resplen = 0;
		apdu->sw1 = rsa_authorized && selected_chipdoc ? 0x90 : 0x69;
		apdu->sw2 = rsa_authorized && selected_chipdoc ? 0 : 0x82;
		if (apdu->sw1 == 0x90) {
			memset(apdu->resp, 0x5A, 256); /* Synthetic output, not an RSA signature. */
			apdu->resplen = 256;
		}
		return 0;
	}
	if (apdu->ins != 0xCB || apdu->p1 != 0x3F || apdu->p2 != 0xFF ||
			apdu->datalen < 10 || apdu->data[5] != 0x81 || apdu->data[6] != 1) {
		forbidden++;
		return SC_ERROR_NOT_SUPPORTED;
	}
	metadata++;
	if (metadata_error) return metadata_error;
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
	memcpy(apdu->resp, response, length);
	apdu->resplen = length;
	apdu->sw1 = 0x90;
	apdu->sw2 = 0;
	return 0;
}

static void reset_run(struct sc_card *card, struct sc_pin_cmd_data *pin)
{
	iasecc_chv_cache_clean(card, pin);
	sc_invalidate_cache(card);
	selected_chipdoc = chv_verified = rsa_authorized = 0;
	selects = verifies = signatures = metadata = statuses = forbidden = 0;
	select_error = select_rejected = verify_error = metadata_error = 0;
}

static int sign_after_select(struct sc_card *card)
{
	struct iasecc_private_data *prv = card->drv_data;
	struct sc_path path;
	unsigned char output[256];
	const unsigned char synthetic_digest[] = {0x30,0x00};
	int rv;
	sc_format_path("E828BD080FD25047656E65726963", &path);
	path.type = SC_PATH_TYPE_DF_NAME;
	rv = iasecc_select_file(card, &path, NULL);
	if (rv < 0) return rv;
	prv->security_env.operation = SC_SEC_OPERATION_AUTHENTICATE;
	return iasecc_compute_signature_at(card, synthetic_digest, sizeof(synthetic_digest),
			output, sizeof(output));
}

int main(void)
{
	struct sc_context ctx = {0};
	struct sc_card card = {0};
	struct sc_card_operations ops = {0};
	struct iasecc_private_data prv = {0};
	struct sc_app_info app = {0};
	struct sc_pin_cmd_data pin = {0};
	struct iasecc_pin_policy policy = {0};
	const unsigned char synthetic_pin[] = "1234";
	char path[] = "/tmp/jcop3-pin-application-XXXXXX", trace[4096];
	FILE *file;
	size_t length;
	int fd = mkstemp(path), rv;
	if (fd < 0) return 1;
	close(fd);
	card.ctx = &ctx;
	card.type = SC_CARD_TYPE_IASECC_AMOS;
	card.ops = &ops;
	card.drv_data = &prv;
	card.app_count = 1;
	card.app[0] = &app;
	app.aid.len = sizeof(chipdoc_aid);
	memcpy(app.aid.value, chipdoc_aid, sizeof(chipdoc_aid));
	ops.check_sw = sc_get_iso7816_driver()->ops->check_sw;
	ops.pin_cmd = mock_pin;
	iso_ops = &ops;
	pin.cmd = SC_PIN_CMD_VERIFY;
	pin.pin_type = SC_AC_CHV;
	pin.pin_reference = 1;
	pin.pin1.data = synthetic_pin;
	pin.pin1.len = sizeof(synthetic_pin) - 1;
	unsetenv("OPENSC_API_TRACE_FILE");
	unsetenv("OPENSC_JCOP3_POST_LOGIN_STATUS");
	unsetenv("OPENSC_JCOP3_POST_LOGIN_SELECT");
	unsetenv("OPENSC_JCOP3_PIN_STATUS_IN_PLACE");
	rv = iasecc_pin_verify(&card, &pin);
	check("control: root VERIFY succeeds but subsequent ChipDoc RSA returns 6982",
			rv == 0 && chv_verified && !rsa_authorized && verifies == 1 && selects == 0 &&
			sign_after_select(&card) == SC_ERROR_SECURITY_STATUS_NOT_SATISFIED && !forbidden);
	reset_run(&card, &pin);
	setenv("OPENSC_JCOP3_PIN_STATUS_IN_PLACE", "yes", 1);
	setenv("OPENSC_API_TRACE_FILE", path, 1);
	rv = iasecc_pin_verify(&card, &pin);
	check("candidate verifies in ChipDoc after peer root selection and permits subsequent RSA",
			rv == 0 && selects == 1 && verifies == 1 && metadata == 2 && statuses == 1 &&
			rsa_authorized && sign_after_select(&card) == 256 && signatures == 1 && !forbidden);
	file = fopen(path, "r");
	length = file ? fread(trace, 1, sizeof(trace) - 1, file) : 0;
	if (file) fclose(file);
	trace[length] = 0;
	check("public application selection is traced before VERIFY without credentials",
			strstr(trace, "PRE_VERIFY_SELECT session=0x0 rv=0x0 a=0x3 b=0x0\nCARD_PIN_VERIFY") &&
			!strstr(trace, "1234") && !strstr(trace, "PIN_VERIFY_CACHED"));
	unsetenv("OPENSC_API_TRACE_FILE");
	reset_run(&card, &pin);
	select_error = SC_ERROR_TRANSMIT_FAILED;
	rv = iasecc_pin_verify(&card, &pin);
	check("selection transport failure stops before credential VERIFY without retries",
			rv == select_error && selects == 1 && !verifies && !signatures && !forbidden);
	reset_run(&card, &pin);
	select_rejected = 1;
	rv = iasecc_pin_verify(&card, &pin);
	check("selection rejection stops before credential VERIFY preserving card error",
			rv == SC_ERROR_FILE_NOT_FOUND && selects == 1 && !verifies && !signatures && !forbidden);
	reset_run(&card, &pin);
	verify_error = SC_ERROR_PIN_CODE_INCORRECT;
	rv = iasecc_pin_verify(&card, &pin);
	check("wrong PIN is propagated after one selection and one VERIFY without retry",
			rv == verify_error && selects == 1 && verifies == 1 && !signatures && !forbidden);
	reset_run(&card, &pin);
	metadata_error = SC_ERROR_TRANSMIT_FAILED;
	rv = iasecc_pin_verify(&card, &pin);
	check("policy failure performs neither application selection nor credential VERIFY",
			rv == metadata_error && metadata == 1 && !selects && !verifies && !forbidden);
	reset_run(&card, &pin);
	pin.pin1.len = 0;
	rv = iasecc_pin_verify(&card, &pin);
	check("empty VERIFY remains only a status query without application selection",
			rv == 0 && statuses == 1 && !selects && !verifies && !metadata && !forbidden);
	pin.pin1.len = sizeof(synthetic_pin) - 1;
	policy.read_verify_only = 1;
	for (int variant = 0; variant < 9; variant++) {
		reset_run(&card, &pin);
		switch (variant) {
		case 0: setenv("OPENSC_JCOP3_PIN_STATUS_IN_PLACE", "no", 1); break;
		case 1: card.type = SC_CARD_TYPE_IASECC_SAGEM; break;
		case 2: pin.pin_reference |= IASECC_OBJECT_REF_LOCAL; break;
		case 3: pin.pin_reference = 2; break;
		case 4: pin.cmd = SC_PIN_CMD_CHANGE; break;
		case 5: pin.pin_type = SC_AC_AUT; break;
		case 6: app.aid.value[0] ^= 1; break;
		case 7: app.aid.len--; break;
		case 8: policy.read_verify_only = 0; break;
		}
		rv = iasecc_chipdoc_select_for_verify(&card, &pin, &policy);
		char label[100];
		snprintf(label, sizeof(label), "application selection excludes profile variant %d without card commands", variant);
		check(label, rv == 0 && !selects && !verifies && !metadata && !forbidden);
		setenv("OPENSC_JCOP3_PIN_STATUS_IN_PLACE", "yes", 1);
		card.type = SC_CARD_TYPE_IASECC_AMOS;
		pin.pin_reference = 1;
		pin.cmd = SC_PIN_CMD_VERIFY;
		pin.pin_type = SC_AC_CHV;
		app.aid.len = sizeof(chipdoc_aid);
		memcpy(app.aid.value, chipdoc_aid, sizeof(chipdoc_aid));
		policy.read_verify_only = 1;
	}
	reset_run(&card, &pin);
	unsetenv("OPENSC_JCOP3_PIN_STATUS_IN_PLACE");
	unlink(path);
	return failures ? 1 : 0;
}
