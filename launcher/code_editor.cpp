#include "code_editor.hpp"
#include "theme.hpp"

#include <QAbstractItemView>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QTimer>
#include <QToolTip>

#include <algorithm>
#include <cmath>

// ── CRKA completion keywords (static fallback when LSP unavailable) ───────────

static QStringList crkaKeywords()
{
    return {
        "say", "narrate", "menu", "if", "else", "endif",
        "set", "label", "jump", "call", "include", "end",
        "bg", "char", "hide", "bgm", "stop_bgm", "sfx",
        "save", "load", "checkpoint", "store",
        "button", "goto", "exit",
        "wait", "click", "assert",
        "ui", "textbox", "namebox", "font", "advance_keys"
    };
}

// ── Constructor ───────────────────────────────────────────────────────────────

CodeEditor::CodeEditor(QWidget *parent)
    : QPlainTextEdit(parent)
{
    // Color scheme — dark theme from Theme::*
    m_lineNumberColor    = QColor(Theme::TextDim);
    m_lineNumberBg       = QColor(Theme::BgDeep);
    m_foldMarkerColor    = QColor(Theme::TextFaint);
    m_gutterBorderColor  = QColor(Theme::BorderDivider);
    m_indentGuideColor   = QColor(30, 30, 45);
    m_bracketMatchBg     = QColor(Theme::GoldDimBg);

    // Error squiggle format
    m_errorSquiggleFormat.setUnderlineStyle(QTextCharFormat::WaveUnderline);
    m_errorSquiggleFormat.setUnderlineColor(QColor(255, 60, 60));

    // Bracket match format
    m_bracketMatchFormat.setBackground(m_bracketMatchBg);
    m_bracketMatchFormat.setForeground(QColor(Theme::Gold));

    // Editor appearance
    setStyleSheet(QString(R"(
        QPlainTextEdit {
            background-color: %1; color: %2;
            border: none; padding: 0;
            font-family: "Menlo", "Consolas", "Courier New", monospace;
            font-size: 13px;
            selection-background-color: %3;
        }
    )").arg(Theme::BgBase)
       .arg(Theme::TextPrimary)
       .arg(Theme::GoldDimBg));

    setTabStopDistance(fontMetrics().horizontalAdvance(' ') * 4);
    setLineWrapMode(NoWrap);
    setMouseTracking(true);

    // Completer
    m_keywordCompletions = crkaKeywords();
    m_completionModel = new QStringListModel(m_keywordCompletions, this);
    m_completer = new QCompleter(m_completionModel, this);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    m_completer->setCompletionMode(QCompleter::PopupCompletion);
    m_completer->setFilterMode(Qt::MatchContains);
    m_completer->setWidget(this);
    connect(m_completer, QOverload<const QString &>::of(&QCompleter::activated),
            this, &CodeEditor::onCompleterActivated);

    // Signals
    connect(this, &QPlainTextEdit::cursorPositionChanged,
            this, &CodeEditor::onCursorPositionChanged);
    connect(this, &QPlainTextEdit::textChanged,
            this, &CodeEditor::onTextChanged);

    // Hover debounce timer
    m_hoverTimer = new QTimer(this);
    m_hoverTimer->setSingleShot(true);
    m_hoverTimer->setInterval(400);
    connect(m_hoverTimer, &QTimer::timeout, this, [this]() {
        if (m_hoverPending && m_lspClient && m_lspClient->isRunning()) {
            QString uri = QUrl::fromLocalFile("").toString(); // caller sets real uri
            emit hoverRequested(uri, m_lastHoverLine, m_lastHoverCol);
        }
        m_hoverPending = false;
    });

    updateLineNumberAreaWidth();
}

// ── Line number / fold areas ──────────────────────────────────────────────────

int CodeEditor::lineNumberAreaWidth() const
{
    int digits = 1;
    int maxLines = qMax(1, blockCount());
    while (maxLines >= 10) {
        maxLines /= 10;
        ++digits;
    }
    return 8 + fontMetrics().horizontalAdvance('0') * digits;
}

