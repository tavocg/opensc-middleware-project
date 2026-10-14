/* Offline RSA2048/SHA256/PKCS1-v1.5 verification for the bounded diagnostic.
 * Only public certificate material is loaded; no private key or PIN API. */
#ifndef JCOP4_SIGNATURE_H
#define JCOP4_SIGNATURE_H
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/rsa.h>
#include <openssl/rand.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

struct jcop4_signature {
	X509 *certificate; EVP_PKEY *key;
	unsigned char message[128], digest[32], signature[256]; size_t message_size;
	int directory_fd, verified;
};
static void jcop4_signature_clear(struct jcop4_signature *s)
{
	if (!s) return;
	X509_free(s->certificate); EVP_PKEY_free(s->key);
	if (s->directory_fd >= 0) close(s->directory_fd);
	OPENSSL_cleanse(s, sizeof(*s)); s->directory_fd = -1;
}
static int jcop4_signature_prepare(struct jcop4_signature *s, const char *certificate_path, const char *directory)
{
	unsigned char der[16384], nonce[16]; size_t size = 0; unsigned int digest_size = 0;
	FILE *file = NULL; ASN1_BIT_STRING *usage = NULL; int result = 1; struct stat st;
	if (!s) return 1;
	memset(s, 0, sizeof(*s)); s->directory_fd = -1;
	if (!certificate_path || !directory || !(file = fopen(certificate_path, "rb"))) goto out;
	size = fread(der, 1, sizeof(der), file);
	if (ferror(file) || !size || size == sizeof(der)) goto out;
	const unsigned char *cursor = der;
	s->certificate = d2i_X509(NULL, &cursor, (long)size);
	if (!s->certificate || cursor != der + size || !(s->key = X509_get_pubkey(s->certificate)) ||
		!EVP_PKEY_is_a(s->key, "RSA") || EVP_PKEY_get_bits(s->key) != 2048 || EVP_PKEY_get_size(s->key) != 256) goto out;
	usage = X509_get_ext_d2i(s->certificate, NID_key_usage, NULL, NULL);
	if (!usage || !(ASN1_BIT_STRING_get_bit(usage, 0) || ASN1_BIT_STRING_get_bit(usage, 1))) goto out;
	s->directory_fd = open(directory, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
	if (s->directory_fd < 0 || fstat(s->directory_fd, &st) || !S_ISDIR(st.st_mode) ||
		(st.st_mode & 0777) != 0700 || st.st_uid != getuid()) goto out;
	if (RAND_bytes(nonce, sizeof(nonce)) != 1) goto out;
	const char prefix[] = "libjcop3p11: prueba tecnica JCOP4 RSA2048 SHA256.\nNonce: ";
	memcpy(s->message, prefix, sizeof(prefix)-1); s->message_size = sizeof(prefix)-1;
	const char hex[] = "0123456789abcdef";
	for (size_t i = 0; i < sizeof(nonce); i++) { s->message[s->message_size++] = hex[nonce[i] >> 4]; s->message[s->message_size++] = hex[nonce[i] & 15]; }
	s->message[s->message_size++] = '\n';
	if (EVP_Digest(s->message, s->message_size, s->digest, &digest_size, EVP_sha256(), NULL) != 1 || digest_size != 32) goto out;
	result = 0;
out:
	if (file) fclose(file);
	ASN1_BIT_STRING_free(usage); OPENSSL_cleanse(der, sizeof(der)); OPENSSL_cleanse(nonce, sizeof(nonce));
	if (result) jcop4_signature_clear(s);
	return result;
}
static int jcop4_signature_verify(struct jcop4_signature *s, const unsigned char *signature, size_t size)
{
	if (!s) return 1;
	EVP_MD_CTX *ctx = EVP_MD_CTX_new(); EVP_PKEY_CTX *pctx = NULL; int result = 1;
	s->verified = 0; OPENSSL_cleanse(s->signature, sizeof(s->signature));
	if (!ctx || !s->key || !signature || size != 256 || !s->message_size ||
		EVP_DigestVerifyInit(ctx, &pctx, EVP_sha256(), NULL, s->key) != 1 ||
		EVP_PKEY_CTX_set_rsa_padding(pctx, RSA_PKCS1_PADDING) <= 0 ||
		EVP_DigestVerify(ctx, signature, size, s->message, s->message_size) != 1) goto out;
	memcpy(s->signature, signature, 256); s->verified = 1; result = 0;
out:
	EVP_MD_CTX_free(ctx); return result;
}
static int jcop4_signature_save(struct jcop4_signature *s)
{
	if (!s) return 1;
	const char *names[] = {"message.txt", "signature.bin", "certificate.der"};
	unsigned char *der = NULL; int der_size = 0, created[3] = {0}, result = 1;
	if (!s->verified || s->directory_fd < 0 || (der_size = i2d_X509(s->certificate, &der)) <= 0) goto out;
	const unsigned char *data[] = {s->message, s->signature, der};
	size_t sizes[] = {s->message_size, sizeof(s->signature), (size_t)der_size};
	for (int i = 0; i < 3; i++) {
		int fd = openat(s->directory_fd, names[i], O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
		if (fd < 0) goto out;
		created[i] = 1; size_t offset = 0; int failed = 0;
		while (offset < sizes[i]) { ssize_t n = write(fd, data[i] + offset, sizes[i] - offset); if (n <= 0) { failed = 1; break; } offset += (size_t)n; }
		if (close(fd)) failed = 1;
		if (failed) goto out;
	}
	result = 0;
out:
	if (result && s->directory_fd >= 0) for (int i = 0; i < 3; i++) if (created[i]) unlinkat(s->directory_fd, names[i], 0);
	OPENSSL_free(der); return result;
}
#endif
