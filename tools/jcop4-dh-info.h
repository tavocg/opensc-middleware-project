/* Diagnostic only: a bounded public-parameter read, never a channel setup. */
#include <openssl/ec.h>
#include <openssl/obj_mac.h>

static int d004_control_matches_observation(const unsigned char *buf, size_t size)
{
	const unsigned char *p = buf, *value;
	size_t len, left;
	unsigned int cla, tag;
	int descriptor = 0, fid = 0, length = 0;
	if (!buf || !size || size > 256 ||
			sc_asn1_read_tag(&p, size, &cla, &tag, &len) < 0 || !p ||
			cla != 0x60 || (tag != 2 && tag != 15) || (size_t)(p - buf) + len != size)
		return 0;
	left = len;
	while (left) {
		size_t consumed;
		value = p;
		if (sc_asn1_read_tag(&value, left, &cla, &tag, &len) < 0 || !value ||
				(size_t)(value - p) > left || len > left - (size_t)(value - p)) return 0;
		consumed = (size_t)(value - p) + len;
		switch (cla | tag) {
		case 0x82:
			if (descriptor++ || !len || (value[0] & 0x80) ||
					((value[0] & 0x3F) != 1 && (value[0] & 0x3F) != 9)) return 0;
			break;
		case 0x83:
			if (fid++ || len != 2 || value[0] != 0xD0 || value[1] != 4) return 0;
			break;
		case 0x80: {
			size_t i, candidate = 0;
			if (length++ || !len || len > 4) return 0;
			for (i = 0; i < len; i++) candidate = (candidate << 8) | value[i];
			if (candidate != 137) return 0;
			break;
		}
		/* Other control fields, including security attributes, stay opaque.
		 * This match checks identity/size, not a grant derived from an ACL. */
		}
		left -= consumed;
		p = value + len;
	}
	return descriptor == 1 && fid == 1 && length == 1;
}

static int summarize_d004_parameters(const unsigned char *buf, size_t size)
{
	EC_GROUP *group = NULL;
	EC_POINT *point = NULL;
	int valid = 0;
	/* Installed Idopte parser: discriminator at2; coordinates at5.
	 * Its discriminator18 selects OpenSSL NID716 (secp521r1).
	 * Remaining header fields have no asserted semantics here. */
	if (!buf || size != 137 || buf[2] != 18 || buf[4] != 4) {
		printf("dh_parameters_result=unrecognized_layout_stop\n");
		return 1;
	}
	group = EC_GROUP_new_by_curve_name(NID_secp521r1);
	if (group) point = EC_POINT_new(group);
	if (point && EC_POINT_oct2point(group, point, buf + 4, 133, NULL) == 1 &&
			EC_POINT_is_at_infinity(group, point) == 0 &&
			EC_POINT_is_on_curve(group, point, NULL) == 1) valid = 1;
	EC_POINT_clear_free(point);
	EC_GROUP_free(group);
	if (!valid) {
		printf("dh_parameters_result=point_validation_failed_stop\n");
		return 1;
	}
	printf("dh_parameters_curve=secp521r1\ndh_parameters_point=valid_non_infinity\n"
			"dh_parameters_result=recognized_point_only_no_channel\n");
	return 0;
}

static int inspect_d004_parameters(sc_card_t *card, const unsigned char *control, size_t control_size)
{
	unsigned char block[137] = {0};
	sc_apdu_t apdu;
	int rv, result = 1;
	if (!d004_control_matches_observation(control, control_size)) {
		printf("dh_control_result=unexpected_metadata_no_read\n");
		return 1;
	}
	printf("dh_control_result=observed_identity_structure_size\n");
	/* Exactly one basic-channel READ, no SFI, input, continuation or retries. */
	sc_format_apdu(card, &apdu, SC_APDU_CASE_2_SHORT, 0xB0, 0, 0);
	apdu.cla = 0;
	apdu.flags |= SC_APDU_FLAGS_NO_RETRY_WL | SC_APDU_FLAGS_NO_GET_RESP;
	apdu.resp = block;
	apdu.le = apdu.resplen = sizeof(block);
	rv = sc_transmit_apdu(card, &apdu);
	printf("dh_read_transport=%d\ndh_read_sw=%02X%02X\n", rv, apdu.sw1, apdu.sw2);
	if (rv >= 0 && apdu.sw1 == 0x90 && apdu.sw2 == 0 && apdu.resplen == sizeof(block))
		result = summarize_d004_parameters(block, sizeof(block));
	else printf("dh_parameters_result=read_failed_or_incomplete_stop\n");
	sc_mem_clear(block, sizeof(block));
	return result;
}
