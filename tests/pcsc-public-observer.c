#define dlsym fixture_dlsym
#include "../tools/pcsc-public-observer.c"
#undef dlsym
#include <assert.h>
#include <sys/stat.h>

static int calls, control_calls, control_missing, control_response_fixture;
static DWORD control_capacity, control_size = 16;
static LONG control_result;
static unsigned int fixture_sw = 0x9000;
static unsigned int fixture_payload = 16, fixture_overrun;
static LONG fixture_transmit(SCARDHANDLE card, const SCARD_IO_REQUEST *send_pci,
		const unsigned char *cmd, DWORD len, SCARD_IO_REQUEST *recv_pci,
		unsigned char *response, DWORD *response_len)
{
	(void)card; (void)send_pci; (void)cmd; (void)len; (void)recv_pci;
	calls++;
	assert(*response_len >= fixture_payload + 2);
	if (control_response_fixture) {
		const unsigned char fcp[] = { 0x62, 9, 0x82, 1, 1, 0x86, 4, 0xA1, 0xB2, 0xC3, 0xD4, 0x90, 0 };
		memcpy(response, fcp, sizeof(fcp)); *response_len = sizeof(fcp);
		return SCARD_S_SUCCESS;
	}
	memset(response, 'X', fixture_payload);
	if (fixture_payload >= 16) memcpy(response, "PRIVATE_RESPONSE", 16);
	response[fixture_payload] = fixture_sw >> 8; response[fixture_payload + 1] = fixture_sw & 0xFF;
	*response_len = fixture_overrun ? *response_len + 1 : fixture_payload + 2;
	return SCARD_S_SUCCESS;
}
static LONG fixture_reconnect(SCARDHANDLE card, DWORD share, DWORD protocols,
		DWORD action, DWORD *protocol)
{
	assert(card == 7 && share == SCARD_SHARE_SHARED && protocols == SCARD_PROTOCOL_T1 && action == SCARD_RESET_CARD);
	*protocol = SCARD_PROTOCOL_T1;
	return SCARD_S_SUCCESS;
}
static LONG fixture_control(SCARDHANDLE card, DWORD code, const void *send, DWORD size,
		void *receive, DWORD capacity, DWORD *returned)
{
	assert(card == 7 && code == CM_IOCTL_GET_FEATURE_REQUEST && !send && !size);
	assert(receive && capacity && capacity <= 256 && returned);
	control_calls++; control_capacity = capacity;
	if (capacity >= 16) memcpy(receive, "PRIVATE_RESPONSE", 16);
	*returned = control_size;
	return control_result;
}
void *fixture_dlsym(void *handle, const char *name)
{
	assert(handle == RTLD_NEXT);
	if (!strcmp(name, "SCardTransmit")) return (void *)fixture_transmit;
	if (!strcmp(name, "SCardControl")) return control_missing ? NULL : (void *)fixture_control;
	assert(!strcmp(name, "SCardReconnect"));
	return (void *)fixture_reconnect;
}

