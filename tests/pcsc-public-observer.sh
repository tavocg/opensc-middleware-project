#!/usr/bin/env bash
set -euo pipefail
umask 077
unset JCOP4_DIAGNOSTIC_READ4141 JCOP4_DIAGNOSTIC_READ4109 JCOP4_DIAGNOSTIC_AODF JCOP4_DIAGNOSTIC_PRKDF JCOP4_DIAGNOSTIC_PUKDF JCOP4_DIAGNOSTIC_CDF JCOP4_DIAGNOSTIC_PIN_STATUS JCOP4_DIAGNOSTIC_SELECT_DF0F JCOP4_DIAGNOSTIC_READ_DF0F JCOP4_DIAGNOSTIC_CERT_B001 JCOP4_DIAGNOSTIC_CERT_B002
task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
# Dependency failures must stop before compilation, including failed flag lookup.
(
    task_dependency_tmp=$(mktemp -d)
    trap 'rm -rf -- "$task_dependency_tmp"' EXIT
    cat > "$task_dependency_tmp/pkg-config" <<'PKG_CONFIG'
#!/usr/bin/env bash
if [[ ${OBSERVER_DEPENDENCY_FIXTURE:-} == flags && $1 == --exists ]]; then exit 0; fi
exit 1
PKG_CONFIG
    cat > "$task_dependency_tmp/compiler" <<'COMPILER'
#!/usr/bin/env bash
printf 'unexpected compiler call\n' > "$OBSERVER_COMPILER_MARKER"
exit 1
COMPILER
    chmod +x "$task_dependency_tmp/pkg-config" "$task_dependency_tmp/compiler"
    for task_dependency_mode in missing flags; do
        if env PATH="$task_dependency_tmp:$PATH" CC="$task_dependency_tmp/compiler" \
                OBSERVER_DEPENDENCY_FIXTURE="$task_dependency_mode" \
                OBSERVER_COMPILER_MARKER="$task_dependency_tmp/compiler-called" \
                bash "$task_root/scripts/build-pcsc-public-observer.sh" \
                > "$task_dependency_tmp/output" 2>&1; then
            printf 'Dependency fixture unexpectedly compiled: %s\n' "$task_dependency_mode" >&2
            exit 1
        fi
        [[ ! -e $task_dependency_tmp/compiler-called ]]
        if [[ $task_dependency_mode == missing ]]; then
            task_dependency_output=$(< "$task_dependency_tmp/output")
            [[ $task_dependency_output == *'nix develop -c make inspect/jcop4-idopte-discovery'* ]]
        fi
        printf 'PASS observer builder stops before compilation on %s pkg-config dependency\n' "$task_dependency_mode"
    done
)
bash "$task_root/scripts/build-pcsc-public-observer.sh"
read -r -a task_pcsc_cflags <<< "$(pkg-config --cflags libpcsclite)"
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror "${task_pcsc_cflags[@]}" \
    "$task_root/tests/pcsc-public-observer.c" -ldl -pthread \
    -o "$task_root/build/diagnostics/pcsc-public-observer-regression"
task_spy_tmp=$(mktemp -d)
trap 'rm -rf -- "$task_spy_tmp"' EXIT
env -u JCOP4_PUBLIC_DISCOVERY_TRACE -u JCOP4_PUBLIC_SELECT_SPY_TRACE \
    PCSC_SELECT_SPY_FIXTURE="$task_spy_tmp/select.raw" "$task_root/build/diagnostics/pcsc-public-observer-regression"
python3 "$(command -v pcsc-spy)" --nocolor "$task_spy_tmp/select.raw" > "$task_spy_tmp/select.txt" 2>&1
rg -q '00 A4 02 00 02 41 41 00' "$task_spy_tmp/select.txt"
rg -q '86 04 A1 B2 C3 D4' "$task_spy_tmp/select.txt"
[[ $(rg -c '^SCardTransmit' "$task_spy_tmp/select.txt") == 1 ]]
printf 'PASS real pcsc-spy decodes the narrow SELECT stream without a FIFO or hardware\n'
rm -rf -- "$task_spy_tmp"
trap - EXIT

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

