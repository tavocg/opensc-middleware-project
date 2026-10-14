/* Native PC/SC mock only. No transport library or real reader is linked. */
#define main reader_features_main
#include "../tools/jcop4-reader-features.c"
#undef main
#include <assert.h>
#include <unistd.h>

static int failure, contexts, lists, connects, controls, disconnects, releases;
static unsigned char features_fixture[256], properties_fixture[1024];
static size_t features_size, properties_size;
static char output[4096];
LONG SCardEstablishContext(DWORD scope, LPCVOID a, LPCVOID b, LPSCARDCONTEXT context)
{ assert(scope == SCARD_SCOPE_SYSTEM && !a && !b); contexts++; *context = 1; return failure == 1 ? SCARD_E_NO_SERVICE : SCARD_S_SUCCESS; }
LONG SCardListReaders(SCARDCONTEXT context, LPCSTR groups, LPSTR names, LPDWORD size)
{
	assert(context == 1 && !groups && *size == 4096 && ++lists == 1);
	if (failure == 2) return SCARD_E_NO_READERS_AVAILABLE;
	const char fixture[] = "SYNTHETIC PRIVATE READER\0";
	memcpy(names, fixture, sizeof(fixture)); *size = sizeof(fixture);
	if (failure == 3) names[*size - 2] = 1;
	return SCARD_S_SUCCESS;
}
LONG SCardConnect(SCARDCONTEXT context, LPCSTR name, DWORD share, DWORD requested, LPSCARDHANDLE handle, LPDWORD protocol)
{ assert(context == 1 && !strcmp(name, "SYNTHETIC PRIVATE READER") && share == SCARD_SHARE_DIRECT && !requested && ++connects == 1); *handle = 2; *protocol = 0; return failure == 4 ? SCARD_E_SHARING_VIOLATION : SCARD_S_SUCCESS; }
LONG SCardControl(SCARDHANDLE handle, DWORD control, LPCVOID input, DWORD input_size, LPVOID response, DWORD capacity, LPDWORD size)
{
	assert(handle == 2 && !input && !input_size && controls < 2); controls++;
	if (controls == 1) {
		assert(control == CM_IOCTL_GET_FEATURE_REQUEST && capacity == 256);
		if (failure == 5) return SCARD_E_UNSUPPORTED_FEATURE;
		memcpy(response, features_fixture, features_size); *size = failure == 6 ? 257 : features_size;
	} else {
		assert(control == READER_PROPERTIES_CONTROL && capacity == 1024);
		if (failure == 7) return SCARD_E_NOT_TRANSACTED;
		memcpy(response, properties_fixture, properties_size); *size = failure == 8 ? 1025 : properties_size;
	}
	return SCARD_S_SUCCESS;
}
LONG SCardDisconnect(SCARDHANDLE handle, DWORD disposition)
{ assert(handle == 2 && disposition == SCARD_LEAVE_CARD && ++disconnects == 1); return failure == 9 ? SCARD_E_NOT_TRANSACTED : SCARD_S_SUCCESS; }
LONG SCardReleaseContext(SCARDCONTEXT context)
{ assert(context == 1 && ++releases == 1); return failure == 10 ? SCARD_E_INVALID_HANDLE : SCARD_S_SUCCESS; }
static void defaults(void)
{
	failure = contexts = lists = connects = controls = disconnects = releases = 0;
	const unsigned char feature[] = {0x12,4,0x42,0x33,0,0x12}; memcpy(features_fixture, feature, 6); features_size = 6;
	const unsigned char property[] = {10,4,0,0,1,0}; memcpy(properties_fixture, property, 6); properties_size = 6;
}
static int invoke(const char *index)
{
	char *args[] = {"reader-features",(char *)index}; FILE *capture = tmpfile(); assert(capture);
	int saved = dup(STDOUT_FILENO); assert(saved >= 0); fflush(stdout); assert(dup2(fileno(capture), STDOUT_FILENO) >= 0);
	int rv = reader_features_main(2, args); fflush(stdout); rewind(capture);
	size_t size = fread(output, 1, sizeof(output)-1, capture); output[size] = 0;
	assert(dup2(saved, STDOUT_FILENO) >= 0); close(saved); fclose(capture);
	assert(!strstr(output, "SYNTHETIC") && !strstr(output, "Sending:") && !strstr(output, "42330006"));
	if (contexts && failure != 1) assert(releases == 1);
	if (connects && failure != 4) assert(disconnects == 1);
	return rv;
}
int main(void)
{
	defaults(); assert(invoke("0") == 0 && controls == 2 && disconnects == 1 && releases == 1);
	assert(strstr(output, "verify_pin_direct=not_advertised") && strstr(output, "max_apdu_data=65536"));
	assert(strstr(output, "extended_apdu=advertised_not_tested") && strstr(output, "card_apdu_count=0"));
	defaults(); memcpy(features_fixture + 6, "\x06\x04\x42\x33\0\x06", 6); features_size = 12;
	assert(invoke("0") == 0 && controls == 2 && strstr(output, "verify_pin_direct=advertised_not_executed"));
	defaults(); features_size = 0; assert(invoke("0") == 0 && controls == 1 && strstr(output, "max_apdu_data=not_advertised"));
	defaults(); memset(properties_fixture + 2, 0, 4); assert(invoke("0") == 0 && strstr(output, "short_only_advertised"));
	defaults(); properties_size = 0; assert(invoke("0") == 0 && strstr(output, "max_apdu_data=not_advertised"));
	puts("PASS DIRECT reader controls only: feature06 is reported but never executed, optional whitelisted CCID properties yield only advertised limits, no card protocol/APDU/reset and full LEAVE/release closure");
	for (int stage = 1; stage <= 10; stage++) { defaults(); failure = stage; assert(invoke("0") == 1 && controls <= 2); }
	defaults(); assert(invoke("1") == 1 && !connects && !controls);
	const char *bad_index[] = {"", "-1", "+0", "0foo", "4294967296", "99999999999999999999999"};
	for (size_t i = 0; i < sizeof(bad_index)/sizeof(bad_index[0]); i++) { defaults(); assert(invoke(bad_index[i]) == 2 && !contexts); }
	setenv("LD_PRELOAD", "synthetic_not_loaded", 1); defaults(); assert(invoke("0") == 2 && !contexts); unsetenv("LD_PRELOAD");
	puts("PASS every PC/SC/oversize/closure failure stops without retries; missing reader, invalid index and inherited preload cannot access a reader");
	for (int mutation = 0; mutation < 7; mutation++) {
		defaults();
		if (mutation == 0) features_fixture[1] = 3;
		if (mutation == 1) features_size = 5;
		if (mutation == 2) { memcpy(features_fixture + 6, features_fixture, 6); features_size = 12; }
		if (mutation == 3) { memset(features_fixture + 2, 0, 4); }
		if (mutation == 4) features_fixture[0] = 6; /* Dangerous alias must not authorize properties. */
		if (mutation == 5) memcpy(features_fixture + 2, "\x42\x33\0\x06", 4); /* PIN ioctl mislabeled as properties. */
		if (mutation == 6) memcpy(features_fixture + 2, "\x12\0\x33\x42", 4); /* Wrong endian/unknown control. */
		assert(invoke("0") == 1 && controls == 1);
	}
	puts("PASS truncated/duplicate/malformed/zero feature controls and aliases fail before properties; arbitrary/PIN/byte-swapped control codes are never invoked");
	for (int mutation = 0; mutation < 6; mutation++) {
		defaults();
		if (mutation == 0) properties_fixture[1] = 3;
		if (mutation == 1) properties_size = 5;
		if (mutation == 2) { memcpy(properties_fixture + 6, properties_fixture, 6); properties_size = 12; }
		if (mutation == 3) memcpy(properties_fixture + 2, "\xff\0\0\0", 4);
		if (mutation == 4) memcpy(properties_fixture + 2, "\x01\0\x01\0", 4);
		if (mutation == 5) properties_size = 1;
		assert(invoke("0") == 1 && !strstr(output, "max_apdu_data=") && controls == 2);
	}
	defaults(); properties_fixture[6] = 8; properties_fixture[7] = 6; memcpy(properties_fixture + 8, "SECRET", 6); properties_size = 14;
	assert(invoke("0") == 0 && !strstr(output, "SECRET"));
	assert(!reader_at("a\0\0\0", 4, 0));
	puts("PASS little-endian property limits reject duplicates/truncation/invalid ranges without publishing partial values; firmware/reader strings and premature list termination remain private");
	return 0;
}
