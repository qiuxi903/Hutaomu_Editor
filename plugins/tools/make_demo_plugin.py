#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Hutaomu Editor - 官方示例插件生成器。

用法：python plugins/tools/make_demo_plugin.py
产物：plugins/hutaomu.demo-toolkit/（plugin.json + themes/sunrise/*）

示例插件示范了三类贡献点：
  - commands：一个内置动作 + 一个外部命令（需要 shell 权限）
  - formats ：把 *.log 认领为日志（按纯文本高亮）
  - themes  ：自带一套主题（由现存主题换色生成，保证色键完整）
"""
import json
import os
import shutil

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))   # plugins/
REPO = os.path.dirname(ROOT)
PLUGIN_DIR = os.path.join(ROOT, 'hutaomu.demo-toolkit')
SOURCE_THEME = os.path.join(REPO, 'themes', 'vscode-light-plus', 'theme.json')


def shift(hex_color, target):
    """只保留亮度关系、换一个色相：用于生成"同一主题的另一种强调色"。"""
    return target


def main():
    os.makedirs(os.path.join(PLUGIN_DIR, 'themes', 'sunrise'), exist_ok=True)

    # 1) 主题：以现成主题为底，改 id/名称与强调色（色键完整，避免缺色）
    with open(SOURCE_THEME, encoding='utf-8') as f:
        theme = json.load(f)
    theme['id'] = 'sunrise'
    theme['name'] = '日出（示例插件）'
    theme['description'] = '由示例插件提供的主题：暖色强调，浅色背景。'
    theme['colors']['accent'] = '#e8590c'
    theme['colors']['accentHover'] = '#f76707'
    for key in ('syntax.keyword', 'syntax.include', 'syntax.macro'):
        theme['colors'][key] = '#d9480f'
    theme['metrics'] = {
        'uiDensity': 'comfortable',
        'fontSizeSmall': 12,
        'fontSizeNormal': 13,
        'cornerRadius': 5,
        'treeRowHeight': 24,
    }
    theme.pop('background', None)
    theme['capabilities'] = ['colors', 'metrics']
    with open(os.path.join(PLUGIN_DIR, 'themes', 'sunrise', 'theme.json'), 'w',
              encoding='utf-8') as f:
        json.dump(theme, f, ensure_ascii=False, indent=4)
        f.write('\n')

    # 2) 插件清单
    manifest = {
        "id": "hutaomu.demo-toolkit",
        "name": "示例插件：日志与写作工具",
        "version": "1.0.0",
        "publisher": "hutaomu",
        "description": "官方示例：自带一套主题、认领 *.log 格式、提供两个命令。",
        "license": "LicenseRef-Proprietary",
        "engines": {"hutaomu": "0.1.0"},
        "permissions": ["shell"],
        "contributes": {
            "themes": ["themes/sunrise"],
            "statusBar": [
                {"id": "words", "type": "wordCount", "alignment": "right"},
                {"id": "lines", "type": "lineCount", "alignment": "right"},
                {"id": "indent", "type": "indent", "alignment": "right"}
            ],
            "pages": [
                {"id": "guide", "title": "插件说明", "type": "markdown",
                 "content": "README.md", "icon": "assets/icons/page.svg",
                 "defaultSide": "left"}
            ],
            "formats": [
                {"extension": "log", "language": "plaintext",
                 "label": "日志（纯文本）"}
            ],
            "commands": [
                {"id": "toggleWrap", "title": "切换自动换行",
                 "category": "编辑", "action": "view.toggleWordWrap",
                 "shortcut": "Ctrl+Alt+W"},
                {"id": "revealInExplorer", "title": "在文件管理器中显示当前文件",
                 "category": "工具", "shell": "explorer",
                 "args": "/select,{file}"},
                {"id": "setSunriseTheme", "title": "切换到「日出」主题",
                 "category": "外观", "action": "theme.set:sunrise"}
            ]
        }
    }
    with open(os.path.join(PLUGIN_DIR, 'plugin.json'), 'w', encoding='utf-8') as f:
        json.dump(manifest, f, ensure_ascii=False, indent=4)
        f.write('\n')
    icons_dir = os.path.join(PLUGIN_DIR, 'assets', 'icons')
    os.makedirs(icons_dir, exist_ok=True)
    with open(os.path.join(icons_dir, 'page.svg'), 'w', encoding='utf-8') as f:
        f.write('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">'
                '<path fill="FILLCOLOR" d="M6 2h8l6 6v14a2 2 0 0 1-2 2H6a2 2 0 0 1-2-2V4a2 2 0 0 1 '
                '2-2Zm7 1.5V9h5.5L13 3.5ZM7 12h10v1.6H7V12Zm0 4h10v1.6H7V16Z"/></svg>\n')

    with open(os.path.join(PLUGIN_DIR, 'README.md'), 'w', encoding='utf-8') as f:
        f.write("# 示例插件：日志与写作工具\n\n"
                "官方示例插件，示范 `.htmed` 的四类贡献点（命令 / 格式 / 主题 / 侧栏页面）。\n"
                "安装：设置 → 插件 → 导入插件包（用构建产物 "
                "`<build>/plugins/hutaomu.demo-toolkit.htmed`）。\n")

    # 3) 预览图（复用主题生成器的预览渲染）
    try:
        import importlib.util
        spec = importlib.util.spec_from_file_location(
            "make_themes", os.path.join(REPO, 'themes', 'tools', 'make_themes.py'))
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        module.render_preview(
            os.path.join(PLUGIN_DIR, 'themes', 'sunrise', 'preview.png'),
            theme['colors'])
        print('preview rendered')
    except Exception as error:   # 预览图缺失不影响插件可用
        print('preview skipped:', error)

    print('generated plugin at', PLUGIN_DIR)


if __name__ == '__main__':
    main()
