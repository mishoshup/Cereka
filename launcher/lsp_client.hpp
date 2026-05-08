#pragma once

#include <QJsonObject>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QUrl>

#include <cstdint>
#include <functional>
#include <map>
#include <vector>

// ── LSP types ─────────────────────────────────────────────────────────────────

struct LspLocation {
    QString uri;
    int line = 0;
    int col = 0;
};

struct LspDiagnostic {
    int line = 0;
    int col = 0;
    int endLine = 0;
    int endCol = 0;
    QString message;
    QString severity; // "Error" | "Warning" | "Information" | "Hint"
};

using LspCallback = std::function<void(QJsonObject)>;

// ── LspClient ─────────────────────────────────────────────────────────────────

class LspClient : public QObject {
    Q_OBJECT
public:
    explicit LspClient(QObject *parent = nullptr);
    ~LspClient() override;

    /// Start the LSP server process at the given binary path.
    /// Returns false if the binary cannot be found or started.
    bool start(const QString &binaryPath);
    void stop();
    bool isRunning() const;

    // ── LSP lifecycle ─────────────────────────────────────────────────────────
    void initialize();
    void initialized();
    void didOpen(const QString &uri, const QString &text);
    void didChange(const QString &uri, const QString &text, int version);
    void didClose(const QString &uri);

    // ── LSP requests (return request ID; callback receives full response object) ──
    int completion(const QString &uri, int line, int col, LspCallback cb);
    int definition(const QString &uri, int line, int col, LspCallback cb);
    int hover(const QString &uri, int line, int col, LspCallback cb);
    int references(const QString &uri, int line, int col, LspCallback cb);
    int documentSymbol(const QString &uri, LspCallback cb);

    /// Resolve the LSP server binary path.
    /// Checks:
    ///   1. ~/dev/tree-sitter-cereka/lsp/dist/cereka-lsp (development path)
    ///   2. <launcher dir>/../../tree-sitter-cereka/lsp/dist/cereka-lsp
    /// Returns empty string if not found.
    static QString resolveBinary();

signals:
    void diagnosticsReceived(const QString &uri,
                             const QList<LspDiagnostic> &diagnostics);
    void messageReceived(const QString &level, const QString &message);
    void connectionFailed();
    void initializedOk();

private slots:
    void onReadyRead();
    void onProcessError(QProcess::ProcessError error);
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    int sendRequest(const QString &method, const QJsonObject &params,
                    LspCallback cb);
    void sendNotification(const QString &method, const QJsonObject &params);
    void handleMessage(const QJsonObject &msg);
    void processBuffer();
    void scheduleReconnect();
    void resetReconnect();

    QProcess *m_process = nullptr;
    QByteArray m_buffer;
    int m_requestId = 1;
    std::map<int, LspCallback> m_pendingRequests;
    bool m_initialized = false;
    QString m_binaryPath;

    // Reconnection state
    int m_reconnectAttempt = 0;
    bool m_intentionalStop = false;
    static constexpr int MAX_RECONNECT_DELAY_MS = 30000;
    static constexpr int INITIAL_RECONNECT_DELAY_MS = 1000;
};
