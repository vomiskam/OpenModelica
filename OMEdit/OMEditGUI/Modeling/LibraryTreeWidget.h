/*
 * This file is part of OpenModelica.
 *
 * Copyright (c) 1998-2014, Open Source Modelica Consortium (OSMC),
 * c/o Linköpings universitet, Department of Computer and Information Science,
 * SE-58183 Linköping, Sweden.
 *
 * All rights reserved.
 *
 * THIS PROGRAM IS PROVIDED UNDER THE TERMS OF GPL VERSION 3 LICENSE OR
 * THIS OSMC PUBLIC LICENSE (OSMC-PL) VERSION 1.2.
 * ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS PROGRAM CONSTITUTES
 * RECIPIENT'S ACCEPTANCE OF THE OSMC PUBLIC LICENSE OR THE GPL VERSION 3,
 * ACCORDING TO RECIPIENTS CHOICE.
 *
 * The OpenModelica software and the Open Source Modelica
 * Consortium (OSMC) Public License (OSMC-PL) are obtained
 * from OSMC, either from the above address,
 * from the URLs: http://www.ida.liu.se/projects/OpenModelica or
 * http://www.openmodelica.org, and in the OpenModelica distribution.
 * GNU version 3 is obtained from: http://www.gnu.org/copyleft/gpl.html.
 *
 * This program is distributed WITHOUT ANY WARRANTY; without
 * even the implied warranty of  MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE, EXCEPT AS EXPRESSLY SET FORTH
 * IN THE BY RECIPIENT SELECTED SUBSIDIARY LICENSE CONDITIONS OF OSMC-PL.
 *
 * See the full OSMC Public License conditions for more details.
 *
 */
/*
 *
 * @author Adeel Asghar <adeel.asghar@liu.se>
 *
 * RCS: $Id$
 *
 */

#ifndef LIBRARYTREEWIDGET_H
#define LIBRARYTREEWIDGET_H

#include "MainWindow.h"
#include "StringHandler.h"

class ItemDelegate : public QItemDelegate
{
  Q_OBJECT
private:
  bool mDrawRichText;
  QPoint mLastTextPos;
  bool mDrawGrid;
  QColor mGridColor;
  QObject *mpParent;
public:
  ItemDelegate(QObject *pParent = 0, bool drawRichText = false, bool drawGrid = false);
  QColor getGridColor() {return mGridColor;}
  void setGridColor(QColor color) {mGridColor = color;}
  QString formatDisplayText(QVariant variant) const;
  void initTextDocument(QTextDocument *pTextDocument, QFont font, int width) const;
  virtual void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
  void drawHover(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
  virtual QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const;
  virtual bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index);
};

