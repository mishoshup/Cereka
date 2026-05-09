#include "template_model.hpp"

// ── Constructor ──────────────────────────────────────────────────────────────

TemplateModel::TemplateModel(QObject *parent)
    : QAbstractListModel(parent)
{
    m_templates = {
        {
            QStringLiteral("Blank Project"),
            QStringLiteral("Minimal scaffold: entry script + empty assets folders. "
                           "Start from scratch with no example code."),
            QStringLiteral("main.crka"),
        },
        {
            QStringLiteral("Default"),
            QStringLiteral("Full demo project with main menu, dialogue, "
                           "variables, choices, branching, save/load, "
                           "and placeholder graphics."),
            QStringLiteral("main.crka"),
        },
    };
}

// ── Model interface ─────────────────────────────────────────────────────────

int TemplateModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return static_cast<int>(m_templates.size());
}

QVariant TemplateModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_templates.size())
        return {};

    const TemplateInfo &t = m_templates[index.row()];

    switch (role) {
    case NameRole:
        return t.name;
    case DescriptionRole:
        return t.description;
    case EntryRole:
        return t.entryScriptName;
    default:
        return {};
    }
}

QHash<int, QByteArray> TemplateModel::roleNames() const
{
    return {
        { NameRole,        "name" },
        { DescriptionRole, "description" },
        { EntryRole,       "entryScript" },
    };
}

const TemplateInfo *TemplateModel::templateAt(int row) const
{
    if (row < 0 || row >= m_templates.size())
        return nullptr;
    return &m_templates[row];
}
