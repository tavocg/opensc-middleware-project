#!/usr/bin/env python3
"""Reproduce public synthetic codec fixtures using OpenSSL CLI, never card I/O."""
import hashlib
import json
import subprocess

KENC = bytes(range(16))
KMAC = bytes(range(16, 32))


def cli(args, data):
    return subprocess.run(["openssl", *args], input=data, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, check=True).stdout


def pad(data):
    return data + b"\x80" + bytes((-len(data) - 1) % 16)


def iv(ssc):
    return cli(["enc", "-aes-128-ecb", "-nopad", "-nosalt", "-K", KENC.hex()], ssc)


def encrypt(data, ssc):
    return cli(["enc", "-aes-128-cbc", "-nopad", "-nosalt", "-K", KENC.hex(),
                "-iv", iv(ssc).hex()], data)


def mac(data):
    return cli(["mac", "-binary", "-macopt", "cipher:AES-128-CBC",
                "-macopt", "hexkey:" + KMAC.hex(), "CMAC"], data)[:8]


def tlv(tag, value):
    size = len(value)
    length = bytes([size]) if size < 128 else (
        b"\x81" + bytes([size]) if size <= 255 else b"\x82" + size.to_bytes(2, "big"))
    return bytes([tag]) + length + value


def command(header, data=b"", le=None):
    ssc = (1).to_bytes(16, "big")
    body = tlv(0x87, b"\x01" + encrypt(pad(data), ssc)) if data else b""
    if le is not None:
        body += tlv(0x97, bytes([le % 256]))
    auth = ssc + pad(header) + body
    return body + tlv(0x8E, mac(pad(auth) if body else auth))


def response(data, valid_padding=True):
    ssc = (2).to_bytes(16, "big")
    body = tlv(0x87, b"\x01" + encrypt(pad(data) if valid_padding else data, ssc))
    body += tlv(0x99, b"\x90\x00")
    return body + tlv(0x8E, mac(pad(ssc + body))) + b"\x90\x00"


if __name__ == "__main__":
    vectors = {
        "iv_command": iv((1).to_bytes(16, "big")).hex(),
        "iv_response": iv((2).to_bytes(16, "big")).hex(),
        "iv_full_ssc": iv(bytes.fromhex("00112233445566778899aabbccddeeff")).hex(),
        "select": command(b"\x0c\xa4\x02\x00", b"\x3f\x00", 256).hex(),
        "response": response(b"PUBLIC FIXTURE").hex(),
        "invalid_padding": response(bytes(16), valid_padding=False).hex(),
    }
    for size in (128, 384, 3584):
        body = command(b"\x0c\xda\x00\x00", bytes(i % 256 for i in range(size)), 256)
        vectors[f"long_{size}"] = hashlib.sha256(body).hexdigest()
    print(json.dumps(vectors, indent=2))