class ModelWidget;
class LibraryTreeItem
{
public:
  enum LibraryType {
    InvalidType, /* Used to catch errors */
    Modelica,   /* Used to represent Modelica models. */
    Text,       /* Used to represent text based files. */
    TLM         /* Used to represent TLM files. */
  };
  enum SaveContentsType {
    SaveInOneFile,
    SaveFolderStructure,
    SaveUnspecified
  };
  LibraryTreeItem();
  LibraryTreeItem(LibraryType type, QString text, QString nameStructure, OMCInterface::getClassInformation_res classInformation,
                  QString fileName, bool isSaved, LibraryTreeItem *pParent = 0);
  ~LibraryTreeItem();
  bool isRootItem() {return mIsRootItem;}
  QList<LibraryTreeItem*> getChildren() const {return mChildren;}
  LibraryType getLibraryType() {return mLibraryType;}
  void setLibraryType(LibraryType libraryType) {mLibraryType = libraryType;}
  void setSystemLibrary(bool systemLibrary) {mSystemLibrary = systemLibrary;}
  bool isSystemLibrary() {return mSystemLibrary;}
  void setModelWidget(ModelWidget *pModelWidget) {mpModelWidget = pModelWidget;}
  ModelWidget* getModelWidget() {return mpModelWidget;}
  void setName(QString name) {mName = name;}
  const QString& getName() const {return mName;}
  void setNameStructure(QString nameStructure) {mNameStructure = nameStructure;}
  const QString& getNameStructure() {return mNameStructure;}
  void setClassInformation(OMCInterface::getClassInformation_res classInformation);
  OMCInterface::getClassInformation_res getClassInformation() {return mClassInformation;}
  void setFileName(QString fileName);
  const QString& getFileName() {return mFileName;}
  void setReadOnly(bool readOnly) {mReadOnly = readOnly;}
  bool isReadOnly() {return mReadOnly;}
  void setIsSaved(bool isSaved) {mIsSaved = isSaved;}
  bool isSaved() {return mIsSaved;}
  void setIsProtected(bool isProtected) {mIsProtected = isProtected;}
  bool isProtected() {return mIsProtected;}
  void setIsDocumentationClass(bool documentationClass) {mDocumentationClass = documentationClass;}
  bool isDocumentationClass() {return mDocumentationClass;}
  void setSaveContentsType(LibraryTreeItem::SaveContentsType saveContentsType) {mSaveContentsType = saveContentsType;}
  StringHandler::ModelicaClasses getRestriction() {return StringHandler::getModelicaClassType(mClassInformation.restriction);}
  bool isPartial() {return mClassInformation.partialPrefix;}
  SaveContentsType getSaveContentsType() {return mSaveContentsType;}
  void setToolTip(QString toolTip) {mToolTip = toolTip;}
  void setIcon(QIcon icon) {mIcon = icon;}
  void setPixmap(QPixmap pixmap) {mPixmap = pixmap;}
  QPixmap getPixmap() {return mPixmap;}
  void setDragPixmap(QPixmap dragPixmap) {mDragPixmap = dragPixmap;}
  QPixmap getDragPixmap() {return mDragPixmap;}
  void setClassText(QString classText) {mClassText = classText;}
  QString getClassText() {return mClassText;}
  bool isExpanded() const {return mExpanded;}
  void setExpanded(bool expanded) {mExpanded = expanded;}
  void updateAttributes();
  QIcon getModelicaNodeIcon();
  bool inRange(int lineNumber) {return (lineNumber >= mClassInformation.lineNumberStart) && (lineNumber <= mClassInformation.lineNumberEnd);}
  bool isInPackageOneFile();
  void insertChild(int position, LibraryTreeItem *pLibraryTreeItem);
  LibraryTreeItem* child(int row);
  void removeChildren();
  void removeChild(LibraryTreeItem *pLibraryTreeItem);
  int columnCount() const;
  QVariant data(int column, int role = Qt::DisplayRole) const;
  int row() const;
  LibraryTreeItem* parent();
  LibraryTreeItem* rootParent();
  bool isTopLevel();
  bool isSimulationAllowed();
private:
  bool mIsRootItem;
  LibraryTreeItem *mpParentLibraryTreeItem;
  QList<LibraryTreeItem*> mChildren;
  LibraryType mLibraryType;
  bool mSystemLibrary;
  ModelWidget *mpModelWidget;
  QString mName;
  QString mParentName;
  QString mNameStructure;
  OMCInterface::getClassInformation_res mClassInformation;
  QString mFileName;
  bool mReadOnly;
  bool mIsSaved;
  bool mIsProtected;
  bool mDocumentationClass;
  SaveContentsType mSaveContentsType;
  QString mToolTip;
  QIcon mIcon;
  QPixmap mPixmap;
  QPixmap mDragPixmap;
  QString mClassText;
  bool mExpanded;
};

class LibraryWidget;
class LibraryTreeProxyModel : public QSortFilterProxyModel
{
  Q_OBJECT
public:
  LibraryTreeProxyModel(LibraryWidget *pLibraryWidget);
private:
  LibraryWidget *mpLibraryWidget;
protected:
  virtual bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const;
};