int CodeEditor::foldAreaWidth() const
{
    return 14;
}

void CodeEditor::updateLineNumberAreaWidth()
{
    setViewportMargins(lineNumberAreaWidth() + foldAreaWidth(), 0, 0, 0);
}

void CodeEditor::resizeEvent(QResizeEvent *event)
{
    QPlainTextEdit::resizeEvent(event);
    updateLineNumberAreaWidth();
}

// ── Paint ─────────────────────────────────────────────────────────────────────

void CodeEditor::paintEvent(QPaintEvent *event)
{
    // 1. Gutter background
    int gutterW = lineNumberAreaWidth() + foldAreaWidth();
    QRect gutterRect(0, 0, gutterW, viewport()->height());

    QPainter painter(viewport());
    painter.fillRect(gutterRect, m_lineNumberBg);

    // 2. Paint line numbers
    paintLineNumbers(painter, gutterRect);

    // 3. Paint fold markers
    paintFoldMarkers(painter, gutterRect);

    painter.end();

    // 4. Base paint (text + selections)
    QPlainTextEdit::paintEvent(event);

    // 5. Indentation guides
    QPainter p2(viewport());
    paintIndentGuides(p2, viewport()->rect());
    p2.end();
}

// ── Gutter painting ───────────────────────────────────────────────────────────

void CodeEditor::paintLineNumbers(QPainter &painter, const QRect &rect)
{
    int lineNumW = lineNumberAreaWidth();
    QFont lineFont = font();
    lineFont.setPointSize(font().pointSize() - 1);
    painter.setFont(lineFont);

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    qreal top = blockBoundingGeometry(block).translated(contentOffset()).top();
    qreal bottom = top + blockBoundingRect(block).height();

    int currentLine = textCursor().blockNumber();

    while (block.isValid() && top <= rect.bottom()) {
        if (block.isVisible() && bottom >= rect.top()) {
            QString num = QString::number(blockNumber + 1);
            painter.setPen(blockNumber == currentLine
                               ? QColor(Theme::TextMuted)
                               : m_lineNumberColor);
            int x = rect.left() + foldAreaWidth();
            int w = lineNumW;
            painter.drawText(QRect(x, static_cast<int>(top), w,
                                   static_cast<int>(bottom - top)),
                             Qt::AlignRight | Qt::AlignVCenter, num);
        }

        block = block.next();
        top = bottom;
        bottom = top + blockBoundingRect(block).height();
        ++blockNumber;
    }

    // Gutter border line
    painter.setPen(m_gutterBorderColor);
    int gutterEndX = rect.left() + lineNumW + foldAreaWidth();
    painter.drawLine(gutterEndX, rect.top(), gutterEndX, rect.bottom());
}

void CodeEditor::paintFoldMarkers(QPainter &painter, const QRect &rect)
{
    Q_UNUSED(rect);
    QFont fnt = font();
    fnt.setPointSize(font().pointSize() - 2);
    painter.setFont(fnt);
    painter.setPen(m_foldMarkerColor);

    QTextBlock block = firstVisibleBlock();
    qreal top = blockBoundingGeometry(block).translated(contentOffset()).top();
    qreal bottom = top + blockBoundingRect(block).height();

    while (block.isValid() && top <= rect.bottom()) {
        if (block.isVisible() && bottom >= rect.top()) {
            // Show fold marker for blocks that contain children
            // (simplified: show on any block that's not empty and not single-line)
            QString text = block.text().trimmed();
            if (!text.isEmpty()) {
                // Check next block — if it's indented, show fold marker
                QTextBlock next = block.next();
                if (next.isValid() && !next.text().trimmed().isEmpty()) {
                    QString nextTrim = next.text().trimmed();
                    // Check if next block is indented relative to this one
                    int myIndent = block.text().size() - block.text().size();
                    // Simplified: show marker on keyword blocks
                    static const char *foldKeywords[] = {
                        "menu", "if", "ui", nullptr
                    };
                    bool hasFold = false;
                    for (const char **kw = foldKeywords; *kw; ++kw) {
                        if (text.startsWith(*kw)) {
                            hasFold = true;
                            break;
                        }
                    }
                    if (hasFold) {
                        QRectF blockRect = blockBoundingRect(block);
                        QPointF offset = contentOffset();
                        qreal x = 2;
                        qreal y = blockRect.translated(offset).top()
                                   + (blockRect.height() - 10) / 2;
                        QRectF markerRect(x, y, 10, 10);
                        painter.drawText(markerRect, Qt::AlignCenter, "▶");
                    }
                }
            }
        }

        block = block.next();
        top = bottom;
        bottom = top + blockBoundingRect(block).height();
    }
}

