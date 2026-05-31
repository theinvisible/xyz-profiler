#include "importers/dvdprofiler/DvdProfilerXmlImporter.h"

#include <QBuffer>
#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QTest>

using xyz::DvdProfilerXmlImporter;
using xyz::Importer;

namespace {

Importer::Result parse(const QByteArray& xml,
                       const QString& imagesDir = {})
{
    QBuffer buf;
    buf.setData(xml);
    buf.open(QIODevice::ReadOnly);
    DvdProfilerXmlImporter importer(imagesDir);
    return importer.importDevice(&buf);
}

} // namespace

class TestDvdProfilerXmlImporter : public QObject {
    Q_OBJECT

private slots:
    void parses_basic_movie();
    void canonicalises_ultrahd_media_type();
    void parses_multiple_movies();
    void ignores_unknown_elements();
    void reports_wrong_root_element();
    void resolves_cover_paths_if_present();

    void parses_audio_tracks_attribute_style();
    void parses_audio_tracks_legacy_child_names();
    void parses_audio_tracks_real_dp4_child_names();
    void parses_subtitles();
    void parses_discs();
    void parses_box_set_parent();
    void parses_box_set_child();
    void parses_tags();
    void parses_custom_fields_attribute_style();
    void parses_custom_fields_child_element_style();
    void parses_purchase_info_with_denomination();
    void parses_gift_purchase();

    // Stage-2 sections — extended DP4 schema coverage.
    void parses_id_metadata();
    void parses_collection_membership();
    void parses_countries_of_origin();
    void parses_rating_info();
    void parses_video_format_with_color_and_dimensions();
    void parses_features_keeps_only_enabled();
    void parses_features_with_other_text();
    void parses_actor_full_attributes();
    void parses_credit_full_attributes();
    void parses_media_companies_and_regions();
    void parses_srp_with_denomination();
    void parses_loan_info_active();
    void parses_events_history();
    void parses_review_attributes();
    void parses_easter_eggs_multiline();
    void parses_media_banners();
    void parses_locks_keeps_only_enabled();
    void parses_profile_timestamps();

    void parses_sample_data_file();
};

void TestDvdProfilerXmlImporter::parses_basic_movie()
{
    const QByteArray xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<Collection>
  <DVD>
    <ID>012345678901</ID>
    <Title>Forrest Gump</Title>
    <OriginalTitle>Forrest Gump</OriginalTitle>
    <ProductionYear>1994</ProductionYear>
    <Released>2001-09-25</Released>
    <RunningTime>142</RunningTime>
    <Overview>Life is like a box of chocolates.</Overview>
    <MediaTypes>
      <DVD>True</DVD>
      <BluRay>False</BluRay>
    </MediaTypes>
    <Genres>
      <Genre>Drama</Genre>
      <Genre>Romance</Genre>
    </Genres>
    <Actors>
      <Actor FirstName="Tom" LastName="Hanks" Role="Forrest Gump" />
      <Actor FirstName="Robin" LastName="Wright" Role="Jenny" />
    </Actors>
    <Credits>
      <Credit FirstName="Robert" LastName="Zemeckis"
              CreditType="Direction" CreditSubtype="Director" />
    </Credits>
    <Studios>
      <Studio>Paramount</Studio>
    </Studios>
    <PurchaseInfo>
      <PurchaseDate>2011-01-15</PurchaseDate>
      <PurchasePrice>9.99</PurchasePrice>
    </PurchaseInfo>
  </DVD>
</Collection>
)";

    const auto res = parse(xml);
    QVERIFY2(res.ok, qPrintable(res.errorString));
    QCOMPARE(res.movies.size(), 1);

    const auto& m = res.movies.first();
    QCOMPARE(m.id, QStringLiteral("012345678901"));
    QCOMPARE(m.title, QStringLiteral("Forrest Gump"));
    QCOMPARE(m.productionYear, 1994);
    QCOMPARE(m.releaseDate, QDate(2001, 9, 25));
    QCOMPARE(m.runningTimeMinutes, 142);
    QCOMPARE(m.format, QStringLiteral("DVD"));
    QCOMPARE(m.overview, QStringLiteral("Life is like a box of chocolates."));
    QCOMPARE(m.genres, QStringList({QStringLiteral("Drama"), QStringLiteral("Romance")}));
    QCOMPARE(m.actors.size(), 2);
    QCOMPARE(m.actors.first().lastName, QStringLiteral("Hanks"));
    QCOMPARE(m.actors.first().role, QStringLiteral("Forrest Gump"));
    QCOMPARE(m.credits.size(), 1);
    QCOMPARE(m.credits.first().creditType, QStringLiteral("Direction"));
    QCOMPARE(m.credits.first().role, QStringLiteral("Director"));
    QCOMPARE(m.studios, QStringList({QStringLiteral("Paramount")}));
    QCOMPARE(m.purchase.date,        QDate(2011, 1, 15));
    QCOMPARE(m.purchase.price.value, QStringLiteral("9.99"));
}

