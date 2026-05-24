#pragma once

#include <QString>

namespace xyz {

// Represents a person attached to a media item.
//
// The same struct is reused for both cast and crew/credits:
//   - For an actor: `role` holds the character name; `creditType` is empty;
//     the `voice` / `uncredited` / `puppeteer` flags carry the actor-only
//     DP4 attributes.
//   - For a credit: `creditType` holds the department ("Direction",
//     "Writing", "Music", ...) and `role` holds the specific job
//     ("Director", "Screenplay", "Composer", ...).
//
// `creditedAs` captures the on-screen name when it differs from the
// person's canonical name (e.g. "Paul T. Scheuring" for Paul Scheuring).
//
// Reusing one shape keeps storage and UI code uniform and works equally well
// for non-film media (e.g. game designers, composers, voice cast).
struct Person {
    QString firstName;
    QString middleName;
    QString lastName;
    int     birthYear = 0;
    QString role;
    QString creditType;
    QString creditedAs;
    bool    voice      = false;
    bool    uncredited = false;
    bool    puppeteer  = false;
};

} // namespace xyz
