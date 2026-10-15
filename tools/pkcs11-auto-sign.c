/* One PKCS#11 login/signature using normal driver autodetection.
 * No configuration, APDU hooks, model selection or mechanism fallback.
 * Certificate, ID, PIN, digest and signature remain in process memory. */
#include "pkcs11/pkcs11.h"
#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#ifndef AUTO_MODULE_PATH
#error Pin the locally built module at compile time
#endif
static volatile sig_atomic_t interrupted;
static const unsigned char message[] = "libjcop3p11: prueba tecnica de firma con autodeteccion RSA SHA-256.\n";
static FILE *report;
static void stop(int sig) { (void)sig; interrupted = 1; }
static int result(const char *name, CK_RV rv)
{
    fprintf(report, "%s=%lu\n", name, rv);
    return rv != CKR_OK || interrupted;
}
static int read_pin(unsigned char pin[16], CK_ULONG *size)
{
    int fd = open("/dev/tty", O_RDWR | O_CLOEXEC), failed = 1;
    struct termios saved, hidden;
    ssize_t n = -1;
    *size = 0;
    if (fd < 0) return 1;
    if (!isatty(fd) || tcgetattr(fd, &saved) || !(saved.c_lflag & ICANON)) goto done;
    hidden = saved;
    hidden.c_lflag &= ~(ECHO | ECHONL);
    if (tcsetattr(fd, TCSAFLUSH, &hidden)) goto done;
    const char notice[] = "\nSe firmara una sola vez este texto tecnico:\n";
    const char prompt[] = "PIN (sin eco; un solo intento): ";
    if (!interrupted && write(fd, notice, sizeof(notice)-1) == sizeof(notice)-1 &&
        write(fd, message, sizeof(message)-1) == sizeof(message)-1 &&
        write(fd, prompt, sizeof(prompt)-1) == sizeof(prompt)-1) {
        struct pollfd input = {fd, POLLIN, 0};
        int ready = 0;
        while (!interrupted && !ready) ready = poll(&input, 1, 100);
        if (!interrupted && ready > 0 && (input.revents & POLLIN)) n = read(fd, pin, 16);
    }
    int restored = tcsetattr(fd, TCSANOW, &saved);
    if (write(fd, "\n", 1) != 1 || restored || interrupted || n < 7 || n > 15 || pin[n-1] != '\n') goto done;
    *size = (CK_ULONG)n-1;
    for (CK_ULONG i = 0; i < *size; i++) if (pin[i] < 0x20 || pin[i] > 0x7e) goto done;
    failed = 0;
done:
    if (failed) { (void)tcflush(fd, TCIFLUSH); OPENSSL_cleanse(pin, 16); *size = 0; }
    close(fd);
    return failed;
}
static int unique(CK_FUNCTION_LIST_PTR api, CK_SESSION_HANDLE session,
                  CK_ATTRIBUTE *attrs, CK_ULONG size, CK_OBJECT_HANDLE *object)
{
    CK_OBJECT_HANDLE objects[2];
    CK_ULONG count = 0;
    CK_RV init = api->C_FindObjectsInit(session, attrs, size);
    if (init != CKR_OK) { result("find_init_result", init); return 1; }
    CK_RV found = CKR_OK;
    CK_ULONG total = 0;
    do {
        count = 0;
        CK_ULONG available = 2-total;
        found = interrupted ? CKR_FUNCTION_CANCELED : api->C_FindObjects(session, objects+total, available, &count);
        if (found || count > available) { found = found ? found : CKR_GENERAL_ERROR; break; }
        total += count;
    } while (count && total < 2);
    CK_RV final = api->C_FindObjectsFinal(session);
    int find_failed = result("find_result", found);
    int final_failed = result("find_final_result", final);
    if (find_failed || final_failed || total != 1) return 1;
    *object = objects[0];
    return 0;
}
static int attribute(CK_FUNCTION_LIST_PTR api, CK_SESSION_HANDLE session,
                     CK_OBJECT_HANDLE object, CK_ATTRIBUTE_TYPE type, void *out, CK_ULONG *size)
{
    CK_ULONG capacity = *size;
    CK_ATTRIBUTE attr = {type, out, capacity};
    if (api->C_GetAttributeValue(session, object, &attr, 1) || attr.ulValueLen > capacity || !attr.ulValueLen) return 1;
    *size = attr.ulValueLen;
    return 0;
}
static int verify(EVP_PKEY *key, const unsigned char signature[256], int altered)
{
    unsigned char text[sizeof(message)];
    memcpy(text, message, sizeof(text));
    if (altered) text[0] ^= 1;
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    int rv = -1;
    if (ctx && EVP_DigestVerifyInit(ctx, NULL, EVP_sha256(), NULL, key) > 0)
        rv = EVP_DigestVerify(ctx, signature, 256, text, sizeof(text)-1);
    EVP_MD_CTX_free(ctx);
    return rv;
}
static int run(CK_FUNCTION_LIST_PTR api, unsigned long index, const char *label)
{
    CK_SLOT_ID slots[32]; CK_ULONG count = 32;
    CK_SESSION_HANDLE session = 0;
    int initialized = 0, opened = 0, logged = 0, failed = 1;
    unsigned char der[8192] = {0}, id[64] = {0}, pin[16] = {0}, signature[256] = {0};
    unsigned char hash[32] = {0}, digest_info[51] = {0};
    EVP_PKEY *key = NULL; X509 *cert = NULL; X509_SIG *info = NULL;
    CK_RV init = api->C_Initialize(NULL);
    initialized = init == CKR_OK;
    if (result("initialize_result", init)) goto done;
    if (result("slot_list_result", api->C_GetSlotList(CK_TRUE, slots, &count)) || count > 32 || index >= count) goto done;
    CK_MECHANISM_INFO mechanism_info;
    if (result("mechanism_info_result", api->C_GetMechanismInfo(slots[index], CKM_RSA_PKCS, &mechanism_info)) ||
        !(mechanism_info.flags & CKF_SIGN) || mechanism_info.ulMinKeySize > 2048 || mechanism_info.ulMaxKeySize < 2048) goto done;
    CK_RV open_result = api->C_OpenSession(slots[index], CKF_SERIAL_SESSION, NULL, NULL, &session);
    opened = open_result == CKR_OK;
    if (result("session_open_result", open_result)) goto done;
    CK_SESSION_INFO state;
    if (result("session_info_result", api->C_GetSessionInfo(session, &state)) || state.state != CKS_RO_PUBLIC_SESSION) goto done;
    CK_OBJECT_CLASS cls = CKO_CERTIFICATE;
    CK_ATTRIBUTE cert_filter[] = {{CKA_CLASS, &cls, sizeof(cls)}, {CKA_LABEL, (void *)label, strlen(label)}};
    CK_OBJECT_HANDLE object;
    if (unique(api, session, cert_filter, 2, &object)) goto done;
    CK_ULONG size = sizeof(der), id_size = sizeof(id);
    if (attribute(api, session, object, CKA_ID, id, &id_size) || attribute(api, session, object, CKA_VALUE, der, &size)) goto done;
    const unsigned char *cursor = der;
    cert = d2i_X509(NULL, &cursor, (long)size);
    key = cert ? X509_get_pubkey(cert) : NULL;
    if (!key || cursor != der+size || !EVP_PKEY_is_a(key, "RSA") || EVP_PKEY_get_bits(key) != 2048) goto done;
    fprintf(report, "certificate_policy=rsa2048_matching_id_no_trust_validation\n");
    unsigned int hash_size = 0;
    info = X509_SIG_new();
    X509_ALGOR *alg = NULL; ASN1_OCTET_STRING *digest = NULL;
    if (!info || !EVP_Digest(message, sizeof(message)-1, hash, &hash_size, EVP_sha256(), NULL) || hash_size != 32) goto done;
    X509_SIG_getm(info, &alg, &digest);
    X509_ALGOR_set0(alg, OBJ_nid2obj(NID_sha256), V_ASN1_NULL, NULL);
    if (!ASN1_OCTET_STRING_set(digest, hash, 32) || i2d_X509_SIG(info, NULL) != 51) goto done;
    unsigned char *encoded = digest_info;
    if (i2d_X509_SIG(info, &encoded) != 51 || encoded != digest_info+51) goto done;
    fprintf(report, "signature_mechanism=CKM_RSA_PKCS\nsignature_input_policy=canonical_sha256_digestinfo\n");
    CK_ULONG pin_size;
    if (interrupted || read_pin(pin, &pin_size)) goto done;
    CK_RV login = api->C_Login(session, CKU_USER, pin, pin_size);
    OPENSSL_cleanse(pin, sizeof(pin));
    if (login == CKR_OK) logged = 1;
    if (result("login_result", login)) goto done;
    cls = CKO_PRIVATE_KEY;
    CK_BBOOL yes = CK_TRUE;
    CK_ATTRIBUTE private_filter[] = {{CKA_CLASS, &cls, sizeof(cls)}, {CKA_ID, id, id_size}, {CKA_SIGN, &yes, sizeof(yes)}};
    if (unique(api, session, private_filter, 3, &object)) goto done;
    CK_MECHANISM mechanism = {CKM_RSA_PKCS, NULL, 0};
    if (result("sign_init_result", api->C_SignInit(session, &mechanism, object))) goto done;
    size = sizeof(signature);
    if (result("sign_result", api->C_Sign(session, digest_info, sizeof(digest_info), signature, &size)) || size != 256) goto done;
    int valid = verify(key, signature, 0) == 1;
    int rejected = verify(key, signature, 1) == 0;
    fprintf(report, "signature_verification=%s\nchanged_message_verification=%s\n", valid ? "valid" : "failed", rejected ? "rejected" : "failed");
    if (!valid || !rejected) goto done;
    failed = 0;
done:
    OPENSSL_cleanse(pin, sizeof(pin)); OPENSSL_cleanse(der, sizeof(der));
    OPENSSL_cleanse(id, sizeof(id)); OPENSSL_cleanse(signature, sizeof(signature));
    OPENSSL_cleanse(hash, sizeof(hash)); OPENSSL_cleanse(digest_info, sizeof(digest_info));
    EVP_PKEY_free(key); X509_free(cert); X509_SIG_free(info);
    if (logged) {
        CK_RV rv = api->C_Logout(session);
        fprintf(report, "logout_result=%lu\n", rv);
        if (rv != CKR_OK && rv != CKR_USER_NOT_LOGGED_IN) failed = 1;
    }
    if (opened && result("session_close_result", api->C_CloseSession(session))) failed = 1;
    if (initialized && result("finalize_result", api->C_Finalize(NULL))) failed = 1;
    if (interrupted) failed = 1;
    fprintf(report, "test_result=%s\n", failed ? "incomplete_stop_no_retry" : "signature_verified_cleanup_api_ok");
    return failed;
}
int main(int argc, char **argv)
{
    if (argc != 3 || !argv[1][0] || !argv[2][0] || strlen(argv[2]) > 128) return 2;
    for (const char *p = argv[1]; *p; p++) if (*p < '0' || *p > '9') return 2;
    char *end; errno = 0; unsigned long index = strtoul(argv[1], &end, 10);
    if (errno || *end || index > 31) return 2;
    /* The launcher sets /dev/null, bypassing all model-specific/user configs. */
    const char *conf = getenv("OPENSC_CONF");
    if (!conf || strcmp(conf, "/dev/null")) return 2;
    int saved = dup(STDOUT_FILENO), null = open("/dev/null", O_WRONLY | O_CLOEXEC);
    if (saved < 0 || null < 0) return 1;
    report = fdopen(saved, "w");
    if (!report || dup2(null, STDOUT_FILENO) < 0 || dup2(null, STDERR_FILENO) < 0) return 1;
    close(null); setvbuf(report, NULL, _IOLBF, 0);
    struct sigaction action = {0}; action.sa_handler = stop; sigemptyset(&action.sa_mask);
    if (sigaction(SIGINT, &action, NULL) || sigaction(SIGTERM, &action, NULL) ||
        sigaction(SIGHUP, &action, NULL) || sigaction(SIGQUIT, &action, NULL)) return 1;
    void *module = dlopen(AUTO_MODULE_PATH, RTLD_NOW | RTLD_LOCAL);
    CK_C_GetFunctionList get = module ? dlsym(module, "C_GetFunctionList") : NULL;
    CK_FUNCTION_LIST_PTR api = NULL;
    Dl_info origin;
    int failed = 1;
    if (get && !get(&api) && api && dladdr((void *)api->C_Initialize, &origin) &&
        !strcmp(origin.dli_fname, AUTO_MODULE_PATH)) {
        fprintf(report, "module_identity=local_build\nconfiguration=none_dev_null\ndetection=normal_driver_dispatch\nslot_index=%lu\n", index);
        failed = run(api, index, argv[2]);
    }
    if (module) dlclose(module);
    fclose(report);
    return failed;
}