void TestDvdProfilerXmlImporter::canonicalises_ultrahd_media_type()
{
    // DP4 names the 4K media type <UltraHD>; the importer must store the app's
    // canonical "UHD" so imported entries match dialog-created ones.
    const QByteArray xml = R"(<Collection>
  <DVD>
    <ID>9</ID><Title>Dune</Title>
    <MediaTypes>
      <DVD>false</DVD>
      <BluRay>false</BluRay>
      <UltraHD>true</UltraHD>
    </MediaTypes>
  </DVD>
</Collection>
)";
    const auto res = parse(xml);
    QVERIFY2(res.ok, qPrintable(res.errorString));
    QCOMPARE(res.movies.size(), 1);
    QCOMPARE(res.movies.first().format, QStringLiteral("UHD"));
}

void TestDvdProfilerXmlImporter::parses_audio_tracks_real_dp4_child_names()
{
    // Real DP4 exports use Audio*-prefixed child element names. This is
    // the shape the importer must handle for the user's actual library.
    const QByteArray xml = R"(<Collection>
  <DVD>
    <ID>1</ID><Title>T</Title>
    <Audio>
      <AudioTrack>
        <AudioContent>English</AudioContent>
        <AudioFormat>DTS-HD Master Audio</AudioFormat>
        <AudioChannels>5.1</AudioChannels>
      </AudioTrack>
      <AudioTrack>
        <AudioContent>German</AudioContent>
        <AudioFormat>DTS-HD High Resolution</AudioFormat>
        <AudioChannels>7.1</AudioChannels>
      </AudioTrack>
    </Audio>
  </DVD>
</Collection>
)";
    const auto res = parse(xml);
    QVERIFY2(res.ok, qPrintable(res.errorString));
    const auto& tracks = res.movies.first().audioTracks;
    QCOMPARE(tracks.size(), 2);
    QCOMPARE(tracks[0].content,  QStringLiteral("English"));
    QCOMPARE(tracks[0].format,   QStringLiteral("DTS-HD Master Audio"));
    QCOMPARE(tracks[0].channels, QStringLiteral("5.1"));
    QCOMPARE(tracks[1].content,  QStringLiteral("German"));
    QCOMPARE(tracks[1].channels, QStringLiteral("7.1"));
}

void TestDvdProfilerXmlImporter::parses_multiple_movies()
{
    const QByteArray xml = R"(<Collection>
  <DVD><ID>1</ID><Title>A</Title></DVD>
  <DVD><ID>2</ID><Title>B</Title></DVD>
  <DVD><ID>3</ID><Title>C</Title></DVD>
</Collection>
)";
    const auto res = parse(xml);
    QVERIFY2(res.ok, qPrintable(res.errorString));
    QCOMPARE(res.movies.size(), 3);
    QCOMPARE(res.movies[1].title, QStringLiteral("B"));
}

void TestDvdProfilerXmlImporter::ignores_unknown_elements()
{
    const QByteArray xml = R"(<Collection>
  <DVD>
    <ID>1</ID>
    <Title>X</Title>
    <SomeFutureField>ignored</SomeFutureField>
    <NestedUnknown><Child>also ignored</Child></NestedUnknown>
  </DVD>
  <PluginData>nothing to see</PluginData>
</Collection>
)";
    const auto res = parse(xml);
    QVERIFY2(res.ok, qPrintable(res.errorString));
    QCOMPARE(res.movies.size(), 1);
    QCOMPARE(res.movies.first().title, QStringLiteral("X"));
}

void TestDvdProfilerXmlImporter::reports_wrong_root_element()
{
    const QByteArray xml = R"(<NotACollection><DVD/></NotACollection>)";
    const auto res = parse(xml);
    QVERIFY(!res.ok);
    QVERIFY(!res.errorString.isEmpty());
}

void TestDvdProfilerXmlImporter::resolves_cover_paths_if_present()
{
    const QString tmp = QDir::tempPath() + QStringLiteral("/xyz-profiler-test-covers");
    QDir().mkpath(tmp);
    const QString frontPath = tmp + QStringLiteral("/999f.jpg");
    {
        QFile f(frontPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("\xff\xd8\xff", 3);
    }

    const QByteArray xml = R"(<Collection>
  <DVD><ID>999</ID><Title>HasCover</Title></DVD>
  <DVD><ID>000</ID><Title>NoCover</Title></DVD>
</Collection>
)";
    const auto res = parse(xml, tmp);
    QVERIFY2(res.ok, qPrintable(res.errorString));
    QCOMPARE(res.movies.size(), 2);
    QCOMPARE(res.movies[0].coverFrontPath, frontPath);
    QVERIFY(res.movies[0].coverBackPath.isEmpty());
    QVERIFY(res.movies[1].coverFrontPath.isEmpty());

    QFile::remove(frontPath);
    QDir().rmdir(tmp);
}

