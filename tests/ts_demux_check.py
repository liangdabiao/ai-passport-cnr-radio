# -*- coding: utf-8 -*-
"""TS→PES→ADTS 解复用验证（先 Python 证明，再移植 C）。"""
import struct
import sys

data = open(sys.argv[1] if len(sys.argv) > 1 else "seg.ts", "rb").read()
assert len(data) % 188 == 0, f"size {len(data)} not multiple of 188"
n_pkts = len(data) // 188

pmt_pid = None
audio_pid = None
pes_count = 0
adts_frames = 0
out = bytearray()


def parse_pat(p):
    # payload 起 5 字节指针后是 sections；取第一个 program 的 PMT PID
    payload = p
    # pointer_field
    ptr = payload[0]
    s = payload[1 + ptr:]
    section_len = ((s[1] & 0x0F) << 8) | s[2]
    body = s[8:8 + section_len - 5 - 4]  # 去 header+CRC
    progs = {}
    for i in range(0, len(body) - 3, 4):
        num = (body[i] << 8) | body[i + 1]
        pid = ((body[i + 2] & 0x1F) << 8) | body[i + 3]
        if num != 0:
            progs[num] = pid
    return progs


def parse_pmt(p):
    ptr = p[0]
    s = p[1 + ptr:]
    section_len = ((s[1] & 0x0F) << 8) | s[2]
    body = s[8:8 + section_len - 5 - 4]
    prog_info_len = ((body[2] & 0x0F) << 8) | body[3]
    i = 4 + prog_info_len
    streams = []
    while i + 5 <= len(body):
        stype = body[i]
        epid = ((body[i + 1] & 0x1F) << 8) | body[i + 2]
        es_len = ((body[i + 3] & 0x0F) << 8) | body[i + 4]
        streams.append((stype, epid))
        i += 5 + es_len
    return streams


pes_buf = {}
for k in range(n_pkts):
    pkt = data[k * 188:(k + 1) * 188]
    if pkt[0] != 0x47:
        print("sync lost at packet", k)
        break
    pusi = (pkt[1] >> 6) & 1
    pid = ((pkt[1] & 0x1F) << 8) | pkt[2]
    afc = (pkt[3] >> 4) & 3
    off = 4
    if afc & 2:
        off += 1 + pkt[4]
    if not (afc & 1):
        continue
    payload = pkt[off:]

    if pid == 0:
        progs = parse_pat(payload)
        pmt_pid = next(iter(progs.values()))
    elif pmt_pid is not None and pid == pmt_pid and audio_pid is None:
        for stype, epid in parse_pmt(payload):
            print(f"stream type=0x{stype:02x} pid={epid}")
            if stype == 0x0F:
                audio_pid = epid
    elif audio_pid is not None and pid == audio_pid:
        if pusi:
            # PES 头
            assert payload[0:3] == b"\x00\x00\x01", payload[:4]
            hdr_len = 9 + payload[8]
            pes_count += 1
            pes_buf[k] = payload[hdr_len:]
        else:
            pes_buf[k] = payload

# 统计
raw = b"".join(pes_buf[k] for k in sorted(pes_buf))
i = 0
frames = 0
first_err = None
while i + 7 <= len(raw):
    if raw[i] == 0xFF and (raw[i + 1] & 0xF6) == 0xF0:
        sri = (raw[i + 2] >> 2) & 0xF
        ch = ((raw[i + 2] & 1) << 2) | (raw[i + 3] >> 6)
        fl = ((raw[i + 3] & 3) << 11) | (raw[i + 4] << 3) | (raw[i + 5] >> 5)
        if sri < 13 and 1 <= ch <= 7 and 8 <= fl <= 8192 and i + fl <= len(raw):
            out += raw[i:i + fl]
            i += fl
            frames += 1
            continue
    i += 1

dur = frames * 1024 / 44100
print(f"packets={n_pkts} pmt={pmt_pid} audio_pid={audio_pid}")
print(f"PES={pes_count} ADTS frames={frames} ≈{dur:.1f}s  payload={len(raw)}B")
open("seg_adts.bin", "wb").write(out)
