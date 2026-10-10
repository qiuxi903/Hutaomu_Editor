cmake_minimum_required(VERSION 3.21)

project(HutaomuEditor
    VERSION 0.1.0
    DESCRIPTION "Hutaomu Editor - native text, code, and Markdown editor"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Core5Compat Svg)

qt_standard_project_setup()

set(HUTAOMU_SOURCES
    src/main.cpp
    src/app/ActivityBar.h
    src/app/ActivityBar.cpp
    src/app/MainWindow.h
    src/app/MainWindow.cpp
    src/app/CommandIndex.h
    src/app/CommandIndex.cpp
    src/app/CommandPalette.h
    src/app/CommandPalette.cpp
    src/app/FileAssociations.h
    src/app/FileAssociations.cpp
    src/app/DropZoneOverlay.h
    src/app/DropZoneOverlay.cpp
    src/app/PanelColumn.h
    src/app/PanelColumn.cpp
    src/app/PanelDragMover.h
    src/app/PanelDragMover.cpp
    src/app/QuickOpenDialog.h
    src/app/QuickOpenDialog.cpp
    src/app/SettingsDialog.h
    src/app/SettingsDialog.cpp
    src/app/TitleBar.h
    src/app/TitleBar.cpp
    src/app/UiFx.h
    src/app/UiFx.cpp
    src/app/UiAnimations.h
    src/app/UiAnimations.cpp
    src/app/BeautifulDialog.h
    src/app/BeautifulDialog.cpp
    src/core/Document.h
    src/core/Document.cpp
    src/core/ZipWriter.h
    src/core/ZipWriter.cpp
    src/core/Encoding.h
    src/core/Encoding.cpp
    src/core/LineEndings.h
    src/editor/CodeEditor.h
    src/editor/CodeEditor.cpp
    src/editor/EditorCommands.h
    src/editor/EditorCommands.cpp
    src/editor/FindBar.h
    src/editor/FindBar.cpp
    src/editor/TableOverlayManager.h
    src/editor/TableOverlayManager.cpp
    src/editor/TouchScroller.h
    src/editor/TouchScroller.cpp
    src/markdown/MarkdownBlocks.h
    src/markdown/MarkdownBlocks.cpp
    src/markdown/MarkdownLiveHighlighter.h
    src/markdown/MarkdownLiveHighlighter.cpp
    src/markdown/MarkdownRenderer.h
    src/markdown/MarkdownRenderer.cpp
    src/panels/ExplorerPanel.h
    src/panels/ExplorerPanel.cpp
    src/panels/MarkdownPreview.h
    src/panels/MarkdownPreview.cpp
    src/panels/OutlinePanel.h
    src/panels/OutlinePanel.cpp
    src/panels/SearchPanel.h
    src/panels/SearchPanel.cpp
    src/plugins/PluginManifest.h
    src/plugins/PluginManifest.cpp
    src/plugins/PluginManager.h
    src/plugins/PluginManager.cpp
    src/plugins/PluginPagePanel.h
    src/plugins/PluginPagePanel.cpp
    src/settings/AppSettings.h
    src/settings/AppSettings.cpp
    src/syntax/LanguageRegistry.h
    src/syntax/LanguageRegistry.cpp
    src/syntax/CodeHighlightToHtml.h
    src/syntax/CodeHighlightToHtml.cpp
    src/syntax/LanguageSpec.h
    src/syntax/TreeSitterHighlighter.h
    src/syntax/TreeSitterHighlighter.cpp
    src/themes/IconLoader.h
    src/themes/IconLoader.cpp
    src/themes/ThemeManager.h
    src/themes/ThemeManager.cpp
    src/themes/UserBackground.h
    src/themes/UserBackground.cpp
    src/themes/ThemePackage.h
    src/themes/ThemePackage.cpp
    src/plugins/PluginManifest.h
    src/plugins/PluginManifest.cpp
    src/plugins/PluginManager.h
    src/plugins/PluginManager.cpp
    src/core/ZipWriter.h
    src/core/ZipWriter.cpp
    src/viewers/DocumentViewer.h
    src/viewers/DocumentViewer.cpp
    src/viewers/ImageViewer.h
    src/viewers/ImageViewer.cpp
    src/viewers/MediaViewer.h
    src/viewers/MediaViewer.cpp
    src/viewers/OfficeViewer.h
    src/viewers/OfficeViewer.cpp
    src/viewers/PdfViewer.h
    src/viewers/PdfViewer.cpp
    src/viewers/ViewerFactory.h
    src/viewers/ViewerFactory.cpp
    src/viewers/ZipReader.h
    src/viewers/ZipReader.cpp
)

qt_add_executable(HutaomuEditor WIN32
    ${HUTAOMU_SOURCES}
)

target_compile_definitions(HutaomuEditor PRIVATE
    HUTAOMU_VERSION="${PROJECT_VERSION}"
)

