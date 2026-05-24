#pragma once

#include <QString>

namespace xyz {

// One audio track on a disc, as exported by DVD Profiler's <AudioTrack> nodes.
// All fields are optional — older or sparse exports may only set `content`.
struct AudioTrack {
    QString content;    // language/purpose, e.g. "English", "Director's Commentary"
    QString format;     // codec, e.g. "Dolby Digital", "DTS-HD MA"
    QString channels;   // layout, e.g. "5.1", "2.0", "Atmos"
};

} // namespace xyz
