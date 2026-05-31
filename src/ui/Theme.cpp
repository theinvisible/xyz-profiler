#include "ui/Theme.h"

namespace xyz {
namespace {

bool g_dark = true;

// Pre-computed colour-mix() results from theme.css (blue accent #2563EB).
const Palette kLight = {
    /* accent       */ QColor(0x25, 0x63, 0xEB),
    /* accentFg     */ QColor(0xff, 0xff, 0xff),
    /* accentHover  */ QColor(0x21, 0x57, 0xCF),
    /* bg           */ QColor(0xc5, 0xc8, 0xcf),
    /* win          */ QColor(0xf3, 0xf4, 0xf6),
    /* panel        */ QColor(0xff, 0xff, 0xff),
    /* panel2       */ QColor(0xf7, 0xf8, 0xfa),
    /* panel3       */ QColor(0xee, 0xf0, 0xf3),
    /* border       */ QColor(0xe4, 0xe6, 0xea),
    /* borderStrong */ QColor(0xd3, 0xd6, 0xdc),
    /* text         */ QColor(0x1a, 0x1c, 0x20),
    /* text2        */ QColor(0x65, 0x6a, 0x73),
    /* text3        */ QColor(0x9a, 0xa0, 0xa9),
    /* hover        */ QColor(0xee, 0xf0, 0xf3),
    /* sel          */ QColor(0xe3, 0xeb, 0xfc),
    /* selLine      */ QColor(0xa8, 0xc1, 0xf7),
    /* titlebar     */ QColor(0xe9, 0xeb, 0xee),
    /* inset        */ QColor(0xff, 0xff, 0xff),
};

const Palette kDark = {
    /* accent       */ QColor(0x25, 0x63, 0xEB),
    /* accentFg     */ QColor(0xff, 0xff, 0xff),
    /* accentHover  */ QColor(0x21, 0x57, 0xCF),
    /* bg           */ QColor(0x13, 0x14, 0x18),
    /* win          */ QColor(0x1f, 0x21, 0x26),
    /* panel        */ QColor(0x26, 0x28, 0x2e),
    /* panel2       */ QColor(0x2b, 0x2e, 0x35),
    /* panel3       */ QColor(0x31, 0x34, 0x3c),
    /* border       */ QColor(0x34, 0x37, 0x3f),
    /* borderStrong */ QColor(0x41, 0x45, 0x4e),
    /* text         */ QColor(0xe7, 0xe9, 0xed),
    /* text2        */ QColor(0x9a, 0xa0, 0xab),
    /* text3        */ QColor(0x6c, 0x72, 0x7c),
    /* hover        */ QColor(0x2f, 0x32, 0x3a),
    /* sel          */ QColor(0x24, 0x36, 0x5e),
    /* selLine      */ QColor(0x25, 0x48, 0x95),
    /* titlebar     */ QColor(0x29, 0x2c, 0x32),
    /* inset        */ QColor(0x1c, 0x1e, 0x23),
};

} // namespace

namespace Theme {

bool isDark() { return g_dark; }
void setDark(bool dark) { g_dark = dark; }

const Palette& palette(bool dark) { return dark ? kDark : kLight; }

QColor ageColor(int age)
{
    // xp-age[data-age] — snap to the nearest defined FSK band.
    if (age <= 0)  return QColor(0x4a, 0x9e, 0x54);
    if (age <= 6)  return QColor(0x3b, 0xa0, 0xa0);
    if (age <= 12) return QColor(0xd2, 0xa5, 0x2e);
    if (age <= 16) return QColor(0xdd, 0x7b, 0x2f);
    return QColor(0xcf, 0x40, 0x40);
}

FormatBadge formatBadge(const QString& rawFormat)
{
    const QString f = rawFormat.trimmed();
    const QString lo = f.toLower();

    if (lo.contains(QStringLiteral("uhd")) || lo.contains(QStringLiteral("4k"))
        || lo.contains(QStringLiteral("ultra")))   // "UHD", "UltraHD", "Ultra HD"
        return { QStringLiteral("4K UHD"), QColor(0xb0, 0x7d, 0x18), true };
    if (lo.contains(QStringLiteral("blu")))   // "BluRay", "Blu-ray"
        return { QStringLiteral("Blu-ray"), QColor(0x2f, 0x6f, 0xd0), true };
    if (lo.contains(QStringLiteral("dvd")))
        return { QStringLiteral("DVD"), current().text2, false };

    // Unknown format — keep the source label, neutral styling.
    return { f, current().text2, false };
}

void coverGradientColors(const QString& title, QColor& top, QColor& bottom)
{
    // Deterministic hue from the title; a deep, saturated two-stop gradient
    // reminiscent of the design's placeholder posters.
    uint h = 0;
    for (const QChar c : title) h = h * 31u + c.unicode();
    const int hue = int(h % 360u);
    top    = QColor::fromHsl(hue, 110, 70);   // richer, lighter top
    bottom = QColor::fromHsl(hue, 130, 28);   // dark bottom
}

QColor avatarColor(const QString& name)
{
    const uint first = name.isEmpty() ? 0u : name.at(0).unicode();
    const int hue = int((first * 7u + uint(name.size()) * 31u) % 360u);
    return QColor::fromHsl(hue, int(0.38 * 255), int(0.42 * 255));
}

} // namespace Theme
} // namespace xyz
