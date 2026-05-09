#include "syntax_highlighter.hpp"
#include "theme.hpp"

#include <QColor>

// ── Constructor ───────────────────────────────────────────────────────────────

CrkaHighlighter::CrkaHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    // ── Color definitions ─────────────────────────────────────────────────────
    QColor kwColor(Theme::Gold);               // gold
    QColor strColor("#6BB5C5");                // ice-blue
    QColor commentColor(Theme::TextDim);       // dim gray
    QColor numColor("#7ECB8B");                // green
    QColor labelColor(Theme::Gold);            // gold (same as keyword, but bold)
    QColor varColor("#A78BFA");                // light purple
    QColor opColor(Theme::Gold);               // gold

    // ── Formats ───────────────────────────────────────────────────────────────
    m_keywordFormat.setForeground(kwColor);
    m_keywordFormat.setFontWeight(QFont::Bold);

    m_stringFormat.setForeground(strColor);

    m_commentFormat.setForeground(commentColor);
    m_commentFormat.setFontItalic(true);

    m_numberFormat.setForeground(numColor);

    m_labelFormat.setForeground(labelColor);
    m_labelFormat.setFontWeight(QFont::Bold);

    m_variableFormat.setForeground(varColor);
    m_variableFormat.setFontWeight(QFont::Bold);

    m_operatorFormat.setForeground(opColor);

    // ── Patterns ──────────────────────────────────────────────────────────────
    m_commentPattern  = QRegularExpression(";.*$");
    m_stringPattern   = QRegularExpression(R"("(?:[^"\\]|\\.)*")");
    m_variablePattern = QRegularExpression(R"(\{[^}]*\})");
    m_numberPattern   = QRegularExpression(R"(\b\d+(\.\d+)?\b)");
    m_labelPattern    = QRegularExpression(R"(^label\s+(\S+))");

    // ── Keyword rules ─────────────────────────────────────────────────────────
    QStringList keywords = {
        "say", "narrate", "menu",
        "if", "else", "endif",
        "set", "label", "jump", "call", "include", "end",
        "bg", "char", "hide",
        "bgm", "stop_bgm", "sfx",
        "save", "load", "checkpoint", "store",
        "button", "goto", "exit",
        "wait", "click", "assert",
        "ui", "textbox", "namebox", "font", "advance_keys"
    };

    for (const QString &kw : keywords) {
        HighlightRule rule;
        rule.pattern = QRegularExpression(
            R"(\b)" + kw + R"(\b)",
            QRegularExpression::PatternOption::CaseInsensitiveOption);
        rule.format = m_keywordFormat;
        m_keywordRules.append(rule);
    }

    // ── Special case: standalone `$` for numeric assignment ───────────────────
    {
        HighlightRule rule;
        rule.pattern = QRegularExpression(R"(^\s*\$ )");
        rule.format = m_keywordFormat;
        m_keywordRules.append(rule);
    }

    // ── Operator rules ────────────────────────────────────────────────────────
    QStringList operators = {"==", "!=", ">=", "<=", ">", "<",
                             "+=", "-=", "*=", "/="};
    for (const QString &op : operators) {
        HighlightRule rule;
        rule.pattern = QRegularExpression(QRegularExpression::escape(op));
        rule.format = m_operatorFormat;
        m_operatorRules.append(rule);
    }
}

// ── Highlight block ───────────────────────────────────────────────────────────

void CrkaHighlighter::highlightBlock(const QString &text)
{
    if (text.isEmpty())
        return;

    // 1. Check for full-line comment
    QString trimmed = text.trimmed();
    if (trimmed.startsWith(';')) {
        setFormat(0, text.length(), m_commentFormat);
        return;
    }

    // 2. Find string regions so keyword/operator/number rules skip inside them
    auto stringRegions = findStringRegions(text);

    // 3. Apply keyword rules (skip inside strings)
    for (const auto &rule : m_keywordRules) {
        QRegularExpressionMatchIterator it = rule.pattern.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            if (!isInsideRegion(match.capturedStart(), match.capturedLength(),
                                stringRegions)) {
                setFormat(match.capturedStart(), match.capturedLength(),
                          rule.format);
            }
        }
    }

    // 4. Apply operator rules (skip inside strings)
    for (const auto &rule : m_operatorRules) {
        QRegularExpressionMatchIterator it = rule.pattern.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            if (!isInsideRegion(match.capturedStart(), match.capturedLength(),
                                stringRegions)) {
                setFormat(match.capturedStart(), match.capturedLength(),
                          rule.format);
            }
        }
    }

    // 5. Numbers (skip inside strings)
    {
        QRegularExpressionMatchIterator it = m_numberPattern.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            if (!isInsideRegion(match.capturedStart(), match.capturedLength(),
                                stringRegions)) {
                setFormat(match.capturedStart(), match.capturedLength(),
                          m_numberFormat);
            }
        }
    }

    // 6. Labels (keyword "label" already matched, but we want the label name bold)
    //    Apply after keywords so the label name gets special formatting
    {
        QRegularExpressionMatch match = m_labelPattern.match(text);
        if (match.hasMatch() && !isInsideRegion(match.capturedStart(1),
                                                match.capturedLength(1),
                                                stringRegions)) {
            setFormat(match.capturedStart(1), match.capturedLength(1),
                      m_labelFormat);
        }
    }

    // 7. Strings
    for (const auto &region : stringRegions) {
        setFormat(region.first, region.second, m_stringFormat);
    }

    // 8. Variables (applied after strings to override inside them)
    {
        QRegularExpressionMatchIterator it = m_variablePattern.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            setFormat(match.capturedStart(), match.capturedLength(),
                      m_variableFormat);
        }
    }

    // 9. Inline comments (after strings so `"; comment"` stays as string)
    {
        QRegularExpressionMatchIterator it = m_commentPattern.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            if (!isInsideRegion(match.capturedStart(), match.capturedLength(),
                                stringRegions)) {
                setFormat(match.capturedStart(), match.capturedLength(),
                          m_commentFormat);
            }
        }
    }
}

// ── Helpers ───────────────────────────────────────────────────────────────────

QVector<QPair<int, int>> CrkaHighlighter::findStringRegions(
    const QString &text) const
{
    QVector<QPair<int, int>> regions;
    QRegularExpressionMatchIterator it = m_stringPattern.globalMatch(text);
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        regions.append({match.capturedStart(), match.capturedLength()});
    }
    return regions;
}

bool CrkaHighlighter::isInsideRegion(int pos, int length,
                                     const QVector<QPair<int, int>> &regions)
{
    for (const auto &region : regions) {
        if (pos >= region.first && pos + length <= region.first + region.second)
            return true;
    }
    return false;
}
