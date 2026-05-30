#include "SettingsDialog.h"

#include "controllers/SettingsController.h"
#include "ui/IconFactory.h"
#include "ui/Theme.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QStackedWidget>
#include <QSvgRenderer>
#include <QToolButton>
#include <QVBoxLayout>

namespace xyz {
namespace {

QLabel* groupLabel(const QString& text)
{
    auto* l = new QLabel(text);
    QFont f = l->font();
    f.setBold(true);
    l->setFont(f);
    return l;
}

} // namespace

SettingsDialog::SettingsDialog(SettingsController* settings, QWidget* parent)
    : QDialog(parent), m_settings(settings)
{
    setWindowTitle(tr("Settings"));
    setModal(true);
    resize(660, 480);

    // Seed the segmented selections before building so the right buttons
    // start checked (makeSegmented reads these at build time).
    if (m_settings) {
        m_theme = m_settings->themeName();
        m_view  = m_settings->viewMode();
    }
    buildUi();
    loadFromController();
}

QWidget* SettingsDialog::makeSegmented(
    const QList<QPair<QString, QString>>& options, QString* value)
{
    auto* wrap = new QWidget;
    wrap->setObjectName(QStringLiteral("viewSeg"));
    auto* lay = new QHBoxLayout(wrap);
    lay->setContentsMargins(2, 2, 2, 2);
    lay->setSpacing(2);
    wrap->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);

    auto* group = new QButtonGroup(wrap);
    group->setExclusive(true);
    for (const auto& opt : options) {
        auto* btn = new QToolButton;
        btn->setObjectName(QStringLiteral("segText"));
        btn->setText(opt.second);
        btn->setCheckable(true);
        btn->setChecked(*value == opt.first);
        const QString stored = opt.first;
        connect(btn, &QToolButton::clicked, this,
                [value, stored] { *value = stored; });
        group->addButton(btn);
        lay->addWidget(btn);
    }
    return wrap;
}

