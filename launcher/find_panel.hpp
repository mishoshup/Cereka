#pragma once

#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QWidget>

class CodeEditor;

/// Find/Replace panel docked at the bottom of the editor area.
class FindPanel : public QWidget {
    Q_OBJECT
public:
    explicit FindPanel(QWidget *parent = nullptr);

    /// Set the editor to search within.
    void setEditor(CodeEditor *editor) { m_editor = editor; }

    /// Focus the search input and select all text.
    void focusSearch();

    /// Show or hide the replace input row.
    void showReplace(bool visible);
    bool isReplaceVisible() const { return m_replaceRow->isVisible(); }

signals:
    /// Emitted when the user closes the panel (ESC or close button).
    void closed();

public slots:
    void onNextMatch();
    void onPreviousMatch();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onSearchChanged(const QString &text);
    void onReplaceOne();
    void onReplaceAll();
    void toggleCaseSensitive();
    void toggleWholeWord();
    void toggleRegex();
    void onResultActivated(QListWidgetItem *item);

private:
    struct Match {
        int line = 0;
        int col = 0;
        int length = 0;
        QString context;
    };

    void runSearch();
    void applyHighlights();
    void clearHighlights();
    void navigateTo(int index);

    CodeEditor *m_editor = nullptr;

    QLineEdit *m_searchInput = nullptr;
    QLineEdit *m_replaceInput = nullptr;
    QListWidget *m_resultsList = nullptr;
    QLabel *m_matchCount = nullptr;
    QPushButton *m_caseBtn = nullptr;
    QPushButton *m_wordBtn = nullptr;
    QPushButton *m_regexBtn = nullptr;
    QPushButton *m_replaceBtn = nullptr;
    QPushButton *m_replaceAllBtn = nullptr;
    QWidget *m_replaceRow = nullptr;

    QVector<Match> m_matches;
    int m_currentIndex = -1;
    bool m_caseSensitive = false;
    bool m_wholeWord = false;
    bool m_regexMode = false;

    static constexpr int MAX_RESULTS = 200;
};
