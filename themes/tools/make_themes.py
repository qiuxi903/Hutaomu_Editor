#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Hutaomu Editor - 官方示范主题生成器（T1 配色 + T2 度量）。

用法：python themes/tools/make_themes.py
产物：themes/<id>/theme.json + themes/<id>/preview.png（每个主题目录）

主题的完整色键集合以 src/themes/paper.json 为准；本脚本从一组语义基色
推导出全部键，避免手写 ~90 个键时漏项或前后不一致。
"""
import json
import os
import struct
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


# ---------- 颜色工具 ----------

def rgb(hex_color):
    h = hex_color.lstrip('#')
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


def hx(color):
    return '#%02x%02x%02x' % color


def mix(a, b, t):
    """a、b 为 #rrggbb，t=0 取 a，t=1 取 b。"""
    ca, cb = rgb(a), rgb(b)
    return hx(tuple(round(ca[i] + (cb[i] - ca[i]) * t) for i in range(3)))


def lighten(c, t):
    return mix(c, '#ffffff', t)


def darken(c, t):
    return mix(c, '#000000', t)


# ---------- 主题定义 ----------
# 每个主题给出：基色 + 语法色 + 度量（T2）。其余键由基色推导。

THEMES = [
    {
        "id": "typora-immersive",
        "name": "沉浸写作 Typora",
        "description": "单栏沉浸写作形态：衬线字体、大行距、宽松密度，仿 Typora。",
        "dark": False,
        "base": {
            "window": "#ffffff",
            "titleBar": "#fcfcfc",
            "titleBarText": "#2f3437",
            "sidebar": "#fafafa",
            "sidebarText": "#454b4f",
            "editorBg": "#ffffff",
            "editorFg": "#2f3437",
            "accent": "#6c5ce7",
            "border": "#ececec",
            "hover": "#f1f0fb",
            "selected": "#e7e4fb",
            "selectedFg": "#2b2350",
            "buttonFg": "#ffffff",
            "statusBar": "#fafafa",
            "menuBg": "#ffffff",
            "tooltipBg": "#ffffff",
        },
        "syntax": {
            "comment": "#9aa5a0",
            "keyword": "#8f4bd8",
            "string": "#3f8f5f",
            "number": "#c1652a",
            "function": "#2f6fb5",
            "type": "#a05a2c",
            "builtin": "#2f6fb5",
            "property": "#3f7f5f",
            "attribute": "#a05a2c",
        },
        "assets": {
            "brand": "book",
            "background": {"kind": "paper", "mode": "tile", "opacity": 0.55},
        },
        # T4 形态：Typora 式写作 —— 收起侧栏、下划线标签、居中栏宽、标题下划线
        "layout": {
            "activityBar": {"visible": True},
            "sidebar": {"visible": False},
            "statusBar": {"visible": True},
            "tabs": {"style": "underline"},
            "editor": {"centered": True, "maxWidth": 760, "headingUnderline": True},
        },
        "metrics": {
            "uiDensity": "spacious",
            "fontSizeSmall": 13,
            "fontSizeNormal": 15,
            "editorFontFamily": "Georgia, Noto Serif SC, Songti SC, serif",
            "editorFontSize": 17,
            "tabHeight": 36,
            "treeRowHeight": 28,
            "statusBarPaddingH": 12,
            "cornerRadius": 6,
            "cornerRadiusLarge": 8,
            "scrollbarWidth": 10,
            "menuItemPaddingV": 7,
            "menuItemPaddingLeft": 28,
            "menuItemPaddingRight": 32,
        },
    },
    {
        "id": "obsidian-cards",
        "name": "卡片笔记 Obsidian",
        "description": "深色卡片化界面：紫色强调、圆角与分组更明显，仿 Obsidian。",
        "dark": True,
        "base": {
            "window": "#1e1e1e",
            "titleBar": "#262626",
            "titleBarText": "#dadada",
            "sidebar": "#262626",
            "sidebarText": "#b9b9b9",
            "editorBg": "#1e1e1e",
            "editorFg": "#dcddde",
            "accent": "#7f6df2",
            "border": "#333333",
            "hover": "#2f2f2f",
            "selected": "#3a3a52",
            "selectedFg": "#e8e6ff",
            "buttonFg": "#ffffff",
            "statusBar": "#262626",
            "menuBg": "#2a2a2a",
            "tooltipBg": "#2a2a2a",
        },
        "syntax": {
            "comment": "#7c7c7c",
            "keyword": "#c792ea",
            "string": "#a5d6a7",
            "number": "#f78c6c",
            "function": "#82aaff",
            "type": "#ffcb6b",
            "builtin": "#82aaff",
            "property": "#a5d6a7",
            "attribute": "#ffcb6b",
        },
        "assets": {
            "brand": "gem",
            "background": {"kind": "dots", "mode": "tile", "opacity": 0.7},
        },
        # T4 形态：Obsidian 式卡片侧栏 + 胶囊标签
        "layout": {
            "sidebar": {"visible": True, "style": "card"},
            "outlinePanel": {"visible": False},
            "tabs": {"style": "pill"},
        },
        "metrics": {
            "uiDensity": "comfortable",
            "treeRowHeight": 26,
            "cornerRadius": 8,
            "cornerRadiusLarge": 10,
            "scrollbarWidth": 10,
            "tabHeight": 36,
            "editorFontSize": 15,
            "editorFontFamily": "Consolas, JetBrains Mono, monospace",
            "menuItemPaddingV": 6,
        },
    },
    {
        "id": "vscode-dark-plus",
        "name": "VS Code Dark+",
        "description": "对齐 VS Code 默认深色配色，紧凑 IDE 密度。",
        "dark": True,
        "base": {
            "window": "#1e1e1e",
            "titleBar": "#323233",
            "titleBarText": "#cccccc",
            "sidebar": "#252526",
            "sidebarText": "#cccccc",
            "editorBg": "#1e1e1e",
            "editorFg": "#d4d4d4",
            "accent": "#007acc",
            "border": "#3c3c3c",
            "hover": "#2a2d2e",
            "selected": "#37373d",
            "selectedFg": "#ffffff",
            "buttonFg": "#ffffff",
            "statusBar": "#007acc",
            "menuBg": "#252526",
            "tooltipBg": "#252526",
        },
        "syntax": {
            "comment": "#6a9955",
            "keyword": "#569cd6",
            "string": "#ce9178",
            "number": "#b5cea8",
            "function": "#dcdcaa",
            "type": "#4ec9b0",
            "builtin": "#4fc1ff",
            "property": "#9cdcfe",
            "attribute": "#9cdcfe",
        },
        "metrics": {
            "uiDensity": "compact",
            "tabHeight": 32,
            "treeRowHeight": 22,
            "editorFontFamily": "Consolas, Cascadia Mono, monospace",
            "editorFontSize": 14,
            "scrollbarWidth": 10,
            "cornerRadius": 3,
            "cornerRadiusLarge": 5,
            "fontSizeSmall": 12,
            "fontSizeNormal": 13,
            "menuItemPaddingV": 5,
        },
    },
    {
        "id": "vscode-light-plus",
        "name": "VS Code Light+",
        "description": "对齐 VS Code 默认浅色配色，紧凑 IDE 密度。",
        "dark": False,
        "base": {
            "window": "#ffffff",
            "titleBar": "#dddddd",
            "titleBarText": "#333333",
            "sidebar": "#f3f3f3",
            "sidebarText": "#333333",
            "editorBg": "#ffffff",
            "editorFg": "#333333",
            "accent": "#007acc",
            "border": "#e5e5e5",
            "hover": "#e8e8e8",
            "selected": "#cde3f6",
            "selectedFg": "#1e1e1e",
            "buttonFg": "#ffffff",
            "statusBar": "#007acc",
            "menuBg": "#ffffff",
            "tooltipBg": "#ffffff",
        },
        "syntax": {
            "comment": "#008000",
            "keyword": "#0000ff",
            "string": "#a31515",
            "number": "#098658",
            "function": "#795e26",
            "type": "#267f99",
            "builtin": "#0070c1",
            "property": "#001080",
            "attribute": "#e50000",
        },
        "metrics": {
            "uiDensity": "compact",
            "tabHeight": 32,
            "treeRowHeight": 22,
            "editorFontFamily": "Consolas, Cascadia Mono, monospace",
            "editorFontSize": 14,
            "scrollbarWidth": 10,
            "cornerRadius": 3,
            "cornerRadiusLarge": 5,
            "fontSizeSmall": 12,
            "fontSizeNormal": 13,
            "menuItemPaddingV": 5,
        },
    },
]