void CodeEditor::paintIndentGuides(QPainter &painter, const QRect &rect)
{
    painter.setPen(QPen(m_indentGuideColor, 1));

    int tabW = static_cast<int>(tabStopDistance());
    if (tabW <= 0)
        tabW = fontMetrics().horizontalAdvance(' ') * 4;

    QTextBlock block = firstVisibleBlock();
    qreal top = blockBoundingGeometry(block).translated(contentOffset()).top();

    while (block.isValid() && top <= rect.bottom()) {
        QString text = block.text();
        int indent = 0;
        for (QChar ch : text) {
            if (ch == ' ')
                ++indent;
            else if (ch == '\t')
                indent += 4;
            else
                break;
        }

        int pixels = indent * fontMetrics().horizontalAdvance(' ');
        for (int x = tabW; x <= pixels; x += tabW) {
            qreal lineY = top + blockBoundingRect(block).height() - 1;
            painter.drawLine(QPointF(x + foldAreaWidth() + lineNumberAreaWidth(), top),
                             QPointF(x + foldAreaWidth() + lineNumberAreaWidth(), lineY));
        }

        block = block.next();
        top += blockBoundingRect(block).height();
    }
}

// ── Bracket matching ──────────────────────────────────────────────────────────

QTextCursor CodeEditor::findMatchingBracket(QTextCursor cursor) const
{
    QChar fwd = document()->characterAt(cursor.position());
    QChar bwd = cursor.position() > 0
                    ? document()->characterAt(cursor.position() - 1)
                    : QChar();

    static const QMap<QChar, QChar> pairs = {
        {'{', '}'}, {'(', ')'}, {'[', ']'}
    };

    QChar open, close;
    int dir = 0; // 1 = forward, -1 = backward

    if (pairs.contains(fwd)) {
        open = fwd;
        close = pairs[fwd];
        dir = 1;
    } else if (pairs.values().contains(bwd)) {
        close = bwd;
        open = pairs.key(bwd);
        dir = -1;
    } else {
        return {};
    }

    int depth = 0;
    QTextCursor scan = cursor;
    if (dir == 1)
        scan.movePosition(QTextCursor::Right);

    int startPos = scan.position();

    while (!scan.atEnd() && !scan.atStart()) {
        QChar ch = document()->characterAt(scan.position());
        if (ch == open) {
            ++depth;
        } else if (ch == close) {
            if (depth == 0) {
                scan.movePosition(QTextCursor::Right);
                return scan;
            }
            --depth;
        }

        if (dir == 1)
            scan.movePosition(QTextCursor::Right);
        else
            scan.movePosition(QTextCursor::Left);
    }

    return {};
}

// ── Extra selections (bracket matching + error squiggles) ─────────────────────

