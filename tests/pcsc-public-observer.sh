#!/usr/bin/env bash
set -euo pipefail
umask 077
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

# Match Idopte's loading boundary: a client dlopens a plugin with RTLD_LOCAL.
# First reproduce the old RTLD_NEXT failure, then exercise the actual pcsc-lite
# redirector with an observer delegate whose ONLY transport is our fake library.
cat > "$task_tmp/plugin.c" <<'PLUGIN'
#include <assert.h>
#include <winscard.h>
#include <reader.h>
int run_fixture(int mode)
{
    const unsigned char version[] = { 0, 0xCA, 1, 0x16, 6 };
    const unsigned char name[] = { 0, 0xCA, 1, 0, 0 };
    const unsigned char blocked_ef[] = { 0, 0xA4, 2, 0, 2, 0xE0, 0 };
    const unsigned char mf[] = { 0, 0xA4, 0, 0, 2, 0x3F, 0 };
    const unsigned char serial[] = { 0, 0xA4, 2, 0, 2, 0xD0, 3 };
    const unsigned char read[] = { 0, 0xB0, 0, 0, 0 };
    const unsigned char pin[] = { 0, 0x20, 0, 1, 10, 'S','E','C','R','E','T','_','P','I','N' };
    unsigned char response[64]; DWORD len = sizeof(response), protocol;
    SCARDCONTEXT context; SCARDHANDLE card;
    assert(SCardEstablishContext(SCARD_SCOPE_USER, 0, 0, &context) == 0);
    LONG rv = SCardConnect(context, mode == 1 ? "PRIVATE_READER_CONNECT_FAILURE" : "PRIVATE_READER",
            SCARD_SHARE_SHARED, SCARD_PROTOCOL_T1, &card, &protocol);
    if (mode == 1) { assert(rv == SCARD_E_SHARING_VIOLATION); return 0; }
    assert(rv == 0 && protocol == SCARD_PROTOCOL_T1);
    assert(SCardReconnect(card, SCARD_SHARE_SHARED, SCARD_PROTOCOL_T1, SCARD_LEAVE_CARD, &protocol) == 0);
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
    assert(count && count() == (mode == 9 ? 3 : mode == 7 ? 2 : mode == 0 || mode == 10 ? 1 : 0));
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
for task_mode in 0 1 3 4 7 8 9 10 11 12 13; do
    env -u LD_PRELOAD -u LIBPCSCLITE_SPY_DELEGATE \
        LIBPCSCLITE_DELEGATE="$task_tmp/delegate.so" JCOP4_PUBLIC_DISCOVERY_TRACE="$task_tmp/delegate-$task_mode.txt" \
        "$task_tmp/local-client" "$task_tmp/redirect-plugin.so" "$task_tmp/libfakepcsc.so" "$task_mode"
    task_headers=$(< "$task_tmp/delegate-$task_mode.txt")
    [[ $(stat -c %a "$task_tmp/delegate-$task_mode.txt") == 600 &&
       $task_headers == *'pcsc_call=SCardEstablishContext'* && $task_headers == *'pcsc_call=SCardConnect'* &&
       $task_headers != *'PRIVATE_'* && $task_headers != *'SECRET_PIN'* &&
       $task_headers != *'length'* && $task_headers != *'data='* ]]
    [[ $task_headers == *'format=public_discovery_headers_v5'* ]]
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