# Match Idopte's loading boundary: a client dlopens a plugin with RTLD_LOCAL.
# First reproduce the old RTLD_NEXT failure, then exercise the actual pcsc-lite
# redirector with an observer delegate whose ONLY transport is our fake library.
cat > "$task_tmp/plugin.c" <<'PLUGIN'
#include <assert.h>
#include <winscard.h>
#include <reader.h>
#include <string.h>
int run_fixture(int mode)
{
    const unsigned char version[] = { 0, 0xCA, 1, 0x16, 6 };
    const unsigned char name[] = { 0, 0xCA, 1, 0, 0 };
    const unsigned char blocked_ef[] = { 0, 0xA4, 2, 0, 2, 0xE0, 0 };
    const unsigned char mf[] = { 0, 0xA4, 0, 0, 2, 0x3F, 0 };
    const unsigned char serial[] = { 0, 0xA4, 2, 0, 2, 0xD0, 3 };
    const unsigned char read[] = { 0, 0xB0, 0, 0, 0 };
    const unsigned char app[] = { 0, 0xA4, 4, 0x0C, 15, 0xE8, 0x28, 0xBD, 8, 0x0F, 0xA0, 0, 0, 3, 0x63, 0x64, 0x79, 0x6E, 0x69, 0x64 };
    const unsigned char target[] = { 0, 0xA4, 2, 0, 2, 0x41, 0x41, 0 };
    const unsigned char pin[] = { 0, 0x20, 0, 1, 10, 'S','E','C','R','E','T','_','P','I','N' };
    unsigned char response[402]; DWORD len = sizeof(response), protocol;
    SCARDCONTEXT context; SCARDHANDLE card;
    assert(SCardEstablishContext(SCARD_SCOPE_USER, 0, 0, &context) == 0);
    LONG rv = SCardConnect(context, mode == 1 ? "PRIVATE_READER_CONNECT_FAILURE" : "PRIVATE_READER",
            SCARD_SHARE_SHARED, SCARD_PROTOCOL_T1, &card, &protocol);
    if (mode == 1) { assert(rv == SCARD_E_SHARING_VIOLATION); return 0; }
    if (mode == 18) { assert(rv != 0); return 0; }
    assert(rv == 0 && protocol == SCARD_PROTOCOL_T1);
    assert(SCardReconnect(card, SCARD_SHARE_SHARED, SCARD_PROTOCOL_T1, SCARD_LEAVE_CARD, &protocol) == 0);
    if (mode == 17 || mode == 19 || mode == 20 || mode == 21 || mode == 22 || mode == 23 || mode == 24 || mode == 25 || mode == 26 || mode == 27 || mode == 28) {
        assert(SCardTransmit(card, 0, app, sizeof(app), 0, response, &len) == 0);
        len = sizeof(response);
        assert(SCardTransmit(card, 0, target, sizeof(target), 0, response, &len) == 0);
        len = sizeof(response);
        assert(SCardTransmit(card, 0, read, sizeof(read), 0, response, &len) == 0);
        if (mode == 19 || mode == 20 || mode == 21 || mode == 22 || mode == 23 || mode == 24 || mode == 25 || mode == 26 || mode == 27 || mode == 28) {
            const unsigned char next[] = { 0, 0xA4, 2, 0, 2, 0x41, 0x09, 0 };
            const unsigned char small[] = { 0, 0xB0, 0, 0, 17 };
            len = sizeof(response);
            assert(SCardTransmit(card, 0, next, sizeof(next), 0, response, &len) == 0);
            len = sizeof(response);
            assert(SCardTransmit(card, 0, small, sizeof(small), 0, response, &len) == 0 && len == 19);
        }
        if (mode == 20 || mode == 21 || mode == 22 || mode == 23 || mode == 24 || mode == 25 || mode == 26 || mode == 27 || mode == 28) {
            const unsigned char aodf[] = { 0, 0xA4, 2, 0, 2, 0x70, 1, 0 };
            const unsigned char all[] = { 0, 0xB0, 0, 0, 0, 1, 0x2C };
            len = sizeof(response);
            assert(SCardTransmit(card, 0, aodf, sizeof(aodf), 0, response, &len) == 0);
            len = sizeof(response);
            assert(SCardTransmit(card, 0, all, sizeof(all), 0, response, &len) == 0 && len == 302);
        }
        if (mode == 21 || mode == 22 || mode == 23 || mode == 24 || mode == 25 || mode == 26 || mode == 27 || mode == 28) {
            const unsigned char prkdf[] = { 0, 0xA4, 2, 0, 2, 0x70, 2, 0 };
            len = sizeof(response);
            assert(SCardTransmit(card, 0, prkdf, sizeof(prkdf), 0, response, &len) == 0);
            len = sizeof(response);
            assert(SCardTransmit(card, 0, read, sizeof(read), 0, response, &len) == 0 && len == 258);
        }
        if (mode == 22 || mode == 23 || mode == 24 || mode == 25 || mode == 26 || mode == 27 || mode == 28) {
            const unsigned char pukdf[] = { 0, 0xA4, 2, 0, 2, 0x70, 4, 0 };
            len = sizeof(response);
            assert(SCardTransmit(card, 0, pukdf, sizeof(pukdf), 0, response, &len) == 0);
            len = sizeof(response);
            assert(SCardTransmit(card, 0, read, sizeof(read), 0, response, &len) == 0 && len == 258);
        }
        if (mode == 23 || mode == 24 || mode == 25 || mode == 26 || mode == 27 || mode == 28) {
            const unsigned char cdf[] = { 0, 0xA4, 2, 0, 2, 0x70, 5, 0 };
            len = sizeof(response);
            assert(SCardTransmit(card, 0, cdf, sizeof(cdf), 0, response, &len) == 0);
            len = sizeof(response);
            assert(SCardTransmit(card, 0, read, sizeof(read), 0, response, &len) == 0 && len == 258);
        }
        if (mode == 24 || mode == 25 || mode == 26 || mode == 27 || mode == 28) {
            const unsigned char empty[] = { 0, 0x20, 0, 1 };
            len = sizeof(response);
            assert(SCardTransmit(card, 0, mf, sizeof(mf), 0, response, &len) == 0);
            len = sizeof(response);
            assert(SCardTransmit(card, 0, empty, sizeof(empty), 0, response, &len) == 0 &&
                    len == 2 && response[0] == 0x63 && response[1] == 0xC7);
        }
        if (mode == 25 || mode == 26 || mode == 27 || mode == 28) {
            const unsigned char df0f[] = { 0, 0xA4, 2, 0, 2, 0xDF, 0x0F, 0 };
            len = sizeof(response);
            assert(SCardTransmit(card, 0, app, sizeof(app), 0, response, &len) == 0);
            len = sizeof(response);
            assert(SCardTransmit(card, 0, df0f, sizeof(df0f), 0, response, &len) == 0);
            len = sizeof(response);
            if (mode == 26 || mode == 27 || mode == 28) {
                const unsigned char bounded[] = { 0, 0xB0, 0, 0, 150 };
                assert(SCardTransmit(card, 0, bounded, sizeof(bounded), 0, response, &len) == 0 && len == 152 &&
                        !memcmp(response, "PRIVATE_DF0F_INFO", 17));
            } else {
                assert(SCardTransmit(card, 0, read, sizeof(read), 0, response, &len) != 0 && !len);
            }
        }
        if (mode == 27 || mode == 28) {
            const unsigned char cert[] = { 0, 0xA4, 2, 0, 2, 0xB0, 1, 0 };
            len = sizeof(response); assert(SCardTransmit(card, 0, mf, sizeof(mf), 0, response, &len) == 0);
            len = sizeof(response); assert(SCardTransmit(card, 0, app, sizeof(app), 0, response, &len) == 0);
            len = sizeof(response); assert(SCardTransmit(card, 0, cert, sizeof(cert), 0, response, &len) == 0);
            len = sizeof(response); assert(SCardTransmit(card, 0, read, sizeof(read), 0, response, &len) == 0 && len == 258 &&
                    !memcmp(response, "PRIVATE_CERT_DER", 16));
        }
        if (mode == 28) {
            const unsigned char cert[] = { 0, 0xA4, 2, 0, 2, 0xB0, 2, 0 };
            len = sizeof(response); assert(SCardTransmit(card, 0, cert, sizeof(cert), 0, response, &len) == 0);
            len = sizeof(response); assert(SCardTransmit(card, 0, read, sizeof(read), 0, response, &len) == 0 && len == 258 &&
                    !memcmp(response, "PRIVATE_CERT_DER", 16));
        }
        len = sizeof(response);
        assert(SCardTransmit(card, 0, pin, sizeof(pin), 0, response, &len) != 0 && !len);
        return 0;
    }
    if (mode == 14 || mode == 15 || mode == 16) {
        if (mode != 15) {
            assert(SCardTransmit(card, 0, mode == 14 ? app : mf,
                    mode == 14 ? sizeof(app) : sizeof(mf), 0, response, &len) == 0);
            len = sizeof(response);
        }
        LONG selected = SCardTransmit(card, 0, target, sizeof(target), 0, response, &len);
        assert(mode == 14 ? selected == 0 : selected != 0);
        len = sizeof(response);
        assert(SCardTransmit(card, 0, read, sizeof(read), 0, response, &len) != 0 && len == 0);
        len = sizeof(response);
        assert(SCardTransmit(card, 0, pin, sizeof(pin), 0, response, &len) != 0);
        return 0;
    }
    if (mode == 3) {
        assert(SCardControl(card, 0, pin, sizeof(pin), response, sizeof(response), &len) != 0);
        return 0;
    }
    if (mode == 4) {
        assert(SCardSetAttrib(card, 0, pin, sizeof(pin)) != 0);
        return 0;
    }
    if (mode == 10 || mode == 12 || mode == 13) {
        assert(SCardControl(card, CM_IOCTL_GET_FEATURE_REQUEST, 0, 0,
                response, sizeof(response), &len) == 0 && len == 6 && response[0] == FEATURE_VERIFY_PIN_DIRECT);
        len = sizeof(response);
        if (mode != 10) {
            assert(SCardControl(card, 0x42000006, mode == 12 ? pin : 0,
                    mode == 12 ? sizeof(pin) : 0, response, sizeof(response), &len) != 0 && len == 0);
            len = sizeof(response);
            assert(SCardTransmit(card, 0, version, sizeof(version), 0, response, &len) != 0);
            return 0;
        }
        assert(SCardTransmit(card, 0, version, sizeof(version), 0, response, &len) == 0);
        len = sizeof(response);
        assert(SCardTransmit(card, 0, pin, sizeof(pin), 0, response, &len) != 0);
        return 0;
    }
    if (mode == 11) {
        assert(SCardControl(card, CM_IOCTL_GET_FEATURE_REQUEST,
                mode == 10 ? 0 : pin, mode == 10 ? 0 : sizeof(pin),
                response, sizeof(response), &len) != 0 && len == 0);
        len = sizeof(response);
        assert(SCardTransmit(card, 0, version, sizeof(version), 0, response, &len) != 0);
        return 0;
    }
    if (mode == 8) {
        assert(SCardTransmit(card, 0, blocked_ef, sizeof(blocked_ef), 0, response, &len) != 0);
        len = sizeof(response);
        assert(SCardTransmit(card, 0, version, sizeof(version), 0, response, &len) != 0);
        return 0;
    }
    if (mode == 9) {
        assert(SCardTransmit(card, 0, mf, sizeof(mf), 0, response, &len) == 0);
        len = sizeof(response);
        assert(SCardTransmit(card, 0, serial, sizeof(serial), 0, response, &len) == 0);
        len = sizeof(response);
        assert(SCardTransmit(card, 0, read, sizeof(read), 0, response, &len) == 0);
        len = sizeof(response);
        assert(SCardTransmit(card, 0, pin, sizeof(pin), 0, response, &len) != 0);
        return 0;
    }
    if (mode == 7) {
        assert(SCardTransmit(card, 0, name, sizeof(name), 0, response, &len) == 0);
        len = sizeof(response);
    }
    rv = SCardTransmit(card, 0, version, sizeof(version), 0, response, &len);
    if (mode == 2 || mode == 6) { assert(rv != 0); return 0; }
    assert(rv == 0);
    len = sizeof(response);
    assert(SCardTransmit(card, 0, pin, sizeof(pin), 0, response, &len) != 0 && len == 0);
    len = sizeof(response);
    assert(SCardTransmit(card, 0, version, sizeof(version), 0, response, &len) != 0);
    return 0;
}
PLUGIN
cat > "$task_tmp/local-client.c" <<'LOCAL'
#include <assert.h>
#include <dlfcn.h>
#include <stdlib.h>
int main(int argc, char **argv)
{
    assert(argc == 4);
    int mode = atoi(argv[3]);
    void *plugin = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    assert(plugin);
    int (*run)(int) = (int (*)(int))dlsym(plugin, "run_fixture");
    assert(run && run(mode) == 0);
    void *core = dlopen(argv[2], RTLD_NOW | RTLD_LOCAL);
    assert(core);
    int (*count)(void) = (int (*)(void))dlsym(core, "fixture_count");
    assert(count && count() == (mode == 28 ? 24 : mode == 27 ? 22 : mode == 26 ? 18 : mode == 25 ? 17 : mode == 24 ? 15 : mode == 23 ? 13 : mode == 22 ? 11 : mode == 21 ? 9 : mode == 20 ? 7 : mode == 19 ? 5 : mode == 9 || mode == 17 ? 3 : mode == 7 || mode == 14 ? 2 : mode == 0 || mode == 10 || mode == 16 ? 1 : 0));
    int (*controls)(void) = (int (*)(void))dlsym(core, "fixture_control_count");
    if (controls) assert(controls() == (mode == 10 || mode == 12 || mode == 13 ? 1 : 0));
    return 0;
}
LOCAL
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror -fPIC -shared "${task_pcsc_cflags[@]}" \
    "$task_root/tests/pcsc-delegate-fixture.c" -Wl,-soname,libfakepcsc.so -o "$task_tmp/libfakepcsc.so"
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror "$task_tmp/local-client.c" -ldl -o "$task_tmp/local-client"
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror -fPIC -shared "${task_pcsc_cflags[@]}" \
    "$task_tmp/plugin.c" -L"$task_tmp" -Wl,-rpath,"$task_tmp" -lfakepcsc -o "$task_tmp/direct-plugin.so"
