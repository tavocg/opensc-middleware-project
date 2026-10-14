/* Synthetic PIN, RSA key/certificate and software reader. No hardware. */
#include "libopensc/opensc.h"
static int test_signature_reply(sc_apdu_t *a);
#define JCOP4_SIGN_DIAGNOSTIC
#define JCOP4_SIGN_FIXTURE
#include "jcop4-channel-login.c"

static EVP_PKEY *rsa_fixture;
static unsigned char protected_response[293]; static size_t response_offset;
static int sign_mode_fixture, signature_mse_calls, signature_pso_calls, signature_get_calls;
static int application_mode_fixture, application_error, signature_application_calls;
static char certificate_path[512], directory_path[512];

static void cipher_iv(unsigned int counter, unsigned char iv[16])
{
	unsigned char ssc[16] = {0}; ssc[15] = (unsigned char)counter; int n, last;
	EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new(); assert(ctx);
	assert(EVP_EncryptInit_ex(ctx, EVP_aes_128_ecb(), NULL, test_flow.derived.keys.encryption, NULL) == 1);
	assert(EVP_CIPHER_CTX_set_padding(ctx, 0) == 1 && EVP_EncryptUpdate(ctx, iv, &n, ssc, 16) == 1);
	assert(EVP_EncryptFinal_ex(ctx, iv + n, &last) == 1 && n + last == 16); EVP_CIPHER_CTX_free(ctx);
}
static void check_command(sc_apdu_t *a, int pso, unsigned char digest[32])
{
	unsigned char iv[16], plain[48], auth[96] = {0}, mac[16]; int n, last;
	size_t cipher_size = pso ? 48 : 16, auth_size = pso ? 96 : 64, before_mac = pso ? 54 : 19;
	assert(a->cla == 0x0C && a->lc == (pso ? 64u : 29u));
	assert(a->ins == (pso ? 0x2A : 0x22) && a->p1 == (pso ? 0x9E : 0x81) && a->p2 == (pso ? 0x9A : 0xB6));
	cipher_iv((pso ? 11 : 9) + 2 * application_mode_fixture, iv); EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new(); assert(ctx);
	assert(EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, test_flow.derived.keys.encryption, iv) == 1);
	assert(EVP_CIPHER_CTX_set_padding(ctx, 0) == 1 && EVP_DecryptUpdate(ctx, plain, &n, a->data + 3, (int)cipher_size) == 1);
	assert(EVP_DecryptFinal_ex(ctx, plain + n, &last) == 1 && n + last == (int)cipher_size); EVP_CIPHER_CTX_free(ctx);
	if (pso) { memcpy(digest, plain, 32); assert(!memcmp(plain, probe_signature.digest, 32)); }
	else assert(!memcmp(plain, "\x91\x02\x28\x91", 4));
	assert(plain[pso ? 32 : 4] == 0x80); for (size_t i = pso ? 33 : 5; i < cipher_size; i++) assert(!plain[i]);
	auth[15] = (pso ? 11 : 9) + 2 * application_mode_fixture; auth[16] = 0x0C; auth[17] = a->ins; auth[18] = a->p1; auth[19] = a->p2; auth[20] = 0x80;
	memcpy(auth + 32, a->data, before_mac); auth[32 + before_mac] = 0x80;
	cmac(test_flow.derived.keys.mac, auth, auth_size, mac); assert(!memcmp(mac, a->data + before_mac + 2, 8));
	OPENSSL_cleanse(plain, sizeof(plain)); OPENSSL_cleanse(mac, sizeof(mac)); OPENSSL_cleanse(iv, sizeof(iv));
}
static int test_signature_reply(sc_apdu_t *a)
{
	if (probe_guard.phase == PROBE_SIGNATURE_APP) {
		unsigned char iv[16], plain[32], auth[64] = {0}, mac[16]; int n, last;
		assert(++signature_application_calls == 1 && application_mode_fixture && verifies == 1 && !signature_mse_calls);
		assert(a->cla == 0x0C && a->ins == 0xA4 && a->p1 == 4 && a->p2 == 0x0C && a->lc == 29 && a->le == 256);
		cipher_iv(9, iv); EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new(); assert(ctx);
		assert(EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, test_flow.derived.keys.encryption, iv) == 1);
		assert(EVP_CIPHER_CTX_set_padding(ctx, 0) == 1 && EVP_DecryptUpdate(ctx, plain, &n, a->data + 3, 16) == 1);
		assert(EVP_DecryptFinal_ex(ctx, plain + n, &last) == 1 && n + last == 16); EVP_CIPHER_CTX_free(ctx);
		assert(!memcmp(plain, probe_aid, sizeof(probe_aid)) && plain[15] == 0x80);
		auth[15] = 9; memcpy(auth + 16, "\x0c\xa4\x04\x0c\x80", 5); memcpy(auth + 32, a->data, 19); auth[51] = 0x80;
		cmac(test_flow.derived.keys.mac, auth, 64, mac); assert(!memcmp(mac, a->data + 21, 8));
		if (application_error == 1) return -7;
		memset(auth, 0, sizeof(auth)); auth[15] = application_error == 4 ? 8 : 10;
		unsigned int sw = application_error == 3 ? 0x6982 : 0x9000;
		auth[16] = 0x99; auth[17] = 2; auth[18] = sw >> 8; auth[19] = sw & 255; auth[20] = 0x80;
		cmac(test_flow.derived.keys.mac, auth, 32, mac); if (application_error == 2) mac[0] ^= 1;
		memcpy(a->resp, auth + 16, 4); memcpy(a->resp + 4, "\x8e\x08", 2); memcpy(a->resp + 6, mac, 8); a->resplen = 14;
		if (application_error == 5) {
			/* A valid MAC with unexpected application response data must stop. */
			memset(plain, 0, sizeof(plain)); plain[0] = 0xA5; plain[1] = 0x80; cipher_iv(10, iv);
			EVP_CIPHER_CTX *enc = EVP_CIPHER_CTX_new(); assert(enc);
			memcpy(a->resp, "\x87\x11\x01", 3);
			assert(EVP_EncryptInit_ex(enc, EVP_aes_128_cbc(), NULL, test_flow.derived.keys.encryption, iv) == 1);
			assert(EVP_CIPHER_CTX_set_padding(enc, 0) == 1 && EVP_EncryptUpdate(enc, a->resp + 3, &n, plain, 16) == 1);
			assert(EVP_EncryptFinal_ex(enc, a->resp + 3 + n, &last) == 1 && n + last == 16); EVP_CIPHER_CTX_free(enc);
			memcpy(a->resp + 19, "\x99\x02\x90\0", 4); memset(auth, 0, sizeof(auth)); auth[15] = 10;
			memcpy(auth + 16, a->resp, 23); auth[39] = 0x80; cmac(test_flow.derived.keys.mac, auth, 48, mac);
			memcpy(a->resp + 23, "\x8e\x08", 2); memcpy(a->resp + 25, mac, 8); a->resplen = 33;
		}
		if (application_error == 6) a->resplen = 0;
		if (application_error == 7) { a->sw1 = 0x61; a->sw2 = 0; }
		OPENSSL_cleanse(plain, sizeof(plain)); OPENSSL_cleanse(iv, sizeof(iv)); OPENSSL_cleanse(mac, sizeof(mac)); return 0;
	}
	if (probe_guard.phase == PROBE_SIGNATURE_MSE) {
		unsigned char digest[32], auth[32] = {0}, mac[16]; assert(++signature_mse_calls == 1 && verifies == 1);
		check_command(a, 0, digest); if (sign_mode_fixture == 5) return -7;
		unsigned int sw = sign_mode_fixture == 4 ? 0x6982 : 0x9000;
		auth[15] = 10 + 2 * application_mode_fixture; auth[16] = 0x99; auth[17] = 2; auth[18] = sw >> 8; auth[19] = sw & 255; auth[20] = 0x80;
		cmac(test_flow.derived.keys.mac, auth, 32, mac); if (sign_mode_fixture == 6) mac[0] ^= 1;
		memcpy(a->resp, auth + 16, 4); a->resp[4] = 0x8E; a->resp[5] = 8; memcpy(a->resp + 6, mac, 8); a->resplen = 14;
		return 0;
	}
	if (probe_guard.phase == PROBE_SIGNATURE_PSO) {
		unsigned char digest[32], signature[256], padded[272] = {0}, iv[16], auth[304] = {0}, mac[16];
		assert(++signature_pso_calls == 1 && signature_mse_calls == 1); check_command(a, 1, digest);
		if (sign_mode_fixture == 7) return -7;
		if (sign_mode_fixture >= 12) {
			/* Independent status-only response, including the observed shape.
			 * No real card data or MAC is used by this software fixture. */
			unsigned int sw = sign_mode_fixture == 14 ? 0x9000 : 0x6A82;
			auth[15] = (sign_mode_fixture == 15 ? 10 : 12) + 2 * application_mode_fixture;
			auth[16] = 0x99; auth[17] = 2; auth[18] = sw >> 8; auth[19] = sw & 255; auth[20] = 0x80;
			cmac(test_flow.derived.keys.mac, auth, 32, mac);
			if (sign_mode_fixture == 13) mac[0] ^= 1;
			if (sign_mode_fixture >= 100) mac[(sign_mode_fixture - 100) / 8] ^= 1 << ((sign_mode_fixture - 100) % 8);
			memcpy(a->resp, auth + 16, 4); memcpy(a->resp + 4, "\x8e\x08", 2); memcpy(a->resp + 6, mac, 8); a->resplen = 14;
			a->sw1 = 0x6A; a->sw2 = sign_mode_fixture == 18 ? 0x83 : 0x82;
			if (sign_mode_fixture == 16) a->resplen = 0;
			if (sign_mode_fixture == 17) a->resp[0] = 0x85;
			if (sign_mode_fixture == 19) { a->sw1 = 0x63; a->sw2 = 0xC7; }
			if (sign_mode_fixture == 20) a->resplen = 13;
			OPENSSL_cleanse(mac, sizeof(mac)); return 0;
		}
		EVP_PKEY_CTX *rsa = EVP_PKEY_CTX_new(rsa_fixture, NULL); assert(rsa); size_t size = 256;
		assert(EVP_PKEY_sign_init(rsa) == 1 && EVP_PKEY_CTX_set_rsa_padding(rsa, RSA_PKCS1_PADDING) > 0);
		assert(EVP_PKEY_CTX_set_signature_md(rsa, EVP_sha256()) > 0 && EVP_PKEY_sign(rsa, signature, &size, digest, 32) == 1 && size == 256);
		EVP_PKEY_CTX_free(rsa); if (sign_mode_fixture == 8) signature[0] ^= 1;
		memcpy(padded, signature, 256); padded[256] = 0x80; cipher_iv(12 + 2 * application_mode_fixture, iv);
		memcpy(protected_response, "\x87\x82\x01\x11\x01", 5);
		EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new(); assert(ctx); int n, last;
		assert(EVP_EncryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, test_flow.derived.keys.encryption, iv) == 1);
		assert(EVP_CIPHER_CTX_set_padding(ctx, 0) == 1 && EVP_EncryptUpdate(ctx, protected_response + 5, &n, padded, 272) == 1);
		assert(EVP_EncryptFinal_ex(ctx, protected_response + 5 + n, &last) == 1 && n + last == 272); EVP_CIPHER_CTX_free(ctx);
		memcpy(protected_response + 277, "\x99\x02\x90\0", 4); auth[15] = (sign_mode_fixture == 9 ? 10 : 12) + 2 * application_mode_fixture;
		memcpy(auth + 16, protected_response, 281); auth[297] = 0x80; cmac(test_flow.derived.keys.mac, auth, 304, mac);
		if (sign_mode_fixture == 10) mac[0] ^= 1;
		memcpy(protected_response + 281, "\x8e\x08", 2); memcpy(protected_response + 283, mac, 8); protected_response[291] = 0x90; protected_response[292] = 0;
		OPENSSL_cleanse(signature, sizeof(signature)); OPENSSL_cleanse(padded, sizeof(padded)); OPENSSL_cleanse(iv, sizeof(iv));
		if (sign_mode_fixture == 0) { assert(a->resplen == 291); memcpy(a->resp, protected_response, 291); a->resplen = 291; return 0; }
		response_offset = sign_mode_fixture == 2 ? 256 : 0;
		memcpy(a->resp, protected_response, response_offset); a->resplen = response_offset; a->sw1 = 0x61; a->sw2 = response_offset ? 35 : 0;
		return 0;
	}
	assert(probe_guard.phase == PROBE_SIGNATURE_GET && !a->cla && a->ins == 0xC0 && !a->lc);
	signature_get_calls++; if (sign_mode_fixture == 11) return -7;
	size_t remaining = 291 - response_offset, take = remaining > 256 ? 256 : remaining;
	assert(a->le == take && a->resplen == take && signature_get_calls <= 3);
	if (sign_mode_fixture == 3 && take > 100) take = 100;
	memcpy(a->resp, protected_response + response_offset, take); response_offset += take; a->resplen = take;
	a->sw1 = response_offset == 291 ? 0x90 : 0x61; a->sw2 = response_offset == 291 ? 0 : (291 - response_offset >= 256 ? 0 : 291 - response_offset);
	return 0;
}
static void certificate_fixture(void)
{
	EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL); assert(ctx);
	assert(EVP_PKEY_keygen_init(ctx) == 1 && EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048) > 0 && EVP_PKEY_keygen(ctx, &rsa_fixture) == 1); EVP_PKEY_CTX_free(ctx);
	X509 *cert = X509_new(); assert(cert); assert(X509_set_version(cert, 2) && ASN1_INTEGER_set(X509_get_serialNumber(cert), 1));
	assert(X509_gmtime_adj(X509_getm_notBefore(cert), -60) && X509_gmtime_adj(X509_getm_notAfter(cert), 3600));
	assert(X509_set_pubkey(cert, rsa_fixture)); X509_NAME *name = X509_get_subject_name(cert);
	assert(X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (unsigned char *)"Synthetic fixture", -1, -1, 0));
	assert(X509_set_issuer_name(cert, name)); X509_EXTENSION *ext = X509V3_EXT_conf_nid(NULL, NULL, NID_key_usage, "critical,digitalSignature");
	assert(ext && X509_add_ext(cert, ext, -1)); X509_EXTENSION_free(ext); assert(X509_sign(cert, rsa_fixture, EVP_sha256()) > 0);
	FILE *file = fopen(certificate_path, "wb"); assert(file && i2d_X509_fp(file, cert) == 1 && fclose(file) == 0); X509_free(cert);
}
static int invoke_sign(void)
{
	char *args[] = {"jcop4-channel-sign", "0", application_mode_fixture ? "--sign-application-once" : "--sign-once", certificate_path, directory_path};
	int saved = dup(STDOUT_FILENO); FILE *capture = tmpfile(); assert(saved >= 0 && capture); fflush(stdout);
	assert(dup2(fileno(capture), STDOUT_FILENO) >= 0); int rv = channel_probe_main(5, args); fflush(stdout); rewind(capture);
	size_t size = fread(output, 1, sizeof(output)-1, capture); output[size] = 0;
	assert(!strstr(output, "777777") && !strstr(output, "Synthetic fixture") && !strstr(output, "Sending:"));
	assert(dup2(saved, STDOUT_FILENO) >= 0); close(saved); fclose(capture);
	assert(!test_lock_depth);
	if (test_state.connects) assert(test_state.resets == 2 && test_state.unlocks == 1 && test_state.disconnects == 1);
	return rv;
}
static void sign_defaults(void)
{
	login_defaults(); signature_mse_calls = signature_pso_calls = signature_get_calls = 0;
	application_mode_fixture = application_error = signature_application_calls = 0;
}
static void remove_outputs(void)
{
	const char *names[] = {"message.txt","signature.bin","certificate.der"}; char path[768];
	for (int i = 0; i < 3; i++) { snprintf(path, sizeof(path), "%s/%s", directory_path, names[i]); unlink(path); }
}
int main(void)
{
	char root[] = "/tmp/jcop4-signature-fixture-XXXXXX"; assert(mkdtemp(root));
	snprintf(certificate_path, sizeof(certificate_path), "%s/public.der", root); snprintf(directory_path, sizeof(directory_path), "%s/output", root);
	assert(mkdir(directory_path, 0700) == 0); certificate_fixture();
	init(&test_flow); memcpy(test_initial, test_flow.initial, 137); setenv("OPENSC_CONF", "/tmp/synthetic", 1); setenv("OPENSC_DEBUG", "0", 1);
	for (int mode = 0; mode <= 3; mode++) {
		sign_defaults(); sign_mode_fixture = mode;
		assert(invoke_sign() == 0 && verifies == 1 && signature_mse_calls == 1 && signature_pso_calls == 1);
		assert(strstr(output, "signature_result=rsa_sha256_verified_and_saved") && strstr(output, "signature_pso_mac_check=verified"));
		assert(strstr(output, mode == 0 ? "signature_first_size=293" : mode == 2 ? "signature_first_size=258" : "signature_first_size=2"));
		assert(signature_get_calls == (mode == 0 ? 0 : mode == 1 ? 2 : mode == 2 ? 1 : 3));
		struct stat st; char path[768]; snprintf(path, sizeof(path), "%s/signature.bin", directory_path);
		assert(stat(path, &st) == 0 && st.st_size == 256 && (st.st_mode & 0777) == 0600); remove_outputs();
	}
	puts("PASS software RSA2048 fixture verifies signature after authenticated MSE/PSO and direct293-byte or bounded0/1/2/3-GET physical responses, one PIN, complete closure before save");
	for (int mode = 4; mode <= 11; mode++) {
		sign_defaults(); sign_mode_fixture = mode;
		assert(invoke_sign() == 1 && verifies == 1 && signature_mse_calls == 1);
		assert(signature_pso_calls == (mode <= 6 ? 0 : 1) && strstr(output, "signature_result=not_saved_stop"));
		char path[768]; snprintf(path, sizeof(path), "%s/signature.bin", directory_path); assert(access(path, F_OK) != 0);
		if (mode == 8) assert(strstr(output, "signature_verification=invalid_stop"));
	}
	puts("PASS MSE/PSO/GET transport, authenticated error, bad MAC/replay and valid-MAC non-RSA bytes never save artifacts or retry PIN/signature");
	for (int mode = 12; mode <= 20; mode++) {
		sign_defaults(); sign_mode_fixture = mode;
		assert(invoke_sign() == 1 && verifies == 1 && signature_mse_calls == 1 && signature_pso_calls == 1 && !signature_get_calls);
		assert(strstr(output, "signature_result=not_saved_stop") && !strstr(output, "signature_verification="));
		char path[768]; snprintf(path, sizeof(path), "%s/signature.bin", directory_path); assert(access(path, F_OK) != 0);
		if (mode == 12) assert(strstr(output, "signature_pso_mac_check=verified") && strstr(output, "signature_pso_sw=6A82"));
		else assert(!strstr(output, "signature_pso_sw="));
		if (mode == 14) assert(strstr(output, "signature_pso_mac_check=verified") && strstr(output, "authenticated_status_outer_mismatch"));
	}
	for (int bit = 0; bit < 64; bit++) {
		sign_defaults(); sign_mode_fixture = 100 + bit;
		assert(invoke_sign() == 1 && verifies == 1 && signature_pso_calls == 1 && !signature_get_calls);
		assert(strstr(output, "signature_pso_mac_check=failed") && !strstr(output, "signature_pso_sw="));
	}
	puts("PASS PSO status-only6A82 is authenticated once and stops without GET/retry/artifacts; all MAC bits, status mismatch, replay, bare/unknown/truncated envelopes fail closed");
	for (int mode = 0; mode <= 3; mode++) {
		sign_defaults(); application_mode_fixture = 1; sign_mode_fixture = mode;
		assert(invoke_sign() == 0 && verifies == 1 && signature_application_calls == 1 && signature_mse_calls == 1 && signature_pso_calls == 1);
		assert(strstr(output, "signature_application_mac_check=verified") && strstr(output, "signature_application_sw=9000"));
		assert(strstr(output, "signature_result=rsa_sha256_verified_and_saved")); remove_outputs();
	}
	for (int error = 1; error <= 7; error++) {
		sign_defaults(); application_mode_fixture = 1; application_error = error; sign_mode_fixture = 0;
		assert(invoke_sign() == 1 && verifies == 1 && signature_application_calls == 1 && !signature_mse_calls && !signature_pso_calls && !signature_get_calls);
		assert(strstr(output, "signature_application_result=not_confirmed_stop") && strstr(output, "signature_result=not_saved_stop"));
	}
	sign_defaults(); application_mode_fixture = 1; sign_mode_fixture = 12;
	assert(invoke_sign() == 1 && verifies == 1 && signature_application_calls == 1 && signature_pso_calls == 1 && !signature_get_calls);
	assert(strstr(output, "signature_pso_mac_check=verified") && strstr(output, "signature_pso_sw=6A82") && strstr(output, "signature_result=not_saved_stop"));
	sign_defaults(); application_mode_fixture = 1; reply_error = 3;
	assert(invoke_sign() == 1 && verifies == 1 && !signature_application_calls && !signature_mse_calls);
	puts("PASS explicit application selection uses only the announced AID at SSC9/10 after login; RSA MSE/PSO shift to11..14, all selection errors stop before MSE and no login retry follows PSO errors");
	sign_defaults(); sign_mode_fixture = 0; test_state.reset_error = 2;
	assert(invoke_sign() == 1 && signature_pso_calls == 1 && strstr(output, "signature_verification=rsa_sha256_pkcs1_verified"));
	char path[768]; snprintf(path, sizeof(path), "%s/signature.bin", directory_path); assert(access(path, F_OK) != 0);
	puts("PASS a mathematically valid signature is not saved if the remote session's final reset fails");
	sign_defaults(); reply_error = 3; assert(invoke_sign() == 1 && verifies == 1 && !signature_mse_calls);
	sign_defaults(); test_state.context_fail = 5; assert(invoke_sign() == 1 && !pin_reads && !signature_mse_calls);
	assert(chmod(directory_path, 0755) == 0); sign_defaults(); assert(invoke_sign() == 1 && !test_state.connects && !pin_reads); assert(chmod(directory_path, 0700) == 0);
	struct stat cert_stat; assert(stat(certificate_path, &cert_stat) == 0);
	FILE *malformed = fopen(certificate_path, "ab"); assert(malformed && fputc(0, malformed) == 0 && fclose(malformed) == 0);
	sign_defaults(); assert(invoke_sign() == 1 && !test_state.connects && !pin_reads);
	assert(truncate(certificate_path, cert_stat.st_size) == 0);
	puts("PASS unverified login/context or nonprivate output directory prevent signing and invalid certificate setup cannot prompt for a PIN");
	sign_defaults(); sign_mode_fixture = 0; FILE *file = fopen(path, "wb"); assert(file && fputs("EXISTING", file) >= 0 && fclose(file) == 0);
	assert(invoke_sign() == 1); file = fopen(path, "rb"); char kept[9] = {0}; assert(file && fread(kept, 1, 8, file) == 8 && fclose(file) == 0 && !strcmp(kept, "EXISTING")); remove_outputs();
	puts("PASS artifact writes are exclusive and roll back only files created by this run, preserving an existing file");
	{
		for (int stage = 0; stage < 3; stage++) for (int mutation = 0; mutation < 8; mutation++) {
			sign_defaults(); test_state.locks = 1; memset(&probe_guard, 0, sizeof(probe_guard));
			probe_guard.reader = &test_reader; probe_guard.original = &test_ops; probe_guard.sign_mode = 1;
			probe_guard.login_authenticated = 1; probe_guard.signature_mse_verified = 1; probe_guard.signature_pending_sw = 0x6123;
			probe_guard.phase = stage == 0 ? PROBE_SIGNATURE_MSE : stage == 1 ? PROBE_SIGNATURE_PSO : PROBE_SIGNATURE_GET;
			unsigned char wire[70] = {0}, reply[293]; size_t size = stage == 0 ? 35 : stage == 1 ? 70 : 5;
			memcpy(wire, stage == 0 ? "\x0c\x22\x81\xb6\x1d" : stage == 1 ? "\x0c\x2a\x9e\x9a\x40" : "\0\xc0\0\0\x23", 5);
			if (stage < 2) {
				memcpy(wire + 5, stage ? "\x87\x31\x01" : "\x87\x11\x01", 3);
				memcpy(wire + (stage ? 56 : 24), stage ? "\x97\x01\0\x8e\x08" : "\x8e\x08", stage ? 5 : 2);
			}
			memcpy(probe_guard.expected, wire, size); probe_guard.expected_size = size;
			sc_apdu_t a; sc_format_apdu(&test_card, &a, stage == 2 ? SC_APDU_CASE_2_SHORT : SC_APDU_CASE_4_SHORT, wire[1], wire[2], wire[3]);
			a.cla = wire[0]; a.le = stage == 2 ? 35 : 256; a.resp = reply; a.resplen = sizeof(reply);
			if (stage < 2) { a.data = wire + 5; a.lc = a.datalen = wire[4]; }
			if (mutation == 0) a.cla = stage == 2 ? 0x0C : 0;
			if (mutation == 1) a.ins = 0x24;
			if (mutation == 2) a.le = 172;
			if (mutation == 3) probe_guard.sign_mode = 0;
			if (mutation == 4) probe_guard.login_authenticated = 0;
			if (mutation == 5) probe_guard.called = 1;
			if (mutation == 6) { if (stage < 2) wire[8] ^= 1; else probe_guard.signature_pending_sw = 0x9000; }
			if (mutation == 7) { if (!stage) a.p2 = 0xA4; else probe_guard.signature_mse_verified = 0; }
			assert(probe_guard_transmit(&test_reader, &a) < 0 && probe_guard.blocked && !test_state.physical);
		}
	}
	puts("PASS signature guard rejects plaintext/changed bytes/headers/Le, missing login/MSE proof, unsolicited GET RESPONSE and repeated commands before backend");
	for (int mutation = 0; mutation < 10; mutation++) {
		sign_defaults(); test_state.locks = 1; memset(&probe_guard, 0, sizeof(probe_guard));
		probe_guard.reader = &test_reader; probe_guard.original = &test_ops; probe_guard.sign_mode = 1;
		probe_guard.signature_application_mode = 1; probe_guard.login_authenticated = 1; probe_guard.phase = PROBE_SIGNATURE_APP;
		unsigned char wire[35] = {0}, reply[258]; memcpy(wire, "\x0c\xa4\x04\x0c\x1d\x87\x11\x01", 8); memcpy(wire + 24, "\x8e\x08", 2);
		memcpy(probe_guard.expected, wire, 35); probe_guard.expected_size = 35;
		sc_apdu_t a; sc_format_apdu(&test_card, &a, SC_APDU_CASE_4_SHORT, 0xA4, 4, 0x0C);
		a.cla = 0x0C; a.le = 256; a.resp = reply; a.resplen = sizeof(reply); a.data = wire + 5; a.lc = a.datalen = 29;
		if (mutation == 0) probe_guard.signature_application_mode = 0;
		if (mutation == 1) probe_guard.login_authenticated = 0;
		if (mutation == 2) a.cla = 0;
		if (mutation == 3) a.p1 = 0;
		if (mutation == 4) a.p2 = 0;
		if (mutation == 5) a.le = 172;
		if (mutation == 6) wire[8] ^= 1;
		if (mutation == 7) probe_guard.called = 1;
		if (mutation == 8) probe_guard.signature_application_verified = 1;
		if (mutation == 9) {
			probe_guard.phase = PROBE_SIGNATURE_MSE; a.ins = 0x22; a.p1 = 0x81; a.p2 = 0xB6;
			memcpy(wire, "\x0c\x22\x81\xb6\x1d", 5); memcpy(probe_guard.expected, wire, 35);
		}
		assert(probe_guard_transmit(&test_reader, &a) < 0 && probe_guard.blocked && !test_state.physical);
	}
	puts("PASS application guard rejects missing opt-in/login, plaintext/changed ciphertext/P1/P2/Le, repeated SELECT and MSE without confirmed application before backend");
	unlink(certificate_path); rmdir(directory_path); rmdir(root); jcop4_ca_pending_clear(&test_flow.derived); EVP_PKEY_free(test_flow.peer); EVP_PKEY_free(rsa_fixture); return 0;
}
