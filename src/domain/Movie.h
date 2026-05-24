#pragma once

#include "domain/AudioTrack.h"
#include "domain/BoxSet.h"
#include "domain/Disc.h"
#include "domain/MediaItem.h"
#include "domain/VideoFormat.h"

#include <QList>
#include <QString>
#include <QStringList>

namespace xyz {

// Banner-art automation settings as DP4 ships them on <MediaBanners>.
// Values seen in real exports: "Automatic", "Off". Kept as plain strings
// so unfamiliar values don't get lost.
struct MediaBanners {
    QString front;
    QString back;
};

// A film/series entry. Adds movie-specific fields on top of MediaItem.
//
// `format` holds the primary disc format ("DVD", "BluRay", "UHD", ...) — the
// first `<MediaTypes>` flag the source export marks as True wins.
//
// `features` carries the set of enabled feature names (the DP4 element
// name without the "Feature" prefix — e.g. "SceneAccess", "Commentary",
// "DigitalCopy"). Disabled features are not stored. `otherFeatures` is the
// free-text DP4 field that lives next to the flag block.
struct Movie : MediaItem {
    int               runningTimeMinutes = 0;
    QString           format;
    VideoFormat       videoFormat;
    QList<AudioTrack> audioTracks;
    QStringList       subtitles;
    QList<Disc>       discs;
    BoxSet            boxSet;
    QStringList       features;
    QString           otherFeatures;
    MediaBanners      mediaBanners;
};

} // namespace xyz