# Compile the original guard shape without a transport dependency. Only APDU
# and control wrappers are preloaded; lifecycle APIs stay in the local plugin.
sed '/^static void log_lifecycle(/,/^static int public_file(/ { /^static int public_file(/!d; }' \
    "$task_root/tools/pcsc-public-observer.c" > "$task_tmp/preload-v1.c"
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror -Wno-unused-variable -fPIC -shared "${task_pcsc_cflags[@]}" \
    "$task_tmp/preload-v1.c" -ldl -pthread -o "$task_tmp/preload-v1.so"
env -u LIBPCSCLITE_DELEGATE -u LIBPCSCLITE_SPY_DELEGATE \
    LD_PRELOAD="$task_tmp/preload-v1.so" JCOP4_PUBLIC_DISCOVERY_TRACE="$task_tmp/scope.txt" \
    "$task_tmp/local-client" "$task_tmp/direct-plugin.so" "$task_tmp/libfakepcsc.so" 2
task_headers=$(< "$task_tmp/scope.txt")
[[ $task_headers == *'public_header=00CA0116'* && $task_headers != *'transport='* ]]
printf 'PASS reproduces missing RTLD_NEXT transport when the original preload observes a locally loaded plugin\n'

task_pcsc_libdir=$(pkg-config --variable=libdir libpcsclite)
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror -fPIC -shared "${task_pcsc_cflags[@]}" \
    "$task_root/tools/pcsc-public-observer.c" -ldl -pthread \
    -Wl,--no-as-needed "$task_tmp/libfakepcsc.so" -Wl,--as-needed -Wl,-rpath,"$task_tmp" \
    -o "$task_tmp/delegate.so"
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror -fPIC -shared "${task_pcsc_cflags[@]}" \
    "$task_tmp/plugin.c" -L"$task_pcsc_libdir" -Wl,-rpath,"$task_pcsc_libdir" \
    -lpcsclite -o "$task_tmp/redirect-plugin.so"
for task_mode in 0 1 3 4 7 8 9 10 11 12 13 14 15 16; do
    env -u LD_PRELOAD -u LIBPCSCLITE_SPY_DELEGATE \
        LIBPCSCLITE_DELEGATE="$task_tmp/delegate.so" JCOP4_PUBLIC_DISCOVERY_TRACE="$task_tmp/delegate-$task_mode.txt" \
        "$task_tmp/local-client" "$task_tmp/redirect-plugin.so" "$task_tmp/libfakepcsc.so" "$task_mode"
    task_headers=$(< "$task_tmp/delegate-$task_mode.txt")
    [[ $(stat -c %a "$task_tmp/delegate-$task_mode.txt") == 600 &&
       $task_headers == *'pcsc_call=SCardEstablishContext'* && $task_headers == *'pcsc_call=SCardConnect'* &&
       $task_headers != *'PRIVATE_'* && $task_headers != *'SECRET_PIN'* &&
       $task_headers != *'length'* && $task_headers != *'data='* ]]
    [[ $task_headers == *'format=public_discovery_headers_v6'* ]]
