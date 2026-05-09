#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QVector>

// ── TemplateInfo ────────────────────────────────────────────────────────────
//
// Describes a single project template shown in the template gallery.
//
struct TemplateInfo {
    QString name;
    QString description;

    /// Name of the default entry script (e.g. "main.crka").
    QString entryScriptName;
};

// ── TemplateModel ───────────────────────────────────────────────────────────
//
// QAbstractListModel of available project templates.  Predefined data:
//   - "Blank Project" (minimal — one script, no assets beyond what createProject
//     already provides)
//   - "Default" (current templates.hpp scaffold — full demo script with all
//     command examples, placeholder assets, scene_two, ui theme)
//
class TemplateModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        NameRole        = Qt::DisplayRole,
        DescriptionRole = Qt::UserRole + 1,
        EntryRole       = Qt::UserRole + 2,
    };

    explicit TemplateModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    /// Return the TemplateInfo for a given row, or nullptr if out of range.
    const TemplateInfo *templateAt(int row) const;

private:
    QVector<TemplateInfo> m_templates;
};