void TestDvdProfilerXmlImporter::parses_audio_tracks_attribute_style()
{
    const QByteArray xml = R"(<Collection>
  <DVD>
    <ID>1</ID><Title>T</Title>
    <Audio>
      <AudioTrack Content="English" Format="Dolby Digital" Channels="5.1" />
      <AudioTrack Content="Director's Commentary" Format="Dolby Digital" Channels="2.0" />
    </Audio>
  </DVD>
</Collection>
)";
    const auto res = parse(xml);
    QVERIFY2(res.ok, qPrintable(res.errorString));
    const auto& tracks = res.movies.first().audioTracks;
    QCOMPARE(tracks.size(), 2);
    QCOMPARE(tracks[0].content, QStringLiteral("English"));
    QCOMPARE(tracks[0].format, QStringLiteral("Dolby Digital"));
    QCOMPARE(tracks[0].channels, QStringLiteral("5.1"));
    QCOMPARE(tracks[1].content, QStringLiteral("Director's Commentary"));
    QCOMPARE(tracks[1].channels, QStringLiteral("2.0"));
}

void TestDvdProfilerXmlImporter::parses_audio_tracks_legacy_child_names()
{
    // Older / community DP4 schemas use unprefixed names. We accept both
    // so the reader is robust against schema drift.
    const QByteArray xml = R"(<Collection>
  <DVD>
    <ID>1</ID><Title>T</Title>
    <Audio>
      <AudioTrack>
        <Content>German</Content>
        <Format>DTS-HD MA</Format>
        <Channels>7.1</Channels>
      </AudioTrack>
    </Audio>
  </DVD>
</Collection>
)";
    const auto res = parse(xml);
    QVERIFY2(res.ok, qPrintable(res.errorString));
    const auto& tracks = res.movies.first().audioTracks;
    QCOMPARE(tracks.size(), 1);
    QCOMPARE(tracks[0].content,  QStringLiteral("German"));
    QCOMPARE(tracks[0].format,   QStringLiteral("DTS-HD MA"));
    QCOMPARE(tracks[0].channels, QStringLiteral("7.1"));
}

void TestDvdProfilerXmlImporter::parses_subtitles()
{
    const QByteArray xml = R"(<Collection>
  <DVD>
    <ID>1</ID><Title>T</Title>
    <Subtitles>
      <Subtitle>English</Subtitle>
      <Subtitle>German</Subtitle>
      <Subtitle>French</Subtitle>
    </Subtitles>
  </DVD>
</Collection>
)";
    const auto res = parse(xml);
    QVERIFY2(res.ok, qPrintable(res.errorString));
    QCOMPARE(res.movies.first().subtitles,
             QStringList({QStringLiteral("English"),
                          QStringLiteral("German"),
                          QStringLiteral("French")}));
}

void TestDvdProfilerXmlImporter::parses_discs()
{
    // Real DP4 Discs/Disc shape with side-A / side-B detail and a
    // two-disc combo pack.
    const QByteArray xml = R"(<Collection>
  <DVD>
    <ID>1</ID><Title>T</Title>
    <Discs>
      <Disc>
        <DescriptionSideA>Main Feature (Ultra HD Blu-ray)</DescriptionSideA>
        <DescriptionSideB/>
        <DiscIDSideA>15E296D713B68FC9</DiscIDSideA>
        <DiscIDSideB/>
        <LabelSideA>UHD_DEEPWATER_HORIZON</LabelSideA>
        <LabelSideB/>
        <DualLayeredSideA>true</DualLayeredSideA>
        <DualLayeredSideB>false</DualLayeredSideB>
        <DualSided>false</DualSided>
        <Location>Shelf A</Location>
        <Slot>3</Slot>
      </Disc>
      <Disc>
        <DescriptionSideA>Main Feature (Blu-ray)</DescriptionSideA>
        <DiscIDSideA>9E9CFCD4851671C4</DiscIDSideA>
        <LabelSideA>BD_DEEPWATER_HORIZON</LabelSideA>
      </Disc>
    </Discs>
  </DVD>
</Collection>
)";
    const auto res = parse(xml);
    QVERIFY2(res.ok, qPrintable(res.errorString));
    const auto& discs = res.movies.first().discs;
    QCOMPARE(discs.size(), 2);
    QCOMPARE(discs[0].descriptionSideA, QStringLiteral("Main Feature (Ultra HD Blu-ray)"));
    QCOMPARE(discs[0].discIdSideA,      QStringLiteral("15E296D713B68FC9"));
    QCOMPARE(discs[0].labelSideA,       QStringLiteral("UHD_DEEPWATER_HORIZON"));
    QVERIFY (discs[0].dualLayeredSideA);
    QVERIFY (!discs[0].dualLayeredSideB);
    QVERIFY (!discs[0].dualSided);
    QCOMPARE(discs[0].location,         QStringLiteral("Shelf A"));
    QCOMPARE(discs[0].slot,             QStringLiteral("3"));
    QCOMPARE(discs[1].discIdSideA,      QStringLiteral("9E9CFCD4851671C4"));
    QVERIFY (discs[1].discIdSideB.isEmpty());
}