# Sources include project headers by module path ("core/Encoding.h").
target_include_directories(HutaomuEditor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
# pdfium（PDF 查看器渲染引擎，见 third_party/pdfium）
target_include_directories(HutaomuEditor SYSTEM PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/pdfium/include)

target_link_libraries(HutaomuEditor PRIVATE
    Qt6::Widgets
    Qt6::Core5Compat
    Qt6::Svg
)

if(WIN32)
    target_link_libraries(HutaomuEditor PRIVATE dwmapi mfplay)
endif()
# pdfium 导入库（运行时加载 third_party/pdfium/bin/pdfium.dll）
if(WIN32)
    target_link_libraries(HutaomuEditor PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/pdfium/lib/libpdfium_mingw.a)
endif()
target_link_libraries(HutaomuEditor PRIVATE z)

# BASE makes files register at PREFIX/<filename> (stripping the source
# subdirectory), matching the paths used in ThemeManager / IconLoader.
qt_add_resources(HutaomuEditor "hutaomu_themes"
    PREFIX "/themes"
    BASE src/themes
    FILES
        src/themes/template.qss
        src/themes/paper.json
        src/themes/greenwood-dark.json
        src/themes/graphite.json
)

# ---- 官方示范主题（themes/<id>/）：编译进资源，同时可打包为 .htmtpi ----
set(HUTAOMU_OFFICIAL_THEME_IDS
    typora-immersive
    obsidian-cards
    vscode-dark-plus
    vscode-light-plus
    hl-combine
    hl-lambda
    hl-portal
)
set(HUTAOMU_OFFICIAL_THEME_FILES "")
foreach(theme_id IN LISTS HUTAOMU_OFFICIAL_THEME_IDS)
    # glob 覆盖 theme.json / preview.png 与 T3 资源（assets/**）；
    # CONFIGURE_DEPENDS 让新增资源文件在下次构建时自动纳入。
    file(GLOB theme_files CONFIGURE_DEPENDS
        RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
        "themes/${theme_id}/*.json"
        "themes/${theme_id}/*.png"
        "themes/${theme_id}/assets/*.png"
        "themes/${theme_id}/assets/icons/*.svg")
    list(APPEND HUTAOMU_OFFICIAL_THEME_FILES ${theme_files})
endforeach()
# 把同一份主题源目录打包成 .htmtpi（ZIP），供分发与安装测试使用。
set(HUTAOMU_THEME_PACKAGE_FILES "")
foreach(theme_id IN LISTS HUTAOMU_OFFICIAL_THEME_IDS)
    set(package "${CMAKE_BINARY_DIR}/themes/${theme_id}.htmtpi")
    add_custom_command(OUTPUT "${package}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/themes"
        COMMAND ${CMAKE_COMMAND} -E tar cf "${package}" --format=zip -- ${theme_id}
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/themes"
        DEPENDS ${HUTAOMU_OFFICIAL_THEME_FILES}
        COMMENT "打包主题 ${theme_id}.htmtpi"
        VERBATIM)
    list(APPEND HUTAOMU_THEME_PACKAGE_FILES "${package}")
endforeach()
add_custom_target(theme_packages ALL DEPENDS ${HUTAOMU_THEME_PACKAGE_FILES})
# ---- 预装插件（P5b）：把官方插件铺到 <build>/plugins/<id>/ ----
# 运行时从"程序目录/plugins"发现预装插件；官方主题包的主题内容来自仓库 themes/。
set(HUTAOMU_PREINSTALLED_STAGE "${CMAKE_BINARY_DIR}/plugins")
set(HUTAOMU_PREINSTALLED_PLUGIN_IDS
    hutaomu.official-themes
    hutaomu.config-formats
)
set(HUTAOMU_PREINSTALL_TARGETS "")
foreach(plugin_id IN LISTS HUTAOMU_PREINSTALLED_PLUGIN_IDS)
    set(stage_dir "${HUTAOMU_PREINSTALLED_STAGE}/${plugin_id}")
    set(stamp "${CMAKE_BINARY_DIR}/preinstall-${plugin_id}.stamp")
    add_custom_command(OUTPUT "${stamp}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${stage_dir}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/plugins/${plugin_id}/plugin.json"
                "${stage_dir}/plugin.json"
        COMMAND ${CMAKE_COMMAND} -E touch "${stamp}"
        DEPENDS "${CMAKE_SOURCE_DIR}/plugins/${plugin_id}/plugin.json"
        COMMENT "铺开预装插件 ${plugin_id}"
        VERBATIM)
    list(APPEND HUTAOMU_PREINSTALL_TARGETS "${stamp}")
endforeach()
add_custom_target(preinstalled_plugins ALL DEPENDS ${HUTAOMU_PREINSTALL_TARGETS})

# 官方主题包的主题内容：构建时从 themes/ 复制进它的 themes/ 目录（单一来源）
add_custom_target(preinstalled_themes ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "${HUTAOMU_PREINSTALLED_STAGE}/hutaomu.official-themes/themes"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/themes"
            "${HUTAOMU_PREINSTALLED_STAGE}/hutaomu.official-themes/themes"
    COMMENT "复制官方主题到预装插件"
    VERBATIM)
add_dependencies(preinstalled_themes preinstalled_plugins)

# ---- 官方示例插件：打包成 .htmed（ZIP），用于安装链路与文档示例 ----
set(HUTAOMU_DEMO_PLUGIN_ID hutaomu.demo-toolkit)
set(HUTAOMU_DEMO_PLUGIN_DIR "${CMAKE_SOURCE_DIR}/plugins/${HUTAOMU_DEMO_PLUGIN_ID}")
set(HUTAOMU_DEMO_PLUGIN_PACKAGE "${CMAKE_BINARY_DIR}/plugins/${HUTAOMU_DEMO_PLUGIN_ID}.htmed")
file(GLOB_RECURSE HUTAOMU_DEMO_PLUGIN_FILES CONFIGURE_DEPENDS
    "${HUTAOMU_DEMO_PLUGIN_DIR}/*")
add_custom_command(OUTPUT "${HUTAOMU_DEMO_PLUGIN_PACKAGE}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/plugins"
    COMMAND ${CMAKE_COMMAND} -E tar cf "${HUTAOMU_DEMO_PLUGIN_PACKAGE}" --format=zip --
            ${HUTAOMU_DEMO_PLUGIN_ID}
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/plugins"
    DEPENDS ${HUTAOMU_DEMO_PLUGIN_FILES}
    COMMENT "打包示例插件 ${HUTAOMU_DEMO_PLUGIN_ID}.htmed"
    VERBATIM)
add_custom_target(plugin_packages ALL DEPENDS "${HUTAOMU_DEMO_PLUGIN_PACKAGE}")

# 示例插件的资源（供 PluginTest 使用：模拟"预装插件"目录）
qt_add_resources(HutaomuEditor "hutaomu_demo_plugin"
    PREFIX "/demo-plugin"
    BASE plugins/hutaomu.demo-toolkit
    FILES
        plugins/hutaomu.demo-toolkit/plugin.json
        plugins/hutaomu.demo-toolkit/themes/sunrise/theme.json
        plugins/hutaomu.demo-toolkit/themes/sunrise/preview.png
)


# 内置图标清单（应用与需要 IconLoader 的测试共用）
set(HUTAOMU_ICON_FILES
    src/resources/icons/brand.svg
    src/resources/icons/activity-about.svg
    src/resources/icons/activity-explorer.svg
    src/resources/icons/activity-extensions.svg
    src/resources/icons/activity-scm.svg
    src/resources/icons/activity-search.svg
    src/resources/icons/collapse-all.svg
    src/resources/icons/folder-open.svg
    src/resources/icons/refresh.svg
    src/resources/icons/tab-add.svg
    src/resources/icons/win-close.svg
    src/resources/icons/win-max.svg
    src/resources/icons/win-min.svg
    src/resources/icons/win-restore.svg
)
qt_add_resources(HutaomuEditor "hutaomu_icons"
    PREFIX "/icons"
    BASE src/resources/icons
    FILES ${HUTAOMU_ICON_FILES}
)

# --- tree-sitter 运行时与各语言语法（第三方源码，见 third_party/） ---

add_library(hutaomu_ts_runtime STATIC third_party/tree-sitter/lib/src/lib.c)
target_include_directories(hutaomu_ts_runtime SYSTEM PRIVATE
    third_party/tree-sitter/lib/include
    third_party/tree-sitter/lib/src)
target_compile_options(hutaomu_ts_runtime PRIVATE -O2 -w)

function(hutaomu_add_grammar name srcdir)
    file(GLOB HUTAOMU_GRAMMAR_SOURCES CONFIGURE_DEPENDS
        "${srcdir}/parser.c"
        "${srcdir}/scanner*.c")
    if(NOT HUTAOMU_GRAMMAR_SOURCES)
        message(FATAL_ERROR "grammar ${name}: no sources under ${srcdir}")
    endif()
    add_library(hutaomu_grammar_${name} STATIC ${HUTAOMU_GRAMMAR_SOURCES})
    target_include_directories(hutaomu_grammar_${name} SYSTEM PRIVATE ${srcdir})
    target_compile_options(hutaomu_grammar_${name} PRIVATE -O2 -w)
endfunction()

hutaomu_add_grammar(c           third_party/tree-sitter-c/src)
hutaomu_add_grammar(cpp         third_party/tree-sitter-cpp/src)
hutaomu_add_grammar(c-sharp     third_party/tree-sitter-c-sharp/src)
hutaomu_add_grammar(java        third_party/tree-sitter-java/src)
hutaomu_add_grammar(javascript  third_party/tree-sitter-javascript/src)
hutaomu_add_grammar(typescript  third_party/tree-sitter-typescript/typescript/src)
hutaomu_add_grammar(python      third_party/tree-sitter-python/src)
hutaomu_add_grammar(rust        third_party/tree-sitter-rust/src)
hutaomu_add_grammar(go          third_party/tree-sitter-go/src)
hutaomu_add_grammar(json        third_party/tree-sitter-json/src)
hutaomu_add_grammar(html        third_party/tree-sitter-html/src)
hutaomu_add_grammar(css         third_party/tree-sitter-css/src)
hutaomu_add_grammar(bash        third_party/tree-sitter-bash/src)
hutaomu_add_grammar(yaml        third_party/tree-sitter-yaml/src)
hutaomu_add_grammar(toml        third_party/tree-sitter-toml/src)
hutaomu_add_grammar(php         third_party/tree-sitter-php/php/src)
hutaomu_add_grammar(xml         third_party/tree-sitter-xml/xml/src)

set(HUTAOMU_GRAMMAR_LIBS
    hutaomu_grammar_c hutaomu_grammar_cpp hutaomu_grammar_c-sharp
    hutaomu_grammar_java hutaomu_grammar_javascript hutaomu_grammar_typescript
    hutaomu_grammar_python hutaomu_grammar_rust hutaomu_grammar_go
    hutaomu_grammar_json hutaomu_grammar_html hutaomu_grammar_css
    hutaomu_grammar_bash hutaomu_grammar_yaml hutaomu_grammar_toml
    hutaomu_grammar_php hutaomu_grammar_xml)

target_include_directories(HutaomuEditor SYSTEM PRIVATE
    third_party/tree-sitter/lib/include)

target_link_libraries(HutaomuEditor PRIVATE
    Qt6::Widgets
    Qt6::Core5Compat
    Qt6::Svg
    hutaomu_ts_runtime
    ${HUTAOMU_GRAMMAR_LIBS}
)

# 高亮 query 资源
qt_add_resources(HutaomuEditor "hutaomu_grammars"
    PREFIX "/grammars"
    BASE third_party
    FILES
        third_party/tree-sitter-c/queries/highlights.scm
        third_party/tree-sitter-cpp/queries/highlights.scm
        third_party/tree-sitter-c-sharp/queries/highlights.scm
        third_party/tree-sitter-java/queries/highlights.scm
        third_party/tree-sitter-javascript/queries/highlights.scm
        third_party/tree-sitter-typescript/queries/highlights.scm
        third_party/tree-sitter-python/queries/highlights.scm
        third_party/tree-sitter-rust/queries/highlights.scm
        third_party/tree-sitter-go/queries/highlights.scm
        third_party/tree-sitter-json/queries/highlights.scm
        third_party/tree-sitter-html/queries/highlights.scm
        third_party/tree-sitter-css/queries/highlights.scm
        third_party/tree-sitter-bash/queries/highlights.scm
        third_party/tree-sitter-yaml/queries/highlights.scm
        third_party/tree-sitter-toml/queries/highlights.scm
        third_party/tree-sitter-php/queries/highlights.scm
        third_party/tree-sitter-xml/queries/xml/highlights.scm
)

# md4c（Markdown 解析，M2 预览用）
add_library(hutaomu_md4c STATIC
    third_party/md4c/src/md4c.c
    third_party/md4c/src/entity.c)
target_include_directories(hutaomu_md4c SYSTEM PUBLIC third_party/md4c/src)
target_compile_options(hutaomu_md4c PRIVATE -O2 -w)
target_link_libraries(HutaomuEditor PRIVATE hutaomu_md4c)

if(MSVC)
    target_compile_options(HutaomuEditor PRIVATE /W4 /permissive-)
else()
    target_compile_options(HutaomuEditor PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(HutaomuEditor PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "Hutaomu Editor"
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
)

# --- Tests ---
enable_testing()

add_executable(EncodingTest tests/EncodingTest.cpp
    src/core/Encoding.h
    src/core/Encoding.cpp
)
target_compile_definitions(EncodingTest PRIVATE
    HUTAOMU_VERSION="${PROJECT_VERSION}"
)
target_include_directories(EncodingTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(EncodingTest PRIVATE Qt6::Core Qt6::Core5Compat)
add_test(NAME EncodingTest COMMAND EncodingTest)

# --- tree-sitter 集成自测 ---
add_executable(TreeSitterTest tests/TreeSitterTest.cpp
    src/syntax/LanguageSpec.h
    src/syntax/LanguageRegistry.h
    src/syntax/LanguageRegistry.cpp
    src/core/Encoding.h
    src/core/Encoding.cpp
)
target_compile_definitions(TreeSitterTest PRIVATE
    HUTAOMU_VERSION="${PROJECT_VERSION}"
)
target_include_directories(TreeSitterTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    third_party/tree-sitter/lib/include)
target_link_libraries(TreeSitterTest PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Core5Compat
    hutaomu_ts_runtime ${HUTAOMU_GRAMMAR_LIBS})
qt_add_resources(TreeSitterTest "ts_test_grammars"
    PREFIX "/grammars"
    BASE third_party
    FILES
        third_party/tree-sitter-c/queries/highlights.scm
        third_party/tree-sitter-cpp/queries/highlights.scm
        third_party/tree-sitter-c-sharp/queries/highlights.scm
        third_party/tree-sitter-java/queries/highlights.scm
        third_party/tree-sitter-javascript/queries/highlights.scm
        third_party/tree-sitter-typescript/queries/highlights.scm
        third_party/tree-sitter-python/queries/highlights.scm
        third_party/tree-sitter-rust/queries/highlights.scm
        third_party/tree-sitter-go/queries/highlights.scm
        third_party/tree-sitter-json/queries/highlights.scm
        third_party/tree-sitter-html/queries/highlights.scm
        third_party/tree-sitter-css/queries/highlights.scm
        third_party/tree-sitter-bash/queries/highlights.scm
        third_party/tree-sitter-yaml/queries/highlights.scm
        third_party/tree-sitter-toml/queries/highlights.scm
        third_party/tree-sitter-php/queries/highlights.scm
        third_party/tree-sitter-xml/queries/xml/highlights.scm
)
add_test(NAME TreeSitterTest COMMAND TreeSitterTest)

# --- 注释切换自测 ---
add_executable(CommentTest tests/CommentTest.cpp
    src/editor/EditorCommands.h
    src/editor/EditorCommands.cpp
    src/syntax/LanguageSpec.h
    src/syntax/LanguageRegistry.h
    src/syntax/LanguageRegistry.cpp
)
target_compile_definitions(CommentTest PRIVATE HUTAOMU_VERSION="${PROJECT_VERSION}")
target_include_directories(CommentTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(CommentTest PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets
    hutaomu_ts_runtime ${HUTAOMU_GRAMMAR_LIBS})
add_test(NAME CommentTest COMMAND CommentTest)

# --- Markdown 功能自测 ---
add_executable(MarkdownTest tests/MarkdownTest.cpp
    src/editor/TouchScroller.h
    src/editor/TouchScroller.cpp
    src/markdown/MarkdownBlocks.h
    src/markdown/MarkdownBlocks.cpp
    src/markdown/MarkdownLiveHighlighter.h
    src/markdown/MarkdownLiveHighlighter.cpp
    src/markdown/MarkdownRenderer.h
    src/markdown/MarkdownRenderer.cpp
    src/panels/OutlinePanel.h
    src/panels/OutlinePanel.cpp
    src/syntax/CodeHighlightToHtml.h
    src/syntax/CodeHighlightToHtml.cpp
    src/syntax/LanguageSpec.h
    src/syntax/LanguageRegistry.h
    src/syntax/LanguageRegistry.cpp
    src/themes/ThemeManager.h
    src/themes/ThemeManager.cpp
    src/themes/ThemePackage.h
    src/themes/ThemePackage.cpp
    src/plugins/PluginManifest.h
    src/plugins/PluginManifest.cpp
    src/plugins/PluginManager.h
    src/plugins/PluginManager.cpp
    src/core/ZipWriter.h
    src/core/ZipWriter.cpp
    src/viewers/ZipReader.h
    src/viewers/ZipReader.cpp
)
target_compile_definitions(MarkdownTest PRIVATE HUTAOMU_VERSION="${PROJECT_VERSION}")
target_include_directories(MarkdownTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    third_party/tree-sitter/lib/include)
target_link_libraries(MarkdownTest PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Core5Compat Qt6::Svg z
    hutaomu_ts_runtime hutaomu_md4c ${HUTAOMU_GRAMMAR_LIBS})
qt_add_resources(MarkdownTest "md_test_res"
    PREFIX "/"
    BASE .
    FILES
        src/themes/template.qss
        src/themes/paper.json
        src/themes/greenwood-dark.json
        src/themes/graphite.json
        third_party/tree-sitter-c/queries/highlights.scm
        third_party/tree-sitter-cpp/queries/highlights.scm
)
add_test(NAME MarkdownTest COMMAND MarkdownTest)

# --- 实时模式折叠表格几何回归 ---
add_executable(TableFoldTest tests/TableFoldTest.cpp
    src/themes/UserBackground.h
    src/themes/UserBackground.cpp
    src/editor/CodeEditor.h
    src/editor/CodeEditor.cpp
    src/editor/FindBar.h
    src/editor/FindBar.cpp
    src/editor/TableOverlayManager.h
    src/editor/TableOverlayManager.cpp
    src/editor/TouchScroller.h
    src/editor/TouchScroller.cpp
    src/markdown/MarkdownBlocks.h
    src/markdown/MarkdownBlocks.cpp
    src/markdown/MarkdownLiveHighlighter.h
    src/markdown/MarkdownLiveHighlighter.cpp
    src/markdown/MarkdownRenderer.h
    src/markdown/MarkdownRenderer.cpp
    src/panels/OutlinePanel.h
    src/panels/OutlinePanel.cpp
    src/syntax/CodeHighlightToHtml.h
    src/syntax/CodeHighlightToHtml.cpp
    src/syntax/LanguageSpec.h
    src/syntax/LanguageRegistry.h
    src/syntax/LanguageRegistry.cpp
    src/syntax/TreeSitterHighlighter.h
    src/syntax/TreeSitterHighlighter.cpp
    src/themes/ThemeManager.h
    src/themes/ThemeManager.cpp
    src/themes/ThemePackage.h
    src/themes/ThemePackage.cpp
    src/plugins/PluginManifest.h
    src/plugins/PluginManifest.cpp
    src/plugins/PluginManager.h
    src/plugins/PluginManager.cpp
    src/core/ZipWriter.h
    src/core/ZipWriter.cpp
    src/viewers/ZipReader.h
    src/viewers/ZipReader.cpp
)
target_compile_definitions(TableFoldTest PRIVATE HUTAOMU_VERSION="${PROJECT_VERSION}")
target_include_directories(TableFoldTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    third_party/tree-sitter/lib/include)
target_link_libraries(TableFoldTest PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Core5Compat Qt6::Svg z
    hutaomu_ts_runtime hutaomu_md4c ${HUTAOMU_GRAMMAR_LIBS})
qt_add_resources(TableFoldTest "tf_test_res"
    PREFIX "/themes"
    BASE src/themes
    FILES
        src/themes/template.qss
        src/themes/paper.json
        src/themes/greenwood-dark.json
        src/themes/graphite.json
)
add_test(NAME TableFoldTest COMMAND TableFoldTest)

# --- 文档查看器（图片/视频/PDF/Office） ---
add_executable(ViewerTest tests/ViewerTest.cpp
    src/core/ZipWriter.h
    src/core/ZipWriter.cpp
    src/viewers/DocumentViewer.h
    src/viewers/DocumentViewer.cpp
    src/viewers/ImageViewer.h
    src/viewers/ImageViewer.cpp
    src/viewers/MediaViewer.h
    src/viewers/MediaViewer.cpp
    src/viewers/OfficeViewer.h
    src/viewers/OfficeViewer.cpp
    src/viewers/PdfViewer.h
    src/viewers/PdfViewer.cpp
    src/viewers/ViewerFactory.h
    src/viewers/ViewerFactory.cpp
    src/viewers/ZipReader.h
    src/viewers/ZipReader.cpp
)
target_include_directories(ViewerTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(ViewerTest SYSTEM PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/pdfium/include)
if(WIN32)
    target_link_libraries(ViewerTest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/pdfium/lib/libpdfium_mingw.a)
endif()
if(WIN32)
    target_link_libraries(ViewerTest PRIVATE mfplay)
endif()
target_link_libraries(ViewerTest PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets z)
add_test(NAME ViewerTest COMMAND ViewerTest)

# --- 查看器模糊健壮性（垃圾输入不崩溃） ---
add_executable(ViewerFuzzTest tests/ViewerFuzzTest.cpp
    src/core/ZipWriter.h
    src/core/ZipWriter.cpp
    src/viewers/DocumentViewer.h
    src/viewers/DocumentViewer.cpp
    src/viewers/ImageViewer.h
    src/viewers/ImageViewer.cpp
    src/viewers/MediaViewer.h
    src/viewers/MediaViewer.cpp
    src/viewers/OfficeViewer.h
    src/viewers/OfficeViewer.cpp
    src/viewers/PdfViewer.h
    src/viewers/PdfViewer.cpp
    src/viewers/ViewerFactory.h
    src/viewers/ViewerFactory.cpp
    src/viewers/ZipReader.h
    src/viewers/ZipReader.cpp
)
target_include_directories(ViewerFuzzTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(ViewerFuzzTest SYSTEM PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/pdfium/include)
if(WIN32)
    target_link_libraries(ViewerFuzzTest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/pdfium/lib/libpdfium_mingw.a)
endif()
if(WIN32)
    target_link_libraries(ViewerFuzzTest PRIVATE mfplay)
endif()
target_link_libraries(ViewerFuzzTest PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets z)
add_test(NAME ViewerFuzzTest COMMAND ViewerFuzzTest)

# --- 查看器标签页集成回归（空指针崩溃） ---
set(VIEWERTAB_SOURCES ${HUTAOMU_SOURCES})
list(REMOVE_ITEM VIEWERTAB_SOURCES src/main.cpp)
add_executable(ViewerTabTest tests/ViewerTabTest.cpp
    ${VIEWERTAB_SOURCES}
)
target_compile_definitions(ViewerTabTest PRIVATE HUTAOMU_VERSION="${PROJECT_VERSION}")
target_include_directories(ViewerTabTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    third_party/tree-sitter/lib/include)
target_include_directories(ViewerTabTest SYSTEM PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/pdfium/include)
if(WIN32)
    target_link_libraries(ViewerTabTest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/pdfium/lib/libpdfium_mingw.a)
endif()
if(WIN32)
    target_link_libraries(ViewerTabTest PRIVATE mfplay)
endif()
target_link_libraries(ViewerTabTest PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Core5Compat Qt6::Svg z
    hutaomu_ts_runtime hutaomu_md4c ${HUTAOMU_GRAMMAR_LIBS})
qt_add_resources(ViewerTabTest "vt_test_res"
    PREFIX "/themes"
    BASE src/themes
    FILES
        src/themes/template.qss
        src/themes/paper.json
        src/themes/greenwood-dark.json
        src/themes/graphite.json
)
qt_add_resources(ViewerTabTest "vt_test_grammars"
    PREFIX "/grammars"
    BASE third_party
    FILES
        third_party/tree-sitter-c/queries/highlights.scm
        third_party/tree-sitter-cpp/queries/highlights.scm
)
add_test(NAME ViewerTabTest COMMAND ViewerTabTest)

# --- 大纲自动显隐回归 ---
set(OUTLINEHIDE_SOURCES ${HUTAOMU_SOURCES})
list(REMOVE_ITEM OUTLINEHIDE_SOURCES src/main.cpp)
add_executable(OutlineAutoHideTest tests/OutlineAutoHideTest.cpp
    ${OUTLINEHIDE_SOURCES}
)
target_compile_definitions(OutlineAutoHideTest PRIVATE HUTAOMU_VERSION="${PROJECT_VERSION}")
target_include_directories(OutlineAutoHideTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    third_party/tree-sitter/lib/include)
target_include_directories(OutlineAutoHideTest SYSTEM PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/pdfium/include)
if(WIN32)
    target_link_libraries(OutlineAutoHideTest PRIVATE mfplay
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/pdfium/lib/libpdfium_mingw.a)
endif()
target_link_libraries(OutlineAutoHideTest PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Core5Compat Qt6::Svg z
    hutaomu_ts_runtime hutaomu_md4c ${HUTAOMU_GRAMMAR_LIBS})
qt_add_resources(OutlineAutoHideTest "oh_test_res"
    PREFIX "/themes"
    BASE src/themes
    FILES
        src/themes/template.qss
        src/themes/paper.json
        src/themes/greenwood-dark.json
        src/themes/graphite.json
)
qt_add_resources(OutlineAutoHideTest "oh_test_grammars"
    PREFIX "/grammars"
    BASE third_party
    FILES
        third_party/tree-sitter-c/queries/highlights.scm
        third_party/tree-sitter-cpp/queries/highlights.scm
)
add_test(NAME OutlineAutoHideTest COMMAND OutlineAutoHideTest)

# --- 面板拖动往返 + PDF 翻页回归 ---
set(PANELPDF_SOURCES ${HUTAOMU_SOURCES})
list(REMOVE_ITEM PANELPDF_SOURCES src/main.cpp)
add_executable(PanelPdfRegressionTest tests/PanelPdfRegressionTest.cpp
    ${PANELPDF_SOURCES}
)
target_compile_definitions(PanelPdfRegressionTest PRIVATE HUTAOMU_VERSION="${PROJECT_VERSION}")
target_include_directories(PanelPdfRegressionTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    third_party/tree-sitter/lib/include)
target_include_directories(PanelPdfRegressionTest SYSTEM PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/pdfium/include)
if(WIN32)
    target_link_libraries(PanelPdfRegressionTest PRIVATE mfplay
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/pdfium/lib/libpdfium_mingw.a)
endif()
target_link_libraries(PanelPdfRegressionTest PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Core5Compat Qt6::Svg z
    hutaomu_ts_runtime hutaomu_md4c ${HUTAOMU_GRAMMAR_LIBS})
qt_add_resources(PanelPdfRegressionTest "pp_test_res"
    PREFIX "/themes"
    BASE src/themes
    FILES
        src/themes/template.qss
        src/themes/paper.json
        src/themes/greenwood-dark.json
        src/themes/graphite.json
)
qt_add_resources(PanelPdfRegressionTest "pp_test_grammars"
    PREFIX "/grammars"
    BASE third_party
    FILES
        third_party/tree-sitter-c/queries/highlights.scm
        third_party/tree-sitter-cpp/queries/highlights.scm
)
add_test(NAME PanelPdfRegressionTest COMMAND PanelPdfRegressionTest)

# --- pdfium.dll 部署到各输出目录（运行时动态加载） ---
if(WIN32)
    add_custom_command(TARGET HutaomuEditor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party/pdfium/bin/pdfium.dll
            $<TARGET_FILE_DIR:HutaomuEditor>)
    add_custom_command(TARGET ViewerTest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party/pdfium/bin/pdfium.dll
            $<TARGET_FILE_DIR:ViewerTest>)
    add_custom_command(TARGET ViewerFuzzTest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party/pdfium/bin/pdfium.dll
            $<TARGET_FILE_DIR:ViewerFuzzTest>)
    add_custom_command(TARGET ViewerTabTest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party/pdfium/bin/pdfium.dll
            $<TARGET_FILE_DIR:ViewerTabTest>)
endif()

# --- 面板拖拽换边 + 布局位置回归 ---
set(LAYOUTDRAG_SOURCES ${HUTAOMU_SOURCES})
list(REMOVE_ITEM LAYOUTDRAG_SOURCES src/main.cpp)
add_executable(LayoutDragTest tests/LayoutDragTest.cpp
    ${LAYOUTDRAG_SOURCES}
)
target_compile_definitions(LayoutDragTest PRIVATE HUTAOMU_VERSION="${PROJECT_VERSION}")
target_include_directories(LayoutDragTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    third_party/tree-sitter/lib/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/pdfium/include)
target_link_libraries(LayoutDragTest PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Core5Compat Qt6::Svg z
    hutaomu_ts_runtime hutaomu_md4c ${HUTAOMU_GRAMMAR_LIBS})
if(WIN32)
    target_link_libraries(LayoutDragTest PRIVATE mfplay
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/pdfium/lib/libpdfium_mingw.a)
endif()
qt_add_resources(LayoutDragTest "ld_test_res"
    PREFIX "/themes"
    BASE src/themes
    FILES
        src/themes/template.qss
        src/themes/paper.json
        src/themes/greenwood-dark.json
        src/themes/graphite.json
)
qt_add_resources(LayoutDragTest "ld_test_grammars"
    PREFIX "/grammars"
    BASE third_party
    FILES
        third_party/tree-sitter-c/queries/highlights.scm
        third_party/tree-sitter-cpp/queries/highlights.scm
)
add_test(NAME LayoutDragTest COMMAND LayoutDragTest)

# --- 视图模式切换崩溃回归 ---
set(VIEWMODE_SOURCES ${HUTAOMU_SOURCES})
list(REMOVE_ITEM VIEWMODE_SOURCES src/main.cpp)
add_executable(ViewModeTest tests/ViewModeTest.cpp
    ${VIEWMODE_SOURCES}
)
target_compile_definitions(ViewModeTest PRIVATE HUTAOMU_VERSION="${PROJECT_VERSION}")
target_include_directories(ViewModeTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    third_party/tree-sitter/lib/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/pdfium/include)
target_link_libraries(ViewModeTest PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Core5Compat Qt6::Svg z
    hutaomu_ts_runtime hutaomu_md4c ${HUTAOMU_GRAMMAR_LIBS})
if(WIN32)
    target_link_libraries(ViewModeTest PRIVATE mfplay
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/pdfium/lib/libpdfium_mingw.a)
endif()
qt_add_resources(ViewModeTest "vm_test_res"
    PREFIX "/themes"
    BASE src/themes
    FILES
        src/themes/template.qss
        src/themes/paper.json
        src/themes/greenwood-dark.json
        src/themes/graphite.json
)
# 官方示范主题也进该测试资源：主题切换回归需要真实的主题包内容。
qt_add_resources(ViewModeTest "vm_test_official"
    PREFIX "/themes"
    BASE themes
    FILES ${HUTAOMU_OFFICIAL_THEME_FILES}
)
qt_add_resources(ViewModeTest "vm_test_grammars"
    PREFIX "/grammars"
    BASE third_party
    FILES
        third_party/tree-sitter-c/queries/highlights.scm
        third_party/tree-sitter-cpp/queries/highlights.scm
)
add_test(NAME ViewModeTest COMMAND ViewModeTest)

# --- 主题包（.htmtpi）安装 / T2 度量回归 ---
add_executable(ThemePackageTest tests/ThemePackageTest.cpp
    src/themes/ThemeManager.h
    src/themes/ThemeManager.cpp
    src/themes/ThemePackage.h
    src/themes/ThemePackage.cpp
    src/plugins/PluginManifest.h
    src/plugins/PluginManifest.cpp
    src/plugins/PluginManager.h
    src/plugins/PluginManager.cpp
    src/core/ZipWriter.h
    src/core/ZipWriter.cpp
    src/viewers/ZipReader.h
    src/viewers/ZipReader.cpp
)
target_include_directories(ThemePackageTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(ThemePackageTest PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets
    z)
qt_add_resources(ThemePackageTest "tp_test_res"
    PREFIX "/themes"
    BASE src/themes
    FILES
        src/themes/template.qss
        src/themes/paper.json
        src/themes/greenwood-dark.json
        src/themes/graphite.json
)
qt_add_resources(ThemePackageTest "tp_test_official"
    PREFIX "/themes"
    BASE themes
    FILES ${HUTAOMU_OFFICIAL_THEME_FILES}
)
add_test(NAME ThemePackageTest COMMAND ThemePackageTest)

# --- 主题资源覆盖（T3）回归：logo/图标覆盖、背景图规格与渲染 ---
add_executable(ThemeAssetTest tests/ThemeAssetTest.cpp
    src/core/ZipWriter.h
    src/core/ZipWriter.cpp
    src/themes/UserBackground.h
    src/themes/UserBackground.cpp
    src/editor/CodeEditor.h
    src/editor/CodeEditor.cpp
    src/editor/FindBar.h
    src/editor/FindBar.cpp
    src/editor/TableOverlayManager.h
    src/editor/TableOverlayManager.cpp
    src/editor/TouchScroller.h
    src/editor/TouchScroller.cpp
    src/markdown/MarkdownBlocks.h
    src/markdown/MarkdownBlocks.cpp
    src/markdown/MarkdownLiveHighlighter.h
    src/markdown/MarkdownLiveHighlighter.cpp
    src/markdown/MarkdownRenderer.h
    src/markdown/MarkdownRenderer.cpp
    src/panels/OutlinePanel.h
    src/panels/OutlinePanel.cpp
    src/syntax/CodeHighlightToHtml.h
    src/syntax/CodeHighlightToHtml.cpp
    src/syntax/LanguageSpec.h
    src/syntax/LanguageRegistry.h
    src/syntax/LanguageRegistry.cpp
    src/syntax/TreeSitterHighlighter.h
    src/syntax/TreeSitterHighlighter.cpp
    src/themes/IconLoader.h
    src/themes/IconLoader.cpp
    src/themes/ThemeManager.h
    src/themes/ThemeManager.cpp
    src/themes/ThemePackage.h
    src/themes/ThemePackage.cpp
    src/plugins/PluginManifest.h
    src/plugins/PluginManifest.cpp
    src/plugins/PluginManager.h
    src/plugins/PluginManager.cpp
    src/core/ZipWriter.h
    src/core/ZipWriter.cpp
    src/viewers/ZipReader.h
    src/viewers/ZipReader.cpp
)
target_include_directories(ThemeAssetTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    third_party/tree-sitter/lib/include)
target_link_libraries(ThemeAssetTest PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Svg z
    hutaomu_ts_runtime hutaomu_md4c ${HUTAOMU_GRAMMAR_LIBS})
qt_add_resources(ThemeAssetTest "ta_test_res"
    PREFIX "/themes"
    BASE src/themes
    FILES
        src/themes/template.qss
        src/themes/paper.json
        src/themes/greenwood-dark.json
        src/themes/graphite.json
)
qt_add_resources(ThemeAssetTest "ta_test_official"
    PREFIX "/themes"
    BASE themes
    FILES ${HUTAOMU_OFFICIAL_THEME_FILES}
)
qt_add_resources(ThemeAssetTest "ta_test_icons"
    PREFIX "/icons"
    BASE src/resources/icons
    FILES ${HUTAOMU_ICON_FILES}
)
qt_add_resources(ThemeAssetTest "ta_test_grammars"
    PREFIX "/grammars"
    BASE third_party
    FILES
        third_party/tree-sitter-c/queries/highlights.scm
        third_party/tree-sitter-cpp/queries/highlights.scm
)
add_test(NAME ThemeAssetTest COMMAND ThemeAssetTest)

# --- 主题形态（T4）回归：layout.json 解析 + 界面形态生效/还原 ---
set(THEMELAYOUT_SOURCES ${HUTAOMU_SOURCES})
list(REMOVE_ITEM THEMELAYOUT_SOURCES src/main.cpp)
add_executable(ThemeLayoutTest tests/ThemeLayoutTest.cpp
    ${THEMELAYOUT_SOURCES}
)
target_compile_definitions(ThemeLayoutTest PRIVATE HUTAOMU_VERSION="${PROJECT_VERSION}")
target_include_directories(ThemeLayoutTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    third_party/tree-sitter/lib/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/pdfium/include)
target_link_libraries(ThemeLayoutTest PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Core5Compat Qt6::Svg z
    hutaomu_ts_runtime hutaomu_md4c ${HUTAOMU_GRAMMAR_LIBS})
if(WIN32)
    target_link_libraries(ThemeLayoutTest PRIVATE mfplay
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/pdfium/lib/libpdfium_mingw.a)
endif()
qt_add_resources(ThemeLayoutTest "tl_test_res"
    PREFIX "/themes"
    BASE src/themes
    FILES
        src/themes/template.qss
        src/themes/paper.json
        src/themes/greenwood-dark.json
        src/themes/graphite.json
)
qt_add_resources(ThemeLayoutTest "tl_test_official"
    PREFIX "/themes"
    BASE themes
    FILES ${HUTAOMU_OFFICIAL_THEME_FILES}
)
qt_add_resources(ThemeLayoutTest "tl_test_grammars"
    PREFIX "/grammars"
    BASE third_party
    FILES
        third_party/tree-sitter-c/queries/highlights.scm
        third_party/tree-sitter-cpp/queries/highlights.scm
)
add_test(NAME ThemeLayoutTest COMMAND ThemeLayoutTest)

# --- 声明式插件（L1）回归：manifest / 安装 / 贡献点 / 启停卸载 ---
add_executable(PluginTest tests/PluginTest.cpp
    src/core/ZipWriter.h
    src/core/ZipWriter.cpp
    src/plugins/PluginManifest.h
    src/plugins/PluginManifest.cpp
    src/plugins/PluginManager.h
    src/plugins/PluginManager.cpp
    src/syntax/LanguageSpec.h
    src/syntax/LanguageRegistry.h
    src/syntax/LanguageRegistry.cpp
    src/themes/ThemeManager.h
    src/themes/ThemeManager.cpp
    src/themes/ThemePackage.h
    src/themes/ThemePackage.cpp
    src/plugins/PluginManifest.h
    src/plugins/PluginManifest.cpp
    src/plugins/PluginManager.h
    src/plugins/PluginManager.cpp
    src/core/ZipWriter.h
    src/core/ZipWriter.cpp
    src/viewers/ZipReader.h
    src/viewers/ZipReader.cpp
)
target_include_directories(PluginTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    third_party/tree-sitter/lib/include)
target_link_libraries(PluginTest PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Core5Compat
    hutaomu_ts_runtime ${HUTAOMU_GRAMMAR_LIBS} z)
qt_add_resources(PluginTest "pt_test_res"
    PREFIX "/themes"
    BASE src/themes
    FILES
        src/themes/template.qss
        src/themes/paper.json
        src/themes/greenwood-dark.json
        src/themes/graphite.json
)
qt_add_resources(PluginTest "pt_test_grammars"
    PREFIX "/grammars"
    BASE third_party
    FILES
        third_party/tree-sitter-c/queries/highlights.scm
)
add_test(NAME PluginTest COMMAND PluginTest)

# --- 命令面板（P5）回归：条目汇总 + 模糊过滤 ---
add_executable(CommandPaletteTest tests/CommandPaletteTest.cpp
    src/app/CommandIndex.h
    src/app/CommandIndex.cpp
    src/app/CommandPalette.h
    src/app/CommandPalette.cpp
    src/plugins/PluginManifest.h
    src/plugins/PluginManifest.cpp
    src/plugins/PluginManager.h
    src/plugins/PluginManager.cpp
    src/themes/ThemeManager.h
    src/themes/ThemeManager.cpp
    src/themes/ThemePackage.h
    src/themes/ThemePackage.cpp
    src/core/ZipWriter.h
    src/core/ZipWriter.cpp
    src/viewers/ZipReader.h
    src/viewers/ZipReader.cpp
)
target_include_directories(CommandPaletteTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(CommandPaletteTest PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets z)
qt_add_resources(CommandPaletteTest "cp_test_res"
    PREFIX "/themes"
    BASE src/themes
    FILES
        src/themes/template.qss
        src/themes/paper.json
        src/themes/greenwood-dark.json
        src/themes/graphite.json
)
add_test(NAME CommandPaletteTest COMMAND CommandPaletteTest)

# --- xlsx 呈现层（A 路线）回归：sharedStrings/多表/合并/样式/数字格式 ---
add_executable(OfficeXlsxTest tests/OfficeXlsxTest.cpp
    src/core/ZipWriter.h
    src/core/ZipWriter.cpp
    src/plugins/PluginManifest.h
    src/plugins/PluginManifest.cpp
    src/plugins/PluginManager.h
    src/plugins/PluginManager.cpp
    src/themes/ThemeManager.h
    src/themes/ThemeManager.cpp
    src/themes/ThemePackage.h
    src/themes/ThemePackage.cpp
    src/viewers/DocumentViewer.h
    src/viewers/DocumentViewer.cpp
    src/viewers/OfficeViewer.h
    src/viewers/OfficeViewer.cpp
    src/viewers/ZipReader.h
    src/viewers/ZipReader.cpp
)
target_compile_definitions(OfficeXlsxTest PRIVATE HUTAOMU_VERSION="${PROJECT_VERSION}")
target_include_directories(OfficeXlsxTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(OfficeXlsxTest PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Core5Compat Qt6::Svg z)
qt_add_resources(OfficeXlsxTest "ox_test_res"
    PREFIX "/themes"
    BASE src/themes
    FILES
        src/themes/template.qss
        src/themes/paper.json
        src/themes/greenwood-dark.json
        src/themes/graphite.json
)
add_test(NAME OfficeXlsxTest COMMAND OfficeXlsxTest)

# --- docx 编辑保存往返（round-trip）测试 ---
add_executable(DocxRoundTripTest tests/DocxRoundTripTest.cpp
    src/core/ZipWriter.h
    src/core/ZipWriter.cpp
    src/viewers/DocumentViewer.h
    src/viewers/DocumentViewer.cpp
    src/viewers/OfficeViewer.h
    src/viewers/OfficeViewer.cpp
    src/viewers/ZipReader.h
    src/viewers/ZipReader.cpp
)
target_include_directories(DocxRoundTripTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(DocxRoundTripTest PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Core5Compat Qt6::Svg z)
add_test(NAME DocxRoundTripTest COMMAND DocxRoundTripTest)
