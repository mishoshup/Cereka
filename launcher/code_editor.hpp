#pragma once

#include "lsp_client.hpp"

#include <QCompleter>
#include <QPlainTextEdit>
#include <QStringListModel>
#include <QTextCharFormat>

#include <vector>

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

private:
    void updateLineNumberAreaWidth();
    void updateExtraSelections();
    void updateCompleterPopup();

    int lineNumberAreaWidth() const;
    int foldAreaWidth() const;

    // Gutter painting helpers
    void paintLineNumbers(QPainter &painter, const QRect &rect);
    void paintFoldMarkers(QPainter &painter, const QRect &rect);
    void paintIndentGuides(QPainter &painter, const QRect &rect);

    // Find matching bracket
    QTextCursor findMatchingBracket(QTextCursor cursor) const;

    LspClient *m_lspClient = nullptr;

    // Diagnostics
    QList<LspDiagnostic> m_diagnostics;

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

    // Indent guide color
    QColor m_indentGuideColor;
    QColor m_lineNumberColor;
    QColor m_lineNumberBg;
    QColor m_foldMarkerColor;
    QColor m_gutterBorderColor;
    QColor m_bracketMatchBg;
};
