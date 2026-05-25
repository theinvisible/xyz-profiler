#include "ImportPreviewDialog.h"

namespace xyz {

// -----------------------------------------------------------------------
// Construction
// -----------------------------------------------------------------------

ImportPreviewDialog::ImportPreviewDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Import Preview"));
    setModal(true);
    buildUi();
}

// -----------------------------------------------------------------------
// Public interface
// -----------------------------------------------------------------------

void ImportPreviewDialog::setPreview(int count,
                                     const QString& sourceName,
                                     const QStringList& sampleTitles,
                                     const QString& imagesDir)
{
    m_summaryLabel->setText(
        QStringLiteral("Ready to import %1 movies from %2")
            .arg(count)
            .arg(sourceName));

    if (imagesDir.isEmpty()) {
        m_imagesLabel->setText(
            QStringLiteral("Cover images: <i>not configured</i>"));
    } else {
        m_imagesLabel->setText(
            QStringLiteral("Cover images: %1").arg(imagesDir));
    }

    m_titleList->clear();
    const int shown = qMin(sampleTitles.size(), 8);
    for (int i = 0; i < shown; ++i) {
        m_titleList->addItem(sampleTitles.at(i));
    }
    // Make the list read-only — items are not selectable.
    m_titleList->setSelectionMode(QAbstractItemView::NoSelection);

    if (count > 8) {
        m_moreLabel->setText(
            QStringLiteral("…and %1 more").arg(count - 8));
        m_moreLabel->setVisible(true);
    } else {
        m_moreLabel->setVisible(false);
    }
}

// -----------------------------------------------------------------------
// UI construction
// -----------------------------------------------------------------------

void ImportPreviewDialog::buildUi()
{
    auto* root = new QVBoxLayout(this);

    m_summaryLabel = new QLabel(this);
    QFont summaryFont = m_summaryLabel->font();
    summaryFont.setPointSize(12);
    m_summaryLabel->setFont(summaryFont);
    m_summaryLabel->setWordWrap(true);
    root->addWidget(m_summaryLabel);

    m_imagesLabel = new QLabel(this);
    m_imagesLabel->setWordWrap(true);
    root->addWidget(m_imagesLabel);

    root->addSpacing(8);

    m_titleList = new QListWidget(this);
    m_titleList->setSelectionMode(QAbstractItemView::NoSelection);
    root->addWidget(m_titleList, 1);

    m_moreLabel = new QLabel(this);
    m_moreLabel->setVisible(false);
    QFont moreFont = m_moreLabel->font();
    moreFont.setItalic(true);
    m_moreLabel->setFont(moreFont);
    root->addWidget(m_moreLabel);

    root->addSpacing(8);

    m_buttonBox = new QDialogButtonBox(this);
    auto* importBtn = m_buttonBox->addButton(
        QStringLiteral("Import"), QDialogButtonBox::AcceptRole);
    m_buttonBox->addButton(QDialogButtonBox::Cancel);

    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Give Import the default focus
    importBtn->setDefault(true);

    root->addWidget(m_buttonBox);
}

} // namespace xyz
