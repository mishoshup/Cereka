#pragma once

#include "lsp_client.hpp"

#include <QCompleter>
#include <QPlainTextEdit>
#include <QStringListModel>
#include <QTextCharFormat>

#include <vector>

// ── LineNumberArea (sits in the left margin) ───────────────────────────────────

class CodeEditor;

class LineNumberArea : public QWidget {
public:
    explicit LineNumberArea(CodeEditor *editor);
    QSize sizeHint() const override { return QSize(m_width, 0); }
    void setWidth(int w) { m_width = w; }
protected:
    void paintEvent(QPaintEvent *event) override;
private:
    CodeEditor *m_editor;
    int m_width = 50;
};

// ── CodeEditor ────────────────────────────────────────────────────────────────

class CodeEditor : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit CodeEditor(QWidget *parent = nullptr);

    void setLspClient(LspClient *client) { m_lspClient = client; }
    LspClient *lspClient() const { return m_lspClient; }

    /// Set diagnostics from LSP (drives error squiggles).
    void setDiagnostics(const QList<LspDiagnostic> &diags);

    /// Show hover tooltip text (called after LSP hover response).
    void showHoverTooltip(const QString &text);

    /// Show completion items from LSP.
    void showCompletions(const QStringList &items);

    /// Set extra selections from the find/replace panel for match highlighting.
    /// These are merged with the editor's own extra selections (diagnostics, bracket matching).
    void setSearchHighlights(const QList<QTextEdit::ExtraSelection> &selections);

signals:
    /// Emitted when Ctrl+click triggers go-to-definition.
    void goToDefinitionRequested(const QString &uri, int line, int col);
    /// Emitted on mouse hover to request LSP hover info.
    void hoverRequested(const QString &uri, int line, int col);
    /// Emitted when cursor is at a position that may need completions.
    void completionTriggered(const QString &uri, int line, int col);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool event(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void onCursorPositionChanged();
    void onTextChanged();
    void onCompleterActivated(const QString &text);

    friend class LineNumberArea;

private:
    void updateLineNumberAreaWidth();
    void updateExtraSelections();
    void updateCompleterPopup();

    int lineNumberAreaWidth() const;
    int foldAreaWidth() const;

    // Gutter painting helpers
    void paintLineNumbers(QPainter &painter, const QRect &rect, int lineNumW = -1);
    void paintFoldMarkers(QPainter &painter, const QRect &rect);
    void paintIndentGuides(QPainter &painter, const QRect &rect);
    void updateLineNumberArea(const QRect &rect, int dy);

    // Find matching bracket
    QTextCursor findMatchingBracket(QTextCursor cursor) const;

    LspClient *m_lspClient = nullptr;

    // Diagnostics
    QList<LspDiagnostic> m_diagnostics;

    // Search highlights (from find/replace panel)
    QList<QTextEdit::ExtraSelection> m_searchSelections;

    // Bracket matching
    QTextCharFormat m_bracketMatchFormat;
    QTextCharFormat m_errorSquiggleFormat;

    // Completer
    QCompleter *m_completer = nullptr;
    QStringListModel *m_completionModel = nullptr;
    QStringList m_keywordCompletions;
    bool m_completerActive = false;

    // Hover
    QString m_lastHoverWord;
    int m_lastHoverLine = -1;
    int m_lastHoverCol = -1;
    bool m_hoverPending = false;
    QTimer *m_hoverTimer = nullptr;

    // Line number area widget
    LineNumberArea *m_lineNumberArea = nullptr;

    // Indent guide color
    QColor m_indentGuideColor;
    QColor m_lineNumberColor;
    QColor m_lineNumberBg;
    QColor m_foldMarkerColor;
    QColor m_gutterBorderColor;
    QColor m_bracketMatchBg;
};
