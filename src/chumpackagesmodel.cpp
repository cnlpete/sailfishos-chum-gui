#include "chumpackagesmodel.h"
#include "chum.h"

#include <QDebug>

#include <algorithm>

ChumPackagesModel::ChumPackagesModel(QObject *parent)
  : QAbstractListModel{parent}
{
  connect(Chum::instance(), &Chum::packagesChanged, this, &ChumPackagesModel::reset);
}

int ChumPackagesModel::rowCount(const QModelIndex &parent) const {
  return !parent.isValid() ? m_packages.size() : 0;
}

QVariant ChumPackagesModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() >= m_packages.size()) {
    return QVariant{};
  }

  const ChumPackage *p = Chum::instance()->package(m_packages[index.row()]);
  switch (role) {
  case ChumPackage::PackageIdRole:
    return p->id();
  case ChumPackage::PackageIconRole:
    return p->icon();
  case ChumPackage::PackageInstalledRole:
    return p->installed();
  case ChumPackage::PackageInstalledVersionRole:
    return p->installedVersion();
  case ChumPackage::PackageNameRole:
    return p->name();
  case ChumPackage::PackageStarsCountRole:
    return p->starsCount();
  default:
    return QVariant{};
  }
}

QHash<int, QByteArray> ChumPackagesModel::roleNames() const {
  return {
    {ChumPackage::PackageIdRole,       QByteArrayLiteral("packageId")},
    {ChumPackage::PackageIconRole,     QByteArrayLiteral("packageIcon")},
    {ChumPackage::PackageInstalledRole,  QByteArrayLiteral("packageInstalled")},
    {ChumPackage::PackageInstalledVersionRole,  QByteArrayLiteral("packageInstalledVersion")},
    {ChumPackage::PackageNameRole,     QByteArrayLiteral("packageName")},
    {ChumPackage::PackageStarsCountRole, QByteArrayLiteral("packageStarsCount")},
    {ChumPackage::PackageUpdateAvailableRole,  QByteArrayLiteral("packageUpdateAvailable")},
  };
}

void ChumPackagesModel::reset() {
  if (m_postpone_loading) return;
  beginResetModel();

  m_packages.clear();
  QList<ChumPackage*> packages;

  // filter packages
  for (ChumPackage* p: Chum::instance()->packages()) {
    disconnect(p, nullptr, this, nullptr);
    // apply filters, such as category, updatable, search query
    if (m_filter_updates_only && !p->updateAvailable())
      continue;
    if (!m_search.isEmpty()) {
      bool found = true;
      QStringList lines{ p->name(),
            p->summary(),
            p->categories().join(' '),
            p->developerLogin(),
            p->developerName(),
            p->description() };
      QString txt = lines.join('\n').normalized(QString::NormalizationForm_KC).toLower();
      for (QString query: m_search.split(' ', QString::SkipEmptyParts)) {
          query = query.normalized(QString::NormalizationForm_KC).toLower();
          found = found && txt.contains(query);
      }
      if (!found) continue;
    }

    // add to filtered packages and follow package updates
    packages.push_back(p);
    connect(p, &ChumPackage::updated, this, &ChumPackagesModel::updatePackage);
  }

  // sort packages
  std::sort(packages.begin(), packages.end(), [](const ChumPackage *a, const ChumPackage *b){
    return a->name() < b->name();
  });

  // record the result
  for (const ChumPackage* p: packages)
    m_packages.push_back(p->id());

  qDebug() << "Packages in the list:" << m_packages.length() << "Search:" << m_search;

  endResetModel();
}

void ChumPackagesModel::updatePackage(QString packageId, ChumPackage::Role role) {
  // check if update is of interest
  QList<ChumPackage::Role> roles{
    ChumPackage::PackageRefreshRole,
    ChumPackage::PackageIconRole,
    ChumPackage::PackageIdRole,
    ChumPackage::PackageNameRole,
    ChumPackage::PackageStarsCountRole,
    ChumPackage::PackageInstalledRole,
    ChumPackage::PackageInstalledVersionRole,
    ChumPackage::PackageUpdateAvailableRole
  };

  QList<ChumPackage::Role> search_roles{
    ChumPackage::PackageNameRole,
    ChumPackage::PackageSummaryRole,
    ChumPackage::PackageCategoriesRole,
    ChumPackage::PackageDeveloperLoginRole,
    ChumPackage::PackageDeveloperNameRole,
    ChumPackage::PackageDescriptionRole
  };

  QList<ChumPackage::Role> sort_roles{
    ChumPackage::PackageNameRole
  };

  // skip the roles we don't follow and when chum reposirory is refreshed
  if (!roles.contains(role) ||
      (role == ChumPackage::PackageRefreshRole && Chum::instance()->busy()))
    return;

  // call reset if some package was reporting refreshrole
  if (role==ChumPackage::PackageRefreshRole) {
      reset();
      return;
  }

  // check if it can trigger any of the filters
  bool filter_or_order_may_change = false;
  if (!m_search.isEmpty() && search_roles.contains(role))
    filter_or_order_may_change = true;
  if (m_filter_updates_only && role == ChumPackage::PackageUpdateAvailableRole)
    filter_or_order_may_change = true;
  // TODO: other filters

  // check if sorting maybe altered
  if (sort_roles.contains(role))
    filter_or_order_may_change = true;

  if (filter_or_order_may_change) {
      reset();
      return;
  }

  // minor change and invalidate corresponding cell
  int i = m_packages.indexOf(packageId);
  if (i < 0) return; // no such package
  dataChanged(index(i), index(i) ); // just refresh whole row to simplify processing here
}

void ChumPackagesModel::setFilterUpdatesOnly(bool filter) {
  m_filter_updates_only = filter;
  emit filterUpdatesOnlyChanged();
  reset();
}

void ChumPackagesModel::setSearch(QString search) {
  if (search == m_search) return;
  m_search = search;
  emit searchChanged();
  reset();
}