def build_colors(spec):
    b = spec["base"]
    sy = spec["syntax"]
    dark = spec["dark"]
    window = b["window"]
    sidebar = b["sidebar"]
    editor_bg = b["editorBg"]
    editor_fg = b["editorFg"]
    accent = b["accent"]
    border = b["border"]
    hover = b["hover"]
    selection = mix(accent, editor_bg, 0.35)
    panel = sidebar
    muted = mix(editor_fg, window, 0.45 if not dark else 0.4)
    disabled = mix(editor_fg, window, 0.65 if not dark else 0.55)

    colors = {
        "accent": accent,
        "accentHover": lighten(accent, 0.18) if dark else darken(accent, 0.12),
        "windowBg": window,
        "titleBarBg": b["titleBar"],
        "titleBarText": b["titleBarText"],
        "titleBarMutedFg": muted,
        "brandSub": muted,
        "separatorColor": border,
        "menuFg": b["titleBarText"],
        "menuHoverBg": hover,
        "menuPressedBg": mix(hover, editor_fg, 0.08),
        "winButtonHoverBg": hover,
        "winCloseHoverBg": "#d9534f",
        "activityBarBg": darken(panel, 0.06) if dark else panel,
        "activityHoverBorder": mix(editor_fg, panel, 0.5),
        "sidebarBg": panel,
        "sidebarTitleFg": muted,
        "sidebarText": b["sidebarText"],
        "sidebarHoverBg": hover,
        "sidebarSelectedBg": b["selected"],
        "sidebarSelectedFg": b["selectedFg"],
        "sidebarButtonHoverBg": hover,
        "welcomeHintFg": muted,
        "statusBarBg": b["statusBar"],
        "statusBarFg": "#ffffff" if dark and b["statusBar"] == accent else b["sidebarText"],
        "statusBarBorder": border,
        "tabBarBg": darken(panel, 0.04) if dark else panel,
        "tabFg": muted,
        "tabBg": darken(panel, 0.04) if dark else panel,
        "tabActiveBg": editor_bg,
        "tabActiveFg": editor_fg,
        "tabHoverBg": hover,
        "editorBg": editor_bg,
        "editorFg": editor_fg,
        "currentLineBg": mix(accent, editor_bg, 0.06),
        "selectionBg": selection,
        "lineNumberFg": mix(editor_fg, editor_bg, 0.62),
        "lineNumberActiveFg": editor_fg,
        "scrollbarHandle": mix(editor_fg, editor_bg, 0.72),
        "scrollbarHandleHover": mix(editor_fg, editor_bg, 0.55),
        "menuBg": b["menuBg"],
        "menuBorder": border,
        "menuItemHoverBg": hover,
        "menuItemHoverFg": editor_fg,
        "menuDisabledFg": disabled,
        "tooltipBg": b["tooltipBg"],
        "tooltipFg": editor_fg,
        "tooltipBorder": border,
        "dialogBg": window,
        "buttonFg": b["buttonFg"],
        "buttonBg": accent,
        "buttonHover": lighten(accent, 0.15) if dark else darken(accent, 0.1),
        "splitterHandle": border,
        "searchHighlight": mix(accent, editor_bg, 0.3),
        "activityIconFg": b["sidebarText"],
        "activityIconDisabledFg": disabled,
        "sashColor": mix(border, editor_fg, 0.1),
    }

    colors.update({
        "syntax.comment": sy["comment"],
        "syntax.keyword": sy["keyword"],
        "syntax.string": sy["string"],
        "syntax.number": sy["number"],
        "syntax.function": sy["function"],
        "syntax.method": sy["function"],
        "syntax.constructor": sy["type"],
        "syntax.type": sy["type"],
        "syntax.class": sy["type"],
        "syntax.struct": sy["type"],
        "syntax.builtin": sy["builtin"],
        "syntax.constant": sy["number"],
        "syntax.boolean": sy["number"],
        "syntax.property": sy["property"],
        "syntax.field": sy["property"],
        "syntax.parameter": editor_fg,
        "syntax.variable": editor_fg,
        "syntax.operator": muted,
        "syntax.punctuation": muted,
        "syntax.tag": sy["property"],
        "syntax.attribute": sy["attribute"],
        "syntax.namespace": sy["builtin"],
        "syntax.module": sy["builtin"],
        "syntax.label": sy["type"],
        "syntax.escape": sy["string"],
        "syntax.regexp": sy["string"],
        "syntax.include": sy["keyword"],
        "syntax.annotation": sy["type"],
        "syntax.macro": sy["keyword"],
    })
    return colors


