// Hutaomu Editor - Markdown preview pane (native QTextBrowser).
// SPDX-License-Identifier: LicenseRef-Proprietary
#include "MarkdownPreview.h"

#include <QApplication>
#include <QDesktopServices>
#include <QLabel>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QMouseEvent>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>

#include "markdown/MarkdownRenderer.h"
#include "themes/ThemeManager.h"
#include "editor/TouchScroller.h"

namespace panels {

MarkdownPreview::MarkdownPreview(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("previewPane"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // 面板标题栏：与编辑区明确区分
    m_header = new QWidget(this);
    m_header->setObjectName(QStringLiteral("previewHeader"));
    auto* headerLayout = new QHBoxLayout(m_header);
    headerLayout->setContentsMargins(12, 6, 12, 6);
    m_headerTitle = new QLabel(tr("预览"), m_header);
    m_headerTitle->setObjectName(QStringLiteral("previewHeaderTitle"));
    headerLayout->addWidget(m_headerTitle);
    headerLayout->addStretch();
    layout->addWidget(m_header);

    m_browser = new QTextBrowser(this);
    m_browser->setObjectName(QStringLiteral("previewBrowser"));
    m_browser->setOpenLinks(false); // 由 anchorClicked 统一分发
    m_browser->setFrameShape(QFrame::NoFrame);
    layout->addWidget(m_browser);

    connect(m_browser, &QTextBrowser::anchorClicked,
            this, [this](const QUrl& url) {
                if (url.fragment().startsWith(QStringLiteral("sec-"))) {
                    bool ok = false;
                    const int index = url.fragment().mid(4).toInt(&ok);
                    if (ok)
                        emit headingClicked(index);
                    return;
                }
                if (url.scheme() == QStringLiteral("http")
                    || url.scheme() == QStringLiteral("https")
                    || url.scheme() == QStringLiteral("mailto"))
                    QDesktopServices::openUrl(url);
            });

    // 触屏：拖动滚动（带惯性）；轻点合成为鼠标点击以保留链接跳转
    new editor::TouchScroller(m_browser, m_browser,
                              [this](const QPoint& p) {
                                  QMouseEvent press(QEvent::MouseButtonPress, p, p,
                                                    Qt::LeftButton, Qt::LeftButton,
                                                    Qt::NoModifier);
                                  QMouseEvent release(QEvent::MouseButtonRelease, p, p,
                                                      Qt::LeftButton, {},
                                                      Qt::NoModifier);
                                  QApplication::sendEvent(m_browser->viewport(), &press);
                                  QApplication::sendEvent(m_browser->viewport(), &release);
                              },
                              this);
}

QString MarkdownPreview::buildPage(const QString& bodyHtml) const
{
    const editor::ThemeTokens& t = editor::ThemeManager::tokens();
    const QColor codeBg = t.currentLineBackground;
    const QColor quoteColor = editor::ThemeManager::syntaxColor(QStringLiteral("comment"));
    const QColor softBorder = editor::ThemeManager::color(QStringLiteral("separatorColor"));
    const QColor fg = t.editorForeground;

    return QStringLiteral(
               "<style>"
               "body { color:%1; }"
               "h1, h2, h3, h4, h5, h6 { color:%1; }"
               "code { color:%2; background-color:%3; }"
               "pre { color:%2; background-color:%3; }"
               "blockquote { color:%4; }"
               "a { color:%5; }"
               "hr { border: none; border-top: 1px solid %6; }"
               "</style>")
        .arg(fg.name(), t.accent.name(), codeBg.name(), quoteColor.name(),
             t.accent.name(), softBorder.name())
        + bodyHtml;
}

void MarkdownPreview::setMarkdown(const QString& text, const QString& filePath)
{
    m_filePath = filePath;
    m_headerTitle->setText(tr("预览 — %1").arg(QFileInfo(filePath).fileName()));
    const markdown::RenderResult result = markdown::renderToHtml(text);
    m_lastHtmlBody = result.html;

    const QFileInfo info(filePath);
    const QString dir = info.absolutePath();
    m_browser->document()->setBaseUrl(
        QUrl::fromLocalFile(dir.isEmpty() ? QStringLiteral("./") : dir + QStringLiteral("/")));

    m_browser->setHtml(buildPage(result.html));
}

QScrollBar* MarkdownPreview::verticalScrollBar() const
{
    return m_browser->verticalScrollBar();
}

void MarkdownPreview::clear()
{
    m_lastHtmlBody.clear();
    m_browser->clear();
}

bool MarkdownPreview::event(QEvent* event)
{
    // 图片加载失败等情况静默处理，避免 QTextBrowser 弹原生错误页
    if (event->type() == QEvent::DeferredDelete)
        return QWidget::event(event);
    return QWidget::event(event);
}

} // namespace panels
