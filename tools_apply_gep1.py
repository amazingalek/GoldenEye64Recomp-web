#!/usr/bin/env python3
# Makes the TLBFREE ROM from a retail NTSC-U dump with vanilla_to_tlbfree.gep1, the same
# patch the app embeds for its ROM picker (format: src/main/rom_conversion.cpp). For build
# hosts without xdelta3, e.g. Windows:
#   python3 tools_apply_gep1.py baserom.u.z64 vanilla_to_tlbfree.gep1 ge007.tlbfree.z64
import hashlib
import struct
import sys

RETAIL_US_SHA1 = "abe01e4aeb033b6c0836819f549c791b26cfde83"


def apply_gep1(patch, src):
    if patch[:4] != b"GEP1":
        raise ValueError("not a GEP1 patch")
    src_size, tgt_size = struct.unpack_from("<II", patch, 4)
    if src_size != len(src):
        raise ValueError("source is %d bytes, the patch expects %d" % (len(src), src_size))
    out = bytearray()
    o = 12
    while True:
        op = patch[o]
        o += 1
        if op == 0x00:  # COPY: u32 src_off, u32 len
            off, length = struct.unpack_from("<II", patch, o)
            o += 8
            if off + length > len(src):
                raise ValueError("COPY past the end of the source")
            out += src[off:off + length]
        elif op == 0x01:  # DATA: u32 len, <len bytes>
            (length,) = struct.unpack_from("<I", patch, o)
            o += 4
            out += patch[o:o + length]
            o += length
        elif op == 0xFF:  # END
            break
        else:
            raise ValueError("unknown record 0x%02x" % op)
    if len(out) != tgt_size:
        raise ValueError("made %d bytes, the patch expects %d" % (len(out), tgt_size))
    return out


def main():
    if len(sys.argv) != 4:
        sys.exit("usage: %s <retail .z64> <patch .gep1> <output .z64>" % sys.argv[0])
    with open(sys.argv[1], "rb") as f:
        src = f.read()
    if hashlib.sha1(src).hexdigest() != RETAIL_US_SHA1:
        sys.exit("%s is not the retail NTSC-U GoldenEye 007 dump (big-endian .z64, sha1 %s)"
                 % (sys.argv[1], RETAIL_US_SHA1))
    with open(sys.argv[2], "rb") as f:
        patch = f.read()
    out = apply_gep1(patch, src)
    with open(sys.argv[3], "wb") as f:
        f.write(out)


if __name__ == "__main__":
    main()
