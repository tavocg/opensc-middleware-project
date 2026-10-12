/* Replay the native signing SDO path and exercise the real PKCS#15 selector.
 * No hardware, PIN, MSE or signature is used.
 */
#include <stdio.h>
#include <string.h>

/* Isolate unused public definitions from this included source: otherwise
 * they override libopensc's exports and retain unrelated crypto paths.
 */
#define sc_pkcs15_decipher fixture_decipher
#define sc_pkcs15_derive fixture_derive
#define sc_pkcs15_unwrap fixture_unwrap
#define sc_pkcs15_wrap fixture_wrap
#define sc_pkcs15_compute_signature fixture_compute_signature
#define sc_pkcs15_encrypt_sym fixture_encrypt_sym
#define sc_pkcs15_decrypt_sym fixture_decrypt_sym
#include "libopensc/pkcs15-sec.c"
#undef sc_pkcs15_decipher
#undef sc_pkcs15_derive
#undef sc_pkcs15_unwrap
#undef sc_pkcs15_wrap
#undef sc_pkcs15_compute_signature
#undef sc_pkcs15_encrypt_sym
#undef sc_pkcs15_decrypt_sym
#include "libopensc/pkcs15-iasecc.c"

static int failures, selections;
static struct sc_path selected;

int sc_select_file(struct sc_card *card, const struct sc_path *path, struct sc_file **file)
{
	(void)card;
	(void)file;
	selected = *path;
	selections++;
	return SC_SUCCESS;
}

static void check(const char *name, int valid)
{
	if (valid) {
		printf("PASS %s\n", name);
	} else {
		fprintf(stderr, "FAIL %s\n", name);
		failures++;
	}
}

int main(void)
{
	const unsigned char aid[] = {
		0xE8,0x28,0xBD,0x08,0x0F,0xD2,0x50,0x47,0x65,0x6E,0x65,0x72,0x69,0x63
	};
	const char *names[] = {
		"signing SDO 3 selects only ChipDoc",
		"unprobed authentication key 2 keeps its path",
		"other private key paths stay unchanged",
		"signing path on another application stays unchanged",
		"signing path on another card stays unchanged",
		"non-native keys keep their paths",
		"other RSA key sizes keep their paths",
		"another path AID stays unchanged",
		"a path without an AID stays unchanged"
	};
	struct sc_context ctx = {0};
	struct sc_card card = {0};
	struct sc_app_info app = {0};
	struct sc_pkcs15_card p15card = {0};
	struct sc_pkcs15_tokeninfo tokeninfo = {0};
	struct sc_algorithm_info algorithms[3] = {0};
	struct sc_pkcs15_object obj = {0};
	struct sc_pkcs15_prkey_info info;
	struct sc_security_env env = {0};
	struct sc_path original, fixed;
	int result;

	ctx.app_name = "iasecc-key-path-regression";
	card.ctx = &ctx;
	p15card.card = &card;
	p15card.app = &app;
	p15card.obj_list = &obj;
	p15card.tokeninfo = &tokeninfo;
	card.algorithms = algorithms;
	card.algorithm_count = 3;
	algorithms[0].algorithm = algorithms[1].algorithm = SC_ALGORITHM_RSA;
	algorithms[0].key_length = 2048;
	algorithms[1].key_length = 1024;
	algorithms[2].algorithm = SC_ALGORITHM_EC;
	algorithms[2].key_length = 256;
	obj.type = SC_PKCS15_TYPE_PRKEY_RSA;
	obj.data = &info;
	for (int variant = 0; variant < 9; variant++) {
		card.type = SC_CARD_TYPE_IASECC_AMOS;
		app.aid.len = sizeof(aid);
		memcpy(app.aid.value, aid, sizeof(aid));
		memset(&info, 0, sizeof(info));
		info.native = 1;
		info.key_reference = 3;
		info.modulus_length = 2048;
		info.usage = SC_PKCS15_PRKEY_USAGE_SIGN;
		sc_format_path("0003", &info.path);
		info.path.aid = app.aid;
		switch (variant) {
		case 1: info.key_reference = info.path.value[1] = 2; break;
		case 2: info.path.value[1] = 4; break;
		case 3: app.aid.value[0] ^= 1; break;
		case 4: card.type = SC_CARD_TYPE_IASECC_GEMALTO; break;
		case 5: info.native = 0; break;
		case 6: info.modulus_length = 1024; break;
		case 7: info.path.aid.value[0] ^= 1; break;
		case 8: info.path.aid.len = 0; break;
		}
		original = info.path;
		_iasecc_fixup_signing_key_paths(&p15card);
		check(names[variant], variant ? !memcmp(&info.path, &original, sizeof(original)) :
				!info.path.len && info.path.aid.len == sizeof(aid) &&
				!memcmp(info.path.aid.value, aid, sizeof(aid)) && info.key_reference == 3);
		unsigned long caps = SC_ALGORITHM_RSA_PAD_PKCS1_TYPE_01 | SC_ALGORITHM_RSA_HASH_NONE |
				SC_ALGORITHM_RSA_HASH_SHA1 | SC_ALGORITHM_RSA_HASH_SHA256;
		for (int i = 0; i < 3; i++)
			algorithms[i].flags = caps;
		_iasecc_fixup_signature_algorithms(&p15card);
		check(variant ? "SHA-256 capabilities stay unchanged outside the observed signing profile" :
				"ChipDoc software SHA-256 preserves SHA-1, RSA-PKCS, other sizes and ECC",
				algorithms[0].flags == (variant ? caps : caps & ~SC_ALGORITHM_RSA_HASH_SHA256) &&
				algorithms[1].flags == caps && algorithms[2].flags == caps);
		if (!variant) {
			fixed = info.path;
			_iasecc_fixup_signing_key_paths(&p15card);
			check("signing path fixup is idempotent", !memcmp(&info.path, &fixed, sizeof(fixed)));
			result = select_key_file(&p15card, &obj, &env);
			check("PKCS#15 key selector selects the AID without file 0003", result == SC_SUCCESS &&
					selections == 1 && !selected.len && selected.aid.len == sizeof(aid) &&
					!memcmp(selected.aid.value, aid, sizeof(aid)) &&
					!(env.flags & SC_SEC_ENV_FILE_REF_PRESENT));
			for (int guard = 0; guard < 6; guard++) {
				info.algo_refs[0] = 0;
				tokeninfo.supported_algos[0].reference = 0;
				info.usage = SC_PKCS15_PRKEY_USAGE_SIGN;
				algorithms[0].flags = caps;
				switch (guard) {
				case 0: info.algo_refs[0] = 1; break;
				case 1: tokeninfo.supported_algos[0].reference = 1; break;
				case 2: info.usage = SC_PKCS15_PRKEY_USAGE_DECRYPT; break;
				case 3: algorithms[0].flags &= ~SC_ALGORITHM_RSA_HASH_NONE; break;
				case 4: algorithms[0].flags |= SC_ALGORITHM_RSA_RAW; break;
				case 5: p15card.tokeninfo = NULL; break;
				}
				unsigned long original_flags = algorithms[0].flags;
				_iasecc_fixup_signature_algorithms(&p15card);
				check("declared algorithms or unverified software-hash prerequisites are preserved",
						algorithms[0].flags == original_flags);
				p15card.tokeninfo = &tokeninfo;
			}
		}
	}
	return failures ? 1 : 0;
}
