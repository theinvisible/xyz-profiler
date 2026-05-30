#pragma once

#include <QString>

class QPalette;

namespace xyz {

// Applies the XYZ-Profiler look (ported from the Claude Design `theme.css`):
// Fusion base style + a themed QPalette + an application-wide QSS, for the
// Dark / Light / System variants. The accent is the canonical blue and the
// density is "regular" (the project's chosen scope).
class DarkFusionStyle {
public:
    DarkFusionStyle() = delete;

    // Sets the Fusion style, the matching palette/font, marks Theme::setDark,
    // and installs the app-wide stylesheet. "System" follows the OS scheme.
    static void applyTheme(const QString& themeName);

    // The full themed stylesheet for the given variant.
    static QString styleSheet(bool dark);

    static QPalette lightPalette();
    static QPalette darkPalette();
};

} // namespace xyz
