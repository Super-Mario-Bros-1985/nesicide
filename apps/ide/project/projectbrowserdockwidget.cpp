#include "projectbrowserdockwidget.h"
#include "ui_projectbrowserdockwidget.h"

#include "cnesicideproject.h"
#include "model/cprojectmodel.h"
#include "model/cfiltermodel.h"

#include "cprojecttreecontextmenu.h"
#include "cprojecttreeopenaction.h"

#include <iostream>

//--------------------------------------------------------------------------------------
// Sorting functions
//--------------------------------------------------------------------------------------
static void sortChildrenOf(QTreeWidgetItem* parent, CProjectTreeWidget* widget, CProjectModel* model);

struct SortTopLevelItems
{
   SortTopLevelItems(CProjectTreeWidget* widget, CProjectModel* model)
   {
      tree = widget;
      project = model;
   }

   bool operator()(const QTreeWidgetItem* i1, const QTreeWidgetItem* i2) const
   {
      // Sort order:
      // 1. Filters, sorted by internal filter id
      // 2. Items not in a filter, sorted by name.
      CFilterModel* filters = project->getFilterModel();

      // Get uuids.
      QUuid item1 = tree->getUuidOf(i1);
      QUuid item2 = tree->getUuidOf(i2);

      if (filters->isFilter(item1))
      {
         if (filters->isFilter(item2))
         {
            return filters->compare(item1, item2);
         }
         else
         {
            return true; // item1 > item2
         }
      }
      else if (filters->isFilter(item2))
      {
         return false; // item1 < item2
      }
      else
      {
         return i1->text(0) < i2->text(0);
      }
   }

   CProjectModel* project;
   CProjectTreeWidget* tree;
};

//--------------------------------------------------------------------------------------
// Class methods
//--------------------------------------------------------------------------------------
ProjectBrowserDockWidget::ProjectBrowserDockWidget(CProjectTabWidget* pTarget, QWidget *parent)
   : QDockWidget(parent), ui(new Ui::ProjectBrowserDockWidget), m_pProjectModel(NULL)
{
   ui->setupUi(this);

   m_pTarget = pTarget;

   setProjectModel(NULL);

   ui->projectTreeWidget->setHeaderLabel("Project Items");
   ui->openProjectItems->setHeaderLabel("Open Items");

   // Respond to signals of our children.
   QObject::connect(ui->projectTreeWidget, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)),
                                     this, SLOT(openItemRequested(QTreeWidgetItem*,int)));

   QObject::connect(ui->projectTreeWidget, SIGNAL(customContextMenuRequested(QPoint)),
                                     this, SLOT(treeWidgetContextMenuRequested(QPoint)));

   QObject::connect(ui->openProjectItems, SIGNAL(itemSelectionChanged()), this, SLOT(itemSelectionChanged()));

   // Connect open items to tab widget.
   // Main window intercepts this and closes the tab before we receive the signal.
   QObject::connect( pTarget, SIGNAL(tabAdded(int)),       this, SLOT(itemOpened(int)) );
   QObject::connect( pTarget, SIGNAL(tabRemoved(int)),     this, SLOT(itemClosed(int)) );
   QObject::connect( pTarget, SIGNAL(currentChanged(int)), this, SLOT(itemSelected(int)) );
}

ProjectBrowserDockWidget::~ProjectBrowserDockWidget()
{
   delete ui;
}

void ProjectBrowserDockWidget::layoutChangedEvent()
{
   setProjectModel(m_pProjectModel);
}

void ProjectBrowserDockWidget::enableNavigation()
{

}

void ProjectBrowserDockWidget::disableNavigation()
{

}

