#include "find_panel.hpp"
#include "code_editor.hpp"
#include "theme.hpp"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextCursor>
#include <QVBoxLayout>

// ── Static stylesheet helpers (used by constructor) ────────────────────────────

static QString btnStyle()
{
    return QString(R"(
        QPushButton {
            background-color: %1; border: 1px solid %2; border-radius: 3px;
            color: %3; font-size: 11px; padding: 2px;
        }
        QPushButton:hover { background-color: %4; color: %5; }
        QPushButton:checked { background-color: %6; border-color: %7; color: %8; }
    )").arg(Theme::BgSurface).arg(Theme::BorderPanel)
       .arg(Theme::TextMuted).arg(Theme::BgSurfaceHover)
       .arg(Theme::TextPrimary)
       .arg(Theme::GoldDimBg).arg(Theme::Gold).arg(Theme::GoldSelected);
}

static QString smallBtnStyle()
{
    return QString(R"(
        QPushButton {
            background-color: %1; border: 1px solid %2; border-radius: 3px;
            color: %3; font-size: 11px; padding: 2px 8px;
        }
        QPushButton:hover { background-color: %4; color: %5; }
    )").arg(Theme::BgSurface).arg(Theme::BorderPanel)
       .arg(Theme::TextMuted).arg(Theme::BgSurfaceHover).arg(Theme::TextPrimary);
}

// ── Constructor ───────────────────────────────────────────────────────────────

