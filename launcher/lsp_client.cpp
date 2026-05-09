#include "lsp_client.hpp"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QTimer>
#include <QCoreApplication>

#include <algorithm>
#include <cctype>
#include <cstring>

// ── Helpers ───────────────────────────────────────────────────────────────────

static QString pathToUri(const QString &localPath)
{
    return QUrl::fromLocalFile(localPath).toString();
}

static QString uriToPath(const QString &uri)
{
    return QUrl(uri).toLocalFile();
}

// ── Constructor / destructor ──────────────────────────────────────────────────

LspClient::LspClient(QObject *parent)
    : QObject(parent)
{
}

LspClient::~LspClient()
{
    stop();
}

// ── Start / stop / status ─────────────────────────────────────────────────────

bool LspClient::start(const QString &path)
{
    if (m_process)
        stop();

    m_binaryPath = path;
    m_initialized = false;
    m_intentionalStop = false;

    if (!QFileInfo::exists(path)) {
        emit connectionFailed();
        return false;
    }

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    m_process->setReadChannel(QProcess::StandardOutput);

    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &LspClient::onReadyRead);
    connect(m_process, &QProcess::errorOccurred,
            this, &LspClient::onProcessError);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &LspClient::onProcessFinished);

    // If path ends with .js, run via node; otherwise treat as standalone binary
    if (path.endsWith(".js")) {
        // Find node binary — on macOS, GUI apps don't inherit shell PATH
        static const char *nodeCandidates[] = {
            "/opt/homebrew/bin/node",
            "/usr/local/bin/node",
            "/usr/bin/node",
            "/opt/local/bin/node",
        };
        QString nodeBin;
        for (const char *candidate : nodeCandidates) {
            if (QFileInfo::exists(candidate)) {
                nodeBin = candidate;
                break;
            }
        }
        if (nodeBin.isEmpty())
            nodeBin = "node"; // fallback — will work if PATH has it
        m_process->start(nodeBin, QStringList() << path);
    } else {
        m_process->start(path, QStringList());
    }

    if (!m_process->waitForStarted(5000)) {
        delete m_process;
        m_process = nullptr;
        emit connectionFailed();
        return false;
    }

    return true;
}

void LspClient::stop()
{
    m_intentionalStop = true;

    if (!m_process)
        return;

    if (m_process->state() != QProcess::NotRunning) {
        // Send exit notification immediately (shutdown handshake omitted for
        // non-blocking close — the server can handle exit without shutdown)
        QJsonObject empty;
        sendNotification("exit", empty);
        m_process->closeWriteChannel();

        // Give the process a short grace period, then kill
        if (!m_process->waitForFinished(2000))
            m_process->kill();
    }

    m_pendingRequests.clear();
    m_buffer.clear();
    delete m_process;
    m_process = nullptr;
}

bool LspClient::isRunning() const
{
    return m_process && m_process->state() == QProcess::Running;
}

// ── LSP lifecycle ─────────────────────────────────────────────────────────────

void LspClient::initialize()
{
    if (!isRunning())
        return;

    QJsonObject params;
    params["processId"] = QCoreApplication::applicationPid();
    params["rootUri"] = QJsonValue::Null;

    QJsonObject capabilities;
    capabilities["textDocumentSync"] = 1; // Full sync
    QJsonObject completionOpts;
    completionOpts["triggerCharacters"] = QJsonArray{"."};
    capabilities["completionProvider"] = completionOpts;
    capabilities["definitionProvider"] = true;
    capabilities["hoverProvider"] = true;
    capabilities["referencesProvider"] = true;
    capabilities["documentSymbolProvider"] = true;
    params["capabilities"] = capabilities;

    sendRequest("initialize", params, [this](QJsonObject resp) {
        // Check for capabilities we care about
        m_initialized = true;
        resetReconnect();
        initialized();
        emit initializedOk();
    });
}

void LspClient::initialized()
{
    QJsonObject params;
    sendNotification("initialized", params);
}