void ProjectBrowserDockWidget::setProjectModel(CProjectModel *model)
{
   if (m_pProjectModel)
   {
      QObject::disconnect(m_pProjectModel, SIGNAL(itemAdded(QUuid)), this, SLOT(projectTreeChanged(QUuid)));
      QObject::disconnect(m_pProjectModel, SIGNAL(itemRemoved(QUuid)), this, SLOT(projectTreeChanged(QUuid)));
   }

   m_pProjectModel = model;
   rebuildProjectTree();

   if (model)
   {
      QObject::connect(m_pProjectModel, SIGNAL(itemAdded(QUuid)), this, SLOT(projectTreeChanged(QUuid)));
      QObject::connect(m_pProjectModel, SIGNAL(itemRemoved(QUuid)), this, SLOT(projectTreeChanged(QUuid)));
   }
}

void ProjectBrowserDockWidget::itemOpened(QUuid /*uuid*/)
{
   // Add to list of open items
   //ui->openFilesTreeWidget->addItem(m_pProjectModel, uuid, QUuid());
   rebuildProjectTree();
}

void ProjectBrowserDockWidget::itemClosed(QUuid /*uuid*/)
{
   // Remove from list of open items
   //ui->openFilesTreeWidget->removeItem(uuid);
   rebuildProjectTree();
}

void ProjectBrowserDockWidget::itemOpened(int tabId)
{
   // Add new item to list.
   QTreeWidgetItem* item = new QTreeWidgetItem();
   item->setText(0, m_pTarget->tabText(tabId));

   // If item has a UUID, find and extract it.
   QUuid uuid;
   QWidget* tab = m_pTarget->widget(tabId);
   CDesignerEditorBase* editor = dynamic_cast<CDesignerEditorBase*>(tab);
   if (editor != NULL)
   {
      IProjectTreeViewItem* item = editor->treeLink();
      if (item != NULL)
      {
         uuid = item->uuid();
      }
   }
   // This is stored for later reference in future updates.
   item->setData(0, Qt::UserRole, QVariant(uuid));

   ui->openProjectItems->addTopLevelItem(item);
}

void ProjectBrowserDockWidget::itemSelected(int tabId)
{
   // Backup to prevent infinite looping.
   int oldIndex = ui->openProjectItems->currentIndex().row();
   if (oldIndex != tabId)
      ui->openProjectItems->setCurrentItem(ui->openProjectItems->topLevelItem(tabId));
}

void ProjectBrowserDockWidget::itemClosed(int tabId)
{
   // Remove index tabId from list of open items.
   delete ui->openProjectItems->takeTopLevelItem(tabId);
}

void ProjectBrowserDockWidget::itemSelectionChanged()
{
   int tabIndex = ui->openProjectItems->currentIndex().row();
   m_pTarget->setCurrentIndex(tabIndex);
}

void ProjectBrowserDockWidget::projectTreeChanged(QUuid /*uuid*/)
{
   std::cerr << "REBUILDING" << std::endl;
   rebuildProjectTree();
}


void ProjectBrowserDockWidget::openItemRequested(QTreeWidgetItem *item, int)
{
   openNewProjectItem(ui->projectTreeWidget->getUuidOf(item));
}


void ProjectBrowserDockWidget::treeWidgetContextMenuRequested(QPoint pos)
{
   // Temporarily listen to newly added project items. If this slot is invoked,
   // the project browser opens the item (which must have been
   // created via the context menu just now)
   QObject::connect(m_pProjectModel, SIGNAL(itemAdded(QUuid)), this, SLOT(openNewProjectItem(QUuid)));

   // Invoke context menu.
   QUuid uuid = ui->projectTreeWidget->getUuidAt(pos);
   QPoint screenPos = ui->projectTreeWidget->mapToGlobal(pos);
   CProjectTreeContextMenu contextMenu(this, screenPos, m_pProjectModel);
   m_pProjectModel->visitDataItem(uuid, contextMenu);

   // Remove listening again, so changing projects doesn't open all project items immediately.
   QObject::disconnect(m_pProjectModel, SIGNAL(itemAdded(QUuid)), this, SLOT(openNewProjectItem(QUuid)));
}