done
[[ $(< "$task_tmp/delegate-0.txt") == *'public_header=00CA0116'* &&
   $(< "$task_tmp/delegate-0.txt") == *'sw=9000'* &&
   $(< "$task_tmp/delegate-0.txt") == *'blocked_header=00200001'* &&
   $(< "$task_tmp/delegate-1.txt") != *'public_header='* &&
   $(< "$task_tmp/delegate-3.txt") == *'blocked=reader_control'* &&
   $(< "$task_tmp/delegate-4.txt") == *'blocked=reader_attribute'* ]]
[[ $(< "$task_tmp/delegate-7.txt") == *'public_header=00CA0100'* &&
   $(< "$task_tmp/delegate-7.txt") == *'public_header=00CA0116'* &&
   $(< "$task_tmp/delegate-7.txt") == *'blocked_header=00200001'* ]]
[[ $(< "$task_tmp/delegate-8.txt") == *'blocked_header=00A40200'* &&
   $(< "$task_tmp/delegate-8.txt") == *'blocked_select_ef=E000'* &&
   $(< "$task_tmp/delegate-8.txt") != *'public_header='* ]]
[[ $(< "$task_tmp/delegate-9.txt") == *'public_header=00A40000'* &&
   $(< "$task_tmp/delegate-9.txt") == *'public_header=00A40200'* &&
   $(< "$task_tmp/delegate-9.txt") == *'public_header=00B00000'* &&
   $(< "$task_tmp/delegate-9.txt") == *'blocked_header=00200001'* ]]
[[ $(< "$task_tmp/delegate-10.txt") == *'public_control_code=42000D48'* &&
   $(< "$task_tmp/delegate-10.txt") == *'control_transport=0'* &&
   $(< "$task_tmp/delegate-10.txt") == *'blocked_header=00200001'* &&
   $(< "$task_tmp/delegate-11.txt") == *'blocked_control_code=42000D48'* &&
   $(< "$task_tmp/delegate-11.txt") == *'blocked_control_kind=feature_request_unsupported'* &&
   $(< "$task_tmp/delegate-11.txt") != *'public_header='* ]]
for task_mode in 12 13; do
    [[ $(< "$task_tmp/delegate-$task_mode.txt") == *'public_control_code=42000D48'* &&
       $(< "$task_tmp/delegate-$task_mode.txt") == *'blocked_control_code=42000006'* &&
       $(< "$task_tmp/delegate-$task_mode.txt") != *'public_header='* ]]
done
[[ $(< "$task_tmp/delegate-14.txt") == *'public_select=known_4141_metadata_only'* &&
   $(< "$task_tmp/delegate-14.txt") == *'blocked_header=00B00000'* &&
   $(< "$task_tmp/delegate-14.txt") != *'public_header=00B00000'* &&
   $(< "$task_tmp/delegate-15.txt") == *'blocked_select_ef=4141'* &&
   $(< "$task_tmp/delegate-15.txt") != *'public_select='* &&
   $(< "$task_tmp/delegate-16.txt") == *'blocked_select_ef=4141'* &&
   $(< "$task_tmp/delegate-16.txt") == *'public_select=known_master_file'* &&
   $(< "$task_tmp/delegate-16.txt") != *'public_select=known_4141_metadata_only'* ]]
printf 'PASS actual PC/SC delegation permits only known-ADF SELECT 4141 and blocks its READ before fake transport\n'
env -u LD_PRELOAD -u LIBPCSCLITE_SPY_DELEGATE -u JCOP4_PUBLIC_SELECT_SPY_TRACE \
    LIBPCSCLITE_DELEGATE="$task_tmp/delegate.so" JCOP4_PUBLIC_DISCOVERY_TRACE="$task_tmp/compare.txt" \
    JCOP4_DIAGNOSTIC_READ4141=1 \
    "$task_tmp/local-client" "$task_tmp/redirect-plugin.so" "$task_tmp/libfakepcsc.so" 17
task_headers=$(< "$task_tmp/compare.txt")
[[ $task_headers == *'comparison=bounded_read4141_contents_omitted'* &&
   $task_headers == *'diagnostic_read=known_4141_contents_omitted'* &&
   $task_headers == *'blocked_header=00200001'* && $task_headers != *'PRIVATE_'* && $task_headers != *'SECRET_PIN'* ]]
printf 'PASS opt-in comparison uses real PC/SC delegation for bounded READ while VERIFY and payload logging remain blocked\n'
env -u LD_PRELOAD -u LIBPCSCLITE_SPY_DELEGATE -u JCOP4_PUBLIC_SELECT_SPY_TRACE \
    LIBPCSCLITE_DELEGATE="$task_tmp/delegate.so" JCOP4_PUBLIC_DISCOVERY_TRACE="$task_tmp/read4109.txt" \
    JCOP4_DIAGNOSTIC_READ4141=1 JCOP4_DIAGNOSTIC_READ4109=1 \
    "$task_tmp/local-client" "$task_tmp/redirect-plugin.so" "$task_tmp/libfakepcsc.so" 19
task_headers=$(< "$task_tmp/read4109.txt")
[[ $task_headers == *'comparison_extension=bounded_read4109_contents_omitted'* &&
   $task_headers == *'diagnostic_read=known_4109_contents_omitted'* &&
   $task_headers == *'blocked_header=00200001'* && $task_headers != *'PRIVATE_'* ]]
printf 'PASS actual delegation returns the 17-byte file only to its caller and blocks VERIFY after both diagnostic reads\n'
env -u LD_PRELOAD -u LIBPCSCLITE_SPY_DELEGATE -u JCOP4_PUBLIC_SELECT_SPY_TRACE \
    LIBPCSCLITE_DELEGATE="$task_tmp/delegate.so" JCOP4_PUBLIC_DISCOVERY_TRACE="$task_tmp/aodf.txt" \
    JCOP4_DIAGNOSTIC_READ4141=1 JCOP4_DIAGNOSTIC_READ4109=1 JCOP4_DIAGNOSTIC_AODF=1 \
    "$task_tmp/local-client" "$task_tmp/redirect-plugin.so" "$task_tmp/libfakepcsc.so" 20
task_headers=$(< "$task_tmp/aodf.txt")
[[ $task_headers == *'comparison_directory=bounded_aodf7001_contents_omitted'* &&
   $task_headers == *'diagnostic_read=known_7001_contents_omitted'* &&
   $task_headers == *'blocked_header=00200001'* && $task_headers != *'PRIVATE_'* ]]
printf 'PASS actual delegation reads known AODF with its 300-byte budget and blocks PIN without logging metadata contents\n'
env -u LD_PRELOAD -u LIBPCSCLITE_SPY_DELEGATE -u JCOP4_PUBLIC_SELECT_SPY_TRACE \
    LIBPCSCLITE_DELEGATE="$task_tmp/delegate.so" JCOP4_PUBLIC_DISCOVERY_TRACE="$task_tmp/prkdf.txt" \
    JCOP4_DIAGNOSTIC_READ4141=1 JCOP4_DIAGNOSTIC_READ4109=1 JCOP4_DIAGNOSTIC_AODF=1 JCOP4_DIAGNOSTIC_PRKDF=1 \
    "$task_tmp/local-client" "$task_tmp/redirect-plugin.so" "$task_tmp/libfakepcsc.so" 21
