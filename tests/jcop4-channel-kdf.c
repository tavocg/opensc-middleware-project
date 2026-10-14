/* Synthetic SHA1 fixtures calculated independently with Python hashlib.
 * No card data, session keys, PIN or PC/SC. */
#include "../tools/jcop4-channel-kdf.h"
#include <assert.h>
#include <stdio.h>

static void assert_hex(const unsigned char *bytes, const char *hex)
{
    static const char alphabet[] = "0123456789abcdef";
    for (size_t i = 0; i < 16; i++) {
        assert(hex[2*i] == alphabet[bytes[i] >> 4]);
        assert(hex[2*i+1] == alphabet[bytes[i] & 15]);
    }
    assert(!hex[32]);
}
static int all_zero(const unsigned char *bytes, size_t size)
{
    for (size_t i = 0; i < size; i++) if (bytes[i]) return 0;
    return 1;
}
int main(void)
{
    unsigned char secret[67] = {0}, saved[67];
    struct jcop4_channel_keys keys;
    memset(secret, 0, sizeof(secret));
    memcpy(saved, secret, sizeof(secret));
    memset(&keys, 0xAA, sizeof(keys));
    assert(jcop4_ca_aes128_kdf(secret, 66, &keys) == 0);
    assert_hex(keys.encryption, "47e96aacd21a6d10d66116a838865061");
    assert_hex(keys.mac, "53879982559b22440d0bc1c9cf3f09aa");
    assert(all_zero(keys.ssc, sizeof(keys.ssc)));
    assert(!memcmp(saved, secret, sizeof(secret)));
    assert(memcmp(keys.encryption, keys.mac, 16));
    puts("PASS synthetic KDF vector 1: fixed-width secret, big-endian counters, first16 bytes, zero SSC and unchanged input");
    for (size_t i = 0; i < 66; i++) secret[i] = (unsigned char)i;
    memcpy(saved, secret, sizeof(secret));
    memset(&keys, 0xAA, sizeof(keys));
    assert(jcop4_ca_aes128_kdf(secret, 66, &keys) == 0);
    assert_hex(keys.encryption, "a441a69484d02559e1c58ae917129cb9");
    assert_hex(keys.mac, "e0d1fe294ef782d272deee5956651237");
    assert(all_zero(keys.ssc, sizeof(keys.ssc)));
    assert(!memcmp(saved, secret, sizeof(secret)));
    assert(memcmp(keys.encryption, keys.mac, 16));
    puts("PASS synthetic KDF vector 2: fixed-width secret, big-endian counters, first16 bytes, zero SSC and unchanged input");
    memset(secret, 0, sizeof(secret)); secret[65] = 1;
    memcpy(saved, secret, sizeof(secret));
    memset(&keys, 0xAA, sizeof(keys));
    assert(jcop4_ca_aes128_kdf(secret, 66, &keys) == 0);
    assert_hex(keys.encryption, "7b055a93d227228fa828ad147c137145");
    assert_hex(keys.mac, "5d05c6b08dd26a03fc2056e07ba96cf9");
    assert(all_zero(keys.ssc, sizeof(keys.ssc)));
    assert(!memcmp(saved, secret, sizeof(secret)));
    assert(memcmp(keys.encryption, keys.mac, 16));
    puts("PASS synthetic KDF vector 3: fixed-width secret, big-endian counters, first16 bytes, zero SSC and unchanged input");
    for (size_t size = 0; size < 68; size++) {
        if (size == 66) continue;
        memset(&keys, 0xAA, sizeof(keys));
        assert(jcop4_ca_aes128_kdf(secret, size, &keys) == 1);
        assert(all_zero((const unsigned char *)&keys, sizeof(keys)));
    }
    puts("PASS KDF rejects minimal/truncated/oversized secrets and clears every output on failure");
    memset(&keys, 0xAA, sizeof(keys));
    assert(jcop4_ca_aes128_kdf(NULL, 66, &keys) == 1);
    assert(all_zero((const unsigned char *)&keys, sizeof(keys)));
    assert(jcop4_ca_aes128_kdf(secret, 66, NULL) == 1);
    puts("PASS KDF rejects absent secret/output without partial keys or fallback");
    OPENSSL_cleanse(&keys, sizeof(keys));
    OPENSSL_cleanse(secret, sizeof(secret));
    OPENSSL_cleanse(saved, sizeof(saved));
    return 0;
}