void SettingsDialog::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // --- Header -----------------------------------------------------------
    auto* header = new QWidget;
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(20, 16, 20, 16);
    hl->addWidget(groupLabel(tr("Settings")));
    hl->addStretch();
    root->addWidget(header);

    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(QStringLiteral("color:%1;").arg(Theme::current().border.name()));
    root->addWidget(sep);

    // --- Body: sidebar + pages -------------------------------------------
    auto* body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);

    auto* sidebar = new QWidget;
    sidebar->setObjectName(QStringLiteral("prefSidebar"));
    sidebar->setFixedWidth(190);
    auto* sl = new QVBoxLayout(sidebar);
    sl->setContentsMargins(10, 10, 10, 10);
    sl->setSpacing(2);

    m_pages = new QStackedWidget;

    struct Cat { QString label; QString icon; };
    const QList<Cat> cats = {
        { tr("Appearance"),   QStringLiteral("sun") },
        { tr("Library"),      QStringLiteral("film") },
        { tr("Data & Sync"),  QStringLiteral("refresh") },
        { tr("About"),        QStringLiteral("disc") },
    };

    auto* catGroup = new QButtonGroup(this);
    catGroup->setExclusive(true);
    for (int i = 0; i < cats.size(); ++i) {
        auto* btn = new QToolButton;
        btn->setObjectName(QStringLiteral("prefCat"));
        btn->setText(cats[i].label);
        btn->setIcon(IconFactory::icon(cats[i].icon, Theme::current().text2, 16));
        btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        btn->setCheckable(true);
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        btn->setChecked(i == 0);
        connect(btn, &QToolButton::clicked, this, [this, i] { m_pages->setCurrentIndex(i); });
        catGroup->addButton(btn);
        sl->addWidget(btn);
    }
    sl->addStretch();

    // ---- Page: Appearance ------------------------------------------------
    {
        auto* page = new QWidget;
        auto* pl = new QVBoxLayout(page);
        pl->setContentsMargins(24, 24, 24, 24);
        pl->setSpacing(26);

        auto* themeGroup = new QVBoxLayout;
        themeGroup->setSpacing(11);
        themeGroup->addWidget(groupLabel(tr("Appearance")));
        themeGroup->addWidget(makeSegmented({
            { QStringLiteral("Light"),  tr("Light") },
            { QStringLiteral("Dark"),   tr("Dark") },
            { QStringLiteral("System"), tr("System") },
        }, &m_theme));
        pl->addLayout(themeGroup);

        auto* viewGroup = new QVBoxLayout;
        viewGroup->setSpacing(11);
        viewGroup->addWidget(groupLabel(tr("Default view")));
        viewGroup->addWidget(makeSegmented({
            { QStringLiteral("list"), tr("List") },
            { QStringLiteral("grid"), tr("Cover grid") },
        }, &m_view));
        pl->addLayout(viewGroup);

        pl->addStretch();
        m_pages->addWidget(page);
    }

    // ---- Page: Library ---------------------------------------------------
    {
        auto* page = new QWidget;
        auto* pl = new QVBoxLayout(page);
        pl->setContentsMargins(24, 24, 24, 24);
        pl->setSpacing(11);
        pl->addWidget(groupLabel(tr("Cover images directory")));
        auto* row = new QHBoxLayout;
        m_imagesDirEdit = new QLineEdit;
        m_imagesDirEdit->setPlaceholderText(tr("Path to DVD Profiler Images/ folder"));
        row->addWidget(m_imagesDirEdit, 1);
        auto* browse = new QPushButton(tr("Browse…"));
        connect(browse, &QPushButton::clicked, this, [this] {
            const QString dir = QFileDialog::getExistingDirectory(
                this, tr("Select Cover Images Directory"), m_imagesDirEdit->text());
            if (!dir.isEmpty()) m_imagesDirEdit->setText(dir);
        });
        row->addWidget(browse);
        pl->addLayout(row);
        pl->addStretch();
        m_pages->addWidget(page);
    }

    // ---- Page: Data & Sync ----------------------------------------------
    {
        auto* page = new QWidget;
        auto* pl = new QVBoxLayout(page);
        pl->setContentsMargins(24, 24, 24, 24);
        pl->setSpacing(11);
        pl->addWidget(groupLabel(tr("TMDb API key")));
        auto* row = new QHBoxLayout;
        m_apiKeyEdit = new QLineEdit;
        m_apiKeyEdit->setEchoMode(QLineEdit::Password);
        m_apiKeyEdit->setPlaceholderText(tr("Enter your TMDb v3 API key"));
        row->addWidget(m_apiKeyEdit, 1);
        m_showKeyCheck = new QCheckBox(tr("Show"));
        connect(m_showKeyCheck, &QCheckBox::toggled, this, [this](bool on) {
            m_apiKeyEdit->setEchoMode(on ? QLineEdit::Normal : QLineEdit::Password);
        });
        row->addWidget(m_showKeyCheck);
        pl->addLayout(row);

        // Attribution.
        auto* attrRow = new QHBoxLayout;
        attrRow->setContentsMargins(0, 8, 0, 0);
        auto* logo = new QLabel;
        {
            QSvgRenderer renderer(QStringLiteral(":/tmdb_logo.svg"));
            if (renderer.isValid()) {
                QPixmap pix(120, 20);
                pix.fill(Qt::transparent);
                QPainter p(&pix);
                renderer.render(&p);
                logo->setPixmap(pix);
            } else {
                logo->setText(QStringLiteral("TMDb"));
            }
        }
        attrRow->addWidget(logo);
        auto* attr = new QLabel(tr("This product uses the TMDb API but is not "
                                   "endorsed or certified by TMDb."));
        attr->setWordWrap(true);
        attr->setStyleSheet(QStringLiteral("color:%1;").arg(Theme::current().text2.name()));
        attrRow->addWidget(attr, 1);
        pl->addLayout(attrRow);

        pl->addStretch();
        m_pages->addWidget(page);
    }

    // ---- Page: About -----------------------------------------------------
    {
        auto* page = new QWidget;
        auto* pl = new QVBoxLayout(page);
        pl->setContentsMargins(24, 24, 24, 24);
        pl->setAlignment(Qt::AlignCenter);
        pl->setSpacing(10);
        auto* icon = new QLabel;
        icon->setPixmap(IconFactory::pixmap(QStringLiteral("disc"),
                                            Theme::current().text3, 40, 1.1,
                                            devicePixelRatioF()));
        icon->setAlignment(Qt::AlignCenter);
        pl->addWidget(icon);
        auto* name = groupLabel(tr("XYZ Profiler"));
        name->setAlignment(Qt::AlignCenter);
        pl->addWidget(name);
        auto* desc = new QLabel(tr("A modern manager for your DVD / Blu-ray / "
                                   "UHD collection.\nBuilt with Qt 6 Widgets."));
        desc->setAlignment(Qt::AlignCenter);
        desc->setStyleSheet(QStringLiteral("color:%1;").arg(Theme::current().text2.name()));
        pl->addWidget(desc);
        m_pages->addWidget(page);
    }

    body->addWidget(sidebar);
    body->addWidget(m_pages, 1);
    root->addLayout(body, 1);

    // --- Footer -----------------------------------------------------------
    auto* footSep = new QFrame;
    footSep->setFrameShape(QFrame::HLine);
    footSep->setStyleSheet(QStringLiteral("color:%1;").arg(Theme::current().border.name()));
    root->addWidget(footSep);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    if (auto* save = buttons->button(QDialogButtonBox::Save))
        save->setObjectName(QStringLiteral("primary"));
    auto* footer = new QWidget;
    auto* fl = new QHBoxLayout(footer);
    fl->setContentsMargins(20, 12, 20, 12);
    fl->addStretch();
    fl->addWidget(buttons);
    root->addWidget(footer);

    connect(buttons, &QDialogButtonBox::accepted, this, [this] { saveToController(); accept(); });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void SettingsDialog::loadFromController()
{
    if (!m_settings) return;
    m_theme = m_settings->themeName();
    m_view  = m_settings->viewMode();
    m_apiKeyEdit->setText(m_settings->tmdbApiKey());
    m_imagesDirEdit->setText(m_settings->imagesDirectory());
    // Segmented controls already reflect m_theme/m_view at build time.
}

void SettingsDialog::saveToController()
{
    if (!m_settings) return;
    m_settings->setTmdbApiKey(m_apiKeyEdit->text().trimmed());
    m_settings->setImagesDirectory(m_imagesDirEdit->text().trimmed());
    m_settings->setViewMode(m_view);
    m_settings->setThemeName(m_theme);
}

} // namespace xyz