void CodeEditor::updateExtraSelections()
{
    QList<QTextEdit::ExtraSelection> selections;

    // Error squiggles
    for (const LspDiagnostic &d : m_diagnostics) {
        QTextEdit::ExtraSelection sel;
        QTextCursor cursor(document());

        QTextBlock startBlock = document()->findBlockByNumber(d.line);
        if (!startBlock.isValid())
            continue;

        cursor.setPosition(startBlock.position() + d.col);
        int endPos = startBlock.position() + d.endCol;
        if (d.endLine != d.line) {
            QTextBlock endBlock = document()->findBlockByNumber(d.endLine);
            if (endBlock.isValid())
                endPos = endBlock.position() + endBlock.length() - 1;
        }
        cursor.setPosition(endPos, QTextCursor::KeepAnchor);

        sel.cursor = cursor;
        sel.format = m_errorSquiggleFormat;
        selections.append(sel);
    }

    // Bracket matching
    QTextCursor cursor = textCursor();
    QTextCursor match = findMatchingBracket(cursor);

    if (!match.isNull()) {
        QTextEdit::ExtraSelection sel1, sel2;

        // Match the bracket at cursor
        sel1.cursor = cursor;
        sel1.cursor.clearSelection();
        sel1.format = m_bracketMatchFormat;

        // Match the paired bracket
        sel2.cursor = match;
        sel2.cursor.clearSelection();
        sel2.format = m_bracketMatchFormat;

        selections.append(sel1);
        selections.append(sel2);
    }

    // Search highlights from find/replace panel
    selections.append(m_searchSelections);

    setExtraSelections(selections);
}

// ── Diagnostics ───────────────────────────────────────────────────────────────

void CodeEditor::setDiagnostics(const QList<LspDiagnostic> &diags)
{
    m_diagnostics = diags;
    updateExtraSelections();
}

void CodeEditor::setSearchHighlights(const QList<QTextEdit::ExtraSelection> &selections)
{
    m_searchSelections = selections;
    updateExtraSelections();
}

// ── Mouse events ──────────────────────────────────────────────────────────────

void CodeEditor::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton
        && (event->modifiers() & Qt::ControlModifier))
    {
        // Ctrl+click: go to definition
        QTextCursor cursor = cursorForPosition(event->pos());
        cursor.select(QTextCursor::WordUnderCursor);
        QString word = cursor.selectedText();

        if (!word.isEmpty() && m_lspClient && m_lspClient->isRunning()) {
            int line = cursor.blockNumber();
            int col = cursor.positionInBlock();
            QString uri = QUrl::fromLocalFile("").toString(); // caller fills real uri
            emit goToDefinitionRequested(uri, line, col);
            return;
        }
    }

    QPlainTextEdit::mousePressEvent(event);
}

void CodeEditor::mouseMoveEvent(QMouseEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        viewport()->setCursor(Qt::PointingHandCursor);
    } else {
        viewport()->setCursor(Qt::IBeamCursor);
    }

    // Track hover for tooltip
    QTextCursor cursor = cursorForPosition(event->pos());
    cursor.select(QTextCursor::WordUnderCursor);
    QString word = cursor.selectedText();

    int line = cursor.blockNumber();
    int col = cursor.positionInBlock();

    if (!word.isEmpty() && (word != m_lastHoverWord || line != m_lastHoverLine)) {
        m_lastHoverWord = word;
        m_lastHoverLine = line;
        m_lastHoverCol = col;
        m_hoverPending = true;
        m_hoverTimer->start();
    } else if (word.isEmpty()) {
        m_hoverPending = false;
        m_hoverTimer->stop();
        QToolTip::hideText();
    }

    QPlainTextEdit::mouseMoveEvent(event);
}

void CodeEditor::leaveEvent(QEvent *event)
{
    m_hoverPending = false;
    m_hoverTimer->stop();
    QToolTip::hideText();
    QPlainTextEdit::leaveEvent(event);
}

// ── Keyboard ──────────────────────────────────────────────────────────────────

