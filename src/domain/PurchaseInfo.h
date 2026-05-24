#pragma once

#include "domain/MonetaryAmount.h"

#include <QDate>
#include <QString>

namespace xyz {

// Captures when/where/how a media item was acquired.
//
// Maps to DP4's <PurchaseInfo>: price + denomination as MonetaryAmount,
// place metadata as three parallel strings, the gift attribution as a
// dedicated bool + name pair.
struct PurchaseInfo {
    MonetaryAmount price;

    QString place;
    QString placeType;
    QString placeWebsite;

    QDate   date;

    bool    receivedAsGift = false;
    QString giftFromFirstName;
    QString giftFromLastName;
};

} // namespace xyz