task_headers=$(< "$task_tmp/prkdf.txt")
[[ $task_headers == *'comparison_keys=bounded_prkdf7002_contents_omitted'* &&
   $task_headers == *'diagnostic_read=known_7002_contents_omitted'* &&
   $task_headers == *'blocked_header=00200001'* && $task_headers != *'PRIVATE_'* ]]
printf 'PASS actual delegation reads bounded native-key directory metadata and blocks PIN without retaining contents\n'
env -u LD_PRELOAD -u LIBPCSCLITE_SPY_DELEGATE -u JCOP4_PUBLIC_SELECT_SPY_TRACE \
    LIBPCSCLITE_DELEGATE="$task_tmp/delegate.so" JCOP4_PUBLIC_DISCOVERY_TRACE="$task_tmp/pukdf.txt" \
    JCOP4_DIAGNOSTIC_READ4141=1 JCOP4_DIAGNOSTIC_READ4109=1 JCOP4_DIAGNOSTIC_AODF=1 JCOP4_DIAGNOSTIC_PRKDF=1 JCOP4_DIAGNOSTIC_PUKDF=1 \
    "$task_tmp/local-client" "$task_tmp/redirect-plugin.so" "$task_tmp/libfakepcsc.so" 22
task_headers=$(< "$task_tmp/pukdf.txt")
[[ $task_headers == *'comparison_public_keys=bounded_pukdf7004_contents_omitted'* &&
   $task_headers == *'diagnostic_read=known_7004_contents_omitted'* &&
   $task_headers == *'blocked_header=00200001'* && $task_headers != *'PRIVATE_'* ]]
printf 'PASS actual delegation returns bounded PuKDF metadata only to its caller and stops PIN before fake transport\n'
env -u LD_PRELOAD -u LIBPCSCLITE_SPY_DELEGATE -u JCOP4_PUBLIC_SELECT_SPY_TRACE \
    LIBPCSCLITE_DELEGATE="$task_tmp/delegate.so" JCOP4_PUBLIC_DISCOVERY_TRACE="$task_tmp/cdf.txt" \
    JCOP4_DIAGNOSTIC_READ4141=1 JCOP4_DIAGNOSTIC_READ4109=1 JCOP4_DIAGNOSTIC_AODF=1 JCOP4_DIAGNOSTIC_PRKDF=1 JCOP4_DIAGNOSTIC_PUKDF=1 JCOP4_DIAGNOSTIC_CDF=1 \
    "$task_tmp/local-client" "$task_tmp/redirect-plugin.so" "$task_tmp/libfakepcsc.so" 23
task_headers=$(< "$task_tmp/cdf.txt")
[[ $task_headers == *'comparison_certificates=bounded_cdf7005_contents_omitted'* &&
   $task_headers == *'diagnostic_read=known_7005_contents_omitted'* &&
   $task_headers == *'blocked_header=00200001'* && $task_headers != *'PRIVATE_'* ]]
printf 'PASS actual delegation returns CDF data only to caller and blocks VERIFY before fake transport\n'
env -u LD_PRELOAD -u LIBPCSCLITE_SPY_DELEGATE -u JCOP4_PUBLIC_SELECT_SPY_TRACE \
    LIBPCSCLITE_DELEGATE="$task_tmp/delegate.so" JCOP4_PUBLIC_DISCOVERY_TRACE="$task_tmp/status.txt" \
    JCOP4_DIAGNOSTIC_READ4141=1 JCOP4_DIAGNOSTIC_READ4109=1 JCOP4_DIAGNOSTIC_AODF=1 JCOP4_DIAGNOSTIC_PRKDF=1 JCOP4_DIAGNOSTIC_PUKDF=1 JCOP4_DIAGNOSTIC_CDF=1 JCOP4_DIAGNOSTIC_PIN_STATUS=1 \
    "$task_tmp/local-client" "$task_tmp/redirect-plugin.so" "$task_tmp/libfakepcsc.so" 24
task_headers=$(< "$task_tmp/status.txt")
[[ $task_headers == *'diagnostic_pin_status=mf_pin1_case1_no_data'* &&
   $task_headers == *'sw=63C7'* && $task_headers == *'blocked_verify_form=other_contents_omitted'* && $task_headers != *'PRIVATE_'* ]]
printf 'PASS actual delegation transmits one SW-only status request but no credential-bearing VERIFY\n'
env -u LD_PRELOAD -u LIBPCSCLITE_SPY_DELEGATE -u JCOP4_PUBLIC_SELECT_SPY_TRACE \
    LIBPCSCLITE_DELEGATE="$task_tmp/delegate.so" JCOP4_PUBLIC_DISCOVERY_TRACE="$task_tmp/df0f.txt" \
    JCOP4_DIAGNOSTIC_READ4141=1 JCOP4_DIAGNOSTIC_READ4109=1 JCOP4_DIAGNOSTIC_AODF=1 JCOP4_DIAGNOSTIC_PRKDF=1 JCOP4_DIAGNOSTIC_PUKDF=1 JCOP4_DIAGNOSTIC_CDF=1 JCOP4_DIAGNOSTIC_PIN_STATUS=1 JCOP4_DIAGNOSTIC_SELECT_DF0F=1 \
    "$task_tmp/local-client" "$task_tmp/redirect-plugin.so" "$task_tmp/libfakepcsc.so" 25
task_headers=$(< "$task_tmp/df0f.txt")
[[ $task_headers == *'public_select=known_df0f_metadata_only'* &&
   $task_headers == *'blocked_header=00B00000'* && $task_headers != *'PRIVATE_'* ]]
printf 'PASS actual delegation permits SELECT DF0F after MF status/ADF and blocks its READ before fake transport\n'
env -u LD_PRELOAD -u LIBPCSCLITE_SPY_DELEGATE -u JCOP4_PUBLIC_SELECT_SPY_TRACE \
    LIBPCSCLITE_DELEGATE="$task_tmp/delegate.so" JCOP4_PUBLIC_DISCOVERY_TRACE="$task_tmp/read-df0f.txt" \
    JCOP4_DIAGNOSTIC_READ4141=1 JCOP4_DIAGNOSTIC_READ4109=1 JCOP4_DIAGNOSTIC_AODF=1 JCOP4_DIAGNOSTIC_PRKDF=1 JCOP4_DIAGNOSTIC_PUKDF=1 JCOP4_DIAGNOSTIC_CDF=1 JCOP4_DIAGNOSTIC_PIN_STATUS=1 JCOP4_DIAGNOSTIC_SELECT_DF0F=1 JCOP4_DIAGNOSTIC_READ_DF0F=1 \
    "$task_tmp/local-client" "$task_tmp/redirect-plugin.so" "$task_tmp/libfakepcsc.so" 26
task_headers=$(< "$task_tmp/read-df0f.txt")
[[ $task_headers == *'comparison_df0f_read=once150_contents_omitted'* &&
   $task_headers == *'diagnostic_read=known_DF0F_contents_omitted'* &&
   $task_headers == *'blocked_header=00200001'* && $task_headers != *'PRIVATE_'* ]]