FindPanel::FindPanel(QWidget *parent)
    : QWidget(parent)
{
    setStyleSheet(QString(
        "background-color: %1; border-top: 1px solid %2;"
    ).arg(Theme::BgDeep).arg(Theme::BorderDivider));

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 6, 8, 6);
    mainLayout->setSpacing(4);

    // ── Search row ────────────────────────────────────────────────────────────
    auto *searchRow = new QHBoxLayout();
    searchRow->setSpacing(4);

    m_searchInput = new QLineEdit();
    m_searchInput->setPlaceholderText("Search\u2026");
    m_searchInput->setClearButtonEnabled(true);
    m_searchInput->setStyleSheet(QString(R"(
        QLineEdit {
            background-color: %1; border: 1px solid %2; border-radius: 4px;
            padding: 4px 8px; color: %3; font-size: 12px;
        }
        QLineEdit:focus { border-color: %4; }
    )").arg(Theme::BgSurface).arg(Theme::BorderPanel)
       .arg(Theme::TextPrimary).arg(Theme::Gold));
    connect(m_searchInput, &QLineEdit::textChanged,
            this, &FindPanel::onSearchChanged);
    searchRow->addWidget(m_searchInput, 1);

    // Match count
    m_matchCount = new QLabel("0/0");
    m_matchCount->setFixedWidth(56);
    m_matchCount->setStyleSheet(QString("color: %1; font-size: 11px;")
        .arg(Theme::TextFaint));
    m_matchCount->setAlignment(Qt::AlignCenter);
    searchRow->addWidget(m_matchCount);

    // Case-sensitive toggle
    m_caseBtn = new QPushButton("Aa");
    m_caseBtn->setCheckable(true);
    m_caseBtn->setFixedWidth(30);
    m_caseBtn->setToolTip("Case sensitive");
    m_caseBtn->setStyleSheet(btnStyle());
    connect(m_caseBtn, &QPushButton::clicked,
            this, &FindPanel::toggleCaseSensitive);
    searchRow->addWidget(m_caseBtn);

    // Whole-word toggle
    m_wordBtn = new QPushButton("W");
    m_wordBtn->setCheckable(true);
    m_wordBtn->setFixedWidth(30);
    m_wordBtn->setToolTip("Whole word");
    m_wordBtn->setStyleSheet(btnStyle());
    connect(m_wordBtn, &QPushButton::clicked,
            this, &FindPanel::toggleWholeWord);
    searchRow->addWidget(m_wordBtn);

    // Regex toggle
    m_regexBtn = new QPushButton(".*");
    m_regexBtn->setCheckable(true);
    m_regexBtn->setFixedWidth(36);
    m_regexBtn->setToolTip("Regular expression");
    m_regexBtn->setStyleSheet(btnStyle());
    connect(m_regexBtn, &QPushButton::clicked,
            this, &FindPanel::toggleRegex);
    searchRow->addWidget(m_regexBtn);

    // Close button
    auto *closeBtn = new QPushButton("\u00D7");
    closeBtn->setFixedWidth(24);
    closeBtn->setToolTip("Close (Esc)");
    closeBtn->setStyleSheet(
        QString("QPushButton { background: transparent; color: %1; border: none; "
                "font-size: 16px; } QPushButton:hover { color: %2; }")
            .arg(Theme::TextFaint).arg(Theme::TextPrimary));
    connect(closeBtn, &QPushButton::clicked, this, [this]() {
        clearHighlights();
        emit closed();
    });
    searchRow->addWidget(closeBtn);

    mainLayout->addLayout(searchRow);

    // ── Replace row ───────────────────────────────────────────────────────────
    m_replaceRow = new QWidget();
    auto *replaceLayout = new QHBoxLayout(m_replaceRow);
    replaceLayout->setContentsMargins(0, 0, 0, 0);
    replaceLayout->setSpacing(4);

    m_replaceInput = new QLineEdit();
    m_replaceInput->setPlaceholderText("Replace\u2026");
    m_replaceInput->setClearButtonEnabled(true);
    m_replaceInput->setStyleSheet(m_searchInput->styleSheet());
    replaceLayout->addWidget(m_replaceInput, 1);

    m_replaceBtn = new QPushButton("Replace");
    m_replaceBtn->setFixedHeight(26);
    m_replaceBtn->setStyleSheet(smallBtnStyle());
    connect(m_replaceBtn, &QPushButton::clicked,
            this, &FindPanel::onReplaceOne);
    replaceLayout->addWidget(m_replaceBtn);

    m_replaceAllBtn = new QPushButton("Replace All");
    m_replaceAllBtn->setFixedHeight(26);
    m_replaceAllBtn->setStyleSheet(smallBtnStyle());
    connect(m_replaceAllBtn, &QPushButton::clicked,
            this, &FindPanel::onReplaceAll);
    replaceLayout->addWidget(m_replaceAllBtn);

    mainLayout->addWidget(m_replaceRow);
    m_replaceRow->setVisible(false);

    // ── Results list ──────────────────────────────────────────────────────────
    m_resultsList = new QListWidget();
    m_resultsList->setMaximumHeight(120);
    m_resultsList->setStyleSheet(QString(R"(
        QListWidget {
            background-color: %1; border: 1px solid %2; border-radius: 4px;
            color: %3; font-size: 11px; font-family: monospace;
        }
        QListWidget::item { padding: 2px 4px; }
        QListWidget::item:selected {
            background-color: %4; color: %5;
        }
    )").arg(Theme::BgSurface).arg(Theme::BorderPanel)
       .arg(Theme::TextSecondary)
       .arg(Theme::GoldDimBg).arg(Theme::Gold));
    connect(m_resultsList, &QListWidget::itemActivated,
            this, &FindPanel::onResultActivated);
    connect(m_resultsList, &QListWidget::currentRowChanged,
            this, [this](int row) {
                if (row >= 0 && row < m_matches.size())
                    navigateTo(row);
            });
    mainLayout->addWidget(m_resultsList);

    // Event filters for keyboard navigation
    m_searchInput->installEventFilter(this);
    m_replaceInput->installEventFilter(this);
    m_resultsList->installEventFilter(this);

    setFixedHeight(154);
}

// ── Public interface ──────────────────────────────────────────────────────────

void FindPanel::focusSearch()
{
    m_searchInput->setFocus();
    m_searchInput->selectAll();
}

void FindPanel::showReplace(bool visible)
{
    m_replaceRow->setVisible(visible);
    // Adjust height to fit the replace row
    setFixedHeight(visible ? 182 : 154);
}

// ── Event handling ────────────────────────────────────────────────────────────

bool FindPanel::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        auto *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Escape) {
            clearHighlights();
            emit closed();
            return true;
        }
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
            if (key->modifiers() & Qt::ShiftModifier)
                onPreviousMatch();
            else
                onNextMatch();
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void FindPanel::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        clearHighlights();
        emit closed();
        return;
    }
    QWidget::keyPressEvent(event);
}

// ── Search logic ──────────────────────────────────────────────────────────────

void FindPanel::onSearchChanged(const QString &text)
{
    Q_UNUSED(text);
    runSearch();
}