void TestDvdProfilerXmlImporter::parses_box_set_parent()
{
    const QByteArray xml = R"(<Collection>
  <DVD>
    <ID>set-1</ID><Title>Trilogy Box</Title>
    <BoxSet>
      <Parent/>
      <Contents>
        <Content>I71AD42688F19C318.5</Content>
        <Content>I9229D490A1CAD59A.5</Content>
      </Contents>
    </BoxSet>
  </DVD>
</Collection>
)";
    const auto res = parse(xml);
    QVERIFY2(res.ok, qPrintable(res.errorString));
    const auto& bs = res.movies.first().boxSet;
    QVERIFY(bs.isParent);
    QVERIFY(bs.parentId.isEmpty());
    QCOMPARE(bs.childIds, QStringList({QStringLiteral("I71AD42688F19C318.5"),
                                       QStringLiteral("I9229D490A1CAD59A.5")}));
}

void TestDvdProfilerXmlImporter::parses_box_set_child()
{
    // Real DP4 child shape: <Parent> inside <BoxSet>, NOT a top-level <ParentID>.
    const QByteArray xml = R"(<Collection>
  <DVD>
    <ID>child-a</ID><Title>Part One</Title>
    <BoxSet>
      <Parent>4010232048387.5</Parent>
      <Contents/>
    </BoxSet>
  </DVD>
</Collection>
)";
    const auto res = parse(xml);
    QVERIFY2(res.ok, qPrintable(res.errorString));
    const auto& bs = res.movies.first().boxSet;
    QVERIFY(!bs.isParent);
    QCOMPARE(bs.parentId, QStringLiteral("4010232048387.5"));
    QVERIFY(bs.childIds.isEmpty());
}

void TestDvdProfilerXmlImporter::parses_tags()
{
    const QByteArray xml = R"(<Collection>
  <DVD>
    <ID>1</ID><Title>T</Title>
    <Tags>
      <Tag>watched</Tag>
      <Tag>favourite</Tag>
    </Tags>
  </DVD>
</Collection>
)";
    const auto res = parse(xml);
    QVERIFY2(res.ok, qPrintable(res.errorString));
    QCOMPARE(res.movies.first().tags,
             QStringList({QStringLiteral("watched"),
                          QStringLiteral("favourite")}));
}

void TestDvdProfilerXmlImporter::parses_custom_fields_attribute_style()
{
    const QByteArray xml = R"(<Collection>
  <DVD>
    <ID>1</ID><Title>T</Title>
    <CustomFields>
      <CustomField Name="Shelf" Value="A-12" />
      <CustomField Name="Rating" Value="5" />
    </CustomFields>
  </DVD>
</Collection>
)";
    const auto res = parse(xml);
    QVERIFY2(res.ok, qPrintable(res.errorString));
    const auto& cf = res.movies.first().customFields;
    QCOMPARE(cf.size(), 2);
    QCOMPARE(cf[0].name,  QStringLiteral("Shelf"));
    QCOMPARE(cf[0].value, QStringLiteral("A-12"));
    QCOMPARE(cf[1].name,  QStringLiteral("Rating"));
    QCOMPARE(cf[1].value, QStringLiteral("5"));
}

void TestDvdProfilerXmlImporter::parses_custom_fields_child_element_style()
{
    const QByteArray xml = R"(<Collection>
  <DVD>
    <ID>1</ID><Title>T</Title>
    <CustomFields>
      <CustomField>
        <Name>Lending</Name>
        <Value>To: Alice</Value>
      </CustomField>
    </CustomFields>
  </DVD>
</Collection>
)";
    const auto res = parse(xml);
    QVERIFY2(res.ok, qPrintable(res.errorString));
    const auto& cf = res.movies.first().customFields;
    QCOMPARE(cf.size(), 1);
    QCOMPARE(cf[0].name,  QStringLiteral("Lending"));
    QCOMPARE(cf[0].value, QStringLiteral("To: Alice"));
}

void TestDvdProfilerXmlImporter::parses_purchase_info_with_denomination()
{
    // Real DP4 PurchasePrice carries currency code + label + locale-formatted
    // display string as attributes alongside the numeric body.
    const QByteArray xml = R"xml(<Collection>
  <DVD>
    <ID>1</ID><Title>T</Title>
    <PurchaseInfo>
      <PurchasePrice DenominationType="EUR" DenominationDesc="Europe (Euro)" FormattedValue="14,99 EUR">14.99</PurchasePrice>
      <PurchasePlace>Saturn</PurchasePlace>
      <PurchasePlaceType>Store</PurchasePlaceType>
      <PurchasePlaceWebsite>https://saturn.de</PurchasePlaceWebsite>
      <PurchaseDate>2024-03-12</PurchaseDate>
      <ReceivedAsGift>false</ReceivedAsGift>
      <GiftFrom FirstName="" LastName=""/>
    </PurchaseInfo>
  </DVD>
</Collection>
)xml";
    const auto res = parse(xml);
    QVERIFY2(res.ok, qPrintable(res.errorString));
    const auto& p = res.movies.first().purchase;
    QCOMPARE(p.date,                           QDate(2024, 3, 12));
    QCOMPARE(p.price.value,                    QStringLiteral("14.99"));
    QCOMPARE(p.price.denominationType,         QStringLiteral("EUR"));
    QCOMPARE(p.price.denominationDescription,  QStringLiteral("Europe (Euro)"));
    QCOMPARE(p.price.formattedValue,           QStringLiteral("14,99 EUR"));
    QCOMPARE(p.place,                          QStringLiteral("Saturn"));
    QCOMPARE(p.placeType,                      QStringLiteral("Store"));
    QCOMPARE(p.placeWebsite,                   QStringLiteral("https://saturn.de"));
    QVERIFY (!p.receivedAsGift);
}

