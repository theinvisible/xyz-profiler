#pragma once

#include <QDate>
#include <QDialog>
#include <QString>

class QDateEdit;
class QLineEdit;

namespace xyz {

// Small "lend this title out" prompt: who has it, and when it is due back.
// Both are optional — DP4 exports contain plenty of loans with no due date,
// and knowing a disc is out is useful even without a name.
class LendDialog : public QDialog {
    Q_OBJECT

public:
    explicit LendDialog(const QString& movieTitle, QWidget* parent = nullptr);

    QString firstName() const;
    QString lastName() const;
    // Invalid when the user left the date on "not set".
    QDate   due() const;

private:
    QLineEdit* m_firstName = nullptr;
    QLineEdit* m_lastName  = nullptr;
    QDateEdit* m_due       = nullptr;
};

} // namespace xyz
