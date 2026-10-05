// Hutaomu Editor - Workspace-wide text search panel.
// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 邱息 (Hutaomu Editor authors)
#include "SearchPanel.h"

#include <QCheckBox>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRegularExpression>
#include <QVBoxLayout>

namespace panels {
namespace {

const QStringList& skippedDirs()
{
    static const QStringList dirs = {
        QStringLiteral(".git"), QStringLiteral("build"), QStringLiteral("build-"),
        QStringLiteral("node_modules"), QStringLiteral("__pycache__"),
        QStringLiteral("dist"), QStringLiteral(".vs"), QStringLiteral(".idea"),
    };
    return dirs;
}

bool looksBinary(const QByteArray& bytes)
{
    return bytes.left(8192).contains('\0');
}

} // namespace

QStringList collectWorkspaceFiles(const QString& root, int maxFiles)
{
    QStringList files;
    QDirIterator it(root, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext() && files.size() < maxFiles) {
        const QString path = it.next();
        const QString relative = QDir(root).relativeFilePath(path);
        bool skip = false;
        for (const QString& part : relative.split(QLatin1Char('/'))) {
            for (const QString& skipDir : skippedDirs()) {
                if (part == skipDir || part.startsWith(skipDir)) {
                    skip = true;
                    break;
                }
            }
            if (skip)
                break;
        }
        if (!skip)
            files.append(path);
    }
    return files;
}

SearchPanel::SearchPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("searchPanel"));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // 标题栏 = 拖拽把手（与资源管理器/大纲一致，可拖动换列/收起）
    m_header = new QWidget(this);
    m_header->setObjectName(QStringLiteral("searchHeader"));
    auto* headerLayout = new QHBoxLayout(m_header);
    headerLayout->setContentsMargins(14, 8, 6, 8);
    auto* title = new QLabel(tr("全局搜索"), m_header);
    title->setObjectName(QStringLiteral("searchTitle"));
    headerLayout->addWidget(title);
    headerLayout->addStretch();
    layout->addWidget(m_header);

    auto* body = new QWidget(this);
    auto* bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(10, 10, 10, 8);
    bodyLayout->setSpacing(6);
    layout->addWidget(body, 1);
    auto* layout_ = bodyLayout; // 后续控件挂到 body
    Q_UNUSED(layout_);

    auto* inputRow = new QHBoxLayout;
    inputRow->setSpacing(6);
    m_input = new QLineEdit(this);
    m_input->setObjectName(QStringLiteral("searchInput"));
    m_input->setPlaceholderText(tr("在工作区中搜索..."));
    m_searchButton = new QPushButton(tr("搜索"), this);
    inputRow->addWidget(m_input, 1);
    inputRow->addWidget(m_searchButton);
    bodyLayout->addLayout(inputRow);

    auto* optionsRow = new QHBoxLayout;
    optionsRow->setSpacing(10);
    m_caseBox = new QCheckBox(tr("区分大小写"), this);
    m_wordBox = new QCheckBox(tr("全词"), this);
    m_regexBox = new QCheckBox(tr("正则"), this);
    optionsRow->addWidget(m_caseBox);
    optionsRow->addWidget(m_wordBox);
    optionsRow->addWidget(m_regexBox);
    optionsRow->addStretch();
    bodyLayout->addLayout(optionsRow);

    m_results = new QListWidget(this);
    m_results->setObjectName(QStringLiteral("searchResults"));
    m_results->setUniformItemSizes(false);
    m_results->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    bodyLayout->addWidget(m_results, 1);

    connect(m_searchButton, &QPushButton::clicked, this, &SearchPanel::runSearch);
    connect(m_input, &QLineEdit::returnPressed, this, &SearchPanel::runSearch);
    connect(m_results, &QListWidget::itemActivated,
            this, [this](QListWidgetItem* item) {
                const QStringList parts = item->data(Qt::UserRole).toStringList();
                if (parts.size() == 4)
                    emit resultActivated(parts[0], parts[1].toInt(),
                                         parts[2].toInt(), parts[3].toInt());
            });
    connect(m_results, &QListWidget::itemClicked,
            this, [this](QListWidgetItem* item) {
                const QStringList parts = item->data(Qt::UserRole).toStringList();
                if (parts.size() == 4)
                    emit resultActivated(parts[0], parts[1].toInt(),
                                         parts[2].toInt(), parts[3].toInt());
            });
}

void SearchPanel::setWorkspace(const QString& path)
{
    m_workspace = path;
}

void SearchPanel::focusInput()
{
    m_input->setFocus();
    m_input->selectAll();
}

void SearchPanel::runSearch()
{
    m_results->clear();
    const QString needle = m_input->text();
    if (needle.isEmpty() || m_workspace.isEmpty())
        return;

    QRegularExpression regex;
    if (m_regexBox->isChecked()) {
        regex.setPattern(needle);
        regex.setPatternOptions(m_caseBox->isChecked()
                                    ? QRegularExpression::NoPatternOption
                                    : QRegularExpression::CaseInsensitiveOption);
        if (!regex.isValid()) {
            new QListWidgetItem(tr("正则表达式无效"), m_results);
            return;
        }
    }

    int matchCount = 0;
    const QStringList files = collectWorkspaceFiles(m_workspace, 5000);
    for (const QString& path : files) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            continue;
        const QByteArray bytes = file.readAll();
        if (looksBinary(bytes))
            continue;

        const QString text = QString::fromUtf8(bytes);
        const QStringList lines = text.split(QLatin1Char('\n'));
        for (int i = 0; i < lines.size() && matchCount < 2000; ++i) {
            const QString& line = lines.at(i);
            int column = -1;
            int length = int(needle.size());

            if (m_regexBox->isChecked()) {
                const QRegularExpressionMatch m = regex.match(line);
                if (m.hasMatch()) {
                    column = m.capturedStart();
                    length = m.capturedLength();
                }
            } else {
                column = line.indexOf(needle, 0,
                                      m_caseBox->isChecked()
                                          ? Qt::CaseSensitive : Qt::CaseInsensitive);
            }

            if (column < 0)
                continue;

            ++matchCount;
            const QString display = QStringLiteral("%1 : %2 : %3")
                                        .arg(QDir(m_workspace).relativeFilePath(path))
                                        .arg(i + 1)
                                        .arg(line.trimmed().left(160));
            auto* item = new QListWidgetItem(display, m_results);
            item->setToolTip(display);
            item->setData(Qt::UserRole, QStringList{
                path, QString::number(i + 1), QString::number(column), QString::number(length) });

            if (matchCount == 1)
                m_results->setCurrentItem(item);
        }
        if (matchCount >= 2000)
            break;
    }

    if (matchCount == 0)
        new QListWidgetItem(tr("无结果"), m_results);
    else if (matchCount >= 2000)
        new QListWidgetItem(tr("结果过多，仅显示前 2000 条"), m_results);
}

} // namespace panels