void TestDvdProfilerXmlImporter::parses_gift_purchase()
{
    const QByteArray xml = R"xml(<Collection>
  <DVD>
    <ID>1</ID><Title>T</Title>
    <PurchaseInfo>
      <PurchasePrice DenominationType="EUR" DenominationDesc="Europe (Euro)" FormattedValue="0,00 EUR">0</PurchasePrice>
      <ReceivedAsGift>true</ReceivedAsGift>
      <GiftFrom FirstName="Alice" LastName="Schmidt"/>
    </PurchaseInfo>
  </DVD>
</Collection>
)xml";
    const auto res = parse(xml);
    QVERIFY2(res.ok, qPrintable(res.errorString));
    const auto& p = res.movies.first().purchase;
    QVERIFY (p.receivedAsGift);
    QCOMPARE(p.giftFromFirstName, QStringLiteral("Alice"));
    QCOMPARE(p.giftFromLastName,  QStringLiteral("Schmidt"));
}

// ---------------------------------------------------------------------------
// Stage-2 tests: extended DP4 schema sections.
// ---------------------------------------------------------------------------

void TestDvdProfilerXmlImporter::parses_id_metadata()
{
    const QByteArray xml = R"xml(<Collection>
  <DVD>
    <ID>4010232039842.5</ID>
    <ID_Base>4010232039842</ID_Base>
    <ID_VariantNum>0</ID_VariantNum>
    <ID_LocalityID>5</ID_LocalityID>
    <ID_LocalityDesc>Germany</ID_LocalityDesc>
    <ID_Type>UPCEAN</ID_Type>
    <UPC>4-010232-039842</UPC>
    <Title>T</Title>
  </DVD>
</Collection>
)xml";
    const auto res = parse(xml);
    QVERIFY2(res.ok, qPrintable(res.errorString));
    const auto& m = res.movies.first();
    QCOMPARE(m.idMetadata.base,                QStringLiteral("4010232039842"));
    QCOMPARE(m.idMetadata.variantNum,          0);
    QCOMPARE(m.idMetadata.localityId,          5);
    QCOMPARE(m.idMetadata.localityDescription, QStringLiteral("Germany"));
    QCOMPARE(m.idMetadata.type,                QStringLiteral("UPCEAN"));
    QCOMPARE(m.upc,                            QStringLiteral("4-010232-039842"));
}

void TestDvdProfilerXmlImporter::parses_collection_membership()
{
    const QByteArray xml = R"xml(<Collection>
  <DVD>
    <ID>1</ID><Title>T</Title>
    <CollectionType IsPartOfOwnedCollection="true">Owned</CollectionType>
    <CollectionNumber>221</CollectionNumber>
    <CountAs>2</CountAs>
    <WishPriority>0</WishPriority>
  </DVD>
</Collection>
)xml";
    const auto res = parse(xml);
    QVERIFY2(res.ok, qPrintable(res.errorString));
    const auto& m = res.movies.first();
    QCOMPARE(m.membership.type,                    QStringLiteral("Owned"));
    QVERIFY (m.membership.isPartOfOwnedCollection);
    QCOMPARE(m.collectionNumber, 221);
    QCOMPARE(m.countAs,          2);
    QCOMPARE(m.wishPriority,     0);
}

void TestDvdProfilerXmlImporter::parses_countries_of_origin()
{
    const QByteArray xml = R"xml(<Collection>
  <DVD>
    <ID>1</ID><Title>T</Title>
    <CountryOfOrigin>United States</CountryOfOrigin>
    <CountryOfOrigin2>Germany</CountryOfOrigin2>
    <CountryOfOrigin3/>
  </DVD>
</Collection>
)xml";
    const auto res = parse(xml);
    QVERIFY2(res.ok, qPrintable(res.errorString));
    QCOMPARE(res.movies.first().countriesOfOrigin,
             QStringList({QStringLiteral("United States"),
                          QStringLiteral("Germany")}));
}

void TestDvdProfilerXmlImporter::parses_rating_info()
{
    const QByteArray xml = R"xml(<Collection>
  <DVD>
    <ID>1</ID><Title>T</Title>
    <RatingSystem>Film</RatingSystem>
    <Rating>R</Rating>
    <RatingAge>17</RatingAge>
    <RatingVariant>0</RatingVariant>
    <RatingDetails>Strong Violence</RatingDetails>
  </DVD>
</Collection>
)xml";
    const auto res = parse(xml);
    QVERIFY2(res.ok, qPrintable(res.errorString));
    const auto& rt = res.movies.first().rating;
    QCOMPARE(rt.system,  QStringLiteral("Film"));
    QCOMPARE(rt.value,   QStringLiteral("R"));
    QCOMPARE(rt.age,     17);
    QCOMPARE(rt.variant, 0);
    QCOMPARE(rt.details, QStringLiteral("Strong Violence"));
}

