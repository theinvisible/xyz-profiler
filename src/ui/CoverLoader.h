#pragma once

#include <QObject>
#include <QPixmap>
#include <QSet>
#include <QSize>
#include <QString>
#include <QThreadPool>
#include <QVector>

namespace xyz {

// ---------------------------------------------------------------------------
// CoverLoader — asynchronous, cache-filling decoder for cover artwork.
// ---------------------------------------------------------------------------
// The view delegates used to decode covers synchronously inside paint(): a
// QPixmapCache miss meant a full JPEG decode plus smooth downscale on the GUI
// thread, per tile, mid-scroll — the stutter when flinging through hundreds
// of movies. pixmap() instead returns only what is already cached and hands
// misses to a small worker pool. QImageReader::setScaledSize decodes JPEGs
// near target resolution (libjpeg's scaled IDCT), the result is
// center-cropped, its corner radius baked in (so the delegates don't need an
// antialiased clip path per tile per frame), and inserted into QPixmapCache
// back on the GUI thread. coverReady(path) then lets the views repaint —
// a plain cache hit by that point.
//
// Coherency and bookkeeping:
// - Keys go through CoverCache::key() (generation-aware). The key captured at
//   enqueue time is re-derived on completion; if CoverCache::bump() ran in
//   between (re-match replaced the file in place), the stale decode is
//   dropped and the next paint re-requests.
// - The queue is drained LIFO so the most recently painted (= still visible)
//   tiles decode first when the user out-scrolls the worker pool.
// - Paths that fail to decode are remembered per generation and not retried,
//   so a broken file doesn't re-enqueue on every paint.
// - GUI-thread-only API (same contract as CoverCache); the workers run a pure
//   decode function and touch no shared state.
// ---------------------------------------------------------------------------
class CoverLoader : public QObject {
    Q_OBJECT

public:
    static CoverLoader* instance();

    // Cached pixmap for `path`, scaled-to-fill and center-cropped to
    // logicalSize * dpr with `cornerRadius` (logical px) baked in — or a null
    // pixmap after scheduling the decode. `variant` separates consumers
    // ("grid", "row", ...); size and dpr are folded into the cache key, so a
    // monitor/theme DPI change re-renders instead of serving the wrong scale.
    QPixmap pixmap(const QString& path, const QString& variant,
                   QSize logicalSize, qreal dpr, qreal cornerRadius);

Q_SIGNALS:
    // A decode finished and its pixmap is now in QPixmapCache. Views repaint
    // their viewport on this; Qt coalesces the update()s per frame.
    void coverReady(const QString& path);

private:
    explicit CoverLoader();

    struct Job {
        QString path;
        QString variant;    // full variant (consumer + size + dpr)
        QString key;        // CoverCache::key at enqueue time
        QSize   targetPx;   // device pixels
        qreal   dpr = 1.0;
        qreal   radiusPx = 0.0;
    };

    void pump_();
    void finishJob_(const Job& job, const QImage& img);

    QThreadPool  m_pool;
    QVector<Job> m_queue;     // pending, drained from the back (LIFO)
    QSet<QString> m_inFlight; // keys queued or currently decoding
    QSet<QString> m_failed;   // keys whose decode failed — don't retry
    int m_active = 0;
};

} // namespace xyz