static FILE *fixture_reset(void)
{
	FILE *file = tmpfile();
	assert(file);
	trace_fd = fileno(file);
	stopped = selected_public_file = selected_mf = selected_app = handle_set = pending_response = calls = events = lifecycle_events = 0;
	control_calls = control_missing = control_capacity = 0;
	control_response_fixture = 0; select_spy_fd = -1;
	control_size = 16; control_result = 0;
	fixture_sw = 0x9000;
	fixture_payload = 16; fixture_overrun = 0;
	compare_read4109 = read4109_calls = compare_read4141 = read4141_budget = read4141_offset = read4141_calls = 0;
	comparison_atr_ok = 1; /* Unit fixture simulates a validated connection. */
	return file;
}
static LONG send_command(const unsigned char *cmd, size_t size)
{
	unsigned char response[64];
	DWORD len = sizeof(response);
	return SCardTransmit(7, NULL, cmd, size, NULL, response, &len);
}
static LONG send_large(const unsigned char *cmd, size_t size)
{
	unsigned char response[402];
	DWORD len = sizeof(response);
	LONG rv = SCardTransmit(7, NULL, cmd, size, NULL, response, &len);
	if (rv != SCARD_S_SUCCESS) assert(!len);
	return rv;
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
	const unsigned char mf[] = { 0, 0xA4, 0, 0, 2, 0x3F, 0 };
	const unsigned char serial[] = { 0, 0xA4, 2, 0, 2, 0xD0, 3 };
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
	const unsigned char blocked_ef[] = { 0, 0xA4, 2, 0, 2, 0xE0, 0 };
	const unsigned char payload_ef[] = { 0, 0xA4, 2, 0, 10, 'S','E','C','R','E','T','_','P','I','N' };
	const unsigned char private_get[] = { 0, 0xCA, 0x7F, 0x48, 0 };
	const unsigned char protected_version[] = { 0x0C, 0xCA, 1, 0x16, 6 };
	FILE *file = fixture_reset();
	assert(send_command(root, sizeof(root)) == 0 && send_command(app, sizeof(app)) == 0);
	assert(send_command(select, sizeof(select)) == 0 && send_command(read, sizeof(read)) == 0 &&
			send_command(version, sizeof(version)) == 0 && calls == 5);
	check_private(file);
	puts("PASS public observer forwards only allowed discovery and logs no payloads or lengths");
	{
		const unsigned char source[] = { 0, 0xA4, 2, 0, 2, 0x41, 0x41, 0 };
		const unsigned char target[] = { 0, 0xA4, 2, 0, 2, 0x41, 0x09, 0 };
		const unsigned char small[] = { 0, 0xB0, 0, 0, 17 };
		const unsigned char extended[] = { 0, 0xB0, 0, 0, 0, 0, 17 };
		for (int i = 0; i < 2; i++) {
			file = fixture_reset(); compare_read4141 = compare_read4109 = 1;
			assert(send_command(app, sizeof(app)) == 0 && send_command(source, sizeof(source)) == 0);
			assert(send_large(read, sizeof(read)) == 0 && send_command(target, sizeof(target)) == 0);
			fixture_payload = 17;
			assert(send_large(i ? extended : small, i ? sizeof(extended) : sizeof(small)) == 0 &&
					read4109_calls == 1 && read4141_calls == 1 && calls == 5);
			assert(send_command(pin, sizeof(pin)) != 0 && calls == 5);
			{
				char log[2048] = {0};
				fflush(file); rewind(file); assert(fread(log, 1, sizeof(log)-1, file) < sizeof(log)-1);
				assert(strstr(log, "diagnostic_read=known_4109_contents_omitted"));
			}
			check_private(file);
		}
		puts("PASS separate opt-in accepts one unchanged 17-byte case-2 READ 4109, omits contents and blocks PIN");
		for (int i = 0; i < 9; i++) {
			unsigned char bad[sizeof(small)]; memcpy(bad, small, sizeof(bad));
			file = fixture_reset(); compare_read4141 = compare_read4109 = 1;
			assert(send_command(app, sizeof(app)) == 0 && send_command(source, sizeof(source)) == 0);
			assert(send_large(read, sizeof(read)) == 0 && send_command(target, sizeof(target)) == 0);
			if (i == 0) bad[0] = 1;
			if (i == 1) bad[0] = 0x0C;
			if (i == 2) bad[2] = 0x80;
			if (i == 3) bad[3] = 1;
			if (i == 4) bad[4] = 0;
			if (i == 5) bad[4] = 18;
			if (i == 6) bad[4] = 16;
			if (i == 7) compare_read4109 = 0;
			if (i == 8) comparison_atr_ok = 0;
			assert(send_large(bad, sizeof(bad)) != 0 && calls == 4 && !read4109_calls);
			check_private(file);
		}
		puts("PASS READ4109 refuses altered request sizes, SM, channels, SFI, offsets or missing opt-in/ATR before transport");
		for (int i = 0; i < 6; i++) {
			file = fixture_reset(); compare_read4141 = compare_read4109 = 1;
			assert(send_command(app, sizeof(app)) == 0 && send_command(source, sizeof(source)) == 0);
			assert(send_large(read, sizeof(read)) == 0 && send_command(target, sizeof(target)) == 0);
			fixture_payload = 17;
			if (i == 0) fixture_sw = 0x6982;
			if (i == 1) fixture_sw = 0x6101;
			if (i == 2) fixture_overrun = 1;
			if (i == 3) {
				unsigned char out[18]; DWORD size = sizeof(out);
				assert(SCardTransmit(7, NULL, small, sizeof(small), NULL, out, &size) != 0 && calls == 4 && !size);
			} else {
				if (i == 4) fixture_sw = 0x6282;
				assert((send_large(small, sizeof(small)) == 0) == (i >= 4));
			}
			int before = calls;
			assert(send_large(small, sizeof(small)) != 0 && send_large(get_response, sizeof(get_response)) != 0 && calls == before);
			check_private(file);
		}
		puts("PASS READ4109 bounds output, accepts 6282/9000 only and prevents repeat reads or GET RESPONSE after failure/success");
		file = fixture_reset(); compare_read4141 = compare_read4109 = 1;
		assert(send_command(app, sizeof(app)) == 0 && send_command(source, sizeof(source)) == 0);
		assert(send_large(read, sizeof(read)) == 0 && send_command(target, sizeof(target)) == 0);
		int saved = trace_fd; trace_fd = open("/dev/full", O_WRONLY); assert(trace_fd >= 0);
		assert(send_large(small, sizeof(small)) != 0 && calls == 4);
		close(trace_fd); trace_fd = saved; check_private(file);
		puts("PASS READ4109 stops before transport if its private diagnostic marker cannot be written");
	}
	{
		const unsigned char source[] = { 0, 0xA4, 2, 0, 2, 0x41, 0x41, 0 };
		const unsigned char target[] = { 0, 0xA4, 2, 0, 2, 0x41, 0x09, 0 };
		const unsigned char record[] = { 0, 0xB2, 1, 4, 0 };
		for (int i = 0; i < 3; i++) {
			file = fixture_reset(); compare_read4141 = 1;
			assert(send_command(app, sizeof(app)) == 0 && send_command(source, sizeof(source)) == 0);
			assert(send_large(read, sizeof(read)) == 0 && read4141_calls == 1);
			assert(send_command(target, sizeof(target) - (i == 1)) == 0 && calls == 4 &&
					selected_app && !selected_mf && !selected_public_file && read4141_calls == 1);
			assert(send_command(i == 0 ? read : i == 1 ? record : pin,
					i == 0 ? sizeof(read) : i == 1 ? sizeof(record) : sizeof(pin)) != 0 && calls == 4 && stopped);
			{
				char log[2048] = {0};
				fflush(file); rewind(file); assert(fread(log, 1, sizeof(log)-1, file) < sizeof(log)-1);
				assert(strstr(log, "public_select=known_4109_metadata_only"));
			}
			check_private(file);
		}
		puts("PASS comparison SELECT 4109 after bounded READ 4141 revokes read permission and blocks READ BINARY, RECORD and PIN before transport");
		for (int i = 0; i < 7; i++) {
			unsigned char bad[sizeof(target)]; memcpy(bad, target, sizeof(bad));
			file = fixture_reset(); compare_read4141 = 1;
			assert(send_command(app, sizeof(app)) == 0 && send_command(source, sizeof(source)) == 0);
			assert(send_large(read, sizeof(read)) == 0);
			if (i == 0) compare_read4141 = 0;
			if (i == 1) comparison_atr_ok = 0;
			if (i == 2) { assert(send_command(root, sizeof(root)) == 0); }
			if (i == 3) bad[0] = 1;
			if (i == 4) bad[2] = 0;
			if (i == 5) bad[3] = 0x0C;
			if (i == 6) bad[6] = 0x08;
			int before = calls;
			assert(send_command(bad, sizeof(bad)) != 0 && calls == before);
			check_private(file);
		}
		file = fixture_reset(); compare_read4141 = 1;
		assert(send_command(app, sizeof(app)) == 0 && send_command(source, sizeof(source)) == 0);
		assert(send_command(target, sizeof(target)) != 0 && calls == 2);
		check_private(file);
		puts("PASS SELECT 4109 remains blocked outside the comparison, prior-read sequence, verified ATR, known ADF and exact basic-channel form");
		for (int i = 0; i < 2; i++) {
			file = fixture_reset(); compare_read4141 = 1;
			assert(send_command(app, sizeof(app)) == 0 && send_command(source, sizeof(source)) == 0);
			assert(send_large(read, sizeof(read)) == 0);
			fixture_sw = i ? 0x6101 : 0x6982;
			assert(send_command(target, sizeof(target)) == 0 && !selected_app && !selected_public_file);
			assert(send_command(read, sizeof(read)) != 0 && calls == 4);
			check_private(file);
		}
		puts("PASS failed or pending SELECT 4109 never retains the previous file read permission");
	}
	{
		const unsigned char target[] = { 0, 0xA4, 2, 0, 2, 0x41, 0x41, 0 };
		const unsigned char rest[] = { 0, 0xB0, 1, 0, 144 };
		const unsigned char full[] = { 0, 0xB0, 0, 0, 0, 1, 0x90 };
		const unsigned char mse[] = { 0, 0x22, 0x41, 0xA4, 12, 0x80, 10, 4, 0, 0x7F, 0, 7, 2, 2, 3, 2, 2, 2 };
		file = fixture_reset(); compare_read4141 = 1;
		assert(send_command(app, sizeof(app)) == 0 && send_command(target, sizeof(target)) == 0);
		fixture_payload = 256;
		assert(send_large(read, sizeof(read)) == 0 && read4141_offset == 256);
		fixture_payload = 144;
		assert(send_large(rest, sizeof(rest)) == 0 && read4141_offset == 400 && read4141_budget == 400);
		assert(send_large(read, sizeof(read)) != 0 && calls == 4 && stopped);
		check_private(file);
		puts("PASS opt-in comparison forwards at most 400 contiguous bytes in two short reads and never logs contents");
		file = fixture_reset(); compare_read4141 = 1;
		assert(send_command(app, sizeof(app)) == 0 && send_command(target, sizeof(target)) == 0);
		fixture_payload = 400;
		assert(send_large(full, sizeof(full)) == 0 && read4141_budget == 400);
		fixture_payload = 16;
		assert(send_command(mse, sizeof(mse)) != 0 && calls == 3);
		{
			char log[1024] = {0};
			fflush(file); rewind(file); assert(fread(log, 1, sizeof(log)-1, file) < sizeof(log)-1);
			assert(strstr(log, "blocked_mse_profile=ca_ecdh_aes128") && strstr(log, "blocked_mse_case=short_case4"));
		}
		check_private(file);
		puts("PASS comparison accepts a bounded extended read then classifies the ZIP CA MSE without transmitting it");
		for (int i = 0; i < 6; i++) {
			const unsigned char bad[][7] = {
				{ 1, 0xB0, 0, 0, 0 }, { 0x0C, 0xB0, 0, 0, 0 },
				{ 0, 0xB0, 0x80, 0, 0 }, { 0, 0xB0, 0, 1, 0 },
				{ 0, 0xB0, 0, 0, 0, 1, 0x91 }, { 0, 0xB2, 1, 4, 0 }
			};
			file = fixture_reset(); compare_read4141 = 1;
			assert(send_command(app, sizeof(app)) == 0 && send_command(target, sizeof(target)) == 0);
			assert(send_large(bad[i], i == 4 ? 7 : 5) != 0 && calls == 2);
			check_private(file);
		}
		puts("PASS comparison rejects other channels, SM, SFI, noncontiguous or oversized reads and READ RECORD");
		file = fixture_reset(); compare_read4141 = 1;
		assert(send_command(app, sizeof(app)) == 0 && send_command(target, sizeof(target)) == 0);
		assert(send_command(read, sizeof(read)) != 0 && calls == 2);
		check_private(file);
		for (int i = 0; i < 3; i++) {
			file = fixture_reset(); compare_read4141 = 1;
			assert(send_command(app, sizeof(app)) == 0 && send_command(target, sizeof(target)) == 0);
			fixture_sw = i == 0 ? 0x6982 : i == 1 ? 0x6104 : 0x9000;
			fixture_overrun = i == 2;
			assert(send_large(read, sizeof(read)) != 0 && stopped && calls == 3);
			assert(send_large(get_response, sizeof(get_response)) != 0 && calls == 3);
			check_private(file);
		}
		puts("PASS comparison requires adequate output and stops on security, continuation or oversized transport response");
		file = fixture_reset(); compare_read4141 = 1;
		assert(send_command(app, sizeof(app)) == 0 && send_command(target, sizeof(target)) == 0);
		fixture_payload = 256;
		assert(send_large(read, sizeof(read)) == 0);
		fixture_payload = 16;
		assert(send_command(app, sizeof(app)) == 0 && send_command(target, sizeof(target)) == 0);
		{
			const unsigned char repeated[] = { 0, 0xB0, 0, 0, 144 };
			assert(send_large(repeated, sizeof(repeated)) != 0 && calls == 5 && read4141_budget == 256);
		}
		check_private(file);
		file = fixture_reset(); compare_read4141 = 1;
		assert(send_command(app, sizeof(app)) == 0 && send_command(target, sizeof(target)) == 0);
		assert(send_large(read, sizeof(read)) == 0);
		drop_card_context();
		assert(stopped && read4141_budget == 256 && send_command(app, sizeof(app)) != 0 && calls == 3);
		check_private(file);
		puts("PASS reselection cannot replay a read or replenish budget and reconnection after READ stops transport");
		file = fixture_reset(); compare_read4141 = 1;
		assert(send_command(app, sizeof(app)) == 0 && send_command(target, sizeof(target)) == 0);
		{
			int saved_fd = trace_fd;
			trace_fd = open("/dev/full", O_WRONLY);
			assert(trace_fd >= 0);
			assert(send_large(read, sizeof(read)) != 0 && calls == 2 && stopped);
			close(trace_fd); trace_fd = saved_fd;
		}
		check_private(file);
		puts("PASS mandatory comparison-log write failure stops before READ transport");
		for (int i = 0; i < 2; i++) {
			const unsigned char empty_verify[] = { 0, 0x20, 0, 1 };
			file = fixture_reset(); compare_read4141 = 1;
			assert(send_command(app, sizeof(app)) == 0 && send_command(target, sizeof(target)) == 0);
			assert(send_command(i ? pin : empty_verify, i ? sizeof(pin) : sizeof(empty_verify)) != 0 && calls == 2);
			check_private(file);
		}
		puts("PASS comparison blocks VERIFY even empty before transport and never logs a credential");
	}
	{
		const unsigned char target[] = { 0, 0xA4, 2, 0, 2, 0x41, 0x41, 0 };
		const unsigned char record[] = { 0, 0xB2, 1, 4, 0 };
		const unsigned char *reads[] = { read, record };
		for (size_t i = 0; i < 2; i++) {
			file = fixture_reset();
			assert(send_command(app, sizeof(app)) == 0 && selected_app);
			assert(send_command(select, sizeof(select)) == 0 && selected_app && selected_public_file);
			assert(send_command(target, 7 + i) == 0 && selected_app && !selected_mf && !selected_public_file);
			assert(send_command(reads[i], 5) != 0 && calls == 3 && stopped);
			assert(send_command(version, sizeof(version)) != 0 && calls == 3);
			{
				char log[1024] = {0};
				fflush(file); rewind(file);
				assert(fread(log, 1, sizeof(log) - 1, file) < sizeof(log) - 1);
				assert(strstr(log, "public_select=known_4141_metadata_only") && !strstr(log, "blocked_select_ef=4141"));
			}
			check_private(file);
		}
		puts("PASS SELECT 4141 requires known application and revokes previous file READ permission before blocking binary or record access");
		{
			const unsigned char variants[][16] = {
				{ 1, 0xA4, 2, 0, 2, 0x41, 0x41 },
				{ 0x0C, 0xA4, 2, 0, 2, 0x41, 0x41 },
				{ 0, 0xA4, 0, 0, 2, 0x41, 0x41 },
				{ 0, 0xA4, 2, 0x0C, 2, 0x41, 0x41 },
				{ 0, 0xA4, 8, 0, 4, 0x3F, 0, 0x41, 0x41 },
				{ 0, 0xA4, 2, 0, 10, 'S','E','C','R','E','T','_','P','I','N' },
				{ 0, 0xA4, 2, 0, 2, 0x41, 0x42 }
			};
			const size_t sizes[] = { 7, 7, 7, 7, 9, 15, 7 };
			for (size_t i = 0; i < 7; i++) {
				file = fixture_reset();
				assert(send_command(app, sizeof(app)) == 0);
				assert(send_command(variants[i], sizes[i]) != 0 && calls == 1 && stopped);
				check_private(file);
			}
		}
		file = fixture_reset();
		assert(send_command(target, sizeof(target)) != 0 && !calls);
		check_private(file);
		file = fixture_reset();
		assert(send_command(root, sizeof(root)) == 0 && !selected_app);
		assert(send_command(target, sizeof(target)) != 0 && calls == 1);
		check_private(file);
		puts("PASS metadata-only 4141 exception rejects other context, channels, selection forms, arbitrary payloads and neighboring files");
		for (int i = 0; i < 8; i++) {
			file = fixture_reset();
			assert(send_command(app, sizeof(app)) == 0 && selected_app);
			if (i == 0) { fixture_sw = 0x6A82; assert(send_command(app, sizeof(app)) == 0); }
			if (i == 1) { fixture_sw = 0x6A82; assert(send_command(select, sizeof(select)) == 0); }
			if (i == 2) assert(send_command(mf, sizeof(mf)) == 0);
			if (i == 3) assert(send_command(root, sizeof(root)) == 0);
			if (i == 4) {
				DWORD protocol;
				assert(SCardReconnect(7, SCARD_SHARE_SHARED, SCARD_PROTOCOL_T1, SCARD_RESET_CARD, &protocol) == 0);
			}
			if (i == 5) {
				const unsigned char channel_version[] = { 1, 0xCA, 1, 0x16, 6 };
				assert(send_command(channel_version, sizeof(channel_version)) == 0);
			}
			if (i == 6) {
				const unsigned char absolute[] = { 0, 0xA4, 8, 0, 4, 0x3F, 0, 0x2F, 0 };
				assert(send_command(absolute, sizeof(absolute)) == 0);
			}
			if (i == 7) { fixture_sw = 0x6104; assert(send_command(app, sizeof(app)) == 0); }
			assert(!selected_app);
			int before = calls;
			assert(send_command(target, sizeof(target)) != 0 && calls == before && stopped);
			check_private(file);
		}
		puts("PASS known application eligibility is lost on failed or pending selection, MF/path/channel changes and reconnect");
		file = fixture_reset();
		assert(send_command(app, sizeof(app)) == 0);
		fixture_sw = 0x6104;
		assert(send_command(target, sizeof(target)) == 0 && pending_response && !selected_public_file);
		fixture_sw = 0x9000;
		assert(send_command(get_response, sizeof(get_response)) == 0 && !pending_response && !selected_public_file);
		assert(send_command(read, sizeof(read)) != 0 && calls == 3 && stopped);
		check_private(file);
		file = fixture_reset();
		assert(send_command(app, sizeof(app)) == 0 && send_command(target, sizeof(target)) == 0);
		assert(send_command(pin, sizeof(pin)) != 0 && calls == 2 && stopped);
		check_private(file);
		puts("PASS metadata continuation grants no file READ or authentication permission and preserves private response handling");
	}
	file = fixture_reset();
	assert(send_command(serial, sizeof(serial)) != 0 && !calls);
	check_private(file);
	file = fixture_reset();
	assert(send_command(mf, sizeof(mf)) == 0 && selected_mf);
	assert(send_command(serial, sizeof(serial)) == 0 && selected_mf && selected_public_file == 2);
	assert(send_command(read, sizeof(read)) == 0 && calls == 3);
	check_private(file);
	file = fixture_reset();
	assert(send_command(root, sizeof(root)) == 0 && selected_mf);
	assert(send_command(serial, sizeof(serial)) == 0 && calls == 2);
	check_private(file);
	{
		const unsigned char variants[][9] = {
			{ 1, 0xA4, 2, 0, 2, 0xD0, 3 },
			{ 0x0C, 0xA4, 2, 0, 2, 0xD0, 3 },
			{ 0, 0xA4, 0, 0, 2, 0xD0, 3 },
			{ 0, 0xA4, 2, 0x0C, 2, 0xD0, 3 },
			{ 0, 0xA4, 8, 0, 4, 0x3F, 0, 0xD0, 3 }
		};
		for (size_t i = 0; i < sizeof(variants) / sizeof(variants[0]); i++) {
			file = fixture_reset();
			assert(send_command(mf, sizeof(mf)) == 0);
			assert(send_command(variants[i], i == 4 ? 9 : 7) != 0 && calls == 1);
			check_private(file);
		}
	}
	puts("PASS D003 selection and binary reads require confirmed basic-channel MF and never log serial data");
	file = fixture_reset(); fixture_sw = 0x6A82;
	assert(send_command(mf, sizeof(mf)) == 0 && !selected_mf);
	assert(send_command(serial, sizeof(serial)) != 0 && calls == 1);
	check_private(file);
	file = fixture_reset();
	assert(send_command(mf, sizeof(mf)) == 0);
	fixture_sw = 0x6A82;
	assert(send_command(serial, sizeof(serial)) == 0 && !selected_public_file && !selected_mf);
	assert(send_command(read, sizeof(read)) != 0 && calls == 2);
	check_private(file);
	file = fixture_reset();
	assert(send_command(mf, sizeof(mf)) == 0);
	assert(send_command(app, sizeof(app)) == 0 && !selected_mf);
	assert(send_command(serial, sizeof(serial)) != 0 && calls == 2);
	check_private(file);
	file = fixture_reset();
	assert(send_command(mf, sizeof(mf)) == 0);
	{
		const unsigned char other_channel_mf[] = { 1, 0xA4, 0, 0, 2, 0x3F, 0 };
		assert(send_command(other_channel_mf, sizeof(other_channel_mf)) == 0 && !selected_mf);
	}
	assert(send_command(serial, sizeof(serial)) != 0 && calls == 2);
	check_private(file);
	file = fixture_reset();
	assert(send_command(mf, sizeof(mf)) == 0);
	{
		DWORD protocol;
		assert(SCardReconnect(7, SCARD_SHARE_SHARED, SCARD_PROTOCOL_T1, SCARD_RESET_CARD, &protocol) == 0 && !selected_mf);
	}
	assert(send_command(serial, sizeof(serial)) != 0 && calls == 1);
	check_private(file);
	{
		const unsigned char record[] = { 0, 0xB2, 1, 4, 0 };
		const unsigned char channel_read[] = { 1, 0xB0, 0, 0, 0 };
		const unsigned char *variants[] = { record, channel_read };
		for (size_t i = 0; i < sizeof(variants) / sizeof(variants[0]); i++) {
			file = fixture_reset();
			assert(send_command(mf, sizeof(mf)) == 0 && send_command(serial, sizeof(serial)) == 0);
			assert(send_command(variants[i], 5) != 0 && calls == 2);
			check_private(file);
		}
	}
	puts("PASS D003 permission is lost on failures, application/channel changes and reconnect; record/channel reads stay blocked");
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
	assert(send_command(blocked_ef, sizeof(blocked_ef)) != 0 && !calls && stopped);
	assert(send_command(read, sizeof(read)) != 0 && !calls);
	{
		char log[256] = {0};
		fflush(file); rewind(file);
		assert(fread(log, 1, sizeof(log) - 1, file) < sizeof(log) - 1);
		assert(strstr(log, "blocked_header=00A40200") && strstr(log, "blocked_select_ef=E000"));
	}
	check_private(file);
	file = fixture_reset();
	assert(send_command(payload_ef, sizeof(payload_ef)) != 0 && !calls && stopped);
	{
		char log[256] = {0};
		fflush(file); rewind(file);
		assert(fread(log, 1, sizeof(log) - 1, file) < sizeof(log) - 1);
		assert(strstr(log, "blocked_select_form=unsupported") && !strstr(log, "blocked_select_ef="));
	}
	check_private(file);
	puts("PASS blocked SELECT logs only a validated two-byte EF identifier and never forwards selection or fallback reads");
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
	{
		const DWORD codes[] = { CM_IOCTL_GET_FEATURE_REQUEST, 0x42000006, 0x42000006 };
		const void *inputs[] = { "SECRET_PIN", "SECRET_PIN", NULL };
		const DWORD sizes[] = { 10, 10, 0 };
		const char *kinds[] = { "feature_request_unsupported", "unknown", "unknown" };
		for (size_t i = 0; i < sizeof(codes) / sizeof(codes[0]); i++) {
			char log[1024] = {0}, identifier[80];
			unsigned char response[64]; DWORD returned = 99;
			file = fixture_reset();
			assert(SCardControl(7, codes[i], inputs[i], sizes[i], response, sizeof(response), &returned) != 0);
			assert(returned == 0 && stopped && !calls && !control_calls);
			assert(send_command(version, sizeof(version)) != 0 && !calls);
			/* Later controls cannot add diagnostics after the first stop. */
			assert(SCardControl(7, 0xDEADBEEF, NULL, 0, NULL, 0, NULL) != 0);
			snprintf(identifier, sizeof(identifier), "blocked_control_code=%08lX", (unsigned long)codes[i]);
			fflush(file); rewind(file);
			assert(fread(log, 1, sizeof(log) - 1, file) < sizeof(log) - 1);
			assert(strstr(log, identifier) && strstr(log, kinds[i]) && !strstr(log, "DEADBEEF"));
			check_private(file);
		}
	}
	puts("PASS blocked reader controls record only their identifier and bounded feature-query classification without forwarding");
	{
		unsigned char response[512]; DWORD returned;
		file = fixture_reset();
		assert(send_command(mf, sizeof(mf)) == 0 && send_command(serial, sizeof(serial)) == 0);
		assert(SCardControl(7, CM_IOCTL_GET_FEATURE_REQUEST, NULL, 0, response, sizeof(response), &returned) == 0);
		assert(control_calls == 1 && control_capacity == 256 && returned == 16);
		assert(!memcmp(response, "PRIVATE_RESPONSE", 16) && selected_mf && selected_public_file == 2);
		assert(send_command(read, sizeof(read)) == 0 && calls == 3);
		/* A pointer with zero length is normalized to NULL before forwarding. */
		control_size = 0;
		assert(SCardControl(7, CM_IOCTL_GET_FEATURE_REQUEST, "SECRET_PIN", 0, response, 64, &returned) == 0);
		assert(control_calls == 2 && control_capacity == 64 && returned == 0 && !stopped);
		check_private(file);
	}
	puts("PASS empty reader capability query forwards bounded responses privately and preserves card context");
	{
		unsigned char response[64]; DWORD returned;
		for (int i = 0; i < 6; i++) {
			file = fixture_reset();
			if (i == 0) control_result = SCARD_E_NOT_TRANSACTED;
			if (i == 1) control_size = 65;
			if (i == 2) control_missing = 1;
			memset(response, 0xAA, sizeof(response)); returned = 99;
			assert(SCardControl(7, CM_IOCTL_GET_FEATURE_REQUEST, NULL, 0,
					i == 3 ? NULL : response, i == 4 ? 0 : sizeof(response), i == 5 ? NULL : &returned) != 0);
			assert(stopped && !calls && control_calls == (i < 2 ? 1 : 0));
			assert(response[0] == 0xAA && (i == 5 || returned == 0));
			assert(send_command(version, sizeof(version)) != 0 && !calls);
			check_private(file);
		}
	}
	puts("PASS reader query rejects errors, oversized replies, missing transport and invalid buffers before copying responses");
	{
		unsigned char response[64]; DWORD returned;
		file = fixture_reset(); events = 128;
		assert(SCardControl(7, CM_IOCTL_GET_FEATURE_REQUEST, NULL, 0, response, sizeof(response), &returned) != 0 && !control_calls);
		check_private(file);
		file = fixture_reset();
		assert(send_command(version, sizeof(version)) == 0);
		assert(SCardControl(8, CM_IOCTL_GET_FEATURE_REQUEST, NULL, 0, response, sizeof(response), &returned) != 0 && !control_calls);
		check_private(file);
		file = fixture_reset();
		assert(SCardControl(7, CM_IOCTL_GET_FEATURE_REQUEST, NULL, 0, response, sizeof(response), &returned) == 0);
		assert(SCardControl(7, 0x42000006, NULL, 0, response, sizeof(response), &returned) != 0 && control_calls == 1);
		assert(SCardControl(7, CM_IOCTL_GET_FEATURE_REQUEST, NULL, 0, response, sizeof(response), &returned) != 0 && control_calls == 1);
		check_private(file);
		assert(SCardControl(7, CM_IOCTL_GET_FEATURE_REQUEST, NULL, 0, response, sizeof(response), &returned) != 0 && control_calls == 1);
	}
	puts("PASS reader queries share APDU handle/event limits and cannot bypass a stop or missing private log");
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
	{
		const unsigned char file4141[] = { 0, 0xA4, 2, 0, 2, 0x41, 0x41, 0 };
		FILE *spy = tmpfile();
		char capture[2048] = {0};
		const char *capture_path = getenv("PCSC_SELECT_SPY_FIXTURE");
		assert(spy);
		file = fixture_reset(); select_spy_fd = fileno(spy);
		assert(send_command(app, sizeof(app)) == 0);
		assert(ftell(spy) == 0); /* Non-target responses never enter this stream. */
		control_response_fixture = 1;
		assert(send_command(file4141, sizeof(file4141)) == 0);
		assert(send_command(read, sizeof(read)) != 0 && calls == 2);
		assert(send_command(pin, sizeof(pin)) != 0 && calls == 2);
		fflush(spy); rewind(spy);
		assert(fread(capture, 1, sizeof(capture) - 1, spy) > 0);
		assert(strstr(capture, "00 A4 02 00 02 41 41 00\n") &&
			strstr(capture, "62 09 82 01 01 86 04 A1 B2 C3 D4 90 00\n") &&
			!strstr(capture, "00 B0") && !strstr(capture, "00 20") &&
			!strstr(capture, "53 45 43 52 45 54") && !strstr(capture, "PRIVATE_RESPONSE"));
		if (capture_path) {
			FILE *export = fopen(capture_path, "wx");
			assert(export && fputs(capture, export) >= 0 && fclose(export) == 0);
		}
		fclose(spy); select_spy_fd = -1; check_private(file);
	}
	puts("PASS optional spy stream records only permitted SELECT 4141 control data and excludes blocked READ and PIN");
	{
		const unsigned char file4141[] = { 0, 0xA4, 2, 0, 2, 0x41, 0x41 };
		FILE *spy = tmpfile();
		struct timeval start;
		unsigned char oversized[259] = {0};
		char capture[2048] = {0};
		assert(spy); select_spy_fd = fileno(spy); gettimeofday(&start, NULL);
		assert(!capture_select(file4141, sizeof(file4141), oversized, sizeof(oversized), 0, &start));
		fflush(spy); rewind(spy); assert(fread(capture, 1, sizeof(capture) - 1, spy) > 0);
		assert(strstr(capture, "0@NULL\n") && !strstr(capture, "00 00 00"));
		fclose(spy);
		file = fixture_reset();
		/* Closed output must stop further commands after a capture failure. */
		spy = tmpfile(); assert(spy); select_spy_fd = fileno(spy); fclose(spy);
		assert(send_command(app, sizeof(app)) == 0);
		assert(send_command(file4141, sizeof(file4141)) != 0 && stopped && calls == 2);
		assert(send_command(version, sizeof(version)) != 0 && calls == 2);
		select_spy_fd = -1; check_private(file);
	}
	puts("PASS spy capture omits oversized responses and stops transport after a log write failure");
	{
		char dir[] = "/tmp/jcop4-select-spy-XXXXXX", headers[256], raw[256], target[256];
		struct stat info;
		assert(mkdtemp(dir));
		snprintf(target, sizeof(target), "%s/untouched", dir);
		FILE *sentinel = fopen(target, "wx"); assert(sentinel);
		assert(fputs("DO_NOT_CHANGE", sentinel) >= 0 && fclose(sentinel) == 0);
		for (int i = 0; i < 4; i++) {
			snprintf(headers, sizeof(headers), "%s/headers-%d", dir, i);
			snprintf(raw, sizeof(raw), "%s/raw-%d", dir, i);
			if (i == 0) { FILE *existing = fopen(raw, "wx"); assert(existing && fclose(existing) == 0); }
			if (i == 1) assert(!symlink(target, raw));
			if (i == 2) snprintf(raw, sizeof(raw), "%s/missing/raw", dir);
			assert(!setenv("JCOP4_PUBLIC_DISCOVERY_TRACE", headers, 1));
			assert(!setenv("JCOP4_PUBLIC_SELECT_SPY_TRACE", raw, 1));
			stopped = 0; calls = 0;
			observer_init();
			if (i < 3) assert(stopped && send_command(version, sizeof(version)) != 0 && !calls);
			else assert(!stopped && select_spy_fd >= 0 && !fstat(select_spy_fd, &info) && (info.st_mode & 0777) == 0600);
			if (trace_fd >= 0) close(trace_fd);
			if (select_spy_fd >= 0) close(select_spy_fd);
			trace_fd = select_spy_fd = -1;
			assert(!unlink(headers));
			if (i != 2) assert(!unlink(raw));
		}
		char value[32] = {0}; sentinel = fopen(target, "r"); assert(sentinel);
		assert(fread(value, 1, sizeof(value) - 1, sentinel) == strlen("DO_NOT_CHANGE") && !strcmp(value, "DO_NOT_CHANGE"));
		fclose(sentinel); assert(!unlink(target) && !rmdir(dir));
		unsetenv("JCOP4_PUBLIC_DISCOVERY_TRACE"); unsetenv("JCOP4_PUBLIC_SELECT_SPY_TRACE");
	}
	puts("PASS optional spy requires a new private log and refuses existing, symlinked or unavailable destinations before transport");
	return 0;
}