void LspClient::didOpen(const QString &uri, const QString &text)
{
    QJsonObject textDoc;
    textDoc["uri"] = uri;
    textDoc["languageId"] = "cereka";
    textDoc["version"] = 1;
    textDoc["text"] = text;

    QJsonObject params;
    params["textDocument"] = textDoc;

    sendNotification("textDocument/didOpen", params);
}

void LspClient::didChange(const QString &uri, const QString &text, int version)
{
    QJsonObject textDoc;
    textDoc["uri"] = uri;
    textDoc["version"] = version;

    QJsonObject change;
    change["text"] = text;

    QJsonArray contentChanges;
    contentChanges.append(change);

    QJsonObject params;
    params["textDocument"] = textDoc;
    params["contentChanges"] = contentChanges;

    sendNotification("textDocument/didChange", params);
}

void LspClient::didClose(const QString &uri)
{
    QJsonObject textDoc;
    textDoc["uri"] = uri;

    QJsonObject params;
    params["textDocument"] = textDoc;

    sendNotification("textDocument/didClose", params);
}

// ── LSP requests ──────────────────────────────────────────────────────────────

int LspClient::completion(const QString &uri, int line, int col, LspCallback cb)
{
    // Cancel any pending completion request
    // We find the last completion request and remove it from pending map
    // by iterating backwards (completions use method "textDocument/completion")
    for (auto it = m_pendingRequests.begin(); it != m_pendingRequests.end(); ) {
        // Can't check method without storing it — we just store method string per pending request
        // For simplicity we rely on the caller to handle stale results
        ++it;
    }

    QJsonObject pos;
    pos["line"] = line;
    pos["character"] = col;

    QJsonObject textDoc;
    textDoc["uri"] = uri;

    QJsonObject params;
    params["textDocument"] = textDoc;
    params["position"] = pos;

    return sendRequest("textDocument/completion", params, std::move(cb));
}

int LspClient::definition(const QString &uri, int line, int col, LspCallback cb)
{
    QJsonObject pos;
    pos["line"] = line;
    pos["character"] = col;

    QJsonObject textDoc;
    textDoc["uri"] = uri;

    QJsonObject params;
    params["textDocument"] = textDoc;
    params["position"] = pos;

    return sendRequest("textDocument/definition", params, std::move(cb));
}

int LspClient::hover(const QString &uri, int line, int col, LspCallback cb)
{
    QJsonObject pos;
    pos["line"] = line;
    pos["character"] = col;

    QJsonObject textDoc;
    textDoc["uri"] = uri;

    QJsonObject params;
    params["textDocument"] = textDoc;
    params["position"] = pos;

    return sendRequest("textDocument/hover", params, std::move(cb));
}

int LspClient::references(const QString &uri, int line, int col, LspCallback cb)
{
    QJsonObject pos;
    pos["line"] = line;
    pos["character"] = col;

    QJsonObject textDoc;
    textDoc["uri"] = uri;

    QJsonObject params;
    params["textDocument"] = textDoc;
    params["position"] = pos;

    QJsonObject context;
    context["includeDeclaration"] = true;
    params["context"] = context;

    return sendRequest("textDocument/references", params, std::move(cb));
}

int LspClient::documentSymbol(const QString &uri, LspCallback cb)
{
    QJsonObject textDoc;
    textDoc["uri"] = uri;

    QJsonObject params;
    params["textDocument"] = textDoc;

    return sendRequest("textDocument/documentSymbol", params, std::move(cb));
}

// ── Repo root → find sibling tree-sitter-cereka/ ─────────────────────────────