void TestDvdProfilerXmlImporter::parses_video_format_with_color_and_dimensions()
{
    const QByteArray xml = R"xml(<Collection>
  <DVD>
    <ID>1</ID><Title>T</Title>
    <Format>
      <FormatAspectRatio>2.40</FormatAspectRatio>
      <FormatVideoStandard>NTSC</FormatVideoStandard>
      <ColorFormat>
        <ClrColor>true</ClrColor>
        <ClrBlackAndWhite>false</ClrBlackAndWhite>
        <ClrColorized>false</ClrColorized>
        <ClrMixed>false</ClrMixed>
      </ColorFormat>
      <FormatLetterBox>true</FormatLetterBox>
      <FormatPanAndScan>false</FormatPanAndScan>
      <FormatFullFrame>false</FormatFullFrame>
      <Format16X9>true</Format16X9>
      <FormatDualSided>false</FormatDualSided>
      <FormatDualLayered>true</FormatDualLayered>
      <Dimensions>
        <Dim2D>false</Dim2D>
        <Dim3DAnaglyph>false</Dim3DAnaglyph>
        <Dim3DBluRay>true</Dim3DBluRay>
      </Dimensions>
    </Format>
  </DVD>
</Collection>
)xml";
    const auto res = parse(xml);
    QVERIFY2(res.ok, qPrintable(res.errorString));
    const auto& vf = res.movies.first().videoFormat;
    QCOMPARE(vf.aspectRatio,     QStringLiteral("2.40"));
    QCOMPARE(vf.videoStandard,   QStringLiteral("NTSC"));
    QCOMPARE(vf.colorMode,       QStringLiteral("Color"));
    QCOMPARE(vf.dimensions,      QStringLiteral("3DBluRay"));
    QVERIFY (vf.letterBox);
    QVERIFY (!vf.panAndScan);
    QVERIFY (vf.enhancedFor16x9);
    QVERIFY (vf.dualLayered);
    QVERIFY (!vf.dualSided);
}

void TestDvdProfilerXmlImporter::parses_features_keeps_only_enabled()
{
    const QByteArray xml = R"xml(<Collection>
  <DVD>
    <ID>1</ID><Title>T</Title>
    <Features>
      <FeatureSceneAccess>true</FeatureSceneAccess>
      <FeatureCommentary>false</FeatureCommentary>
      <FeatureTrailer>true</FeatureTrailer>
      <FeatureBDLive>true</FeatureBDLive>
    </Features>
  </DVD>
</Collection>
)xml";
    const auto res = parse(xml);
    QVERIFY2(res.ok, qPrintable(res.errorString));
    QCOMPARE(res.movies.first().features,
             QStringList({QStringLiteral("SceneAccess"),
                          QStringLiteral("Trailer"),
                          QStringLiteral("BDLive")}));
}

void TestDvdProfilerXmlImporter::parses_features_with_other_text()
{
    const QByteArray xml = R"xml(<Collection>
  <DVD>
    <ID>1</ID><Title>T</Title>
    <Features>
      <FeatureSceneAccess>true</FeatureSceneAccess>
      <OtherFeatures>Kurzfilm: SAW</OtherFeatures>
    </Features>
  </DVD>
</Collection>
)xml";
    const auto res = parse(xml);
    QVERIFY2(res.ok, qPrintable(res.errorString));
    QCOMPARE(res.movies.first().features,      QStringList({QStringLiteral("SceneAccess")}));
    QCOMPARE(res.movies.first().otherFeatures, QStringLiteral("Kurzfilm: SAW"));
}

void TestDvdProfilerXmlImporter::parses_actor_full_attributes()
{
    const QByteArray xml = R"xml(<Collection>
  <DVD>
    <ID>1</ID><Title>T</Title>
    <Actors>
      <Actor FirstName="Adrien" MiddleName="" LastName="Brody"
             BirthYear="1973" Role="Travis" CreditedAs=""
             Voice="false" Uncredited="false" Puppeteer="false"/>
      <Actor FirstName="Frank" MiddleName="Oz" LastName="Oznowicz"
             BirthYear="1944" Role="Yoda" CreditedAs="Frank Oz"
             Voice="true" Uncredited="false" Puppeteer="true"/>
    </Actors>
  </DVD>
</Collection>
)xml";
    const auto res = parse(xml);
    QVERIFY2(res.ok, qPrintable(res.errorString));
    const auto& a = res.movies.first().actors;
    QCOMPARE(a.size(), 2);
    QCOMPARE(a[0].firstName,  QStringLiteral("Adrien"));
    QCOMPARE(a[0].birthYear,  1973);
    QVERIFY (!a[0].voice);
    QVERIFY (!a[0].puppeteer);
    QCOMPARE(a[1].middleName, QStringLiteral("Oz"));
    QCOMPARE(a[1].creditedAs, QStringLiteral("Frank Oz"));
    QVERIFY (a[1].voice);
    QVERIFY (a[1].puppeteer);
    QVERIFY (!a[1].uncredited);
}

