/* Synthetic module for testing pkcs11-tool's signing fallback. No PIN,
 * reader, cryptographic key or real signature is involved.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pkcs11/pkcs11.h"

static int found;
static void event(const char *name)
{
	FILE *file = fopen(getenv("FIXTURE_API_LOG"), "a");
	if (file) { fprintf(file, "%s\n", name); fclose(file); }
}
static CK_RV initialize(CK_VOID_PTR args) { return CKR_OK; }
static CK_RV finalize(CK_VOID_PTR args) { return CKR_OK; }
static CK_RV info(CK_INFO_PTR info) { memset(info, 0, sizeof(*info)); return CKR_OK; }
static CK_RV slots(CK_BBOOL present, CK_SLOT_ID_PTR slots, CK_ULONG_PTR count)
{
	if (slots) slots[0] = 0;
	*count = 1;
	return CKR_OK;
}
static CK_RV slot_info(CK_SLOT_ID slot, CK_SLOT_INFO_PTR info)
{
	memset(info, 0, sizeof(*info));
	info->flags = CKF_TOKEN_PRESENT;
	return CKR_OK;
}
static CK_RV token_info(CK_SLOT_ID slot, CK_TOKEN_INFO_PTR info)
{
	memset(info, 0, sizeof(*info));
	info->flags = CKF_TOKEN_INITIALIZED;
	return CKR_OK;
}
static CK_RV mechanisms(CK_SLOT_ID slot, CK_MECHANISM_TYPE_PTR types, CK_ULONG_PTR count)
{
	if (types) types[0] = CKM_RSA_PKCS;
	*count = 1;
	return CKR_OK;
}
static CK_RV mechanism_info(CK_SLOT_ID slot, CK_MECHANISM_TYPE type, CK_MECHANISM_INFO_PTR info)
{
	info->ulMinKeySize = 2048;
	info->ulMaxKeySize = 2048;
	info->flags = CKF_SIGN;
	return CKR_OK;
}
static CK_RV open_session(CK_SLOT_ID slot, CK_FLAGS flags, CK_VOID_PTR data,
		CK_NOTIFY notify, CK_SESSION_HANDLE_PTR session) { *session = 1; return CKR_OK; }
static CK_RV close_session(CK_SESSION_HANDLE session) { return CKR_OK; }
static CK_RV find_init(CK_SESSION_HANDLE session, CK_ATTRIBUTE_PTR attrs, CK_ULONG count)
{
	found = 0;
	return CKR_OK;
}
static CK_RV find_objects(CK_SESSION_HANDLE session, CK_OBJECT_HANDLE_PTR objects,
		CK_ULONG maximum, CK_ULONG_PTR count)
{
	*count = found ? 0 : 1;
	if (!found) objects[0] = 1;
	found = 1;
	return CKR_OK;
}
static CK_RV find_final(CK_SESSION_HANDLE session) { return CKR_OK; }
static CK_RV attributes(CK_SESSION_HANDLE session, CK_OBJECT_HANDLE object,
		CK_ATTRIBUTE_PTR attrs, CK_ULONG count)
{
	for (CK_ULONG i = 0; i < count; i++) {
		CK_ULONG value = 0;
		CK_BBOOL boolean = CK_FALSE;
		void *data = &value;
		size_t length = sizeof(value);
		switch (attrs[i].type) {
		case CKA_CLASS: value = CKO_PRIVATE_KEY; break;
		case CKA_KEY_TYPE: value = CKK_RSA; break;
		case CKA_MODULUS_BITS: value = 2048; break;
		case CKA_ALWAYS_AUTHENTICATE: data = &boolean; length = sizeof(boolean); break;
		default: attrs[i].ulValueLen = CK_UNAVAILABLE_INFORMATION; return CKR_ATTRIBUTE_TYPE_INVALID;
		}
		if (attrs[i].pValue) {
			if (attrs[i].ulValueLen < length) return CKR_BUFFER_TOO_SMALL;
			memcpy(attrs[i].pValue, data, length);
		}
		attrs[i].ulValueLen = length;
	}
	return CKR_OK;
}
static CK_RV sign_init(CK_SESSION_HANDLE session, CK_MECHANISM_PTR mechanism, CK_OBJECT_HANDLE key)
{
	event("init");
	return CKR_OK;
}
static CK_RV sign(CK_SESSION_HANDLE session, CK_BYTE_PTR data, CK_ULONG length,
		CK_BYTE_PTR output, CK_ULONG_PTR output_length)
{
	event(output ? "sign" : "size");
	*output_length = 256;
	if (output) {
		CK_RV error = strtoul(getenv("FIXTURE_SIGN_ERROR"), NULL, 0);
		if (error) return error;
		memset(output, 0xCB, 256);
	}
	return CKR_OK;
}
static CK_RV sign_update(CK_SESSION_HANDLE session, CK_BYTE_PTR data, CK_ULONG length)
{
	event("update");
	return CKR_OK;
}
static CK_RV sign_final(CK_SESSION_HANDLE session, CK_BYTE_PTR output, CK_ULONG_PTR length)
{
	event(output ? "final" : "final-size");
	*length = 256;
	return output ? strtoul(getenv("FIXTURE_SIGN_ERROR"), NULL, 0) : CKR_OK;
}
CK_RV C_GetFunctionList(CK_FUNCTION_LIST_PTR_PTR list)
{
	static CK_FUNCTION_LIST functions = {
		.version = {2, 20}, .C_Initialize = initialize, .C_Finalize = finalize,
		.C_GetInfo = info, .C_GetFunctionList = C_GetFunctionList,
		.C_GetSlotList = slots, .C_GetSlotInfo = slot_info, .C_GetTokenInfo = token_info,
		.C_GetMechanismList = mechanisms, .C_GetMechanismInfo = mechanism_info,
		.C_OpenSession = open_session, .C_CloseSession = close_session,
		.C_FindObjectsInit = find_init, .C_FindObjects = find_objects, .C_FindObjectsFinal = find_final,
		.C_GetAttributeValue = attributes, .C_SignInit = sign_init, .C_Sign = sign,
		.C_SignUpdate = sign_update, .C_SignFinal = sign_final
	};
	*list = &functions;
	return CKR_OK;
}