void ProjectBrowserDockWidget::openNewProjectItem(QUuid uuid)
{
   int oldOpenItemCount = ui->openProjectItems->topLevelItemCount();
   CProjectTreeOpenAction action(m_pTarget, ui->openProjectItems, m_pProjectModel);
   m_pProjectModel->visitDataItem(uuid, action);

   // If a new item has been opened, select it.
   if (ui->openProjectItems->topLevelItemCount() > oldOpenItemCount)
   {
      this->itemSelected(ui->openProjectItems->topLevelItemCount() - 1);
   }

   // TODO After reworking more views: Translate event.
   //emit openUuidRequest(uuid);
}

void ProjectBrowserDockWidget::buildProjectTree()
{
   // Remove old tree.
   ui->projectTreeWidget->clear();

   // If no tree to construct, nothing more to do.
   if (m_pProjectModel == NULL)
      return;

   // Create a node for each uuid.
   foreach(QUuid uuid, m_pProjectModel->getUuids())
   {
      ui->projectTreeWidget->addItem(m_pProjectModel, uuid, QUuid());
   }

   // Connect child items to CFilter parents.
   CFilterModel* pFilters = m_pProjectModel->getFilterModel();
   foreach(QUuid filterUuid, pFilters->getUuids())
   {
      foreach(QUuid childUuid, pFilters->getFilteredItems(filterUuid))
      {
         ui->projectTreeWidget->setParent(childUuid, filterUuid);
      }
   }

   // Sort top level items: Take em, sort em, insert em in a stew.
   QList<QTreeWidgetItem*> topLevelItems;
   while(ui->projectTreeWidget->topLevelItemCount() > 0)
   {
      topLevelItems.append(ui->projectTreeWidget->takeTopLevelItem(0));
   }

   qSort(topLevelItems.begin(), topLevelItems.end(), SortTopLevelItems(ui->projectTreeWidget, m_pProjectModel));

   ui->projectTreeWidget->addTopLevelItems(topLevelItems);

   // Recursively sort filter contents.
   foreach(QTreeWidgetItem* item, topLevelItems)
   {
      sortChildrenOf(item, ui->projectTreeWidget, m_pProjectModel);
   }
}

static void sortChildrenOf(QTreeWidgetItem* parent, CProjectTreeWidget *widget, CProjectModel *model)
{
   if (parent == NULL)
      return;

   QList<QTreeWidgetItem*> children = parent->takeChildren();
   qSort( children.begin(), children.end(), SortTopLevelItems(widget, model) );
   parent->addChildren(children);
   foreach(QTreeWidgetItem* child, children)
   {
      sortChildrenOf(child, widget, model);
   }
}

void ProjectBrowserDockWidget::rebuildProjectTree()
{
   QList<QUuid> expansionState = this->saveProjectTreeExpansionState();
   buildProjectTree();
   this->restoreProjectTreeExpansionState(expansionState);
}

QList<QUuid> ProjectBrowserDockWidget::saveProjectTreeExpansionState()
{
   QList<QUuid> expandedItems;

   QTreeWidgetItemIterator it(ui->projectTreeWidget);
   while (*it)
   {
      QTreeWidgetItem* item = *it;
      if (item->isExpanded())
      {
         expandedItems.append(ui->projectTreeWidget->getUuidOf(item));
      }
      ++it;
   }

   return expandedItems;
}

void ProjectBrowserDockWidget::restoreProjectTreeExpansionState(const QList<QUuid> &itemsToExpand)
{
   QTreeWidgetItemIterator it(ui->projectTreeWidget);
   while (*it)
   {
      QTreeWidgetItem* item = *it;
      QUuid uuid = ui->projectTreeWidget->getUuidOf(item);
      bool shouldBeExanded = itemsToExpand.indexOf(uuid) >= 0;
      item->setExpanded(shouldBeExanded);
      ++it;
   }
}

