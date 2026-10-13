/* Synthetic PC/SC implementation for ELF/delegate tests. No daemon or card. */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <winscard.h>
#include <reader.h>

static int calls, control_calls;
static unsigned int selected_file;
int fixture_count(void) { return calls; }
int fixture_control_count(void) { return control_calls; }

LONG SCardGetAttrib(SCARDHANDLE card, DWORD attribute, unsigned char *data, DWORD *size)
{
	const unsigned char atr[] = { 0x3B, 0xD5, 0x18, 0xFF, 0x81, 0x91, 0xFE, 0x1F, 0xC3, 0x80, 0x73, 0xC8, 0x21, 0x10, 0x0A };
	assert(card == 7 && attribute == SCARD_ATTR_ATR_STRING && data && size && *size >= sizeof(atr));
	assert(getenv("JCOP4_DIAGNOSTIC_READ4141") && !strcmp(getenv("JCOP4_DIAGNOSTIC_READ4141"), "1"));
	memcpy(data, atr, sizeof(atr)); *size = sizeof(atr);
	if (getenv("FIXTURE_COMPARISON_BAD_ATR")) data[0] ^= 1;
	return SCARD_S_SUCCESS;
}

LONG SCardEstablishContext(DWORD scope, const void *r1, const void *r2, SCARDCONTEXT *context)
{
	assert(scope == SCARD_SCOPE_USER && !r1 && !r2);
	*context = 17;
	return SCARD_S_SUCCESS;
}

LONG SCardControl(SCARDHANDLE card, DWORD code, const void *send, DWORD size,
		void *receive, DWORD capacity, DWORD *returned)
{
	const unsigned char features[] = { FEATURE_VERIFY_PIN_DIRECT, 4, 0x42, 0, 0, 6 };
	assert(card == 7 && code == CM_IOCTL_GET_FEATURE_REQUEST && !send && !size);
	assert(receive && capacity >= sizeof(features) && capacity <= 256 && returned);
	memcpy(receive, features, sizeof(features)); *returned = sizeof(features);
	control_calls++;
	return SCARD_S_SUCCESS;
}
LONG SCardConnect(SCARDCONTEXT context, const char *reader, DWORD share,
		DWORD protocols, SCARDHANDLE *card, DWORD *protocol)
{
	assert(context == 17 && share == SCARD_SHARE_SHARED && protocols == SCARD_PROTOCOL_T1);
	if (!strcmp(reader, "PRIVATE_READER_CONNECT_FAILURE"))
		return SCARD_E_SHARING_VIOLATION;
	assert(!strcmp(reader, "PRIVATE_READER"));
	*card = 7; *protocol = SCARD_PROTOCOL_T1;
	return SCARD_S_SUCCESS;
}
LONG SCardReconnect(SCARDHANDLE card, DWORD share, DWORD protocols, DWORD action, DWORD *protocol)
{
	assert(card == 7 && share == SCARD_SHARE_SHARED && protocols == SCARD_PROTOCOL_T1 && action == SCARD_LEAVE_CARD);
	*protocol = SCARD_PROTOCOL_T1;
	return SCARD_S_SUCCESS;
}
LONG SCardTransmit(SCARDHANDLE card, const SCARD_IO_REQUEST *pci,
		const unsigned char *cmd, DWORD size, SCARD_IO_REQUEST *rpci,
		unsigned char *response, DWORD *len)
{
	(void)pci; (void)rpci;
	/* Any leaked authentication or other command fails the fixture immediately. */
	assert(card == 7 && cmd[0] == 0);
	if (cmd[1] == 0xCA) {
		assert(size == 5 && cmd[2] == 1 && (cmd[3] == 0x16 || cmd[3] == 0));
	} else if (cmd[1] == 0xA4) {
		if (cmd[2] == 4) {
			const unsigned char aid[] = { 0xE8, 0x28, 0xBD, 8, 0x0F, 0xA0, 0, 0, 3, 0x63, 0x64, 0x79, 0x6E, 0x69, 0x64 };
			assert(size == 5 + sizeof(aid) && cmd[3] == 0x0C && cmd[4] == sizeof(aid) &&
					!memcmp(cmd + 5, aid, sizeof(aid)));
			selected_file = 0xFFFF; /* Synthetic known ADF marker. */
		} else {
			assert((size == 7 || size == 8) && cmd[3] == 0 && cmd[4] == 2);
			unsigned int fid = ((unsigned int)cmd[5] << 8) | cmd[6];
			assert((size == 7 && cmd[2] == 0 && fid == 0x3F00) ||
					(size == 7 && cmd[2] == 2 && fid == 0xD003 && selected_file == 0x3F00) ||
					(size == 8 && cmd[7] == 0 && cmd[2] == 2 && fid == 0x4141 && selected_file == 0xFFFF) ||
					(size == 8 && cmd[7] == 0 && cmd[2] == 2 && fid == 0x4109 && selected_file == 0x4141) ||
					(size == 8 && cmd[7] == 0 && cmd[2] == 2 && fid == 0x7001 && selected_file == 0x4109) ||
					(size == 8 && cmd[7] == 0 && cmd[2] == 2 && fid == 0x7002 && selected_file == 0x7001));
			selected_file = fid;
		}
	} else if (cmd[1] == 0xB0 && selected_file == 0x7002) {
		assert(size == 5 && cmd[2] == 0 && cmd[3] == 0 && cmd[4] == 0);
		assert(getenv("JCOP4_DIAGNOSTIC_PRKDF") && *len == 258);
		calls++;
		memset(response, 'X', 256); memcpy(response, "PRIVATE_PRKDF_INFO", 18);
		response[256] = 0x90; response[257] = 0; *len = 258;
		return SCARD_S_SUCCESS;
	} else if (cmd[1] == 0xB0 && selected_file == 0x7001) {
		assert(size == 7 && cmd[2] == 0 && cmd[3] == 0 && cmd[4] == 0 && cmd[5] == 1 && cmd[6] == 0x2C);
		assert(getenv("JCOP4_DIAGNOSTIC_AODF") && *len == 302);
		calls++;
		memset(response, 'X', 300); memcpy(response, "PRIVATE_AODF_INFO", 17);
		response[300] = 0x90; response[301] = 0; *len = 302;
		return SCARD_S_SUCCESS;
	} else {
		assert(size == 5 && cmd[1] == 0xB0 && cmd[2] == 0 && cmd[3] == 0 &&
				(selected_file == 0xD003 || (selected_file == 0x4109 && cmd[4] == 17 && getenv("JCOP4_DIAGNOSTIC_READ4109")) || (selected_file == 0x4141 && getenv("JCOP4_DIAGNOSTIC_READ4141") &&
				!strcmp(getenv("JCOP4_DIAGNOSTIC_READ4141"), "1"))));
	}
	if (cmd[1] == 0xB0 && selected_file == 0x4109) {
		assert(*len == 19);
		calls++;
		memcpy(response, "PRIVATE_FILE4109!", 17);
		response[17] = 0x90; response[18] = 0; *len = 19;
		return SCARD_S_SUCCESS;
	}
	assert(*len >= 18);
	calls++;
	memcpy(response, cmd[1] == 0xB0 ? "PRIVATE_SERIALNR" : "PRIVATE_RESPONSE", 16);
	response[16] = 0x90; response[17] = 0; *len = 18;
	return SCARD_S_SUCCESS;
}