printf 'PASS actual delegation returns DF0F data only to caller and blocks credentials before fake transport\n'
env -u LD_PRELOAD -u LIBPCSCLITE_SPY_DELEGATE -u JCOP4_PUBLIC_SELECT_SPY_TRACE \
    LIBPCSCLITE_DELEGATE="$task_tmp/delegate.so" JCOP4_PUBLIC_DISCOVERY_TRACE="$task_tmp/b001.txt" \
    JCOP4_DIAGNOSTIC_READ4141=1 JCOP4_DIAGNOSTIC_READ4109=1 JCOP4_DIAGNOSTIC_AODF=1 JCOP4_DIAGNOSTIC_PRKDF=1 JCOP4_DIAGNOSTIC_PUKDF=1 JCOP4_DIAGNOSTIC_CDF=1 JCOP4_DIAGNOSTIC_PIN_STATUS=1 JCOP4_DIAGNOSTIC_SELECT_DF0F=1 JCOP4_DIAGNOSTIC_READ_DF0F=1 JCOP4_DIAGNOSTIC_CERT_B001=1 \
    "$task_tmp/local-client" "$task_tmp/redirect-plugin.so" "$task_tmp/libfakepcsc.so" 27
task_headers=$(< "$task_tmp/b001.txt")
[[ $task_headers == *'diagnostic_read=known_B001_contents_omitted'* &&
   $task_headers == *'public_select=known_b001_bounded_read_comparison'* &&
   $task_headers == *'blocked_header=00200001'* && $task_headers != *'PRIVATE_'* ]]
printf 'PASS actual delegation returns B001 data only to caller and blocks credentials before fake transport\n'
for task_b001_flag in invalid missing_parent; do
    task_b001_env=("JCOP4_DIAGNOSTIC_CERT_B001=invalid" "JCOP4_DIAGNOSTIC_READ_DF0F=1")
    if [[ $task_b001_flag == missing_parent ]]; then task_b001_env=("JCOP4_DIAGNOSTIC_CERT_B001=1"); fi
    env -u JCOP4_DIAGNOSTIC_READ_DF0F -u LD_PRELOAD -u LIBPCSCLITE_SPY_DELEGATE \
        LIBPCSCLITE_DELEGATE="$task_tmp/delegate.so" JCOP4_PUBLIC_DISCOVERY_TRACE="$task_tmp/b001-$task_b001_flag.txt" \
        JCOP4_DIAGNOSTIC_READ4141=1 JCOP4_DIAGNOSTIC_READ4109=1 JCOP4_DIAGNOSTIC_AODF=1 JCOP4_DIAGNOSTIC_PRKDF=1 JCOP4_DIAGNOSTIC_PUKDF=1 JCOP4_DIAGNOSTIC_CDF=1 JCOP4_DIAGNOSTIC_PIN_STATUS=1 JCOP4_DIAGNOSTIC_SELECT_DF0F=1 "${task_b001_env[@]}" \
        "$task_tmp/local-client" "$task_tmp/redirect-plugin.so" "$task_tmp/libfakepcsc.so" 6
    [[ $(< "$task_tmp/b001-$task_b001_flag.txt") != *'public_header='* ]]
    printf 'PASS B001 comparison rejects %s configuration before transport\n' "$task_b001_flag"
done
env -u LD_PRELOAD -u LIBPCSCLITE_SPY_DELEGATE -u JCOP4_PUBLIC_SELECT_SPY_TRACE \
    LIBPCSCLITE_DELEGATE="$task_tmp/delegate.so" JCOP4_PUBLIC_DISCOVERY_TRACE="$task_tmp/b002.txt" \
    JCOP4_DIAGNOSTIC_READ4141=1 JCOP4_DIAGNOSTIC_READ4109=1 JCOP4_DIAGNOSTIC_AODF=1 JCOP4_DIAGNOSTIC_PRKDF=1 JCOP4_DIAGNOSTIC_PUKDF=1 JCOP4_DIAGNOSTIC_CDF=1 JCOP4_DIAGNOSTIC_PIN_STATUS=1 JCOP4_DIAGNOSTIC_SELECT_DF0F=1 JCOP4_DIAGNOSTIC_READ_DF0F=1 JCOP4_DIAGNOSTIC_CERT_B001=1 JCOP4_DIAGNOSTIC_CERT_B002=1 \
    "$task_tmp/local-client" "$task_tmp/redirect-plugin.so" "$task_tmp/libfakepcsc.so" 28
task_headers=$(< "$task_tmp/b002.txt")
[[ $task_headers == *'diagnostic_read=known_B002_contents_omitted'* &&
   $task_headers == *'public_select=known_b002_bounded_read_comparison'* &&
   $task_headers == *'blocked_header=00200001'* && $task_headers != *'PRIVATE_'* ]]
printf 'PASS actual delegation returns B002 data only to caller and blocks credentials before fake transport\n'
for task_b002_flag in invalid missing_parent; do
    task_b002_env=("JCOP4_DIAGNOSTIC_CERT_B002=invalid" "JCOP4_DIAGNOSTIC_CERT_B001=1")
    if [[ $task_b002_flag == missing_parent ]]; then task_b002_env=("JCOP4_DIAGNOSTIC_CERT_B002=1"); fi
    env -u JCOP4_DIAGNOSTIC_CERT_B001 -u LD_PRELOAD -u LIBPCSCLITE_SPY_DELEGATE \
        LIBPCSCLITE_DELEGATE="$task_tmp/delegate.so" JCOP4_PUBLIC_DISCOVERY_TRACE="$task_tmp/b002-$task_b002_flag.txt" \
        JCOP4_DIAGNOSTIC_READ4141=1 JCOP4_DIAGNOSTIC_READ4109=1 JCOP4_DIAGNOSTIC_AODF=1 JCOP4_DIAGNOSTIC_PRKDF=1 JCOP4_DIAGNOSTIC_PUKDF=1 JCOP4_DIAGNOSTIC_CDF=1 JCOP4_DIAGNOSTIC_PIN_STATUS=1 JCOP4_DIAGNOSTIC_SELECT_DF0F=1 JCOP4_DIAGNOSTIC_READ_DF0F=1 "${task_b002_env[@]}" \
        "$task_tmp/local-client" "$task_tmp/redirect-plugin.so" "$task_tmp/libfakepcsc.so" 6
    [[ $(< "$task_tmp/b002-$task_b002_flag.txt") != *'public_header='* ]]
    printf 'PASS B002 comparison rejects %s configuration before transport\n' "$task_b002_flag"
done
for task_read_df0f_flag in invalid missing_parent; do
    task_read_df0f_env=("JCOP4_DIAGNOSTIC_READ_DF0F=invalid" "JCOP4_DIAGNOSTIC_SELECT_DF0F=1")
    if [[ $task_read_df0f_flag == missing_parent ]]; then task_read_df0f_env=("JCOP4_DIAGNOSTIC_READ_DF0F=1"); fi
    env -u JCOP4_DIAGNOSTIC_SELECT_DF0F -u LD_PRELOAD -u LIBPCSCLITE_SPY_DELEGATE \
        LIBPCSCLITE_DELEGATE="$task_tmp/delegate.so" JCOP4_PUBLIC_DISCOVERY_TRACE="$task_tmp/read-df0f-$task_read_df0f_flag.txt" \
        JCOP4_DIAGNOSTIC_READ4141=1 JCOP4_DIAGNOSTIC_READ4109=1 JCOP4_DIAGNOSTIC_AODF=1 JCOP4_DIAGNOSTIC_PRKDF=1 JCOP4_DIAGNOSTIC_PUKDF=1 JCOP4_DIAGNOSTIC_CDF=1 JCOP4_DIAGNOSTIC_PIN_STATUS=1 "${task_read_df0f_env[@]}" \
        "$task_tmp/local-client" "$task_tmp/redirect-plugin.so" "$task_tmp/libfakepcsc.so" 6
    [[ $(< "$task_tmp/read-df0f-$task_read_df0f_flag.txt") != *'public_header='* ]]
    printf 'PASS bounded DF0F READ rejects %s configuration before transport\n' "$task_read_df0f_flag"
