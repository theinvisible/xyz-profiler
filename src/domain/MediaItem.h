#pragma once

#include "domain/CollectionMembership.h"
#include "domain/CustomField.h"
#include "domain/Event.h"
#include "domain/IdMetadata.h"
#include "domain/LoanInfo.h"
#include "domain/MonetaryAmount.h"
#include "domain/Person.h"
#include "domain/PurchaseInfo.h"
#include "domain/RatingInfo.h"
#include "domain/Review.h"

#include <QDate>
#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>

namespace xyz {

// Base struct for any catalogued item — film, series, game, ... — carrying
// only fields that make sense across media types. Specialisations (Movie,
// future Game, ...) inherit from this and add format-specific data.
//
// Plain aggregate inheritance keeps value semantics; this is a data carrier,
// not a polymorphic interface. Code that needs to handle mixed-type lists
// will dispatch on a separate `kind` discriminator at the persistence layer,
// not via virtual functions here.
struct MediaItem {
    // ---- Identity ----------------------------------------------------------
    QString    id;             // primary identifier from the source (UPC/EAN for DP4)
    IdMetadata idMetadata;     // pre-parsed sibling metadata (base / variant / locality)
    QString    upc;            // formatted/user-friendly UPC, distinct from `id`

    // ---- Titles ------------------------------------------------------------
    QString title;
    QString originalTitle;
    QString sortTitle;
    QString distTrait;         // edition trait ("Director's Cut", "Steelbook", ...)

    // ---- Provenance --------------------------------------------------------
    int     productionYear = 0;
    QDate   releaseDate;
    QStringList countriesOfOrigin;  // up to three in DP4

    // ---- Classification ----------------------------------------------------
    QStringList genres;
    QStringList tags;
    RatingInfo  rating;
    QString     caseType;
    bool        caseSlipCover = false;
    QStringList regions;            // "A", "B", "1", "2", ...

    // ---- People & studios --------------------------------------------------
    QList<Person> actors;     // for games: voice cast
    QList<Person> credits;    // crew / developers / designers / composers
    QStringList   studios;    // film studios / game publishers / developers
    QStringList   mediaCompanies;   // distributors / labels

    // ---- User-managed ------------------------------------------------------
    int                  collectionNumber = 0;
    int                  countAs          = 1;
    int                  wishPriority     = 0;
    CollectionMembership membership;
    QList<CustomField>   customFields;
    QStringList          lockedFields;    // names of fields locked from online updates
    QString              myLinks;         // raw text body; DP4 schema unclear

    // ---- Descriptions ------------------------------------------------------
    QString overview;
    QString notes;
    QString easterEggs;             // free text, can be multi-line

    // ---- Purchase & price --------------------------------------------------
    PurchaseInfo   purchase;
    MonetaryAmount srp;             // suggested retail price

    // ---- Lending -----------------------------------------------------------
    LoanInfo     loan;
    QList<Event> events;

    // ---- Review ------------------------------------------------------------
    Review review;

    // ---- Sync / timestamps -------------------------------------------------
    QDateTime profileTimestamp;     // when the profile was first created/synced
    QDateTime lastEdited;

    // ---- Storage location --------------------------------------------------
    QString locationId;
    QString coverFrontPath;  // resolved on import if an images dir was provided
    QString coverBackPath;
};

} // namespace xyz