class LibraryTreeModel : public QAbstractItemModel
{
  Q_OBJECT
public:
  LibraryTreeModel(LibraryWidget *pLibraryWidget);
  LibraryTreeItem* getRootLibraryTreeItem() {return mpRootLibraryTreeItem;}
  int columnCount(const QModelIndex &parent = QModelIndex()) const;
  int rowCount(const QModelIndex &parent = QModelIndex()) const;
  QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
  QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
  QModelIndex parent(const QModelIndex & index) const;
  QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const;
  Qt::ItemFlags flags(const QModelIndex &index) const;
  LibraryTreeItem* findLibraryTreeItem(const QString &name, LibraryTreeItem *root = 0) const;
  LibraryTreeItem* findLibraryTreeItem(const QRegExp &regExp, LibraryTreeItem *root = 0) const;
  QModelIndex libraryTreeItemIndex(const LibraryTreeItem *pLibraryTreeItem) const;
  QModelIndex libraryTreeItemIndexHelper(const LibraryTreeItem *pLibraryTreeItem, const LibraryTreeItem *pParentLibraryTreeItem,
                                           const QModelIndex &parentIndex) const;
  void addModelicaLibraries(QSplashScreen *pSplashScreen);
  void createLibraryTreeItems(LibraryTreeItem *pLibraryTreeItem);
  LibraryTreeItem* createLibraryTreeItem(QString name, LibraryTreeItem *pParentLibraryTreeItem, bool isSaved = true);
  LibraryTreeItem* createLibraryTreeItem(LibraryTreeItem::LibraryType type, QString name, bool isSaved);
  void readLibraryTreeItemClassText(LibraryTreeItem *pLibraryTreeItem);
  QString readLibraryTreeItemClassTextFromText(LibraryTreeItem *pLibraryTreeItem, QString contents);
  QString readLibraryTreeItemClassTextFromFile(LibraryTreeItem *pLibraryTreeItem);
  void updateLibraryTreeItemClassText(LibraryTreeItem *pLibraryTreeItem);
  void updateChildLibraryTreeItemClassText(LibraryTreeItem *pLibraryTreeItem, QString contents, QString fileName);
  LibraryTreeItem* getContainingParentLibraryTreeItem(LibraryTreeItem *pLibraryTreeItem);
  void loadLibraryTreeItemPixmap(LibraryTreeItem *pLibraryTreeItem);
  void loadDependentLibraries(QStringList libraries);
  LibraryTreeItem* getLibraryTreeItemFromFile(QString fileName, int lineNumber);
  void showModelWidget(LibraryTreeItem *pLibraryTreeItem, QString text = QString(), bool show = true, bool newModel = false);
  void showHideProtectedClasses();
  bool unloadClass(LibraryTreeItem *pLibraryTreeItem, bool askQuestion = true);
  void unloadClassChildren(LibraryTreeItem *pParentLibraryTreeItem);
  bool unloadTextFile(LibraryTreeItem *pLibraryTreeItem, bool askQuestion = true);
  bool unloadTLMFile(LibraryTreeItem *pLibraryTreeItem, bool askQuestion = true);
  QString getUniqueTopLevelItemName(QString name, int number = 1);
private:
  LibraryWidget *mpLibraryWidget;
  LibraryTreeItem *mpRootLibraryTreeItem;
  LibraryTreeItem* getLibraryTreeItemFromFileHelper(LibraryTreeItem *pLibraryTreeItem, QString fileName, int lineNumber);
  void unloadClassHelper(LibraryTreeItem *pLibraryTreeItem, LibraryTreeItem *pParentLibraryTreeItem);
protected:
  Qt::DropActions supportedDropActions() const;
};