done
for task_df0f_flag in invalid missing_parent; do
    task_df0f_env=("JCOP4_DIAGNOSTIC_SELECT_DF0F=invalid" "JCOP4_DIAGNOSTIC_PIN_STATUS=1")
    if [[ $task_df0f_flag == missing_parent ]]; then task_df0f_env=("JCOP4_DIAGNOSTIC_SELECT_DF0F=1"); fi
    env -u JCOP4_DIAGNOSTIC_PIN_STATUS -u LD_PRELOAD -u LIBPCSCLITE_SPY_DELEGATE \
        LIBPCSCLITE_DELEGATE="$task_tmp/delegate.so" JCOP4_PUBLIC_DISCOVERY_TRACE="$task_tmp/df0f-$task_df0f_flag.txt" \
        JCOP4_DIAGNOSTIC_READ4141=1 JCOP4_DIAGNOSTIC_READ4109=1 JCOP4_DIAGNOSTIC_AODF=1 JCOP4_DIAGNOSTIC_PRKDF=1 JCOP4_DIAGNOSTIC_PUKDF=1 JCOP4_DIAGNOSTIC_CDF=1 "${task_df0f_env[@]}" \
        "$task_tmp/local-client" "$task_tmp/redirect-plugin.so" "$task_tmp/libfakepcsc.so" 6
    [[ $(< "$task_tmp/df0f-$task_df0f_flag.txt") != *'public_header='* ]]
    printf 'PASS DF0F SELECT-only mode rejects %s configuration before transport\n' "$task_df0f_flag"
done
for task_status_flag in invalid missing_parent; do
    task_status_env=("JCOP4_DIAGNOSTIC_PIN_STATUS=invalid" "JCOP4_DIAGNOSTIC_CDF=1")
    if [[ $task_status_flag == missing_parent ]]; then task_status_env=("JCOP4_DIAGNOSTIC_PIN_STATUS=1"); fi
    env -u JCOP4_DIAGNOSTIC_CDF -u LD_PRELOAD -u LIBPCSCLITE_SPY_DELEGATE \
        LIBPCSCLITE_DELEGATE="$task_tmp/delegate.so" JCOP4_PUBLIC_DISCOVERY_TRACE="$task_tmp/status-$task_status_flag.txt" \
        JCOP4_DIAGNOSTIC_READ4141=1 JCOP4_DIAGNOSTIC_READ4109=1 JCOP4_DIAGNOSTIC_AODF=1 JCOP4_DIAGNOSTIC_PRKDF=1 JCOP4_DIAGNOSTIC_PUKDF=1 "${task_status_env[@]}" \
        "$task_tmp/local-client" "$task_tmp/redirect-plugin.so" "$task_tmp/libfakepcsc.so" 6
    [[ $(< "$task_tmp/status-$task_status_flag.txt") != *'public_header='* ]]
    printf 'PASS PIN-status mode rejects %s configuration before transport\n' "$task_status_flag"
done
for task_cdf_flag in invalid missing_parent; do
    task_cdf_env=("JCOP4_DIAGNOSTIC_CDF=invalid" "JCOP4_DIAGNOSTIC_PUKDF=1")
    if [[ $task_cdf_flag == missing_parent ]]; then task_cdf_env=("JCOP4_DIAGNOSTIC_CDF=1"); fi
    env -u JCOP4_DIAGNOSTIC_PUKDF -u LD_PRELOAD -u LIBPCSCLITE_SPY_DELEGATE \
        LIBPCSCLITE_DELEGATE="$task_tmp/delegate.so" JCOP4_PUBLIC_DISCOVERY_TRACE="$task_tmp/cdf-$task_cdf_flag.txt" \
        JCOP4_DIAGNOSTIC_READ4141=1 JCOP4_DIAGNOSTIC_READ4109=1 JCOP4_DIAGNOSTIC_AODF=1 JCOP4_DIAGNOSTIC_PRKDF=1 "${task_cdf_env[@]}" \
        "$task_tmp/local-client" "$task_tmp/redirect-plugin.so" "$task_tmp/libfakepcsc.so" 6
    [[ $(< "$task_tmp/cdf-$task_cdf_flag.txt") != *'public_header='* ]]
    printf 'PASS CDF rejects %s configuration before transport\n' "$task_cdf_flag"
done
for task_pukdf_flag in invalid missing_parent; do
    task_pukdf_env=("JCOP4_DIAGNOSTIC_PUKDF=invalid" "JCOP4_DIAGNOSTIC_PRKDF=1")
    if [[ $task_pukdf_flag == missing_parent ]]; then task_pukdf_env=("JCOP4_DIAGNOSTIC_PUKDF=1"); fi
    env -u JCOP4_DIAGNOSTIC_PRKDF -u LD_PRELOAD -u LIBPCSCLITE_SPY_DELEGATE \
        LIBPCSCLITE_DELEGATE="$task_tmp/delegate.so" JCOP4_PUBLIC_DISCOVERY_TRACE="$task_tmp/pukdf-$task_pukdf_flag.txt" \
        JCOP4_DIAGNOSTIC_READ4141=1 JCOP4_DIAGNOSTIC_READ4109=1 JCOP4_DIAGNOSTIC_AODF=1 "${task_pukdf_env[@]}" \
        "$task_tmp/local-client" "$task_tmp/redirect-plugin.so" "$task_tmp/libfakepcsc.so" 6
    [[ $(< "$task_tmp/pukdf-$task_pukdf_flag.txt") != *'public_header='* ]]
    printf 'PASS PuKDF rejects %s configuration before transport\n' "$task_pukdf_flag"
done
for task_prkdf_flag in invalid missing_parent; do
    task_prkdf_env=("JCOP4_DIAGNOSTIC_PRKDF=invalid" "JCOP4_DIAGNOSTIC_AODF=1")
    if [[ $task_prkdf_flag == missing_parent ]]; then task_prkdf_env=("JCOP4_DIAGNOSTIC_PRKDF=1"); fi
    env -u JCOP4_DIAGNOSTIC_AODF -u LD_PRELOAD -u LIBPCSCLITE_SPY_DELEGATE \
        LIBPCSCLITE_DELEGATE="$task_tmp/delegate.so" JCOP4_PUBLIC_DISCOVERY_TRACE="$task_tmp/prkdf-$task_prkdf_flag.txt" \
        JCOP4_DIAGNOSTIC_READ4141=1 JCOP4_DIAGNOSTIC_READ4109=1 "${task_prkdf_env[@]}" \
        "$task_tmp/local-client" "$task_tmp/redirect-plugin.so" "$task_tmp/libfakepcsc.so" 6
    [[ $(< "$task_tmp/prkdf-$task_prkdf_flag.txt") != *'public_header='* ]]
    printf 'PASS PrKDF rejects %s configuration before transport\n' "$task_prkdf_flag"
