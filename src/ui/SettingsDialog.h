#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QSvgRenderer>
#include <QVBoxLayout>

namespace xyz {

class SettingsController;

// Modal dialog for editing persistent user preferences.  Works on a
// local copy of every setting; changes are written to the
// SettingsController only when the user clicks Save.
class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(SettingsController* settings,
                            QWidget* parent = nullptr);

private:
    void buildUi();
    void loadFromController();
    void saveToController();

    SettingsController* m_settings = nullptr;

    // TMDb group
    QLineEdit*  m_apiKeyEdit   = nullptr;
    QCheckBox*  m_showKeyCheck = nullptr;

    // Cover images group
    QLineEdit*  m_imagesDirEdit = nullptr;

    // Theme group
    QComboBox*  m_themeCombo = nullptr;

    QDialogButtonBox* m_buttonBox = nullptr;
};

} // namespace xyz