/* pcsc-lite resolves its full API when establishing a context. These symbols
 * must exist, but this test must never invoke them or reach a real PC/SC API. */
#define U __attribute__((unused))
#define UNEXPECTED(name, args) LONG name args { abort(); }
UNEXPECTED(SCardReleaseContext, (SCARDCONTEXT context U))
UNEXPECTED(SCardIsValidContext, (SCARDCONTEXT context U))
UNEXPECTED(SCardDisconnect, (SCARDHANDLE card U, DWORD action U))
UNEXPECTED(SCardBeginTransaction, (SCARDHANDLE card U))
UNEXPECTED(SCardEndTransaction, (SCARDHANDLE card U, DWORD action U))
UNEXPECTED(SCardStatus, (SCARDHANDLE card U, char *reader U, DWORD *size U, DWORD *state U,
		DWORD *protocol U, unsigned char *atr U, DWORD *atr_size U))
UNEXPECTED(SCardGetStatusChange, (SCARDCONTEXT context U, DWORD timeout U, SCARD_READERSTATE *states U, DWORD count U))
UNEXPECTED(SCardListReaderGroups, (SCARDCONTEXT context U, char *groups U, DWORD *size U))
UNEXPECTED(SCardListReaders, (SCARDCONTEXT context U, const char *groups U, char *readers U, DWORD *size U))
UNEXPECTED(SCardFreeMemory, (SCARDCONTEXT context U, const void *data U))
UNEXPECTED(SCardCancel, (SCARDCONTEXT context U))
UNEXPECTED(SCardSetAttrib, (SCARDHANDLE card U, DWORD attribute U, const unsigned char *data U, DWORD size U))
