#include "ui/LendDialog.h"

#include "ui/Theme.h"

#include <QDateEdit>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace xyz {
namespace {

// Same sentinel trick as EditMovieDialog: QDateEdit cannot be empty, so its
// minimum date doubles as "no due date" and shows specialValueText.
const QDate kNoDate(1900, 1, 1);

QLabel* fieldLabel(const QString& text)
{
    auto* l = new QLabel(text);
    l->setStyleSheet(QStringLiteral("color:%1;").arg(Theme::current().text2.name()));
    return l;
}

} // namespace

LendDialog::LendDialog(const QString& movieTitle, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Lend Out"));
    setModal(true);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 12);
    root->setSpacing(12);

    auto* intro = new QLabel(tr("Lend out \"%1\".").arg(movieTitle));
    intro->setWordWrap(true);
    root->addWidget(intro);

    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(8);

    m_firstName = new QLineEdit;
    m_lastName  = new QLineEdit;
    grid->addWidget(fieldLabel(tr("First name")), 0, 0);
    grid->addWidget(m_firstName, 0, 1);
    grid->addWidget(fieldLabel(tr("Last name")), 1, 0);
    grid->addWidget(m_lastName, 1, 1);

    m_due = new QDateEdit;
    m_due->setCalendarPopup(true);
    m_due->setDisplayFormat(QStringLiteral("dd.MM.yyyy"));
    m_due->setMinimumDate(kNoDate);
    m_due->setSpecialValueText(tr("Not set"));
    m_due->setDate(kNoDate);
    grid->addWidget(fieldLabel(tr("Due back")), 2, 0);
    grid->addWidget(m_due, 2, 1);

    grid->setColumnStretch(1, 1);
    root->addLayout(grid);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    if (auto* ok = buttons->button(QDialogButtonBox::Ok)) {
        ok->setText(tr("Lend out"));
        ok->setObjectName(QStringLiteral("primary"));
    }
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    m_firstName->setFocus();
}

QString LendDialog::firstName() const { return m_firstName->text().trimmed(); }
QString LendDialog::lastName()  const { return m_lastName->text().trimmed(); }

QDate LendDialog::due() const
{
    const QDate d = m_due->date();
    return (d == kNoDate) ? QDate() : d;
}

} // namespace xyz
