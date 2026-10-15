/* Replay public RSA DOCP and the MSE configuration accepted by the card.
 * Simulated transport rejects PINs, private templates and crypto operations.
 */
#include <stdio.h>
#include <string.h>

#include "libopensc/card-iasecc.c"
#include "libopensc/iasecc-sdo.c"
#include "libopensc/iasecc-chipdoc-sm.c"
#include "libopensc/iasecc-chipdoc-bind.c"

static int failures, metadata_queries, mse_queries, forbidden_commands;
static int reject_mse;

static void check(const char *name, int valid)
{
	if (valid) {
		printf("PASS %s\n", name);
	} else {
		fprintf(stderr, "FAIL %s\n", name);
		failures++;
	}
}

int sc_transmit_apdu(struct sc_card *card, struct sc_apdu *apdu)
{
	static const unsigned char request[] = {0x4D,0x08,0x70,0x06,0xBF,0x90,0x03,0x02,0xA0,0x80};
	static const unsigned char docp[] = {
		0x70,0x2B,0xBF,0x90,0x03,0x27,0xA0,0x25,0x9E,0x01,0x00,
		0x80,0x02,0x01,0x00,0xA1,0x1C,
		0x8C,0x0C,0xC0,0x13,0x90,0x13,0xA0,0x13,0x88,0x13,0x82,0xD3,0x81,0x00,
		0x9C,0x0C,0xC0,0x13,0x90,0x13,0xA0,0x13,0x88,0x13,0x82,0xD3,0x81,0x00
	};
	static const unsigned char ht[] = {0x80,0x01,0x40};
	static const unsigned char dst[] = {0x84,0x01,0x83,0x80,0x01,0x42};
	static const unsigned char at[] = {0x84,0x01,0x83,0x80,0x01,0x02};
	int valid;

	(void)card;
	if (apdu->ins == 0xCB && apdu->p1 == 0x3F && apdu->p2 == 0xFF &&
			apdu->datalen == sizeof(request) && !memcmp(apdu->data, request, sizeof(request))) {
		if (apdu->resplen < sizeof(docp))
			return SC_ERROR_BUFFER_TOO_SMALL;
		memcpy(apdu->resp, docp, sizeof(docp));
		apdu->resplen = sizeof(docp);
		metadata_queries++;
	} else if (apdu->ins == 0x22 && apdu->p1 == 0x41) {
		valid = (apdu->p2 == 0xAA && mse_queries == 0 && apdu->datalen == sizeof(ht) &&
				!memcmp(apdu->data, ht, sizeof(ht))) ||
			(apdu->p2 == 0xB6 && mse_queries == 1 && apdu->datalen == sizeof(dst) &&
				!memcmp(apdu->data, dst, sizeof(dst))) ||
			(apdu->p2 == 0xA4 && mse_queries == 0 && apdu->datalen == sizeof(at) &&
				!memcmp(apdu->data, at, sizeof(at)));
		if (!valid) {
			forbidden_commands++;
			return SC_ERROR_NOT_SUPPORTED;
		}
		mse_queries++;
		apdu->resplen = 0;
		if (reject_mse) {
			apdu->sw1 = 0x6A;
			apdu->sw2 = 0x80;
			return SC_SUCCESS;
		}
	} else {
		forbidden_commands++;
		return SC_ERROR_NOT_SUPPORTED;
	}
	apdu->sw1 = 0x90;
	apdu->sw2 = 0;
	return SC_SUCCESS;
}

static void reset_transport(void)
{
	metadata_queries = mse_queries = forbidden_commands = reject_mse = 0;
}