void TestDvdProfilerXmlImporter::parses_credit_full_attributes()
{
    const QByteArray xml = R"xml(<Collection>
  <DVD>
    <ID>1</ID><Title>T</Title>
    <Credits>
      <Credit FirstName="Paul" MiddleName="" LastName="Scheuring"
              BirthYear="1968" CreditType="Direction" CreditSubtype="Director"
              CreditedAs="Paul T. Scheuring"/>
    </Credits>
  </DVD>
</Collection>
)xml";
    const auto res = parse(xml);
    QVERIFY2(res.ok, qPrintable(res.errorString));
    const auto& c = res.movies.first().credits.first();
    QCOMPARE(c.firstName,  QStringLiteral("Paul"));
    QCOMPARE(c.lastName,   QStringLiteral("Scheuring"));
    QCOMPARE(c.birthYear,  1968);
    QCOMPARE(c.creditType, QStringLiteral("Direction"));
    QCOMPARE(c.role,       QStringLiteral("Director"));
    QCOMPARE(c.creditedAs, QStringLiteral("Paul T. Scheuring"));
}

void TestDvdProfilerXmlImporter::parses_media_companies_and_regions()
{
    const QByteArray xml = R"xml(<Collection>
  <DVD>
    <ID>1</ID><Title>T</Title>
    <Regions>
      <Region>A</Region>
      <Region>B</Region>
    </Regions>
    <MediaCompanies>
      <MediaCompany>Sony Pictures Home Entertainment</MediaCompany>
      <MediaCompany>Magnet Releasing</MediaCompany>
    </MediaCompanies>
  </DVD>
</Collection>
)xml";
    const auto res = parse(xml);
    QVERIFY2(res.ok, qPrintable(res.errorString));
    const auto& m = res.movies.first();
    QCOMPARE(m.regions,
             QStringList({QStringLiteral("A"), QStringLiteral("B")}));
    QCOMPARE(m.mediaCompanies,
             QStringList({QStringLiteral("Sony Pictures Home Entertainment"),
                          QStringLiteral("Magnet Releasing")}));
}

void TestDvdProfilerXmlImporter::parses_srp_with_denomination()
{
    const QByteArray xml = R"xml(<Collection>
  <DVD>
    <ID>1</ID><Title>T</Title>
    <SRP DenominationType="USD" DenominationDesc="United States (Dollar)" FormattedValue="$30,95">30.95</SRP>
  </DVD>
</Collection>
)xml";
    const auto res = parse(xml);
    QVERIFY2(res.ok, qPrintable(res.errorString));
    const auto& srp = res.movies.first().srp;
    QCOMPARE(srp.value,                    QStringLiteral("30.95"));
    QCOMPARE(srp.denominationType,         QStringLiteral("USD"));
    QCOMPARE(srp.denominationDescription,  QStringLiteral("United States (Dollar)"));
    QCOMPARE(srp.formattedValue,           QStringLiteral("$30,95"));
}

void TestDvdProfilerXmlImporter::parses_loan_info_active()
{
    const QByteArray xml = R"xml(<Collection>
  <DVD>
    <ID>1</ID><Title>T</Title>
    <LoanInfo>
      <Loaned>true</Loaned>
      <Due>2025-01-20</Due>
      <User FirstName="David" LastName="Meier" EmailAddress="d@example.com" PhoneNumber="123"/>
    </LoanInfo>
  </DVD>
</Collection>
)xml";
    const auto res = parse(xml);
    QVERIFY2(res.ok, qPrintable(res.errorString));
    const auto& l = res.movies.first().loan;
    QVERIFY (l.loaned);
    QCOMPARE(l.due,             QDate(2025, 1, 20));
    QCOMPARE(l.userFirstName,   QStringLiteral("David"));
    QCOMPARE(l.userLastName,    QStringLiteral("Meier"));
    QCOMPARE(l.userEmail,       QStringLiteral("d@example.com"));
    QCOMPARE(l.userPhone,       QStringLiteral("123"));
}

void TestDvdProfilerXmlImporter::parses_events_history()
{
    const QByteArray xml = R"xml(<Collection>
  <DVD>
    <ID>1</ID><Title>T</Title>
    <Events>
      <Event>
        <EventType>Borrowed</EventType>
        <Timestamp>2025-01-18T19:09:49.213Z</Timestamp>
        <Note>Lent to David</Note>
        <User FirstName="David" LastName="" EmailAddress="" PhoneNumber=""/>
      </Event>
      <Event>
        <EventType>Returned</EventType>
        <Timestamp>2025-01-20T10:00:00.000Z</Timestamp>
        <Note/>
        <User FirstName="David" LastName="" EmailAddress="" PhoneNumber=""/>
      </Event>
    </Events>
  </DVD>
</Collection>
)xml";
    const auto res = parse(xml);
    QVERIFY2(res.ok, qPrintable(res.errorString));
    const auto& events = res.movies.first().events;
    QCOMPARE(events.size(), 2);
    QCOMPARE(events[0].type,          QStringLiteral("Borrowed"));
    QCOMPARE(events[0].note,          QStringLiteral("Lent to David"));
    QCOMPARE(events[0].userFirstName, QStringLiteral("David"));
    QVERIFY (events[0].timestamp.isValid());
    QCOMPARE(events[1].type,          QStringLiteral("Returned"));
}

