#define dlsym fixture_dlsym
#include "../tools/pcsc-public-observer.c"
#undef dlsym
#include <assert.h>

static int calls;
static unsigned int fixture_sw = 0x9000;
static LONG fixture_transmit(SCARDHANDLE card, const SCARD_IO_REQUEST *send_pci,
		const unsigned char *cmd, DWORD len, SCARD_IO_REQUEST *recv_pci,
		unsigned char *response, DWORD *response_len)
{
	(void)card; (void)send_pci; (void)cmd; (void)len; (void)recv_pci;
	calls++;
	assert(*response_len >= 18);
	memcpy(response, "PRIVATE_RESPONSE", 16);
	response[16] = fixture_sw >> 8; response[17] = fixture_sw & 0xFF;
	*response_len = 18;
	return SCARD_S_SUCCESS;
}
static LONG fixture_reconnect(SCARDHANDLE card, DWORD share, DWORD protocols,
		DWORD action, DWORD *protocol)
{
	assert(card == 7 && share == SCARD_SHARE_SHARED && protocols == SCARD_PROTOCOL_T1 && action == SCARD_RESET_CARD);
	*protocol = SCARD_PROTOCOL_T1;
	return SCARD_S_SUCCESS;
}
void *fixture_dlsym(void *handle, const char *name)
{
	assert(handle == RTLD_NEXT);
	if (!strcmp(name, "SCardTransmit")) return (void *)fixture_transmit;
	assert(!strcmp(name, "SCardReconnect"));
	return (void *)fixture_reconnect;
}

