// Hutaomu Editor - Markdown to HTML renderer (md4c based, for preview pane).
// SPDX-License-Identifier: LicenseRef-Proprietary
#include "MarkdownRenderer.h"

#include <QStringList>
#include <QTextDocument>

#include "syntax/CodeHighlightToHtml.h"

extern "C" {
#include <md4c.h>
}

namespace markdown {
namespace {

QString escapeHtml(const QString& text)
{
    QString out = text;
    out.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
    out.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
    out.replace(QLatin1Char('>'), QStringLiteral("&gt;"));
    out.replace(QLatin1Char('"'), QStringLiteral("&quot;"));
    return out;
}

class Renderer {
public:
    explicit Renderer(const QString& source)
        : m_utf8(source.toUtf8())
    {
        m_renderer.abi_version = 0;
        m_renderer.flags = MD_FLAG_TABLES | MD_FLAG_STRIKETHROUGH | MD_FLAG_TASKLISTS;
        m_renderer.enter_block = &Renderer::thunkEnterBlock;
        m_renderer.leave_block = &Renderer::thunkLeaveBlock;
        m_renderer.enter_span = &Renderer::thunkEnterSpan;
        m_renderer.leave_span = &Renderer::thunkLeaveSpan;
        m_renderer.text = &Renderer::thunkText;
        m_renderer.debug_log = nullptr;
        m_renderer.syntax = nullptr;
    }

    RenderResult run()
    {
        md_parse(m_utf8.constData(), MD_SIZE(m_utf8.size()), &m_renderer, this);
        return { m_html, m_title };
    }

private:
    static int thunkEnterBlock(MD_BLOCKTYPE type, void* detail, void* ud)
    { return static_cast<Renderer*>(ud)->enterBlock(type, detail); }
    static int thunkLeaveBlock(MD_BLOCKTYPE type, void* detail, void* ud)
    { return static_cast<Renderer*>(ud)->leaveBlock(type, detail); }
    static int thunkEnterSpan(MD_SPANTYPE type, void* detail, void* ud)
    { return static_cast<Renderer*>(ud)->enterSpan(type, detail); }
    static int thunkLeaveSpan(MD_SPANTYPE type, void* detail, void* ud)
    { return static_cast<Renderer*>(ud)->leaveSpan(type, detail); }
    static int thunkText(MD_TEXTTYPE type, const MD_CHAR* data, MD_SIZE size, void* ud)
    { return static_cast<Renderer*>(ud)->text(type, QString::fromUtf8(data, int(size))); }

    static QString alignAttribute(int align)
    {
        switch (align) {
        case MD_ALIGN_LEFT:   return QStringLiteral("left");
        case MD_ALIGN_CENTER: return QStringLiteral("center");
        case MD_ALIGN_RIGHT:  return QStringLiteral("right");
        default:              return QString();
        }
    }

    static QString attrText(const MD_ATTRIBUTE& attr)
    {
        return QString::fromUtf8(attr.text, int(attr.size));
    }

    int enterBlock(MD_BLOCKTYPE type, void* detail)
    {
        switch (type) {
        case MD_BLOCK_QUOTE: m_html += QStringLiteral("<blockquote>"); break;
        case MD_BLOCK_UL: m_html += QStringLiteral("<ul>"); break;
        case MD_BLOCK_OL: {
            auto* d = static_cast<MD_BLOCK_OL_DETAIL*>(detail);
            m_html += (d->start == 1) ? QStringLiteral("<ol>")
                                      : QStringLiteral("<ol start=\"%1\">").arg(d->start);
            break;
        }
        case MD_BLOCK_LI: {
            auto* d = static_cast<MD_BLOCK_LI_DETAIL*>(detail);
            if (d->is_task) {
                const bool done = (d->task_mark == QLatin1Char('x')
                                   || d->task_mark == QLatin1Char('X'));
                m_html += done ? QStringLiteral("<li>&#9745;&nbsp;")
                               : QStringLiteral("<li>&#9744;&nbsp;");
            } else {
                m_html += QStringLiteral("<li>");
            }
            break;
        }
        case MD_BLOCK_HR: m_html += QStringLiteral("<hr/>"); break;
        case MD_BLOCK_H: {
            auto* d = static_cast<MD_BLOCK_H_DETAIL*>(detail);
            m_html += QStringLiteral("<a name=\"sec-%1\"></a>").arg(m_headingIndex);
            ++m_headingIndex;
            m_html += QStringLiteral("<h%1>").arg(d->level);
            break;
        }
        case MD_BLOCK_CODE: {
            auto* d = static_cast<MD_BLOCK_CODE_DETAIL*>(detail);
            m_codeLang = attrText(d->info).section(QLatin1Char(' '), 0, 0).toLower();
            m_codeBuffer.clear();
            m_inCodeBlock = true;
            break;
        }
        case MD_BLOCK_P: m_html += QStringLiteral("<p>"); break;
        case MD_BLOCK_TABLE:
            m_html += QStringLiteral(
                // 100% 宽铺满容器；配 table-layout:auto 让列宽按内容自适应，
                // 避免长标题被挤压截断
                "<table border=\"1\" cellspacing=\"0\" cellpadding=\"6\" "
                "width=\"100%\" style=\"table-layout:auto;\">");
            break;
        case MD_BLOCK_THEAD: m_html += QStringLiteral("<thead>"); break;
        case MD_BLOCK_TBODY: m_html += QStringLiteral("<tbody>"); break;
        case MD_BLOCK_TR: m_html += QStringLiteral("<tr>"); break;
        case MD_BLOCK_TH: {
            auto* d = static_cast<MD_BLOCK_TD_DETAIL*>(detail);
            const QString align = alignAttribute(d->align);
            m_html += align.isEmpty() ? QStringLiteral("<th>")
                                      : QStringLiteral("<th align=\"%1\">").arg(align);
            break;
        }
        case MD_BLOCK_TD: {
            auto* d = static_cast<MD_BLOCK_TD_DETAIL*>(detail);
            const QString align = alignAttribute(d->align);
            m_html += align.isEmpty() ? QStringLiteral("<td>")
                                      : QStringLiteral("<td align=\"%1\">").arg(align);
            break;
        }
        default: break;
        }
        return 0;
    }