# ---------- 预览图（160x100 迷你窗口示意，纯 Python PNG 写出） ----------

def write_png(path, width, height, pixels):
    """pixels: 每行像素列表，每像素为 (r,g,b) 或 (r,g,b,a) —— 统一按 RGBA 写出。"""
    rows = []
    for row in pixels:
        flat = bytearray()
        for px in row:
            if len(px) == 4:
                flat += bytes(px)
            else:
                flat += bytes(px) + b'\xff'
        rows.append(b'\x00' + bytes(flat))
    raw = b''.join(rows)

    def chunk(tag, data):
        return (struct.pack('>I', len(data)) + tag + data
                + struct.pack('>I', zlib.crc32(tag + data) & 0xffffffff))

    png = b'\x89PNG\r\n\x1a\n'
    png += chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0))
    png += chunk(b'IDAT', zlib.compress(raw, 9))
    png += chunk(b'IEND', b'')
    with open(path, 'wb') as f:
        f.write(png)


def render_preview(path, colors):
    """按主题色绘制迷你窗口：标题栏/标签栏/活动栏/侧栏/编辑器/状态栏。"""
    w, h = 160, 100
    c = {k: rgb(v) for k, v in colors.items()}
    px = [[c['editorBg'] for _ in range(w)] for _ in range(h)]

    def rect(x0, y0, x1, y1, color):
        for y in range(max(0, y0), min(h, y1)):
            for x in range(max(0, x0), min(w, x1)):
                px[y][x] = color

    rect(0, 0, w, 10, c['titleBarBg'])                       # 标题栏
    rect(0, 10, w, 11, c['separatorColor'])
    rect(0, 11, 14, h - 10, c['activityBarBg'])              # 活动栏
    for i in range(4):                                       # 活动栏图标位
        rect(5, 16 + i * 9, 9, 20 + i * 9,
             c['accent'] if i == 0 else c['activityIconFg'])
    rect(14, 11, 58, h - 10, c['sidebarBg'])                 # 侧栏
    rect(20, 18, 52, 21, c['sidebarSelectedBg'] if True else c['sidebarText'])
    rect(20, 26, 48, 28, c['sidebarText'])
    rect(20, 33, 50, 35, c['sidebarText'])
    rect(20, 40, 44, 42, c['sidebarText'])
    rect(58, 11, w, h - 10, c['editorBg'])                  # 编辑器
    rect(64, 16, 96, 21, c['editorFg'])                      # 标题
    rect(64, 27, 140, 29, c['editorFg'])
    rect(64, 34, 130, 36, c['editorFg'])
    rect(64, 41, 118, 43, c['syntax.keyword'])               # 代码行
    rect(64, 48, 100, 50, c['syntax.string'])
    rect(64, 55, 136, 57, c['editorFg'])
    rect(64, 64, 84, 69, c['accent'])                        # 强调按钮
    rect(0, h - 10, w, h, c['statusBarBg'])                  # 状态栏
    rect(4, h - 7, 40, h - 5, c['statusBarFg'])

    write_png(path, w, h, px)