void CodeEditor::keyPressEvent(QKeyEvent *event)
{
    if (m_completer && m_completer->popup()->isVisible()) {
        switch (event->key()) {
        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Tab:
        case Qt::Key_Backtab: {
            QModelIndex idx = m_completer->popup()->currentIndex();
            if (idx.isValid()) {
                QString text = idx.data().toString();
                onCompleterActivated(text);
                m_completer->popup()->hide();
                return;
            }
            break;
        }
            break;
        case Qt::Key_Escape:
            m_completer->popup()->hide();
            return;
        default:
            break;
        }
    }

    QPlainTextEdit::keyPressEvent(event);

    if (event->key() == Qt::Key_Space && (event->modifiers() & Qt::ControlModifier)) {
        // Ctrl+Space triggers completion
        if (m_lspClient && m_lspClient->isRunning()) {
            QTextCursor cursor = textCursor();
            int line = cursor.blockNumber();
            int col = cursor.positionInBlock();
            QString uri = QUrl::fromLocalFile("").toString();
            emit completionTriggered(uri, line, col);
        } else {
            // Fallback: show keyword completions
            updateCompleterPopup();
        }
    }
}

bool CodeEditor::event(QEvent *event)
{
    // Tooltip event handling
    if (event->type() == QEvent::ToolTip) {
        QHelpEvent *help = static_cast<QHelpEvent *>(event);
        QTextCursor cursor = cursorForPosition(help->pos());
        cursor.select(QTextCursor::WordUnderCursor);
        QString word = cursor.selectedText();

        if (!word.isEmpty()) {
            // We don't have the hover text until LSP responds,
            // so we handle this via showHoverTooltip
            // Signal has already been sent in mouseMoveEvent
            return true;
        }
    }

    return QPlainTextEdit::event(event);
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void CodeEditor::onCursorPositionChanged()
{
    updateExtraSelections();
}

void CodeEditor::onTextChanged()
{
    // Auto-trigger completion after certain patterns
    QTextCursor cursor = textCursor();
    QString text = cursor.block().text().left(cursor.positionInBlock());

    static const QStringList triggers = {"goto ", "button "};
    for (const QString &tr : triggers) {
        if (text.endsWith(tr)) {
            if (m_lspClient && m_lspClient->isRunning()) {
                int line = cursor.blockNumber();
                int col = cursor.positionInBlock();
                QString uri = QUrl::fromLocalFile("").toString();
                emit completionTriggered(uri, line, col);
            }
            return;
        }
    }

    // Close completer on whitespace-only text or empty
    if (text.isEmpty() || text.back().isSpace()) {
        if (m_completer && m_completer->popup()->isVisible()) {
            m_completer->popup()->hide();
        }
    }
}

void CodeEditor::onCompleterActivated(const QString &text)
{
    QTextCursor cursor = textCursor();
    // Select the current partial word
    cursor.select(QTextCursor::WordUnderCursor);
    cursor.removeSelectedText();
    cursor.insertText(text);
}

// ── Hover tooltip ─────────────────────────────────────────────────────────────

void CodeEditor::showHoverTooltip(const QString &text)
{
    if (text.isEmpty())
        return;

    QToolTip::showText(QCursor::pos(), text, this);
}

// ── Completion popup ─────────────────────────────────────────────────────────

void CodeEditor::showCompletions(const QStringList &items)
{
    if (items.isEmpty())
        return;

    // Temporarily replace model with LSP results
    m_completionModel->setStringList(items);
    updateCompleterPopup();
}

void CodeEditor::updateCompleterPopup()
{
    if (!m_completer)
        return;

    QTextCursor cursor = textCursor();
    cursor.select(QTextCursor::WordUnderCursor);
    QString prefix = cursor.selectedText();

    if (prefix.isEmpty())
        return;

    m_completer->setCompletionPrefix(prefix);

    if (m_completer->completionCount() > 0) {
        QRect cr = cursorRect();
        cr.setWidth(m_completer->popup()->sizeHintForColumn(0)
                    + m_completer->popup()->verticalScrollBar()->sizeHint().width()
                    + 30);
        m_completer->complete(cr);
    } else {
        m_completer->popup()->hide();
    }
}
