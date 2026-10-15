/* Standalone PKCS11 software fixture. No OpenSC, PCSC or hardware linkage. */
#include "pkcs11/pkcs11.h"
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static EVP_PKEY *key;
static unsigned char *der;
static int der_size, logged, searching, yielded;
static CK_OBJECT_CLASS wanted;
static const unsigned char id[] = {0x47};
static int mode(const char *value) { return !strcmp(getenv("FIXTURE_MODE"), value); }
static void event(const char *value)
{
    FILE *f = fopen(getenv("FIXTURE_EVENTS"), "a"); assert(f);
    fprintf(f, "%s\n", value); fclose(f);
}
static CK_RV initialize(CK_VOID_PTR arg)
{
    assert(!arg); event("initialize");
    const char *conf = getenv("OPENSC_CONF"); assert(conf && !strcmp(conf, "/dev/null"));
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL); assert(ctx);
    assert(EVP_PKEY_keygen_init(ctx) > 0 && EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048) > 0 && EVP_PKEY_keygen(ctx, &key) > 0);
    EVP_PKEY_CTX_free(ctx);
    X509 *cert = X509_new(); assert(cert);
    assert(X509_set_version(cert, 2) && ASN1_INTEGER_set(X509_get_serialNumber(cert), 1));
    assert(X509_gmtime_adj(X509_getm_notBefore(cert), 0) && X509_gmtime_adj(X509_getm_notAfter(cert), 3600));
    assert(X509_set_pubkey(cert, key));
    X509_NAME *name = X509_get_subject_name(cert);
    assert(X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (const unsigned char *)"Synthetic private identity", -1, -1, 0));
    assert(X509_set_issuer_name(cert, name) && X509_sign(cert, key, EVP_sha256()) > 0);
    der_size = i2d_X509(cert, &der); assert(der_size > 0); X509_free(cert);
    return CKR_OK;
}
static CK_RV finalize(CK_VOID_PTR arg)
{
    assert(!arg && !searching); event("finalize");
    EVP_PKEY_free(key); key = NULL; OPENSSL_free(der); der = NULL;
    return mode("finalize_fail") ? CKR_GENERAL_ERROR : CKR_OK;
}
static CK_RV slots(CK_BBOOL present, CK_SLOT_ID_PTR out, CK_ULONG_PTR count)
{ assert(present && *count >= 1); out[0] = 7; *count = 1; event("slots"); return CKR_OK; }
static CK_RV mechanism(CK_SLOT_ID slot, CK_MECHANISM_TYPE type, CK_MECHANISM_INFO_PTR out)
{
    assert(slot == 7 && type == CKM_RSA_PKCS); event("mechanism");
    out->flags = CKF_SIGN; out->ulMinKeySize = out->ulMaxKeySize = 2048;
    return mode("no_mechanism") ? CKR_MECHANISM_INVALID : CKR_OK;
}
static CK_RV open_session(CK_SLOT_ID slot, CK_FLAGS flags, CK_VOID_PTR application, CK_NOTIFY notify, CK_SESSION_HANDLE_PTR out)
{ assert(slot == 7 && flags == CKF_SERIAL_SESSION && !application && !notify); *out = 9; event("open"); return CKR_OK; }
static CK_RV close_session(CK_SESSION_HANDLE session)
{ assert(session == 9); event("close"); return mode("close_fail") ? CKR_GENERAL_ERROR : CKR_OK; }
static CK_RV session_info(CK_SESSION_HANDLE session, CK_SESSION_INFO_PTR out)
{ assert(session == 9); out->state = mode("already_logged") ? CKS_RO_USER_FUNCTIONS : CKS_RO_PUBLIC_SESSION; return CKR_OK; }
static CK_RV find_init(CK_SESSION_HANDLE session, CK_ATTRIBUTE_PTR attrs, CK_ULONG count)
{
    assert(session == 9 && !searching && count >= 2); searching = 1; yielded = 0;
    assert(attrs[0].type == CKA_CLASS); wanted = *(CK_OBJECT_CLASS *)attrs[0].pValue;
    if (wanted == CKO_CERTIFICATE) {
        assert(attrs[1].type == CKA_LABEL && attrs[1].ulValueLen == 12 && !memcmp(attrs[1].pValue, "LlaveDeFirma", 12));
        event("find_cert");
    } else {
        assert(wanted == CKO_PRIVATE_KEY && logged && count == 3);
        assert(attrs[1].type == CKA_ID && attrs[1].ulValueLen == sizeof(id) && !memcmp(attrs[1].pValue, id, sizeof(id)));
        assert(attrs[2].type == CKA_SIGN && *(CK_BBOOL *)attrs[2].pValue); event("find_private");
    }
    return CKR_OK;
}
static CK_RV find_objects(CK_SESSION_HANDLE session, CK_OBJECT_HANDLE_PTR objects, CK_ULONG max, CK_ULONG_PTR count)
{
    assert(session == 9 && searching && max > 0 && max <= 2);
    int duplicate = (wanted == CKO_CERTIFICATE && mode("duplicate_cert")) || (wanted == CKO_PRIVATE_KEY && mode("duplicate_key"));
    *count = yielded < (duplicate ? 2 : 1) ? 1 : 0; /* Return one object per call. */
    if (*count) { objects[0] = (wanted == CKO_CERTIFICATE ? 11 : 12) + yielded; yielded++; }
    return CKR_OK;
}
static CK_RV find_final(CK_SESSION_HANDLE session) { assert(session == 9 && searching); searching = 0; return CKR_OK; }
static CK_RV attribute(CK_SESSION_HANDLE session, CK_OBJECT_HANDLE object, CK_ATTRIBUTE_PTR attr, CK_ULONG count)
{
    assert(session == 9 && object == 11 && count == 1);
    const unsigned char *value; CK_ULONG size;
    if (attr->type == CKA_ID) { value = id; size = sizeof(id); }
    else { assert(attr->type == CKA_VALUE); value = der; size = (CK_ULONG)der_size; }
    assert(attr->pValue && attr->ulValueLen >= size);
    memcpy(attr->pValue, value, size); attr->ulValueLen = size; return CKR_OK;
}
static CK_RV login(CK_SESSION_HANDLE session, CK_USER_TYPE type, CK_UTF8CHAR_PTR pin, CK_ULONG size)
{
    assert(session == 9 && type == CKU_USER && !logged && (size == 6 || size == 14));
    for (CK_ULONG i = 0; i < size; i++) assert(pin[i] == '7');
    event("login"); if (mode("login_fail")) return CKR_PIN_INCORRECT;
    logged = 1; return CKR_OK;
}
static CK_RV logout(CK_SESSION_HANDLE session)
{ assert(session == 9 && logged); event("logout"); logged = 0; return mode("auto_logout") ? CKR_USER_NOT_LOGGED_IN : CKR_OK; }
static CK_RV sign_init(CK_SESSION_HANDLE session, CK_MECHANISM_PTR mechanism, CK_OBJECT_HANDLE object)
{
    assert(session == 9 && logged && object == 12 && mechanism->mechanism == CKM_RSA_PKCS && !mechanism->pParameter && !mechanism->ulParameterLen);
    event("sign_init"); return CKR_OK;
}
static CK_RV sign_data(CK_SESSION_HANDLE session, CK_BYTE_PTR data, CK_ULONG size, CK_BYTE_PTR signature, CK_ULONG_PTR length)
{
    assert(session == 9 && logged && size == 51 && signature && *length == 256);
    event("sign"); if (mode("sign_fail")) return CKR_DEVICE_ERROR;
    const unsigned char *cursor = data; X509_SIG *info = d2i_X509_SIG(NULL, &cursor, size);
    assert(info && cursor == data+size); X509_SIG_free(info);
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(key, NULL); assert(ctx);
    assert(EVP_PKEY_sign_init(ctx) > 0 && EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_PADDING) > 0);
    size_t output = *length; assert(EVP_PKEY_sign(ctx, signature, &output, data, size) > 0 && output == 256);
    EVP_PKEY_CTX_free(ctx); *length = output;
    if (mode("bad_signature")) signature[0] ^= 1;
    return CKR_OK;
}
static CK_FUNCTION_LIST functions = {
    .version = {2, 20}, .C_Initialize = initialize, .C_Finalize = finalize,
    .C_GetSlotList = slots, .C_GetMechanismInfo = mechanism,
    .C_OpenSession = open_session, .C_CloseSession = close_session, .C_GetSessionInfo = session_info,
    .C_FindObjectsInit = find_init, .C_FindObjects = find_objects, .C_FindObjectsFinal = find_final,
    .C_GetAttributeValue = attribute, .C_Login = login, .C_Logout = logout,
    .C_SignInit = sign_init, .C_Sign = sign_data
};
CK_RV C_GetFunctionList(CK_FUNCTION_LIST_PTR_PTR out) { *out = &functions; return CKR_OK; }
