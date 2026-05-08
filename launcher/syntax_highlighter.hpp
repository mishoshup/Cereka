#pragma once

#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QVector>

// ── CrkaHighlighter ───────────────────────────────────────────────────────────

class CrkaHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit CrkaHighlighter(QTextDocument *parent = nullptr);

protected:
    void highlightBlock(const QString &text) override;

private:
    struct HighlightRule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };

    QVector<HighlightRule> m_keywordRules;
    QVector<HighlightRule> m_operatorRules;

    QTextCharFormat m_keywordFormat;
    QTextCharFormat m_stringFormat;
    QTextCharFormat m_commentFormat;
    QTextCharFormat m_numberFormat;
    QTextCharFormat m_labelFormat;
    QTextCharFormat m_variableFormat;
    QTextCharFormat m_operatorFormat;

    QRegularExpression m_stringPattern;
    QRegularExpression m_variablePattern;
    QRegularExpression m_numberPattern;
    QRegularExpression m_commentPattern;
    QRegularExpression m_labelPattern;

    /// Find all string regions ("...") in the text.
    QVector<QPair<int, int>> findStringRegions(const QString &text) const;

    /// Check if a position falls within any of the given string regions.
    static bool isInsideRegion(int pos, int length,
                               const QVector<QPair<int, int>> &regions);
};
