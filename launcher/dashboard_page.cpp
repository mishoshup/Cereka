#include "dashboard_page.hpp"
#include "project_manager.hpp"
#include "template_model.hpp"
#include "theme.hpp"

#include <QDateTime>
#include <QDir>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QListView>
#include <QPainter>
#include <QSizePolicy>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QVBoxLayout>

// ── Helpers ───────────────────────────────────────────────────────────────────

static QFrame *makeHRule(const char *color = Theme::BorderDivider)
{
    QFrame *f = new QFrame();
    f->setFixedHeight(1);
    f->setStyleSheet(QString("background-color: %1; border: none;").arg(color));
    return f;
}

static QLabel *makeSectionLabel(const QString &text)
{
    QLabel *l = new QLabel(text);
    l->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: 600; "
                             "letter-spacing: 2px;")
                         .arg(Theme::TextDimmer));
    return l;
}

static QFont boldFont(int ptSize)
{
    QFont f;
    f.setPointSize(ptSize);
    f.setBold(true);
    return f;
}

// ── Constructor / Destructor ────────────────────────────────────────────────

DashboardPage::DashboardPage(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
}

DashboardPage::~DashboardPage() = default;

// ── Build UI ─────────────────────────────────────────────────────────────────

void DashboardPage::buildUi()
{
    setStyleSheet(QString("background-color: %1;").arg(Theme::BgBase));

    QVBoxLayout *v = new QVBoxLayout(this);
    v->setContentsMargins(28, 24, 28, 24);
    v->setSpacing(0);

    // ── Header: title + rename ───────────────────────────────────────────────
    QHBoxLayout *header = new QHBoxLayout();
    header->setSpacing(12);

    m_titleLabel = new QLabel();
    m_titleLabel->setFont(boldFont(Theme::FontTitle));
    m_titleLabel->setStyleSheet(QString("color: %1;").arg(Theme::TextPrimary));
    m_titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    header->addWidget(m_titleLabel);

    m_renameBtn = new QPushButton("Rename");
    m_renameBtn->setMinimumHeight(Theme::BtnHeightMicro);
    m_renameBtn->setStyleSheet(Theme::ghostBtn(Theme::RadiusSmall)
                               + "QPushButton { font-size: 11px; padding: 0 12px; }");
    connect(m_renameBtn, &QPushButton::clicked, this, &DashboardPage::onRenameClicked);
    header->addWidget(m_renameBtn);
    v->addLayout(header);

    v->addSpacing(8);

    m_statusLabel = new QLabel();
    m_statusLabel->setStyleSheet(QString("color: %1; font-size: 12px;").arg(Theme::Gold));
    v->addWidget(m_statusLabel);

    v->addSpacing(8);
    v->addWidget(makeHRule(Theme::BgSurface));
    v->addSpacing(16);

    // ── Metadata section ─────────────────────────────────────────────────────
    m_metaSection = new QWidget();
    m_metaSection->setStyleSheet("background: transparent;");
    QVBoxLayout *metaV = new QVBoxLayout(m_metaSection);
    metaV->setContentsMargins(0, 0, 0, 0);
    metaV->setSpacing(4);

    auto addMetaRow = [&](const QString &label, QLabel *&valueLabel) {
        QHBoxLayout *row = new QHBoxLayout();
        row->setSpacing(8);
        QLabel *l = new QLabel(label);
        l->setFixedWidth(120);
        l->setStyleSheet(QString("color: %1; font-size: 12px;").arg(Theme::TextFaint));
        row->addWidget(l);
        valueLabel = new QLabel();
        valueLabel->setStyleSheet(QString("color: %1; font-size: 12px;").arg(Theme::TextSecondary));
        valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        row->addWidget(valueLabel, 1);
        metaV->addLayout(row);
    };

    addMetaRow("UUID:", m_uuidValue);
    addMetaRow("Last opened:", m_lastOpenedValue);
    addMetaRow("Play time:", m_playTimeValue);
    addMetaRow("Engine version:", m_engineVersionValue);

    v->addWidget(m_metaSection);
    v->addSpacing(16);

    // ── Game actions (shown when game.cfg exists) ─────────────────────────────
    m_gameActions = new QWidget();
    m_gameActions->setStyleSheet("background: transparent;");
    QVBoxLayout *actionsV = new QVBoxLayout(m_gameActions);
    actionsV->setContentsMargins(0, 0, 0, 0);
    actionsV->setSpacing(10);
    actionsV->addWidget(makeSectionLabel("ACTIONS"));

    // Quick run row
    QHBoxLayout *runRow = new QHBoxLayout();
    runRow->setSpacing(10);

    m_quickRunBtn = new QPushButton("▶  Quick Run");
    m_quickRunBtn->setMinimumHeight(Theme::BtnHeightAction);
    m_quickRunBtn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    m_quickRunBtn->setStyleSheet(Theme::primaryBtn()
                                 + "QPushButton { padding: 0 26px; font-size: 14px; }");
    connect(m_quickRunBtn, &QPushButton::clicked, this, &DashboardPage::onQuickRunClicked);
    runRow->addWidget(m_quickRunBtn);

    m_specCombo = new QComboBox();
    m_specCombo->setMinimumHeight(Theme::BtnHeightAction);
    m_specCombo->setMinimumWidth(220);
    m_specCombo->setStyleSheet(QString(R"(
        QComboBox {
            background-color: %1; color: %2; border: 1px solid %3;
            border-radius: %4px; padding: 0 12px; font-size: 13px; min-height: 32px;
        }
        QComboBox:hover { border-color: %5; }
        QComboBox::drop-down { border: none; width: 28px; }
        QComboBox::down-arrow { image: none; }
        QComboBox QAbstractItemView {
            background-color: %1; border: 1px solid %3; selection-background-color: %6;
        }
    )").arg(Theme::BgSurface).arg(Theme::TextSecondary).arg(Theme::BorderAccent)
       .arg(Theme::RadiusStd).arg(Theme::BorderPanel).arg(Theme::GoldDimBg));
    m_specCombo->setPlaceholderText("Select spec file...");
    runRow->addWidget(m_specCombo);

    m_specRunBtn = new QPushButton("Run Spec");
    m_specRunBtn->setMinimumHeight(Theme::BtnHeightAction);
    m_specRunBtn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    m_specRunBtn->setStyleSheet(Theme::secondaryBtn()
                                + "QPushButton { padding: 0 20px; font-size: 13px; }");
    connect(m_specRunBtn, &QPushButton::clicked, this, &DashboardPage::onSpecRunClicked);
    runRow->addWidget(m_specRunBtn);

    runRow->addStretch();
    actionsV->addLayout(runRow);

    // Second row: Open in Editor
    QHBoxLayout *editRow = new QHBoxLayout();
    editRow->setSpacing(10);

    m_openEditorBtn = new QPushButton("✎  Open in Editor");
    m_openEditorBtn->setMinimumHeight(Theme::BtnHeightAction);
    m_openEditorBtn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    m_openEditorBtn->setStyleSheet(Theme::outlineBtn()
                                   + "QPushButton { padding: 0 24px; font-size: 13px; }");
    connect(m_openEditorBtn, &QPushButton::clicked, this, &DashboardPage::onOpenEditorClicked);
    editRow->addWidget(m_openEditorBtn);
    editRow->addStretch();
    actionsV->addLayout(editRow);

    v->addWidget(m_gameActions);
    v->addSpacing(16);

    // ── Template gallery ─────────────────────────────────────────────────────
    addTemplateGallerySection(v);

    v->addSpacing(16);
    v->addWidget(makeSectionLabel("OUTPUT"));
    v->addSpacing(8);

    // ── Log ───────────────────────────────────────────────────────────────────
    m_log = new QTextEdit();
    m_log->setReadOnly(true);
    m_log->setPlaceholderText("Output will appear here...");
    m_log->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_log->setStyleSheet(QString(R"(
        QTextEdit {
            background-color: %1; border: 1px solid %2;
            border-radius: %3px; padding: 14px; font-size: 12px; color: %4;
        }
    )").arg(Theme::BgDeep).arg(Theme::BorderLog).arg(Theme::RadiusStd).arg(Theme::TextLog));
    v->addWidget(m_log, 1);

    // ── Init widget (shown when no game.cfg) ─────────────────────────────────
    m_initWidget = new QWidget();
    m_initWidget->setStyleSheet("background: transparent;");
    QVBoxLayout *initV = new QVBoxLayout(m_initWidget);
    initV->setContentsMargins(0, 0, 0, 0);
    initV->setSpacing(10);

    QLabel *initDesc = new QLabel("This folder is not a Cereka project.");
    initDesc->setStyleSheet(QString("color: %1; font-size: 13px;").arg(Theme::TextFaint));
    initV->addWidget(initDesc);

    QPushButton *initBtn = new QPushButton("Initialize as Cereka Project");
    initBtn->setMinimumHeight(Theme::BtnHeightAction);
    initBtn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    initBtn->setStyleSheet(Theme::outlineBtn()
                           + "QPushButton { padding: 0 24px; font-size: 13px; }");
    connect(initBtn, &QPushButton::clicked, this, &DashboardPage::onInitClicked);
    initV->addWidget(initBtn, 0, Qt::AlignLeft);
    m_initWidget->setVisible(false);
    v->addWidget(m_initWidget);

    // ── Auto-save timer (every 60s) ───────────────────────────────────────────
    m_autoSaveTimer = new QTimer(this);
    m_autoSaveTimer->setInterval(60000);
    connect(m_autoSaveTimer, &QTimer::timeout, this, &DashboardPage::onAutoSaveTick);
    m_autoSaveTimer->start();
}

void DashboardPage::addTemplateGallerySection(QVBoxLayout *parent)
{
    m_templateGallery = new QWidget();
    m_templateGallery->setStyleSheet("background: transparent;");
    QVBoxLayout *gv = new QVBoxLayout(m_templateGallery);
    gv->setContentsMargins(0, 0, 0, 0);
    gv->setSpacing(8);

    gv->addWidget(makeSectionLabel("TEMPLATES"));

    auto *templateModel = new TemplateModel(this);
    auto *templateView  = new QListView();
    templateView->setModel(templateModel);
    templateView->setFixedHeight(120);
    templateView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    templateView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    templateView->setStyleSheet(QString(R"(
        QListView {
            background-color: %1; border: 1px solid %2;
            border-radius: %3px; padding: 4px;
        }
        QListView::item {
            padding: 8px 10px; border-radius: 4px;
            margin: 2px 0; color: %4;
        }
        QListView::item:selected {
            background-color: %5; color: %6;
        }
        QListView::item:hover:!selected {
            background-color: %7;
        }
    )").arg(Theme::BgSurface)
       .arg(Theme::BorderAccent)
       .arg(Theme::RadiusStd)
       .arg(Theme::TextSecondary)
       .arg(Theme::GoldSelectedBg)
       .arg(Theme::GoldSelected)
       .arg(Theme::BgSurfaceHover));

    // Show name + description via a custom delegate using role names
    TemplateModel *templateModelPtr = templateModel; // capture non-const for signal emission
    connect(templateView, &QListView::clicked, this,
            [this, templateModelPtr](const QModelIndex &index) {
        if (!index.isValid())
            return;
        const TemplateInfo *info = templateModelPtr->templateAt(index.row());
        if (!info)
            return;

        // Trigger new project creation with template hint.
        // LauncherWindow picks this up and calls ProjectManager::createProject
        // with the appropriate template flavour.
        emit createFromTemplate(info->name, info->entryScriptName);
    });

    // Use a styled delegate to show name + description in each item
    class TemplateDelegate : public QStyledItemDelegate {
    public:
        using QStyledItemDelegate::QStyledItemDelegate;
        QSize sizeHint(const QStyleOptionViewItem &opt,
                       const QModelIndex &) const override
        {
            return QSize(opt.rect.width(), 48);
        }
        void paint(QPainter *painter, const QStyleOptionViewItem &opt,
                   const QModelIndex &index) const override
        {
            // Draw selection/hover background
            QStyleOptionViewItem o = opt;
            initStyleOption(&o, index);
            painter->save();

            if (o.state & QStyle::State_Selected) {
                painter->fillRect(o.rect, QColor(Theme::GoldSelectedBg));
            } else if (o.state & QStyle::State_MouseOver) {
                painter->fillRect(o.rect, QColor(Theme::BgSurfaceHover));
            }

            // Name
            QRect nameRect = o.rect.adjusted(10, 6, -10, -22);
            QFont nameFont = o.font;
            nameFont.setPointSize(nameFont.pointSize() + 1);
            nameFont.setBold(true);
            painter->setFont(nameFont);
            painter->setPen(QColor(Theme::TextPrimary));
            painter->drawText(nameRect, Qt::AlignLeft | Qt::AlignBottom,
                              index.data(TemplateModel::NameRole).toString());

            // Description
            QRect descRect = o.rect.adjusted(10, 22, -10, -6);
            QFont descFont = o.font;
            descFont.setPointSize(descFont.pointSize() - 1);
            painter->setFont(descFont);
            painter->setPen(QColor(Theme::TextFaint));
            painter->drawText(descRect, Qt::AlignLeft | Qt::AlignTop,
                              index.data(TemplateModel::DescriptionRole).toString());

            painter->restore();
        }
    };
    templateView->setItemDelegate(new TemplateDelegate(templateView));

    gv->addWidget(templateView);

    parent->addWidget(m_templateGallery);
}

// ── loadProject / clearProject ───────────────────────────────────────────────

void DashboardPage::loadProject(const fs::path &projectPath)
{
    m_projectPath = projectPath;

    auto &pm = ProjectManager::instance();
    m_titleLabel->setText(QString::fromStdString(pm.currentTitle()));
    m_hasGameCfg = pm.currentHasGameCfg();

    refreshMetadata();

    if (m_hasGameCfg) {
        m_gameActions->setVisible(true);
        m_initWidget->setVisible(false);
        onRefreshSpecFiles();
    } else {
        m_gameActions->setVisible(false);
        m_initWidget->setVisible(true);
    }
}

void DashboardPage::clearProject()
{
    m_projectPath.clear();
    m_titleLabel->clear();
    m_uuidValue->clear();
    m_lastOpenedValue->clear();
    m_playTimeValue->clear();
    m_engineVersionValue->clear();
    m_specCombo->clear();
    m_log->clear();
    m_gameActions->setVisible(false);
    m_initWidget->setVisible(false);
    m_hasGameCfg = false;
}

void DashboardPage::setRunning(bool running)
{
    m_quickRunBtn->setEnabled(!running);
    m_specRunBtn->setEnabled(!running);
    m_openEditorBtn->setEnabled(!running);
    m_renameBtn->setEnabled(!running);
    m_statusLabel->setText(running ? "Running..." : "");
}

// ── Metadata ─────────────────────────────────────────────────────────────────

void DashboardPage::refreshMetadata()
{
    auto &meta = ProjectManager::instance().currentMetadata();

    m_uuidValue->setText(QString::fromStdString(meta.uuid));

    if (!meta.lastOpened.empty()) {
        QDateTime dt = QDateTime::fromString(
            QString::fromStdString(meta.lastOpened), Qt::ISODate);
        m_lastOpenedValue->setText(
            dt.isValid() ? dt.toLocalTime().toString("yyyy-MM-dd hh:mm:ss")
                         : QString::fromStdString(meta.lastOpened));
    } else {
        m_lastOpenedValue->setText("Never");
    }

    int totalSec = meta.playTimeSeconds;
    int h = totalSec / 3600;
    int m = (totalSec % 3600) / 60;
    int s = totalSec % 60;
    if (h > 0)
        m_playTimeValue->setText(QString("%1h %2m %3s").arg(h).arg(m).arg(s));
    else
        m_playTimeValue->setText(QString("%1m %2s").arg(m).arg(s));

    m_engineVersionValue->setText(
        meta.engineVersion.empty() ? "Unknown" : QString::fromStdString(meta.engineVersion));
}

// ── Spec files ───────────────────────────────────────────────────────────────

void DashboardPage::onRefreshSpecFiles()
{
    m_specCombo->clear();
    m_specCombo->addItem("Select spec file...", QString());

    fs::path scriptsDir = m_projectPath / "assets" / "scripts";
    if (!fs::exists(scriptsDir))
        return;

    std::error_code ec;
    for (auto &entry : fs::directory_iterator(scriptsDir, ec)) {
        std::string name = entry.path().filename().string();
        // Match *.spec.crka files
        if (name.size() > 10 && name.substr(name.size() - 10) == ".spec.crka") {
            QString qname = QString::fromStdString(name);
            m_specCombo->addItem(qname, qname);
        }
    }
}

// ── Slots ────────────────────────────────────────────────────────────────────

void DashboardPage::onQuickRunClicked()
{
    if (m_projectPath.empty())
        return;

    std::string entry = ProjectManager::instance().currentEntry();
    if (entry.empty())
        entry = "assets/scripts/main.crka";

    // Track play session
    ProjectManager::instance().startPlaySession();

    emit quickRunRequested(
        QString::fromStdString(m_projectPath.string()),
        QString::fromStdString(entry));
}

void DashboardPage::onSpecRunClicked()
{
    if (m_projectPath.empty())
        return;

    QString specFile = m_specCombo->currentData().toString();
    if (specFile.isEmpty())
        return;

    emit specRunRequested(
        QString::fromStdString(m_projectPath.string()),
        specFile);
}

void DashboardPage::onOpenEditorClicked()
{
    if (m_projectPath.empty())
        return;

    std::string entry = ProjectManager::instance().currentEntry();
    if (entry.empty())
        entry = "assets/scripts/main.crka";

    fs::path fullPath = m_projectPath / entry;
    emit openInEditorRequested(
        QString::fromStdString(m_projectPath.string()),
        QString::fromStdString(fullPath.string()));
}

void DashboardPage::onRenameClicked()
{
    emit renameRequested();
}

void DashboardPage::onInitClicked()
{
    emit initRequested();
}

void DashboardPage::onAutoSaveTick()
{
    // Persist accumulated play time to disk for crash safety.
    ProjectManager::instance().saveMetadata();
}