    int leaveBlock(MD_BLOCKTYPE type, void* detail)
    {
        switch (type) {
        case MD_BLOCK_QUOTE: m_html += QStringLiteral("</blockquote>"); break;
        case MD_BLOCK_UL: m_html += QStringLiteral("</ul>"); break;
        case MD_BLOCK_OL: m_html += QStringLiteral("</ol>"); break;
        case MD_BLOCK_LI: m_html += QStringLiteral("</li>\n"); break;
        case MD_BLOCK_H: {
            auto* d = static_cast<MD_BLOCK_H_DETAIL*>(detail);
            m_html += QStringLiteral("</h%1>\n").arg(d->level);
            break;
        }
        case MD_BLOCK_CODE: {
            m_inCodeBlock = false;
            m_html += QStringLiteral("<pre>");
            const QString highlighted =
                syntax::codeToHtml(m_codeBuffer, m_codeLang);
            m_html += highlighted;
            m_html += QStringLiteral("</pre>\n");
            break;
        }
        case MD_BLOCK_P: m_html += QStringLiteral("</p>\n"); break;
        case MD_BLOCK_TABLE: m_html += QStringLiteral("</table>"); break;
        case MD_BLOCK_THEAD: m_html += QStringLiteral("</thead>"); break;
        case MD_BLOCK_TBODY: m_html += QStringLiteral("</tbody>"); break;
        case MD_BLOCK_TR: m_html += QStringLiteral("</tr>\n"); break;
        case MD_BLOCK_TH: m_html += QStringLiteral("</th>"); break;
        case MD_BLOCK_TD: m_html += QStringLiteral("</td>"); break;
        default: break;
        }
        return 0;
    }

    int enterSpan(MD_SPANTYPE type, void* detail)
    {
        switch (type) {
        case MD_SPAN_EM: m_html += QStringLiteral("<em>"); break;
        case MD_SPAN_STRONG: m_html += QStringLiteral("<strong>"); break;
        case MD_SPAN_A: {
            auto* d = static_cast<MD_SPAN_A_DETAIL*>(detail);
            m_html += QStringLiteral("<a href=\"%1\">").arg(escapeHtml(attrText(d->href)));
            break;
        }
        case MD_SPAN_IMG: {
            auto* d = static_cast<MD_SPAN_IMG_DETAIL*>(detail);
            m_html += QStringLiteral("<img src=\"%1\" alt=\"").arg(escapeHtml(attrText(d->src)));
            break;
        }
        case MD_SPAN_CODE:
            m_html += QStringLiteral("<code>");
            m_inInlineCode = true;
            break;
        case MD_SPAN_DEL: m_html += QStringLiteral("<del>"); break;
        case MD_SPAN_U: m_html += QStringLiteral("<u>"); break;
        default: break;
        }
        return 0;
    }

    int leaveSpan(MD_SPANTYPE type, void*)
    {
        switch (type) {
        case MD_SPAN_EM: m_html += QStringLiteral("</em>"); break;
        case MD_SPAN_STRONG: m_html += QStringLiteral("</strong>"); break;
        case MD_SPAN_A: m_html += QStringLiteral("</a>"); break;
        case MD_SPAN_IMG: m_html += QStringLiteral("\">"); break;
        case MD_SPAN_CODE:
            m_inInlineCode = false;
            m_html += QStringLiteral("</code>");
            break;
        case MD_SPAN_DEL: m_html += QStringLiteral("</del>"); break;
        case MD_SPAN_U: m_html += QStringLiteral("</u>"); break;
        default: break;
        }
        return 0;
    }

    int text(MD_TEXTTYPE type, const QString& content)
    {
        // 标题（用于大纲）
        if (content.trimmed().isEmpty() && m_html.endsWith(QLatin1Char('>'))
            && m_html.contains(QStringLiteral("<h1>"))) {
            // 占位；首个 h1 文本在下方捕获
        }

        switch (type) {
        case MD_TEXT_CODE:
            if (m_inCodeBlock) {
                m_codeBuffer += content;
            } else {
                m_html += escapeHtml(content);
            }
            break;
        case MD_TEXT_HTML:
            m_html += content; // 原样透传行内/块级 HTML
            break;
        case MD_TEXT_ENTITY:
            m_html += content; // 实体名已含 &..; 形式
            break;
        case MD_TEXT_BR:
            m_html += QStringLiteral("<br/>\n");
            break;
        case MD_TEXT_SOFTBR:
            m_html += QStringLiteral("\n");
            break;
        default:
            m_html += escapeHtml(content);
            if (m_html.endsWith(QStringLiteral("<h1>"))) {
                // 尚未闭合的 h1：记录标题
                if (m_title.isEmpty())
                    m_title = content.trimmed();
            }
            break;
        }
        return 0;
    }

    MD_PARSER m_renderer = {};
    QByteArray m_utf8;
    QString m_html;
    QString m_title;
    QString m_codeBuffer;
    QString m_codeLang;
    bool m_inCodeBlock = false;
    bool m_inInlineCode = false;
    int m_headingIndex = 0;
};

} // namespace

RenderResult renderToHtml(const QString& markdown)
{
    Renderer renderer(markdown);
    return renderer.run();
}

} // namespace markdown
