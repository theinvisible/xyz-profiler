#pragma once

#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QStringList>
#include <QVBoxLayout>

namespace xyz {

// Modal dialog shown after a Collection.xml is parsed.  Displays a summary
// (count + source file + cover images path) and the first few titles so the
// user can confirm before the write-to-DB step.
class ImportPreviewDialog : public QDialog {
    Q_OBJECT

public:
    explicit ImportPreviewDialog(QWidget* parent = nullptr);

    /// Populate the preview.  @p count total movies found, @p sourceName
    /// the import file basename, @p sampleTitles first <=8 titles,
    /// @p imagesDir the cover-images directory (may be empty).
    void setPreview(int count,
                    const QString& sourceName,
                    const QStringList& sampleTitles,
                    const QString& imagesDir);

private:
    void buildUi();

    QLabel*          m_summaryLabel  = nullptr;
    QLabel*          m_imagesLabel   = nullptr;
    QListWidget*     m_titleList     = nullptr;
    QLabel*          m_moreLabel     = nullptr;
    QDialogButtonBox* m_buttonBox    = nullptr;
};

} // namespace xyz
