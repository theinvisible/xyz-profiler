#pragma once

#include <QColor>
#include <QString>

namespace xyz {

// ---------------------------------------------------------------------------
// Theme — design tokens ported 1:1 from the Claude Design `theme.css`.
// ---------------------------------------------------------------------------
// The design medium was HTML/CSS with CSS custom properties; this is the Qt
// translation. `Palette` mirrors the `--*` tokens for one theme. QSS string
// generation (DarkFusionStyle) and the custom-painted widgets (cover grid,
// list rows, detail pane) all read their colours from here so the whole UI
// stays in sync with one source of truth.
//
// Accent is fixed to the canonical blue (#2563EB) and density to "regular",
// per the project's chosen scope.
// ---------------------------------------------------------------------------
struct Palette {
    QColor accent;       // --accent
    QColor accentFg;     // --accent-fg
    QColor accentHover;  // mix(accent 88%, black)
    QColor bg;           // --bg     (desktop behind the window)
    QColor win;          // --win    (window body / collection background)
    QColor panel;        // --panel  (detail pane, dialogs, toolbar)
    QColor panel2;       // --panel-2
    QColor panel3;       // --panel-3 (chips)
    QColor border;       // --border
    QColor borderStrong; // --border-strong
    QColor text;         // --text
    QColor text2;        // --text-2
    QColor text3;        // --text-3
    QColor hover;        // --hover
    QColor sel;          // --sel
    QColor selLine;      // --sel-line
    QColor titlebar;     // --titlebar
    QColor inset;        // --inset  (input fields)
};

namespace Theme {

// Whether the currently-applied theme is the dark variant. Set by
// DarkFusionStyle::applyTheme(); read by every custom painter.
bool        isDark();
void        setDark(bool dark);

const Palette& palette(bool dark);
inline const Palette& current() { return palette(isDark()); }

// --- Accent / fixed design colours -----------------------------------------
constexpr const char* kAccent = "#2563EB";

// Star colours (xp-stars).
inline QColor starOn()  { return QColor(0xe6, 0xa9, 0x3b); }
inline QColor starOff() { return current().borderStrong; }

// Loan accent (xp-loan-dot / banner).
inline QColor loanAccent() { return QColor(0xe0, 0x92, 0x2f); }

// FSK / age-rating badge colour (xp-age[data-age]).
QColor ageColor(int age);

// Format badge palette (xp-fmt-badge). Returns the foreground colour and a
// normalised display label ("4K UHD", "Blu-ray", "DVD", or the raw value).
struct FormatBadge {
    QString label;
    QColor  fg;
    bool    tinted;   // true for UHD/Blu-ray (coloured), false = neutral
};
FormatBadge formatBadge(const QString& rawFormat);

// Deterministic two-stop gradient for a placeholder cover, derived from the
// title so a given film always gets the same colours (mirrors the design's
// hand-picked `cover: [c1, c2]`).
void coverGradientColors(const QString& title, QColor& top, QColor& bottom);

// Avatar background hue (xp-avatar): hsl(h 38% 42%).
QColor avatarColor(const QString& name);

} // namespace Theme
} // namespace xyz
