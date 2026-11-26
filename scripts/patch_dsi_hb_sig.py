#!/usr/bin/env python3
"""Prepend classic 4-word NDS homebrew entry signature right before calico's
__ds9_bootstub (0x020047EC -> 0x02004800) so pre-2024 TWiLightMenu++ versions
recognize dunedsi.nds as modern DSi homebrew instead of launching retail
nds-bootstrap."""

import struct
import sys

def nds_crc16(buf: bytes) -> int:
    crc = 0xFFFF
    for b in buf:
        crc ^= b
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc & 0xFFFF

def main():
    if len(sys.argv) != 2:
        sys.exit("Usage: patch_dsi_hb_sig.py <file.nds>")

    path = sys.argv[1]
    data = bytearray(open(path, "rb").read())

    arm9_off, arm9_entry, arm9_dst, arm9_sz = struct.unpack_from("<4I", data, 0x20)
    if arm9_entry != arm9_dst + 0x800:
        sys.exit(f"Unexpected ARM9 entry 0x{arm9_entry:08X} (dst=0x{arm9_dst:08X})")

    stub_file_off = arm9_off + 0x800
    patch_file_off = stub_file_off - 20
    new_entry = arm9_entry - 20

    if any(b != 0 for b in data[patch_file_off:stub_file_off]):
        sys.exit("Refusing to patch: bytes before __ds9_bootstub are not zero")

    # Classic libnds homebrew entry signature recognized by TWiLightMenu++ (2023+)
    #   mov r0, #0x04000000    ; 0xE3A00301
    #   str r0, [r0, #0x208]   ; 0xE5800208 (REG_IME = 0)
    #   mov r0, #0x13          ; 0xE3A00013 (SVC mode)
    #   msr cpsr_c, r0         ; 0xE129F000
    #   nop                    ; 0xE1A00000 -> falls through to __ds9_bootstub
    prologue = struct.pack("<5I", 0xE3A00301, 0xE5800208, 0xE3A00013, 0xE129F000, 0xE1A00000)
    data[patch_file_off:stub_file_off] = prologue
    struct.pack_into("<I", data, 0x24, new_entry)

    # Recompute Secure Area CRC (0x6C, over 0x4000..0x7FFF) and Header CRC (0x15E, over 0x000..0x15D)
    sec_crc = nds_crc16(data[arm9_off : arm9_off + 0x4000])
    struct.pack_into("<H", data, 0x6C, sec_crc)
    hdr_crc = nds_crc16(data[0 : 0x15E])
    struct.pack_into("<H", data, 0x15E, hdr_crc)

    open(path, "wb").write(data)
    print(f"Patched {path}: ARM9 entry 0x{arm9_entry:08X} -> 0x{new_entry:08X}, secCRC=0x{sec_crc:04X}, hdrCRC=0x{hdr_crc:04X}")

if __name__ == "__main__":
    main()