static QString findSiblingDir(const QString &siblingName)
{
    // Try multiple starting points to find the git repo root
    QStringList startingPoints = {
        QCoreApplication::applicationDirPath(),   // launcher binary dir
        QDir::currentPath(),                      // cwd when launcher was started
        QDir::homePath() + "/personal/dev/cereka", // dev workspace root
    };

    for (const QString &start : startingPoints) {
        QProcess git;
        git.start("git", QStringList() << "-C" << start << "rev-parse" << "--show-toplevel");
        if (!git.waitForFinished(3000) || git.exitCode() != 0)
            continue;

        QString repoRoot = QString::fromUtf8(git.readAllStandardOutput()).trimmed();
        if (repoRoot.isEmpty())
            continue;

        QDir parent = QFileInfo(repoRoot).absoluteDir();
        QString siblingPath = parent.absoluteFilePath(siblingName);
        if (QFileInfo::exists(siblingPath))
            return siblingPath;
    }
    return {};
}

// ── Binary resolution ─────────────────────────────────────────────────────────

QString LspClient::resolveBinary()
{
    // Find the tree-sitter-cereka repo via git (works wherever repos are cloned)
    QString tsDir = findSiblingDir("tree-sitter-cereka");
    if (tsDir.isEmpty()) {
        // Fallback: try the old hardcoded dev path
        tsDir = QDir::homePath() + "/dev/tree-sitter-cereka";
        if (!QFileInfo::exists(tsDir))
            return {};
    }

    QString serverDir = tsDir + "/lsp";
    QString binaryPath = serverDir + "/dist/cereka-lsp";
#ifdef _WIN32
    binaryPath += ".exe";
#endif

    // 1. Standalone SEA binary
    if (QFileInfo::exists(binaryPath))
        return binaryPath;

    // 2. Relative to launcher binary
    QString exeDir = QCoreApplication::applicationDirPath();
    QString relBinary = exeDir + "/../../tree-sitter-cereka/lsp/dist/cereka-lsp";
#ifdef _WIN32
    relBinary += ".exe";
#endif
    if (QFileInfo::exists(relBinary))
        return relBinary;

    // 3. Fallback: run server.js via node
    QString serverJs = serverDir + "/server.js";
    if (QFileInfo::exists(serverJs))
        return serverJs;

    return {};
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void LspClient::onReadyRead()
{
    m_buffer.append(m_process->readAllStandardOutput());
    processBuffer();
}

void LspClient::onProcessError(QProcess::ProcessError error)
{
    if (error == QProcess::FailedToStart) {
        emit connectionFailed();
    }
}

void LspClient::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    if (m_intentionalStop)
        return;

    // Unexpected crash — attempt reconnect
    emit connectionFailed();
    scheduleReconnect();
}

// ── Internal: JSON-RPC frame handling ─────────────────────────────────────────

int LspClient::sendRequest(const QString &method, const QJsonObject &params,
                           LspCallback cb)
{
    int id = m_requestId++;

    QJsonObject msg;
    msg["jsonrpc"] = "2.0";
    msg["id"] = id;
    msg["method"] = method;
    msg["params"] = params;

    if (cb)
        m_pendingRequests[id] = std::move(cb);

    QByteArray body = QJsonDocument(msg).toJson(QJsonDocument::Compact);
    QByteArray frame = "Content-Length: " + QByteArray::number(body.size())
                       + "\r\n\r\n" + body;

    if (m_process && m_process->state() == QProcess::Running) {
        m_process->write(frame);
    }

    return id;
}

void LspClient::sendNotification(const QString &method,
                                 const QJsonObject &params)
{
    QJsonObject msg;
    msg["jsonrpc"] = "2.0";
    msg["method"] = method;
    if (!params.isEmpty())
        msg["params"] = params;

    QByteArray body = QJsonDocument(msg).toJson(QJsonDocument::Compact);
    QByteArray frame = "Content-Length: " + QByteArray::number(body.size())
                       + "\r\n\r\n" + body;

    if (m_process && m_process->state() == QProcess::Running) {
        m_process->write(frame);
    }
}