int main(void)
{
	static const unsigned char aid[] = {
		0xE8,0x28,0xBD,0x08,0x0F,0xD2,0x50,0x47,0x65,0x6E,0x65,0x72,0x69,0x63
	};
	struct sc_context ctx = {0};
	struct sc_card card = {0};
	struct sc_card_operations operations = {0};
	struct iasecc_private_data private_data = {0};
	struct sc_file df = {0};
	struct sc_security_env env = {0};
	int result;

	ctx.app_name = "iasecc-sign-setup-regression";
	card.ctx = &ctx;
	card.type = SC_CARD_TYPE_IASECC_AMOS;
	card.drv_data = &private_data;
	card.ops = &operations;
	iso_ops = sc_get_iso7816_driver()->ops;
	operations.check_sw = iso_ops->check_sw;
	private_data.cache.valid = 1;
	private_data.cache.current_df = &df;
	df.path.type = SC_PATH_TYPE_DF_NAME;
	df.path.len = sizeof(aid);
	memcpy(df.path.value, aid, sizeof(aid));
	env.algorithm = SC_ALGORITHM_RSA;
	env.key_size_bits = 2048;
	env.flags = SC_SEC_ENV_KEY_REF_PRESENT;
	env.key_ref_len = 1;
	env.key_ref[0] = 3;
	env.operation = SC_SEC_OPERATION_SIGN;
	env.algorithm_flags = SC_ALGORITHM_RSA_PAD_PKCS1_TYPE_01 | SC_ALGORITHM_RSA_HASH_SHA256;
	result = iasecc_set_security_env(&card, &env, 0);
	check("ChipDoc SHA-256 setup reads only DOCP then sends captured HT/DST", result == SC_SUCCESS &&
			metadata_queries == 1 && mse_queries == 2 && !forbidden_commands &&
			private_data.key_size == 256 && private_data.op_method == SC_AC_SEN &&
			private_data.op_ref == 3 && private_data.security_env.operation == SC_SEC_OPERATION_SIGN);
	reset_transport();
	env.algorithm_flags = SC_ALGORITHM_RSA_PAD_PKCS1_TYPE_01 | SC_ALGORITHM_RSA_HASH_NONE;
	result = iasecc_set_security_env(&card, &env, 0);
	check("ChipDoc RSA PKCS setup reads only DOCP then sends captured AT", result == SC_SUCCESS &&
			metadata_queries == 1 && mse_queries == 1 && !forbidden_commands &&
			private_data.security_env.operation == SC_SEC_OPERATION_AUTHENTICATE);
	reset_transport();
	reject_mse = 1;
	env.algorithm_flags = SC_ALGORITHM_RSA_PAD_PKCS1_TYPE_01 | SC_ALGORITHM_RSA_HASH_SHA256;
	memset(&private_data.security_env, 0, sizeof(private_data.security_env));
	result = iasecc_set_security_env(&card, &env, 0);
	check("a rejected MSE stops setup without retry or crypto command", result < 0 &&
			metadata_queries == 1 && mse_queries == 1 && !forbidden_commands &&
			!private_data.security_env.operation);
	reset_transport();
	env.algorithm_flags = SC_ALGORITHM_RSA_PAD_PKCS1_TYPE_01 | SC_ALGORITHM_RSA_HASH_SHA1;
	result = iasecc_set_security_env(&card, &env, 0);
	check("unprobed SHA-1 has no implicit algorithm fallback", result == SC_ERROR_NOT_SUPPORTED &&
			metadata_queries == 1 && !mse_queries && !forbidden_commands);
	for (int variant = 0; variant < 10; variant++) {
		const char *names[] = {
			"another card gets no algorithm fallback", "another AID gets no algorithm fallback",
			"another key gets no algorithm fallback", "another RSA size gets no algorithm fallback",
			"missing key reference flag gets no fallback", "multiple key reference bytes get no fallback",
			"another algorithm gets no fallback", "an invalid DF cache gets no fallback",
			"a missing DF gets no fallback", "a file path gets no fallback"
		};
		struct sc_security_env changed = env;
		card.type = SC_CARD_TYPE_IASECC_AMOS;
		private_data.cache.valid = 1;
		private_data.cache.current_df = &df;
		df.path.type = SC_PATH_TYPE_DF_NAME;
		memcpy(df.path.value, aid, sizeof(aid));
		switch (variant) {
		case 0: card.type = SC_CARD_TYPE_IASECC_GEMALTO; break;
		case 1: df.path.value[0] ^= 1; break;
		case 2: changed.key_ref[0] = 2; break;
		case 3: changed.key_size_bits = 1024; break;
		case 4: changed.flags = 0; break;
		case 5: changed.key_ref_len = 2; break;
		case 6: changed.algorithm = SC_ALGORITHM_EC; break;
		case 7: private_data.cache.valid = 0; break;
		case 8: private_data.cache.current_df = NULL; break;
		case 9: df.path.type = SC_PATH_TYPE_PATH; break;
		}
		check(names[variant], !iasecc_get_algorithm(&card, &changed, SC_PKCS15_ALGO_OP_HASH, CKM_SHA256));
	}
	card.type = SC_CARD_TYPE_IASECC_AMOS;
	private_data.cache.valid = 1;
	private_data.cache.current_df = &df;
	df.path.type = SC_PATH_TYPE_DF_NAME;
	memcpy(df.path.value, aid, sizeof(aid));
	env.supported_algos[0].reference = 1;
	env.supported_algos[0].operations = SC_PKCS15_ALGO_OP_COMPUTE_SIGNATURE;
	env.supported_algos[0].mechanism = CKM_RSA_PKCS;
	env.supported_algos[0].algo_ref = 0x55; /* Synthetic declared table, never sent. */
	check("a declared algorithm reference takes precedence", iasecc_get_algorithm(&card, &env,
			SC_PKCS15_ALGO_OP_COMPUTE_SIGNATURE, CKM_RSA_PKCS) == 0x55);
	check("an incomplete declared table is not filled with guesses", !iasecc_get_algorithm(&card, &env,
			SC_PKCS15_ALGO_OP_HASH, CKM_SHA256));
	env.supported_algos[0].algo_ref = 0;
	check("a declared zero reference is not replaced", !iasecc_get_algorithm(&card, &env,
			SC_PKCS15_ALGO_OP_COMPUTE_SIGNATURE, CKM_RSA_PKCS));
	return failures ? 1 : 0;
}