static FILE *fixture_reset(void)
{
	FILE *file = tmpfile();
	assert(file);
	trace_fd = fileno(file);
	stopped = selected_public_file = handle_set = pending_response = calls = events = lifecycle_events = 0;
	fixture_sw = 0x9000;
	return file;
}
static LONG send_command(const unsigned char *cmd, size_t size)
{
	unsigned char response[64];
	DWORD len = sizeof(response);
	return SCardTransmit(7, NULL, cmd, size, NULL, response, &len);
}
static void check_private(FILE *file)
{
	char log[4096] = {0};
	fflush(file); rewind(file);
	assert(fread(log, 1, sizeof(log) - 1, file) < sizeof(log) - 1);
	assert(!strstr(log, "PRIVATE_RESPONSE") && !strstr(log, "SECRET_PIN") &&
			!strstr(log, "length") && !strstr(log, "len=") && !strstr(log, "data="));
	fclose(file); trace_fd = -1;
}
int main(void)
{
	const unsigned char select[] = { 0, 0xA4, 0, 0x0C, 2, 0x2F, 0 };
	const unsigned char root[] = { 0, 0xA4, 4, 0x0C, 7, 0xA0, 0, 0, 0, 0, 0x3F, 0 };
	const unsigned char app[] = { 0, 0xA4, 4, 0x0C, 15, 0xE8, 0x28, 0xBD, 8, 0x0F, 0xA0, 0, 0, 3, 0x63, 0x64, 0x79, 0x6E, 0x69, 0x64 };
	const unsigned char read[] = { 0, 0xB0, 0, 0, 0 };
	const unsigned char version[] = { 0, 0xCA, 1, 0x16, 6 };
	const unsigned char name[] = { 0, 0xCA, 1, 0, 0 };
	const unsigned char unknown_name_tag[] = { 0, 0xCA, 1, 1, 0 };
	const unsigned char name_with_payload[] = { 0, 0xCA, 1, 0, 1, 'x' };
	const unsigned char other_channel_name[] = { 1, 0xCA, 1, 0, 0 };
	const unsigned char protected_name[] = { 0x0C, 0xCA, 1, 0, 0 };
	const unsigned char get_response[] = { 0, 0xC0, 0, 0, 0 };
	const unsigned char pin[] = { 0, 0x20, 0, 1, 10, 'S','E','C','R','E','T','_','P','I','N' };
	const unsigned char private_select[] = { 0, 0xA4, 0, 0x0C, 2, 0xE0, 0 };
	const unsigned char private_get[] = { 0, 0xCA, 0x7F, 0x48, 0 };
	const unsigned char protected_version[] = { 0x0C, 0xCA, 1, 0x16, 6 };
	FILE *file = fixture_reset();
	assert(send_command(root, sizeof(root)) == 0 && send_command(app, sizeof(app)) == 0);
	assert(send_command(select, sizeof(select)) == 0 && send_command(read, sizeof(read)) == 0 &&
			send_command(version, sizeof(version)) == 0 && calls == 5);
	check_private(file);
	puts("PASS public observer forwards only allowed discovery and logs no payloads or lengths");
	file = fixture_reset();
	assert(send_command(name, sizeof(name)) == 0 && calls == 1);
	check_private(file);
	{
		const unsigned char *rejected[] = { unknown_name_tag, name_with_payload, other_channel_name, protected_name, name };
		const size_t sizes[] = { sizeof(unknown_name_tag), sizeof(name_with_payload), sizeof(other_channel_name), sizeof(protected_name), 4 };
		for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
			file = fixture_reset();
			assert(send_command(rejected[i], sizes[i]) != 0 && !calls && stopped);
			check_private(file);
		}
	}
	puts("PASS ChipDoc name discovery permits only basic-channel case-2 GET DATA and keeps replies private");
	file = fixture_reset();
	assert(send_command(pin, sizeof(pin)) != 0 && !calls && stopped);
	assert(send_command(version, sizeof(version)) != 0 && !calls);
	check_private(file);
	puts("PASS public observer blocks VERIFY before transport and blocks subsequent fallback commands");
	file = fixture_reset();
	assert(send_command(private_select, sizeof(private_select)) != 0 && !calls);
	assert(send_command(read, sizeof(read)) != 0 && !calls);
	check_private(file);
	file = fixture_reset();
	assert(send_command(select, sizeof(select)) == 0);
	fixture_sw = 0x6283;
	assert(send_command(select, sizeof(select)) == 0 && !selected_public_file);
	assert(send_command(read, sizeof(read)) != 0 && calls == 2);
	check_private(file);
	file = fixture_reset();
	assert(send_command(private_get, sizeof(private_get)) != 0 && !calls);
	check_private(file);
	file = fixture_reset();
	assert(send_command(protected_version, sizeof(protected_version)) != 0 && !calls);
	check_private(file);
	puts("PASS public observer blocks reads of other files, unknown GET DATA and secure messaging");
	file = fixture_reset();
	assert(send_command(get_response, sizeof(get_response)) != 0 && !calls);
	check_private(file);
	file = fixture_reset(); fixture_sw = 0x6104;
	assert(send_command(version, sizeof(version)) == 0 && pending_response);
	fixture_sw = 0x9000;
	assert(send_command(get_response, sizeof(get_response)) == 0 && calls == 2 && !pending_response);
	check_private(file);
	puts("PASS public observer permits GET RESPONSE only after a permitted command returned 61xx");
	file = fixture_reset();
	assert(send_command(select, sizeof(select)) == 0);
	pending_response = 1;
	{
		DWORD protocol;
		assert(SCardReconnect(7, SCARD_SHARE_SHARED, SCARD_PROTOCOL_T1, SCARD_RESET_CARD, &protocol) == 0);
	}
	assert(!selected_public_file && !pending_response);
	assert(send_command(read, sizeof(read)) != 0 && calls == 1);
	check_private(file);
	puts("PASS reconnect drops file and pending-response permissions before later reads");
	file = fixture_reset(); fixture_sw = 0x6982;
	assert(send_command(version, sizeof(version)) == 0 && stopped && calls == 1);
	assert(send_command(version, sizeof(version)) != 0 && calls == 1);
	check_private(file);
	file = fixture_reset();
	assert(SCardControl(7, 0, "SECRET_PIN", 10, NULL, 0, NULL) != 0 && stopped && !calls);
	check_private(file);
	file = fixture_reset();
	assert(SCardSetAttrib(7, 0, (const unsigned char *)"SECRET_PIN", 10) != 0 && stopped && !calls);
	check_private(file);
	puts("PASS public observer stops on security errors and blocks reader PIN-pad control paths");
	file = fixture_reset(); events = 128;
	assert(send_command(version, sizeof(version)) != 0 && !calls);
	check_private(file);
	file = fixture_reset();
	assert(send_command(version, sizeof(version)) == 0);
	{
		unsigned char response[64]; DWORD len = sizeof(response);
		assert(SCardTransmit(8, NULL, version, sizeof(version), NULL, response, &len) != 0 && calls == 1);
	}
	check_private(file);
	assert(send_command(version, sizeof(version)) != 0);
	puts("PASS public observer enforces event/handle limits and refuses transport without a private log");
	return 0;
}