void LspClient::handleMessage(const QJsonObject &msg)
{
    // Response (has "id" + "result" or "error")
    if (msg.contains("id") && !msg["id"].isNull()) {
        int id = msg["id"].toInt();
        auto it = m_pendingRequests.find(id);
        if (it != m_pendingRequests.end()) {
            LspCallback cb = std::move(it->second);
            m_pendingRequests.erase(it);
            if (cb)
                cb(msg);
        }
        return;
    }

    // Notification (no "id")
    QString method = msg["method"].toString();
    QJsonObject params = msg["params"].toObject();

    if (method == "textDocument/publishDiagnostics") {
        QString uri = params["uri"].toString();
        QJsonArray diags = params["diagnostics"].toArray();
        QList<LspDiagnostic> results;

        for (const QJsonValue &v : diags) {
            QJsonObject d = v.toObject();
            QJsonObject range = d["range"].toObject();
            QJsonObject start = range["start"].toObject();
            QJsonObject end = range["end"].toObject();

            LspDiagnostic diag;
            diag.line = start["line"].toInt();
            diag.col = start["character"].toInt();
            diag.endLine = end["line"].toInt();
            diag.endCol = end["character"].toInt();
            diag.message = d["message"].toString();
            diag.severity = d["severity"].toInt(1) == 1 ? "Error"
                          : d["severity"].toInt(2) == 2 ? "Warning"
                          : d["severity"].toInt(3) == 3 ? "Information"
                                                        : "Hint";
            results.append(diag);
        }

        emit diagnosticsReceived(uri, results);
    } else if (method == "window/showMessage") {
        QString type = params["type"].toInt() == 1 ? "Error"
                      : params["type"].toInt() == 2 ? "Warning"
                      : params["type"].toInt() == 3 ? "Info"
                                                    : "Log";
        QString msgText = params["message"].toString();
        emit messageReceived(type, msgText);
    }
}

void LspClient::processBuffer()
{
    while (true) {
        // Find "Content-Length: " header
        static const char HEADER[] = "Content-Length: ";
        int headerPos = m_buffer.indexOf(HEADER);
        if (headerPos < 0)
            break;

        // Parse content length
        int afterHeader = headerPos + static_cast<int>(std::strlen(HEADER));
        int endOfLine = m_buffer.indexOf("\r\n", afterHeader);
        if (endOfLine < 0)
            break; // Header incomplete

        QByteArray lenStr = m_buffer.mid(afterHeader, endOfLine - afterHeader);
        bool ok = false;
        int contentLen = lenStr.trimmed().toInt(&ok);
        if (!ok || contentLen <= 0)
            break;

        // Skip header line and blank line: Content-Length: N\r\n\r\n
        int bodyStart = endOfLine + 2; // Skip \r\n after header value
        // There should be another \r\n (blank line) before the body
        // We search for \r\n\r\n after the header
        int headerEnd = m_buffer.indexOf("\r\n\r\n", bodyStart);
        if (headerEnd < 0)
            break;
        bodyStart = headerEnd + 4; // Skip the \r\n\r\n

        // Check if we have the full body
        if (bodyStart + contentLen > m_buffer.size())
            break; // Body incomplete, wait for more data

        QByteArray body = m_buffer.mid(bodyStart, contentLen);

        // Parse JSON
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(body, &err);
        if (err.error == QJsonParseError::NoError && doc.isObject()) {
            handleMessage(doc.object());
        }

        // Remove processed bytes from buffer
        int total = bodyStart + contentLen;
        m_buffer.remove(0, total);
    }
}

// ── Reconnection ──────────────────────────────────────────────────────────────

void LspClient::scheduleReconnect()
{
    if (m_intentionalStop)
        return;

    m_reconnectAttempt++;
    int delay = std::min(
        INITIAL_RECONNECT_DELAY_MS * (1 << (m_reconnectAttempt - 1)),
        MAX_RECONNECT_DELAY_MS);

    QTimer::singleShot(delay, this, [this]() {
        if (m_intentionalStop)
            return;

        // Re-create process
        if (start(m_binaryPath))
            initialize();
    });
}

void LspClient::resetReconnect()
{
    m_reconnectAttempt = 0;
}