class LibraryTreeView : public QTreeView
{
  Q_OBJECT
public:
  LibraryTreeView(LibraryWidget *pLibraryWidget);
  LibraryWidget* getLibraryWidget() {return mpLibraryWidget;}
private:
  LibraryWidget *mpLibraryWidget;
  QAction *mpViewClassAction;
  QAction *mpViewDocumentationAction;
  QAction *mpNewModelicaClassAction;
  QAction *mpInstantiateModelAction;
  QAction *mpCheckModelAction;
  QAction *mpCheckAllModelsAction;
  QAction *mpSimulateAction;
  QAction *mpSimulateWithTransformationalDebuggerAction;
  QAction *mpSimulateWithAlgorithmicDebuggerAction;
  QAction *mpSimulationSetupAction;
  QAction *mpDuplicateClassAction;
  QAction *mpUnloadClassAction;
  QAction *mpUnloadTextFileAction;
  QAction *mpUnloadTLMFileAction;
  QAction *mpRefreshAction;
  QAction *mpExportFMUAction;
  QAction *mpExportXMLAction;
  QAction *mpExportFigaroAction;
  QAction *mpFetchInterfaceDataAction;
  QAction *mpTLMCoSimulationAction;
  void createActions();
  LibraryTreeItem* getSelectedLibraryTreeItem();
  void libraryTreeItemExpanded(LibraryTreeItem* pLibraryTreeItem);
public slots:
  void libraryTreeItemExpanded(QModelIndex index);
  void showContextMenu(QPoint point);
  void viewClass();
  void viewDocumentation();
  void createNewModelicaClass();
  void instantiateModel();
  void checkModel();
  void checkAllModels();
  void simulate();
  void simulateWithTransformationalDebugger();
  void simulateWithAlgorithmicDebugger();
  void simulationSetup();
  void duplicateClass();
  void unloadClass();
  void unloadTextFile();
  void unloadTLMFile();
  void exportModelFMU();
  void exportModelXML();
  void exportModelFigaro();
  void fetchInterfaceData();
  void TLMSimulate();
protected:
  virtual void mouseDoubleClickEvent(QMouseEvent *event);
  virtual void startDrag(Qt::DropActions supportedActions);
};

class MainWindow;
class LibraryWidget : public QWidget
{
  Q_OBJECT
public:
  LibraryWidget(MainWindow *pMainWindow);
  MainWindow* getMainWindow() {return mpMainWindow;}
  LibraryTreeModel* getLibraryTreeModel() {return mpLibraryTreeModel;}
  LibraryTreeProxyModel* getLibraryTreeProxyModel() {return mpLibraryTreeProxyModel;}
  LibraryTreeView* getLibraryTreeView() {return mpLibraryTreeView;}
  void openFile(QString fileName, QString encoding = Helper::utf8, bool showProgress = true, bool checkFileExists = false);
  void openModelicaFile(QString fileName, QString encoding = Helper::utf8, bool showProgress = true);
  void openTLMFile(QFileInfo fileInfo, bool showProgress = true);
  void parseAndLoadModelicaText(QString modelText);
  bool saveLibraryTreeItem(LibraryTreeItem *pLibraryTreeItem);
  void openLibraryTreeItem(QString nameStructure);
private:
  MainWindow *mpMainWindow;
  TreeSearchFilters *mpTreeSearchFilters;
  LibraryTreeModel *mpLibraryTreeModel;
  LibraryTreeProxyModel *mpLibraryTreeProxyModel;
  LibraryTreeView *mpLibraryTreeView;
  bool saveModelicaLibraryTreeItem(LibraryTreeItem *pLibraryTreeItem);
  bool saveTextLibraryTreeItem(LibraryTreeItem *pLibraryTreeItem);
  bool saveTLMLibraryTreeItem(LibraryTreeItem *pLibraryTreeItem);
  bool saveLibraryTreeItemHelper(LibraryTreeItem *pLibraryTreeItem);
  bool saveLibraryTreeItemOneFileHelper(LibraryTreeItem *pLibraryTreeItem);
  bool setSubModelsFileNameOneFileHelper(LibraryTreeItem *pLibraryTreeItem, QString filePath);
  void setSubModelsSavedOneFileHelper(LibraryTreeItem *pLibraryTreeItem);
  bool saveLibraryTreeItemFolderHelper(LibraryTreeItem *pLibraryTreeItem);
  bool saveSubModelsFolderHelper(LibraryTreeItem *pLibraryTreeItem, QString directoryName);
  bool saveLibraryTreeItemOneFileOrFolderHelper(LibraryTreeItem *pLibraryTreeItem);
public slots:
  void searchClasses();
};

#endif // LIBRARYTREEWIDGET_H
