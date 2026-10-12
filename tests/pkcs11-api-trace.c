/* Only numeric API metadata is accepted. No card, PIN or data buffer. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "pkcs11/debug.c"

static int failures;
static void check(const char *name, int valid)
{
	printf("%s %s\n", valid ? "PASS" : "FAIL", name);
	if (!valid) failures++;
}

static size_t read_trace(const char *path, char *output, size_t size)
{
	FILE *file = fopen(path, "r");
	size_t length = 0;
	if (file) {
		length = fread(output, 1, size - 1, file);
		fclose(file);
	}
	output[length] = 0;
	return length;
}

int main(void)
{
	char path[] = "/tmp/jcop3-api-trace-XXXXXX";
	char invalid_path[sizeof(path) + 20], output[1024];
	int fd = mkstemp(path);
	if (fd < 0) return 1;
	close(fd);
	unsetenv("OPENSC_API_TRACE_FILE");
	sc_pkcs11_api_trace(SC_API_LOGIN, 1, CKR_OK, CKU_USER, 0);
	check("API trace is disabled without its opt-in environment variable",
			read_trace(path, output, sizeof(output)) == 0);
	setenv("OPENSC_API_TRACE_FILE", path, 1);
	errno = EDOM;
	sc_pkcs11_api_trace(SC_API_LOGIN, 1, CKR_OK, CKU_USER, 0);
	int preserved_errno = errno;
	sc_pkcs11_api_trace(SC_API_SIGN_FINAL, 2, CKR_USER_NOT_LOGGED_IN, 1, 0);
	sc_pkcs11_api_trace(SC_API_SIGN_KEY, 2, CKR_OK, 3, 2048);
	read_trace(path, output, sizeof(output));
	check("API trace contains only fixed event names and numeric metadata, preserving errno",
			preserved_errno == EDOM && !strcmp(output,
			"C_Login session=0x1 rv=0x0 a=0x1 b=0x0\n"
			"C_SignFinal session=0x2 rv=0x101 a=0x1 b=0x0\n"
			"SIGN_KEY session=0x2 rv=0x0 a=0x3 b=0x800\n"));
	size_t length = strlen(output);
	sc_pkcs11_api_trace((enum sc_pkcs11_api_event)-1, 0, 0, 0, 0);
	sc_pkcs11_api_trace((enum sc_pkcs11_api_event)999, 0, 0, 0, 0);
	check("unrecognized events cannot append arbitrary diagnostic text",
			read_trace(path, output, sizeof(output)) == length);
	snprintf(invalid_path, sizeof(invalid_path), "%s/missing", path);
	setenv("OPENSC_API_TRACE_FILE", invalid_path, 1);
	errno = EDOM;
	sc_pkcs11_api_trace(SC_API_LOGIN, 1, CKR_OK, CKU_USER, 0);
	check("an unwritable trace does not change the caller's errno", errno == EDOM);
	unsetenv("OPENSC_API_TRACE_FILE");
	unlink(path);
	return failures ? 1 : 0;
}
