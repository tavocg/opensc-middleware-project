/* Reader metadata only. No SCardTransmit, card selection, PIN or reset. */
#include <winscard.h>
#include <reader.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Explicit CCID namespace, not an arbitrary reader-provided control code.
 * CCID's ccid_ifdhandler.h defines CLASS2_IOCTL_MAGIC=0x330000. */
#define READER_PROPERTIES_CONTROL SCARD_CTL_CODE(0x330000 + FEATURE_GET_TLV_PROPERTIES)
struct reader_features { int verify_direct, properties; uint32_t properties_control; };
static int decode_features(const unsigned char *data, size_t size, struct reader_features *output)
{
	unsigned char seen[256] = {0}; struct reader_features parsed = {0};
	if (!output) return 1;
	memset(output, 0, sizeof(*output));
	if (!data || size > 256 || size % 6) return 1;
	for (size_t i = 0; i < size; i += 6) {
		unsigned int tag = data[i];
		uint32_t control = (uint32_t)data[i+2]<<24 | (uint32_t)data[i+3]<<16 | (uint32_t)data[i+4]<<8 | data[i+5];
		if (seen[tag]++ || data[i+1] != 4 || !control || control == CM_IOCTL_GET_FEATURE_REQUEST ||
			(control == READER_PROPERTIES_CONTROL && tag != FEATURE_GET_TLV_PROPERTIES)) return 1;
		if (tag == FEATURE_VERIFY_PIN_DIRECT) parsed.verify_direct = 1; /* Never execute it. */
		if (tag == FEATURE_GET_TLV_PROPERTIES) { parsed.properties = 1; parsed.properties_control = control; }
	}
	*output = parsed; return 0;
}
static int decode_properties(const unsigned char *data, size_t size, int *found, uint32_t *maximum)
{
	unsigned char seen[256] = {0}; int present = 0; uint32_t value = 0;
	if (!found || !maximum) return 1;
	*found = 0; *maximum = 0;
	if (!data || size > 1024) return 1;
	for (size_t i = 0; i < size;) {
		if (size - i < 2) return 1;
		unsigned int tag = data[i++], length = data[i++];
		if (seen[tag]++ || length > size - i) return 1;
		if (tag == PCSCv2_PART10_PROPERTY_dwMaxAPDUDataSize) {
			if (length != 4) return 1;
			value = (uint32_t)data[i] | (uint32_t)data[i+1]<<8 | (uint32_t)data[i+2]<<16 | (uint32_t)data[i+3]<<24;
			if (value > 65536 || (value && value <= 256)) return 1;
			present = 1;
		}
		i += length;
	}
	*found = present; *maximum = value; return 0;
}
static const char *reader_at(const char *names, size_t size, unsigned long index)
{
	if (!names || size < 2 || size > 4096 || names[size-1] || names[size-2]) return NULL;
	const char *selected = NULL; unsigned long count = 0;
	for (size_t i = 0; i < size - 1;) {
		if (!names[i]) return NULL; /* Premature terminator/trailing bytes. */
		const char *end = memchr(names + i, 0, size - i); if (!end) return NULL;
		if (count++ == index) selected = names + i;
		i = (size_t)(end - names) + 1;
	}
	return selected;
}
int main(int argc, char **argv)
{
	const char *unsafe[] = {"LD_PRELOAD", "LD_AUDIT", "LIBPCSCLITE_DELEGATE", "LIBPCSCLITE_SPY_DELEGATE",
		"OPENSC_API_TRACE_FILE", "OPENSC_DEBUG_FILE", "JCOP4_PUBLIC_DISCOVERY_TRACE", "JCOP4_PUBLIC_SELECT_SPY_TRACE"};
	SCARDCONTEXT context = 0; SCARDHANDLE handle = 0; DWORD protocol = 0, size;
	char names[4096] = {0}, *end; unsigned char data[1024] = {0}; unsigned long index;
	LONG rv; int established = 0, connected = 0, result = 1, found; uint32_t maximum;
	struct reader_features features;
	if (argc != 2 || !argv[1][0] || strspn(argv[1], "0123456789") != strlen(argv[1])) return 2;
	errno = 0; index = strtoul(argv[1], &end, 10); if (errno || *end || index > UINT_MAX) return 2;
	for (size_t i = 0; i < sizeof(unsafe)/sizeof(unsafe[0]); i++) if (getenv(unsafe[i])) return 2;
	printf("mode=reader_features_only_no_card_apdu\n");
	rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &context);
	printf("context_result=%08lX\n", (unsigned long)(uint32_t)rv); if (rv != SCARD_S_SUCCESS) goto out; established = 1;
	size = sizeof(names); rv = SCardListReaders(context, NULL, names, &size);
	printf("reader_list_result=%08lX\n", (unsigned long)(uint32_t)rv); if (rv != SCARD_S_SUCCESS) goto out;
	const char *name = reader_at(names, size, index); if (!name) { printf("reader_result=not_found_or_malformed_list_stop\n"); goto out; }
	/* DIRECT/0 requests access to reader controls without negotiating a card protocol. */
	rv = SCardConnect(context, name, SCARD_SHARE_DIRECT, 0, &handle, &protocol);
	printf("direct_connect_result=%08lX\n", (unsigned long)(uint32_t)rv); if (rv != SCARD_S_SUCCESS) goto out; connected = 1;
	size = 0; rv = SCardControl(handle, CM_IOCTL_GET_FEATURE_REQUEST, NULL, 0, data, 256, &size);
	printf("features_control_result=%08lX\n", (unsigned long)(uint32_t)rv); if (rv != SCARD_S_SUCCESS) goto out;
	if (decode_features(data, size, &features)) { printf("features_result=malformed_stop\n"); goto out; }
	printf("features_result=decoded\nverify_pin_direct=%s\nget_tlv_properties=%s\n",
		features.verify_direct ? "advertised_not_executed" : "not_advertised", features.properties ? "advertised" : "not_advertised");
	if (!features.properties) { printf("max_apdu_data=not_advertised\n"); result = 0; goto out; }
	if (features.properties_control != READER_PROPERTIES_CONTROL) { printf("properties_result=unsupported_control_stop\n"); goto out; }
	memset(data, 0, sizeof(data)); size = 0;
	rv = SCardControl(handle, READER_PROPERTIES_CONTROL, NULL, 0, data, sizeof(data), &size);
	printf("properties_control_result=%08lX\n", (unsigned long)(uint32_t)rv); if (rv != SCARD_S_SUCCESS) goto out;
	if (decode_properties(data, size, &found, &maximum)) { printf("properties_result=malformed_stop\n"); goto out; }
	printf("properties_result=decoded\n");
	if (!found) printf("max_apdu_data=not_advertised\n");
	else printf("max_apdu_data=%u\nextended_apdu=%s\n", (unsigned int)maximum,
		maximum ? "advertised_not_tested" : "short_only_advertised");
	result = 0;
out:
	if (connected) { rv = SCardDisconnect(handle, SCARD_LEAVE_CARD); printf("disconnect_result=%08lX\n", (unsigned long)(uint32_t)rv); if (rv != SCARD_S_SUCCESS) result = 1; }
	if (established) { rv = SCardReleaseContext(context); printf("release_result=%08lX\n", (unsigned long)(uint32_t)rv); if (rv != SCARD_S_SUCCESS) result = 1; }
	memset(data, 0, sizeof(data)); memset(names, 0, sizeof(names));
	printf("reader_features_result=%s\ncard_apdu_count=0\n", result ? "incomplete_stop" : "metadata_only_no_pin_or_signature");
	return result;
}
