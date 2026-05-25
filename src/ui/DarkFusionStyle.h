#pragma once

#include <QString>

namespace xyz {

class DarkFusionStyle {
public:
    DarkFusionStyle() = delete;

    static void applyTheme(const QString& themeName);
    static QString darkStyleSheet();
};

} // namespace xyz
