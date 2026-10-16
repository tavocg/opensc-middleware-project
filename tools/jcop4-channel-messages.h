#ifndef MIDDLEWARE_JCOP4_CHANNEL_MESSAGES_H
#define MIDDLEWARE_JCOP4_CHANNEL_MESSAGES_H
#include "jcop4-channel-primitives.h"
/* Shared implementation owned by the local OpenSC submodule. */
#include "../vendor/opensc/src/libopensc/jcop4-channel-messages.h"

enum jcop4_sm_decode_result {
	JCOP4_SM_DECODE_ARGUMENT,
	JCOP4_SM_DECODE_SIZE,
	JCOP4_SM_DECODE_OUTER_SW,
	JCOP4_SM_DECODE_SSC,
	JCOP4_SM_DECODE_CIPHER_TLV,
	JCOP4_SM_DECODE_STATUS_TLV,
	JCOP4_SM_DECODE_MAC_TLV,
	JCOP4_SM_DECODE_MAC,
	JCOP4_SM_DECODE_DECRYPT,
	JCOP4_SM_DECODE_PADDING,
	JCOP4_SM_DECODE_CAPACITY,
	JCOP4_SM_DECODE_OK,
	JCOP4_SM_DECODE_STATUS_MISMATCH
};
/* Explicit opt-ins for observed response families. No policy accepts a bare
 * status or bypasses MAC/structure validation. The PSO policy accepts only
 * the complete status-only16-byte error envelope, never signature data. */

enum jcop4_sm_mac_result { JCOP4_SM_MAC_NOT_CHECKED,
	JCOP4_SM_MAC_FAILED,
	JCOP4_SM_MAC_VERIFIED };
struct jcop4_sm_decode_report {
	enum jcop4_sm_decode_result result;
	enum jcop4_sm_mac_result mac;
};

/* Decoder telemetry belongs to the standalone diagnostics, not libOpenSC. */
static inline int
jcop4_diagnostic_sm_open_report_policy_ex(OSSL_LIB_CTX *libctx, struct jcop4_sm_channel *channel, const unsigned char *response,
		size_t size, unsigned char *plain, size_t capacity, size_t *written, unsigned int *sw,
		enum jcop4_sm_outer_policy policy, struct jcop4_sm_decode_report *report)
{
	unsigned char next[16] = {0}, iv[16] = {0};
	unsigned char authenticated[JCOP4_SM_CRYPTO_LIMIT] = {0}, decrypted[JCOP4_SM_CRYPTO_LIMIT] = {0};
	const unsigned char *ciphertext = NULL, *status = NULL, *tag = NULL;
	size_t offset = 0, cipher_size = 0, length = 0, auth_end = 0, auth_size = 0, plain_size = 0;
	unsigned int inner_sw = 0, outer_sw = 0;
	struct jcop4_sm_decode_report local = {JCOP4_SM_DECODE_ARGUMENT, JCOP4_SM_MAC_NOT_CHECKED};
	int result = 1;
	if (written)
		*written = 0;
	if (sw)
		*sw = 0;
	if (!channel || channel->phase != JCOP4_SM_WAITING || !written || !sw || !plain || !response ||
			(policy != JCOP4_SM_OUTER_9000 && policy != JCOP4_SM_OUTER_9000_OR_MATCHING_63CX &&
					policy != JCOP4_SM_OUTER_PSO_STATUS_6A82))
		goto out;
	local.result = JCOP4_SM_DECODE_SIZE;
	if (size < 2 || size > JCOP4_SM_CRYPTO_LIMIT - 32)
		goto out;
	local.result = JCOP4_SM_DECODE_OUTER_SW;
	outer_sw = (unsigned int)response[size - 2] * 256 + response[size - 1];
	if (policy == JCOP4_SM_OUTER_PSO_STATUS_6A82) {
		if (outer_sw != 0x6A82 || size != 16 || response[0] != 0x99)
			goto out;
	} else if (outer_sw != 0x9000 && !(policy == JCOP4_SM_OUTER_9000_OR_MATCHING_63CX && (outer_sw & 0xFFF0) == 0x63C0))
		goto out;
	local.result = JCOP4_SM_DECODE_SIZE;
	if (size < 16)
		goto out;
	local.result = JCOP4_SM_DECODE_SSC;
	if (jcop4_sm_next_ssc(channel->keys.ssc, next))
		goto out;
	if (response[0] == 0x87) {
		local.result = JCOP4_SM_DECODE_CIPHER_TLV;
		if (jcop4_sm_take_tlv(response, size - 2, &offset, 0x87, &ciphertext, &length) ||
				length < 17 || ciphertext[0] != 1 || (length - 1) % 16 ||
				length - 1 > JCOP4_SM_DATA_LIMIT + 16)
			goto out;
		ciphertext++;
		cipher_size = length - 1;
	}
	local.result = JCOP4_SM_DECODE_STATUS_TLV;
	if (jcop4_sm_take_tlv(response, size - 2, &offset, 0x99, &status, &length) || length != 2)
		goto out;
	auth_end = offset;
	local.result = JCOP4_SM_DECODE_MAC_TLV;
	if (jcop4_sm_take_tlv(response, size - 2, &offset, 0x8E, &tag, &length) || length != 8 || offset != size - 2)
		goto out;
	memcpy(authenticated, next, 16);
	memcpy(authenticated + 16, response, auth_end);
	local.result = JCOP4_SM_DECODE_MAC;
	local.mac = JCOP4_SM_MAC_FAILED;
	if (jcop4_sm_pad(authenticated, 16 + auth_end, authenticated, sizeof(authenticated), &auth_size) ||
			jcop4_sm_verify_mac8_ex(libctx, channel->keys.mac, authenticated, auth_size, tag))
		goto out;
	local.mac = JCOP4_SM_MAC_VERIFIED;
	inner_sw = (unsigned int)status[0] * 256 + status[1];
	local.result = JCOP4_SM_DECODE_STATUS_MISMATCH;
	if (outer_sw != 0x9000 && inner_sw != outer_sw)
		goto out;
	/* Nothing reaches the caller before MAC, padding and capacity all pass. */
	if (cipher_size) {
		local.result = JCOP4_SM_DECODE_DECRYPT;
		if (jcop4_sm_ssc_iv_ex(libctx, channel->keys.encryption, next, iv) ||
				jcop4_sm_aes128_cbc_ex(libctx, 0, channel->keys.encryption, iv, ciphertext, cipher_size, decrypted, sizeof(decrypted)))
			goto out;
		local.result = JCOP4_SM_DECODE_PADDING;
		if (jcop4_sm_unpad(decrypted, cipher_size, &plain_size))
			goto out;
	}
	local.result = JCOP4_SM_DECODE_CAPACITY;
	if (plain_size > JCOP4_SM_DATA_LIMIT || plain_size > capacity)
		goto out;
	if (plain_size)
		memcpy(plain, decrypted, plain_size);
	*written = plain_size;
	*sw = inner_sw;
	memcpy(channel->keys.ssc, next, 16);
	channel->phase = JCOP4_SM_IDLE;
	local.result = JCOP4_SM_DECODE_OK;
	result = 0;
out:
	if (report)
		*report = local;
	if (result)
		jcop4_sm_invalidate(channel);
	OPENSSL_cleanse(next, sizeof(next));
	OPENSSL_cleanse(authenticated, sizeof(authenticated));
	OPENSSL_cleanse(iv, sizeof(iv));
	OPENSSL_cleanse(decrypted, sizeof(decrypted));
	return result;
}

