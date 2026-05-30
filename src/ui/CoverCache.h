#pragma once

#include <QHash>
#include <QString>

namespace xyz {

// Coherency helper for the path-keyed QPixmapCache entries used by the cover
// painters (CoverGridWidget, MovieRowDelegate) and the detail pane.
//
// Cover files can be replaced in place: re-matching a title on TMDb overwrites
// covers/<id>f.jpg with new artwork while the stored path stays the same. A
// cache keyed only by path would then keep serving the previous poster forever
// — which is exactly why a re-match left the old (English) cover on screen.
//
// Each path carries a "generation" that bump() increments whenever its bytes
// change. The generation is folded into the cache key, so a replaced cover
// lands on a fresh key and is reloaded. Lookups stay a single in-memory hash
// probe — no per-paint filesystem access, which matters because the cover
// directory may sit on a network drive.
//
// GUI-thread only (all cover painting and the poster-download callback run
// there), so the static registry needs no locking.
class CoverCache {
public:
    // Cache key for `path`'s `variant` at the path's current generation.
    // `variant` separates the differently sized pixmaps cached for one file
    // (grid tile vs. list row vs. detail pane).
    static QString key(const QString& path, const QString& variant)
    {
        return QStringLiteral("xpcover|%1|%2|%3")
            .arg(QString::number(generations().value(path, 0)), variant, path);
    }

    // Mark `path`'s artwork as changed so the next key() misses the cache.
    static void bump(const QString& path)
    {
        if (!path.isEmpty())
            ++generations()[path];
    }

private:
    static QHash<QString, int>& generations()
    {
        static QHash<QString, int> g;
        return g;
    }
};

} // namespace xyz
