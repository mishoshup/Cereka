#include "outline_panel.hpp"
#include "code_editor.hpp"
#include "lsp_client.hpp"
#include "theme.hpp"

#include <QJsonArray>
#include <QJsonObject>
#include <QTextBlock>
#include <QTextCursor>
#include <QVBoxLayout>

// ══════════════════════════════════════════════════════════════════════════════
// ── Constructor ──────────────────────────────────────────────────────────────
// ══════════════════════════════════════════════════════════════════════════════

OutlinePanel::OutlinePanel(QWidget *parent)
    : QWidget(parent)
{
    setFixedWidth(180);
    setStyleSheet(QString("background-color: %1; border-left: 1px solid %2;")
        .arg(Theme::BgDeep).arg(Theme::BorderDivider));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // ── Title ───────────────────────────────────────────────────────────────
    auto *header = new QLabel("OUTLINE");
    header->setStyleSheet(QString(
        "color: %1; font-size: 10px; font-weight: 600; letter-spacing: 2px; "
        "padding: 8px 10px; background-color: %2;"
    ).arg(Theme::TextDimmer).arg(Theme::BgDeep));
    layout->addWidget(header);

    // ── Empty state ─────────────────────────────────────────────────────────
    m_emptyLabel = new QLabel("No symbols");
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setStyleSheet(QString(
        "color: %1; font-size: 11px; padding: 20px; background: transparent;"
    ).arg(Theme::TextFaint));
    layout->addWidget(m_emptyLabel, 0, Qt::AlignCenter);

    // ── Symbol list ─────────────────────────────────────────────────────────
    m_list = new QListWidget();
    m_list->setStyleSheet(QString(R"(
        QListWidget {
            background-color: transparent; border: none;
            color: %1; font-size: 11px; font-family: monospace;
            padding: 4px;
        }
        QListWidget::item {
            padding: 3px 6px; border-radius: 3px;
        }
        QListWidget::item:selected {
            background-color: %2; color: %3;
        }
        QListWidget::item:hover:!selected {
            background-color: %4;
        }
    )").arg(Theme::TextSecondary)
       .arg(Theme::GoldSelectedBg).arg(Theme::GoldSelected)
       .arg(Theme::BgItemHover));
    connect(m_list, &QListWidget::itemClicked,
            this, &OutlinePanel::onItemClicked);
    layout->addWidget(m_list, 1);
}

// ══════════════════════════════════════════════════════════════════════════════
// ── Document tracking ────────────────────────────────────────────────────────
// ══════════════════════════════════════════════════════════════════════════════

void OutlinePanel::setActiveDocument(const QString &uri,
                                     CodeEditor *editor,
                                     LspClient *lspClient)
{
    m_uri = uri;
    m_editor = editor;
    m_lspClient = lspClient;
    refresh();
}

void OutlinePanel::refresh()
{
    m_list->clear();
    m_entries.clear();

    if (!m_lspClient || !m_lspClient->isRunning() || m_uri.isEmpty()) {
        m_list->setVisible(false);
        m_emptyLabel->setVisible(true);
        return;
    }

    m_list->setVisible(true);
    m_emptyLabel->setVisible(false);

    m_lspClient->documentSymbol(m_uri,
        [this](QJsonObject resp) { onSymbolResponse(resp); });
}

// ══════════════════════════════════════════════════════════════════════════════
// ── LSP response ─────────────────────────────────────────────────────────────
// ══════════════════════════════════════════════════════════════════════════════

void OutlinePanel::onSymbolResponse(const QJsonObject &resp)
{
    m_list->clear();
    m_entries.clear();

    QJsonArray symbols = resp["result"].toArray();
    if (symbols.isEmpty()) {
        m_list->setVisible(false);
        m_emptyLabel->setVisible(true);
        return;
    }

    m_list->setVisible(true);
    m_emptyLabel->setVisible(false);

    // Recursively add symbols with tree indentation
    std::function<void(const QJsonArray &, int)> addRecursive;
    addRecursive = [this, &addRecursive](const QJsonArray &arr, int depth) {
        for (const QJsonValue &v : arr) {
            QJsonObject sym = v.toObject();
            QString name = sym["name"].toString();
            QJsonObject range = sym["selectionRange"].toObject();
            QJsonObject start = range["start"].toObject();
            int line = start["line"].toInt();
            int col = start["character"].toInt();

            // Build indented display name using non-breaking spaces for alignment
            QString display;
            display.reserve(depth * 2 + name.size() + 4);
            for (int i = 0; i < depth; ++i)
                display += QString(QChar(0x00A0)) + QString(QChar(0x00A0));
            if (depth > 0)
                display += QString(QChar(0x2514)) + QString(QChar(0x2500))
                           + QString(QChar(0x00A0));
            display += name;

            auto *item = new QListWidgetItem(display);
            QFont fnt = item->font();
            fnt.setPointSize(fnt.pointSize() - 1);
            if (depth == 0)
                fnt.setBold(true);
            item->setFont(fnt);
            m_list->addItem(item);

            Entry e;
            e.name = name;
            e.line = line;
            e.col = col;
            m_entries.append(e);

            QJsonArray children = sym["children"].toArray();
            if (!children.isEmpty())
                addRecursive(children, depth + 1);
        }
    };

    addRecursive(symbols, 0);

    if (m_entries.isEmpty()) {
        m_list->setVisible(false);
        m_emptyLabel->setVisible(true);
    }
}

// ══════════════════════════════════════════════════════════════════════════════
// ── Navigation ───────────────────────────────────────────────────────────────
// ══════════════════════════════════════════════════════════════════════════════

void OutlinePanel::onItemClicked(QListWidgetItem *item)
{
    int idx = m_list->row(item);
    if (idx < 0 || idx >= m_entries.size() || !m_editor)
        return;

    const Entry &e = m_entries[idx];
    QTextBlock block = m_editor->document()->findBlockByNumber(e.line);
    if (block.isValid()) {
        QTextCursor cursor(block);
        cursor.movePosition(QTextCursor::StartOfBlock);
        cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, e.col);
        m_editor->setTextCursor(cursor);
        m_editor->ensureCursorVisible();
        m_editor->setFocus();
    }
}
