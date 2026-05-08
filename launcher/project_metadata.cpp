#include "project_metadata.hpp"

#include <QUuid>
#include <QDateTime>

#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

// ── Helpers ──────────────────────────────────────────────────────────────────

static std::string isoNow()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toStdString();
}

static std::string readFile(const fs::path &path)
{
    std::ifstream f(path);
    if (!f)
        return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static bool writeFile(const fs::path &path, const std::string &content)
{
    std::ofstream f(path);
    if (!f)
        return false;
    f << content;
    return f.good();
}

static std::string jsonEscape(const std::string &s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;
        }
    }
    return out;
}

// Simple JSON string extraction (no full parser needed for our schema).
static std::string jsonStringValue(const std::string &json, const std::string &key)
{
    std::string search = "\"" + key + "\": \"";
    auto pos = json.find(search);
    if (pos == std::string::npos)
        return {};

    pos += search.size();
    std::string val;
    while (pos < json.size()) {
        char c = json[pos];
        if (c == '"')
            break;
        if (c == '\\' && pos + 1 < json.size()) {
            char n = json[pos + 1];
            switch (n) {
                case '"':  val += '"';  break;
                case '\\': val += '\\'; break;
                case 'n':  val += '\n'; break;
                case 't':  val += '\t'; break;
                default:   val += n;
            }
            pos += 2;
        } else {
            val += c;
            ++pos;
        }
    }
    return val;
}

// ── Serialisation ───────────────────────────────────────────────────────────

bool ProjectMetadata::load(const fs::path &projectDir)
{
    fs::path filePath = projectDir / ".cereka" / "project.json";
    std::string json = readFile(filePath);
    if (json.empty())
        return false;

    uuid          = jsonStringValue(json, "uuid");
    title         = jsonStringValue(json, "title");
    lastOpened    = jsonStringValue(json, "lastOpened");
    engineVersion = jsonStringValue(json, "engineVersion");

    // playTimeSeconds is an int
    {
        std::string search = "\"playTimeSeconds\": ";
        auto pos = json.find(search);
        if (pos != std::string::npos) {
            pos += search.size();
            playTimeSeconds = 0;
            while (pos < json.size() && json[pos] >= '0' && json[pos] <= '9') {
                playTimeSeconds = playTimeSeconds * 10 + (json[pos] - '0');
                ++pos;
            }
        }
    }

    return !uuid.empty();
}

bool ProjectMetadata::save(const fs::path &projectDir) const
{
    fs::path dir = projectDir / ".cereka";
    std::error_code ec;
    if (!fs::exists(dir))
        fs::create_directories(dir, ec);

    fs::path filePath = dir / "project.json";

    std::string json;
    json += "{\n";
    json += "    \"uuid\": \""              + jsonEscape(uuid)          + "\",\n";
    json += "    \"title\": \""             + jsonEscape(title)         + "\",\n";
    json += "    \"lastOpened\": \""        + jsonEscape(lastOpened)    + "\",\n";
    json += "    \"playTimeSeconds\": "     + std::to_string(playTimeSeconds) + ",\n";
    json += "    \"engineVersion\": \""     + jsonEscape(engineVersion) + "\"\n";
    json += "}\n";

    return writeFile(filePath, json);
}

// ── Factory ─────────────────────────────────────────────────────────────────

ProjectMetadata ProjectMetadata::create(const std::string &title)
{
    ProjectMetadata m;
    m.uuid          = QUuid::createUuid().toString(QUuid::Id128).toStdString();
    m.title         = title;
    m.lastOpened    = isoNow();
    m.playTimeSeconds = 0;
    m.engineVersion = "1.0.0";
    return m;
}