done
for task_aodf_flag in invalid missing_parent; do
    task_aodf_env=("JCOP4_DIAGNOSTIC_AODF=invalid" "JCOP4_DIAGNOSTIC_READ4109=1")
    if [[ $task_aodf_flag == missing_parent ]]; then task_aodf_env=("JCOP4_DIAGNOSTIC_AODF=1"); fi
    env -u JCOP4_DIAGNOSTIC_READ4109 -u LD_PRELOAD -u LIBPCSCLITE_SPY_DELEGATE \
        LIBPCSCLITE_DELEGATE="$task_tmp/delegate.so" JCOP4_PUBLIC_DISCOVERY_TRACE="$task_tmp/aodf-$task_aodf_flag.txt" \
        JCOP4_DIAGNOSTIC_READ4141=1 "${task_aodf_env[@]}" \
        "$task_tmp/local-client" "$task_tmp/redirect-plugin.so" "$task_tmp/libfakepcsc.so" 6
    [[ $(< "$task_tmp/aodf-$task_aodf_flag.txt") != *'public_header='* ]]
    printf 'PASS AODF rejects %s configuration before transport\n' "$task_aodf_flag"
done
for task_flag_case in invalid missing_parent mixed; do
    task_flag_env=("JCOP4_DIAGNOSTIC_READ4109=invalid" "JCOP4_DIAGNOSTIC_READ4141=1")
    if [[ $task_flag_case == missing_parent ]]; then task_flag_env=("JCOP4_DIAGNOSTIC_READ4109=1"); fi
    if [[ $task_flag_case == mixed ]]; then
        task_flag_env=("JCOP4_DIAGNOSTIC_READ4109=1" "JCOP4_DIAGNOSTIC_READ4141=1" "JCOP4_PUBLIC_SELECT_SPY_TRACE=$task_tmp/read4109-mixed.raw")
    fi
    env -u JCOP4_DIAGNOSTIC_READ4141 -u LD_PRELOAD -u LIBPCSCLITE_SPY_DELEGATE \
        LIBPCSCLITE_DELEGATE="$task_tmp/delegate.so" JCOP4_PUBLIC_DISCOVERY_TRACE="$task_tmp/read4109-$task_flag_case.txt" \
        "${task_flag_env[@]}" "$task_tmp/local-client" "$task_tmp/redirect-plugin.so" "$task_tmp/libfakepcsc.so" 6
    [[ $(< "$task_tmp/read4109-$task_flag_case.txt") != *'public_header='* ]]
    printf 'PASS READ4109 rejects %s configuration before transport\n' "$task_flag_case"
done
env -u LD_PRELOAD -u LIBPCSCLITE_SPY_DELEGATE -u JCOP4_PUBLIC_SELECT_SPY_TRACE \
    LIBPCSCLITE_DELEGATE="$task_tmp/delegate.so" JCOP4_PUBLIC_DISCOVERY_TRACE="$task_tmp/bad-atr.txt" \
    JCOP4_DIAGNOSTIC_READ4141=1 FIXTURE_COMPARISON_BAD_ATR=1 \
    "$task_tmp/local-client" "$task_tmp/redirect-plugin.so" "$task_tmp/libfakepcsc.so" 18
[[ $(< "$task_tmp/bad-atr.txt") == *'comparison_atr=unavailable_or_mismatch'* &&
   $(< "$task_tmp/bad-atr.txt") != *'public_header='* ]]
printf 'PASS comparison checks the connected ATR privately and stops a different card before any APDU\n'
for task_invalid in invalid mixed; do
    task_guard_env=("JCOP4_DIAGNOSTIC_READ4141=invalid")
    if [[ $task_invalid == mixed ]]; then
        task_guard_env=("JCOP4_DIAGNOSTIC_READ4141=1" "JCOP4_PUBLIC_SELECT_SPY_TRACE=$task_tmp/mixed.raw")
    fi
    env -u LD_PRELOAD -u LIBPCSCLITE_SPY_DELEGATE \
        LIBPCSCLITE_DELEGATE="$task_tmp/delegate.so" JCOP4_PUBLIC_DISCOVERY_TRACE="$task_tmp/$task_invalid.txt" \
        "${task_guard_env[@]}" \
        "$task_tmp/local-client" "$task_tmp/redirect-plugin.so" "$task_tmp/libfakepcsc.so" 6
    [[ $(< "$task_tmp/$task_invalid.txt") != *'public_header='* ]]
    printf 'PASS comparison rejects %s opt-in before any fake transport\n' "$task_invalid"
done
env -u LD_PRELOAD -u JCOP4_PUBLIC_DISCOVERY_TRACE -u LIBPCSCLITE_SPY_DELEGATE \
    LIBPCSCLITE_DELEGATE="$task_tmp/delegate.so" \
    "$task_tmp/local-client" "$task_tmp/redirect-plugin.so" "$task_tmp/libfakepcsc.so" 6
# pkcs11-tool can also have the redirector in the executable's global scope.
"${CC:-cc}" -std=c99 -Wall -Wextra -Werror "$task_tmp/local-client.c" -ldl \
    -L"$task_pcsc_libdir" -Wl,-rpath,"$task_pcsc_libdir" -Wl,--no-as-needed -lpcsclite \
    -o "$task_tmp/global-client"
env -u LD_PRELOAD -u LIBPCSCLITE_SPY_DELEGATE \
    LIBPCSCLITE_DELEGATE="$task_tmp/delegate.so" JCOP4_PUBLIC_DISCOVERY_TRACE="$task_tmp/global.txt" \
    "$task_tmp/global-client" "$task_tmp/redirect-plugin.so" "$task_tmp/libfakepcsc.so" 0
printf 'PASS actual PC/SC delegation handles local plugins, records connection failures privately and blocks VERIFY, controls and attributes before fake transport\n'
env -u LD_PRELOAD -u LIBPCSCLITE_SPY_DELEGATE \
    LIBPCSCLITE_DELEGATE="$task_tmp/delegate.so" JCOP4_PUBLIC_DISCOVERY_TRACE="$task_tmp/spy-headers.txt" \
    JCOP4_PUBLIC_SELECT_SPY_TRACE="$task_tmp/spy-select.raw" \
    "$task_tmp/local-client" "$task_tmp/redirect-plugin.so" "$task_tmp/libfakepcsc.so" 14
python3 "$(command -v pcsc-spy)" --nocolor "$task_tmp/spy-select.raw" > "$task_tmp/spy-select.txt" 2>&1
[[ $(stat -c %a "$task_tmp/spy-select.raw") == 600 &&
   $(rg -c '^SCardTransmit' "$task_tmp/spy-select.txt") == 1 &&
   $(< "$task_tmp/spy-headers.txt") == *'blocked_header=00B00000'* ]]
rg -q '00 A4 02 00 02 41 41' "$task_tmp/spy-select.txt"
! rg -q '00 B0|00 20|PRIVATE_RESPONSE|SECRET_PIN' "$task_tmp/spy-select.raw"
printf 'PASS actual redirector captures only SELECT for real pcsc-spy while blocked READ never reaches the fake transport\n'
