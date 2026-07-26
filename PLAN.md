# Feature plan

Candidate features for xyz-profiler, written from the perspective of a
**DVD Profiler 4 user migrating their collection**. This is a backlog, not a
commitment — nothing here is scheduled.

It supersedes the short *Roadmap → Post-MVP* list in `CLAUDE.md`: every item
there (refresh-from-TMDb, loan tracking UI, barcode scanning, multi-user /
sync) reappears below with more context.

Effort tags are rough: **S** ≈ a focused session, **M** ≈ needs a dialog or a
new view, **L** ≈ touches the schema, the importer or several subsystems.

---

## A. Already imported and stored — only the UI was missing — **done**

The DP4 importer and the SQLite schema already carried these fields; they
were written on import and survived an edit round-trip, but nothing in
`src/ui` read them. Planning confirmed the whole block needed **no schema
migration and no repository work** — `insertRow_` / `insertChildren_` already
wrote every column and `loadChildren_` read them all back.

Shipped:

- **A1 — Wishlist / collection status.** Toolbar filter (Owned / Wishlist /
  All, default Owned) via `CollectionFilterProxyModel` on both views, a
  status combo in `EditMovieDialog`, and a split headline count in the
  status bar.
- **A2 — Loan actions and history.** `lendItem` / `returnItem` in
  `domain/LoanOps.h`, `LibraryController::lendMovie` / `returnMovie`, the
  new `LendDialog`, an always-reachable Lend/Take-back action in the detail
  pane, and the `Event` list rendered as a History section.
- **A3 — All four review axes.** Video / audio / extras beside the film
  score in the detail header, three spin boxes in the edit dialog, three
  appended (optional) list columns.
- **A4 — Custom fields.** Displayed in the detail pane and edited through a
  name/value table in the edit dialog.
- **A5 — Purchase price and SRP.** Both editable, both shown, routed through
  the new `displayAmount()`.
- **A6 — Back cover.** Click the header cover (or the caption below it) to
  flip.
- **A7 — Orphaned fields.** `collectionNumber`, `countAs`, `wishPriority`
  and `myLinks` are now shown and editable.

Two claims in the first draft of this document were wrong and are corrected
here for the record:

- **A1 was never data loss.** `DvdProfilerXmlImporter::readCollectionType`
  parses `CollectionType` correctly and the value round-trips through
  SQLite. The three hard-coded `"Owned"` assignments only ever applied to
  *newly created* titles. The real defect was that wishlist entries were
  indistinguishable from owned discs and inflated `movieCount()`.
- **A5 was partly done already.** `populateNotes_` had been showing
  `purchase.price.formattedValue` all along; SRP, editing and the display
  fallback were what was missing.

Not done, deliberately: no wishlist badge in the grid and list delegates.
The filter was judged enough; a badge is a cheap follow-up if titles in the
"All" view turn out to be hard to tell apart.

---

## B. DP4 had it, xyz-profiler does not

### B1. Advanced search / filter builder — **L**

Search today is a single FTS5 free-text query over `title`,
`original_title`, `overview`, `notes`, `easter_eggs`, `actors`, `credits`
and `studios` (see the `movies_fts` definition in `Migrations.cpp`). That is
good full-text search and no substitute for DP4's multi-criteria filters:
*Blu-ray AND FSK 16 AND unwatched AND genre Sci-Fi*. Saved filters were a
core part of how people used DP4.

Probably the single most-missed capability in this document.

### B2. Statistics and reports — **M**

`CalendarWindow` is the only analytical view. Missing: distribution by
genre / format / year / studio / age rating, total runtime, collection
value (A5), most-represented actors. `CalendarBuckets` shows the pattern
worth repeating — pure, unit-tested aggregation separate from the painting.

### B3. Browse by person — **M**

Cast and crew are stored in their own tables and indexed in FTS, and the
detail pane lists them — but they are dead text. Clicking an actor should
filter the library to their titles.

### B4. Export and printing — **M**

There is no export path at all: no CSV, no XML, no HTML, no `QPrinter`
anywhere in the tree. For an application whose reason to exist is that DP4
became a dead end, "your data can always leave again" is a trust feature as
much as a convenience one.

### B5. Backup / restore — **S**

One action that archives the SQLite library plus the `covers/` directory,
and one that restores it. Currently a manual folder copy.

### B6. Bulk edit over a multi-selection — **M**

Set format, location, tags or collection status on many titles at once.
Both views already do extended selection, and `BulkTmdbMatchDialog` +
`LibraryController::applyTmdbMatches` are a working template for
"apply a change to N movies in one worker-thread transaction".

### B7. Location / shelf management — **M**

`locationId` is displayed as a fact in the Overview tab and is otherwise
inert — not editable, not a filter, no list of known locations.

### B8. Duplicate detection — **M**

Real DP4 exports accumulate duplicates and regional variants. `IdMetadata`
already parses the UPC into base / variant / locality / type on import,
which is most of the matching logic.

### B9. Audio track and subtitle filters — **M**

"Everything with a German audio track" is a standard DP4 query.
`audioTracks` and `subtitles` are imported and stored; they need to become
filter criteria (fits inside B1).

### B10. Series and episode lists — **L**

DP4 modelled episodes per disc. `Disc` carries side-A/B descriptions but
there is no episode entity. Worth deciding deliberately: a `Series`
sibling to `Movie` under `MediaItem` is the same extension point the
domain layer was designed for.

---

## C. Where a modern replacement can beat DP4

### C1. Watched status / watchlist / last watched — **M**

In DP4 this only existed if you built it yourself out of custom fields. As
a first-class field it also feeds B1 and B2.

### C2. Refresh from TMDb with field locking — **M**

Already on the roadmap in `CLAUDE.md`. The good part: `lockedFields` is
imported from DP4 and stored. That is exactly the concept needed here —
*update everything except my own rating and my own notes* — so the
migration brings its own configuration with it.

### C3. Streaming availability — **S**

TMDb's watch-provider endpoint: "you own this on disc, and it is also
streaming on X right now."

### C4. Trailers — **S**

TMDb videos endpoint, opened in the system browser.

### C5. Barcode scanning — **L**

On the roadmap. A phone companion feeding EANs is more realistic than
webcam capture, and pairs with the TMDb add-title flow that already exists.

### C6. Multi-user / sync — **L**

On the roadmap, and the largest item here. Worth keeping explicitly out of
scope until the single-user experience is complete.

---

## Suggested order

Block A is done. Next:

1. **B1 (filter builder)** — the largest functional gap against DP4, a
   prerequisite for B9, and the natural consumer of what A1 and A3 just
   added. `CollectionFilterProxyModel` is the place it would grow into.
2. **B4 (export)** — small, and it removes the lock-in objection that made
   DP4 a dead end in the first place.
3. **B2 (statistics)** — A5 made the collection-value figure possible;
   `CalendarBuckets` is the pattern to copy for the aggregation.
