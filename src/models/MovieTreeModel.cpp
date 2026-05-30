#include "MovieTreeModel.h"

#include <QFont>
#include <QHash>

namespace xyz {

MovieTreeModel::MovieTreeModel(QObject* parent)
    : QAbstractItemModel(parent),
      m_root(new Node)
{}

MovieTreeModel::~MovieTreeModel() { clear(); delete m_root; }

void MovieTreeModel::clear()
{
    for (auto* top : m_root->children) {
        qDeleteAll(top->children);
        delete top;
    }
    m_root->children.clear();
}

void MovieTreeModel::setMovies(const QList<Movie>& movies)
{
    beginResetModel();
    clear();

    QHash<QString, Node*> parentNodes;

    // First pass: create top-level nodes for standalone + parents
    for (const auto& m : movies) {
        if (!m.boxSet.parentId.isEmpty())
            continue;
        auto* node    = new Node;
        node->movie   = m;
        node->parent  = m_root;
        node->row     = m_root->children.size();
        m_root->children.append(node);
        if (m.boxSet.isParent)
            parentNodes.insert(m.id, node);
    }

    // Second pass: attach children to their parents
    for (const auto& m : movies) {
        if (m.boxSet.parentId.isEmpty())
            continue;
        auto it = parentNodes.constFind(m.boxSet.parentId);
        Node* parentNode = (it != parentNodes.constEnd()) ? it.value() : m_root;
        auto* node    = new Node;
        node->movie   = m;
        node->parent  = parentNode;
        node->row     = parentNode->children.size();
        parentNode->children.append(node);
    }

    // Reorder children of each parent to match the parent's childIds order
    for (auto* pNode : parentNodes) {
        if (pNode->movie.boxSet.childIds.isEmpty()) continue;
        const auto& order = pNode->movie.boxSet.childIds;
        QHash<QString, Node*> childById;
        for (auto* c : pNode->children)
            childById.insert(c->movie.id, c);
        QList<Node*> sorted;
        sorted.reserve(pNode->children.size());
        for (const auto& cid : order) {
            if (auto* c = childById.value(cid))
                sorted.append(c);
        }
        // Append any children not in the order list
        for (auto* c : pNode->children) {
            if (!sorted.contains(c))
                sorted.append(c);
        }
        for (int i = 0; i < sorted.size(); ++i)
            sorted[i]->row = i;
        pNode->children = std::move(sorted);
    }

    endResetModel();
}

QModelIndex MovieTreeModel::index(int row, int column, const QModelIndex& parent) const
{
    if (column < 0 || column >= ColumnCount) return {};
    const Node* p = parent.isValid() ? nodeFromIndex(parent) : m_root;
    if (!p || row < 0 || row >= p->children.size()) return {};
    return createIndex(row, column, p->children[row]);
}

QModelIndex MovieTreeModel::parent(const QModelIndex& child) const
{
    if (!child.isValid()) return {};
    auto* node = nodeFromIndex(child);
    if (!node || node->parent == m_root || !node->parent) return {};
    return createIndex(node->parent->row, 0, node->parent);
}

int MovieTreeModel::rowCount(const QModelIndex& parent) const
{
    if (parent.column() > 0) return 0;
    const Node* p = parent.isValid() ? nodeFromIndex(parent) : m_root;
    return p ? p->children.size() : 0;
}

int MovieTreeModel::columnCount(const QModelIndex&) const
{
    return ColumnCount;
}

MovieTreeModel::Node* MovieTreeModel::nodeFromIndex(const QModelIndex& index) const
{
    if (!index.isValid()) return nullptr;
    return static_cast<Node*>(index.internalPointer());
}

QString MovieTreeModel::movieIdAtIndex(const QModelIndex& index) const
{
    if (auto* n = nodeFromIndex(index)) return n->movie.id;
    return {};
}

QString MovieTreeModel::primaryDirector(const Movie& m)
{
    for (const auto& c : m.credits) {
        if (c.creditType.compare(QLatin1String("Direction"), Qt::CaseInsensitive) == 0) {
            QStringList parts;
            if (!c.firstName.isEmpty()) parts << c.firstName;
            if (!c.lastName.isEmpty())  parts << c.lastName;
            return parts.join(QChar(u' '));
        }
    }
    return {};
}

QVariant MovieTreeModel::columnData(const Movie& m, int col, int role)
{
    // Column-independent custom roles for the row delegate.
    switch (role) {
    case CoverPathRole:      return m.coverFrontPath;
    case IsLoanedRole:       return m.loan.loaned;
    case ReviewFilmRole:     return m.review.film;
    case FormatNameRole:     return m.format;
    case IsBoxSetParentRole: return m.boxSet.isParent;
    case AgeRole:            return m.rating.age;
    default: break;
    }

    if (role == Qt::DisplayRole) {
        switch (col) {
        case Title:         return m.title;
        case OriginalTitle: return m.originalTitle;
        case SortTitle:     return m.sortTitle.isEmpty() ? m.title : m.sortTitle;
        case Year:          return m.productionYear > 0 ? QString::number(m.productionYear) : QString();
        case Runtime:       return m.runningTimeMinutes > 0 ? QStringLiteral("%1 min").arg(m.runningTimeMinutes) : QString();
        case Format:        return m.format;
        case Rating:        return m.rating.value;
        case RatingAge:     return m.rating.age > 0 ? QString::number(m.rating.age) : QString();
        case Director:      return primaryDirector(m);
        case Genres:        return m.genres.join(QStringLiteral(", "));
        case Studios:       return m.studios.join(QStringLiteral(", "));
        case CaseType:      return m.caseType;
        case AspectRatio:   return m.videoFormat.aspectRatio;
        case TmdbId:        return m.tmdbId > 0 ? QString::number(m.tmdbId) : QString();
        case PurchaseDate:  return m.purchase.date.isValid() ? m.purchase.date.toString(Qt::ISODate) : QString();
        case Loaned:        return m.loan.loaned ? QStringLiteral("✓") : QString();
        case BoxSetParent:  return m.boxSet.isParent ? QStringLiteral("✓") : QString();
        }
    }
    if (role == Qt::UserRole) {
        switch (col) {
        case Title:         return m.title;
        case OriginalTitle: return m.originalTitle;
        case SortTitle:     return m.sortTitle.isEmpty() ? m.title : m.sortTitle;
        case Year:          return m.productionYear;
        case Runtime:       return m.runningTimeMinutes;
        case Format:        return m.format;
        case Rating:        return m.rating.value;
        case RatingAge:     return m.rating.age;
        case Director:      return primaryDirector(m);
        case Genres:        return m.genres.join(QStringLiteral(", "));
        case Studios:       return m.studios.join(QStringLiteral(", "));
        case CaseType:      return m.caseType;
        case AspectRatio:   return m.videoFormat.aspectRatio;
        case TmdbId:        return m.tmdbId;
        case PurchaseDate:  return m.purchase.date;
        case Loaned:        return m.loan.loaned;
        case BoxSetParent:  return m.boxSet.isParent;
        }
    }
    if (role == Qt::FontRole && m.boxSet.isParent) {
        QFont f;
        f.setBold(true);
        return f;
    }
    if (role == Qt::TextAlignmentRole) {
        switch (col) {
        case Year: case Runtime: case RatingAge: case TmdbId:
            return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
        }
    }
    return {};
}

QVariant MovieTreeModel::data(const QModelIndex& index, int role) const
{
    auto* node = nodeFromIndex(index);
    if (!node) return {};
    return columnData(node->movie, index.column(), role);
}

QVariant MovieTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return {};
    switch (section) {
    case Title:         return tr("Title");
    case OriginalTitle: return tr("Original Title");
    case SortTitle:     return tr("Sort Title");
    case Year:          return tr("Year");
    case Runtime:       return tr("Runtime");
    case Format:        return tr("Format");
    case Rating:        return tr("Rating");
    case RatingAge:     return tr("Age");
    case Director:      return tr("Director");
    case Genres:        return tr("Genres");
    case Studios:       return tr("Studios");
    case CaseType:      return tr("Case");
    case AspectRatio:   return tr("Aspect Ratio");
    case TmdbId:        return tr("TMDb ID");
    case PurchaseDate:  return tr("Purchase Date");
    case Loaned:        return tr("Loaned");
    case BoxSetParent:  return tr("Box Set");
    }
    return {};
}

} // namespace xyz
