#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Hutaomu Editor - 生成安装向导的品牌图（纯 Python，无第三方依赖）。

用法：python packaging/windows/make-art.py
产物：
  wizard-big.bmp    164×314，向导左侧大图（品牌色渐变 + 书本/文档意象 + 白色 H 标识）
  wizard-small.bmp  55×55，向导右上小图（品牌标识）

Inno Setup 对向导图只认 BMP/PNG/JPEG（旧版本仅 BMP），这里一律写 24 位 BMP
（自下而上、BGR、4 字节行对齐），保证兼容。
"""
import os
import struct

HERE = os.path.dirname(os.path.abspath(__file__))
ACCENT = (0x1F, 0x8A, 0x6D)      # 默认主题 accent
ACCENT_DARK = (0x14, 0x5C, 0x49)


def blend(a, b, t):
    return tuple(round(a[i] + (b[i] - a[i]) * t) for i in range(3))


def write_bmp(path, width, height, pixels):
    """pixels: 行优先、自上而下的 (r,g,b) 列表，[y][x]。"""
    row_size = (width * 3 + 3) & ~3          # 4 字节对齐
    padding = b'\x00' * (row_size - width * 3)
    body = bytearray()
    for y in range(height - 1, -1, -1):      # BMP 自下而上
        row = pixels[y]
        for x in range(width):
            r, g, b = row[x]
            body += bytes((b, g, r))         # BGR
        body += padding

    file_size = 14 + 40 + len(body)
    header = b'BM' + struct.pack('<IHHI', file_size, 0, 0, 14 + 40)
    info = struct.pack('<IiiHHIIiiII', 40, width, height, 1, 24, 0, len(body),
                       2835, 2835, 0, 0)
    with open(path, 'wb') as f:
        f.write(header + info + bytes(body))


def rounded_square(px, size, cx, cy, half, radius, color):
    for y in range(max(0, cy - half), min(size, cy + half)):
        for x in range(max(0, cx - half), min(size, cx + half)):
            dx = max(0, abs(x - cx) - (half - radius))
            dy = max(0, abs(y - cy) - (half - radius))
            if dx * dx + dy * dy <= radius * radius:
                px[y][x] = color


def draw_h(px, size, cx, cy, bar, span, color):
    """白色 H：两竖 + 一横。"""
    left = cx - span // 2
    right = cx + span // 2 - bar
    top = cy - span // 2
    bottom = cy + span // 2
    for y in range(top, bottom):
        for x in range(left, left + bar):
            if 0 <= x < size and 0 <= y < size:
                px[y][x] = color
        for x in range(right, right + bar):
            if 0 <= x < size and 0 <= y < size:
                px[y][x] = color
    mid = cy - bar // 2
    for y in range(mid, mid + bar):
        for x in range(left, right + bar):
            if 0 <= x < size and 0 <= y < size:
                px[y][x] = color


def make_big(width=164, height=314):
    px = [[(0, 0, 0) for _ in range(width)] for _ in range(height)]
    # 竖向渐变 + 斜纹底，避免大片纯色显得"空"
    for y in range(height):
        for x in range(width):
            t = y / (height - 1)
            base = blend(ACCENT_DARK, ACCENT, t)
            stripe = 6 if ((x + y) % 14) < 7 else 0
            px[y][x] = tuple(min(255, c + stripe) for c in base)

    # 白色半透明"文档"意象：三行短条
    doc_w = 96
    doc_x = (width - doc_w) // 2
    for i, w in enumerate((doc_w, doc_w - 18, doc_w - 34)):
        y0 = 196 + i * 18
        for y in range(y0, y0 + 7):
            for x in range(doc_x, doc_x + w):
                px[y][x] = blend(px[y][x], (255, 255, 255), 0.75)

    # 品牌标识：白色圆角块 + accent 色 H（在深色 banner 上更醒目）
    rounded_square(px, width, width // 2, 122, 46, 16, (255, 255, 255))
    draw_h(px, width, width // 2, 122, 11, 44, ACCENT)
    return px


def make_small(size=55):
    px = [[(255, 255, 255) for _ in range(size)] for _ in range(size)]
    rounded_square(px, size, size // 2, size // 2, size // 2 - 1, 12, ACCENT)
    draw_h(px, size, size // 2, size // 2, 6, 26, (255, 255, 255))
    return px


def main():
    write_bmp(os.path.join(HERE, 'wizard-big.bmp'), 164, 314, make_big())
    write_bmp(os.path.join(HERE, 'wizard-small.bmp'), 55, 55, make_small())
    print('wrote wizard-big.bmp (164x314) and wizard-small.bmp (55x55)')


if __name__ == '__main__':
    main()