def _lcg(seed):
    """确定性伪随机（保证每次生成同样的纹理，便于 git diff 稳定）。"""
    state = seed & 0xFFFFFFFF

    def next_rand():
        nonlocal state
        state = (1103515245 * state + 12345) & 0x7FFFFFFF
        return state / 0x7FFFFFFF

    return next_rand


def render_texture(kind):
    """返回 (w, h, pixels)：半透明纹理，叠在 editorBg 之上做纸张颗粒/点阵。"""
    if kind == 'dots':
        w = h = 16
        px = [[(0, 0, 0, 0) for _ in range(w)] for _ in range(h)]
        for y in range(0, h, 8):
            for x in range(0, w, 8):
                px[y][x] = (255, 255, 255, 26)
        return w, h, px

    # paper：细颗粒（黑白点混合，深浅主题都自然）
    w = h = 48
    rand = _lcg(20260930)
    px = [[(0, 0, 0, 0) for _ in range(w)] for _ in range(h)]
    for y in range(h):
        for x in range(w):
            roll = rand()
            if roll < 0.05:
                alpha = 10 + int(rand() * 14)
                px[y][x] = (0, 0, 0, alpha) if rand() < 0.5 else (255, 255, 255, alpha)
    return w, h, px


BRAND_SHAPES = {
    # 书本 + 书签（写作向）
    'book': ('<path fill="FILLCOLOR" d="M4 4.5A2.5 2.5 0 0 1 6.5 2H18a2 2 0 0 1 2 2v16a2 2 '
             '0 0 1-2 2H6.5A2.5 2.5 0 0 1 4 19.5v-15Zm2.5-.5a.5.5 0 0 0 0 1H12v13.5l2.5-1.6 2.5 '
             '1.6V5h1.5V4a.5.5 0 0 0-.5-.5H6.5Z"/>'),
    # 多面体（笔记/知识向）
    'gem': ('<path fill="PANELCOLOR" d="M12 1.6 21 8l-9 14.4L3 8l9-6.4Z"/>'
            '<path fill="FILLCOLOR" d="M12 4.2 6.2 8.4h11.6L12 4.2Zm-7 5.6 5.4 9.3-3-9.3H5Zm11.6 '
            '0-3 9.3 5.4-9.3h-2.4Z"/>'),
}


