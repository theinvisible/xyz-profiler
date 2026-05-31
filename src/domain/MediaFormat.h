#pragma once

#include <QString>

namespace xyz {

// Canonical disc-format vocabulary used across the app: "DVD", "BluRay",
// "UHD", "HDDVD". Two producers feed `Movie::format` and historically used
// different spellings for the same thing: the DP4 importer stores the source
// XML element name from <MediaTypes> (e.g. "UltraHD"), while the add/edit
// dialogs store the canonical codes ("UHD"). This maps any known synonym to
// the canonical code so imported and hand-entered entries agree.
//
// Unknown values pass through unchanged (trimmed) — we never want to lose a
// format we don't recognise. Keep this in sync with the format combos in
// `EditMovieDialog` / `AddTitleDialog` (which use the canonical codes) and the
// display normalisation in `Theme::formatBadge`. The DB migration that rewrites
// legacy rows mirrors the UltraHD→UHD mapping below in SQL.
inline QString canonicalMediaFormat(const QString& raw)
{
    const QString f  = raw.trimmed();
    const QString lo = f.toLower();

    if (lo == QStringLiteral("uhd")       || lo == QStringLiteral("ultrahd")
        || lo == QStringLiteral("ultra hd") || lo == QStringLiteral("4k")
        || lo == QStringLiteral("4k uhd"))
        return QStringLiteral("UHD");

    if (lo == QStringLiteral("bluray") || lo == QStringLiteral("blu-ray")
        || lo == QStringLiteral("blu ray"))
        return QStringLiteral("BluRay");

    if (lo == QStringLiteral("hddvd") || lo == QStringLiteral("hd-dvd")
        || lo == QStringLiteral("hd dvd"))
        return QStringLiteral("HDDVD");

    if (lo == QStringLiteral("dvd"))
        return QStringLiteral("DVD");

    return f;   // unknown format — keep the source label
}

} // namespace xyz
