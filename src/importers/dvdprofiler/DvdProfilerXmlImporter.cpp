#include "DvdProfilerXmlImporter.h"

#include "domain/MediaFormat.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QXmlStreamReader>

namespace xyz {
namespace {

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------

QString attr(const QXmlStreamReader& r, const char* name)
{
    return r.attributes().value(QLatin1String(name)).toString();
}

bool isTruthy(const QString& s)
{
    return s.compare(QLatin1String("True"), Qt::CaseInsensitive) == 0;
}

bool attrTruthy(const QXmlStreamReader& r, const char* name)
{
    return isTruthy(attr(r, name));
}

// Read a flat list of <Child> text elements, e.g.
//   <Wrapper>
//     <Item>a</Item>
//     <Item>b</Item>
//   </Wrapper>
void readStringList(QXmlStreamReader& r, const char* itemName, QStringList& out)
{
    while (r.readNextStartElement()) {
        if (r.name() == QLatin1String(itemName)) {
            out << r.readElementText();
        } else {
            r.skipCurrentElement();
        }
    }
}

// Reusable DP4 <User FirstName=".." LastName=".." EmailAddress=".." PhoneNumber=".."/>
struct DpUser {
    QString firstName, lastName, email, phone;
};

DpUser readUser(QXmlStreamReader& r)
{
    DpUser u;
    u.firstName = attr(r, "FirstName");
    u.lastName  = attr(r, "LastName");
    u.email     = attr(r, "EmailAddress");
    u.phone     = attr(r, "PhoneNumber");
    r.skipCurrentElement();
    return u;
}

// ---------------------------------------------------------------------------
// Sub-element readers — one per logical section of <DVD>.
// Each one is invoked positioned on the section's opening tag and must
// consume the matching end tag before returning.
// ---------------------------------------------------------------------------

void readGenres   (QXmlStreamReader& r, Movie& m) { readStringList(r, "Genre",        m.genres); }
void readStudios  (QXmlStreamReader& r, Movie& m) { readStringList(r, "Studio",       m.studios); }
void readTags     (QXmlStreamReader& r, Movie& m) { readStringList(r, "Tag",          m.tags); }
void readSubtitles(QXmlStreamReader& r, Movie& m) { readStringList(r, "Subtitle",     m.subtitles); }
void readRegions  (QXmlStreamReader& r, Movie& m) { readStringList(r, "Region",       m.regions); }
void readMediaCos (QXmlStreamReader& r, Movie& m) { readStringList(r, "MediaCompany", m.mediaCompanies); }

void readActors(QXmlStreamReader& r, Movie& m)
{
    while (r.readNextStartElement()) {
        if (r.name() == u"Actor") {
            Person p;
            p.firstName  = attr(r, "FirstName");
            p.middleName = attr(r, "MiddleName");
            p.lastName   = attr(r, "LastName");
            p.birthYear  = attr(r, "BirthYear").toInt();
            p.role       = attr(r, "Role");
            p.creditedAs = attr(r, "CreditedAs");
            p.voice      = attrTruthy(r, "Voice");
            p.uncredited = attrTruthy(r, "Uncredited");
            p.puppeteer  = attrTruthy(r, "Puppeteer");
            m.actors << p;
        }
        r.skipCurrentElement();
    }
}

void readCredits(QXmlStreamReader& r, Movie& m)
{
    while (r.readNextStartElement()) {
        if (r.name() == u"Credit") {
            Person p;
            p.firstName  = attr(r, "FirstName");
            p.middleName = attr(r, "MiddleName");
            p.lastName   = attr(r, "LastName");
            p.birthYear  = attr(r, "BirthYear").toInt();
            p.creditType = attr(r, "CreditType");
            p.role       = attr(r, "CreditSubtype");
            p.creditedAs = attr(r, "CreditedAs");
            m.credits << p;
        }
        // <Divider> grouping elements inside <Credits> are intentionally skipped
        // — they're UI hints in DP4, not data.
        r.skipCurrentElement();
    }
}

// DVD Profiler flags the primary disc format with a "True"/"true" boolean
// for each media type. The first one that matches wins. The DP4 element name
// (e.g. "UltraHD") is mapped to the app's canonical code ("UHD") so imported
// entries share one vocabulary with hand-entered ones — see MediaFormat.h.
void readMediaTypes(QXmlStreamReader& r, Movie& m)
{
    while (r.readNextStartElement()) {
        const QString name = r.name().toString();
        const QString val  = r.readElementText().trimmed();
        if (m.format.isEmpty() && isTruthy(val)) {
            m.format = canonicalMediaFormat(name);
        }
    }
}

AudioTrack readAudioTrack(QXmlStreamReader& r)
{
    AudioTrack t;
    t.content  = attr(r, "Content");
    t.format   = attr(r, "Format");
    t.channels = attr(r, "Channels");

    while (r.readNextStartElement()) {
        const auto n = r.name();
        if      (n == u"AudioContent"  || n == u"Content")  t.content  = r.readElementText();
        else if (n == u"AudioFormat"   || n == u"Format")   t.format   = r.readElementText();
        else if (n == u"AudioChannels" || n == u"Channels") t.channels = r.readElementText();
        else                                                r.skipCurrentElement();
    }
    return t;
}

void readAudio(QXmlStreamReader& r, Movie& m)
{
    while (r.readNextStartElement()) {
        if (r.name() == u"AudioTrack") {
            m.audioTracks << readAudioTrack(r);
        } else {
            r.skipCurrentElement();
        }
    }
}

Disc readDisc(QXmlStreamReader& r)
{
    Disc d;
    while (r.readNextStartElement()) {
        const auto n = r.name();
        if      (n == u"DescriptionSideA") d.descriptionSideA = r.readElementText();
        else if (n == u"DescriptionSideB") d.descriptionSideB = r.readElementText();
        else if (n == u"DiscIDSideA")      d.discIdSideA      = r.readElementText();
        else if (n == u"DiscIDSideB")      d.discIdSideB      = r.readElementText();
        else if (n == u"LabelSideA")       d.labelSideA       = r.readElementText();
        else if (n == u"LabelSideB")       d.labelSideB       = r.readElementText();
        else if (n == u"DualLayeredSideA") d.dualLayeredSideA = isTruthy(r.readElementText());
        else if (n == u"DualLayeredSideB") d.dualLayeredSideB = isTruthy(r.readElementText());
        else if (n == u"DualSided")        d.dualSided        = isTruthy(r.readElementText());
        else if (n == u"Location")         d.location         = r.readElementText();
        else if (n == u"Slot")             d.slot             = r.readElementText();
        else                               r.skipCurrentElement();
    }
    return d;
}

void readDiscs(QXmlStreamReader& r, Movie& m)
{
    while (r.readNextStartElement()) {
        if (r.name() == u"Disc") m.discs << readDisc(r);
        else                     r.skipCurrentElement();
    }
}

// DP4 BoxSet: <Parent> for child items, <Contents>/<Content> for parents.
void readBoxSet(QXmlStreamReader& r, Movie& m)
{
    while (r.readNextStartElement()) {
        if (r.name() == u"Parent") {
            m.boxSet.parentId = r.readElementText();
        } else if (r.name() == u"Contents") {
            readStringList(r, "Content", m.boxSet.childIds);
        } else {
            r.skipCurrentElement();
        }
    }
    m.boxSet.isParent = !m.boxSet.childIds.isEmpty();
}

CustomField readCustomField(QXmlStreamReader& r)
{
    CustomField c;
    c.name  = attr(r, "Name");
    c.value = attr(r, "Value");
    while (r.readNextStartElement()) {
        if      (r.name() == u"Name")  c.name  = r.readElementText();
        else if (r.name() == u"Value") c.value = r.readElementText();
        else                           r.skipCurrentElement();
    }
    return c;
}

void readCustomFields(QXmlStreamReader& r, Movie& m)
{
    while (r.readNextStartElement()) {
        if (r.name() == u"CustomField") m.customFields << readCustomField(r);
        else                            r.skipCurrentElement();
    }
}

// Read a <Foo DenominationType=".." DenominationDesc=".." FormattedValue="..">value</Foo>
// shaped element into the given MonetaryAmount. Caller must be positioned
// on the opening tag.
void readMonetary(QXmlStreamReader& r, MonetaryAmount& out)
{
    out.denominationType        = attr(r, "DenominationType");
    out.denominationDescription = attr(r, "DenominationDesc");
    out.formattedValue          = attr(r, "FormattedValue");
    out.value                   = r.readElementText();
}

void readGiftFrom(QXmlStreamReader& r, Movie& m)
{
    m.purchase.giftFromFirstName = attr(r, "FirstName");
    m.purchase.giftFromLastName  = attr(r, "LastName");
    r.skipCurrentElement();
}

void readPurchaseInfo(QXmlStreamReader& r, Movie& m)
{
    while (r.readNextStartElement()) {
        const auto n = r.name();
        if      (n == u"PurchasePrice")        readMonetary(r, m.purchase.price);
        else if (n == u"PurchasePlace")        m.purchase.place          = r.readElementText();
        else if (n == u"PurchasePlaceType")    m.purchase.placeType      = r.readElementText();
        else if (n == u"PurchasePlaceWebsite") m.purchase.placeWebsite   = r.readElementText();
        else if (n == u"PurchaseDate")         m.purchase.date           = QDate::fromString(r.readElementText(), Qt::ISODate);
        else if (n == u"ReceivedAsGift")       m.purchase.receivedAsGift = isTruthy(r.readElementText());
        else if (n == u"GiftFrom")             readGiftFrom(r, m);
        else                                   r.skipCurrentElement();
    }
}

// DP4 ColorFormat: 4 mutually-exclusive booleans collapsed into one string.
void readColorFormat(QXmlStreamReader& r, VideoFormat& vf)
{
    while (r.readNextStartElement()) {
        const auto n   = r.name();
        const bool yes = isTruthy(r.readElementText());
        if (yes && vf.colorMode.isEmpty()) {
            if      (n == u"ClrColor")         vf.colorMode = QStringLiteral("Color");
            else if (n == u"ClrBlackAndWhite") vf.colorMode = QStringLiteral("BlackAndWhite");
            else if (n == u"ClrColorized")     vf.colorMode = QStringLiteral("Colorized");
            else if (n == u"ClrMixed")         vf.colorMode = QStringLiteral("Mixed");
        }
    }
}

// DP4 Dimensions: 3 mutually-exclusive booleans collapsed into one string.
void readDimensions(QXmlStreamReader& r, VideoFormat& vf)
{
    while (r.readNextStartElement()) {
        const auto n   = r.name();
        const bool yes = isTruthy(r.readElementText());
        if (yes && vf.dimensions.isEmpty()) {
            if      (n == u"Dim2D")         vf.dimensions = QStringLiteral("2D");
            else if (n == u"Dim3DAnaglyph") vf.dimensions = QStringLiteral("3DAnaglyph");
            else if (n == u"Dim3DBluRay")   vf.dimensions = QStringLiteral("3DBluRay");
        }
    }
}

void readFormat(QXmlStreamReader& r, Movie& m)
{
    while (r.readNextStartElement()) {
        const auto n = r.name();
        if      (n == u"FormatAspectRatio")   m.videoFormat.aspectRatio     = r.readElementText();
        else if (n == u"FormatVideoStandard") m.videoFormat.videoStandard   = r.readElementText();
        else if (n == u"FormatLetterBox")     m.videoFormat.letterBox       = isTruthy(r.readElementText());
        else if (n == u"FormatPanAndScan")    m.videoFormat.panAndScan      = isTruthy(r.readElementText());
        else if (n == u"FormatFullFrame")     m.videoFormat.fullFrame       = isTruthy(r.readElementText());
        else if (n == u"Format16X9")          m.videoFormat.enhancedFor16x9 = isTruthy(r.readElementText());
        else if (n == u"FormatDualSided")     m.videoFormat.dualSided       = isTruthy(r.readElementText());
        else if (n == u"FormatDualLayered")   m.videoFormat.dualLayered     = isTruthy(r.readElementText());
        else if (n == u"ColorFormat")         readColorFormat(r, m.videoFormat);
        else if (n == u"Dimensions")          readDimensions(r, m.videoFormat);
        else                                  r.skipCurrentElement();
    }
}

// Each enabled Feature* boolean becomes an entry in the features list,
// with the "Feature" prefix stripped. <OtherFeatures> is a sibling
// free-text field.
void readFeatures(QXmlStreamReader& r, Movie& m)
{
    while (r.readNextStartElement()) {
        const QString n = r.name().toString();
        if (n == QLatin1String("OtherFeatures")) {
            m.otherFeatures = r.readElementText();
            continue;
        }
        const QString val = r.readElementText().trimmed();
        if (n.startsWith(QLatin1String("Feature")) && isTruthy(val)) {
            m.features << n.mid(int(qstrlen("Feature")));
        }
    }
}

// Each Locks/* boolean is recorded by name when set; the rest are dropped.
void readLocks(QXmlStreamReader& r, Movie& m)
{
    while (r.readNextStartElement()) {
        const QString n   = r.name().toString();
        const QString val = r.readElementText().trimmed();
        if (isTruthy(val)) m.lockedFields << n;
    }
}

void readReview(QXmlStreamReader& r, Movie& m)
{
    m.review.film   = attr(r, "Film").toInt();
    m.review.video  = attr(r, "Video").toInt();
    m.review.audio  = attr(r, "Audio").toInt();
    m.review.extras = attr(r, "Extras").toInt();
    r.skipCurrentElement();
}

void readMediaBanners(QXmlStreamReader& r, Movie& m)
{
    m.mediaBanners.front = attr(r, "Front");
    m.mediaBanners.back  = attr(r, "Back");
    r.skipCurrentElement();
}

void readLoanInfo(QXmlStreamReader& r, Movie& m)
{
    while (r.readNextStartElement()) {
        const auto n = r.name();
        if      (n == u"Loaned") m.loan.loaned = isTruthy(r.readElementText());
        else if (n == u"Due")    m.loan.due    = QDate::fromString(r.readElementText(), Qt::ISODate);
        else if (n == u"User")   {
            const auto u = readUser(r);
            m.loan.userFirstName = u.firstName;
            m.loan.userLastName  = u.lastName;
            m.loan.userEmail     = u.email;
            m.loan.userPhone     = u.phone;
        }
        else r.skipCurrentElement();
    }
}

Event readEvent(QXmlStreamReader& r)
{
    Event e;
    while (r.readNextStartElement()) {
        const auto n = r.name();
        if      (n == u"EventType") e.type      = r.readElementText();
        else if (n == u"Timestamp") e.timestamp = QDateTime::fromString(r.readElementText(), Qt::ISODate);
        else if (n == u"Note")      e.note      = r.readElementText();
        else if (n == u"User")      {
            const auto u = readUser(r);
            e.userFirstName = u.firstName;
            e.userLastName  = u.lastName;
            e.userEmail     = u.email;
            e.userPhone     = u.phone;
        }
        else r.skipCurrentElement();
    }
    return e;
}

void readEvents(QXmlStreamReader& r, Movie& m)
{
    while (r.readNextStartElement()) {
        if (r.name() == u"Event") m.events << readEvent(r);
        else                      r.skipCurrentElement();
    }
}

void readCollectionType(QXmlStreamReader& r, Movie& m)
{
    m.membership.isPartOfOwnedCollection = attrTruthy(r, "IsPartOfOwnedCollection");
    m.membership.type                    = r.readElementText();
}

// ---------------------------------------------------------------------------
// Top-level item reader
// ---------------------------------------------------------------------------

Movie readMovie(QXmlStreamReader& r)
{
    Movie m;
    while (r.readNextStartElement()) {
        const auto name = r.name();
        // ---- Identity ------------------------------------------------------
        if      (name == u"ID")               m.id                              = r.readElementText();
        else if (name == u"ID_Base")          m.idMetadata.base                 = r.readElementText();
        else if (name == u"ID_VariantNum")    m.idMetadata.variantNum           = r.readElementText().toInt();
        else if (name == u"ID_LocalityID")    m.idMetadata.localityId           = r.readElementText().toInt();
        else if (name == u"ID_LocalityDesc")  m.idMetadata.localityDescription  = r.readElementText();
        else if (name == u"ID_Type")          m.idMetadata.type                 = r.readElementText();
        else if (name == u"UPC")              m.upc                             = r.readElementText();
        // ---- Titles --------------------------------------------------------
        else if (name == u"Title")            m.title                           = r.readElementText();
        else if (name == u"OriginalTitle")    m.originalTitle                   = r.readElementText();
        else if (name == u"SortTitle")        m.sortTitle                       = r.readElementText();
        else if (name == u"DistTrait")        m.distTrait                       = r.readElementText();
        // ---- Provenance ----------------------------------------------------
        else if (name == u"ProductionYear")   m.productionYear                  = r.readElementText().toInt();
        else if (name == u"Released")         m.releaseDate                     = QDate::fromString(r.readElementText(), Qt::ISODate);
        else if (name == u"CountryOfOrigin"  ||
                 name == u"CountryOfOrigin2" ||
                 name == u"CountryOfOrigin3") {
            const QString c = r.readElementText();
            if (!c.isEmpty()) m.countriesOfOrigin << c;
        }
        // ---- Classification ------------------------------------------------
        else if (name == u"RatingSystem")     m.rating.system                   = r.readElementText();
        else if (name == u"Rating")           m.rating.value                    = r.readElementText();
        else if (name == u"RatingAge")        m.rating.age                      = r.readElementText().toInt();
        else if (name == u"RatingVariant")    m.rating.variant                  = r.readElementText().toInt();
        else if (name == u"RatingDetails")    m.rating.details                  = r.readElementText();
        else if (name == u"CaseType")         m.caseType                        = r.readElementText();
        else if (name == u"CaseSlipCover")    m.caseSlipCover                   = isTruthy(r.readElementText());
        else if (name == u"Regions")          readRegions(r, m);
        else if (name == u"Genres")           readGenres(r, m);
        else if (name == u"Tags")             readTags(r, m);
        // ---- Runtime / video -----------------------------------------------
        else if (name == u"RunningTime")      m.runningTimeMinutes              = r.readElementText().toInt();
        else if (name == u"Format")           readFormat(r, m);
        else if (name == u"Features")         readFeatures(r, m);
        // ---- Cast / crew / studios -----------------------------------------
        else if (name == u"Actors")           readActors(r, m);
        else if (name == u"Credits")          readCredits(r, m);
        else if (name == u"Studios")          readStudios(r, m);
        else if (name == u"MediaCompanies")   readMediaCos(r, m);
        // ---- Discs / media -------------------------------------------------
        else if (name == u"MediaTypes")       readMediaTypes(r, m);
        else if (name == u"Audio")            readAudio(r, m);
        else if (name == u"Subtitles")        readSubtitles(r, m);
        else if (name == u"Discs")            readDiscs(r, m);
        else if (name == u"BoxSet")           readBoxSet(r, m);
        else if (name == u"MediaBanners")     readMediaBanners(r, m);
        // ---- User-managed --------------------------------------------------
        else if (name == u"CollectionNumber") m.collectionNumber                = r.readElementText().toInt();
        else if (name == u"CollectionType")   readCollectionType(r, m);
        else if (name == u"CountAs")          m.countAs                         = r.readElementText().toInt();
        else if (name == u"WishPriority")     m.wishPriority                    = r.readElementText().toInt();
        else if (name == u"CustomFields")     readCustomFields(r, m);
        else if (name == u"Locks")            readLocks(r, m);
        else if (name == u"MyLinks")          m.myLinks                         = r.readElementText();
        // ---- Descriptions --------------------------------------------------
        else if (name == u"Overview")         m.overview                        = r.readElementText();
        else if (name == u"Notes")            m.notes                           = r.readElementText();
        else if (name == u"EasterEggs")       m.easterEggs                      = r.readElementText();
        // ---- Purchase / SRP / loan / events / review -----------------------
        else if (name == u"PurchaseInfo")     readPurchaseInfo(r, m);
        else if (name == u"SRP")              readMonetary(r, m.srp);
        else if (name == u"LoanInfo")         readLoanInfo(r, m);
        else if (name == u"Events")           readEvents(r, m);
        else if (name == u"Review")           readReview(r, m);
        // ---- Timestamps ----------------------------------------------------
        else if (name == u"ProfileTimestamp") m.profileTimestamp                = QDateTime::fromString(r.readElementText(), Qt::ISODate);
        else if (name == u"LastEdited")       m.lastEdited                      = QDateTime::fromString(r.readElementText(), Qt::ISODate);
        // ---- Storage location ----------------------------------------------
        else if (name == u"LocationID")       m.locationId                      = r.readElementText();
        // ---- Anything else -------------------------------------------------
        else                                  r.skipCurrentElement();
    }
    return m;
}

void resolveCovers(Movie& m, const QString& imagesDir)
{
    if (imagesDir.isEmpty() || m.id.isEmpty()) return;
    const QDir dir(imagesDir);
    const QString front = dir.filePath(m.id + QLatin1String("f.jpg"));
    const QString back  = dir.filePath(m.id + QLatin1String("b.jpg"));
    if (QFileInfo::exists(front)) m.coverFrontPath = front;
    if (QFileInfo::exists(back))  m.coverBackPath  = back;
}

} // namespace

DvdProfilerXmlImporter::DvdProfilerXmlImporter(QString imagesDirectory)
    : m_imagesDir(std::move(imagesDirectory))
{}

Importer::Result
DvdProfilerXmlImporter::importDevice(QIODevice* device)
{
    Result result;
    QXmlStreamReader r(device);

    if (!r.readNextStartElement() || r.name() != u"Collection") {
        result.ok = false;
        result.errorString = QStringLiteral("Root element is not <Collection>");
        return result;
    }

    while (r.readNextStartElement()) {
        if (r.name() == u"DVD") {
            Movie m = readMovie(r);
            resolveCovers(m, m_imagesDir);
            result.movies << std::move(m);
        } else {
            r.skipCurrentElement();
        }
    }

    if (r.hasError()) {
        result.ok = false;
        result.errorString = r.errorString();
    }
    return result;
}

Importer::Result
DvdProfilerXmlImporter::importFile(const QString& path)
{
    QFile f(path);
    // Binary mode is mandatory: QXmlStreamReader honours the <?xml encoding?>
    // declaration (windows-1252 in real DP4 exports) and would corrupt
    // non-ASCII bytes if the file were opened with Text translation.
    if (!f.open(QIODevice::ReadOnly)) {
        Result r;
        r.ok = false;
        r.errorString = QStringLiteral("Cannot open %1: %2")
                            .arg(path, f.errorString());
        return r;
    }
    return importDevice(&f);
}

} // namespace xyz
