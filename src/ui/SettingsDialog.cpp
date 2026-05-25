#include "SettingsDialog.h"

#include "controllers/SettingsController.h"

#include <QSvgRenderer>

namespace xyz {

// -----------------------------------------------------------------------
// Construction
// -----------------------------------------------------------------------

SettingsDialog::SettingsDialog(SettingsController* settings,
                               QWidget* parent)
    : QDialog(parent)
    , m_settings(settings)
{
    setWindowTitle(QStringLiteral("Settings"));
    setModal(true);
    buildUi();
    loadFromController();
}

// -----------------------------------------------------------------------
// UI construction
// -----------------------------------------------------------------------

void SettingsDialog::buildUi()
{
    auto* root = new QVBoxLayout(this);

    // --- TMDb API Key group -----------------------------------------------
    {
        auto* group = new QGroupBox(QStringLiteral("TMDb API Key"), this);
        auto* groupLayout = new QVBoxLayout(group);

        auto* keyRow = new QHBoxLayout();
        m_apiKeyEdit = new QLineEdit(group);
        m_apiKeyEdit->setEchoMode(QLineEdit::Password);
        m_apiKeyEdit->setPlaceholderText(
            QStringLiteral("Enter your TMDb v3 API key"));
        keyRow->addWidget(m_apiKeyEdit, 1);

        m_showKeyCheck = new QCheckBox(QStringLiteral("Show"), group);
        connect(m_showKeyCheck, &QCheckBox::toggled, this,
                [this](bool checked) {
                    m_apiKeyEdit->setEchoMode(
                        checked ? QLineEdit::Normal : QLineEdit::Password);
                });
        keyRow->addWidget(m_showKeyCheck);
        groupLayout->addLayout(keyRow);

        // TMDb attribution
        auto* attrRow = new QHBoxLayout();
        attrRow->setContentsMargins(0, 4, 0, 0);

        auto* tmdbLogo = new QLabel(group);
        {
            QSvgRenderer renderer(QStringLiteral(":/tmdb_logo.svg"));
            if (renderer.isValid()) {
                QPixmap pix(120, 20);
                pix.fill(Qt::transparent);
                QPainter p(&pix);
                renderer.render(&p);
                tmdbLogo->setPixmap(pix);
            } else {
                tmdbLogo->setText(QStringLiteral("TMDb"));
            }
        }
        tmdbLogo->setFixedHeight(20);
        attrRow->addWidget(tmdbLogo);

        auto* attrText = new QLabel(
            QStringLiteral("This product uses the TMDb API but is not "
                           "endorsed or certified by TMDb."),
            group);
        attrText->setWordWrap(true);
        QFont attrFont = attrText->font();
        attrFont.setPointSize(9);
        attrText->setFont(attrFont);
        attrRow->addWidget(attrText, 1);
        groupLayout->addLayout(attrRow);

        root->addWidget(group);
    }

    // --- Cover Images Directory group -------------------------------------
    {
        auto* group = new QGroupBox(
            QStringLiteral("Cover Images Directory"), this);
        auto* groupLayout = new QHBoxLayout(group);

        m_imagesDirEdit = new QLineEdit(group);
        m_imagesDirEdit->setPlaceholderText(
            QStringLiteral("Path to DVD Profiler Images/ folder"));
        groupLayout->addWidget(m_imagesDirEdit, 1);

        auto* browseBtn = new QPushButton(
            QStringLiteral("Browse…"), group);
        connect(browseBtn, &QPushButton::clicked, this, [this]() {
            const QString dir = QFileDialog::getExistingDirectory(
                this,
                QStringLiteral("Select Cover Images Directory"),
                m_imagesDirEdit->text());
            if (!dir.isEmpty()) {
                m_imagesDirEdit->setText(dir);
            }
        });
        groupLayout->addWidget(browseBtn);

        root->addWidget(group);
    }

    // --- Theme group ------------------------------------------------------
    {
        auto* group = new QGroupBox(QStringLiteral("Theme"), this);
        auto* groupLayout = new QHBoxLayout(group);

        m_themeCombo = new QComboBox(group);
        m_themeCombo->addItem(QStringLiteral("Dark"));
        m_themeCombo->addItem(QStringLiteral("Light"));
        m_themeCombo->addItem(QStringLiteral("System"));
        groupLayout->addWidget(m_themeCombo);
        groupLayout->addStretch();

        root->addWidget(group);
    }

    root->addStretch();

    // --- Button box -------------------------------------------------------
    m_buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);

    connect(m_buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        saveToController();
        accept();
    });
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    root->addWidget(m_buttonBox);
}

// -----------------------------------------------------------------------
// Settings round-trip
// -----------------------------------------------------------------------

void SettingsDialog::loadFromController()
{
    if (!m_settings) return;

    m_apiKeyEdit->setText(m_settings->tmdbApiKey());
    m_imagesDirEdit->setText(m_settings->imagesDirectory());

    // Map stored theme name to combo index.
    const QString theme = m_settings->themeName();
    const int idx = m_themeCombo->findText(theme, Qt::MatchFixedString);
    m_themeCombo->setCurrentIndex(idx >= 0 ? idx : 0);
}

void SettingsDialog::saveToController()
{
    if (!m_settings) return;

    m_settings->setTmdbApiKey(m_apiKeyEdit->text().trimmed());
    m_settings->setImagesDirectory(m_imagesDirEdit->text().trimmed());
    m_settings->setThemeName(m_themeCombo->currentText());
}

} // namespace xyz
