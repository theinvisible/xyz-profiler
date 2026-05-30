#pragma once

#include <QDialog>
#include <QString>

class QCheckBox;
class QLineEdit;
class QStackedWidget;

namespace xyz {

class SettingsController;

// Preferences dialog with a category sidebar (Appearance / Library /
// Data & Sync / About), ported from the Claude Design layout. Edits a local
// copy of every setting; changes are committed to the SettingsController only
// when the user clicks Save.
class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(SettingsController* settings,
                            QWidget* parent = nullptr);

private:
    void buildUi();
    void loadFromController();
    void saveToController();

    // Builds a segmented control bound to `value`. `options` is a list of
    // {storedValue, label} pairs.
    QWidget* makeSegmented(const QList<QPair<QString, QString>>& options,
                           QString* value);

    SettingsController* m_settings = nullptr;

    QStackedWidget* m_pages = nullptr;

    QString m_theme;   // "Light" | "Dark" | "System"
    QString m_view;    // "list" | "grid"

    QLineEdit* m_apiKeyEdit    = nullptr;
    QCheckBox* m_showKeyCheck  = nullptr;
    QLineEdit* m_imagesDirEdit = nullptr;
};

} // namespace xyz
