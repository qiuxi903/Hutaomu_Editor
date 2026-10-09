#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Hutaomu Editor - Half-Life 系列主题生成器。

三套主题，覆盖 Half-Life 宇宙的三种视觉语言：
- hl-combine    HL2 Combine 风：冷蓝黑色 + 钢青色强调（终端/面板感）
- hl-lambda     HL1/Black Mesa 风：暖白底 + Lambda 橙（实验室/工业感）
- hl-portal     Portal 风：极简白底 + Aperture 橙 + GLaDOS 蓝紫（洁净科技感）
"""
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
from make_themes import (build_colors, render_preview, write_png)


def mix(a, b, t):
    def parse(h):
        h = h.lstrip('#')
        return tuple(int(h[i:i+2], 16) for i in (0, 2, 4))
    def fmt(c):
        return '#%02x%02x%02x' % c
    ca, cb = parse(a), parse(b)
    return fmt(tuple(int(ca[i] + (cb[i] - ca[i]) * t) for i in range(3)))


def darken(c, t):
    return mix(c, '#000000', t)

def lighten(c, t):
    return mix(c, '#ffffff', t)


THEMES = [
    {
        "id": "hl-combine",
        "name": "Combine",
        "description": "Half-Life 2 Combine 风：冷蓝黑色调，钢青色强调，终端/面板感。",
        "dark": True,
        "base": {
            "window": "#0a0e14",
            "titleBar": "#0d1420",
            "titleBarText": "#8faccc",
            "sidebar": "#0d1420",
            "sidebarText": "#7a94b4",
            "editorBg": "#0a0e14",
            "editorFg": "#b8cce0",
            "accent": "#4a9eff",
            "border": "#1a2838",
            "hover": "#152232",
            "selected": "#1a3048",
            "selectedFg": "#d0e4ff",
            "buttonFg": "#0a0e14",
            "statusBar": "#0d1420",
            "menuBg": "#0d1420",
            "tooltipBg": "#162030",
        },
        "syntax": {
            "comment": "#4a6080",
            "keyword": "#ff9e64",
            "string": "#7fdbca",
            "number": "#ff9e64",
            "function": "#82aaff",
            "type": "#c792ea",
            "builtin": "#89ddff",
            "property": "#7fdbca",
            "attribute": "#ffcb6b",
        },
        "metrics": {
            "uiDensity": "comfortable",
            "fontSizeSmall": 12,
            "fontSizeNormal": 13,
            "cornerRadius": 4,
            "tabHeight": 32,
            "treeRowHeight": 24,
            "editorFontFamily": "Consolas, Cascadia Mono, monospace",
            "editorFontSize": 14,
        },
        "layout": {
            "sidebar": {"visible": True, "style": "plain"},
            "tabs": {"style": "underline"},
        },
    },
    {
        "id": "hl-lambda",
        "name": "Lambda",
        "description": "Black Mesa 风：暖白底，Lambda 橙强调，实验室/工业感。",
        "dark": False,
        "base": {
            "window": "#f5f3f0",
            "titleBar": "#edeae5",
            "titleBarText": "#3a3530",
            "sidebar": "#edeae5",
            "sidebarText": "#5a5048",
            "editorBg": "#fffefa",
            "editorFg": "#2a2520",
            "accent": "#e8590c",
            "border": "#d8d4cc",
            "hover": "#e8e4dc",
            "selected": "#fde8d4",
            "selectedFg": "#8a4000",
            "buttonFg": "#ffffff",
            "statusBar": "#edeae5",
            "menuBg": "#f5f3f0",
            "tooltipBg": "#fffefa",
        },
        "syntax": {
            "comment": "#99908a",
            "keyword": "#c05621",
            "string": "#2f855a",
            "number": "#b7791f",
            "function": "#2b6cb0",
            "type": "#805ad5",
            "builtin": "#2b6cb0",
            "property": "#2f855a",
            "attribute": "#c05621",
        },
        "metrics": {
            "uiDensity": "comfortable",
            "fontSizeSmall": 12,
            "fontSizeNormal": 13,
            "cornerRadius": 4,
            "tabHeight": 34,
            "treeRowHeight": 24,
            "editorFontFamily": "Consolas, Cascadia Mono, monospace",
            "editorFontSize": 14,
        },
        "layout": {
            "sidebar": {"visible": True, "style": "plain"},
            "tabs": {"style": "tab"},
        },
    },
    {
        "id": "hl-portal",
        "name": "Portal",
        "description": "Aperture Science 风：极简白底，Portal 橙与蓝紫强调，洁净科技感。",
        "dark": False,
        "base": {
            "window": "#ffffff",
            "titleBar": "#f0eeec",
            "titleBarText": "#3a3a3a",
            "sidebar": "#f5f3f2",
            "sidebarText": "#666666",
            "editorBg": "#ffffff",
            "editorFg": "#2d2d2d",
            "accent": "#ff8c00",
            "border": "#e0dedc",
            "hover": "#edeae8",
            "selected": "#fff0dd",
            "selectedFg": "#8a4a00",
            "buttonFg": "#ffffff",
            "statusBar": "#f5f3f2",
            "menuBg": "#ffffff",
            "tooltipBg": "#ffffff",
        },
        "syntax": {
            "comment": "#a0a0a0",
            "keyword": "#e07000",
            "string": "#0088aa",
            "number": "#7b68ee",
            "function": "#4169e1",
            "type": "#9932cc",
            "builtin": "#008b8b",
            "property": "#228b22",
            "attribute": "#cd853f",
        },
        "metrics": {
            "uiDensity": "comfortable",
            "fontSizeSmall": 12,
            "fontSizeNormal": 14,
            "cornerRadius": 6,
            "tabHeight": 36,
            "treeRowHeight": 26,
            "editorFontFamily": "Consolas, Cascadia Mono, monospace",
            "editorFontSize": 15,
        },
        "layout": {
            "sidebar": {"visible": True, "style": "card"},
            "tabs": {"style": "pill"},
        },
    },
]


def generate_all():
    """调用 make_themes 的 build_colors + render_preview 生成三套主题。"""
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
        with open(os.path.join(theme_dir, 'theme.json'), 'w', encoding='utf-8') as f:
            json.dump(theme, f, ensure_ascii=False, indent=4)
            f.write('\n')
        render_preview(os.path.join(theme_dir, 'preview.png'), colors)
        with open(os.path.join(theme_dir, 'LICENSE.txt'), 'w', encoding='utf-8') as f:
            f.write("Hutaomu Editor 官方主题 - AGPL-3.0-only\n"
                    "Copyright (C) 2026 邱息\n")
        print(f'generated {spec["id"]}: {len(colors)} colors')


if __name__ == '__main__':
    generate_all()
