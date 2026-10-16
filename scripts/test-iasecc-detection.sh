#!/usr/bin/env bash
# Real match/bind, synthetic provider returns NO_SERVICE; never real PCSC.
set -euo pipefail
umask 077
ulimit -c 0
task_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
task_build="$task_root/build/opensc"
task_fixture="$task_build/detection-fixture"
mkdir -p "$task_fixture"
read -r -a task_flags <<< "$(pkg-config --cflags cmocka libcrypto libpcsclite)"
read -r -a task_libs <<< "$(pkg-config --libs cmocka libcrypto)"
cat > "$task_fixture/provider.c" <<PROVIDER
#define SCardEstablishContext fixture_unused_establish_context
#include "$task_root/tests/pcsc-delegate-fixture.c"
#undef SCardEstablishContext
LONG SCardEstablishContext(DWORD scope, const void *r1, const void *r2, SCARDCONTEXT *context)
{
    assert(scope == SCARD_SCOPE_USER && !r1 && !r2 && context);
    return SCARD_E_NO_SERVICE;
}
PROVIDER
"${CC:-cc}" -std=c99 -D_GNU_SOURCE -shared -fPIC "${task_flags[@]}" \
    "$task_fixture/provider.c" -o "$task_fixture/provider.so"
printf 'app default { debug = 0; reader_driver pcsc { provider_library = "%s/provider.so"; } }\n' \
    "$task_fixture" > "$task_fixture/provider.conf"
"${CC:-cc}" -std=c99 -D_GNU_SOURCE -DHAVE_CONFIG_H -Wall -Wextra -Werror \
    -I"$task_build" -I"$task_root/vendor/opensc/src" "${task_flags[@]}" \
    "$task_root/vendor/opensc/src/tests/unittests/iasecc-detection.c" \
    -L"$task_build/src/libopensc/.libs" -Wl,-rpath,"$task_build/src/libopensc/.libs" \
    -Wl,-rpath,"$(pkg-config --variable=libdir cmocka)" -lopensc "${task_libs[@]}" \
    -o "$task_build/iasecc-detection-regression"
env -i PATH="$PATH" OPENSC_CONF="$task_fixture/provider.conf" OPENSC_DEBUG=0 \
    "$task_build/iasecc-detection-regression" </dev/null
