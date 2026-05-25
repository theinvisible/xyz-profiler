#pragma once

#include <functional>

#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QList>
#include <QListWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>
#include <QVBoxLayout>

#include "tmdb/TmdbTypes.h"

namespace xyz {

// Modal dialog that presents TMDb search results and lets the user pick one.
// The caller populates candidates via setCandidates(), triggers poster
// loading via loadPosters(), and reads the chosen ID from selectedTmdbId()
// after the dialog closes with Accepted.
class TmdbMatchDialog : public QDialog {
    Q_OBJECT

public:
    explicit TmdbMatchDialog(QWidget* parent = nullptr);

    /// Populate the candidate list.  @p imageUrlFn is called as
    /// imageUrlFn(posterPath, "w185") to build a full poster URL.
    void setCandidates(const QList<TmdbCandidate>& candidates,
                       const std::function<QString(const QString&, const QString&)>& imageUrlFn);

    /// Show or hide the "Searching TMDb..." indicator.
    void setSearching(bool searching);

    /// Display an error message in red.
    void setError(const QString& error);

    /// Start downloading poster thumbnails.  Call after setCandidates().
    void loadPosters(QNetworkAccessManager* nam);

    /// The TMDb ID the user picked, or 0 if the dialog was cancelled.
    int selectedTmdbId() const { return m_selectedTmdbId; }

private:
    void buildUi();
    QWidget* buildCandidateWidget(const TmdbCandidate& candidate, int index);

    QLabel*          m_searchingLabel = nullptr;
    QLabel*          m_errorLabel     = nullptr;
    QListWidget*     m_listWidget     = nullptr;
    QDialogButtonBox* m_buttonBox     = nullptr;

    int m_selectedTmdbId = 0;

    // Poster labels indexed by position in the list, for async image loading.
    QList<QLabel*>   m_posterLabels;
    // Full poster URLs parallel to m_posterLabels.
    QStringList      m_posterUrls;
};

} // namespace xyz