/* Standalone diagnostic adapters; production passes an explicit libctx. */
static inline int
jcop4_sm_ssc_iv(const unsigned char key[16], const unsigned char ssc[16], unsigned char iv[16])
{
	return jcop4_sm_ssc_iv_ex(NULL, key, ssc, iv);
}

static inline int
jcop4_sm_seal_policy(struct jcop4_sm_channel *channel, const unsigned char header[4],
		const unsigned char *data, size_t data_size, int has_le, unsigned int le,
		unsigned char protected_header[4], unsigned char *body, size_t capacity, size_t *written,
		enum jcop4_sm_le_encoding encoding)
{
	return jcop4_sm_seal_policy_ex(NULL, channel, header, data, data_size, has_le, le,
			protected_header, body, capacity, written, encoding);
}

static inline int
jcop4_sm_seal(struct jcop4_sm_channel *channel, const unsigned char header[4],
		const unsigned char *data, size_t data_size, int has_le, unsigned int le,
		unsigned char protected_header[4], unsigned char *body, size_t capacity, size_t *written)
{
	return jcop4_sm_seal_policy(channel, header, data, data_size, has_le, le,
			protected_header, body, capacity, written, JCOP4_SM_LE_SHORT);
}

static inline const char *
jcop4_sm_decode_name(enum jcop4_sm_decode_result result)
{
	switch (result) {
	case JCOP4_SM_DECODE_ARGUMENT:
		return "invalid_arguments_or_state";
	case JCOP4_SM_DECODE_SIZE:
		return "response_size_invalid";
	case JCOP4_SM_DECODE_OUTER_SW:
		return "outer_sw_not_9000";
	case JCOP4_SM_DECODE_SSC:
		return "ssc_overflow";
	case JCOP4_SM_DECODE_CIPHER_TLV:
		return "encrypted_object_invalid";
	case JCOP4_SM_DECODE_STATUS_TLV:
		return "status_object_invalid_or_unsupported";
	case JCOP4_SM_DECODE_MAC_TLV:
		return "mac_object_invalid_or_trailing_data";
	case JCOP4_SM_DECODE_MAC:
		return "mac_verification_failed";
	case JCOP4_SM_DECODE_DECRYPT:
		return "decryption_failed";
	case JCOP4_SM_DECODE_PADDING:
		return "padding_invalid";
	case JCOP4_SM_DECODE_CAPACITY:
		return "plaintext_capacity_exceeded";
	case JCOP4_SM_DECODE_OK:
		return "accepted";
	case JCOP4_SM_DECODE_STATUS_MISMATCH:
		return "authenticated_status_outer_mismatch";
	}
	return "unknown_local_result";
}

static inline int
jcop4_sm_open_report_policy(struct jcop4_sm_channel *channel, const unsigned char *response,
		size_t size, unsigned char *plain, size_t capacity, size_t *written, unsigned int *sw,
		enum jcop4_sm_outer_policy policy, struct jcop4_sm_decode_report *report)
{
	return jcop4_diagnostic_sm_open_report_policy_ex(NULL, channel, response, size, plain, capacity, written, sw, policy, report);
}

static inline int
jcop4_sm_open_report(struct jcop4_sm_channel *channel, const unsigned char *response,
		size_t size, unsigned char *plain, size_t capacity, size_t *written, unsigned int *sw,
		struct jcop4_sm_decode_report *report)
{
	return jcop4_sm_open_report_policy(channel, response, size, plain, capacity, written, sw, JCOP4_SM_OUTER_9000, report);
}

static inline int
jcop4_sm_open(struct jcop4_sm_channel *channel, const unsigned char *response,
		size_t size, unsigned char *plain, size_t capacity, size_t *written, unsigned int *sw)
{
	return jcop4_sm_open_report(channel, response, size, plain, capacity, written, sw, NULL);
}

#endif
