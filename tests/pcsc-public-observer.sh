#!/usr/bin/env bash
set -euo pipefail
umask 077
task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
bash "$task_root/scripts/build-pcsc-public-observer.sh"
read -r -a task_pcsc_cflags <<< "$(pkg-config --cflags libpcsclite)"
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror "${task_pcsc_cflags[@]}" \
    "$task_root/tests/pcsc-public-observer.c" -ldl -pthread \
    -o "$task_root/build/diagnostics/pcsc-public-observer-regression"
env -u JCOP4_PUBLIC_DISCOVERY_TRACE "$task_root/build/diagnostics/pcsc-public-observer-regression"

# Verify real ELF interposition against a fake transport; never load PC/SC.
task_tmp=$(mktemp -d)
trap 'rm -rf -- "$task_tmp"' EXIT
cat > "$task_tmp/transport.c" <<'TRANSPORT'
#include <winscard.h>
static int calls;
int fixture_count(void) { return calls; }
LONG SCardTransmit(SCARDHANDLE card, const SCARD_IO_REQUEST *pci,
        const unsigned char *cmd, DWORD size, SCARD_IO_REQUEST *rpci,
        unsigned char *response, DWORD *len)
{
    (void)card; (void)pci; (void)cmd; (void)size; (void)rpci;
    calls++;
    if (*len < 2) return SCARD_E_INSUFFICIENT_BUFFER;
    response[0] = 0x90; response[1] = 0; *len = 2;
    return SCARD_S_SUCCESS;
}
TRANSPORT
cat > "$task_tmp/client.c" <<'CLIENT'
#include <assert.h>
#include <winscard.h>
int fixture_count(void);
int main(void)
{
    const unsigned char version[] = { 0, 0xCA, 1, 0x16, 6 };
    const unsigned char pin[] = { 0, 0x20, 0, 1, 3, 'p', 'i', 'n' };
    unsigned char response[64]; DWORD len = sizeof(response);
    assert(SCardTransmit(7, 0, version, sizeof(version), 0, response, &len) == 0);
    len = sizeof(response);
    assert(SCardTransmit(7, 0, pin, sizeof(pin), 0, response, &len) != 0);
    assert(fixture_count() == 1);
    return 0;
}
CLIENT
"${CC:-cc}" -Wall -Wextra -Werror -fPIC -shared "${task_pcsc_cflags[@]}" \
    "$task_tmp/transport.c" -o "$task_tmp/libfixture.so"
"${CC:-cc}" -Wall -Wextra -Werror "${task_pcsc_cflags[@]}" \
    "$task_tmp/client.c" -L"$task_tmp" -Wl,-rpath,"$task_tmp" -lfixture -o "$task_tmp/client"
env LD_PRELOAD="$task_root/build/diagnostics/pcsc-public-observer.so" \
    JCOP4_PUBLIC_DISCOVERY_TRACE="$task_tmp/headers.txt" "$task_tmp/client"
task_headers=$(< "$task_tmp/headers.txt")
[[ $(stat -c %a "$task_tmp/headers.txt") == 600 &&
   $task_headers == *'public_header=00CA0116'* && $task_headers == *'blocked_header=00200001'* &&
   $task_headers != *'length'* && $task_headers != *'data='* ]]
printf 'PASS public observer interposes the real dynamic call and prevents VERIFY from reaching a fake transport\n'