void FindPanel::runSearch()
{
    m_matches.clear();
    m_resultsList->clear();
    m_currentIndex = -1;

    if (!m_editor || m_searchInput->text().isEmpty()) {
        clearHighlights();
        m_matchCount->setText("0/0");
        return;
    }

    QString needle = m_searchInput->text();
    QString haystack = m_editor->toPlainText();
    QTextDocument *doc = m_editor->document();
    Qt::CaseSensitivity cs = m_caseSensitive
                                 ? Qt::CaseSensitive
                                 : Qt::CaseInsensitive;

    if (m_regexMode) {
        int opts = 0;
        if (!m_caseSensitive)
            opts |= QRegularExpression::CaseInsensitiveOption;
        QRegularExpression rx(needle, static_cast<QRegularExpression::PatternOption>(opts));
        if (!rx.isValid()) {
            m_matchCount->setText("0/0");
            return;
        }

        QRegularExpressionMatchIterator it = rx.globalMatch(haystack);
        while (it.hasNext() && m_matches.size() < MAX_RESULTS) {
            QRegularExpressionMatch rm = it.next();
            int pos = rm.capturedStart();
            QTextBlock block = doc->findBlock(pos);
            if (!block.isValid())
                continue;

            Match match;
            match.line = block.blockNumber();
            match.col = pos - block.position();
            match.length = rm.capturedLength();
            match.context = block.text().trimmed().left(80);

            m_matches.append(match);
            m_resultsList->addItem(QString("  %1:%2  %3")
                .arg(match.line + 1).arg(match.col + 1).arg(match.context));
        }
    } else {
        int from = 0;
        while (from < haystack.length() && m_matches.size() < MAX_RESULTS) {
            int pos = haystack.indexOf(needle, from, cs);
            if (pos < 0)
                break;

            QTextBlock block = doc->findBlock(pos);
            if (!block.isValid()) {
                from = pos + 1;
                continue;
            }

            int col = pos - block.position();

            // Whole-word boundary check
            if (m_wholeWord) {
                bool boundaryOk = true;
                if (pos > 0) {
                    QChar prev = haystack[pos - 1];
                    if (prev.isLetterOrNumber() || prev == QLatin1Char('_'))
                        boundaryOk = false;
                }
                int after = pos + needle.length();
                if (boundaryOk && after < haystack.length()) {
                    QChar next = haystack[after];
                    if (next.isLetterOrNumber() || next == QLatin1Char('_'))
                        boundaryOk = false;
                }
                if (!boundaryOk) {
                    from = pos + 1;
                    continue;
                }
            }

            Match match;
            match.line = block.blockNumber();
            match.col = col;
            match.length = needle.length();
            match.context = block.text().trimmed().left(80);

            m_matches.append(match);
            m_resultsList->addItem(QString("  %1:%2  %3")
                .arg(match.line + 1).arg(match.col + 1).arg(match.context));

            from = pos + 1;
        }
    }

    m_matchCount->setText(QString("%1/%2")
        .arg(m_matches.isEmpty() ? 0 : 1).arg(m_matches.size()));

    if (!m_matches.isEmpty()) {
        navigateTo(0);
    }
    applyHighlights();
}

// ── Match highlighting ────────────────────────────────────────────────────────

void FindPanel::applyHighlights()
{
    if (!m_editor)
        return;

    QList<QTextEdit::ExtraSelection> selections;
    QTextDocument *doc = m_editor->document();

    // Background highlight for all matches
    QTextEdit::ExtraSelection baseSel;
    baseSel.format.setBackground(QColor(Theme::GoldDimBg));

    for (const Match &match : m_matches) {
        QTextBlock block = doc->findBlockByNumber(match.line);
        if (!block.isValid())
            continue;

        QTextCursor cursor(block);
        cursor.setPosition(block.position() + match.col);
        cursor.setPosition(block.position() + match.col + match.length,
                            QTextCursor::KeepAnchor);
        baseSel.cursor = cursor;
        selections.append(baseSel);
    }

    // Current match in a more prominent color
    if (m_currentIndex >= 0 && m_currentIndex < m_matches.size()) {
        const Match &cm = m_matches[m_currentIndex];
        QTextBlock block = doc->findBlockByNumber(cm.line);
        if (block.isValid()) {
            QTextEdit::ExtraSelection curSel;
            curSel.format.setBackground(QColor(Theme::Gold));
            curSel.format.setForeground(QColor("#0A0A0A"));
            QTextCursor cursor(block);
            cursor.setPosition(block.position() + cm.col);
            cursor.setPosition(block.position() + cm.col + cm.length,
                                QTextCursor::KeepAnchor);
            curSel.cursor = cursor;
            selections.append(curSel);
        }
    }

    m_editor->setSearchHighlights(selections);
}

