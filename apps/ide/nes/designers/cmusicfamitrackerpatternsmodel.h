#ifndef CMUSICFAMITRACKERPATTERNSMODEL_H
#define CMUSICFAMITRACKERPATTERNSMODEL_H

#include <QAbstractTableModel>

#include "famitracker/FamiTrackerDoc.h"

class CMusicFamiTrackerPatternsModel : public QAbstractTableModel
{
   Q_OBJECT
public:
   CMusicFamiTrackerPatternsModel(CFamiTrackerDoc* pDoc,QObject* parent = 0);
   virtual ~CMusicFamiTrackerPatternsModel();
   QVariant data(const QModelIndex& index, int role) const;
   Qt::ItemFlags flags(const QModelIndex& index) const;
   bool setData ( const QModelIndex& index, const QVariant& value, int );
   QVariant headerData(int section, Qt::Orientation orientation,
                       int role = Qt::DisplayRole) const;
   QModelIndex index(int row, int column,
                     const QModelIndex& parent = QModelIndex()) const;
   int rowCount(const QModelIndex& parent = QModelIndex()) const;
   int columnCount(const QModelIndex& parent = QModelIndex()) const;

public slots:
   void update(void);

private:
   CFamiTrackerDoc* m_pDoc;
};

#endif // CMUSICFAMITRACKERPATTERNSMODEL_H
