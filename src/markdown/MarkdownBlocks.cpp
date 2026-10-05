// Hutaomu Editor - Markdown block scanning (tables) shared by the editor
// overlay and the live highlighter.
// SPDX-License-Identifier: LicenseRef-Proprietary
#include "MarkdownBlocks.h"

#include <QRegularExpression>

namespace markdown {
namespace {

bool isFenceLine(const QString& trimmed, int& fenceState)
{
    const bool backtick = trimmed.startsWith(QStringLiteral("```"));
    const bool tilde = trimmed.startsWith(QStringLiteral("~~~"));
    if (fenceState == 0 && (backtick || tilde)) {
        fenceState = backtick ? 1 : 2;
        return true;
    }
    if ((fenceState == 1 && backtick) || (fenceState == 2 && tilde)) {
        fenceState = 0;
        return true;
    }
    return false;
}

} // namespace

bool isTableSeparatorLine(const QString& line)
{
    static const QRegularExpression separator(
        QStringLiteral("^ {0,3}\\|?( *:?-+:? *\\|)+ *:?-+:? *\\|?\\s*$"));
    return separator.match(line).hasMatch();
}

QVector<TableRegion> scanTableRegions(const QString& markdown)
{
    QVector<TableRegion> regions;
    const QStringList lines = markdown.split(QLatin1Char('\n'));

    int fenceState = 0;
    int start = -1;

    for (int i = 0; i < lines.size(); ++i) {
        const QString trimmed = lines.at(i).trimmed();

        if (isFenceLine(trimmed, fenceState)) {
            if (start >= 0) { // 围栏直接截断未闭合的表格（容错）
                if (i - start >= 2)
                    regions.append({ start, i - 1, 0 });
                start = -1;
            }
            continue;
        }
        if (fenceState != 0)
            continue;

        const bool hasPipe = lines.at(i).contains(QLatin1Char('|'));
        if (start < 0) {
            // 表头行（含 | ）且下一行是分隔行 -> 表格开始
            if (hasPipe && i + 1 < lines.size()
                && isTableSeparatorLine(lines.at(i + 1)))
                start = i;
        } else if (!hasPipe || trimmed.isEmpty()) {
            if (i - start >= 2)
                regions.append({ start, i - 1, 0 });
            start = -1;
        }
    }
    if (start >= 0 && lines.size() - start >= 2)
        regions.append({ start, lines.size() - 1, 0 });

    // 初次扫描的估算值（像素）；refresh() 渲染后会用实测高度覆盖
    for (TableRegion& region : regions) {
        const int visibleRows = region.lastLine - region.firstLine; // 分隔行折叠
        region.collapsedHeightPx = qMax(qreal(24.0), visibleRows * 30.0 + 14.0);
    }
    return regions;
}

} // namespace markdown
