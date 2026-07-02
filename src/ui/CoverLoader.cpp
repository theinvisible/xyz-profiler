#include "ui/CoverLoader.h"

#include "ui/CoverCache.h"

#include <QCoreApplication>
#include <QImage>
#include <QImageReader>
#include <QPainter>
#include <QPainterPath>
#include <QPixmapCache>
#include <QThread>
#include <QtConcurrent/QtConcurrentRun>

namespace xyz {
namespace {

// Worker-thread decode. Pure function of its arguments — no shared state.
// Scales during decode where the format supports it (JPEG's scaled IDCT via
// QImageReader::setScaledSize; other formats are scaled by the reader after
// decoding), center-crops to exactly targetPx, then bakes the rounded
// corners so painting is a plain drawPixmap.
QImage decodeCover(const QString& path, QSize targetPx, qreal radiusPx)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);

    bool preScaled = false;
    const QSize src = reader.size();  // header probe — no full decode
    if (src.isValid() && !src.isEmpty()) {
        reader.setScaledSize(src.scaled(targetPx, Qt::KeepAspectRatioByExpanding));
        preScaled = true;
    }

    QImage img = reader.read();
    if (img.isNull())
        return {};
    // Fallback for formats that can't report a size up front, and for the
    // rare EXIF rotation that swaps dimensions after the pre-scale.
    if (!preScaled || img.width() < targetPx.width()
                   || img.height() < targetPx.height())
        img = img.scaled(targetPx, Qt::KeepAspectRatioByExpanding,
                         Qt::SmoothTransformation);

    const int dx = (img.width()  - targetPx.width())  / 2;
    const int dy = (img.height() - targetPx.height()) / 2;
    img = img.copy(qMax(0, dx), qMax(0, dy), targetPx.width(), targetPx.height());

    if (radiusPx > 0.0) {
        QImage rounded(img.size(), QImage::Format_ARGB32_Premultiplied);
        rounded.fill(Qt::transparent);
        QPainter p(&rounded);
        p.setRenderHint(QPainter::Antialiasing, true);
        QPainterPath clip;
        clip.addRoundedRect(QRectF(QPointF(), QSizeF(img.size())),
                            radiusPx, radiusPx);
        p.setClipPath(clip);
        p.drawImage(0, 0, img);
        p.end();
        img = rounded;
    }
    return img;
}

} // namespace

CoverLoader* CoverLoader::instance()
{
    static CoverLoader s;
    return &s;
}

CoverLoader::CoverLoader()
{
    // A couple of decode threads keep up with any scroll speed without
    // starving QtConcurrent's global pool (used by import/DB work).
    m_pool.setMaxThreadCount(qBound(1, QThread::idealThreadCount() / 2, 3));

    // Drop pending work at shutdown; active decodes finish in milliseconds
    // and their continuations simply never run once the event loop is gone.
    if (auto* app = QCoreApplication::instance()) {
        connect(app, &QCoreApplication::aboutToQuit, this, [this] {
            m_queue.clear();
            m_pool.waitForDone();
        });
    }
}

QPixmap CoverLoader::pixmap(const QString& path, const QString& variant,
                            QSize logicalSize, qreal dpr, qreal cornerRadius)
{
    if (path.isEmpty() || logicalSize.isEmpty())
        return {};

    const QString fullVariant = QStringLiteral("%1_%2x%3@%4").arg(
        variant, QString::number(logicalSize.width()),
        QString::number(logicalSize.height()), QString::number(dpr));
    const QString key = CoverCache::key(path, fullVariant);

    QPixmap pm;
    if (QPixmapCache::find(key, &pm))
        return pm;
    if (m_failed.contains(key) || m_inFlight.contains(key))
        return {};

    Job job;
    job.path     = path;
    job.variant  = fullVariant;
    job.key      = key;
    job.targetPx = logicalSize * dpr;
    job.dpr      = dpr;
    job.radiusPx = cornerRadius * dpr;
    m_inFlight.insert(key);
    m_queue.append(job);
    pump_();
    return {};
}

void CoverLoader::pump_()
{
    while (m_active < m_pool.maxThreadCount() && !m_queue.isEmpty()) {
        const Job job = m_queue.takeLast();  // LIFO — newest paint first
        ++m_active;
        QtConcurrent::run(&m_pool, decodeCover, job.path, job.targetPx,
                          job.radiusPx)
            .then(this, [this, job](QImage img) { finishJob_(job, img); });
    }
}

void CoverLoader::finishJob_(const Job& job, const QImage& img)
{
    --m_active;
    m_inFlight.remove(job.key);

    // bump() may have run while we were decoding (a re-match replaced the
    // file in place): the bytes we read are stale — drop them, the next
    // paint re-requests under the new generation's key.
    if (CoverCache::key(job.path, job.variant) == job.key) {
        if (img.isNull()) {
            m_failed.insert(job.key);
        } else {
            QPixmap pm = QPixmap::fromImage(img);
            pm.setDevicePixelRatio(job.dpr);
            QPixmapCache::insert(job.key, pm);
            Q_EMIT coverReady(job.path);
        }
    }
    pump_();
}

} // namespace xyz
