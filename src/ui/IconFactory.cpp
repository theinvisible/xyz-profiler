#include "ui/IconFactory.h"

#include <QByteArray>
#include <QHash>
#include <QPainter>
#include <QPixmapCache>
#include <QSvgRenderer>

namespace xyz {
namespace IconFactory {
namespace {

// Path data ported verbatim from the design's components.jsx ICONS map,
// plus `play` and `import` for actions the prototype implied.
const QHash<QString, QString>& icons()
{
    static const QHash<QString, QString> kIcons = {
        {QStringLiteral("add"),     QStringLiteral("M12 5v14M5 12h14")},
        {QStringLiteral("edit"),    QStringLiteral("M4 20h4L18.5 9.5a2.1 2.1 0 0 0-3-3L5 17v3z M13.5 6.5l3 3")},
        {QStringLiteral("trash"),   QStringLiteral("M4 7h16M9 7V4h6v3M6 7l1 13h10l1-13")},
        {QStringLiteral("search"),  QStringLiteral("M11 19a8 8 0 1 0 0-16 8 8 0 0 0 0 16zM21 21l-4.3-4.3")},
        {QStringLiteral("list"),    QStringLiteral("M8 6h13M8 12h13M8 18h13M3.5 6h.01M3.5 12h.01M3.5 18h.01")},
        {QStringLiteral("grid"),    QStringLiteral("M4 4h7v7H4zM13 4h7v7h-7zM4 13h7v7H4zM13 13h7v7h-7z")},
        {QStringLiteral("sun"),     QStringLiteral("M12 4V2M12 22v-2M4 12H2M22 12h-2M5.6 5.6 4.2 4.2M19.8 19.8l-1.4-1.4M18.4 5.6l1.4-1.4M4.2 19.8l1.4-1.4M12 8a4 4 0 1 0 0 8 4 4 0 0 0 0-8z")},
        {QStringLiteral("moon"),    QStringLiteral("M21 12.8A9 9 0 1 1 11.2 3a7 7 0 0 0 9.8 9.8z")},
        {QStringLiteral("settings"),QStringLiteral("M12 9a3 3 0 1 0 0 6 3 3 0 0 0 0-6zM19.4 13a1.7 1.7 0 0 0 .3 1.9l.1.1a2 2 0 1 1-2.8 2.8l-.1-.1a1.7 1.7 0 0 0-2.9 1.2V21a2 2 0 1 1-4 0v-.1A1.7 1.7 0 0 0 7 19.4a1.7 1.7 0 0 0-1.9.3l-.1.1a2 2 0 1 1-2.8-2.8l.1-.1a1.7 1.7 0 0 0-1.2-2.9H1a2 2 0 1 1 0-4h.1A1.7 1.7 0 0 0 2.6 7a1.7 1.7 0 0 0-.3-1.9l-.1-.1a2 2 0 1 1 2.8-2.8l.1.1a1.7 1.7 0 0 0 1.9.3H7a1.7 1.7 0 0 0 1-1.5V1a2 2 0 1 1 4 0v.1a1.7 1.7 0 0 0 1 1.5 1.7 1.7 0 0 0 1.9-.3l.1-.1a2 2 0 1 1 2.8 2.8l-.1.1a1.7 1.7 0 0 0-.3 1.9V7a1.7 1.7 0 0 0 1.5 1H21a2 2 0 1 1 0 4h-.1a1.7 1.7 0 0 0-1.5 1z")},
        {QStringLiteral("star"),    QStringLiteral("M12 3.5l2.6 5.3 5.9.9-4.3 4.1 1 5.8L12 17l-5.2 2.7 1-5.8-4.3-4.1 5.9-.9z")},
        {QStringLiteral("disc"),    QStringLiteral("M12 3a9 9 0 1 0 0 18 9 9 0 0 0 0-18zm0 6.5a2.5 2.5 0 1 0 0 5 2.5 2.5 0 0 0 0-5z")},
        {QStringLiteral("film"),    QStringLiteral("M3 4h18v16H3zM7 4v16M17 4v16M3 8h4M17 8h4M3 12h4M17 12h4M3 16h4M17 16h4")},
        {QStringLiteral("chevron"), QStringLiteral("M9 6l6 6-6 6")},
        {QStringLiteral("close"),   QStringLiteral("M6 6l12 12M18 6L6 18")},
        {QStringLiteral("sort"),    QStringLiteral("M7 4v16M7 4L4 7M7 4l3 3M17 20V4M17 20l3-3M17 20l-3-3")},
        {QStringLiteral("shelf"),   QStringLiteral("M3 7h18M3 12h18M3 17h18M6 7v10M18 7v10")},
        {QStringLiteral("user"),    QStringLiteral("M12 12a4 4 0 1 0 0-8 4 4 0 0 0 0 8zM5 21a7 7 0 0 1 14 0")},
        {QStringLiteral("check"),   QStringLiteral("M5 12l5 5 9-10")},
        {QStringLiteral("filter"),  QStringLiteral("M3 5h18l-7 8v6l-4 2v-8z")},
        {QStringLiteral("refresh"), QStringLiteral("M3 12a9 9 0 0 1 15-6.7L21 8M21 3v5h-5M21 12a9 9 0 0 1-15 6.7L3 16M3 21v-5h5")},
        {QStringLiteral("play"),    QStringLiteral("M8 5v14l11-7z")},
        {QStringLiteral("import"),  QStringLiteral("M12 3v11M8 10l4 4 4-4M5 20h14")},
    };
    return kIcons;
}

// Icons that are filled rather than stroked.
bool isFilled(const QString& name)
{
    return name == QStringLiteral("star") || name == QStringLiteral("play");
}

QByteArray buildSvg(const QString& name, const QColor& color, qreal stroke)
{
    const QString d = icons().value(name);
    if (d.isEmpty()) return {};

    const QString col  = color.name(QColor::HexRgb);
    const QString fill = isFilled(name) ? col : QStringLiteral("none");

    // The design splits a single `d` string on "M" into separate <path>s.
    QString paths;
    const QStringList segs = d.split(QChar(u'M'), Qt::SkipEmptyParts);
    for (const QString& seg : segs)
        paths += QStringLiteral("<path d=\"M%1\"/>").arg(seg);

    const QString svg = QStringLiteral(
        "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 24 24\" "
        "fill=\"%1\" stroke=\"%2\" stroke-width=\"%3\" "
        "stroke-linecap=\"round\" stroke-linejoin=\"round\">%4</svg>")
        .arg(fill, col)
        .arg(stroke)
        .arg(paths);

    return svg.toUtf8();
}

} // namespace

QPixmap pixmap(const QString& name, const QColor& color,
               int size, qreal stroke, qreal dpr)
{
    // Rasterising an SVG on every call is wasteful (theme switches, per-frame
    // star/avatar painting). Cache by the full appearance key.
    const QString key = QStringLiteral("xpicon_%1_%2_%3_%4_%5")
        .arg(name, color.name(QColor::HexArgb), QString::number(size),
             QString::number(stroke), QString::number(dpr));
    QPixmap cached;
    if (QPixmapCache::find(key, &cached))
        return cached;

    const QByteArray svg = buildSvg(name, color, stroke);
    if (svg.isEmpty()) return {};

    QSvgRenderer renderer(svg);
    if (!renderer.isValid()) return {};

    QPixmap pm(QSize(size, size) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    renderer.render(&p, QRectF(0, 0, size, size));
    p.end();

    QPixmapCache::insert(key, pm);
    return pm;
}

QIcon icon(const QString& name, const QColor& color, int size, qreal stroke)
{
    QPixmap pm1 = pixmap(name, color, size, stroke, 1.0);
    QPixmap pm2 = pixmap(name, color, size, stroke, 2.0);
    QIcon ic;
    if (!pm1.isNull()) ic.addPixmap(pm1);
    if (!pm2.isNull()) ic.addPixmap(pm2);
    return ic;
}

} // namespace IconFactory
} // namespace xyz