void FindPanel::clearHighlights()
{
    if (m_editor)
        m_editor->setSearchHighlights({});
}

// ── Navigation ────────────────────────────────────────────────────────────────

void FindPanel::navigateTo(int index)
{
    if (index < 0 || index >= m_matches.size())
        return;

    m_currentIndex = index;
    const Match &match = m_matches[index];

    // Select and scroll in the editor
    QTextBlock block = m_editor->document()->findBlockByNumber(match.line);
    if (block.isValid()) {
        QTextCursor cursor(block);
        cursor.setPosition(block.position() + match.col);
        cursor.setPosition(block.position() + match.col + match.length,
                            QTextCursor::KeepAnchor);
        m_editor->setTextCursor(cursor);
        m_editor->ensureCursorVisible();
        m_editor->centerCursor();
    }

    m_resultsList->setCurrentRow(index);
    m_matchCount->setText(QString("%1/%2").arg(index + 1).arg(m_matches.size()));
    applyHighlights();
}

void FindPanel::onNextMatch()
{
    if (m_matches.isEmpty())
        return;
    int next = (m_currentIndex + 1) % m_matches.size();
    navigateTo(next);
}

void FindPanel::onPreviousMatch()
{
    if (m_matches.isEmpty())
        return;
    int prev = (m_currentIndex - 1 + m_matches.size()) % m_matches.size();
    navigateTo(prev);
}

// ── Replace operations ────────────────────────────────────────────────────────

void FindPanel::onReplaceOne()
{
    if (m_currentIndex < 0 || m_currentIndex >= m_matches.size() || !m_editor)
        return;

    const Match &match = m_matches[m_currentIndex];
    QTextBlock block = m_editor->document()->findBlockByNumber(match.line);
    if (!block.isValid())
        return;

    QTextCursor cursor(block);
    cursor.setPosition(block.position() + match.col);
    cursor.setPosition(block.position() + match.col + match.length,
                        QTextCursor::KeepAnchor);
    cursor.insertText(m_replaceInput->text());

    // Re-run search from current position
    runSearch();
}

void FindPanel::onReplaceAll()
{
    if (m_matches.isEmpty() || !m_editor)
        return;

    QString replacement = m_replaceInput->text();
    QTextCursor cursor(m_editor->document());

    // Replace in reverse order to preserve positions
    cursor.beginEditBlock();
    for (int i = m_matches.size() - 1; i >= 0; --i) {
        const Match &match = m_matches[i];
        QTextBlock block = m_editor->document()->findBlockByNumber(match.line);
        if (!block.isValid())
            continue;

        cursor.setPosition(block.position() + match.col);
        cursor.setPosition(block.position() + match.col + match.length,
                            QTextCursor::KeepAnchor);
        cursor.insertText(replacement);
    }
    cursor.endEditBlock();

    m_matches.clear();
    m_resultsList->clear();
    m_currentIndex = -1;
    clearHighlights();
    m_matchCount->setText("0/0");
}

// ── Toggle options ────────────────────────────────────────────────────────────

void FindPanel::toggleCaseSensitive()
{
    m_caseSensitive = !m_caseSensitive;
    m_caseBtn->setChecked(m_caseSensitive);
    runSearch();
}

void FindPanel::toggleWholeWord()
{
    m_wholeWord = !m_wholeWord;
    m_wordBtn->setChecked(m_wholeWord);
    runSearch();
}

void FindPanel::toggleRegex()
{
    m_regexMode = !m_regexMode;
    m_regexBtn->setChecked(m_regexMode);
    runSearch();
}

// ── Result click ──────────────────────────────────────────────────────────────

void FindPanel::onResultActivated(QListWidgetItem *item)
{
    int idx = m_resultsList->row(item);
    if (idx >= 0 && idx < m_matches.size())
        navigateTo(idx);
}
