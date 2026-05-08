#pragma once

#include <QListWidget>
#include <QLabel>
#include <QWidget>

#include <QJsonObject>
#include <QVector>

class CodeEditor;
class LspClient;

/// Side panel listing `label` definitions from LSP documentSymbol.
class OutlinePanel : public QWidget {
    Q_OBJECT
public:
    explicit OutlinePanel(QWidget *parent = nullptr);

    /// Set the active document to fetch symbols for.
    void setActiveDocument(const QString &uri, CodeEditor *editor, LspClient *lspClient);

    /// Request updated document symbols from LSP.
    void refresh();

private slots:
    void onItemClicked(QListWidgetItem *item);

private:
    void onSymbolResponse(const QJsonObject &resp);

    QListWidget *m_list = nullptr;
    QLabel *m_emptyLabel = nullptr;
    QString m_uri;
    CodeEditor *m_editor = nullptr;
    LspClient *m_lspClient = nullptr;

    struct Entry {
        QString name;
        int line = 0;
        int col = 0;
    };
    QVector<Entry> m_entries;
};