def write_brand_svg(path, shape):
    svg = ('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">'
           + BRAND_SHAPES[shape] + '</svg>\n')
    with open(path, 'w', encoding='utf-8') as f:
        f.write(svg)


def main():
    for spec in THEMES:
        theme_dir = os.path.join(ROOT, spec['id'])
        os.makedirs(theme_dir, exist_ok=True)
        colors = build_colors(spec)
        theme = {
            "schemaVersion": 1,
            "id": spec["id"],
            "name": spec["name"],
            "description": spec["description"],
            "dark": spec["dark"],
            "capabilities": ["colors", "metrics"],
            "colors": colors,
            "metrics": spec["metrics"],
        }
        # T3 资源：品牌 logo（随主题换色，形状可不同）
        assets = spec.get('assets')
        if assets:
            icons_dir = os.path.join(theme_dir, 'assets', 'icons')
            os.makedirs(icons_dir, exist_ok=True)
            if assets.get('brand'):
                write_brand_svg(os.path.join(icons_dir, 'brand.svg'), assets['brand'])
            background = assets.get('background')
            if background:
                tex_w, tex_h, tex_px = render_texture(background.get('kind', 'paper'))
                write_png(os.path.join(theme_dir, 'assets', 'background.png'),
                          tex_w, tex_h, tex_px)
                theme['background'] = {
                    'image': 'assets/background.png',
                    'mode': background.get('mode', 'tile'),
                    'opacity': background.get('opacity', 0.5),
                }
                if 'assets' not in theme['capabilities']:
                    theme['capabilities'].append('assets')

        # T4 形态：layout.json（存在即声明"会改界面布局"）
        layout = spec.get('layout')
        if layout:
            with open(os.path.join(theme_dir, 'layout.json'), 'w',
                      encoding='utf-8') as f:
                json.dump(layout, f, ensure_ascii=False, indent=4)
                f.write('\n')
            if 'layout' not in theme['capabilities']:
                theme['capabilities'].append('layout')

        with open(os.path.join(theme_dir, 'theme.json'), 'w', encoding='utf-8') as f:
            json.dump(theme, f, ensure_ascii=False, indent=4)
            f.write('\n')
        if not layout:
            stale = os.path.join(theme_dir, 'layout.json')
            if os.path.exists(stale):
                os.remove(stale)

        render_preview(os.path.join(theme_dir, 'preview.png'), colors)
        with open(os.path.join(theme_dir, 'LICENSE.txt'), 'w', encoding='utf-8') as f:
            f.write("Hutaomu Editor 官方主题 - AGPL-3.0-only\n"
                    "版权所有 (c) 2026 Hutaomu，保留所有权利。\n")
        print('generated', spec['id'], len(colors), 'colors')


if __name__ == '__main__':
    main()
