#pragma once

#include <QString>

namespace xyz {

// Applies a Fusion-based colour scheme to the entire QApplication.
//
// Three modes:
//   "Dark"   — Fusion style with a hand-tuned dark palette + dark stylesheet
//   "Light"  — Fusion style with the default (light) palette, no stylesheet
//   "System" — platform-native style and default palette, no stylesheet
//
// Call once from main() after constructing QApplication, and again whenever
// the user flips the theme in SettingsDialog.
class DarkFusionStyle {
public:
    DarkFusionStyle() = delete;

    static void applyTheme(const QString& themeName);
};

} // namespace xyz