void TestDvdProfilerXmlImporter::parses_review_attributes()
{
    const QByteArray xml = R"xml(<Collection>
  <DVD>
    <ID>1</ID><Title>T</Title>
    <Review Film="9" Video="8" Audio="7" Extras="5"/>
  </DVD>
</Collection>
)xml";
    const auto res = parse(xml);
    QVERIFY2(res.ok, qPrintable(res.errorString));
    const auto& rv = res.movies.first().review;
    QCOMPARE(rv.film,   9);
    QCOMPARE(rv.video,  8);
    QCOMPARE(rv.audio,  7);
    QCOMPARE(rv.extras, 5);
}

void TestDvdProfilerXmlImporter::parses_easter_eggs_multiline()
{
    const QByteArray xml = R"xml(<Collection>
  <DVD>
    <ID>1</ID><Title>T</Title>
    <EasterEggs>Hidden Featurette:

In the Extras menu, press down past Trailer and hit Enter.</EasterEggs>
  </DVD>
</Collection>
)xml";
    const auto res = parse(xml);
    QVERIFY2(res.ok, qPrintable(res.errorString));
    const auto& e = res.movies.first().easterEggs;
    QVERIFY(e.contains(QStringLiteral("Hidden Featurette")));
    QVERIFY(e.contains(QChar(u'\n')));
}

void TestDvdProfilerXmlImporter::parses_media_banners()
{
    const QByteArray xml = R"xml(<Collection>
  <DVD>
    <ID>1</ID><Title>T</Title>
    <MediaBanners Front="Automatic" Back="Off"/>
  </DVD>
</Collection>
)xml";
    const auto res = parse(xml);
    QVERIFY2(res.ok, qPrintable(res.errorString));
    const auto& mb = res.movies.first().mediaBanners;
    QCOMPARE(mb.front, QStringLiteral("Automatic"));
    QCOMPARE(mb.back,  QStringLiteral("Off"));
}

void TestDvdProfilerXmlImporter::parses_locks_keeps_only_enabled()
{
    const QByteArray xml = R"xml(<Collection>
  <DVD>
    <ID>1</ID><Title>T</Title>
    <Locks>
      <Entire>false</Entire>
      <Covers>true</Covers>
      <SRP>true</SRP>
      <Title>false</Title>
      <Cast>false</Cast>
    </Locks>
  </DVD>
</Collection>
)xml";
    const auto res = parse(xml);
    QVERIFY2(res.ok, qPrintable(res.errorString));
    QCOMPARE(res.movies.first().lockedFields,
             QStringList({QStringLiteral("Covers"), QStringLiteral("SRP")}));
}

void TestDvdProfilerXmlImporter::parses_profile_timestamps()
{
    const QByteArray xml = R"xml(<Collection>
  <DVD>
    <ID>1</ID><Title>T</Title>
    <ProfileTimestamp>2012-07-22T04:38:08.000Z</ProfileTimestamp>
    <LastEdited>2024-09-08T20:37:50.000Z</LastEdited>
  </DVD>
</Collection>
)xml";
    const auto res = parse(xml);
    QVERIFY2(res.ok, qPrintable(res.errorString));
    const auto& m = res.movies.first();
    QVERIFY (m.profileTimestamp.isValid());
    QVERIFY (m.lastEdited.isValid());
    QCOMPARE(m.profileTimestamp.date(), QDate(2012, 7, 22));
    QCOMPARE(m.lastEdited.date(),       QDate(2024, 9, 8));
}

void TestDvdProfilerXmlImporter::parses_sample_data_file()
{
    const QString path = QStringLiteral(TEST_DATA_DIR "/sample_collection.xml");
    DvdProfilerXmlImporter importer;
    const auto res = importer.importFile(path);
    QVERIFY2(res.ok, qPrintable(res.errorString));
    QVERIFY(res.movies.size() >= 2);

    // The Matrix entry in the sample is a box-set parent and carries
    // audio/subtitle/disc detail; assert that the sample exercises
    // the new fields end-to-end.
    const auto& matrix = res.movies.first();
    QVERIFY(!matrix.audioTracks.isEmpty());
    QVERIFY(!matrix.subtitles.isEmpty());
    QVERIFY(!matrix.discs.isEmpty());
    QVERIFY(!matrix.tags.isEmpty());
    QVERIFY(matrix.boxSet.isParent);
}

QTEST_GUILESS_MAIN(TestDvdProfilerXmlImporter)
#include "test_dvdprofiler_xml_importer.moc"
