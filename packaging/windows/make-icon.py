#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Hutaomu Editor - 生成安装包图标（纯 Python，无第三方依赖）。

用法：python packaging/windows/make-icon.py
产物：packaging/windows/app.ico（256×256 PNG 压缩 ICO，Vista+ 支持）

原来 installer.iss 指向 brand.svg —— Inno Setup 要求 .ico，这一步把它补齐。
图形：品牌强调色圆角方块 + 白色 "H"（小尺寸下也认得出）。
"""
import os
import struct
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ACCENT = (0x1F, 0x8A, 0x6D)  # 默认主题（简约白）的 accent
SIZE = 256


def rounded_square_pixels():
    px = [[(0, 0, 0, 0) for _ in range(SIZE)] for _ in range(SIZE)]
    radius = 56
    for y in range(SIZE):
        for x in range(SIZE):
            # 圆角：四角按半径裁掉
            dx = 0
            dy = 0
            if x < radius:
                dx = radius - x
            elif x >= SIZE - radius:
                dx = x - (SIZE - radius - 1)
            if y < radius:
                dy = radius - y
            elif y >= SIZE - radius:
                dy = y - (SIZE - radius - 1)
            if dx * dx + dy * dy > radius * radius:
                continue
            px[y][x] = (ACCENT[0], ACCENT[1], ACCENT[2], 255)
    return px


def draw_h(px):
    """白色 H：两根竖条 + 一根横梁。"""
    bar = 26
    left = 74
    right = SIZE - 74 - bar
    top = 68
    bottom = SIZE - 68
    for y in range(top, bottom):
        for x in range(left, left + bar):
            px[y][x] = (255, 255, 255, 255)
        for x in range(right, right + bar):
            px[y][x] = (255, 255, 255, 255)
    mid_top = (top + bottom) // 2 - bar // 2
    for y in range(mid_top, mid_top + bar):
        for x in range(left, right + bar):
            px[y][x] = (255, 255, 255, 255)


def write_png_bytes(width, height, pixels):
    rows = []
    for row in pixels:
        flat = bytearray()
        for r, g, b, a in row:
            flat += bytes((r, g, b, a))
        rows.append(b'\x00' + bytes(flat))
    raw = b''.join(rows)

    def chunk(tag, data):
        return (struct.pack('>I', len(data)) + tag + data
                + struct.pack('>I', zlib.crc32(tag + data) & 0xffffffff))

    out = b'\x89PNG\r\n\x1a\n'
    out += chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0))
    out += chunk(b'IDAT', zlib.compress(raw, 9))
    out += chunk(b'IEND', b'')
    return out


def main():
    px = rounded_square_pixels()
    draw_h(px)
    png = write_png_bytes(SIZE, SIZE, px)

    # ICO：1 张 256×256 条目（PNG 压缩）
    header = struct.pack('<HHH', 0, 1, 1)
    entry = struct.pack('<BBBBHHII', 0, 0, 0, 0, 1, 32, len(png), 6 + 16)
    path = os.path.join(HERE, 'app.ico')
    with open(path, 'wb') as f:
        f.write(header + entry + png)
    print('wrote', path, len(png), 'bytes png')


if __name__ == '__main__':
    main()
