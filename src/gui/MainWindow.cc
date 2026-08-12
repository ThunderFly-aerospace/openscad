/*
 *  OpenSCAD (www.openscad.org)
 *  Copyright (C) 2009-2011 Clifford Wolf <clifford@clifford.at> and
 *                          Marius Kintel <marius@kintel.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  As a special exception, you have permission to link this program
 *  with the CGAL library and distribute executables, as long as you
 *  follow the requirements of the GNU GPL in regard to all of the
 *  software in the executable aside from CGAL.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "gui/MainWindow.h"

#include <sys/stat.h>

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QDockWidget>
#include <QDropEvent>
#include <QElapsedTimer>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QList>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMetaObject>
#include <QMimeData>
#include <QMutexLocker>
#include <QPainterPath>
#include <QPoint>
#include <QProcess>
#include <QProgressDialog>
#include <QScreen>
#include <QSettings>  //Include QSettings for direct operations on settings arrays
#include <QSignalMapper>
#include <QSoundEffect>
#include <QSplitter>
#include <QStatusBar>
#include <QStringList>
#include <QTemporaryFile>
#include <QTextEdit>
#include <QTextStream>
#include <QTime>
#include <QTimer>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>
#include <QByteArray>
#include <QDataStream>
#include <QDebug>
#include <QString>
#include <QRegularExpression>
#include <set>
#include <algorithm>
#include <boost/range/adaptor/reversed.hpp>
#include <boost/version.hpp>
#include <cassert>
#include <cstring>
#include <deque>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "openscad_gui.h"
#include "core/AST.h"
#include "core/BuiltinContext.h"
#include "core/Builtins.h"
#include "core/CSGNode.h"
#include "core/Context.h"
#include "core/EvaluationSession.h"
#include "core/Expression.h"
#include "core/RenderVariables.h"
#include "core/ScopeContext.h"
#include "core/Settings.h"
#include "core/SourceFileCache.h"
#include "core/customizer/CommentParser.h"
#include "core/node.h"
#include "core/parsersettings.h"
#include "core/progress.h"
#include "geometry/Geometry.h"
#include "geometry/GeometryCache.h"
#include "geometry/GeometryEvaluator.h"
#include "geometry/PolySet.h"
#include "geometry/PolySetUtils.h"
#include "glview/PolySetRenderer.h"
#include "glview/RenderSettings.h"
#if not defined(USE_POLYSET_FOR_CGAL)
#include "glview/cgal/CGALRenderer.h"
#endif
#include "glview/preview/CSGTreeNormalizer.h"
#include "glview/preview/ThrownTogetherRenderer.h"
#include "gui/AboutDialog.h"
#include "gui/CGALWorker.h"
#include "gui/ColorList.h"
#include "gui/Dock.h"
#include "gui/ai/AIDock.h"
#include "gui/CameraViewDock.h"
#include "gui/FboSnapshotView.h"
#include "gui/DrawingViewDock.h"
#include "core/CameraNode.h"
#include "core/ViewNode.h"
#include "core/SourceFile.h"
#include "gui/ViewScope.h"
#include <boost/algorithm/string/join.hpp>
#include "core/DimrefNode.h"
#include "core/LabelNode.h"
#include "core/SectionNode.h"
#include "glview/GLView.h"
#include "glview/OffscreenView.h"
#include "utils/camera_math.h"
#include "gui/Editor.h"
#include "gui/Export3mfDialog.h"
#include "gui/ExportPdfDialog.h"
#include "gui/ExportSvgDialog.h"
#include "gui/ExternalToolInterface.h"
#include "gui/ImportUtils.h"
#include "gui/LibraryInfoDialog.h"
#include "gui/Measurement.h"
#include "gui/OpenSCADApp.h"
#include "gui/Preferences.h"
#include "Feature.h"
#include "gui/PrintInitDialog.h"
#include "gui/ProgressWidget.h"
#include "gui/QGLView.h"
#include "gui/QSettingsCached.h"
#include "gui/QWordSearchField.h"
#include "gui/ScintillaEditor.h"
#include "gui/SettingsWriter.h"
#include "gui/TabManager.h"
#include "gui/UIUtils.h"
#include "gui/input/InputDriverEvent.h"
#include "gui/input/InputDriverManager.h"
#include "io/dxfdim.h"
#include "io/export.h"
#include "io/export_line_drawing.h"
#include "io/fileutils.h"
#include "openscad.h"
#include "platform/PlatformUtils.h"
#include "utils/exceptions.h"
#include "utils/printutils.h"
#include "version.h"

#ifdef ENABLE_CGAL
#include "geometry/cgal/CGALCache.h"
#include "geometry/cgal/CGALNefGeometry.h"
#include "geometry/cgal/cgal.h"
#endif  // ENABLE_CGAL
#ifdef ENABLE_MANIFOLD
#include "geometry/manifold/ManifoldGeometry.h"
#include "geometry/manifold/manifoldutils.h"
#endif  // ENABLE_MANIFOLD
#ifdef ENABLE_OPENCSG
#include <opencsg.h>

#include "core/CSGTreeEvaluator.h"
#include "glview/preview/OpenCSGRenderer.h"
#endif
#ifdef OPENSCAD_UPDATER
#include "gui/AutoUpdater.h"
#endif

#ifdef ENABLE_PYTHON
#include "nettle/base64.h"
#include "nettle/sha2.h"
#include "python/python_public.h"

std::string SHA256HashString(std::string aString)
{
  uint8_t digest[SHA256_DIGEST_SIZE];
  sha256_ctx sha256_ctx;

  sha256_init(&sha256_ctx);
  sha256_update(&sha256_ctx, aString.length(), (uint8_t *)aString.c_str());
  sha256_digest(&sha256_ctx, SHA256_DIGEST_SIZE, digest);

  base64_encode_ctx base64_ctx;
  char digest_base64[BASE64_ENCODE_LENGTH(SHA256_DIGEST_SIZE) + 1];
  memset(digest_base64, 0, sizeof(digest_base64));

  base64_encode_init(&base64_ctx);
  base64_encode_update(&base64_ctx, digest_base64, SHA256_DIGEST_SIZE, digest);
  base64_encode_final(&base64_ctx, digest_base64);
  return digest_base64;
}

#endif  // ifdef ENABLE_PYTHON

#include "gui/PrintService.h"
#include "input/MouseConfigWidget.h"

// Global application state
unsigned int GuiLocker::guiLocked = 0;

bool MainWindow::undockMode = false;
bool MainWindow::reorderMode = false;
const int MainWindow::tabStopWidth = 15;
QElapsedTimer *MainWindow::progressThrottle = new QElapsedTimer();

namespace {

const int autoReloadPollingPeriodMS = 200;

struct DockFocus {
  Dock *widget;
  std::function<void(MainWindow *)> focus;
};

QAction *findAction(const QList<QAction *>& actions, const std::string& name)
{
  for (const auto action : actions) {
    if (action->objectName().toStdString() == name) {
      return action;
    }
    if (action->menu()) {
      auto foundAction = findAction(action->menu()->actions(), name);
      if (foundAction) return foundAction;
    }
  }
  return nullptr;
}

void fileExportedMessage(const QString& format, const QString& filename)
{
  LOG("%1$s export finished: %2$s", format.toUtf8().constData(), filename.toUtf8().constData());
}

void removeExportActions(QToolBar *toolbar, QAction *action)
{
  int idx = toolbar->actions().indexOf(action);
  while (idx > 0) {
    QAction *a = toolbar->actions().at(idx - 1);
    if (a->objectName().isEmpty())  // separator
      break;
    toolbar->removeAction(a);
    idx--;
  }
}

std::unique_ptr<ExternalToolInterface> createExternalToolService(print_service_t serviceType,
                                                                 const QString& serviceName,
                                                                 FileFormat fileFormat)
{
  switch (serviceType) {
  case print_service_t::NONE:
    // TODO: Print warning
    return nullptr;
    break;
  case print_service_t::PRINT_SERVICE: {
    if (const auto printService = PrintService::getPrintService(serviceName.toStdString())) {
      return createExternalPrintService(printService, fileFormat);
    }
    LOG("Unknown print service \"%1$s\"", serviceName.toStdString());
    return nullptr;
    break;
  }
  case print_service_t::OCTOPRINT:         return createOctoPrintService(fileFormat); break;
  case print_service_t::LOCAL_APPLICATION: return createLocalProgramService(fileFormat); break;
  }
  return {};
}

}  // namespace

MainWindow::MainWindow(const QStringList& filenames) : rubberBandManager(this)
{
  // Main UI setup
  setupWindow();
  setupMenusAndActions();

  // Workers, timers etc.
  // Set up early as some reactions to GUI state changes may trigger
  setupCoreSubsystems();

  // Docks
  setupConsole();
  auto guard = scopedSetCurrentOutput();

  setupStatusBar();
  setupAnimate();
  setupEditor(filenames);
  setupCustomizer();
  setupErrorLog();
  setupFontList();
  setupColorList();
  setupAIDock();
  setupCameraDock();
  setupDrawingDock();
  setupViewSwitcher();
  setupDocks();

  setup3DView();
  setupViewportControl();
  setupInput();
  setupPreferences();

  restoreWindowState();

  this->hideFind();
  show();
  openRemainingFiles(filenames);
}

void MainWindow::setAllMouseViewActions()
{
  // Set the mouse actions to those held in the settings.
  this->qglview->setMouseActions(MouseConfig::MouseAction::LEFT_CLICK,
                                 MouseConfig::viewActionArrays.at(static_cast<MouseConfig::ViewAction>(
                                   Settings::Settings::inputMouseLeftClick.value())));
  this->qglview->setMouseActions(MouseConfig::MouseAction::MIDDLE_CLICK,
                                 MouseConfig::viewActionArrays.at(static_cast<MouseConfig::ViewAction>(
                                   Settings::Settings::inputMouseMiddleClick.value())));
  this->qglview->setMouseActions(MouseConfig::MouseAction::RIGHT_CLICK,
                                 MouseConfig::viewActionArrays.at(static_cast<MouseConfig::ViewAction>(
                                   Settings::Settings::inputMouseRightClick.value())));
  this->qglview->setMouseActions(MouseConfig::MouseAction::SHIFT_LEFT_CLICK,
                                 MouseConfig::viewActionArrays.at(static_cast<MouseConfig::ViewAction>(
                                   Settings::Settings::inputMouseShiftLeftClick.value())));
  this->qglview->setMouseActions(MouseConfig::MouseAction::SHIFT_MIDDLE_CLICK,
                                 MouseConfig::viewActionArrays.at(static_cast<MouseConfig::ViewAction>(
                                   Settings::Settings::inputMouseShiftMiddleClick.value())));
  this->qglview->setMouseActions(MouseConfig::MouseAction::SHIFT_RIGHT_CLICK,
                                 MouseConfig::viewActionArrays.at(static_cast<MouseConfig::ViewAction>(
                                   Settings::Settings::inputMouseShiftRightClick.value())));
  this->qglview->setMouseActions(MouseConfig::MouseAction::CTRL_LEFT_CLICK,
                                 MouseConfig::viewActionArrays.at(static_cast<MouseConfig::ViewAction>(
                                   Settings::Settings::inputMouseCtrlLeftClick.value())));
  this->qglview->setMouseActions(MouseConfig::MouseAction::CTRL_MIDDLE_CLICK,
                                 MouseConfig::viewActionArrays.at(static_cast<MouseConfig::ViewAction>(
                                   Settings::Settings::inputMouseCtrlMiddleClick.value())));
  this->qglview->setMouseActions(MouseConfig::MouseAction::CTRL_RIGHT_CLICK,
                                 MouseConfig::viewActionArrays.at(static_cast<MouseConfig::ViewAction>(
                                   Settings::Settings::inputMouseCtrlRightClick.value())));
  this->qglview->setMouseActions(MouseConfig::MouseAction::CTRL_SHIFT_LEFT_CLICK,
                                 MouseConfig::viewActionArrays.at(static_cast<MouseConfig::ViewAction>(
                                   Settings::Settings::inputMouseCtrlShiftLeftClick.value())));
  this->qglview->setMouseActions(MouseConfig::MouseAction::CTRL_SHIFT_MIDDLE_CLICK,
                                 MouseConfig::viewActionArrays.at(static_cast<MouseConfig::ViewAction>(
                                   Settings::Settings::inputMouseCtrlShiftMiddleClick.value())));
  this->qglview->setMouseActions(MouseConfig::MouseAction::CTRL_SHIFT_RIGHT_CLICK,
                                 MouseConfig::viewActionArrays.at(static_cast<MouseConfig::ViewAction>(
                                   Settings::Settings::inputMouseCtrlShiftRightClick.value())));
}

void MainWindow::onNavigationOpenContextMenu()
{
  navigationMenu->exec(QCursor::pos());
}

void MainWindow::onNavigationCloseContextMenu()
{
  rubberBandManager.hide();
}

void MainWindow::onNavigationTriggerContextMenuEntry()
{
  auto *action = qobject_cast<QAction *>(sender());
  if (!action || !action->property("id").isValid()) return;

  Dock *dock = action->property("id").value<Dock *>();
  assert(dock != nullptr);

  dock->show();
  dock->raise();
  dock->setFocus();

  // Forward the focus on the content of the tabmanager
  if (dock == editorDock) {
    tabManager->setFocus();
  }
}

void MainWindow::onNavigationHoveredContextMenuEntry()
{
  auto *action = qobject_cast<QAction *>(sender());
  if (!action || !action->property("id").isValid()) return;

  Dock *dock = action->property("id").value<Dock *>();
  assert(dock != nullptr);

  // Hover signal is emitted at each mouse move, to avoid excessive
  // load we only raise/emphasize if it is not yet done.
  if (rubberBandManager.isEmphasized(dock)) return;

  dock->raise();
  rubberBandManager.emphasize(dock);
}

void MainWindow::addExportActions(QToolBar *toolbar, QAction *action) const
{
  for (const std::string& identifier :
       {Settings::Settings::toolbarExport3D.value(), Settings::Settings::toolbarExport2D.value()}) {
    QAction *exportAction = formatIdentifierToAction(identifier);
    if (exportAction) {
      toolbar->insertAction(action, exportAction);
    }
  }
}

void MainWindow::updateExportActions()
{
  removeExportActions(editortoolbar, this->designAction3DPrint);
  addExportActions(editortoolbar, this->designAction3DPrint);

  // handle the hide/show of export action in view toolbar according to the visibility of editor dock
  removeExportActions(viewerToolBar, this->viewActionViewAll);
  if (!editorDock->isVisible()) {
    addExportActions(viewerToolBar, this->viewActionViewAll);
  }
}

void MainWindow::openFileFromPath(const QString& path, int line)
{
  if (editorDock->isVisible()) {
    auto guard = scopedSetCurrentOutput();
    activeEditor->setFocus();
    if (!path.isEmpty()) tabManager->open(path);
    activeEditor->setFocus();
    activeEditor->setCursorPosition(line, 0);
  }
}

void MainWindow::addKeyboardShortCut(const QList<QAction *>& actions)
{
  for (auto& action : actions) {
    // prevent adding shortcut twice if action is added to multiple toolbars
    if (action->toolTip().contains("&nbsp;")) {
      continue;
    }

    const QString shortCut(action->shortcut().toString(QKeySequence::NativeText));
    if (shortCut.isEmpty()) {
      continue;
    }

    const QString toolTip(
      "%1 &nbsp;<span style=\"color: gray; font-size: small; font-style: italic\">%2</span>");
    action->setToolTip(toolTip.arg(action->toolTip(), shortCut));
  }
}

void MainWindow::onAxisChanged(InputEventAxisChanged *)
{
}

void MainWindow::onButtonChanged(InputEventButtonChanged *)
{
}

void MainWindow::onTranslateEvent(InputEventTranslate *event)
{
  const double zoomFactor = 0.001 * qglview->cam.zoomValue();

  if (event->viewPortRelative) {
    qglview->translate(event->x, event->y, event->z, event->relative, true);
  } else {
    qglview->translate(zoomFactor * event->x, event->y, zoomFactor * event->z, event->relative, false);
  }
}

void MainWindow::onRotateEvent(InputEventRotate *event)
{
  qglview->rotate(event->x, event->y, event->z, event->relative);
}

void MainWindow::onRotate2Event(InputEventRotate2 *event)
{
  qglview->rotate2(event->x, event->y, event->z);
}

void MainWindow::onActionEvent(InputEventAction *event)
{
  const std::string actionName = event->action;
  if (actionName.find("::") == std::string::npos) {
    QAction *action = findAction(this->menuBar()->actions(), actionName);
    if (action) {
      action->trigger();
    } else if ("viewActionTogglePerspective" == actionName) {
      viewTogglePerspective();
    }
  } else {
    const std::string target = actionName.substr(0, actionName.find("::"));
    if (target == "animate") {
      this->animateWidget->onActionEvent(event);
    } else {
      std::cout << "unknown onActionEvent target: " << actionName << std::endl;
    }
  }
}

void MainWindow::onZoomEvent(InputEventZoom *event)
{
  qglview->zoom(event->zoom, event->relative);
}

void MainWindow::loadViewSettings()
{
  const QSettingsCached settings;

  if (settings.value("view/showEdges").toBool()) {
    viewActionShowEdges->setChecked(true);
  }
  if (settings.value("view/showAxes", true).toBool()) {
    viewActionShowAxes->setChecked(true);
  }
  if (settings.value("view/showCrosshairs").toBool()) {
    viewActionShowCrosshairs->setChecked(true);
  }
  if (settings.value("view/showScaleProportional", true).toBool()) {
    viewActionShowScaleProportional->setChecked(true);
  }
  viewTogglePerspective();

  updateUndockMode(GlobalPreferences::inst()->getValue("advanced/undockableWindows").toBool());
  updateReorderMode(GlobalPreferences::inst()->getValue("advanced/reorderWindows").toBool());
}

void MainWindow::loadDesignSettings()
{
  const QSettingsCached settings;
  if (settings.value("design/autoReload", false).toBool()) {
    designActionAutoReload->setChecked(true);
  }
  auto polySetCacheSizeMB = GlobalPreferences::inst()->getValue("advanced/polysetCacheSizeMB").toUInt();
  GeometryCache::instance()->setMaxSizeMB(polySetCacheSizeMB);
  auto cgalCacheSizeMB = GlobalPreferences::inst()->getValue("advanced/cgalCacheSizeMB").toUInt();
  CGALCache::instance()->setMaxSizeMB(cgalCacheSizeMB);
  auto backend3D =
    GlobalPreferences::inst()->getValue("advanced/renderBackend3D").toString().toStdString();
  RenderSettings::inst()->backend3D =
    renderBackend3DFromString(backend3D).value_or(DEFAULT_RENDERING_BACKEND_3D);
}

void MainWindow::updateUndockMode(bool undockMode)
{
  MainWindow::undockMode = undockMode;
  if (undockMode) {
    editorDock->setFeatures(editorDock->features() | QDockWidget::DockWidgetFloatable);
    consoleDock->setFeatures(consoleDock->features() | QDockWidget::DockWidgetFloatable);
    parameterDock->setFeatures(parameterDock->features() | QDockWidget::DockWidgetFloatable);
    errorLogDock->setFeatures(errorLogDock->features() | QDockWidget::DockWidgetFloatable);
    animateDock->setFeatures(animateDock->features() | QDockWidget::DockWidgetFloatable);
    fontListDock->setFeatures(fontListDock->features() | QDockWidget::DockWidgetFloatable);
    colorListDock->setFeatures(colorListDock->features() | QDockWidget::DockWidgetFloatable);
    viewportControlDock->setFeatures(viewportControlDock->features() | QDockWidget::DockWidgetFloatable);
    aiDock->setFeatures(aiDock->features() | QDockWidget::DockWidgetFloatable);
  } else {
    if (editorDock->isFloating()) {
      editorDock->setFloating(false);
    }
    editorDock->setFeatures(editorDock->features() & ~QDockWidget::DockWidgetFloatable);

    if (consoleDock->isFloating()) {
      consoleDock->setFloating(false);
    }
    consoleDock->setFeatures(consoleDock->features() & ~QDockWidget::DockWidgetFloatable);

    if (parameterDock->isFloating()) {
      parameterDock->setFloating(false);
    }
    parameterDock->setFeatures(parameterDock->features() & ~QDockWidget::DockWidgetFloatable);

    if (errorLogDock->isFloating()) {
      errorLogDock->setFloating(false);
    }
    errorLogDock->setFeatures(errorLogDock->features() & ~QDockWidget::DockWidgetFloatable);

    if (animateDock->isFloating()) {
      animateDock->setFloating(false);
    }
    animateDock->setFeatures(animateDock->features() & ~QDockWidget::DockWidgetFloatable);

    if (fontListDock->isFloating()) {
      fontListDock->setFloating(false);
    }
    fontListDock->setFeatures(fontListDock->features() & ~QDockWidget::DockWidgetFloatable);

    if (colorListDock->isFloating()) {
      colorListDock->setFloating(false);
    }
    colorListDock->setFeatures(colorListDock->features() & ~QDockWidget::DockWidgetFloatable);

    if (viewportControlDock->isFloating()) {
      viewportControlDock->setFloating(false);
    }
    viewportControlDock->setFeatures(viewportControlDock->features() &
                                     ~QDockWidget::DockWidgetFloatable);

    if (aiDock->isFloating()) {
      aiDock->setFloating(false);
    }
    aiDock->setFeatures(aiDock->features() & ~QDockWidget::DockWidgetFloatable);
  }
}

void MainWindow::updateReorderMode(bool reorderMode)
{
  MainWindow::reorderMode = reorderMode;
  for (auto& [dock, name] : docks) {
    dock->setTitleBarVisibility(reorderMode);
  }
}

MainWindow::~MainWindow()
{
  delete this->cgalworker;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
  if (!tabManager->shouldClose()) {
    event->ignore();
    return;
  }
  event->accept();

  // Only save when this is the last MainWindow.
  if (scadApp->windowManager.getWindows().size() == 1) {
    saveWindowState();
  }

  isClosing = true;
  progress_report_fin();

  if (this->tempFile) {
    delete this->tempFile;
    this->tempFile = nullptr;
  }

  // Log to stdout from now on
  clearCurrentOutput();
  // Disable invokeMethod calls for consoleOutput during shutdown,
  // otherwise will segfault if echos are in progress.
  hideCurrentOutput();

  // Make sure all the floating docks are closed too as those
  // would stick around otherwise if the application keeps
  // running (after closing just a single window, or when
  // aborting the close process because of user cancellation).
  for (auto& [dock, title] : docks) {
    if (dock->isFloating()) {
      dock->close();
    }
  }

  scadApp->windowManager.remove(this);
  if (scadApp->windowManager.getWindows().empty()) {
    // Quit application even in case some other windows like
    // Preferences are still open.
    QApplication::quit();
  }
}

void MainWindow::saveWindowState()
{
  QSettingsCached settings;
  settings.setValue("window/geometry", saveGeometry());
  auto windowState = saveState();
  UIUtils::dumpSaveState(windowState);
  settings.setValue("window/state", windowState);
}

void MainWindow::showProgress()
{
  updateStatusBar(qobject_cast<ProgressWidget *>(sender()));
}

void MainWindow::report_func(const std::shared_ptr<const AbstractNode>&, void *vp, int mark)
{
  // limit to progress bar update calls to 5 per second
  static const qint64 MIN_TIMEOUT = 200;
  if (progressThrottle->hasExpired(MIN_TIMEOUT)) {
    progressThrottle->start();

    auto thisp = static_cast<MainWindow *>(vp);
    auto v = static_cast<int>((mark * 1000.0) / progress_report_count);
    auto permille = v < 1000 ? v : 999;
    if (permille > thisp->progresswidget->value()) {
      QMetaObject::invokeMethod(thisp->progresswidget, "setValue", Qt::QueuedConnection,
                                Q_ARG(int, permille));
      QApplication::processEvents();
    }

    // FIXME: Check if cancel was requested by e.g. Application quit
    if (thisp->progresswidget->wasCanceled()) throw ProgressCancelException();
  }
}

bool MainWindow::network_progress_func(const double permille)
{
  QMetaObject::invokeMethod(this->progresswidget, "setValue", Qt::QueuedConnection,
                            Q_ARG(int, (int)permille));
  return (progresswidget && progresswidget->wasCanceled());
}

void MainWindow::updateRecentFiles(const QString& FileSavedOrOpened)
{
  // Check that the canonical file path exists - only update recent files
  // if it does. Should prevent empty list items on initial open etc.
  QSettingsCached settings;  // already set up properly via main.cpp
  auto files = settings.value("recentFileList").toStringList();
  files.removeAll(FileSavedOrOpened);
  files.prepend(FileSavedOrOpened);
  while (files.size() > UIUtils::maxRecentFiles) files.removeLast();
  settings.setValue("recentFileList", files);

  for (auto& widget : QApplication::topLevelWidgets()) {
    auto mainWin = qobject_cast<MainWindow *>(widget);
    if (mainWin) {
      mainWin->updateRecentFileActions();
    }
  }
}

/*!
   compiles the design. Calls compileDone() if anything was compiled
 */
void MainWindow::compile(bool reload, bool forcedone)
{
  OpenSCAD::hardwarnings = GlobalPreferences::inst()->getValue("advanced/enableHardwarnings").toBool();
  OpenSCAD::traceDepth = GlobalPreferences::inst()->getValue("advanced/traceDepth").toUInt();
  OpenSCAD::traceUsermoduleParameters =
    GlobalPreferences::inst()->getValue("advanced/enableTraceUsermoduleParameters").toBool();
  OpenSCAD::parameterCheck =
    GlobalPreferences::inst()->getValue("advanced/enableParameterCheck").toBool();
  OpenSCAD::rangeCheck =
    GlobalPreferences::inst()->getValue("advanced/enableParameterRangeCheck").toBool();

  try {
    bool shouldcompiletoplevel = false;
    bool didcompile = false;

    compileErrors = 0;
    compileWarnings = 0;

    this->renderStatistic.start();

    // Reload checks the timestamp of the toplevel file and refreshes if necessary,
    if (reload) {
      // Refresh files if it has changed on disk
      if (fileChangedOnDisk() && checkEditorModified()) {
        shouldcompiletoplevel =
          tabManager->refreshDocument();  // don't compile if we couldn't open the file
        if (shouldcompiletoplevel &&
            GlobalPreferences::inst()->getValue("advanced/autoReloadRaise").toBool()) {
          // reloading the 'same' document brings the 'old' one to front.
          this->raise();
        }
      }
      // If the file has some content and there is no currently compiled content,
      // then we force the top level compilation.
      else {
        auto current_doc = activeEditor->toPlainText();
        if (current_doc.size() && lastCompiledDoc.size() == 0) {
          shouldcompiletoplevel = true;
        }
      }
    } else {
      shouldcompiletoplevel = true;
    }

    if (this->parsedFile) {
      auto mtime = this->parsedFile->includesChanged();
      if (mtime > this->includesMTime) {
        this->includesMTime = mtime;
        shouldcompiletoplevel = true;
      }
    }

    // Parsing and dependency handling must run to completion even with stop on errors to prevent auto
    // reload picking up where it left off, thwarting the stop, so we turn off exceptions in PRINT.
    no_exceptions_for_warnings();
    if (shouldcompiletoplevel) {
      initialize_rng();
      this->errorLogWidget->clearModel();
      if (GlobalPreferences::inst()->getValue("advanced/consoleAutoClear").toBool()) {
        this->console->clear();
      }
      if (activeEditor->isContentModified()) saveBackup();
      parseTopLevelDocument();
      didcompile = true;
    }

    if (didcompile && parser_error_pos != lastParserErrorPos) {
      if (lastParserErrorPos >= 0) emit unhighlightLastError();
      if (parser_error_pos >= 0) emit highlightError(parser_error_pos);
      lastParserErrorPos = parser_error_pos;
    }

    if (this->rootFile) {
      auto mtime = this->rootFile->handleDependencies();
      if (mtime > this->depsMTime) {
        this->depsMTime = mtime;
        LOG("Used file cache size: %1$d files", SourceFileCache::instance()->size());
        didcompile = true;
      }
    }

    // Had any errors in the parse that would have caused exceptions via PRINT.
    if (would_have_thrown()) throw HardWarningException("");
    // If we're auto-reloading, listen for a cascade of changes by starting a timer
    // if something changed _and_ there are any external dependencies
    if (reload && didcompile && this->rootFile) {
      if (this->rootFile->hasIncludes() || this->rootFile->usesLibraries()) {
        this->waitAfterReloadTimer->start();
        this->procevents = false;
        return;
      }
    }

    compileDone(didcompile | forcedone);
  } catch (const HardWarningException&) {
    exceptionCleanup();
  } catch (const std::exception& ex) {
    UnknownExceptionCleanup(ex.what());
  } catch (...) {
    UnknownExceptionCleanup();
  }
}

void MainWindow::waitAfterReload()
{
  no_exceptions_for_warnings();
  auto mtime = this->rootFile->handleDependencies();
  auto stop = would_have_thrown();
  if (mtime > this->depsMTime) this->depsMTime = mtime;
  else if (!stop) {
    compile(true, true);  // In case file itself or top-level includes changed during dependency updates
    return;
  }
  this->waitAfterReloadTimer->start();
}

void MainWindow::on_toolButtonCompileResultClose_clicked()
{
  frameCompileResult->hide();
}

void MainWindow::updateCompileResult()
{
  if ((compileErrors == 0) && (compileWarnings == 0)) {
    frameCompileResult->hide();
    return;
  }

  if (!Settings::Settings::showWarningsIn3dView.value()) {
    return;
  }

  QString msg;
  if (compileErrors > 0) {
    if (activeEditor->filepath.isEmpty()) {
      msg = QString(_("Compile error."));
    } else {
      const QFileInfo fileInfo(activeEditor->filepath);
      msg = QString(_("Error while compiling '%1'.")).arg(fileInfo.fileName());
    }
    toolButtonCompileResultIcon->setIcon(
      QIcon(QString::fromUtf8(":/icons/information-icons-error.png")));
  } else {
    const char *fmt = ngettext("Compilation generated %1 warning.", "Compilation generated %1 warnings.",
                               compileWarnings);
    msg = QString(fmt).arg(compileWarnings);
    toolButtonCompileResultIcon->setIcon(
      QIcon(QString::fromUtf8(":/icons/information-icons-warning.png")));
  }
  const QFontMetrics fm(labelCompileResultMessage->font());
  const int sizeIcon = std::max(12, std::min(32, fm.height()));
  const int sizeClose = std::max(10, std::min(32, fm.height()) - 4);
  toolButtonCompileResultIcon->setIconSize(QSize(sizeIcon, sizeIcon));
  toolButtonCompileResultClose->setIconSize(QSize(sizeClose, sizeClose));

  msg += _(
    R"( For details see the <a href="#errorlog">error log</a> and <a href="#console">console window</a>.)");
  labelCompileResultMessage->setText(msg);
  frameCompileResult->show();
}

void MainWindow::compileDone(bool didchange)
{
  OpenSCAD::hardwarnings = GlobalPreferences::inst()->getValue("advanced/enableHardwarnings").toBool();
  try {
    const char *callslot;
    if (didchange) {
      instantiateRoot();
      updateCompileResult();
      callslot = afterCompileSlot;
    } else {
      callslot = "compileEnded";
    }

    this->procevents = false;
    QMetaObject::invokeMethod(this, callslot);
  } catch (const HardWarningException&) {
    exceptionCleanup();
  }

  if (didchange) {
    const bool flagAutoCompleteIncludeVariables =
      GlobalPreferences::inst()->getValue("editor/autoCompleteIncludeVariables").toBool();
    const bool flagAutoCompleteIncludeModules =
      GlobalPreferences::inst()->getValue("editor/autoCompleteIncludeModules").toBool();
    const bool flagAutoCompleteIncludeFunctions =
      GlobalPreferences::inst()->getValue("editor/autoCompleteIncludeFunctions").toBool();

    const auto completionMode = Settings::SettingsAutoCompletion::autocompleteMode.value();

    auto *scintillaEditor = dynamic_cast<ScintillaEditor *>(this->activeEditor);

    if (scintillaEditor) {
      if (completionMode == "ParsedFileMode") {
        scintillaEditor->correctUserVarNamesForCompletionFromSourceFile(
          parsedFile.get(), flagAutoCompleteIncludeVariables, flagAutoCompleteIncludeModules,
          flagAutoCompleteIncludeFunctions);

      } else if (completionMode == "RegexInputTextMode") {
        scintillaEditor->correctUserVarNamesForCompletionFromInputText(flagAutoCompleteIncludeVariables,
                                                                       flagAutoCompleteIncludeModules,
                                                                       flagAutoCompleteIncludeFunctions);
      }
    }
  }
}

void MainWindow::compileEnded()
{
  clearCurrentOutput();
  GuiLocker::unlock();
  if (designActionAutoReload->isChecked()) autoReloadTimer->start();
#ifdef ENABLE_GUI_TESTS
  emit compilationDone(this->rootFile.get());
#endif
}

#ifdef ENABLE_GUI_TESTS
std::shared_ptr<AbstractNode> MainWindow::instantiateRootFromSource(SourceFile *file)
{
  EvaluationSession session{file->getFullpath()};
  ContextHandle<BuiltinContext> builtin_context{Context::create<BuiltinContext>(&session)};
  setRenderVariables(builtin_context);

  std::shared_ptr<const FileContext> file_context;
  std::shared_ptr<AbstractNode> node = this->rootFile->instantiate(*builtin_context, &file_context);

  return node;
}
#endif  // ifdef ENABLE_GUI_TESTS

void MainWindow::instantiateRoot()
{
  // Go on and instantiate root_node, then call the continuation slot

  // Invalidate renderers before we kill the CSG tree
  this->qglview->setRenderer(nullptr);
#ifdef ENABLE_OPENCSG
  this->previewRenderer = nullptr;
#endif
  this->thrownTogetherRenderer = nullptr;

  // Remove previous CSG tree
  this->absoluteRootNode.reset();

  this->csgRoot.reset();
  this->normalizedRoot.reset();
  this->rootProduct.reset();

  this->rootNode.reset();
  this->tree.setRoot(nullptr);

  const std::filesystem::path doc(activeEditor->filepath.toStdString());
  this->tree.setDocumentPath(doc.parent_path().string());

  renderedEditor = activeEditor;

  if (this->rootFile) {
    // Evaluate CSG tree
    LOG("Compiling design (CSG Tree generation)...");
    this->processEvents();

    AbstractNode::resetIndexCounter();

    EvaluationSession session{doc.parent_path().string()};
    ContextHandle<BuiltinContext> builtin_context{Context::create<BuiltinContext>(&session)};
    setRenderVariables(builtin_context);

    std::shared_ptr<const FileContext> file_context;
#ifdef ENABLE_PYTHON
    if (python_result_node != NULL && this->python_active) this->absoluteRootNode = python_result_node;
    else
#endif
      this->absoluteRootNode = this->rootFile->instantiate(*builtin_context, &file_context);
    if (file_context) {
      this->qglview->cam.updateView(file_context, false);
      viewportControlWidget->cameraChanged();
    }

    if (this->absoluteRootNode) {
      // Do we have an explicit root node (! modifier)?
      const Location *nextLocation = nullptr;
      if (!(this->rootNode = find_root_tag(this->absoluteRootNode, &nextLocation))) {
        this->rootNode = this->absoluteRootNode;
      }
      if (nextLocation) {
        LOG(message_group::NONE, *nextLocation, builtin_context->documentRoot(),
            "More than one Root Modifier (!)");
      }

      // FIXME: Consider giving away ownership of root_node to the Tree, or use reference counted
      // pointers
      this->tree.setRoot(this->rootNode);
    }
  }

  if (!this->rootNode) {
    if (parser_error_pos < 0) {
      LOG(message_group::Error, "Compilation failed! (no top level object found)");
    } else {
      LOG(message_group::Error, "Compilation failed!");
    }
    LOG(" ");
    this->processEvents();
  }
}

/*!
   Generates CSG tree for OpenCSG evaluation.
   Assumes that the design has been parsed and evaluated (this->root_node is set)
 */
void MainWindow::compileCSG()
{
  OpenSCAD::hardwarnings = GlobalPreferences::inst()->getValue("advanced/enableHardwarnings").toBool();
  try {
    assert(this->rootNode);
    LOG("Compiling design (CSG Products generation)...");
    this->processEvents();

    // Main CSG evaluation
    this->progresswidget = new ProgressWidget(this);
    connect(this->progresswidget, &ProgressWidget::requestShow, this, &MainWindow::showProgress);

    GeometryEvaluator geomevaluator(this->tree);
#ifdef ENABLE_OPENCSG
    CSGTreeEvaluator csgrenderer(this->tree, &geomevaluator);
#endif

    if (!isClosing) progress_report_prep(this->rootNode, report_func, this);
    else return;
    try {
#ifdef ENABLE_OPENCSG
      this->processEvents();
      this->csgRoot = csgrenderer.buildCSGTree(*rootNode);
#endif
      renderStatistic.printCacheStatistic();
      this->processEvents();
    } catch (const ProgressCancelException&) {
      LOG("CSG generation cancelled.");
    } catch (const HardWarningException&) {
      LOG("CSG generation cancelled due to hardwarning being enabled.");
    }
    progress_report_fin();
    updateStatusBar(nullptr);

    LOG("Compiling design (CSG Products normalization)...");
    this->processEvents();

    const size_t normalizelimit =
      2ul * GlobalPreferences::inst()->getValue("advanced/openCSGLimit").toUInt();
    CSGTreeNormalizer normalizer(normalizelimit);

    if (this->csgRoot) {
      this->normalizedRoot = normalizer.normalize(this->csgRoot);
      if (this->normalizedRoot) {
        this->rootProduct = std::make_shared<CSGProducts>();
        this->rootProduct->import(this->normalizedRoot);
      } else {
        this->rootProduct.reset();
        LOG(message_group::Warning, "CSG normalization resulted in an empty tree");
        this->processEvents();
      }
    }

    const std::vector<std::shared_ptr<CSGNode>>& highlight_terms = csgrenderer.getHighlightNodes();
    if (highlight_terms.size() > 0) {
      LOG("Compiling highlights (%1$d CSG Trees)...", highlight_terms.size());
      this->processEvents();

      this->highlightsProducts = std::make_shared<CSGProducts>();
      for (const auto& highlight_term : highlight_terms) {
        auto nterm = normalizer.normalize(highlight_term);
        if (nterm) {
          this->highlightsProducts->import(nterm);
        }
      }
    } else {
      this->highlightsProducts.reset();
    }

    const auto& background_terms = csgrenderer.getBackgroundNodes();
    if (background_terms.size() > 0) {
      LOG("Compiling background (%1$d CSG Trees)...", background_terms.size());
      this->processEvents();

      this->backgroundProducts = std::make_shared<CSGProducts>();
      for (const auto& background_term : background_terms) {
        auto nterm = normalizer.normalize(background_term);
        if (nterm) {
          this->backgroundProducts->import(nterm);
        }
      }
    } else {
      this->backgroundProducts.reset();
    }

    if (this->rootProduct && (this->rootProduct->size() >
                              GlobalPreferences::inst()->getValue("advanced/openCSGLimit").toUInt())) {
      LOG(message_group::UI_Warning, "Normalized tree has %1$d elements!", this->rootProduct->size());
      LOG(message_group::UI_Warning, "OpenCSG rendering has been disabled.");
    }
#ifdef ENABLE_OPENCSG
    else {
      LOG("Normalized tree has %1$d elements!", (this->rootProduct ? this->rootProduct->size() : 0));
      this->previewRenderer = std::make_shared<OpenCSGRenderer>(
        this->rootProduct, this->highlightsProducts, this->backgroundProducts);
    }
#endif  // ifdef ENABLE_OPENCSG
    this->thrownTogetherRenderer = std::make_shared<ThrownTogetherRenderer>(
      this->rootProduct, this->highlightsProducts, this->backgroundProducts);
    LOG("Compile and preview finished.");
    renderStatistic.printRenderingTime();
    this->processEvents();
  } catch (const HardWarningException&) {
    exceptionCleanup();
  }
}

void MainWindow::on_fileActionOpen_triggered()
{
  auto guard = scopedSetCurrentOutput();
  auto fileInfoList = UIUtils::openFiles(this);
  for (auto& i : fileInfoList) {
    if (!i.exists()) {
      return;
    }
    tabManager->open(i.filePath());
  }
}

void MainWindow::on_fileActionNewWindow_triggered()
{
  new MainWindow(QStringList());
}

void MainWindow::on_fileActionOpenWindow_triggered()
{
  auto fileInfoList = UIUtils::openFiles(this);
  for (auto& i : fileInfoList) {
    if (!i.exists()) {
      return;
    }
    new MainWindow(QStringList(i.filePath()));
  }
}

void MainWindow::actionOpenRecent()
{
  auto guard = scopedSetCurrentOutput();
  auto action = qobject_cast<QAction *>(sender());
  tabManager->open(action->data().toString());
}

void MainWindow::on_fileActionClearRecent_triggered()
{
  QSettingsCached settings;  // already set up properly via main.cpp
  const QStringList files;
  settings.setValue("recentFileList", files);

  updateRecentFileActions();
}

// Updates the content of the recent files menu entries
// by iterating over the recently opened files.
void MainWindow::updateRecentFileActions()
{
  auto files = UIUtils::recentFiles();

  for (int i = 0; i < files.size(); ++i) {
    QAction *recent;
    if (i < this->fileActionRecentFiles.size()) {
      recent = this->fileActionRecentFiles[i];
    } else {
      recent = new QAction(this);
      connect(recent, &QAction::triggered, this, &MainWindow::actionOpenRecent);
      this->fileActionRecentFiles.push_back(recent);
    }
    this->menuOpenRecent->addAction(recent);
    recent->setText(QFileInfo(files[i]).fileName().replace("&", "&&"));
    recent->setData(files[i]);
    recent->setVisible(true);
  }
  this->menuOpenRecent->addSeparator();
  this->menuOpenRecent->addAction(this->fileActionClearRecent);
}

void MainWindow::show_examples()
{
  bool found_example = false;

  for (const auto& cat : UIUtils::exampleCategories()) {
    auto examples = UIUtils::exampleFiles(cat.name);
    auto menu = this->menuExamples->addMenu(gettext(cat.name.toStdString().c_str()));
    if (!cat.tooltip.trimmed().isEmpty()) {
      menu->setToolTip(gettext(cat.tooltip.toStdString().c_str()));
      menu->setToolTipsVisible(true);
    }

    for (const auto& ex : examples) {
      auto openAct = new QAction(ex.fileName().replace("&", "&&"), this);
      connect(openAct, &QAction::triggered, this, &MainWindow::actionOpenExample);
      menu->addAction(openAct);
      openAct->setData(ex.canonicalFilePath());
      found_example = true;
    }
  }

  if (!found_example) {
    delete this->menuExamples;
    this->menuExamples = nullptr;
  }
}

void MainWindow::actionOpenExample()
{
  auto guard = scopedSetCurrentOutput();
  const auto action = qobject_cast<QAction *>(sender());
  if (action) {
    const auto& path = action->data().toString();
    tabManager->open(path);
  }
}

void MainWindow::writeBackup(QFile *file)
{
  // see MainWindow::saveBackup()
  file->resize(0);
  QTextStream writer(file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  writer.setCodec("UTF-8");
#endif
  writer << activeEditor->toPlainText();
  this->activeEditor->parameterWidget->saveBackupFile(file->fileName());

  LOG("Saved backup file: %1$s", file->fileName().toUtf8().constData());
}

void MainWindow::saveBackup()
{
  auto path = PlatformUtils::backupPath();
  if ((!fs::exists(path)) && (!PlatformUtils::createBackupPath())) {
    LOG(message_group::UI_Warning, "Cannot create backup path: %1$s", path);
    return;
  }

  auto backupPath = QString::fromLocal8Bit(path.c_str());
  if (!backupPath.endsWith("/")) backupPath.append("/");

  QString basename = "unsaved";
  if (!activeEditor->filepath.isEmpty()) {
    auto fileInfo = QFileInfo(activeEditor->filepath);
    basename = fileInfo.baseName();
  }

  if (!this->tempFile) {
#ifdef ENABLE_PYTHON
    const QString suffix = this->python_active ? "py" : "scad";
#else
    const QString suffix = "scad";
#endif
    this->tempFile = new QTemporaryFile(backupPath.append(basename + "-backup-XXXXXXXX." + suffix));
  }

  if ((!this->tempFile->isOpen()) && (!this->tempFile->open())) {
    LOG(message_group::UI_Warning, "Failed to create backup file");
    return;
  }
  return writeBackup(this->tempFile);
}

void MainWindow::on_fileActionSave_triggered()
{
  tabManager->save(activeEditor);
}

void MainWindow::on_fileActionSaveAs_triggered()
{
  tabManager->saveAs(activeEditor);
}

void MainWindow::on_fileActionPythonRevoke_triggered()
{
  QSettingsCached settings;
#ifdef ENABLE_PYTHON
  python_trusted = false;
  this->trusted_edit_document_name = "";
#endif
  settings.remove("python_hash");
  QMessageBox::information(this, _("Trusted Files"), "All trusted python files revoked",
                           QMessageBox::Ok);
}

void MainWindow::on_fileActionPythonCreateVenv_triggered()
{
#ifdef ENABLE_PYTHON
  const QString selectedDir = QFileDialog::getExistingDirectory(this, "Create Virtual Environment");
  if (selectedDir.isEmpty()) {
    return;
  }

  const QDir venvDir{selectedDir};
  if (!venvDir.exists()) {
    // Should not happen, but just in case double check...
    QMessageBox::critical(this, _("Create Virtual Environment"),
                          "Directory does not exist. Can't create virtual environment.",
                          QMessageBox::Ok);
    return;
  }

  if (!venvDir.isEmpty()) {
    QMessageBox::critical(this, _("Create Virtual Environment"),
                          "Directory is not empty. Can't create virtual environment.", QMessageBox::Ok);
    return;
  }

  const auto& path = venvDir.absolutePath().toStdString();
  LOG("Creating Python virtual environment in '%1$s'...", path);
  int result = pythonCreateVenv(path);

  if (result == 0) {
    Settings::SettingsPython::pythonVirtualEnv.setValue(path);
    Settings::Settings::visit(SettingsWriter());
    LOG("Python virtual environment creation successfull.");
    QMessageBox::information(this, _("Create Virtual Environment"),
                             "Virtual environment created, please restart OpenSCAD to activate.",
                             QMessageBox::Ok);
  } else {
    LOG("Python virtual environment creation failed.");
    QMessageBox::critical(this, _("Create Virtual Environment"), "Virtual environment creation failed.",
                          QMessageBox::Ok);
  }
#endif  // ifdef ENABLE_PYTHON
}

void MainWindow::on_fileActionPythonSelectVenv_triggered()
{
#ifdef ENABLE_PYTHON
  const QString venvDir = QFileDialog::getExistingDirectory(this, "Select Virtual Environment");
  if (venvDir.isEmpty()) {
    return;
  }
  const QFileInfo fileInfo{QDir{venvDir}, "pyvenv.cfg"};
  if (fileInfo.exists()) {
    Settings::SettingsPython::pythonVirtualEnv.setValue(venvDir.toStdString());
    Settings::Settings::visit(SettingsWriter());
    QMessageBox::information(this, _("Select Virtual Environment"),
                             "Virtual environment selected, please restart OpenSCAD to activate.",
                             QMessageBox::Ok);
  }
#endif  // ifdef ENABLE_PYTHON
}

void MainWindow::on_fileActionSaveACopy_triggered()
{
  tabManager->saveACopy(activeEditor);
}

void MainWindow::on_fileShowLibraryFolder_triggered()
{
  auto guard = scopedSetCurrentOutput();
  auto path = PlatformUtils::userLibraryPath();
  if (!fs::exists(path)) {
    LOG(message_group::UI_Warning, "Library path %1$s doesn't exist. Creating", path);
    if (!PlatformUtils::createUserLibraryPath()) {
      LOG(message_group::UI_Error, "Cannot create library path: %1$s", path);
    }
  }
  auto url = QString::fromStdString(path);
  LOG("Opening file browser for %1$s", url.toStdString());
  QDesktopServices::openUrl(QUrl::fromLocalFile(url));
}

void MainWindow::on_fileActionReload_triggered()
{
  auto guard = scopedSetCurrentOutput();
  if (checkEditorModified()) {
    fileChangedOnDisk();                  // force cached autoReloadId to update
    (void)tabManager->refreshDocument();  // ignore errors opening the file
  }
}

void MainWindow::on_editActionCopyVPT_triggered()
{
  const auto vpt = qglview->cam.getVpt();
  const QString txt =
    QString("[ %1, %2, %3 ]").arg(vpt.x(), 0, 'f', 2).arg(vpt.y(), 0, 'f', 2).arg(vpt.z(), 0, 'f', 2);
  QApplication::clipboard()->setText(txt);
}

void MainWindow::on_editActionCopyVPR_triggered()
{
  const auto vpr = qglview->cam.getVpr();
  const QString txt =
    QString("[ %1, %2, %3 ]").arg(vpr.x(), 0, 'f', 2).arg(vpr.y(), 0, 'f', 2).arg(vpr.z(), 0, 'f', 2);
  QApplication::clipboard()->setText(txt);
}

void MainWindow::on_editActionCopyVPD_triggered()
{
  const QString txt = QString::number(qglview->cam.zoomValue(), 'f', 2);
  QApplication::clipboard()->setText(txt);
}

void MainWindow::on_editActionCopyVPF_triggered()
{
  const QString txt = QString::number(qglview->cam.fovValue(), 'f', 2);
  QApplication::clipboard()->setText(txt);
}

QList<double> MainWindow::getTranslation() const
{
  QList<double> ret;
  ret.append(qglview->cam.object_trans.x());
  ret.append(qglview->cam.object_trans.y());
  ret.append(qglview->cam.object_trans.z());
  return ret;
}

QList<double> MainWindow::getRotation() const
{
  QList<double> ret;
  ret.append(qglview->cam.object_rot.x());
  ret.append(qglview->cam.object_rot.y());
  ret.append(qglview->cam.object_rot.z());
  return ret;
}

void MainWindow::hideFind()
{
  find_panel->hide();
  if (activeEditor) {
    activeEditor->findState = TabManager::FIND_HIDDEN;
  }
  editActionFindNext->setEnabled(false);
  editActionFindPrevious->setEnabled(false);
  const int findCount =
    (activeEditor) ? activeEditor->updateFindIndicators(this->findInputField->text(), false) : 0;
  this->findInputField->setFindCount(findCount);
  this->processEvents();
}

// Prepare the UI for the find (and replace if requested)
// Among other thing it makes the text field and replacement field visible and well as it configures the
// activeEditor to appropriate search mode.
void MainWindow::showFind(bool doFindAndReplace)
{
  findInputField->setFindCount(activeEditor->updateFindIndicators(findInputField->text()));
  processEvents();

  if (doFindAndReplace) {
    findTypeComboBox->setCurrentIndex(1);
    replaceInputField->show();
    replaceButton->show();
    replaceAllButton->show();
    activeEditor->findState = TabManager::FIND_REPLACE_VISIBLE;
  } else {
    findTypeComboBox->setCurrentIndex(0);
    replaceInputField->hide();
    replaceButton->hide();
    replaceAllButton->hide();
    activeEditor->findState = TabManager::FIND_VISIBLE;
  }

  find_panel->show();
  editActionFindNext->setEnabled(true);
  editActionFindPrevious->setEnabled(true);
  if (!activeEditor->selectedText().isEmpty()) {
    findInputField->setText(activeEditor->selectedText());
  }
  findInputField->setFocus();
  findInputField->selectAll();
}

void MainWindow::on_editActionFind_triggered()
{
  showFind(false);
}

void MainWindow::findString(const QString& textToFind)
{
  if (!activeEditor) return;

  this->findInputField->setFindCount(activeEditor->updateFindIndicators(textToFind));
  this->processEvents();
  activeEditor->find(textToFind);
}

void MainWindow::on_editActionFindAndReplace_triggered()
{
  showFind(true);
}

void MainWindow::actionSelectFind(int type)
{
  // If type is one, then we shows the find and replace UI component
  showFind(type == 1);
}

void MainWindow::replace()
{
  activeEditor->replaceSelectedText(this->replaceInputField->text());
  activeEditor->find(this->findInputField->text());
}

void MainWindow::replaceAll()
{
  activeEditor->replaceAll(this->findInputField->text(), this->replaceInputField->text());
}

void MainWindow::on_editActionConvertTabsToSpaces_triggered()
{
  const auto text = activeEditor->toPlainText();

  QString converted;

  int cnt = 4;
  for (auto c : text) {
    if (c == '\t') {
      for (; cnt > 0; cnt--) {
        converted.append(' ');
      }
    } else {
      converted.append(c);
    }
    if (cnt <= 0 || c == '\n') {
      cnt = 5;
    }
    cnt--;
  }
  activeEditor->setText(converted);
}

void MainWindow::on_editActionFindNext_triggered()
{
  activeEditor->find(this->findInputField->text(), true);
}

void MainWindow::on_editActionFindPrevious_triggered()
{
  activeEditor->find(this->findInputField->text(), true, true);
}

void MainWindow::on_editActionUseSelectionForFind_triggered()
{
  findInputField->setText(activeEditor->selectedText());
}

void MainWindow::updateFindBuffer(const QString& s)
{
  QApplication::clipboard()->setText(s, QClipboard::FindBuffer);
}

void MainWindow::findBufferChanged()
{
  auto t = QApplication::clipboard()->text(QClipboard::FindBuffer);
  // The convention seems to be to not update the search field if the findbuffer is empty
  if (!t.isEmpty()) {
    findInputField->setText(t);
  }
}

bool MainWindow::event(QEvent *event)
{
  if (event->type() == InputEvent::eventType) {
    auto *inputEvent = dynamic_cast<InputEvent *>(event);
    if (inputEvent) {
      inputEvent->deliver(this);
    }
    event->accept();
    return true;
  }
  return QMainWindow::event(event);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
  if (rubberBandManager.isVisible()) {
    if (event->type() == QEvent::KeyRelease) {
      auto keyEvent = static_cast<QKeyEvent *>(event);
      if (keyEvent->key() == Qt::Key_Control) {
        rubberBandManager.hide();
      }
    }
  }

  if (obj == find_panel) {
    if (event->type() == QEvent::KeyPress) {
      auto keyEvent = static_cast<QKeyEvent *>(event);
      if (keyEvent->key() == Qt::Key_Escape) {
        this->hideFind();
        return true;
      }
    }
    return false;
  }

  return QMainWindow::eventFilter(obj, event);
}

void MainWindow::setRenderVariables(ContextHandle<BuiltinContext>& context)
{
  RenderVariables r = {
    .preview = this->isPreview,
    .time = this->animateWidget->getAnimTval(),
    .camera = qglview->cam,
  };
  r.viewPath = this->activeViewPath;
  r.applyToContext(context);
}

/*!
   Returns true if the current document is a file on disk and that file has new content.
   Returns false if a file on disk has disappeared or if we haven't yet saved.
 */
bool MainWindow::fileChangedOnDisk()
{
  if (!activeEditor->filepath.isEmpty()) {
    struct stat st;
    memset(&st, 0, sizeof(struct stat));
    const bool valid = (stat(activeEditor->filepath.toLocal8Bit(), &st) == 0);
    // If file isn't there, just return and use current editor text
    if (!valid) return false;

    auto newid = str(boost::format("%x.%x") % st.st_mtime % st.st_size);
    if (newid != activeEditor->autoReloadId) {
      activeEditor->autoReloadId = newid;
      return true;
    }
  }
  return false;
}

/*!
   Returns true if anything was compiled.
 */

#ifdef ENABLE_PYTHON
bool MainWindow::trust_python_file(const std::string& file, const std::string& content)
{
  QSettingsCached settings;
  char setting_key[256];
  if (python_trusted) return true;

  std::string act_hash, ref_hash;
  snprintf(setting_key, sizeof(setting_key) - 1, "python_hash/%s", file.c_str());
  act_hash = SHA256HashString(content);

  if (file == this->untrusted_edit_document_name) return false;

  if (file == this->trusted_edit_document_name) {
    settings.setValue(setting_key, act_hash.c_str());
    return true;
  }

  if (content.size() <= 1) {  // 1st character already typed
    this->trusted_edit_document_name = file;
    return true;
  }
  if (content.rfind("from openscad import", 0) == 0) {  // 1st character already typed
    this->trusted_edit_document_name = file;
    return true;
  }

  if (settings.contains(setting_key)) {
    ref_hash = settings.value(setting_key).toString().toStdString();
  }

  if (act_hash == ref_hash) {
    this->trusted_edit_document_name = file;
    return true;
  }

  auto ret = QMessageBox::warning(this, "Application",
                                  _("Python files can potentially contain harmful stuff.\n"
                                    "Do you trust this file ?\n"),
                                  QMessageBox::Yes | QMessageBox::YesAll | QMessageBox::No);
  if (ret == QMessageBox::YesAll) {
    python_trusted = true;
    return true;
  }
  if (ret == QMessageBox::Yes) {
    this->trusted_edit_document_name = file;
    settings.setValue(setting_key, act_hash.c_str());
    return true;
  }

  if (ret == QMessageBox::No) {
    this->untrusted_edit_document_name = file;
    return false;
  }
  return false;
}
#endif  // ifdef ENABLE_PYTHON

std::shared_ptr<SourceFile> MainWindow::parseDocument(EditorInterface *editor)
{
  resetSuppressedMessages();

  auto document = editor->toPlainText();
  auto fulltext = std::string(document.toUtf8().constData()) + "\n\x03\n" + commandline_commands;

  const std::string fname = editor->filepath.isEmpty() ? "" : editor->filepath.toStdString();
#ifdef ENABLE_PYTHON
  this->python_active = false;
  if (boost::algorithm::ends_with(fname, ".py")) {
    std::string content = std::string(this->lastCompiledDoc.toUtf8().constData());
    if (Feature::ExperimentalPythonEngine.is_enabled() && trust_python_file(fname, content))
      this->python_active = true;
    else LOG(message_group::Warning, Location::NONE, "", "Python is not enabled");
  }

  if (this->python_active) {
    auto fulltext_py = std::string(this->lastCompiledDoc.toUtf8().constData());

    const auto& venv = venvBinDirFromSettings();
    initPython(venv, this->animateWidget->getAnimTval());

    if (venv.empty()) {
      LOG("Running %1$s without venv.", python_version());
    } else {
      const auto& v = Settings::SettingsPython::pythonVirtualEnv.value();
      LOG("Running %1$s in venv '%2$s'.", python_version(), v);
    }
    auto error = evaluatePython(fulltext_py, false);
    if (error.size() > 0) LOG(message_group::Error, Location::NONE, "", error.c_str());
    fulltext = "\n";
  }
#endif  // ifdef ENABLE_PYTHON

  SourceFile *sourceFile;
  sourceFile = parse(sourceFile, fulltext, fname, fname, false) ? sourceFile : nullptr;

  editor->resetHighlighting();
  if (sourceFile) {
    // add parameters as annotation in AST
    CommentParser::collectParameters(fulltext, sourceFile);
    editor->parameterWidget->setParameters(sourceFile, fulltext);
    editor->parameterWidget->applyParameters(sourceFile);
    editor->parameterWidget->setEnabled(true);
    editor->setIndicator(sourceFile->indicatorData);
  } else {
    editor->parameterWidget->setEnabled(false);
  }

  return std::shared_ptr<SourceFile>(sourceFile);
}

void MainWindow::parseTopLevelDocument()
{
  resetSuppressedMessages();

  this->lastCompiledDoc = activeEditor->toPlainText();

  activeEditor->resetHighlighting();
  this->rootFile = parseDocument(activeEditor);
  this->parsedFile = this->rootFile;
}

void MainWindow::checkAutoReload()
{
  if (!activeEditor->filepath.isEmpty()) {
    actionReloadRenderPreview();
  }
}

void MainWindow::on_designActionAutoReload_toggled(bool on)
{
  QSettingsCached settings;
  settings.setValue("design/autoReload", designActionAutoReload->isChecked());
  if (on) {
    autoReloadTimer->start(autoReloadPollingPeriodMS);
  } else {
    autoReloadTimer->stop();
  }
}

bool MainWindow::checkEditorModified()
{
  if (activeEditor->isContentModified()) {
    auto ret = QMessageBox::warning(this, _("Application"),
                                    _("The document has been modified.\n"
                                      "Do you really want to reload the file?"),
                                    QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) {
      return false;
    }
  }
  return true;
}

void MainWindow::on_designActionReloadAndPreview_triggered()
{
  actionReloadRenderPreview();
}

void MainWindow::actionReloadRenderPreview()
{
  if (GuiLocker::isLocked()) return;
  GuiLocker::lock();
  autoReloadTimer->stop();
  setCurrentOutput();

  this->afterCompileSlot = "csgReloadRender";
  this->procevents = true;
  this->isPreview = true;
  compile(true);
}

void MainWindow::csgReloadRender()
{
  if (this->rootNode) compileCSG();

  // Go to non-CGAL view mode
  if (viewActionThrownTogether->isChecked()) {
    viewModeThrownTogether();
  } else {
#ifdef ENABLE_OPENCSG
    viewModePreview();
#else
    viewModeThrownTogether();
#endif
  }
  compileEnded();
}

void MainWindow::prepareCompile(const char *afterCompileSlot, bool procevents, bool preview)
{
  setCurrentOutput();
  autoReloadTimer->stop();
  LOG(" ");
  LOG("Parsing design (AST generation)...");
  this->processEvents();
  this->afterCompileSlot = afterCompileSlot;
  this->procevents = procevents;
  this->isPreview = preview;
}

void MainWindow::on_designActionPreview_triggered()
{
  actionRenderPreview();
}

void MainWindow::actionRenderPreview()
{
  static bool preview_requested;
  preview_requested = true;

  if (GuiLocker::isLocked()) return;

  GuiLocker::lock();
  preview_requested = false;

  resetMeasurementsState(false, "Render (not preview) to enable measurements");

  prepareCompile("csgRender", !animateDock->isVisible(), true);
  compile(false, false);

  if (preview_requested) {
    // if the action was called when the gui was locked, we must request it one more time
    // however, it's not possible to call it directly NOR make the loop
    // it must be called from the mainloop
    QTimer::singleShot(0, this, &MainWindow::actionRenderPreview);
    return;
  }
}

void MainWindow::csgRender()
{
  if (this->rootNode) compileCSG();

  // Go to non-CGAL view mode
  if (viewActionThrownTogether->isChecked()) {
    viewModeThrownTogether();
  } else {
#ifdef ENABLE_OPENCSG
    viewModePreview();
#else
    viewModeThrownTogether();
#endif
  }

  if (animateWidget->dumpPictures()) {
    const int steps = animateWidget->nextFrame();
    const QImage img = this->qglview->grabFrame();
    const QString filename = QString("frame%1.png").arg(steps, 5, 10, QChar('0'));
    img.save(filename, "PNG");
  }

  refreshCameras();
  refreshDrawingViews();
  refreshViewSwitcher();

  compileEnded();
}

void MainWindow::sendToExternalTool(ExternalToolInterface& externalToolService)
{
  const QFileInfo activeFile(activeEditor->filepath);
  QString activeFileName = activeFile.fileName();
  if (activeFileName.isEmpty()) activeFileName = "Untitled.scad";
  // TODO: Replace suffix to match exported file format?

  activeFileName = activeFileName +
                   QString::fromStdString("." + fileformat::toSuffix(externalToolService.fileFormat()));

  const bool export_status =
    externalToolService.exportTemporaryFile(rootGeom, activeFileName, &qglview->cam);
  if (!export_status) {
    return;
  }

  this->progresswidget = new ProgressWidget(this);
  connect(this->progresswidget, &ProgressWidget::requestShow, this, &MainWindow::showProgress);

  const bool process_status = externalToolService.process(
    activeFileName.toStdString(), [this](double permille) { return network_progress_func(permille); });
  updateStatusBar(nullptr);
  if (!process_status) {
    return;
  }

  const auto url = externalToolService.getURL();
  if (!url.empty()) {
    QDesktopServices::openUrl(QUrl{QString::fromStdString(url)});
  }
}

void MainWindow::on_designAction3DPrint_triggered()
{
  if (GuiLocker::isLocked()) return;
  const GuiLocker lock;

  // Make sure we can export:
  const unsigned int dim = 3;
  if (!canExport(dim)) return;

  PrintInitDialog printInitDialog;
  const auto status = printInitDialog.exec();

  if (status == QDialog::Accepted) {
    auto guard = scopedSetCurrentOutput();
    const print_service_t serviceType = printInitDialog.getServiceType();
    const QString serviceName = printInitDialog.getServiceName();
    const FileFormat fileFormat = printInitDialog.getFileFormat();

    LOG("Selected File format: %1$s", fileformat::info(fileFormat).description);

    GlobalPreferences::inst()->updateGUI();
    const auto externalToolService = createExternalToolService(serviceType, serviceName, fileFormat);
    if (!externalToolService) {
      LOG("Error: Unable to create service: %1$d %2$s %3$d", static_cast<int>(serviceType),
          serviceName.toStdString(), static_cast<int>(fileFormat));
      return;
    }
    sendToExternalTool(*externalToolService);
  }
}

void MainWindow::on_designActionRender_triggered()
{
  if (GuiLocker::isLocked()) return;
  GuiLocker::lock();

  prepareCompile("cgalRender", true, false);
  compile(false);
}

void MainWindow::cgalRender()
{
  if (!this->rootFile || !this->rootNode) {
    compileEnded();
    return;
  }

  this->qglview->setRenderer(nullptr);
  this->geomRenderer = nullptr;
  rootGeom.reset();

  LOG("Rendering Polygon Mesh using %1$s...",
      renderBackend3DToString(RenderSettings::inst()->backend3D).c_str());

  this->progresswidget = new ProgressWidget(this);
  connect(this->progresswidget, &ProgressWidget::requestShow, this, &MainWindow::showProgress);

  if (!isClosing) progress_report_prep(this->rootNode, report_func, this);
  else return;

  this->cgalworker->start(this->tree);
}

void MainWindow::actionRenderDone(const std::shared_ptr<const Geometry>& root_geom)
{
#ifdef ENABLE_PYTHON
  python_lock();
#endif
  progress_report_fin();
  if (root_geom) {
    std::vector<std::string> options;
    if (Settings::Settings::summaryCamera.value()) {
      options.emplace_back(RenderStatistic::CAMERA);
    }
    if (Settings::Settings::summaryArea.value()) {
      options.emplace_back(RenderStatistic::AREA);
    }
    if (Settings::Settings::summaryBoundingBox.value()) {
      options.emplace_back(RenderStatistic::BOUNDING_BOX);
    }
    renderStatistic.printAll(root_geom, qglview->cam, options);
    LOG("Rendering finished.");

    this->rootGeom = root_geom;
#if defined(USE_POLYSET_FOR_CGAL)
    this->geomRenderer = std::make_shared<PolySetRenderer>(this->rootGeom);
#else
    // Choose PolySetRenderer for PolySet and Polygon2d, and for Manifold since we
    // know that all geometries are convertible to PolySet.
    if (RenderSettings::inst()->backend3D == RenderBackend3D::ManifoldBackend ||
        std::dynamic_pointer_cast<const PolySet>(this->rootGeom) ||
        std::dynamic_pointer_cast<const Polygon2d>(this->rootGeom)) {
      this->geomRenderer = std::make_shared<PolySetRenderer>(this->rootGeom);
    } else {
      this->geomRenderer = std::make_shared<CGALRenderer>(this->rootGeom);
    }
#endif

    // Go to CGAL view mode
    viewModeRender();
    resetMeasurementsState(true, "Click to start measuring");
  } else {
    resetMeasurementsState(false, "No top level geometry; render something to enable measurements");
    LOG(message_group::UI_Warning, "No top level geometry to render");
  }

  updateStatusBar(nullptr);

  const bool renderSoundEnabled =
    GlobalPreferences::inst()->getValue("advanced/enableSoundNotification").toBool();
  const uint soundThreshold =
    GlobalPreferences::inst()->getValue("advanced/timeThresholdOnRenderCompleteSound").toUInt();
  if (renderSoundEnabled && soundThreshold <= renderStatistic.ms().count() / 1000) {
    renderCompleteSoundEffect->play();
  }

  renderedEditor = activeEditor;
  activeEditor->contentsRendered = true;
  compileEnded();
}

void MainWindow::handleMeasurementClicked(QAction *clickedAction)
{
  // If we're unchecking, just stop.
  if (activeMeasurement == clickedAction) {
    resetMeasurementsState(true, "Click to start measuring");
    return;
  }

  resetMeasurementsState(true, "Click to start measuring");
  clickedAction->setToolTip("Click to cancel measurement");
  clickedAction->setChecked(true);
  activeMeasurement = clickedAction;

  if (clickedAction == designActionMeasureDist) {
    meas.startMeasureDist();
  } else if (clickedAction == designActionMeasureAngle) {
    meas.startMeasureAngle();
  }
}

void MainWindow::leftClick(QPoint mouse)
{
  auto state = meas.statemachine(mouse);
  if (state.status != Measurement::Result::Status::NoChange) {
    this->qglview->measure_state = Measurement::MEASURE_DIRTY;
    QMenu resultmenu(this);
    // Ensures we clean the display regardless of how menu gets closed.
    connect(&resultmenu, &QMenu::aboutToHide, this, &MainWindow::measureFinished);

    // Create the context menu and write successful measurements to the console.
    // boost adaptor can eventually be replaced with C++20 std::views::reverse
    bool first = true;
    for (const auto& msg : boost::adaptors::reverse(state.messages)) {
      auto str = msg.display_text;
      if (state.status == Measurement::Result::Status::Success) {
        if (auto m = make_message_obj(first ? "%1$s" : "  %1$s", str.toStdString())) {
          this->consoleOutput(*m);
        }
      }
      auto action = resultmenu.addAction(str);
      auto clipboard = msg.clipboard_text ? *msg.clipboard_text : str;
      connect(action, &QAction::triggered, this,
              [clipboard]() { QApplication::clipboard()->setText(clipboard); });
      first = false;
    }
    resultmenu.addAction("Click any above to copy its data to the clipboard");
    resultmenu.exec(qglview->mapToGlobal(mouse));
    resetMeasurementsState(true, "Click to start measuring");
  }
}

/**
 * Call the mouseselection to determine the id of the clicked-on object.
 * Use the generated ID and try to find it within the list of products
 * And finally move the cursor to the beginning of the selected object in the editor
 */
void MainWindow::rightClick(QPoint position)
{
  // selecting without a renderer?!
  if (!this->qglview->renderer) {
    return;
  }
  // Nothing to select
  if (!this->rootProduct) {
    return;
  }

  // Select the object at mouse coordinates
  const int index = this->qglview->pickObject(position);
  std::deque<std::shared_ptr<const AbstractNode>> path;
  const std::shared_ptr<const AbstractNode> result = this->rootNode->getNodeByID(index, path);

  if (result) {
    // Create context menu with the backtrace
    QMenu tracemenu(this);
    std::stringstream ss;
    for (auto& step : path) {
      // Skip certain node types
      if (step->name() == "root") {
        continue;
      }
      const bool hasSourceRef = step->modinst && !step->modinst->location().isNone();
      if (!hasSourceRef) {
        // Show an entry so the backtrace stays complete; no jump/highlight (no "id", no hover)
        std::string name;
        if (step->modinst) {
          const std::string vname = step->verbose_name();
          const int first_position = (vname.find("module") == std::string::npos) ? 0 : 7;
          name = vname.empty() ? step->modinst->name() : vname.substr(first_position);
        } else {
          const std::string vname = step->verbose_name();
          const int first_position = (vname.find("module") == std::string::npos) ? 0 : 7;
          name = vname.empty() ? "?" : vname.substr(first_position);
        }
        ss.str("");
        ss << name << " (no source reference)";
        tracemenu.addAction(QString::fromStdString(ss.str()));
        continue;
      }
      auto location = step->modinst->location();
      ss.str("");

      // Remove the "module" prefix if any as it induce confusion between the module declaration and
      // instanciation
      const int first_position = (step->verbose_name().find("module") == std::string::npos) ? 0 : 7;
      std::string name = step->verbose_name().substr(first_position);

      // It happens that the verbose_name is empty (eg: in for loops), when this happens instead of
      // letting empty entry in the menu we prefer using the name in the modinstanciation.
      if (step->verbose_name().empty()) name = step->modinst->name();

      // Check if the path is contained in a library (using parsersettings.h)
      const fs::path libpath = get_library_for_path(location.filePath());
      if (!libpath.empty()) {
        // Display the library (without making the window too wide!)
        ss << name << " (library " << location.fileName().substr(libpath.string().length() + 1) << ":"
           << location.firstLine() << ")";
      } else if (renderedEditor->filepath.toStdString() == location.fileName()) {
        // removes the "module" prefix if any as it makes it not clear if it is module declaration or
        // call.
        ss << name << " (" << location.filePath().filename().string() << ":" << location.firstLine()
           << ")";
      } else {
        auto relative_filename =
          fs_uncomplete(location.filePath(),
                        fs::path(renderedEditor->filepath.toStdString()).parent_path())
            .generic_string();

        // Set the displayed name relative to the active editor window
        ss << name << " (" << relative_filename << ":" << location.firstLine() << ")";
      }
      // Prepare the action to be sent
      auto action = tracemenu.addAction(QString::fromStdString(ss.str()));
      if (editorDock->isVisible()) {
        action->setProperty("id", step->idx);
        connect(action, &QAction::hovered, this, &MainWindow::onHoveredObjectInSelectionMenu);
      }
    }

    // Before starting we need to lock the GUI to avoid interferance with reload/update
    // triggered by other part of the application (eg: changing the renderedEditor)
    GuiLocker::lock();

    // Execute this lambda function when the selection menu is closing.
    connect(&tracemenu, &QMenu::aboutToHide, [this]() {
      // remove the visual hints in the editor
      renderedEditor->clearAllSelectionIndicators();
      // unlock the GUI so the other part of the interface can now be updated.
      // (eg: changing the renderedEditor)
      GuiLocker::unlock();
    });
    tracemenu.exec(this->qglview->mapToGlobal(position));
  } else {
    clearAllSelectionIndicators();
  }
}

void MainWindow::measureFinished()
{
  auto didSomething = meas.stopMeasure();
  if (didSomething) resetMeasurementsState(true, "Click to start measuring");
}

void MainWindow::clearAllSelectionIndicators()
{
  this->activeEditor->clearAllSelectionIndicators();
}

void MainWindow::setSelectionIndicatorStatus(EditorInterface *editor, int nodeIndex,
                                             EditorSelectionIndicatorStatus status)
{
  std::deque<std::shared_ptr<const AbstractNode>> stack;
  this->rootNode->getNodeByID(nodeIndex, stack);

  int level = 1;

  // first we flags all the nodes in the stack of the provided index
  // ends at size - 1 because we are not doing anything for the root node.
  // starts at 1 because we will process this one after later
  for (size_t i = 1; i < stack.size() - 1; i++) {
    const auto& node = stack[i];
    if (!node->modinst || node->modinst->location().isNone()) {
      level++;
      continue;
    }
    auto& location = node->modinst->location();
    if (location.filePath().compare(editor->filepath.toStdString()) != 0) {
      level++;
      continue;
    }

    if (node->verbose_name().rfind("module", 0) == 0 || node->modinst->name() == "children") {
      editor->setSelectionIndicatorStatus(status, level, location.firstLine() - 1,
                                          location.firstColumn() - 1, location.lastLine() - 1,
                                          location.lastColumn() - 1);
      level++;
    }
  }

  auto& node = stack[0];
  if (!node->modinst || node->modinst->location().isNone()) {
    return;
  }
  auto location = node->modinst->location();
  auto line = location.firstLine();
  auto column = location.firstColumn();
  auto lastLine = location.lastLine();
  auto lastColumn = location.lastColumn();

  // Update the location returned by location to cover the whole section.
  node->getCodeLocation(0, 0, &line, &column, &lastLine, &lastColumn, 0);

  editor->setSelectionIndicatorStatus(status, 0, line - 1, column - 1, lastLine - 1, lastColumn - 1);
}

void MainWindow::setSelection(int index)
{
  assert(renderedEditor != nullptr);
  if (currentlySelectedObject == index) return;

  std::deque<std::shared_ptr<const AbstractNode>> path;
  const std::shared_ptr<const AbstractNode> selected_node = rootNode->getNodeByID(index, path);

  if (!selected_node) return;
  if (!selected_node->modinst || selected_node->modinst->location().isNone()) {
    return;
  }

  currentlySelectedObject = index;

  auto location = selected_node->modinst->location();
  auto file = location.fileName();
  auto line = location.firstLine();
  auto column = location.firstColumn();

  // Unsaved files do have the pwd as current path, therefore we will not open a new
  // tab on click
  if (!fs::is_directory(fs::path(file))) {
    tabManager->open(QString::fromStdString(file));
  }

  // removes all previsly configure selection indicators.
  renderedEditor->clearAllSelectionIndicators();
  renderedEditor->show();

  std::vector<std::shared_ptr<const AbstractNode>> nodesSameModule{};
  rootNode->findNodesWithSameMod(selected_node, nodesSameModule);

  // highlight in the text editor all the text fragment of the hierarchy of object with same mode.
  for (const auto& element : nodesSameModule) {
    if (element->index() != currentlySelectedObject) {
      setSelectionIndicatorStatus(renderedEditor, element->index(),
                                  EditorSelectionIndicatorStatus::IMPACTED);
    }
  }

  // highlight in the text editor only the fragment correponding to the selected stack.
  // this step must be done after all the impacted element have been marked.
  setSelectionIndicatorStatus(renderedEditor, currentlySelectedObject,
                              EditorSelectionIndicatorStatus::SELECTED);

  renderedEditor->setCursorPosition(line - 1, column - 1);
}

/**
 * Expects the sender to have properties "id" defined
 */
void MainWindow::onHoveredObjectInSelectionMenu()
{
  assert(renderedEditor != nullptr);
  auto *action = qobject_cast<QAction *>(sender());
  if (!action || !action->property("id").isValid()) {
    return;
  }

  setSelection(action->property("id").toInt());
}

void MainWindow::setLastFocus(QWidget *widget)
{
  this->lastFocus = widget;
}

/**
 * Switch version label and progress widget. When switching to the progress
 * widget, the new instance is passed by the caller.
 * In case of resetting back to the version label, nullptr will be passed and
 * multiple calls can happen. So this method must guard against adding the
 * version label multiple times.
 *
 * @param progressWidget a pointer to the progress widget to show or nullptr in
 * case the display should switch back to the version label.
 */
void MainWindow::updateStatusBar(ProgressWidget *progressWidget)
{
  auto sb = this->statusBar();
  if (progressWidget == nullptr) {
    if (this->progresswidget != nullptr) {
      sb->removeWidget(this->progresswidget);
      delete this->progresswidget;
      this->progresswidget = nullptr;
    }
    if (versionLabel == nullptr) {
      versionLabel =
        new QLabel("OpenSCAD " + QString::fromStdString(std::string(openscad_displayversionnumber)));
      sb->addPermanentWidget(this->versionLabel);
    }
  } else {
    if (this->versionLabel != nullptr) {
      sb->removeWidget(this->versionLabel);
      delete this->versionLabel;
      this->versionLabel = nullptr;
    }
    sb->addPermanentWidget(progressWidget);
  }
}

void MainWindow::exceptionCleanup()
{
  LOG("Execution aborted");
  LOG(" ");
  GuiLocker::unlock();
  if (designActionAutoReload->isChecked()) autoReloadTimer->start();
}

void MainWindow::UnknownExceptionCleanup(std::string msg)
{
  auto guard = scopedSetCurrentOutput();  // we need to show this error
  if (msg.size() == 0) {
    LOG(message_group::Error, "Compilation aborted by unknown exception");
  } else {
    LOG(message_group::Error, "Compilation aborted by exception: %1$s", msg);
  }
  LOG(" ");
  GuiLocker::unlock();
  if (designActionAutoReload->isChecked()) autoReloadTimer->start();
}

void MainWindow::showTextInWindow(const QString& type, const QString& content)
{
  auto e = new QTextEdit(this);
  e->setAttribute(Qt::WA_DeleteOnClose);
  e->setWindowFlags(Qt::Window);
  e->setTabStopDistance(tabStopWidth);
  e->setWindowTitle(type + " Dump");
  if (content.isEmpty()) e->setPlainText("No " + type + " to dump. Please try compiling first...");
  else e->setPlainText(content);

  e->setReadOnly(true);
  e->resize(600, 400);
  e->show();
}

void MainWindow::on_designActionDisplayAST_triggered()
{
  auto guard = scopedSetCurrentOutput();
  QString text = (rootFile) ? QString::fromStdString(rootFile->dump("")) : "";
  showTextInWindow("AST", text);
}

void MainWindow::on_designActionDisplayCSGTree_triggered()
{
  auto guard = scopedSetCurrentOutput();
  QString text = (rootNode) ? QString::fromStdString(tree.getString(*rootNode, "  ")) : "";
  showTextInWindow("CSG", text);
}

void MainWindow::on_designActionDisplayCSGProducts_triggered()
{
  auto guard = scopedSetCurrentOutput();
  // a small lambda to avoid code duplication
  auto constexpr dump = [](auto node) { return QString::fromStdString(node ? node->dump() : "N/A"); };
  auto text =
    QString(
      "\nCSG before normalization:\n%1\n\n\nCSG after normalization:\n%2\n\n\nCSG rendering "
      "chain:\n%3\n\n\nHighlights CSG rendering chain:\n%4\n\n\nBackground CSG rendering chain:\n%5\n")
      .arg(dump(csgRoot), dump(normalizedRoot), dump(rootProduct), dump(highlightsProducts),
           dump(backgroundProducts));
  showTextInWindow("CSG Products Dump", text);
}

void MainWindow::on_designCheckValidity_triggered()
{
  if (GuiLocker::isLocked()) return;
  const GuiLocker lock;
  auto guard = scopedSetCurrentOutput();

  if (!rootGeom) {
    LOG("Nothing to validate! Try building first (press F6).");
    return;
  }

  if (rootGeom->getDimension() != 3) {
    LOG("Current top level object is not a 3D object.");
    return;
  }

  bool valid = true;
#ifdef ENABLE_CGAL
  if (auto N = std::dynamic_pointer_cast<const CGALNefGeometry>(rootGeom)) {
    valid = N->p3 ? const_cast<CGAL_Nef_polyhedron3&>(*N->p3).is_valid() : false;
  } else
#endif
#ifdef ENABLE_MANIFOLD
    if (auto mani = std::dynamic_pointer_cast<const ManifoldGeometry>(rootGeom)) {
    valid = mani->isValid();
  }
#endif
  LOG("Valid:      %1$6s", (valid ? "yes" : "no"));
}

// Returns if we can export (true) or not(false) (bool)
// Separated into it's own function for re-use.
bool MainWindow::canExport(unsigned int dim)
{
  auto guard = scopedSetCurrentOutput();
  if (!rootGeom) {
    LOG(message_group::Error, "Nothing to export! Try rendering first (press F6)");
    return false;
  }

  // editor has changed since last render
  if (!activeEditor->contentsRendered) {
    auto ret = QMessageBox::warning(this, "Application",
                                    "The current tab has been modified since its last render (F6).\n"
                                    "Do you really want to export the previous content?",
                                    QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) {
      return false;
    }
  }

  // other tab contents most recently rendered
  if (renderedEditor != activeEditor) {
    auto ret = QMessageBox::warning(this, "Application",
                                    "The rendered data is of different tab.\n"
                                    "Do you really want to export the another tab's content?",
                                    QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) {
      return false;
    }
  }

  if (rootGeom->getDimension() != dim) {
    LOG(message_group::UI_Error, "Current top level object is not a %1$dD object.", dim);
    return false;
  }

  if (rootGeom->isEmpty()) {
    LOG(message_group::UI_Error, "Current top level object is empty.");
    return false;
  }

#ifdef ENABLE_CGAL
  auto N = dynamic_cast<const CGALNefGeometry *>(rootGeom.get());
  if (N && !N->p3->is_simple()) {
    LOG(message_group::UI_Warning,
        "Object may not be a valid 2-manifold and may need repair! See "
        "https://en.wikibooks.org/wiki/OpenSCAD_User_Manual/STL_Import_and_Export");
  }
#endif
#ifdef ENABLE_MANIFOLD
  auto manifold = dynamic_cast<const ManifoldGeometry *>(rootGeom.get());
  if (manifold && !manifold->isValid()) {
    LOG(message_group::UI_Warning,
        "Object may not be a valid manifold and may need repair! "
        "Error message: %1$s. See "
        "https://en.wikibooks.org/wiki/OpenSCAD_User_Manual/STL_Import_and_Export",
        ManifoldUtils::statusToString(manifold->getManifold().Status()));
  }
#endif

  return true;
}

void MainWindow::actionExport(unsigned int dim, ExportInfo& exportInfo)
{
  const auto type_name = QString::fromStdString(exportInfo.info.description);
  const auto suffix = QString::fromStdString(exportInfo.info.suffix);

  // Setting filename skips the file selection dialog and uses the path provided instead.
  if (GuiLocker::isLocked()) return;
  const GuiLocker lock;

  auto guard = scopedSetCurrentOutput();

  // Return if something is wrong and we can't export.
  if (!canExport(dim)) return;

  auto title = QString(_("Export %1 File")).arg(type_name);
  auto filter = QString(_("%1 Files (*%2)")).arg(type_name, suffix);
  auto exportFilename = QFileDialog::getSaveFileName(this, title, exportPath(suffix), filter);
  auto guard2 = scopedSetCurrentOutput();
  if (exportFilename.isEmpty()) {
    return;
  }
  this->exportPaths[suffix] = exportFilename;

  const bool exportResult = exportFileByName(rootGeom, exportFilename.toStdString(), exportInfo);

  if (exportResult) fileExportedMessage(type_name, exportFilename);
}

void MainWindow::actionExportFileFormat(int fmt)
{
  const auto format = static_cast<FileFormat>(fmt);
  const FileFormatInfo& info = fileformat::info(format);

  ExportInfo exportInfo =
    createExportInfo(format, info, activeEditor->filepath.toStdString(), &qglview->cam, {});

  switch (format) {
  case FileFormat::PDF: {
    ExportPdfDialog exportPdfDialog;
    if (exportPdfDialog.exec() == QDialog::Rejected) {
      return;
    }

    exportInfo.optionsPdf = exportPdfDialog.getOptions();
    actionExport(2, exportInfo);
  } break;
  case FileFormat::_3MF: {
    Export3mfDialog export3mfDialog;
    if (export3mfDialog.exec() == QDialog::Rejected) {
      return;
    }

    exportInfo.options3mf = export3mfDialog.getOptions();
    actionExport(3, exportInfo);
  } break;
  case FileFormat::CSG: {
    auto guard = scopedSetCurrentOutput();

    if (!this->rootNode) {
      LOG(message_group::Error, "Nothing to export. Please try compiling first.");
      return;
    }
    const QString suffix = "csg";
    auto csg_filename = QFileDialog::getSaveFileName(this, _("Export CSG File"), exportPath(suffix),
                                                     _("CSG Files (*.csg)"));

    if (csg_filename.isEmpty()) {
      return;
    }

    auto guard2 = scopedSetCurrentOutput();
    std::ofstream fstream(std::filesystem::u8path(csg_filename.toStdString()));
    if (!fstream.is_open()) {
      LOG("Can't open file \"%1$s\" for export", csg_filename.toStdString());
    } else {
      fstream << this->tree.getString(*this->rootNode, "\t") << "\n";
      fstream.close();
      fileExportedMessage("CSG", csg_filename);
      this->exportPaths[suffix] = csg_filename;
    }

  } break;
  case FileFormat::PNG: {
    // Grab first to make sure dialog box isn't part of the grabbed image
    qglview->grabFrame();
    const QString suffix = "png";
    auto img_filename =
      QFileDialog::getSaveFileName(this, _("Export Image"), exportPath(suffix), _("PNG Files (*.png)"));
    if (!img_filename.isEmpty()) {
      const bool saveResult = qglview->save(img_filename.toStdString().c_str());
      if (saveResult) {
        this->exportPaths[suffix] = img_filename;
        auto guard = scopedSetCurrentOutput();
        fileExportedMessage("PNG", img_filename);
      } else {
        LOG("Can't open file \"%1$s\" for export image", img_filename.toStdString());
      }
    }
  } break;
  case FileFormat::SVG: {
    ExportSvgDialog exportSvgDialog;
    if (exportSvgDialog.exec() == QDialog::Rejected) {
      return;
    }
    exportInfo.optionsSvg = std::make_shared<ExportSvgOptions>(exportSvgDialog.getOptions());
    actionExport(2, exportInfo);
  } break;
  default: actionExport(fileformat::is3D(format) ? 3 : fileformat::is2D(format) ? 2 : 0, exportInfo);
  }
}

void MainWindow::on_editActionCopy_triggered()
{
  auto *c = dynamic_cast<Console *>(lastFocus);
  if (c) {
    c->copy();
  } else {
    tabManager->copy();
  }
}

void MainWindow::on_editActionCopyViewport_triggered()
{
  const auto& image = qglview->grabFrame();
  auto clipboard = QApplication::clipboard();
  clipboard->setImage(image);
}

void MainWindow::on_designActionFlushCaches_triggered()
{
  auto guard = scopedSetCurrentOutput();
  GeometryCache::instance()->clear();
  CGALCache::instance()->clear();
  dxf_dim_cache.clear();
  dxf_cross_cache.clear();
  SourceFileCache::instance()->clear();

  LOG("Caches Flushed");
}

void MainWindow::viewModeActionsUncheck()
{
  previewModeGroup->setEnabled(false);
}

#ifdef ENABLE_OPENCSG

void MainWindow::viewModeRender()
{
  previewModeGroup->setEnabled(false);
  this->qglview->setRenderer(this->geomRenderer);
  // Frustum/dimref markers are a preview aid; hide them in the rendered (F6) view.
  this->qglview->setShowCameraFrustums(false);
  this->qglview->setShowDimrefMarkers(false);
  this->qglview->updateColorScheme();
  this->qglview->update();
}

/*!
   Go to the OpenCSG view mode.
   Falls back to thrown together mode if OpenCSG is not available
 */
void MainWindow::on_viewActionPreview_triggered()
{
  viewModePreview();
}

void MainWindow::viewModePreview()
{
  previewModeGroup->setEnabled(true);
  if (this->qglview->hasOpenCSGSupport()) {
    viewActionPreview->setChecked(true);
    this->qglview->setRenderer(this->previewRenderer ? this->previewRenderer
                                                     : this->thrownTogetherRenderer);
    this->qglview->setShowCameraFrustums(true);
    this->qglview->setShowDimrefMarkers(true);
    this->qglview->updateColorScheme();
    this->qglview->update();
  } else {
    viewModeThrownTogether();
  }
}

#endif /* ENABLE_OPENCSG */

void MainWindow::updateViewModeAfterGLInit()
{
#ifdef ENABLE_OPENCSG
  viewActionPreview->setEnabled(this->qglview->hasOpenCSGSupport());
  if (this->qglview->hasOpenCSGSupport()) {
    viewModePreview();
  }
#endif
}

void MainWindow::on_viewActionThrownTogether_triggered()
{
  viewModeThrownTogether();
}

void MainWindow::viewModeThrownTogether()
{
  previewModeGroup->setEnabled(true);
  viewActionThrownTogether->setChecked(true);
  this->qglview->setRenderer(this->thrownTogetherRenderer);
  this->qglview->setShowCameraFrustums(true);
  this->qglview->setShowDimrefMarkers(true);
  this->qglview->updateColorScheme();
  this->qglview->update();
}

void MainWindow::on_viewActionShowEdges_toggled(bool checked)
{
  QSettingsCached settings;
  settings.setValue("view/showEdges", checked);
  this->qglview->setShowEdges(checked);
  this->qglview->update();
}

void MainWindow::on_viewActionShowAxes_toggled(bool checked)
{
  QSettingsCached settings;
  settings.setValue("view/showAxes", checked);
  this->viewActionShowScaleProportional->setEnabled(checked);
  this->qglview->setShowAxes(checked);
  this->qglview->update();
}

void MainWindow::on_viewActionShowCrosshairs_toggled(bool checked)
{
  QSettingsCached settings;
  settings.setValue("view/showCrosshairs", checked);
  this->qglview->setShowCrosshairs(checked);
  this->qglview->update();
}

void MainWindow::on_viewActionShowScaleProportional_toggled(bool checked)
{
  QSettingsCached settings;
  settings.setValue("view/showScaleProportional", checked);
  this->qglview->setShowScaleProportional(checked);
  this->qglview->update();
}

bool MainWindow::isEmpty()
{
  return activeEditor->toPlainText().isEmpty();
}

void MainWindow::editorContentChanged()
{
  // this slot is called when the content of the active editor changed.
  // it rely on the activeEditor member to pick the new data.

  auto current_doc = activeEditor->toPlainText();
  if (current_doc != lastCompiledDoc) {
    animateWidget->editorContentChanged();

    // removes the live selection feedbacks in both the 3d view and editor.
    clearAllSelectionIndicators();
  }
}

void MainWindow::on_viewActionTop_triggered()
{
  qglview->cam.object_rot << 90, 0, 0;
  this->qglview->update();
}

void MainWindow::on_viewActionBottom_triggered()
{
  qglview->cam.object_rot << 270, 0, 0;
  this->qglview->update();
}

void MainWindow::on_viewActionLeft_triggered()
{
  qglview->cam.object_rot << 0, 0, 90;
  this->qglview->update();
}

void MainWindow::on_viewActionRight_triggered()
{
  qglview->cam.object_rot << 0, 0, 270;
  this->qglview->update();
}

void MainWindow::on_viewActionFront_triggered()
{
  qglview->cam.object_rot << 0, 0, 0;
  this->qglview->update();
}

void MainWindow::on_viewActionBack_triggered()
{
  qglview->cam.object_rot << 0, 0, 180;
  this->qglview->update();
}

void MainWindow::on_viewActionDiagonal_triggered()
{
  qglview->cam.object_rot << 35, 0, -25;
  this->qglview->update();
}

void MainWindow::on_viewActionCenter_triggered()
{
  qglview->cam.object_trans << 0, 0, 0;
  this->qglview->update();
}

void MainWindow::setProjectionType(ProjectionType mode)
{
  const bool isOrthogonal = ProjectionType::ORTHOGONAL == mode;
  QSettingsCached settings;
  settings.setValue("view/orthogonalProjection", isOrthogonal);
  qglview->setOrthoMode(isOrthogonal);
  qglview->update();
}

void MainWindow::on_viewActionPerspective_toggled(bool checked)
{
  if (checked) {
    setProjectionType(ProjectionType::PERSPECTIVE);
  }
}

void MainWindow::on_viewActionOrthogonal_toggled(bool checked)
{
  if (checked) {
    setProjectionType(ProjectionType::ORTHOGONAL);
  }
}

void MainWindow::viewTogglePerspective()
{
  const QSettingsCached settings;
  const bool isOrthogonal = settings.value("view/orthogonalProjection").toBool();
  if (isOrthogonal) {
    viewActionOrthogonal->setChecked(true);
  } else {
    viewActionPerspective->setChecked(true);
  }
}

void MainWindow::on_viewActionResetView_triggered()
{
  this->qglview->resetView();
  this->qglview->update();
}

void MainWindow::on_viewActionViewAll_triggered()
{
  this->qglview->viewAll();
  this->qglview->update();
}

void MainWindow::on_viewActionHideEditorToolBar_toggled(bool checked)
{
  QSettingsCached settings;
  settings.setValue("view/hideEditorToolbar", checked);
  if (checked) {
    editortoolbar->hide();
  } else {
    editortoolbar->show();
  }
}

void MainWindow::on_viewActionHide3DViewToolBar_toggled(bool checked)
{
  QSettingsCached settings;
  settings.setValue("view/hide3DViewToolbar", checked);

  if (checked) {
    viewerToolBar->hide();
  } else {
    viewerToolBar->show();
  }
}

void MainWindow::showLink(const QString& link)
{
  if (link == "#console") {
    consoleDock->show();
  } else if (link == "#errorlog") {
    errorLogDock->show();
  } else if (link == "#colorlist") {
    colorListDock->show();
  }
}

void MainWindow::onEditorDockVisibilityChanged(bool isVisible)
{
  auto e = (ScintillaEditor *)this->activeEditor;
  if (isVisible) {
    e->qsci->setReadOnly(false);
    e->setupAutoComplete(false);
    editorDock->raise();
    tabManager->setFocus();
  } else {
    // Workaround manually disabling interactions with editor by setting it
    // to read-only when not being shown.  This is an upstream bug from Qt
    // (tracking ticket: https://bugreports.qt.io/browse/QTBUG-82939) and
    // may eventually get resolved at which point this bit and the stuff in
    // the else should be removed. Currently known to affect 5.14.1 and 5.15.0
    e->qsci->setReadOnly(true);
    e->setupAutoComplete(true);
  }
  updateExportActions();
}

void MainWindow::onConsoleDockVisibilityChanged(bool isVisible)
{
  if (isVisible) {
    frameCompileResult->hide();
    consoleDock->raise();
    console->setFocus();
  }
}

void MainWindow::onErrorLogDockVisibilityChanged(bool isVisible)
{
  if (isVisible) {
    frameCompileResult->hide();
    errorLogDock->raise();
    errorLogWidget->logTable->setFocus();
  }
}

void MainWindow::onAnimateDockVisibilityChanged(bool isVisible)
{
  if (isVisible) {
    animateDock->raise();
    animateWidget->setFocus();
  }
}

void MainWindow::onFontListDockVisibilityChanged(bool isVisible)
{
  if (isVisible) {
    fontListWidget->update_font_list();
    fontListWidget->setFocus();
    fontListDock->raise();
  }
}

void MainWindow::onColorListDockVisibilityChanged(bool isVisible)
{
  if (isVisible) {
    colorListWidget->setFocus();
    colorListDock->raise();
  }
}

void MainWindow::onViewportControlDockVisibilityChanged(bool isVisible)
{
  if (isVisible) {
    viewportControlDock->raise();
    viewportControlWidget->setFocus();
  }
}

void MainWindow::onParametersDockVisibilityChanged(bool isVisible)
{
  if (isVisible) {
    parameterDock->raise();
    activeEditor->parameterWidget->scrollArea->setFocus();
  }
}

void MainWindow::onAIDockVisibilityChanged(bool isVisible)
{
}

void MainWindow::onExperimentalChanged()
{
  bool aiEnabled = Feature::ExperimentalAiFeatures.is_enabled();
  if (this->aiDock) {
    this->aiDock->toggleViewAction()->setVisible(aiEnabled);
    if (!aiEnabled) {
      this->aiDock->hide();
    }
    if (this->navigationMenu) {
      for (auto *action : this->navigationMenu->actions()) {
        if (action->text() == _("&AI Chat")) {
          action->setVisible(aiEnabled);
        }
      }
    }
  }
}

void MainWindow::onColorListColorSelected(const QString& selectedColor)
{
  activeEditor->insertOrReplaceText(selectedColor);
}

// Use the sender's to detect if we are moving forward/backward in docks
// and search for the next dock to "activate" or "emphasize"
// If no dock can be found, returns the first one.
Dock *MainWindow::getNextDockFromSender(QObject *sender)
{
  int direction = 0;

  auto *action = qobject_cast<QAction *>(sender);
  if (action != nullptr) {
    direction = (action == windowActionNextWindow) ? 1 : -1;
  } else {
    auto *shortcut = qobject_cast<QShortcut *>(sender);
    direction = (shortcut == shortcutNextWindow) ? 1 : -1;
  }

  return findVisibleDockToActivate(direction);
}

void MainWindow::onWindowActionNextPrevHovered()
{
  auto dock = getNextDockFromSender(sender());

  // This can happens if there is no visible dock at all
  if (dock == nullptr) return;

  // Hover signal is emitted at each mouse move, to avoid excessive
  // load we only raise/emphasize if it is not yet done.
  if (rubberBandManager.isEmphasized(dock)) return;

  dock->raise();
  rubberBandManager.emphasize(dock);
}

void MainWindow::onWindowActionNextPrevTriggered()
{
  auto dock = getNextDockFromSender(sender());

  // This can happens if there is no visible dock at all
  if (dock == nullptr) return;

  activateDock(dock);
}

void MainWindow::onWindowShortcutNextPrevActivated()
{
  auto dock = getNextDockFromSender(sender());

  // This can happens if there is no visible dock at all
  if (dock == nullptr) return;

  activateDock(dock);
  rubberBandManager.emphasize(dock);
}

QAction *MainWindow::formatIdentifierToAction(const std::string& identifier) const
{
  FileFormat format;
  if (fileformat::fromIdentifier(identifier, format)) {
    const auto it = exportMap.find(format);
    if (it != exportMap.end()) {
      return it->second;
    }
  }
  return nullptr;
}

void MainWindow::onWindowShortcutExport3DActivated()
{
  QAction *action = formatIdentifierToAction(Settings::Settings::toolbarExport3D.value());
  if (action) {
    action->trigger();
  }
}

void MainWindow::on_editActionInsertTemplate_triggered()
{
  activeEditor->displayTemplates();
}

void MainWindow::on_editActionFoldAll_triggered()
{
  activeEditor->foldUnfold();
}

QString MainWindow::getCurrentFileName() const
{
  if (activeEditor == nullptr) return {};

  const QFileInfo fileInfo(activeEditor->filepath);
  QString fname = _("Untitled.scad");
  if (!fileInfo.fileName().isEmpty()) fname = fileInfo.fileName();
  return fname.replace("&", "&&");
}

/**
 * Convert a dock title to a base name for action naming.
 * Removes mnemonic markers (&) and hyphens, creating a camelCase name.
 * Examples: "&Editor" -> "Editor", "Error-&Log" -> "ErrorLog"
 */
QString MainWindow::getDockBaseName(const QString& title) const
{
  QString baseName = title;
  // Remove mnemonic marker
  baseName.remove('&');
  // Remove hyphens
  baseName.remove('-');
  // Remove spaces
  baseName.remove(' ');
  return baseName;
}

void MainWindow::onTabManagerAboutToCloseEditor(EditorInterface *closingEditor)
{
  // This slots is in charge of closing properly the preview when the
  // associated editor is about to close.
  if (closingEditor == renderedEditor) {
    renderedEditor = nullptr;

    // Invalidate renderers before we kill the CSG tree
    this->qglview->setRenderer(nullptr);
#ifdef ENABLE_OPENCSG
    this->previewRenderer = nullptr;
#endif
    this->thrownTogetherRenderer = nullptr;

    // Remove previous CSG tree
    this->absoluteRootNode.reset();

    this->csgRoot.reset();
    this->normalizedRoot.reset();
    this->rootProduct.reset();

    this->rootNode.reset();
    this->tree.setRoot(nullptr);
    this->qglview->update();
  }
}

void MainWindow::onTabManagerEditorContentReloaded(EditorInterface *reloadedEditor)
{
  try {
    // when a new editor is created, it is important to compile the initial geometry
    // so the customizer panels are ok.
    parseDocument(reloadedEditor);
  } catch (const HardWarningException&) {
    exceptionCleanup();
  } catch (const std::exception& ex) {
    UnknownExceptionCleanup(ex.what());
  } catch (...) {
    UnknownExceptionCleanup();
  }

  // updates the content of the Recents Files menu to integrate the one possibly
  // associated with the created editor. The reason is that an editor can be created
  // or updated without a file associated with it.
  updateRecentFileActions();
}

void MainWindow::onTabManagerEditorChanged(EditorInterface *newEditor)
{
  activeEditor = newEditor;

  if (newEditor == nullptr) return;

  parameterDock->setWidget(newEditor->parameterWidget);
  editActionUndo->setEnabled(newEditor->canUndo());

  const QString name = getCurrentFileName();
  setWindowTitle(name);

  consoleDock->setNameSuffix(name);
  errorLogDock->setNameSuffix(name);
  animateDock->setNameSuffix(name);
  fontListDock->setNameSuffix(name);
  colorListDock->setNameSuffix(name);
  viewportControlDock->setNameSuffix(name);

  // If there is no renderedEditor we request for a new preview if the
  // auto-reload is enabled.
  if (renderedEditor == nullptr && designActionAutoReload->isChecked() && !MainWindow::isEmpty()) {
    actionRenderPreview();
  }
}

Dock *MainWindow::findVisibleDockToActivate(int offset) const
{
  const auto dockCount = docks.size();
  int focusedDockIndex = 0;

  // search among the docks the one that is having the focus. This is done by
  // traversing the widget hierarchy from the focused widget up to the docks that
  // contains it.
  const auto focusWidget = QApplication::focusWidget();
  for (auto widget = focusWidget; widget != nullptr; widget = widget->parentWidget()) {
    for (unsigned int index = 0; index < dockCount; ++index) {
      if (docks[index].first == focusWidget) {
        focusedDockIndex = index;
      }
    }
  }

  for (size_t o = 1; o < dockCount; ++o) {
    // starting from dockCount + focusedDockIndex move left or right (o*offset)
    // to find the first visible one. dockCount is there so there is no situation in which
    // (-1) % dockCount
    const int target = (dockCount + focusedDockIndex + o * offset) % dockCount;
    const auto& dock = docks.at(target).first;

    if (dock->isVisible()) {
      return dock;
    }
  }
  return nullptr;
}

void MainWindow::activateDock(Dock *dock)
{
  if (dock == nullptr) return;

  // We always need to activate the window.
  if (dock->isFloating()) dock->activateWindow();
  else QMainWindow::activateWindow();

  dock->raise();
  dock->setFocus();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
  if (event->mimeData()->hasUrls()) {
    event->acceptProposedAction();
  }
}

void MainWindow::dropEvent(QDropEvent *event)
{
  auto guard = scopedSetCurrentOutput();
  const QList<QUrl> urls = event->mimeData()->urls();
  for (const auto& url : urls) {
    handleFileDrop(url);
  }
}

void MainWindow::handleFileDrop(const QUrl& url)
{
  if (url.scheme() != "file") return;
  const auto fileName = url.toLocalFile();
  const auto fileInfo = QFileInfo{fileName};
  const auto suffix = fileInfo.suffix().toLower();
  const auto cmd = Importer::knownFileExtensions[suffix];
  if (cmd.isEmpty()) {
    tabManager->open(fileName);
  } else {
    activeEditor->insert(cmd.arg(fileName));
  }
}

void MainWindow::on_helpActionAbout_triggered()
{
  qApp->setWindowIcon(QApplication::windowIcon());
  auto dialog = new AboutDialog(this);
  dialog->exec();
  dialog->deleteLater();
}

void MainWindow::on_helpActionHomepage_triggered()
{
  UIUtils::openHomepageURL();
}

void MainWindow::on_helpActionManual_triggered()
{
  UIUtils::openUserManualURL();
}

void MainWindow::on_helpActionOfflineManual_triggered()
{
  UIUtils::openOfflineUserManual();
}

void MainWindow::on_helpActionCheatSheet_triggered()
{
  UIUtils::openCheatSheetURL();
}

void MainWindow::on_helpActionOfflineCheatSheet_triggered()
{
  UIUtils::openOfflineCheatSheet();
}

void MainWindow::on_helpActionLibraryInfo_triggered()
{
  if (!this->libraryInfoDialog) {
    const QString rendererInfo(qglview->getRendererInfo().c_str());
    auto dialog = new LibraryInfoDialog(rendererInfo);
    this->libraryInfoDialog = dialog;
  }
  this->libraryInfoDialog->show();
}

void MainWindow::on_editActionPreferences_triggered()
{
  GlobalPreferences::inst()->update();
  GlobalPreferences::inst()->show();
  GlobalPreferences::inst()->activateWindow();
  GlobalPreferences::inst()->raise();
}

void MainWindow::setColorScheme(const QString& scheme)
{
  RenderSettings::inst()->colorscheme = scheme.toStdString();
  this->qglview->setColorScheme(scheme.toStdString());
  this->qglview->update();
}

void MainWindow::setFont(const QString& family, uint size)
{
  QFont font;
  if (!family.isEmpty()) font.setFamily(family);
  else font.setFixedPitch(true);
  if (size > 0) font.setPointSize(size);
  font.setStyleHint(QFont::TypeWriter);
  activeEditor->setFont(font);
}

void MainWindow::consoleOutput(const Message& msgObj, void *userdata)
{
  // Invoke the method in the main thread in case the output
  // originates in a worker thread.
  auto thisp = static_cast<MainWindow *>(userdata);
  QMetaObject::invokeMethod(thisp, "consoleOutput", Q_ARG(Message, msgObj));
}

void MainWindow::consoleOutput(const Message& msgObj)
{
  this->console->addMessage(msgObj);
  if (msgObj.group == message_group::Warning || msgObj.group == message_group::Deprecated) {
    ++this->compileWarnings;
  } else if (msgObj.group == message_group::Error) {
    ++this->compileErrors;
  }
  // FIXME: scad parsing/evaluation should be done on separate thread so as not to block the gui.
  // Then processEvents should no longer be needed here.
  this->processEvents();
  if (consoleUpdater && !consoleUpdater->isActive()) {
    consoleUpdater->start(50);  // Limit console updates to 20 FPS
  }
}

void MainWindow::consoleOutputRaw(const QString& html)
{
  this->console->addHtml(html);
  this->processEvents();
}

void MainWindow::errorLogOutput(const Message& log_msg, void *userdata)
{
  auto thisp = static_cast<MainWindow *>(userdata);
  QMetaObject::invokeMethod(thisp, "errorLogOutput", Q_ARG(Message, log_msg));
}

void MainWindow::errorLogOutput(const Message& log_msg)
{
  this->errorLogWidget->toErrorLog(log_msg);
}

void MainWindow::setCurrentOutput()
{
  set_output_handler(&MainWindow::consoleOutput, &MainWindow::errorLogOutput, this);
}

void MainWindow::hideCurrentOutput()
{
  set_output_handler(&MainWindow::noOutputConsole, &MainWindow::noOutputErrorLog, this);
}

void MainWindow::clearCurrentOutput()
{
  set_output_handler(nullptr, nullptr, nullptr);
}

void MainWindow::openCSGSettingsChanged()
{
#ifdef ENABLE_OPENCSG
  OpenCSG::setOption(OpenCSG::AlgorithmSetting,
                     GlobalPreferences::inst()->getValue("advanced/forceGoldfeather").toBool()
                       ? OpenCSG::Goldfeather
                       : OpenCSG::Automatic);
#endif
}

void MainWindow::processEvents()
{
  if (this->procevents) QApplication::processEvents();
}

QString MainWindow::exportPath(const QString& suffix)
{
  const auto path_it = this->exportPaths.find(suffix);
  const auto basename =
    activeEditor->filepath.isEmpty() ? "Untitled" : QFileInfo(activeEditor->filepath).completeBaseName();
  QString dir;
  if (path_it != exportPaths.end()) {
    dir = QFileInfo(path_it->second).absolutePath();
  } else if (activeEditor->filepath.isEmpty()) {
    dir = QString::fromStdString(PlatformUtils::userDocumentsPath());
  } else {
    dir = QFileInfo(activeEditor->filepath).absolutePath();
  }
  return QString("%1/%2.%3").arg(dir, basename, suffix);
}

void MainWindow::jumpToLine(int line, int col)
{
  this->activeEditor->setCursorPosition(line, col);
}

void MainWindow::resetMeasurementsState(bool enable, const QString& tooltipMessage)
{
  if (RenderSettings::inst()->backend3D != RenderBackend3D::ManifoldBackend) {
    enable = false;
    static const auto noCGALMessage =
      "Measurements only work with Manifold backend; Preferences->Advanced->3D Rendering->Backend";
    this->designActionMeasureDist->setToolTip(noCGALMessage);
    this->designActionMeasureAngle->setToolTip(noCGALMessage);
  } else {
    this->designActionMeasureDist->setToolTip(tooltipMessage);
    this->designActionMeasureAngle->setToolTip(tooltipMessage);
  }

  this->designActionMeasureDist->setEnabled(enable);
  this->designActionMeasureDist->setChecked(false);
  this->designActionMeasureAngle->setEnabled(enable);
  this->designActionMeasureAngle->setChecked(false);

  (void)meas.stopMeasure();
  activeMeasurement = nullptr;
}

/**
  Initialize the GUI from the .ui design file and other top-level GUI setup.
 */
void MainWindow::setupWindow()
{
  installEventFilter(this);
  setupUi(this);
  this->setAttribute(Qt::WA_DeleteOnClose);
  scadApp->windowManager.add(this);
  setAcceptDrops(true);
}

/**
  Set up non-GUI elements like timers, workers, sounds, etc.
 */
void MainWindow::setupCoreSubsystems()
{
  renderCompleteSoundEffect = new QSoundEffect(this);
  renderCompleteSoundEffect->setSource(QUrl("qrc:/sounds/complete.wav"));

  this->cgalworker = new CGALWorker();
  connect(this->cgalworker, &CGALWorker::done, this, &MainWindow::actionRenderDone);

  autoReloadTimer = new QTimer(this);
  autoReloadTimer->setSingleShot(false);
  autoReloadTimer->setInterval(autoReloadPollingPeriodMS);
  connect(autoReloadTimer, &QTimer::timeout, this, &MainWindow::checkAutoReload);

  waitAfterReloadTimer = new QTimer(this);
  waitAfterReloadTimer->setSingleShot(true);
  waitAfterReloadTimer->setInterval(autoReloadPollingPeriodMS);
  connect(waitAfterReloadTimer, &QTimer::timeout, this, &MainWindow::waitAfterReload);

  consoleUpdater = new QTimer(this);
  consoleUpdater->setSingleShot(true);
  connect(consoleUpdater, &QTimer::timeout, this->console, &Console::update);
  this->consoleUpdater->start(0);  // Show initial messages immediately

  progressThrottle->start();
}

/**
  Initialize preferences and set up connections to respond to preference changes.
 */
void MainWindow::setupPreferences()
{
  connect(GlobalPreferences::inst(), &Preferences::updateReorderMode, this,
          &MainWindow::updateReorderMode);
  connect(GlobalPreferences::inst(), &Preferences::updateUndockMode, this,
          &MainWindow::updateUndockMode);
  connect(GlobalPreferences::inst(), &Preferences::openCSGSettingsChanged, this,
          &MainWindow::openCSGSettingsChanged);
  connect(GlobalPreferences::inst(), &Preferences::colorSchemeChanged, this,
          &MainWindow::setColorScheme);
  connect(GlobalPreferences::inst(), &Preferences::toolbarExportChanged, this,
          &MainWindow::updateExportActions);

  connect(GlobalPreferences::inst(), &Preferences::requestRedraw, this->qglview,
          QOverload<>::of(&QGLView::update));
  connect(GlobalPreferences::inst(), &Preferences::updateMouseCentricZoom, this->qglview,
          &QGLView::setMouseCentricZoom);
  connect(GlobalPreferences::inst()->MouseConfig, &MouseConfigWidget::updateMouseActions, this,
          &MainWindow::setAllMouseViewActions);

  GlobalPreferences::inst()->apply_win();
  GlobalPreferences::inst()->ButtonConfig->init();
  GlobalPreferences::inst()->MouseConfig->init();

  connect(GlobalPreferences::inst()->ButtonConfig, &ButtonConfigWidget::inputMappingChanged,
          InputDriverManager::instance(), &InputDriverManager::onInputMappingUpdated,
          Qt::UniqueConnection);
  connect(GlobalPreferences::inst()->AxisConfig, &AxisConfigWidget::inputMappingChanged,
          InputDriverManager::instance(), &InputDriverManager::onInputMappingUpdated,
          Qt::UniqueConnection);
  connect(GlobalPreferences::inst()->AxisConfig, &AxisConfigWidget::inputCalibrationChanged,
          InputDriverManager::instance(), &InputDriverManager::onInputCalibrationUpdated,
          Qt::UniqueConnection);
  connect(GlobalPreferences::inst()->AxisConfig, &AxisConfigWidget::inputGainChanged,
          InputDriverManager::instance(), &InputDriverManager::onInputGainUpdated, Qt::UniqueConnection);

  connect(GlobalPreferences::inst(), &Preferences::ExperimentalChanged, this,
          &MainWindow::onExperimentalChanged);
  onExperimentalChanged();
}

/**
  Set up resources related to the Status Bar
 */
void MainWindow::setupStatusBar()
{
  this->versionLabel = nullptr;  // must be initialized before calling updateStatusBar()
  updateStatusBar(nullptr);
}

/**
  Set up resources related to the Console dock widget
 */
void MainWindow::setupConsole()
{
  connect(this->console, &Console::openWindowRequested, this, &MainWindow::showLink);
  connect(this->console, &Console::openFile, this, &MainWindow::openFileFromPath);

  QObject::connect(consoleDock, &Dock::visibilityChanged, this,
                   &MainWindow::onConsoleDockVisibilityChanged);

  this->console->setConsoleFont(
    GlobalPreferences::inst()->getValue("advanced/consoleFontFamily").toString(),
    GlobalPreferences::inst()->getValue("advanced/consoleFontSize").toUInt());

  consoleOutputRaw(
    QString("<b>OpenSCAD %1</b>").arg(QString::fromStdString(std::string(openscad_versionnumber))));
  consoleOutputRaw(QString("<a href=\"https://www.openscad.org/\">https://www.openscad.org/</a><br>"));
  consoleOutputRaw(
    QString("<p>Copyright (C) 2009-2026 The OpenSCAD Developers</p>"
            "<p>This program is free software; you can redistribute it and/or modify "
            "it under the terms of the GNU General Public License as published by "
            "the Free Software Foundation; either version 2 of the License, or "
            "(at your option) any later version.</p>"));
}

/**
  Set up resources related to the Error Log dock widget
 */
void MainWindow::setupErrorLog()
{
  connect(this->errorLogWidget, &ErrorLog::openFile, this, &MainWindow::openFileFromPath);

  QObject::connect(errorLogDock, &Dock::visibilityChanged, this,
                   &MainWindow::onErrorLogDockVisibilityChanged);
}

/**
  Set up resources related to the Editor dock widget
 */
void MainWindow::setupEditor(const QStringList& filenames)
{
  tabManager = new TabManager(this, filenames.isEmpty() ? QString() : filenames[0]);
  activeEditor = tabManager->editor;
  editorDockContents->layout()->addWidget(tabManager->getTabContent());

  connect(this->fileActionNew, &QAction::triggered, tabManager, &TabManager::actionNew);
  connect(this->fileActionClose, &QAction::triggered, tabManager, &TabManager::closeCurrentTab);
  connect(this->fileActionSaveAll, &QAction::triggered, tabManager, &TabManager::saveAll);

  connect(this, &MainWindow::highlightError, tabManager, &TabManager::highlightError);
  connect(this, &MainWindow::unhighlightLastError, tabManager, &TabManager::unhighlightLastError);

  connect(this->editActionUndo, &QAction::triggered, tabManager, &TabManager::undo);
  connect(this->editActionRedo, &QAction::triggered, tabManager, &TabManager::redo);
  connect(this->editActionRedo_2, &QAction::triggered, tabManager, &TabManager::redo);
  connect(this->editActionCut, &QAction::triggered, tabManager, &TabManager::cut);
  connect(this->editActionPaste, &QAction::triggered, tabManager, &TabManager::paste);

  connect(this->editActionIndent, &QAction::triggered, tabManager, &TabManager::indentSelection);
  connect(this->editActionUnindent, &QAction::triggered, tabManager, &TabManager::unindentSelection);
  connect(this->editActionComment, &QAction::triggered, tabManager, &TabManager::commentSelection);
  connect(this->editActionUncomment, &QAction::triggered, tabManager, &TabManager::uncommentSelection);

  connect(this->editActionToggleBookmark, &QAction::triggered, tabManager, &TabManager::toggleBookmark);
  connect(this->editActionNextBookmark, &QAction::triggered, tabManager, &TabManager::nextBookmark);
  connect(this->editActionPrevBookmark, &QAction::triggered, tabManager, &TabManager::prevBookmark);
  connect(this->editActionJumpToNextError, &QAction::triggered, tabManager,
          &TabManager::jumpToNextError);

  connect(tabManager, &TabManager::editorAboutToClose, this,
          &MainWindow::onTabManagerAboutToCloseEditor);
  connect(tabManager, &TabManager::currentEditorChanged, this, &MainWindow::onTabManagerEditorChanged);
  connect(tabManager, &TabManager::editorContentReloaded, this,
          &MainWindow::onTabManagerEditorContentReloaded);

  connect(this->editActionNextTab, &QAction::triggered, tabManager, &TabManager::nextTab);
  connect(this->editActionPrevTab, &QAction::triggered, tabManager, &TabManager::prevTab);

  onTabManagerEditorChanged(activeEditor);
  QObject::connect(editorDock, &Dock::visibilityChanged, this,
                   &MainWindow::onEditorDockVisibilityChanged);
}

/**
  Set up resources related to the Customizer dock widget
 */
void MainWindow::setupCustomizer()
{
  QObject::connect(parameterDock, &Dock::visibilityChanged, this,
                   &MainWindow::onParametersDockVisibilityChanged);
}

/**
  Set up resources related to the Animate dock widget
 */
void MainWindow::setupAnimate()
{
  this->animateWidget->setMainWindow(this);
  QObject::connect(animateDock, &Dock::visibilityChanged, this,
                   &MainWindow::onAnimateDockVisibilityChanged);
}

/**
  Set up resources related to the Font List  dock widget
 */
void MainWindow::setupFontList()
{
  QObject::connect(fontListDock, &Dock::visibilityChanged, this,
                   &MainWindow::onFontListDockVisibilityChanged);
}

/**
  Set up resources related to the Color List  dock widget
 */
void MainWindow::setupColorList()
{
  QObject::connect(colorListDock, &Dock::visibilityChanged, this,
                   &MainWindow::onColorListDockVisibilityChanged);
  QObject::connect(colorListWidget, &ColorList::colorSelected, this,
                   &MainWindow::onColorListColorSelected);
}

/**
  Set up resources related to the Viewport Control dock widget
 */
void MainWindow::setupViewportControl()
{
  this->viewportControlWidget->setMainWindow(this);
  QObject::connect(viewportControlDock, &Dock::visibilityChanged, this,
                   &MainWindow::onViewportControlDockVisibilityChanged);
}

/**
  Setup AIDock
 */
void MainWindow::setupAIDock()
{
  this->aiDock = new AIDock(this);
  addDockWidget(Qt::RightDockWidgetArea, this->aiDock);
  this->aiDock->hide();

  QObject::connect(this->aiDock, &Dock::visibilityChanged, this, &MainWindow::onAIDockVisibilityChanged);
}

void MainWindow::setupCameraDock()
{
  this->cameraDock = new CameraViewDock(this);
  addDockWidget(Qt::RightDockWidgetArea, this->cameraDock);
  this->cameraDock->hide();

  // Re-render snapshots when the dock becomes visible (e.g. opened from the menu).
  QObject::connect(this->cameraDock, &Dock::visibilityChanged, this, [this](bool visible) {
    if (visible) refreshCameras();
  });
  // Dragging a // @label callout on a snapshot writes laboff= (image px) back.
  QObject::connect(this->cameraDock, &CameraViewDock::labelTextMoved, this,
                   [this](int id, double dx, double dy, bool finished) {
                     onLabelTextMoved(-1, id, dx, dy, finished);
                   });
  // Double-click / right-click a callout: edit it / jump to its source line.
  QObject::connect(this->cameraDock, &CameraViewDock::labelDoubleClicked, this,
                   &MainWindow::onLabelDoubleClicked);
  QObject::connect(this->cameraDock, &CameraViewDock::jumpToSource, this, [this](int line) {
    if (this->activeEditor && line >= 0) {
      this->jumpToLine(line, 0);
      this->activeEditor->setFocus();
    }
  });
  // Clicking a label() marker dot on a snapshot appends a // @label for that camera.
  QObject::connect(this->cameraDock, &CameraViewDock::labelAnchorClicked, this,
                   &MainWindow::onCameraLabelAnchorClicked);
}

// refreshCameras() is defined after the anonymous namespace below, because it
// uses resolveLabels() (a file-local helper declared there).

void MainWindow::setupDrawingDock()
{
  this->drawingDock = new DrawingViewDock(this);
  addDockWidget(Qt::RightDockWidgetArea, this->drawingDock);
  this->drawingDock->hide();

  // Re-render the ortho views when the dock becomes visible (e.g. from the menu).
  QObject::connect(this->drawingDock, &Dock::visibilityChanged, this, [this](bool visible) {
    if (visible) refreshDrawingViews();
  });
  // Clicking two anchors in a view creates a dimension in the source.
  QObject::connect(this->drawingDock, &DrawingViewDock::anchorClicked, this,
                   &MainWindow::onDrawingAnchorClicked);
  // Dragging a dimension line changes its offset= and writes it back on release.
  QObject::connect(this->drawingDock, &DrawingViewDock::dimensionOffsetChanged, this,
                   &MainWindow::onDimensionOffsetChanged);
  // Rotating a radial (⌀/R) dimension writes angle= back on release.
  QObject::connect(this->drawingDock, &DrawingViewDock::dimensionAngleChanged, this,
                   &MainWindow::onDimensionAngleChanged);
  // A persisted sheet setting changed (shown views, scale, fill, gap): write the
  // // @sheet comment from the dock's state, then reproject (mesh stays cached).
  QObject::connect(this->drawingDock, &DrawingViewDock::sheetSettingsChanged, this,
                   &MainWindow::onDrawingSheetSettingsChanged);
  // Dragging a label writes laboff= back on release.
  QObject::connect(this->drawingDock, &DrawingViewDock::labelOffsetChanged, this,
                   &MainWindow::onLabelOffsetChanged);
  // Double-click a dimension to edit its label/precision/arrows/projection.
  QObject::connect(this->drawingDock, &DrawingViewDock::dimensionDoubleClicked, this,
                   &MainWindow::onDimensionDoubleClicked);
  // Clicking a label() marker dot appends a // @label line for that view.
  QObject::connect(this->drawingDock, &DrawingViewDock::labelAnchorClicked, this,
                   &MainWindow::onLabelAnchorClicked);
  // Dragging a // @label text writes laboff= back on release.
  QObject::connect(this->drawingDock, &DrawingViewDock::labelTextMoved, this,
                   &MainWindow::onLabelTextMoved);
  // Double-click a // @label to edit its text/projection/leader.
  QObject::connect(this->drawingDock, &DrawingViewDock::labelDoubleClicked, this,
                   &MainWindow::onLabelDoubleClicked);
  // Right-click a dimref/dimension in a view jumps the editor to its source line.
  QObject::connect(this->drawingDock, &DrawingViewDock::jumpToSource, this, [this](int line) {
    if (this->activeEditor && line >= 0) {
      this->jumpToLine(line, 0);
      this->activeEditor->setFocus();
    }
  });
  // "Materialize kvóty" writes a // @dim kvota=… line for each kvota pair.
  QObject::connect(this->drawingDock, &DrawingViewDock::materializeRequested, this,
                   &MainWindow::onMaterializeKvotas);
  // Drawing a detail circle over a view appends a // @detail line and reprojects.
  QObject::connect(this->drawingDock, &DrawingViewDock::detailRequested, this,
                   &MainWindow::onDetailRequested);
  // Dragging a section / detail tile to free space writes its at=x,y back.
  QObject::connect(this->drawingDock, &DrawingViewDock::tilePlaced, this,
                   &MainWindow::onTilePlaced);
}

// --- Fáze 15: switchable model views -------------------------------------

void MainWindow::setupViewSwitcher()
{
  this->viewSwitcher = new QComboBox(this);
  this->viewSwitcher->setToolTip(_("Active model view — select which view() is shown"));
  this->viewSwitcher->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  // Sit at the front of the editor toolbar; hidden until the model has a view().
  if (this->editortoolbar) {
    const auto acts = this->editortoolbar->actions();
    if (acts.isEmpty()) this->editortoolbar->addWidget(this->viewSwitcher);
    else this->editortoolbar->insertWidget(acts.first(), this->viewSwitcher);
  }
  this->viewSwitcher->hide();
  // activated() fires only on user choice, so repopulating it never recompiles.
  connect(this->viewSwitcher, QOverload<int>::of(&QComboBox::activated), this,
          &MainWindow::onViewSwitcherChanged);
}

// Rebuild the switcher from the parsed source after every compile. The view tree
// is read from the AST (collectViews), so inactive/nested views — including ones
// pulled in via include<> — all appear regardless of which view is active.
void MainWindow::refreshViewSwitcher()
{
  if (!this->viewSwitcher) return;

  // One entry per distinct view path: several blocks may share a name (they are all
  // active together), but the switcher selects a path, so listing it twice is noise.
  const auto views =
    this->rootFile ? uniqueViewPaths(collectViews(this->rootFile.get())) : std::vector<ViewInfo>{};

  if (views.empty()) {
    // Model has no view(): behave exactly like before (switcher hidden, root).
    this->viewSwitcher->hide();
    this->viewSwitcher->clear();
    this->activeViewPath.clear();
    return;
  }

  QStringList active;
  for (const auto& s : this->activeViewPath) active << QString::fromStdString(s);

  const QSignalBlocker block(this->viewSwitcher);
  this->viewSwitcher->clear();
  this->viewSwitcher->addItem(_("(all / common)"), QStringList{});  // root
  for (const auto& v : views) {
    QStringList path;
    for (const auto& s : v.fullPath) path << QString::fromStdString(s);
    const QString label = QString(v.depth * 2, ' ') + QString::fromStdString(v.name);
    this->viewSwitcher->addItem(label, path);
  }

  // Re-select the active path if it still exists; otherwise fall back to root.
  int sel = 0;
  for (int i = 0; i < this->viewSwitcher->count(); ++i) {
    if (this->viewSwitcher->itemData(i).toStringList() == active) {
      sel = i;
      break;
    }
  }
  this->viewSwitcher->setCurrentIndex(sel);
  if (this->viewSwitcher->itemData(sel).toStringList() != active) {
    // The view we were showing was edited away: revert to root on next compile.
    this->activeViewPath.clear();
  }
  this->viewSwitcher->show();
}

void MainWindow::onViewSwitcherChanged(int index)
{
  if (!this->viewSwitcher || index < 0) return;
  const QStringList path = this->viewSwitcher->itemData(index).toStringList();
  std::vector<std::string> newPath;
  newPath.reserve(path.size());
  for (const auto& s : path) newPath.push_back(s.toStdString());
  if (newPath == this->activeViewPath) return;

  this->activeViewPath = std::move(newPath);
  // Re-evaluate with the new $view: preview, cameras and drawing all rebuild from
  // the re-instantiated tree.
  actionRenderPreview();
}

bool MainWindow::exportDrawingHeadless(const std::vector<std::string>& modelViews,
                                       bool allModelViews, bool defaultModelViews,
                                       const QString& prefix, const QString& ext)
{
  if (!this->drawingDock || !this->activeEditor) {
    LOG(message_group::Error, "Drawing export: no document loaded.");
    return false;
  }
  // The dock only rebuilds the sheet data while it is visible (the projection needs
  // the F6 mesh); the MainWindow is shown by gui(), so this makes the dock live.
  this->drawingDock->show();

  // Compile once at the root so rootFile is parsed (needed to enumerate views for
  // --all-model-views). Synchronous: compile() -> compileDone() -> csgRender().
  this->activeViewPath.clear();
  actionRenderPreview();

  // Resolve the view specs (path + filename tag) to export.
  std::vector<std::pair<std::vector<std::string>, std::string>> specs;
  if (defaultModelViews && (allModelViews || !modelViews.empty())) {
    LOG(message_group::Warning,
        "--default-model-views is ignored when --all-model-views or --model-view is given.");
    defaultModelViews = false;
  }
  if (allModelViews) {
    if (this->rootFile) {
      for (const auto& info : uniqueViewPaths(collectViews(this->rootFile.get()))) {
        specs.emplace_back(info.fullPath, boost::algorithm::join(info.fullPath, "_"));
      }
    }
    if (specs.empty()) specs.emplace_back(std::vector<std::string>{}, std::string());
  } else if (defaultModelViews) {
    // Fáze 17: only the views the model marks for drawing export. No fallback to the
    // root — an unmarked model must not silently produce a sheet.
    if (this->rootFile) {
      for (const auto& info : uniqueViewPaths(collectViews(this->rootFile.get()))) {
        if (!viewIsDefaultFor(info, ViewExportKind::Drawings)) continue;
        specs.emplace_back(info.fullPath, boost::algorithm::join(info.fullPath, "_"));
      }
    }
    if (specs.empty()) {
      // Nothing marked is not a failure: a CI job may run the camera, drawing and
      // model exports over every model, and each model marks only what it needs.
      LOG(message_group::Warning,
          "No view() in the model is marked drawings_export = true; nothing to export.");
      return true;
    }
  } else if (!modelViews.empty()) {
    for (const auto& v : modelViews) {
      std::vector<std::string> path;
      for (const QString& s : QString::fromStdString(v).split('/', Qt::SkipEmptyParts))
        path.push_back(s.toStdString());
      specs.emplace_back(path, boost::algorithm::join(path, "_"));
    }
  } else {
    specs.emplace_back(std::vector<std::string>{}, std::string());
  }

  bool ok = true;
  for (const auto& [viewPath, tag] : specs) {
    this->activeViewPath = viewPath;
    actionRenderPreview();  // synchronous rebuild of the dock's sheet for this view

    QString path = prefix;
    if (!tag.empty()) path += "_" + QString::fromStdString(tag);
    path += ext;
    if (this->drawingDock->exportToPath(path)) {
      LOG("Exported drawing (view '%1$s') to %2$s", tag.empty() ? std::string("(root)") : tag,
          path.toStdString());
    } else {
      LOG(message_group::Warning,
          "Drawing export: nothing to draw for view '%1$s' "
          "(needs at least one selected view with geometry).",
          tag.empty() ? std::string("(root)") : tag);
      ok = false;
    }
  }
  return ok;
}

std::vector<ViewScope::Block> MainWindow::resolveViewBlocks(const QString& source) const
{
  std::vector<ViewScope::Block> blocks;
  if (!this->rootFile) return blocks;

  const std::string mainPath = this->rootFile->getFullpath();
  std::vector<ViewScope::ViewRef> refs;
  std::vector<ViewScope::ViewRef> all;
  for (const auto& v : collectViews(this->rootFile.get())) {
    ViewScope::ViewRef r;
    r.path = v.fullPath;
    r.firstLine1 = v.firstLine;
    all.push_back(r);
    // Annotations read here are lines of the main editor file, so only view()
    // blocks in that file can enclose them.
    if (mainPath.empty() || v.path == mainPath) refs.push_back(std::move(r));
  }
  // Fallback for a single-file model whose path string did not compare equal:
  // use every view (their line numbers are all in this one file anyway).
  if (refs.empty()) refs = std::move(all);

  std::vector<std::string> lines;
  for (const QString& L : source.split('\n')) lines.push_back(L.toStdString());
  return ViewScope::resolveBlocks(lines, refs);
}

void MainWindow::insertAnnotationLine(const QString& annLine)
{
  if (!activeEditor) return;
  const QString source = activeEditor->toPlainText();
  const QStringList lines = source.split('\n');
  const auto blocks = resolveViewBlocks(source);
  const std::vector<std::string> active(this->activeViewPath.begin(), this->activeViewPath.end());
  const int at = ViewScope::insertLine(blocks, active);

  if (at < 0 || at >= lines.size()) {
    // Root, or a view with no brace body: append at end of file (as before).
    activeEditor->setCursorPosition(lines.size() - 1, lines.isEmpty() ? 0 : lines.last().length());
    QString ins;
    if (!lines.isEmpty() && !lines.last().isEmpty()) ins += "\n";
    ins += annLine + "\n";
    activeEditor->insertOrReplaceText(ins);
    return;
  }

  // Insert before the closing-brace line, indented one step past the brace.
  const QString braceLine = lines.at(at);
  int ws = 0;
  while (ws < braceLine.size() && (braceLine.at(ws) == ' ' || braceLine.at(ws) == '\t')) ++ws;
  const QString indent = braceLine.left(ws) + "  ";
  activeEditor->setCursorPosition(at, 0);
  activeEditor->insertOrReplaceText(indent + annLine + "\n");
}

namespace {
// Canonical proj= token per standard view, indexed by the Drawing dock's view
// index (0..5), matching DrawingViewDock's checkbox order.
const char *const kProjKeys[6] = {"front", "back", "left", "right", "top", "bottom"};

// Normalize a projection token (english or czech) to one of kProjKeys, or "".
QString normalizeProj(const QString& raw)
{
  const QString p = raw.trimmed().toLower();
  // A section tile: "section<name>", the same key // @detail parent= already uses.
  // Passed through verbatim (lowercased); the section loop matches on it.
  if (p.startsWith("section") && p.size() > 7) return p;
  if (p == "front" || p == "narys" || p == "nárys") return "front";
  if (p == "back" || p == "rear" || p == "zadni" || p == "zadní") return "back";
  if (p == "left" || p == "levy" || p == "levý") return "left";
  if (p == "right" || p == "side" || p == "bokorys" || p == "pravy" || p == "pravý") return "right";
  if (p == "top" || p == "pudorys" || p == "půdorys") return "top";
  if (p == "bottom" || p == "spodni" || p == "spodní") return "bottom";
  return "";
}

// Replace or append a `key=value` token on one editor line (whole-word key).
QString setLineToken(QString line, const QString& key, const QString& value)
{
  const QString token = key + "=" + value;
  const QRegularExpression re("\\b" + QRegularExpression::escape(key) + "=\\S+");
  if (re.match(line).hasMatch()) {
    line.replace(re, token);
  } else {
    if (!line.endsWith(' ')) line += ' ';
    line += token;
  }
  return line;
}
}  // namespace

void MainWindow::onDimensionDoubleClicked(int id)
{
  if (id < 0 || !activeEditor) return;
  QStringList lines = activeEditor->toPlainText().split('\n');
  if (id >= lines.size()) return;
  QString line = lines.at(id);

  // Read current attribute values from the @dim line.
  auto tokenValue = [&](const QString& re) {
    const auto m = QRegularExpression(re).match(line);
    return m.hasMatch() ? m.captured(1) : QString();
  };
  QString curLabel = tokenValue(R"RX(label="([^"]*)")RX");
  if (curLabel.isEmpty()) curLabel = tokenValue(R"(\blabel=(\S+))");
  const QString curPrec = tokenValue(R"(\bprec=(\d+))");
  const QString curArrows = tokenValue(R"(\barrows=(\w+))");
  const QString curLeader = tokenValue(R"(\bleader=(\w+))");
  const QString curProj = tokenValue(R"(\bproj=(\w+))");
  const QString curMeasure = tokenValue(R"(\bmeasure=(\w+))");
  const QString curKind = tokenValue(R"(\bkind=(\w+))");
  const bool isRadial =
    curKind == "diameter" || curKind == "radius" || curKind == "d" || curKind == "r";

  // Build the edit dialog.
  QDialog dialog(this);
  dialog.setWindowTitle(tr("Edit dimension"));
  auto *form = new QFormLayout(&dialog);

  auto *labelEdit = new QLineEdit(curLabel, &dialog);
  labelEdit->setPlaceholderText(tr("%f = measured length, e.g. \"width %f mm\""));
  form->addRow(tr("Label"), labelEdit);

  auto *precSpin = new QSpinBox(&dialog);
  precSpin->setRange(-1, 6);
  precSpin->setSpecialValueText(tr("auto"));
  precSpin->setValue(curPrec.isEmpty() ? -1 : curPrec.toInt());
  form->addRow(tr("Precision"), precSpin);

  auto *arrowsBox = new QComboBox(&dialog);
  arrowsBox->addItems({"in", "out"});
  arrowsBox->setCurrentText(curArrows == "out" ? "out" : "in");
  form->addRow(tr("Arrows"), arrowsBox);

  // Leader line to a dragged-away label (per dimension). Default on.
  auto *leaderCheck = new QCheckBox(tr("Leader when label moved"), &dialog);
  leaderCheck->setChecked(curLeader.toLower() != "off");
  form->addRow(tr("Leader"), leaderCheck);

  // Diameter vs radius: only for radial dims (value comes from the dimref's
  // d=/r=, either implies the other, so flipping this is always possible).
  QComboBox *kindBox = nullptr;
  if (isRadial) {
    kindBox = new QComboBox(&dialog);
    kindBox->addItems({tr("diameter (⌀)"), tr("radius (R)")});
    kindBox->setCurrentIndex(curKind == "radius" || curKind == "r" ? 1 : 0);
    form->addRow(tr("Kind"), kindBox);
  }

  // Offer the views ticked in the Drawing dock: a dim can only show up in a view
  // that is on the sheet. The dim's own proj stays in the list even when its view
  // is unticked, so opening and OK-ing this dialog never silently retargets it.
  // The proj= token is the item data, since the display text may be decorated.
  auto *projBox = new QComboBox(&dialog);
  projBox->addItem(tr("(all)"), QString());
  const QString curProjKey = normalizeProj(curProj);
  for (int i = 0; i < 6; ++i) {
    const QString key = QString::fromLatin1(kProjKeys[i]);
    if (this->drawingDock->isViewSelected(i)) projBox->addItem(key, key);
    else if (key == curProjKey) projBox->addItem(tr("%1 (view not shown)").arg(key), key);
  }
  // Section tiles can be targeted too (proj=section<name>); they are always on the
  // sheet when the model defines them, so there is no "not shown" case.
  if (this->rootNode) {
    for (const auto& info : collectSections(this->rootNode)) {
      if (!info.node || info.node->section_name.empty()) continue;
      const QString key =
        "section" + QString::fromStdString(info.node->section_name).toLower();
      if (projBox->findData(key) < 0) projBox->addItem(key, key);
    }
  }
  if (!curProjKey.isEmpty() && projBox->findData(curProjKey) < 0)
    projBox->addItem(tr("%1 (not on the sheet)").arg(curProjKey), curProjKey);
  projBox->setCurrentIndex(std::max(0, projBox->findData(curProjKey)));
  projBox->setToolTip(tr("Which view or section shows this dimension; "
                         "(all) = every ortho view. Only the views ticked in the "
                         "Drawing dock, plus the model's sections, are listed."));
  form->addRow(tr("Projection"), projBox);

  // Measure mode: how the length is taken relative to the projection plane.
  auto *measureBox = new QComboBox(&dialog);
  measureBox->addItems({"direct", "planar", "h", "v"});
  measureBox->setCurrentText(curMeasure.isEmpty() ? "direct" : curMeasure);
  measureBox->setToolTip(tr("direct = true 3D distance; planar = in the view plane; "
                            "h/v = horizontal/vertical component in the view"));
  form->addRow(tr("Measure"), measureBox);

  auto *buttons =
    new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  auto *deleteButton = buttons->addButton(tr("Delete"), QDialogButtonBox::DestructiveRole);
  deleteButton->setToolTip(tr("Remove this dimension (the // @dim line)."));
  form->addRow(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  bool deleteRequested = false;
  connect(deleteButton, &QPushButton::clicked, &dialog, [&] {
    deleteRequested = true;
    dialog.accept();
  });

  if (dialog.exec() != QDialog::Accepted) return;

  // Delete: drop the whole // @dim line. Line numbers of other dims shift, but
  // refreshDrawingViews re-reads the source so their ids are recomputed.
  if (deleteRequested) {
    lines.removeAt(id);
    activeEditor->setPlainText(lines.join('\n'));
    refreshDrawingViews();
    this->statusBar()->showMessage(tr("Dimension deleted."), 3000);
    return;
  }

  // Rewrite the tokens. Label may contain spaces -> always quote it.
  line.remove(QRegularExpression(R"RX(\s*label="[^"]*")RX"));
  line.remove(QRegularExpression(R"(\s*\blabel=\S+)"));
  const QString labelText = labelEdit->text().trimmed();
  if (!labelText.isEmpty()) line += " label=\"" + labelText + "\"";

  if (precSpin->value() >= 0) {
    line = setLineToken(line, "prec", QString::number(precSpin->value()));
  } else {
    line.remove(QRegularExpression(R"(\s*\bprec=\S+)"));
  }
  line = setLineToken(line, "arrows", arrowsBox->currentText());

  // Leader is on by default: write leader=off only when disabled, else drop it.
  if (!leaderCheck->isChecked()) line = setLineToken(line, "leader", "off");
  else line.remove(QRegularExpression(R"(\s*\bleader=\S+)"));

  line.remove(QRegularExpression(R"(\s*\bproj=\S+)"));
  const QString projVal = projBox->currentData().toString();
  if (!projVal.isEmpty()) line += " proj=" + projVal;

  line.remove(QRegularExpression(R"(\s*\bmeasure=\S+)"));
  if (measureBox->currentText() != "direct") {
    line += " measure=" + measureBox->currentText();
  }

  if (kindBox) line = setLineToken(line, "kind", kindBox->currentIndex() == 1 ? "radius" : "diameter");

  lines[id] = line;
  activeEditor->setPlainText(lines.join('\n'));
  // All edited tokens live in the // @dim comment, so the geometry is unchanged:
  // reproject immediately instead of waiting for F5.
  refreshDrawingViews();
  this->statusBar()->showMessage(tr("Dimension updated."), 3000);
}

void MainWindow::onDetailRequested(const QString& projKey, double cx, double cy, double r)
{
  if (!activeEditor || projKey.isEmpty()) return;
  const QString text = activeEditor->toPlainText();

  // Pick the next unused single-letter detail name (A, B, C, …).
  static const QRegularExpression nameRe(R"(^\s*//\s*@detail\s+(\S+))");
  std::set<QString> used;
  for (const QString& ln : text.split('\n')) {
    const auto m = nameRe.match(ln);
    if (m.hasMatch()) used.insert(m.captured(1).toUpper());
  }
  QString name = "A";
  for (char c = 'A'; c <= 'Z'; ++c)
    if (!used.count(QString(QChar(c)))) { name = QString(QChar(c)); break; }

  // scale=2 (2:1) is a sensible default enlargement; the user edits it afterwards.
  const QString line = QString("// @detail %1 parent=%2 cx=%3 cy=%4 r=%5 scale=2")
                         .arg(name, projKey)  // "front".."bottom" or "section<name>"
                         .arg(cx, 0, 'g', 4).arg(cy, 0, 'g', 4).arg(r, 0, 'g', 4);
  const QStringList lines = text.split('\n');
  activeEditor->setCursorPosition(lines.size() - 1, lines.isEmpty() ? 0 : lines.last().length());
  QString insertText;
  if (!lines.isEmpty() && !lines.last().isEmpty()) insertText += "\n";
  insertText += line + "\n";
  activeEditor->insertOrReplaceText(insertText);
  refreshDrawingViews();
  this->statusBar()->showMessage(
    tr("Detail %1 added — edit scale= to change the enlargement.").arg(name), 5000);
}

void MainWindow::onTilePlaced(const QString& placeKey, double x, double y)
{
  if (!activeEditor) return;
  const QString atVal = QString("%1,%2").arg(x, 0, 'g', 5).arg(y, 0, 'g', 5);
  QStringList lines = activeEditor->toPlainText().split('\n');

  if (placeKey.startsWith("detail:")) {
    // The @detail line's source index is baked into the key; update its at= token.
    const int ln = placeKey.mid(7).toInt();
    if (ln < 0 || ln >= lines.size()) return;
    lines[ln] = setLineToken(lines.at(ln), "at", atVal);
    activeEditor->setPlainText(lines.join('\n'));
  } else if (placeKey.startsWith("section:")) {
    // Sections have no annotation line of their own: update an existing
    // // @section <name> … line, or append one carrying just the placement.
    const QString name = placeKey.mid(8);
    static const QRegularExpression secRe(R"(^\s*//\s*@section\s+(\S+))");
    int found = -1;
    for (int i = 0; i < lines.size(); ++i) {
      const auto m = secRe.match(lines.at(i));
      if (m.hasMatch() && m.captured(1) == name) { found = i; break; }
    }
    if (found >= 0) {
      lines[found] = setLineToken(lines.at(found), "at", atVal);
      activeEditor->setPlainText(lines.join('\n'));
    } else {
      activeEditor->setCursorPosition(lines.size() - 1,
                                      lines.isEmpty() ? 0 : lines.last().length());
      QString ins;
      if (!lines.isEmpty() && !lines.last().isEmpty()) ins += "\n";
      ins += QString("// @section %1 at=%2\n").arg(name, atVal);
      activeEditor->insertOrReplaceText(ins);
    }
  } else {
    return;
  }
  refreshDrawingViews();
}

void MainWindow::onDrawingSheetSettingsChanged()
{
  if (!activeEditor || !this->drawingDock) return;
  // Build the whole // @sheet line from the dock's current state (gap / scale / shown
  // views / fill). One comment carries the sheet configuration so it travels with the
  // file and the export matches the dock.
  static const char *const slug[6] = {"front", "back", "left", "right", "top", "bottom"};
  QStringList shown;
  for (int i = 0; i < 6; ++i)
    if (this->drawingDock->isViewSelected(i)) shown << slug[i];

  const QString line =
    QString("// @sheet gap=%1 scale=%2 views=%3 fill=%4")
      .arg(this->drawingDock->sheetGap(), 0, 'g', 5)
      .arg(this->drawingDock->scaleDenom(), 0, 'g', 5)
      .arg(shown.isEmpty() ? QStringLiteral("none") : shown.join(','),
           this->drawingDock->fillColors() ? QStringLiteral("on") : QStringLiteral("off"));

  // Fáze 15: the // @sheet belongs to the active view. Replace the one owned
  // exactly by the active view (not an inherited common one); if none exists yet,
  // write a fresh // @sheet into that view's block (or EOF for the root).
  const QString source = activeEditor->toPlainText();
  QStringList lines = source.split('\n');
  const auto blocks = resolveViewBlocks(source);
  const std::vector<std::string> active(this->activeViewPath.begin(),
                                        this->activeViewPath.end());
  static const QRegularExpression sheetRe(R"(^\s*//\s*@sheet\b)");
  int found = -1;
  for (int i = 0; i < lines.size(); ++i) {
    if (!sheetRe.match(lines.at(i)).hasMatch()) continue;
    if (ViewScope::ownerPath(blocks, i) == active) { found = i; break; }
  }
  if (found >= 0) {
    if (lines.at(found) == line) return;  // no change: don't churn the document
    lines[found] = line;
    activeEditor->setPlainText(lines.join('\n'));
  } else {
    insertAnnotationLine(line);
  }
  refreshDrawingViews();
}

void MainWindow::onMaterializeKvotas()
{
  if (!activeEditor || !this->rootNode) return;

  // Kvota names with the world positions of their (first two) dimref anchors.
  std::map<std::string, std::vector<Eigen::Vector3d>> kvotaPts;
  for (const auto& d : collectDimrefs(this->rootNode)) {
    if (d.node && !d.node->kvota_name.empty()) {
      kvotaPts[d.node->kvota_name].push_back(d.world.translation());
    }
  }

  // Kvota names that already have a // @dim kvota=… line.
  const QString text = activeEditor->toPlainText();
  static const QRegularExpression kvRe(R"(^\s*//\s*@dim\s+kvota=(\S+))");
  std::set<std::string> already;
  for (const QString& line : text.split('\n')) {
    const auto m = kvRe.match(line);
    if (m.hasMatch()) already.insert(m.captured(1).toStdString());
  }

  // Pick the ortho view where the two anchors are most separated, so the
  // materialized dimension is non-degenerate (e.g. a vertical pair -> front).
  auto bestProj = [](const Eigen::Vector3d& a, const Eigen::Vector3d& b) {
    const Eigen::Vector3d d = b - a;
    const double front = std::hypot(d.x(), d.z());  // XZ plane
    const double side = std::hypot(d.y(), d.z());   // YZ plane
    const double top = std::hypot(d.x(), d.y());    // XY plane
    if (front >= side && front >= top) return QString("front");
    return side >= top ? QString("side") : QString("top");
  };

  QStringList newLines;
  for (const auto& [kvota, pts] : kvotaPts) {
    if (pts.size() >= 2 && !already.count(kvota)) {
      newLines << QString("// @dim kvota=%1 proj=%2")
                    .arg(QString::fromStdString(kvota), bestProj(pts[0], pts[1]));
    }
  }
  if (newLines.isEmpty()) {
    this->statusBar()->showMessage(tr("No new kvota dimensions to materialize."), 3000);
    return;
  }

  QString appended = text;
  if (!appended.isEmpty() && !appended.endsWith('\n')) appended += '\n';
  appended += newLines.join('\n') + '\n';
  activeEditor->setPlainText(appended);
  this->statusBar()->showMessage(
    tr("Materialized %1 kvota dimension(s). Press F5 to update.").arg(newLines.size()), 4000);
}

void MainWindow::onDimensionOffsetChanged(int viewIndex, int id, double offsetPx, bool finished)
{
  // Only persist on release; live drag already updates the overlay in the dock.
  if (!finished || id < 0 || !activeEditor) return;

  const double pxPerMm =
    (viewIndex >= 0 && viewIndex < 6 && this->viewPxPerMm[viewIndex] > 1e-9)
      ? this->viewPxPerMm[viewIndex]
      : 1.0;
  const double offsetMm = offsetPx / pxPerMm;

  QStringList lines = activeEditor->toPlainText().split('\n');
  if (id >= lines.size()) return;
  lines[id] = setLineToken(lines.at(id), "offset", QString::number(offsetMm, 'g', 4));
  activeEditor->setPlainText(lines.join('\n'));
}

void MainWindow::onDimensionAngleChanged(int /*viewIndex*/, int id, double angleDeg, bool finished)
{
  // Only persist on release; the live drag already rotates the overlay in the dock.
  if (!finished || id < 0 || !activeEditor) return;
  QStringList lines = activeEditor->toPlainText().split('\n');
  if (id >= lines.size()) return;
  // Normalise to [0,360) so the written angle stays tidy.
  double a = std::fmod(angleDeg, 360.0);
  if (a < 0) a += 360.0;
  lines[id] = setLineToken(lines.at(id), "angle", QString::number(a, 'g', 4));
  activeEditor->setPlainText(lines.join('\n'));
}

void MainWindow::onLabelOffsetChanged(int viewIndex, int id, double alongPx, double perpPx,
                                      bool finished)
{
  if (!finished || id < 0 || !activeEditor) return;

  const double pxPerMm =
    (viewIndex >= 0 && viewIndex < 6 && this->viewPxPerMm[viewIndex] > 1e-9)
      ? this->viewPxPerMm[viewIndex]
      : 1.0;
  const double alongMm = alongPx / pxPerMm;
  const double perpMm = perpPx / pxPerMm;

  QStringList lines = activeEditor->toPlainText().split('\n');
  if (id >= lines.size()) return;
  const QString value =
    QString("%1,%2").arg(alongMm, 0, 'g', 4).arg(perpMm, 0, 'g', 4);
  lines[id] = setLineToken(lines.at(id), "laboff", value);
  activeEditor->setPlainText(lines.join('\n'));
}

void MainWindow::onLabelAnchorClicked(const QString& projKey, const QString& name)
{
  if (name.isEmpty() || projKey.isEmpty() || !activeEditor) return;
  const QString proj = projKey;  // "front".."bottom" or "section<name>"
  const QString labLine = QString("// @label %1 proj=%2").arg(name, proj);

  insertAnnotationLine(labLine);  // into the active view's block (Fáze 15E)
  refreshDrawingViews();
  this->statusBar()->showMessage(tr("Added label %1 (proj=%2)").arg(name, proj), 4000);
}

QString MainWindow::nameCameraInSource(int cameraIndex)
{
  if (!activeEditor || !this->rootNode) return {};
  const auto cameras = collectCameras(this->rootNode);
  if (cameraIndex < 0 || cameraIndex >= static_cast<int>(cameras.size())) return {};
  const CameraNode *node = cameras[cameraIndex].node;
  if (!node || !node->modinst) return {};

  // First camN not taken by another camera in the model.
  std::set<std::string> taken;
  for (const auto& info : cameras) {
    if (info.node) taken.insert(info.node->camera_name);
  }
  QString newName;
  for (int n = 1;; ++n) {
    const QString candidate = QString("cam%1").arg(n);
    if (!taken.count(candidate.toStdString())) { newName = candidate; break; }
  }

  // Insert name="camN" as the first argument of the camera( … ) call. The
  // instantiation's location gives the line and column of the `camera` token; the
  // opening parenthesis follows it on the same line.
  const Location& loc = node->modinst->location();
  QStringList lines = activeEditor->toPlainText().split('\n');
  const int li = loc.firstLine() - 1;
  if (li < 0 || li >= lines.size()) return {};
  const int from = std::clamp<int>(loc.firstColumn() - 1, 0, lines.at(li).length());
  const int open = lines.at(li).indexOf('(', from);
  if (open < 0) return {};

  QString line = lines.at(li);
  const QString rest = line.mid(open + 1);
  const bool emptyArgs = rest.trimmed().startsWith(')');  // camera() -> no comma
  line.insert(open + 1, QString("name=\"%1\"%2").arg(newName, emptyArgs ? "" : ", "));
  lines[li] = line;
  activeEditor->setPlainText(lines.join('\n'));
  return newName;
}

void MainWindow::onCameraLabelAnchorClicked(int cameraIndex, const QString& cameraName,
                                            const QString& name)
{
  if (name.isEmpty() || !activeEditor) return;

  // A // @label targets a camera by name, so an unnamed camera() gets one written
  // into the source first (which changes the model -> re-preview instead of just
  // reprojecting the overlay).
  QString camera = cameraName;
  const bool autoNamed = camera.isEmpty();
  if (autoNamed) {
    camera = nameCameraInSource(cameraIndex);
    if (camera.isEmpty()) {
      this->statusBar()->showMessage(
        tr("This camera has no name — add name=\"…\" to its camera() call."), 6000);
      return;
    }
  }

  const QString labLine = QString("// @label %1 proj=cam:%2").arg(name, camera);
  const QStringList lines = activeEditor->toPlainText().split('\n');
  activeEditor->setCursorPosition(lines.size() - 1, lines.isEmpty() ? 0 : lines.last().length());
  QString insertText;
  if (!lines.isEmpty() && !lines.last().isEmpty()) insertText += "\n";
  insertText += labLine + "\n";
  activeEditor->insertOrReplaceText(insertText);

  if (autoNamed) {
    actionRenderPreview();  // camera() changed: re-parse, then the snapshots refresh
    this->statusBar()->showMessage(
      tr("Named the camera \"%1\" and added label %2").arg(camera, name), 4000);
  } else {
    refreshCameras();  // the // @label is a comment: reproject, no geometry change
    this->statusBar()->showMessage(tr("Added label %1 (proj=cam:%2)").arg(name, camera), 4000);
  }
}

void MainWindow::onLabelTextMoved(int viewIndex, int id, double dxMm, double dyMm, bool finished)
{
  (void)viewIndex;
  if (!finished || id < 0 || !activeEditor) return;
  QStringList lines = activeEditor->toPlainText().split('\n');
  if (id >= lines.size()) return;
  // The overlay offset is view-local, y-down; laboff in source is the screen frame
  // (dx = right, dy = up), so the vertical component flips sign.
  const double du = dxMm, dv = -dyMm;
  const QString value = QString("%1,%2").arg(du, 0, 'g', 4).arg(dv, 0, 'g', 4);
  lines[id] = setLineToken(lines.at(id), "laboff", value);
  activeEditor->setPlainText(lines.join('\n'));
}

void MainWindow::onLabelDoubleClicked(int id)
{
  if (id < 0 || !activeEditor) return;
  QStringList lines = activeEditor->toPlainText().split('\n');
  if (id >= lines.size()) return;
  QString line = lines.at(id);

  auto tokenValue = [&](const QString& re) {
    const auto m = QRegularExpression(re).match(line);
    return m.hasMatch() ? m.captured(1) : QString();
  };
  QString curText = tokenValue(R"RX(text="([^"]*)")RX");
  if (curText.isEmpty()) curText = tokenValue(R"(\btext=(\S+))");
  const QString curProj = tokenValue(R"(\bproj=(\S+))");
  const QString curLeader = tokenValue(R"(\bleader=(\w+))");

  QDialog dialog(this);
  dialog.setWindowTitle(tr("Edit label"));
  auto *form = new QFormLayout(&dialog);

  auto *textEdit = new QLineEdit(curText, &dialog);
  textEdit->setPlaceholderText(tr("Override the label() text (blank = keep it)"));
  form->addRow(tr("Text"), textEdit);

  auto *projBox = new QComboBox(&dialog);
  projBox->setEditable(true);  // lets the user type cam:<name>
  projBox->addItems({tr("(all)"), "front", "back", "left", "right", "top", "bottom"});
  projBox->setCurrentText(curProj.isEmpty() ? tr("(all)") : curProj);
  projBox->setToolTip(tr("front…bottom = an ortho view; cam:<name> = a camera; "
                         "(all) = every ortho view"));
  form->addRow(tr("Projection"), projBox);

  auto *leaderCheck = new QCheckBox(tr("Leader line to the anchor"), &dialog);
  leaderCheck->setChecked(curLeader.toLower() != "off");
  form->addRow(tr("Leader"), leaderCheck);

  auto *buttons =
    new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  auto *deleteButton = buttons->addButton(tr("Delete"), QDialogButtonBox::DestructiveRole);
  deleteButton->setToolTip(tr("Remove this label (the // @label line)."));
  form->addRow(buttons);
  connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  bool deleteRequested = false;
  connect(deleteButton, &QPushButton::clicked, &dialog, [&] {
    deleteRequested = true;
    dialog.accept();
  });

  if (dialog.exec() != QDialog::Accepted) return;

  if (deleteRequested) {
    lines.removeAt(id);
    activeEditor->setPlainText(lines.join('\n'));
    refreshDrawingViews();
    this->statusBar()->showMessage(tr("Label deleted."), 3000);
    return;
  }

  // Rewrite the tokens. Text may contain spaces -> always quote it.
  line.remove(QRegularExpression(R"RX(\s*text="[^"]*")RX"));
  line.remove(QRegularExpression(R"(\s*\btext=\S+)"));
  const QString textVal = textEdit->text().trimmed();
  if (!textVal.isEmpty()) line += " text=\"" + textVal + "\"";

  line.remove(QRegularExpression(R"(\s*\bproj=\S+)"));
  const QString projVal = projBox->currentText().trimmed();
  if (!projVal.isEmpty() && projVal != tr("(all)")) line += " proj=" + projVal;

  if (!leaderCheck->isChecked()) line = setLineToken(line, "leader", "off");
  else line.remove(QRegularExpression(R"(\s*\bleader=\S+)"));

  lines[id] = line;
  activeEditor->setPlainText(lines.join('\n'));
  refreshDrawingViews();
}

// If the anchor reference `ref` ("A" or "A#index") resolves to a dimref() that
// carries a radial size, return "diameter" or "radius" (whichever was given, d=
// taking precedence); otherwise an empty string. Used so a repeat-click on such
// an anchor emits a ⌀/R dimension instead of a two-point length.
QString MainWindow::radialKindOfAnchor(const QString& ref) const
{
  if (!this->rootNode) return {};
  const bool byLine = ref.startsWith("@line");
  const int wantLine = byLine ? ref.mid(5).toInt() : -1;
  const int hash = ref.indexOf('#');
  const std::string base = (hash >= 0 ? ref.left(hash) : ref).toStdString();
  const int idx = hash >= 0 ? ref.mid(hash + 1).toInt() : 0;
  int seen = 0;
  for (const auto& d : collectDimrefs(this->rootNode)) {
    if (!d.node) continue;
    if (byLine) {
      if (!d.node->modinst || d.node->modinst->location().firstLine() != wantLine) continue;
    } else {
      if (d.node->dimref_name != base) continue;
      if (seen++ != idx) continue;
    }
    if (d.node->diameter >= 0) return QStringLiteral("diameter");
    if (d.node->radius >= 0) return QStringLiteral("radius");
    return {};
  }
  return {};
}

void MainWindow::onDrawingAnchorClicked(const QString& projKey, const QString& name)
{
  if (name.isEmpty() || projKey.isEmpty() || !activeEditor) return;

  const QString proj = projKey;  // "front".."bottom" or "section<name>" 

  // Write a // @dim line into the active view's block (before its closing brace),
  // or at end of file when the active view is the root (Fáze 15E). An @dim line is
  // a comment, so the geometry (and dimref world positions) is unchanged and the
  // overlay updates without an F5.
  auto appendDim = [&](const QString& dimLine, const QString& status) {
    insertAnnotationLine(dimLine);
    refreshDrawingViews();
    this->statusBar()->showMessage(status, 4000);
  };

  // First click: remember the anchor and wait for the second. If the anchor is a
  // radial dimref (d=/r=), a repeat click on it makes a ⌀/R dimension.
  if (this->pendingDimAnchor.isEmpty()) {
    this->pendingDimAnchor = name;
    const QString radial = radialKindOfAnchor(name);
    this->statusBar()->showMessage(
      radial.isEmpty()
        ? tr("Dimension: first point \"%1\" — click the second anchor.").arg(name)
        : tr("Dimension: \"%1\" — click a second anchor for a length, or click it "
             "again for a %2.").arg(name, radial == "radius" ? tr("radius") : tr("diameter")),
      5000);
    return;
  }
  // Clicking the same anchor again: emit a radial dimension if it carries a size,
  // otherwise cancel the pending pick.
  if (name == this->pendingDimAnchor) {
    this->pendingDimAnchor.clear();
    const QString radial = radialKindOfAnchor(name);
    if (radial.isEmpty()) {
      this->statusBar()->showMessage(tr("Dimension cancelled."), 2000);
      return;
    }
    appendDim(QString("// @dim %1 kind=%2 proj=%3").arg(name, radial, proj),
              tr("Added %1 dimension for %2.")
                .arg(radial == "radius" ? tr("radius") : tr("diameter"), name));
    return;
  }

  const QString first = this->pendingDimAnchor;
  this->pendingDimAnchor.clear();
  // Default to the in-plane (planar) distance: in a technical drawing you dimension
  // what you see in that projection, not the true 3D distance.
  appendDim(QString("// @dim %1 %2 proj=%3 measure=planar").arg(first, name, proj),
            tr("Added dimension %1 ↔ %2.").arg(first, name));
}

namespace {

// A dimension resolved to two world-space endpoints, plus which projection it
// belongs to (empty = all views) and its label text.
enum class DimKind { Length, Diameter, Radius, Angle };

struct ResolvedDim {
  Eigen::Vector3d a;
  Eigen::Vector3d b;
  Eigen::Vector3d c{Eigen::Vector3d::Zero()};  // vertex, for angle dims (a,b = ends)
  DimKind kind{DimKind::Length};
  QString proj;         // "front" | "side" | "top" | "" (all)
  QString measure;      // "direct"(default) | "planar" | "h" | "v"
  QString label;        // final label text (template already substituted)
  double offsetMm{0};   // dimension line offset from the anchor line, model units
  bool arrowsOut{false};  // arrowheads outside the extension lines (for tight dims)
  bool leader{true};      // draw a leader to a dragged-away label (leader=on|off)
  double labAlongMm{0};  // label offset along the dimension line, model units
  double labPerpMm{0};   // label offset perpendicular to the dimension line
  int sourceLine{-1};   // 0-based editor line of the // @dim (for write-back)
  int jumpLine{-1};     // 0-based line for right-click jump (kvota= -> its dimref)
};

// World axis indices that map to the horizontal / vertical screen directions of
// an ortho view: front h=X v=Z, top h=X v=Y, side h=Y v=Z (see refreshDrawingViews
// orthoViews). Defaults to front when proj is unspecified.
void measureAxes(const QString& proj, int& hAxis, int& vAxis)
{
  // The screen sign (mirroring) does not matter here: measure lengths use the
  // absolute component, so only the axis pair per projection group is needed.
  if (proj == "top" || proj == "bottom") { hAxis = 0; vAxis = 1; }        // XY
  else if (proj == "side" || proj == "left" || proj == "right") { hAxis = 1; vAxis = 2; }  // YZ
  else { hAxis = 0; vAxis = 2; }  // front / back (and default): XZ
}

// Section frames by their proj token ("section" + lowercased name), so that a
// dimension in a section can be measured in the plane it is actually drawn in.
using SectionFrames = QHash<QString, Transform3d>;

// World-space directions that map to the horizontal / vertical screen directions
// of a view. The six standard views run along world axes, but a section is an
// arbitrary frame in space (projectSection draws screen x = the frame's local X,
// y = its local Y), which no pair of axis indices can express — hence the frame
// lookup. Falls back to the standard views, and to front when proj is unknown.
void measureDirs(const QString& proj, const SectionFrames& sectionFrames,
                 Eigen::Vector3d& hDir, Eigen::Vector3d& vDir)
{
  const auto it = sectionFrames.constFind(proj.toLower());
  if (it != sectionFrames.constEnd()) {
    // Normalized because the accumulated transform may carry a scale; a
    // non-uniform one would skew the projection itself, not just the value.
    hDir = it->linear().col(0).normalized();
    vDir = it->linear().col(1).normalized();
    return;
  }
  int hAxis, vAxis;
  measureAxes(proj, hAxis, vAxis);
  hDir = Eigen::Vector3d::Unit(hAxis);
  vDir = Eigen::Vector3d::Unit(vAxis);
}

// Measured length of a dimension per its measure mode, relative to its
// projection plane. direct = true 3D distance |b-a|; planar = distance in the
// view plane; h/v = the horizontal/vertical component in the view. Holds
// h² + v² = planar².
double dimLength(const Eigen::Vector3d& a, const Eigen::Vector3d& b,
                 const QString& measure, const QString& proj,
                 const SectionFrames& sectionFrames)
{
  const Eigen::Vector3d d = b - a;
  if (measure.isEmpty() || measure == "direct") return d.norm();
  Eigen::Vector3d hDir, vDir;
  measureDirs(proj, sectionFrames, hDir, vDir);
  const double h = std::abs(d.dot(hDir));
  const double v = std::abs(d.dot(vDir));
  if (measure == "h") return h;
  if (measure == "v") return v;
  if (measure == "planar") return std::hypot(h, v);
  return d.norm();  // unknown token -> direct
}

// A dimref anchor with its resolved world position and 0-based source line.
struct AnchorRec { QString label; QString ref; Eigen::Vector3d pos; int sourceLine; };
// The model's dimref anchors + resolved dimensions, independent of any view.
struct ResolvedDrawing {
  std::vector<AnchorRec> anchors;
  std::vector<ResolvedDim> dims;
};

// Resolve dimref anchors and every dimension (// @dim lines + kvota= sugar) to
// world-space geometry. Shared by the drawing-dock overlay and the vector line-
// drawing export. `warn` emits the duplicate-name warning once (secondary
// callers pass false to avoid repeats).
ResolvedDrawing resolveDrawing(const std::vector<DimrefInfo>& dimrefs,
                               const QString& source, bool warn,
                               const SectionFrames& sectionFrames)
{
  // World positions per dimref name, in tree (DFS) order — index = occurrence, so
  // a duplicated name can be pinned as name#index (a plain name takes the first).
  std::map<std::string, std::vector<Eigen::Vector3d>> posByName;
  std::map<std::string, std::vector<const DimrefNode *>> nodeByName;  // parallel to posByName
  // An unnamed dimref (e.g. dimref(d=15)) has no name to reference, yet still needs
  // to be clickable/dimensionable — key it by its 1-based source line as @line<N>.
  std::map<int, Eigen::Vector3d> posByLine;
  std::map<int, const DimrefNode *> nodeByLine;
  std::map<std::string, std::vector<Eigen::Vector3d>> kvotaGroups;
  std::map<std::string, int> kvotaFirstLine;  // first dimref line per kvota (jump target)
  std::vector<AnchorRec> anchorRecs;
  for (const auto& d : dimrefs) {
    if (!d.node) continue;
    const Eigen::Vector3d pos = d.world.translation();
    const int line1 = d.node->modinst ? d.node->modinst->location().firstLine() : -1;
    QString label = QString::fromStdString(d.node->dimref_name);
    if (label.isEmpty()) label = QString::fromStdString(d.node->kvota_name);
    QString ref = label;
    if (!d.node->dimref_name.empty()) {
      auto& v = posByName[d.node->dimref_name];
      // ref = name#index; label shows the same when duplicated (see below).
      ref = label + "#" + QString::number(static_cast<int>(v.size()));
      v.push_back(pos);
      nodeByName[d.node->dimref_name].push_back(d.node);
    } else if (line1 > 0) {
      // Unnamed: reference by source line. The dot carries a compact marker (the
      // value shows in the actual dimension once created), not a full label.
      ref = QStringLiteral("@line%1").arg(line1);
      if (d.node->diameter >= 0) label = QStringLiteral("⌀");
      else if (d.node->radius >= 0) label = QStringLiteral("R");
      else label = QStringLiteral("•");
      posByLine[line1] = pos;
      nodeByLine[line1] = d.node;
    }
    // 0-based editor line of the dimref() call, for right-click jump-to-source.
    const int srcLine = line1 > 0 ? line1 - 1 : -1;
    anchorRecs.push_back({label, ref, pos, srcLine});
    if (!d.node->kvota_name.empty()) {
      kvotaGroups[d.node->kvota_name].push_back(pos);
      if (!kvotaFirstLine.count(d.node->kvota_name)) kvotaFirstLine[d.node->kvota_name] = srcLine;
    }
  }

  // Duplicate names are ambiguous in a plain `@dim A B`. Warn once per name, and
  // for display/click use the plain name when unique but name#index when not.
  for (const auto& [name, positions] : posByName) {
    if (warn && positions.size() > 1) {
      LOG(message_group::UI_Warning,
          "dimref name '%1$s' is used %2$d times; a plain @dim reference takes the "
          "first — qualify as %1$s#index (0..%3$d) to pick another.",
          name, static_cast<int>(positions.size()), static_cast<int>(positions.size() - 1));
    }
  }
  for (auto& rec : anchorRecs) {
    if (rec.ref.startsWith("@line")) continue;  // synthetic line ref: keep as-is
    const std::string base = rec.ref.left(rec.ref.indexOf('#')).toStdString();
    const bool duplicated =
      !base.empty() && posByName.count(base) && posByName[base].size() > 1;
    if (!duplicated) rec.ref = rec.label;  // unique -> clean, unqualified ref
    else rec.label = rec.ref;              // duplicated -> show name#index
  }

  // Resolve an @dim endpoint token ("A" or "A#index") to a world position.
  // Plain name -> first occurrence; #index -> that occurrence.
  auto resolveRef = [&](const QString& tok, Eigen::Vector3d& out) -> bool {
    if (tok.startsWith("@line")) {  // unnamed dimref, keyed by source line
      const auto it = posByLine.find(tok.mid(5).toInt());
      if (it == posByLine.end()) return false;
      out = it->second;
      return true;
    }
    const int hash = tok.indexOf('#');
    const QString base = hash >= 0 ? tok.left(hash) : tok;
    int idx = hash >= 0 ? tok.mid(hash + 1).toInt() : 0;
    const auto it = posByName.find(base.toStdString());
    if (it == posByName.end() || idx < 0 || idx >= static_cast<int>(it->second.size()))
      return false;
    out = it->second[idx];
    return true;
  };
  // Resolve an @dim token to the DimrefNode of that occurrence (for d=/r= values).
  auto resolveNode = [&](const QString& tok) -> const DimrefNode * {
    if (tok.startsWith("@line")) {
      const auto it = nodeByLine.find(tok.mid(5).toInt());
      return it == nodeByLine.end() ? nullptr : it->second;
    }
    const int hash = tok.indexOf('#');
    const QString base = hash >= 0 ? tok.left(hash) : tok;
    int idx = hash >= 0 ? tok.mid(hash + 1).toInt() : 0;
    const auto it = nodeByName.find(base.toStdString());
    if (it == nodeByName.end() || idx < 0 || idx >= static_cast<int>(it->second.size()))
      return nullptr;
    return it->second[idx];
  };

  // Format a length with an optional fixed precision (prec>=0), else general.
  auto formatLen = [](double len, int prec) {
    return prec >= 0 ? QString::number(len, 'f', prec) : QString::number(len, 'g', 4);
  };
  // Build the final label: substitute %f in a template with the formatted length,
  // or use the computed length when no template is given.
  auto makeLabel = [&](const QString& tmpl, double len, int prec) {
    const QString num = formatLen(len, prec);
    if (tmpl.isEmpty()) return num;
    if (tmpl.contains("%f")) return QString(tmpl).replace("%f", num);
    return tmpl;  // literal text, no value
  };

  std::vector<ResolvedDim> dims;

  // 2. Explicit // @dim lines from the source. Forms:
  //      // @dim A B     [proj=] [measure=] [offset=] [label=…] [prec=N] [arrows=in|out]
  //      // @dim kvota=N  [proj=] [measure=] [offset=] [label=…] [prec=N] [arrows=in|out]
  //      // @dim A kind=diameter|radius [angle=deg] [proj=]   (value from dimref d=/r=)
  //      // @dim angle V A B [proj=] [label=]                 (exact 3D angle at V)
  //      // @dim chain|running A B C… [proj=] [offset=] [step=]  (dimension series)
  //   label may be quoted to hold spaces/%f: label="šířka %f mm"
  static const QRegularExpression dimRe(R"(^\s*//\s*@dim\s+(.*\S)\s*$)");
  static const QRegularExpression quotedLabelRe(R"RX(label="([^"]*)")RX");
  std::set<QString> materializedKvotas;  // kvota names already written as @dim
  const QStringList sourceLines = source.split('\n');
  for (int li = 0; li < sourceLines.size(); ++li) {
    const auto m = dimRe.match(sourceLines.at(li));
    if (!m.hasMatch()) continue;

    // Pull out a quoted label first so its spaces don't break tokenizing.
    QString rest = m.captured(1);
    QString labelTemplate;
    bool haveLabel = false;
    const auto lm = quotedLabelRe.match(rest);
    if (lm.hasMatch()) {
      labelTemplate = lm.captured(1);
      haveLabel = true;
      rest = rest.left(lm.capturedStart()) + rest.mid(lm.capturedEnd());
    }

    QStringList toks = rest.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if (toks.isEmpty()) continue;

    // Chain / running: one line lays out a series of length dimensions over an
    // ordered list of anchors, all in one direction.
    //   // @dim chain   A B C D   -> consecutive A-B, B-C, C-D on one line
    //   // @dim running A B C D   -> A-B, A-C, A-D from a common baseline, stacked
    // Shared options: [proj=] [measure=] [offset=] [step=] [prec=] [label=].
    if (toks.first() == "chain" || toks.first() == "running") {
      const bool running = toks.first() == "running";
      QStringList refs;
      int t = 1;
      for (; t < toks.size() && !toks[t].contains('='); ++t) refs << toks[t];
      if (refs.size() < 2) continue;

      QString proj, measure;
      double baseOffset = 0.0, step = 10.0;
      int prec = -1;
      for (; t < toks.size(); ++t) {
        const QString& tok = toks[t];
        if (tok.startsWith("proj=")) proj = normalizeProj(tok.mid(5));
        else if (tok.startsWith("measure=")) measure = tok.mid(8).toLower();
        else if (tok.startsWith("offset=")) baseOffset = tok.mid(7).toDouble();
        else if (tok.startsWith("step=")) step = tok.mid(5).toDouble();
        else if (tok.startsWith("prec=")) prec = tok.mid(5).toInt();
        else if (tok.startsWith("label=")) { labelTemplate = tok.mid(6); haveLabel = true; }
      }

      std::vector<Eigen::Vector3d> pts;
      bool ok = true;
      for (const QString& ref : refs) {
        Eigen::Vector3d p;
        if (!resolveRef(ref, p)) { ok = false; break; }
        pts.push_back(p);
      }
      if (!ok) continue;

      auto pushSeg = [&](const Eigen::Vector3d& A, const Eigen::Vector3d& B, double off) {
        ResolvedDim rd;
        rd.a = A;
        rd.b = B;
        rd.proj = proj;
        rd.measure = measure;
        rd.offsetMm = off;
        rd.sourceLine = li;  // whole chain shares the line; drag shifts the base
        rd.label = makeLabel(haveLabel ? labelTemplate : QString(),
                             dimLength(A, B, measure, proj, sectionFrames), prec);
        dims.push_back(rd);
      };
      if (running) {
        for (size_t i = 1; i < pts.size(); ++i) pushSeg(pts[0], pts[i], baseOffset + (i - 1) * step);
      } else {
        for (size_t i = 0; i + 1 < pts.size(); ++i) pushSeg(pts[i], pts[i + 1], baseOffset);
      }
      continue;
    }

    ResolvedDim rd;
    rd.sourceLine = li;
    int firstOpt = 0;    // index of first option token
    QString radialRef;   // set for radial dims; endpoints computed after options
    if (toks.first() == "angle") {
      // Angle dimension: vertex + two ends (// @dim angle V A B). The value is
      // the exact 3D angle at V between VA and VB.
      if (toks.size() < 4) continue;
      if (!resolveRef(toks[1], rd.c) || !resolveRef(toks[2], rd.a) ||
          !resolveRef(toks[3], rd.b))
        continue;
      rd.kind = DimKind::Angle;
      firstOpt = 4;
    } else if (toks.first().startsWith("kvota=")) {
      const QString kv = toks.first().mid(6);
      const auto it = kvotaGroups.find(kv.toStdString());
      if (it == kvotaGroups.end() || it->second.size() < 2) continue;
      rd.a = it->second[0];
      rd.b = it->second[1];
      materializedKvotas.insert(kv);
      firstOpt = 1;
    } else if (toks.size() >= 2 && !toks[1].contains('=')) {
      // Length dimension: two anchors.
      if (!resolveRef(toks[0], rd.a) || !resolveRef(toks[1], rd.b)) continue;  // unknown anchor
      firstOpt = 2;
    } else {
      // Radial dimension (// @dim A kind=diameter|radius [angle=] [proj=]): a
      // single anchor whose dimref carries the size (d=/r=). Resolved below.
      radialRef = toks[0];
      firstOpt = 1;
    }

    int prec = -1;
    double angleDeg = 0.0;
    QString kindTok;
    for (int t = firstOpt; t < toks.size(); ++t) {
      const QString& tok = toks[t];
      if (tok.startsWith("proj=")) rd.proj = normalizeProj(tok.mid(5));
      else if (tok.startsWith("measure=")) rd.measure = tok.mid(8).toLower();
      else if (tok.startsWith("kind=")) kindTok = tok.mid(5).toLower();
      else if (tok.startsWith("angle=")) angleDeg = tok.mid(6).toDouble();
      else if (tok.startsWith("offset=")) rd.offsetMm = tok.mid(7).toDouble();
      else if (tok.startsWith("prec=")) prec = tok.mid(5).toInt();
      else if (tok.startsWith("arrows=")) rd.arrowsOut = (tok.mid(7).toLower() == "out");
      else if (tok.startsWith("leader=")) rd.leader = (tok.mid(7).toLower() != "off");
      else if (tok.startsWith("laboff=")) {
        const QStringList parts = tok.mid(7).split(',');
        if (parts.size() == 2) {
          rd.labAlongMm = parts[0].toDouble();
          rd.labPerpMm = parts[1].toDouble();
        }
      }
      else if (tok.startsWith("label=")) { labelTemplate = tok.mid(6); haveLabel = true; }
      else if (!haveLabel) { labelTemplate = tok; haveLabel = true; }  // bare token
    }

    double dispValue = 0.0;  // number substituted into the label
    if (rd.kind == DimKind::Angle) {
      const Eigen::Vector3d va = rd.a - rd.c;
      const Eigen::Vector3d vb = rd.b - rd.c;
      if (va.norm() < 1e-9 || vb.norm() < 1e-9) continue;
      const double cosang = std::clamp(va.normalized().dot(vb.normalized()), -1.0, 1.0);
      dispValue = std::acos(cosang) * 180.0 / M_PI;
    } else if (!radialRef.isEmpty()) {
      Eigen::Vector3d center;
      const DimrefNode *node = resolveNode(radialRef);
      if (!resolveRef(radialRef, center) || !node) continue;
      // Value from source (the mesh has no true circle); d= and r= imply each other.
      const double diameter = node->diameter >= 0 ? node->diameter
                              : (node->radius >= 0 ? node->radius * 2.0 : -1.0);
      const double radius = node->radius >= 0 ? node->radius
                            : (node->diameter >= 0 ? node->diameter / 2.0 : -1.0);
      if (diameter < 0 && radius < 0) continue;  // no size on the dimref
      const bool wantRadius = (kindTok == "radius" || kindTok == "r");
      rd.kind = wantRadius ? DimKind::Radius : DimKind::Diameter;
      dispValue = wantRadius ? radius : diameter;
      // Draw direction: the view's horizontal axis rotated by angle=, in-plane.
      Eigen::Vector3d hv, vv;
      measureDirs(rd.proj.isEmpty() ? "front" : rd.proj, sectionFrames, hv, vv);
      const double rad = angleDeg * M_PI / 180.0;
      const Eigen::Vector3d dir = std::cos(rad) * hv + std::sin(rad) * vv;
      if (wantRadius) { rd.a = center; rd.b = center + dir * radius; }
      else { rd.a = center - dir * (diameter / 2.0); rd.b = center + dir * (diameter / 2.0); }
    } else {
      dispValue = dimLength(rd.a, rd.b, rd.measure, rd.proj, sectionFrames);
    }

    // Radial dims get a ⌀/R prefix by default; a custom label= still wins.
    if (!haveLabel && rd.kind == DimKind::Diameter) {
      labelTemplate = QString::fromUtf8("⌀%f"); haveLabel = true;
    } else if (!haveLabel && rd.kind == DimKind::Radius) {
      labelTemplate = "R%f"; haveLabel = true;
    } else if (!haveLabel && rd.kind == DimKind::Angle) {
      labelTemplate = QString::fromUtf8("%f°"); haveLabel = true;
    }
    rd.label = makeLabel(haveLabel ? labelTemplate : QString(), dispValue, prec);
    dims.push_back(rd);
  }

  // 3. kvota= sugar: two dimrefs sharing a kvota name auto-form a dimension,
  //    UNLESS that kvota has already been materialized into an @dim line above.
  for (const auto& [kvota, pts] : kvotaGroups) {
    if (pts.size() < 2) continue;
    if (materializedKvotas.count(QString::fromStdString(kvota))) continue;
    ResolvedDim rd;
    rd.a = pts[0];
    rd.b = pts[1];
    rd.label = makeLabel(QString(), (rd.b - rd.a).norm(), -1);
    // sourceLine stays -1 (not draggable), but right-click can still jump to the
    // kvota's first dimref() call.
    const auto jl = kvotaFirstLine.find(kvota);
    rd.jumpLine = jl != kvotaFirstLine.end() ? jl->second : -1;
    dims.push_back(rd);  // id stays -1: in-memory, not draggable until materialized
  }

  return {std::move(anchorRecs), std::move(dims)};
}

// A label() marker resolved for one target projection: the world anchor to draw a
// leader to, the text to show, which projection it belongs to, and the text
// offset from the projected anchor (in the projection plane, mm: dx = screen
// right, dy = screen up). `proj` is "front".."bottom" for an ortho view,
// "cam:<name>" for a camera, or "" for all ortho views. sourceLine is the 0-based
// // @label line (for laboff= write-back + right-click jump).
struct ResolvedLabel {
  Eigen::Vector3d anchor{Eigen::Vector3d::Zero()};
  QString text;
  QString proj;
  double labDxMm{0};
  double labDyMm{0};
  bool leader{true};
  int sourceLine{-1};
};

// A label() marker with its world position, keyed for reference resolution.
struct LabelAnchorRec {
  QString name;                 // label_name (may be empty)
  QString ref;                  // name or name#index (duplicated) / @line<N> (unnamed)
  QString text;                 // node text (default shown text)
  Eigen::Vector3d pos;
  int sourceLine{-1};           // 0-based label() line, for creation / jump
};

// The model's label markers plus the resolved // @label callouts.
struct ResolvedLabels {
  std::vector<LabelAnchorRec> anchors;
  std::vector<ResolvedLabel> labels;
};

// Resolve label() markers + every // @label line to world-space callouts, shared
// by the drawing-dock overlay and the camera-snapshot overlay. Mirrors
// resolveDrawing() but for the text-annotation family.
//   // @label NAME [proj=front|…|cam:<camera>] [laboff=dx,dy] [text="…"] [leader=on|off]
ResolvedLabels resolveLabels(const std::vector<LabelInfo>& labelInfos, const QString& source)
{
  ResolvedLabels out;

  // World positions + nodes per label name, in tree (DFS) order — index = occurrence.
  std::map<std::string, std::vector<Eigen::Vector3d>> posByName;
  std::map<std::string, std::vector<const LabelNode *>> nodeByName;
  std::map<int, Eigen::Vector3d> posByLine;  // unnamed labels keyed by 1-based line
  for (const auto& li : labelInfos) {
    if (!li.node) continue;
    const Eigen::Vector3d pos = li.world.translation();
    const int line1 = li.node->modinst ? li.node->modinst->location().firstLine() : -1;
    const QString name = QString::fromStdString(li.node->label_name);
    const QString text = QString::fromStdString(li.node->text);
    QString ref = name;
    if (!name.isEmpty()) {
      auto& v = posByName[li.node->label_name];
      ref = name + "#" + QString::number(static_cast<int>(v.size()));
      v.push_back(pos);
      nodeByName[li.node->label_name].push_back(li.node);
    } else if (line1 > 0) {
      ref = QStringLiteral("@line%1").arg(line1);
      posByLine[line1] = pos;
    }
    out.anchors.push_back({name, ref, text, pos, line1 > 0 ? line1 - 1 : -1});
  }
  // Collapse name#0 to a clean name when the name is unique (parity with dimref).
  for (auto& rec : out.anchors) {
    if (rec.ref.startsWith("@line")) continue;
    const std::string base = rec.ref.left(rec.ref.indexOf('#')).toStdString();
    const bool dup = !base.empty() && posByName.count(base) && posByName[base].size() > 1;
    if (!dup) rec.ref = rec.name;
    else rec.name = rec.ref;  // duplicated -> show name#index on the clickable dot
  }

  // Resolve a // @label reference token ("A", "A#index", "@line<N>") to a world pos
  // and the LabelNode (for the default text).
  auto resolve = [&](const QString& tok, Eigen::Vector3d& pos, const LabelNode *&node) -> bool {
    node = nullptr;
    if (tok.startsWith("@line")) {
      const auto it = posByLine.find(tok.mid(5).toInt());
      if (it == posByLine.end()) return false;
      pos = it->second;
      return true;
    }
    const int hash = tok.indexOf('#');
    const QString base = hash >= 0 ? tok.left(hash) : tok;
    const int idx = hash >= 0 ? tok.mid(hash + 1).toInt() : 0;
    const auto it = posByName.find(base.toStdString());
    if (it == posByName.end() || idx < 0 || idx >= static_cast<int>(it->second.size()))
      return false;
    pos = it->second[idx];
    const auto nit = nodeByName.find(base.toStdString());
    if (nit != nodeByName.end() && idx < static_cast<int>(nit->second.size()))
      node = nit->second[idx];
    return true;
  };

  // A camera target keeps its verbatim name; an ortho token is normalized.
  auto normLabelProj = [](const QString& raw) -> QString {
    const QString p = raw.trimmed();
    if (p.startsWith("cam:", Qt::CaseInsensitive)) return "cam:" + p.mid(4);
    return normalizeProj(p);
  };

  static const QRegularExpression labRe(R"(^\s*//\s*@label\s+(.*\S)\s*$)");
  static const QRegularExpression quotedTextRe(R"RX(text="([^"]*)")RX");
  const QStringList lines = source.split('\n');
  for (int li = 0; li < lines.size(); ++li) {
    const auto m = labRe.match(lines.at(li));
    if (!m.hasMatch()) continue;

    // Pull a quoted text="…" out first so its spaces don't break tokenizing.
    QString rest = m.captured(1);
    QString textOverride;
    bool haveText = false;
    const auto tm = quotedTextRe.match(rest);
    if (tm.hasMatch()) {
      textOverride = tm.captured(1);
      haveText = true;
      rest = rest.left(tm.capturedStart()) + rest.mid(tm.capturedEnd());
    }
    QStringList toks = rest.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if (toks.isEmpty()) continue;

    ResolvedLabel rl;
    rl.sourceLine = li;
    const LabelNode *node = nullptr;
    if (!resolve(toks.first(), rl.anchor, node)) continue;  // unknown marker: skip
    rl.text = haveText ? textOverride
                       : (node ? QString::fromStdString(node->text) : toks.first());
    for (int t = 1; t < toks.size(); ++t) {
      const QString& tok = toks[t];
      if (tok.startsWith("proj=")) rl.proj = normLabelProj(tok.mid(5));
      else if (tok.startsWith("laboff=")) {
        const QStringList xy = tok.mid(7).split(',');
        if (xy.size() == 2) { rl.labDxMm = xy[0].toDouble(); rl.labDyMm = xy[1].toDouble(); }
      } else if (tok.startsWith("leader=")) rl.leader = (tok.mid(7).toLower() != "off");
      else if (tok.startsWith("text=")) { rl.text = tok.mid(5); }
    }
    out.labels.push_back(rl);
  }
  return out;
}

}  // namespace

void MainWindow::refreshCameras()
{
  const auto cameras = this->rootNode ? collectCameras(this->rootNode) : std::vector<CameraInfo>{};

  // Frustum markers in the interactive view are updated on every preview,
  // regardless of the dock's visibility.
  std::vector<GLView::CameraFrustum> frustums;
  double autoLength = 0.15 * this->qglview->cam.zoomValue();
  if (!(autoLength > 0)) autoLength = 20.0;
  for (const auto& info : cameras) {
    if (!info.node) continue;
    // Per-camera marker length in mm, or the zoom-scaled auto length.
    const double markerLength = info.node->marker > 0 ? info.node->marker : autoLength;
    const double fov = info.node->fov >= 0
                         ? info.node->fov
                         : camera_math::focalToFovDegrees(info.node->focal, info.node->sensor);
    const double aspect = info.node->pixel_height > 0
                            ? static_cast<double>(info.node->pixel_width) / info.node->pixel_height
                            : 1.0;
    const double halfHeight = markerLength * std::tan(fov * M_PI / 360.0);
    const double halfWidth = halfHeight * aspect;
    const Eigen::Matrix4d m = info.world.matrix();
    auto pt = [&](double x, double y, double z) {
      const Eigen::Vector4d p = m * Eigen::Vector4d(x, y, z, 1.0);
      return Eigen::Vector3d(p.x(), p.y(), p.z());
    };
    GLView::CameraFrustum fr;
    fr.apex = m.block<3, 1>(0, 3);
    fr.corners = {pt(-halfWidth, halfHeight, -markerLength), pt(halfWidth, halfHeight, -markerLength),
                  pt(halfWidth, -halfHeight, -markerLength),
                  pt(-halfWidth, -halfHeight, -markerLength)};
    fr.up = pt(0.0, 1.5 * halfHeight, -markerLength);
    frustums.push_back(fr);
  }
  this->qglview->setCameraFrustums(std::move(frustums));
  this->qglview->update();

  // Snapshots are expensive, so only render them while the dock is visible.
  if (!this->cameraDock || !this->cameraDock->isVisible()) return;
  if (!this->rootNode) {
    this->cameraDock->clearSnapshots();
    return;
  }

  QList<QImage> images;
  QStringList names;
  QList<QList<CameraLabel>> overlays;
  QList<QList<CameraLabel>> markerOverlays;
  ViewOptions options;  // OpenCSG previewer, no axes/edges/crosshairs

  // Resolve label() markers + // @label callouts once; each camera keeps those
  // targeting it (proj=cam:<name>).
  const QString source = activeEditor ? activeEditor->toPlainText() : QString();
  const auto labelInfos = collectLabels(this->rootNode);
  const ResolvedLabels resolvedLabels = resolveLabels(labelInfos, source);

  // Render the snapshots into an FBO on the interactive view's own GL context
  // (FboSnapshotView) rather than allocating a standalone GLX/EGL offscreen
  // context (render_camera_preview / OffscreenView). The standalone context
  // fails on some drivers and AppImage bundles ("Unable to obtain GL Context",
  // e.g. bundled Mesa libGLX shadowing the nvidia driver); the interactive
  // context is always known-good here.
  this->qglview->makeContextCurrent();
  for (const auto& info : cameras) {
    QImage image;
    QList<CameraLabel> camLabels;
    QList<CameraLabel> camMarkers;
    if (info.node) {
      FboSnapshotView view(info.node->pixel_width, info.node->pixel_height);
      if (view.isValid()) {
        configure_camera_snapshot(view, this->tree, options, info);
        image = view.toImage();

        // Project world points onto the snapshot. After configure_camera_snapshot()
        // the view holds valid GL matrices (set in paintGL); world -> clip -> image
        // pixels (y-down) by hand. Returns false when the point is behind the camera.
        const QString camTarget = "cam:" + QString::fromStdString(info.node->camera_name);
        Eigen::Map<const Eigen::Matrix4d> M(view.modelview);
        Eigen::Map<const Eigen::Matrix4d> P(view.projection);
        const Eigen::Matrix4d MVP = P * M;
        const int W = info.node->pixel_width, H = info.node->pixel_height;
        auto project = [&](const Eigen::Vector3d& p, QPointF& out) -> bool {
          const Eigen::Vector4d clip = MVP * Eigen::Vector4d(p.x(), p.y(), p.z(), 1.0);
          if (clip.w() <= 1e-9) return false;  // at or behind the camera plane
          out = QPointF((clip.x() / clip.w() * 0.5 + 0.5) * W,
                        (1.0 - (clip.y() / clip.w() * 0.5 + 0.5)) * H);
          return true;
        };

        // Callouts targeting this camera (proj=cam:<name>).
        for (const auto& lb : resolvedLabels.labels) {
          if (lb.proj.compare(camTarget, Qt::CaseInsensitive) != 0) continue;
          CameraLabel cl;
          if (!project(lb.anchor, cl.anchor)) continue;
          cl.off = QPointF(lb.labDxMm, -lb.labDyMm);  // laboff (px): dx right, dy up
          cl.text = lb.text;
          cl.leader = lb.leader;
          cl.id = lb.sourceLine;
          camLabels.append(cl);
        }
        // All label() markers as clickable dots, so a // @label for this camera can
        // be created by clicking (parity with the drawing dock's green dots).
        for (const auto& rec : resolvedLabels.anchors) {
          CameraLabel cl;
          if (!project(rec.pos, cl.anchor)) continue;
          cl.text = rec.name;
          cl.ref = rec.ref;
          cl.id = rec.sourceLine;
          camMarkers.append(cl);
        }
      }
    }
    images.append(image);
    overlays.append(camLabels);
    markerOverlays.append(camMarkers);
    names.append(info.node ? QString::fromStdString(info.node->camera_name) : QString());
  }
  this->qglview->doneContextCurrent();
  this->cameraDock->setSnapshots(images, names, overlays, markerOverlays);
}

void MainWindow::refreshDrawingViews()
{
  if (!this->drawingDock) return;

  // Resolve dimref anchors + dimensions once (view-independent). Done even when
  // the dock is closed so the 3D markers still update on every preview.
  const auto dimrefs =
    this->rootNode ? collectDimrefs(this->rootNode) : std::vector<DimrefInfo>{};
  const QString source = activeEditor ? activeEditor->toPlainText() : QString();

  // Fáze 15D: resolve the view() blocks of the source and the active view path, so
  // annotations (dims / labels / sheet) can be scoped to the active view. An
  // annotation is visible when its owning view (deepest enclosing view() block, or
  // the root) is a prefix of the active path. With no view() this is all-root and
  // behaves exactly as before.
  const auto viewBlocks = resolveViewBlocks(source);
  const std::vector<std::string> activePath(this->activeViewPath.begin(),
                                            this->activeViewPath.end());
  const QStringList sourceLinesForScope = source.split('\n');
  auto ownerOnActivePath = [&](int line0) {
    return ViewScope::isPrefix(ViewScope::ownerPath(viewBlocks, line0), activePath);
  };

  // Sheet-level settings live in // @sheet comments (gap / scale / shown views /
  // fill). Each view can carry its own; pick the one whose owner is the deepest
  // prefix of the active path (so a view's sheet overrides the common one). Parse
  // it onto the dock's controls BEFORE the layout below reads them, so the source
  // is authoritative; applySheetSettings blocks the controls' signals so this does
  // not write straight back. Missing tokens keep the dock's current value.
  {
    static const char *const slug[6] = {"front", "back", "left", "right", "top", "bottom"};
    double gap = this->drawingDock->sheetGap();
    double scaleDen = this->drawingDock->scaleDenom();
    bool fill = this->drawingDock->fillColors();
    std::vector<bool> shown(6);
    for (int i = 0; i < 6; ++i) shown[i] = this->drawingDock->isViewSelected(i);
    static const QRegularExpression sheetRe(R"(^\s*//\s*@sheet\b(.*)$)");
    int bestOwnerLen = -1;
    QString bestTokens;
    for (int li = 0; li < sourceLinesForScope.size(); ++li) {
      const auto m = sheetRe.match(sourceLinesForScope.at(li));
      if (!m.hasMatch()) continue;
      const auto owner = ViewScope::ownerPath(viewBlocks, li);
      if (!ViewScope::isPrefix(owner, activePath)) continue;  // not this view's sheet
      const int len = static_cast<int>(owner.size());
      if (len > bestOwnerLen) {  // deeper (more specific) wins; ties keep the first
        bestOwnerLen = len;
        bestTokens = m.captured(1);
      }
    }
    if (bestOwnerLen >= 0) {
      for (const QString& tok :
           bestTokens.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts)) {
        const int eq = tok.indexOf('=');
        if (eq < 0) continue;
        const QString k = tok.left(eq), v = tok.mid(eq + 1);
        if (k == "gap") gap = v.toDouble();
        else if (k == "scale") { const double d = v.toDouble(); if (d > 1e-6) scaleDen = d; }
        else if (k == "fill") fill = (v == "on" || v == "1" || v == "true");
        else if (k == "views") {
          for (int i = 0; i < 6; ++i) shown[i] = false;
          for (const QString& s : v.split(',', Qt::SkipEmptyParts))
            for (int i = 0; i < 6; ++i)
              if (s.compare(slug[i], Qt::CaseInsensitive) == 0) shown[i] = true;
        }
      }
    }
    this->drawingDock->applySheetSettings(gap, scaleDen, fill, shown);
  }

  // Section frames, so that h/v/planar dimensions inside a section are measured
  // in the section's own plane rather than in the front view's world axes.
  SectionFrames sectionFrames;
  if (this->rootNode) {
    for (const auto& info : collectSections(this->rootNode)) {
      const QString nm = QString::fromStdString(info.node->section_name);
      if (!nm.isEmpty()) sectionFrames.insert("section" + nm.toLower(), info.world);
    }
  }

  const ResolvedDrawing resolved = resolveDrawing(dimrefs, source, /*warn=*/true, sectionFrames);
  const std::vector<AnchorRec>& anchorRecs = resolved.anchors;
  // Keep only dimensions whose // @dim line is owned by a view on the active path
  // (kvota= dims have sourceLine -1 -> owner root -> always shown).
  std::vector<ResolvedDim> dims;
  dims.reserve(resolved.dims.size());
  for (const auto& d : resolved.dims) {
    if (ownerOnActivePath(d.sourceLine)) dims.push_back(d);
  }

  // Resolve label() markers + // @label callouts (view-independent), shared with
  // the camera overlay.
  const auto labelInfos =
    this->rootNode ? collectLabels(this->rootNode) : std::vector<LabelInfo>{};
  const ResolvedLabels resolvedLabels = resolveLabels(labelInfos, source);
  const std::vector<LabelAnchorRec>& labelAnchorRecs = resolvedLabels.anchors;
  // Same view scoping for // @label callouts.
  std::vector<ResolvedLabel> labels;
  labels.reserve(resolvedLabels.labels.size());
  for (const auto& l : resolvedLabels.labels) {
    if (ownerOnActivePath(l.sourceLine)) labels.push_back(l);
  }

  // Push dimref markers (anchor crosses + dimension lines) to the 3D view. This
  // happens on every preview, like camera frustums, regardless of dock state.
  std::vector<Eigen::Vector3d> markerPoints;
  markerPoints.reserve(anchorRecs.size());
  for (const auto& rec : anchorRecs) markerPoints.push_back(rec.pos);
  std::vector<std::pair<Eigen::Vector3d, Eigen::Vector3d>> markerLines;
  markerLines.reserve(dims.size());
  for (const auto& rd : dims) {
    if (rd.kind == DimKind::Angle) {  // two rays from the vertex, not a chord
      markerLines.emplace_back(rd.c, rd.a);
      markerLines.emplace_back(rd.c, rd.b);
    } else {
      markerLines.emplace_back(rd.a, rd.b);
    }
  }
  this->qglview->setDimrefMarkers(std::move(markerPoints), std::move(markerLines));

  // label() anchors get the same cross in the 3D view, in green (dimref = blue).
  std::vector<Eigen::Vector3d> labelPoints;
  labelPoints.reserve(labelAnchorRecs.size());
  for (const auto& rec : labelAnchorRecs) labelPoints.push_back(rec.pos);
  this->qglview->setLabelMarkers(std::move(labelPoints));
  this->qglview->update();

  // The line drawing needs the concrete mesh (feature edges come from a PolySet),
  // so build it only while the dock is open. GeometryEvaluator hits the geometry
  // cache when the model is unchanged, so repeated refreshes (dimension edits)
  // are cheap; the first evaluation after a geometry change costs an F6.
  if (!this->drawingDock->isVisible()) return;
  if (!this->rootNode) {
    this->drawingDock->clearViews();
    return;
  }
  std::shared_ptr<const Geometry> geom;
  try {
    GeometryEvaluator ge(this->tree);
    geom = ge.evaluateGeometry(*this->rootNode, true);
  } catch (const std::exception& e) {
    LOG(message_group::Warning, "Drawing: geometry evaluation failed: %1$s", e.what());
    geom = nullptr;
  }
  const auto ps = geom ? PolySetUtils::getGeometryAsPolySet(geom) : nullptr;
  if (!ps || ps->isEmpty()) {
    this->drawingDock->clearViews();
    return;
  }

  // Drop the projection cache when the geometry object changed; otherwise every
  // project*() below is a lookup. This is what makes editing a dimension cheap:
  // the comment changed, the mesh did not (see DrawingProjCache in MainWindow.h).
  // The same event bumps the dock's geometry version, so its per-tile pixmap cache
  // survives dimension edits but drops when the mesh really changes.
  if (this->drawingProjCache.geom != geom) {
    this->drawingProjCache.clear();
    this->drawingProjCache.geom = geom;
    ++this->drawingGeomVersion;
  }
  // The colour fills are baked into the tile geometry pixmaps, so toggling fill mode
  // (mesh and scale unchanged) must invalidate them too — otherwise the cached pixmap
  // keeps its old fill state. Treat it like a geometry change and bump the version.
  const bool fillNow = this->drawingDock->fillColors();
  if (fillNow != this->drawingLastFill) {
    ++this->drawingGeomVersion;
    this->drawingLastFill = fillNow;
  }
  this->drawingDock->setGeometryVersion(this->drawingGeomVersion);
  auto cachedEdges = [this, &ps](int vi, const LineDrawingView& v)
    -> const std::vector<LineDrawingSeg2D>& {
    auto it = this->drawingProjCache.edges.find(vi);
    if (it == this->drawingProjCache.edges.end())
      it = this->drawingProjCache.edges.emplace(vi, projectViewEdges(*ps, v, 20.0)).first;
    return it->second;
  };
  auto cachedFaces = [this, &ps](int vi, const LineDrawingView& v)
    -> const std::vector<LineDrawingFace2D>& {
    auto it = this->drawingProjCache.faces.find(vi);
    if (it == this->drawingProjCache.faces.end())
      it = this->drawingProjCache.faces.emplace(vi, projectViewFaces(*ps, v)).first;
    return it->second;
  };
  // Sections are few, so a linear scan over their full parameter set is cheaper
  // (and simpler) than hashing a 4x4 matrix.
  auto cachedSection = [this, &ps](const Transform3d& world, bool flip, double hatchMm,
                                   bool fills) -> const SectionDrawing& {
    for (const auto& e : this->drawingProjCache.sections)
      if (e.flip == flip && e.fills == fills && e.hatchMm == hatchMm &&
          e.world.matrix().isApprox(world.matrix()))
        return e.result;
    DrawingProjCache::SectionEntry e;
    e.world = world;
    e.flip = flip;
    e.hatchMm = hatchMm;
    e.fills = fills;
    e.result = projectSection(*ps, world, flip, 20.0, hatchMm, fills);
    this->drawingProjCache.sections.push_back(std::move(e));
    return this->drawingProjCache.sections.back().result;
  };

  // 4. Project the mesh's feature edges + the dimensions into each ortho view.
  // Coordinates handed to the dock are view-local model-mm, y-down (origin at the
  // view's bounding-box top-left) — the same convention the old raster overlay
  // used, so the dimension/dragging math is unchanged.
  const double scale = this->drawingDock->exportScale();
  const bool fillColors = this->drawingDock->fillColors();
  const std::vector<LineDrawingView>& views = standardViews();
  // Titles parallel to standardViews(): Front, Back, Left, Right, Top, Bottom.
  const char *const titleUtf8[6] = {"Front (nárys)", "Back (zadní)", "Left (levý bok)",
                                    "Right (pravý bok)", "Top (půdorys)", "Bottom (spodní)"};

  // Parse // @detail lines (enlarged view of a circular region of a parent view):
  //   // @detail A parent=front cx=.. cy=.. r=.. scale=..
  // cx/cy/r are in the parent view's view-local model-mm; scale is the detail's own
  // drawing-mm-per-model-mm (independent of the sheet scale). Unknown parent => the
  // detail is skipped (soft, warned) rather than a ghost, like an unresolved @dim.
  struct DetailSpec {
    QString name, parent;
    double cx{0}, cy{0}, r{0}, scale{2}, atx{0}, aty{0};
    bool hasAt{false};
    int line{-1};
  };
  std::vector<DetailSpec> details;
  QHash<QString, QPointF> sectionAt;  // lowercased section name -> at= (sheet-mm)
  {
    const QStringList slines = source.split('\n');
    static const QRegularExpression detRe(R"(^\s*//\s*@detail\s+(.*\S)\s*$)");
    static const QRegularExpression secRe(R"(^\s*//\s*@section\s+(\S+)(.*)$)");
    static const QRegularExpression atRe(R"(\bat=([-0-9.]+),([-0-9.]+))");
    for (int ln = 0; ln < slines.size(); ++ln) {
      const QString& L = slines.at(ln);
      if (const auto sm = secRe.match(L); sm.hasMatch()) {
        const auto am = atRe.match(sm.captured(2));
        if (am.hasMatch())
          sectionAt.insert(sm.captured(1).toLower(),
                           QPointF(am.captured(1).toDouble(), am.captured(2).toDouble()));
        continue;
      }
      const auto m = detRe.match(L);
      if (!m.hasMatch()) continue;
      const QStringList toks =
        m.captured(1).split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
      if (toks.isEmpty()) continue;
      DetailSpec ds;
      ds.name = toks.first();
      ds.line = ln;
      for (const QString& t : toks.mid(1)) {
        const int eq = t.indexOf('=');
        if (eq < 0) continue;
        const QString k = t.left(eq), v = t.mid(eq + 1);
        if (k == "parent") ds.parent = v.toLower();
        else if (k == "cx") ds.cx = v.toDouble();
        else if (k == "cy") ds.cy = v.toDouble();
        else if (k == "r") ds.r = v.toDouble();
        else if (k == "scale") ds.scale = v.toDouble();
        else if (k == "at") {
          const QStringList xy = v.split(',');
          if (xy.size() == 2) { ds.atx = xy[0].toDouble(); ds.aty = xy[1].toDouble(); ds.hasAt = true; }
        }
      }
      if (ds.r > 1e-6 && !ds.parent.isEmpty()) details.push_back(ds);
    }
  }

  // Section markers, resolved once: world plane origin, unit normal (= viewing
  // direction) and label. Reused for the cutting-line annotation in every standard
  // view and for the section tiles below.
  const auto sections = collectSections(this->rootNode);
  struct SectionRef { Eigen::Vector3d p, n; QString label; };
  std::vector<SectionRef> secRefs;
  for (const auto& info : sections) {
    if (!info.node) continue;
    Eigen::Vector3d n = info.world.linear() * Eigen::Vector3d(0, 0, 1);
    if (n.norm() < 1e-9) continue;
    secRefs.push_back({info.world.translation(), n.normalized(),
                       QString::fromStdString(info.node->section_name)});
  }

  // Project the resolved anchors / labels / dimensions into one tile. Shared by
  // the six standard views and (Phase 12) the section tiles, so a // @dim or
  // // @label targeting proj=section<name> behaves exactly as in a standard view.
  // `proj` maps a world point to that tile's view-local mm (y-down); for a section
  // that mapping drops the component along the plane normal, i.e. it projects the
  // anchor perpendicularly onto the cutting plane — the same thing a standard view
  // does when it drops the depth axis.
  // `takeUnscoped` decides what an annotation with no proj= means for this tile:
  // true for the six standard views (documented as "empty proj = all ortho views"),
  // false for section tiles — otherwise every unscoped dimension would also pile
  // onto every section, which is not what "all views" has ever meant.
  auto fillAnnotations = [&](OrthoViewData& vd, const QString& key, bool takeUnscoped,
                             auto&& proj) {
    vd.projKey = key;
    vd.annotated = true;
    for (const auto& rec : anchorRecs)
      vd.anchors.append({proj(rec.pos), rec.label, rec.ref, rec.sourceLine});

    // Label markers as clickable dots (for the "new label" gesture).
    for (const auto& rec : labelAnchorRecs)
      vd.labelAnchors.append({proj(rec.pos), rec.name, rec.ref, rec.sourceLine});

    // Text callouts: draw those targeting this view (empty proj = all ortho
    // views; cam:<name> targets never match an ortho key). laboff is in the
    // view's screen frame (dx = right, dy = up); the overlay is y-down.
    for (const auto& lb : labels) {
      if (lb.proj.isEmpty() ? !takeUnscoped : lb.proj != key) continue;
      OrthoLabel ol;
      ol.anchor = proj(lb.anchor);
      ol.text = lb.text;
      ol.off = QPointF(lb.labDxMm, -lb.labDyMm);
      ol.leader = lb.leader;
      ol.id = lb.sourceLine;
      vd.labels.append(ol);
    }

    for (const auto& rd : dims) {
      if (rd.proj.isEmpty() ? !takeUnscoped : rd.proj != key) continue;
      if (rd.kind == DimKind::Angle) {
        const QPointF pv = proj(rd.c), pa = proj(rd.a), pb = proj(rd.b);
        if (std::hypot((pa - pv).x(), (pa - pv).y()) < 0.1 ||
            std::hypot((pb - pv).x(), (pb - pv).y()) < 0.1)
          continue;
        vd.angles.append({pv, pa, pb, rd.label});
        continue;
      }
      if (rd.kind == DimKind::Diameter || rd.kind == DimKind::Radius) {
        // Radial dim: through the feature centre at the source angle. a/b are
        // the rim points (diameter) or centre + rim point (radius).
        const QPointF pa = proj(rd.a), pb = proj(rd.b);
        QPointF d = pb - pa;
        const double dl = std::hypot(d.x(), d.y());
        if (dl < 1e-6) continue;  // circle edge-on in this view
        OrthoDimension od;
        od.radial = true;
        od.radiusHalf = (rd.kind == DimKind::Radius);
        od.a = pa;
        od.b = pb;
        od.center = od.radiusHalf ? pa : (pa + pb) / 2.0;
        od.dir = d / dl;
        od.label = rd.label;
        od.arrowsOut = rd.arrowsOut;
        od.leader = rd.leader;
        od.labAlongPx = rd.labAlongMm;
        od.labPerpPx = rd.labPerpMm;
        od.id = rd.sourceLine;
        od.jumpLine = rd.sourceLine >= 0 ? rd.sourceLine : rd.jumpLine;
        vd.dims.append(od);
        continue;
      }
      const QPointF pa = proj(rd.a), pb = proj(rd.b);
      // Measurement direction: for h/v the view's screen axis (screen-x is
      // local +x, screen-up is local -y), else along the anchor line.
      QPointF dir = pb - pa;
      if (rd.measure == "h") dir = QPointF(1, 0);
      else if (rd.measure == "v") dir = QPointF(0, -1);
      const double dirLen = std::hypot(dir.x(), dir.y());
      if (dirLen < 1e-9) continue;
      dir /= dirLen;
      // Skip dimensions that collapse to a dot in this view (edge-on).
      const QPointF ab = pb - pa;
      if (std::abs(ab.x() * dir.x() + ab.y() * dir.y()) < 0.05) continue;
      OrthoDimension od;
      od.a = pa;
      od.b = pb;
      od.dir = dir;
      od.label = rd.label;
      od.offsetPx = rd.offsetMm;  // model-mm (dock coordinate space)
      od.arrowsOut = rd.arrowsOut;
      od.leader = rd.leader;
      od.labAlongPx = rd.labAlongMm;
      od.labPerpPx = rd.labPerpMm;
      od.id = rd.sourceLine;  // -1 for kvota= dims (not draggable)
      od.jumpLine = rd.sourceLine >= 0 ? rd.sourceLine : rd.jumpLine;
      vd.dims.append(od);
    }
  };

  QList<OrthoViewData> viewData;
  QStringList titles;
  for (int vi = 0; vi < static_cast<int>(views.size()); ++vi) {
    const LineDrawingView& view = views[vi];
    const QString key = QString(view.name).toLower();  // front|back|left|right|top|bottom
    this->viewPxPerMm[vi] = 1.0;  // dock coords are model-mm now (dim write-back)
    OrthoViewData vd;
    vd.title = QString::fromUtf8(titleUtf8[vi]);
    vd.scale = scale;

    // Only project the views the dock currently shows (mesh stays cached).
    if (!this->drawingDock->isViewSelected(vi)) { viewData.append(vd); titles.append(vd.title); continue; }
    const auto& raw = cachedEdges(vi, view);
    if (!raw.empty()) {
      double minSx = raw[0].x0, maxSx = raw[0].x0, minSy = raw[0].y0, maxSy = raw[0].y0;
      for (const auto& r : raw) {
        minSx = std::min({minSx, r.x0, r.x1}); maxSx = std::max({maxSx, r.x0, r.x1});
        minSy = std::min({minSy, r.y0, r.y1}); maxSy = std::max({maxSy, r.y0, r.y1});
      }
      if (maxSx > minSx && maxSy > minSy) {
        vd.sizeMm = QSizeF(maxSx - minSx, maxSy - minSy);
        // world-plane (sx=world[ax], sy=world[ay], y-up) -> view-local mm, y-down
        auto toLocal = [&](double sx, double sy) { return QPointF(sx - minSx, maxSy - sy); };
        auto proj = [&](const Eigen::Vector3d& P) {
          return toLocal(view.sx * P[view.ax], view.sy * P[view.ay]);
        };

        for (const auto& r : raw)
          vd.edges.append(QLineF(toLocal(r.x0, r.y0), toLocal(r.x1, r.y1)));

        // Coloured face fills (behind the edges), only in fill mode. Painter's
        // order (far first) is preserved from projectViewFaces.
        if (fillColors) {
          for (const auto& f : cachedFaces(vi, view)) {
            OrthoFill of;
            for (const auto& [sx, sy] : f.pts) of.poly << toLocal(sx, sy);
            of.color = QColor::fromRgbF(f.r, f.g, f.b, f.a);
            vd.fills.append(of);
          }
        }

        fillAnnotations(vd, key, /*takeUnscoped=*/true, proj);

        // Cutting-plane lines: section markers seen edge-on in this view (their
        // normal lies in the view plane), drawn across the view with arrows + letter.
        const double W = maxSx - minSx, H = maxSy - minSy;
        auto projDir = [&](const Eigen::Vector3d& d) {
          return QPointF(view.sx * d[view.ax], -view.sy * d[view.ay]);
        };
        Eigen::Vector3d eaz = Eigen::Vector3d::Zero();
        eaz[view.az] = 1.0;
        for (const auto& sr : secRefs) {
          if (std::abs(sr.n.dot(eaz)) > 0.05) continue;  // face-on: no edge line here
          QPointF dir = projDir(eaz.cross(sr.n));
          const double dl = std::hypot(dir.x(), dir.y());
          if (dl < 1e-9) continue;
          dir /= dl;
          const QPointF pt = proj(sr.p);
          double t0 = -1e9, t1 = 1e9;  // clip the line pt + t*dir to [0,W]x[0,H]
          auto slab = [&](double p, double d, double hi) {
            if (std::abs(d) < 1e-9) return p >= 0 && p <= hi;
            double ta = (0 - p) / d, tb = (hi - p) / d;
            if (ta > tb) std::swap(ta, tb);
            t0 = std::max(t0, ta); t1 = std::min(t1, tb);
            return t0 <= t1;
          };
          if (!slab(pt.x(), dir.x(), W) || !slab(pt.y(), dir.y(), H) || t0 > t1) continue;
          // Per ISO 128: the cutting-plane trace (and its arrows + letter) must
          // stand off the outline, not touch it. Extend both clipped ends by
          // 5 drawing-mm (converted to model-mm via the drawing scale).
          const double margin = scale > 1e-9 ? 5.0 / scale : 0.0;
          t0 -= margin; t1 += margin;
          OrthoSectionLine sl;
          sl.p0 = pt + dir * t0;
          sl.p1 = pt + dir * t1;
          sl.arrowDir = projDir(sr.n);
          sl.label = sr.label;
          vd.sectionLines.append(sl);
        }

        // Detail circles whose parent is this view (same view-local mm frame).
        for (const auto& ds : details)
          if (ds.parent == key)
            vd.detailMarks.append({QPointF(ds.cx, ds.cy), ds.r, ds.name});
      }
    }
    viewData.append(vd);
    titles.append(vd.title);
  }
  this->drawingDock->setViews(viewData, titles);

  // 5. Section views from section() markers: cut the mesh, project + hatch, and
  // hand each over as an extra tile "Řez A–A".
  QList<OrthoViewData> sectionData;
  QHash<QString, int> sectionKey;  // "section<name>" (lower) -> index into sectionData
  for (const auto& info : sections) {
    if (!info.node) continue;
    const double hatchMm = info.node->hatch > 0 && scale > 1e-9 ? info.node->hatch / scale : -1.0;
    const SectionDrawing& sd =
      cachedSection(info.world, info.node->flip, hatchMm, fillColors);
    if (!sd.valid) continue;
    double minSx = 1e30, maxSx = -1e30, minSy = 1e30, maxSy = -1e30;
    for (const auto& e : sd.edges) {
      minSx = std::min({minSx, e.x0, e.x1}); maxSx = std::max({maxSx, e.x0, e.x1});
      minSy = std::min({minSy, e.y0, e.y1}); maxSy = std::max({maxSy, e.y0, e.y1});
    }
    if (maxSx <= minSx || maxSy <= minSy) continue;
    auto toLocal = [&](double sx, double sy) { return QPointF(sx - minSx, maxSy - sy); };
    OrthoViewData vd;
    vd.sizeMm = QSizeF(maxSx - minSx, maxSy - minSy);
    vd.scale = scale;
    for (const auto& f : sd.fills) {
      OrthoFill of;
      for (const auto& [sx, sy] : f.pts) of.poly << toLocal(sx, sy);
      of.color = QColor::fromRgbF(f.r, f.g, f.b, f.a);
      vd.fills.append(of);
    }
    for (const auto& e : sd.edges) vd.edges.append(QLineF(toLocal(e.x0, e.y0), toLocal(e.x1, e.y1)));
    for (const auto& hh : sd.hatch) vd.hatch.append(QLineF(toLocal(hh.x0, hh.y0), toLocal(hh.x1, hh.y1)));
    const QString nm = QString::fromStdString(info.node->section_name);
    vd.title = nm.isEmpty() ? tr("Section") : tr("Section %1–%1").arg(nm);
    if (!nm.isEmpty()) {
      // Dimensions / labels targeting proj=section<name>. projectSection views the
      // cut along the frame's +Z with screen x=local X, y=local Y, so a world point
      // maps through world.inverse() and drops the local z — the perpendicular
      // projection onto the cutting plane. Anchors lying in the discarded half
      // still project (they are simply not visible in the cut), matching how a
      // standard view dimensions features hidden behind the surface.
      const Transform3d L = info.world.inverse();
      fillAnnotations(vd, "section" + nm.toLower(), /*takeUnscoped=*/false,
                      [&](const Eigen::Vector3d& P) {
        const Eigen::Vector3d q = L * P;
        return toLocal(q.x(), q.y());
      });
      sectionKey.insert("section" + nm.toLower(), sectionData.size());
      vd.movable = true;  // named sections can be dragged to free space (persist at=)
      vd.placeKey = "section:" + nm;
      const auto it = sectionAt.constFind(nm.toLower());
      if (it != sectionAt.constEnd()) { vd.hasAt = true; vd.atMm = it.value(); }
    }
    sectionData.append(vd);
  }
  this->drawingDock->setSectionViews(sectionData);

  // 6. Detail (enlargement) tiles: clip a parent view's edges/hatch to the
  // // @detail circle and hand them over at the detail's own scale. A missing parent
  // is skipped with a warning (soft), never a ghost tile — like an unresolved @dim.
  auto clipToCircle = [](const QList<QLineF>& segs, const QPointF& C, double r) {
    QList<QLineF> out;
    const double r2 = r * r;
    for (const auto& s : segs) {
      const QPointF a = s.p1(), d = s.p2() - s.p1(), f = s.p1() - C;
      const double A = d.x() * d.x() + d.y() * d.y();
      if (A < 1e-12) {  // degenerate segment (a point)
        const QPointF g = a - C;
        if (g.x() * g.x() + g.y() * g.y() <= r2) out.append(s);
        continue;
      }
      const double B = 2 * (f.x() * d.x() + f.y() * d.y());
      const double Cc = f.x() * f.x() + f.y() * f.y() - r2;
      const double disc = B * B - 4 * A * Cc;
      if (disc < 0) continue;  // line misses the circle entirely
      const double sd = std::sqrt(disc);
      const double lo = std::max(0.0, (-B - sd) / (2 * A));
      const double hi = std::min(1.0, (-B + sd) / (2 * A));
      if (lo >= hi) continue;  // the in-circle span lies outside [0,1]
      out.append(QLineF(a + d * lo, a + d * hi));
    }
    return out;
  };
  auto ratioStr = [](double s) {
    return s >= 1.0 ? QString("%1:1").arg(QString::number(s, 'g', 3))
                    : QString("1:%1").arg(QString::number(1.0 / s, 'g', 3));
  };
  static const char *const detailKeys[6] = {"front", "back", "left", "right", "top", "bottom"};
  QList<OrthoViewData> detailData;
  for (const auto& ds : details) {
    const OrthoViewData *parent = nullptr;
    for (int vi = 0; vi < 6; ++vi)
      if (ds.parent == detailKeys[vi] && vi < viewData.size() && viewData[vi].valid()) {
        parent = &viewData[vi];
        break;
      }
    if (!parent) {  // a section parent ("section<name>")?
      const auto it = sectionKey.constFind(ds.parent);
      if (it != sectionKey.constEnd() && it.value() < sectionData.size())
        parent = &sectionData[it.value()];
    }
    if (!parent) {
      LOG(message_group::Warning, "Drawing: detail '%1$s' references unknown parent '%2$s'.",
          ds.name.toStdString(), ds.parent.toStdString());
      continue;
    }
    const QPointF C(ds.cx, ds.cy);
    const QList<QLineF> ce = clipToCircle(parent->edges, C, ds.r);
    const QList<QLineF> ch = clipToCircle(parent->hatch, C, ds.r);
    if (ce.isEmpty() && ch.isEmpty()) continue;
    const QPointF corner(ds.cx - ds.r, ds.cy - ds.r);
    OrthoViewData dv;
    // Colour fills of the parent, clipped to the same circle. Without this a detail
    // over a filled view came out as bare line work (Phase 10B only filled the views
    // and sections themselves). Clipping a filled polygon to a circle exactly would
    // mean curved edges, so the circle is approximated by a polygon and the two are
    // intersected — the result is filled through the same painter's order as the
    // parent, since parent->fills is already sorted back-to-front.
    if (!parent->fills.isEmpty()) {
      QPainterPath circle;
      circle.addEllipse(C, ds.r, ds.r);
      for (const auto& f : parent->fills) {
        if (!f.poly.boundingRect().intersects(circle.boundingRect())) continue;
        QPainterPath fp;
        fp.addPolygon(f.poly);
        const QPainterPath clipped = fp.intersected(circle);
        if (clipped.isEmpty()) continue;
        for (const QPolygonF& sub : clipped.toSubpathPolygons()) {
          OrthoFill of;
          for (const QPointF& p : sub) of.poly << (p - corner);
          of.color = f.color;
          dv.fills.append(of);
        }
      }
    }
    for (const auto& e : ce) dv.edges.append(QLineF(e.p1() - corner, e.p2() - corner));
    for (const auto& h : ch) dv.hatch.append(QLineF(h.p1() - corner, h.p2() - corner));
    dv.sizeMm = QSizeF(2 * ds.r, 2 * ds.r);
    dv.scale = ds.scale > 1e-9 ? ds.scale : 1.0;
    dv.title = tr("Detail %1 (%2)").arg(ds.name, ratioStr(dv.scale));
    dv.movable = true;  // a detail tile can be dragged to free space (persist at=)
    dv.placeKey = "detail:" + QString::number(ds.line);
    dv.hasAt = ds.hasAt;
    dv.atMm = QPointF(ds.atx, ds.aty);
    detailData.append(dv);
  }
  this->drawingDock->setDetailViews(detailData);
}

/**
  Set up resources related to the 3d View
 */
void MainWindow::setup3DView()
{
  this->qglview->statusLabel = new QLabel(this);
  this->qglview->statusLabel->setMinimumWidth(100);
  statusBar()->addWidget(this->qglview->statusLabel);

  const QSettingsCached settings;
  this->qglview->setMouseCentricZoom(Settings::Settings::mouseCentricZoom.value());
  this->setAllMouseViewActions();
  this->meas.setView(qglview);
  resetMeasurementsState(false, "Render (not preview) to enable measurements");

  // Initial Color Scheme
  const QString cs = GlobalPreferences::inst()->getValue("3dview/colorscheme").toString();
  this->setColorScheme(cs);

  // Initialize View Mode
  // Default to ThrownTogether as OpenCSG support is not known until initializeGL()
  // runs (after show()). The initialized() signal will trigger an update to
  // Preview mode if supported.
  viewModeThrownTogether();

  loadViewSettings();
  loadDesignSettings();

  connect(this->qglview, &QGLView::cameraChanged, animateWidget, &Animate::cameraChanged);
  connect(this->qglview, &QGLView::cameraChanged, viewportControlWidget,
          &ViewportControl::cameraChanged);
  connect(this->qglview, &QGLView::resized, viewportControlWidget, &ViewportControl::viewResized);
  connect(this->qglview, &QGLView::doRightClick, this, &MainWindow::rightClick);
  connect(this->qglview, &QGLView::doLeftClick, this, &MainWindow::leftClick);
  connect(this->qglview, &QGLView::initialized, this, &MainWindow::updateViewModeAfterGLInit);
}

/**
FIXME(kintel): Is this the right place for this?
 */
void MainWindow::setupInput()
{
  InputDriverManager::instance()->registerActions(this->menuBar()->actions(), "", "");
  InputDriverManager::instance()->registerActions(this->animateWidget->actions(), "animation",
                                                  "animate");
}

/**
 Set up glocal Dock widget handling.
 */
void MainWindow::setupDocks()
{
  setCorner(Qt::TopLeftCorner, Qt::LeftDockWidgetArea);
  setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);
  setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
  setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);

  // clang-format off
  docks = {
    {editorDock, _("&Editor")},
    {consoleDock, _("&Console")},
    {parameterDock, _("C&ustomizer")},
    {errorLogDock, _("Error &Log")},
    {animateDock, _("&Animate")},
    {fontListDock, _("&Font List")},
    {colorListDock, _("C&olor List")},
    {viewportControlDock, _("&Viewport Control")},
    {aiDock,_("&AI Chat")},
    {cameraDock, _("&Cameras")},
    {drawingDock, _("&Drawing")}
  };
  // clang-format off

  // Connect the menu "Windows/Navigation" to slot that process it by opening in a pop menu
  // the navigationMenu.
  connect(windowActionJumpTo, &QAction::triggered, this, &MainWindow::onNavigationOpenContextMenu);

  // Create the popup menu to navigate between the docks by keyboard.
  navigationMenu = new QMenu();

  // Create the docks, connect corresponding action and install menu entries
  for (auto& [dock, title] : docks) {
    dock->setName(title);
    dock->setFocusPolicy(Qt::FocusPolicy::StrongFocus);

    // It is neede to have the event filter installed in each dock so that the events are
    // correctly processed when the dock are floating (is in a different window that the mainwindow)
    dock->installEventFilter(this);

    // Get the toggle action from Qt and set an objectName for DBus accessibility
    QAction *toggleAction = dock->toggleViewAction();
    QString baseName = getDockBaseName(title);
    toggleAction->setObjectName("windowActionToggle" + baseName);
    menuWindow->addAction(toggleAction);

    auto dockAction = navigationMenu->addAction(title);
    dockAction->setProperty("id", QVariant::fromValue(dock));
    connect(dockAction, &QAction::triggered, this, &MainWindow::onNavigationTriggerContextMenuEntry);
    connect(dockAction, &QAction::hovered, this, &MainWindow::onNavigationHoveredContextMenuEntry);
  }

  connect(navigationMenu, &QMenu::aboutToHide, this, &MainWindow::onNavigationCloseContextMenu);
  connect(menuWindow, &QMenu::aboutToHide, this, &MainWindow::onNavigationCloseContextMenu);
  windowActionJumpTo->setMenu(navigationMenu);
}

/**
  Connect menus and other actions.
 */
void MainWindow::setupMenusAndActions()
{
  this->exportFormatMapper = new QSignalMapper(this);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
  connect(this->exportFormatMapper, &QSignalMapper::mappedInt, this,
          &MainWindow::actionExportFileFormat);
#else
  connect(this->exportFormatMapper, static_cast<void (QSignalMapper::*)(int)>(&QSignalMapper::mapped),
          this, &MainWindow::actionExportFileFormat);
#endif

  frameCompileResult->hide();
  this->labelCompileResultMessage->setOpenExternalLinks(false);
  connect(this->labelCompileResultMessage, &QLabel::linkActivated, this, &MainWindow::showLink);

  // actions not included in menu
  this->addAction(editActionInsertTemplate);
  this->addAction(editActionFoldAll);

  //
  // File menu
  //


  // Recent files
  updateRecentFileActions();

  show_examples();
#ifndef __APPLE__
  auto shortcuts = this->fileActionReload->shortcuts();
  shortcuts.push_back(QKeySequence(Qt::Key_F3));
  this->fileActionReload->setShortcuts(shortcuts);
#endif


#ifndef __APPLE__
  shortcuts = this->fileActionSave->shortcuts();
  this->fileActionSave->setShortcuts(shortcuts);
#endif


  connect(this->fileActionQuit, &QAction::triggered, scadApp, &OpenSCADApp::closeApp, Qt::QueuedConnection);

#ifdef ENABLE_PYTHON
#else
  this->menuPython->menuAction()->setVisible(false);
#endif

  //
  // Edit menu
  //
  // Edit->Find
#ifdef Q_OS_WIN
  this->editActionFindAndReplace->setShortcut(QKeySequence("Ctrl+Shift+F"));
#endif

  //
  // Design menu
  //
  measurementGroup = new QActionGroup(this);
  measurementGroup->addAction(designActionMeasureDist);
  measurementGroup->addAction(designActionMeasureAngle);
  connect(this->measurementGroup, &QActionGroup::triggered, this, &MainWindow::handleMeasurementClicked);

  exportMap[FileFormat::BINARY_STL] = this->fileActionExportBinarySTL;
  exportMap[FileFormat::ASCII_STL] = this->fileActionExportAsciiSTL;
  exportMap[FileFormat::_3MF] = this->fileActionExport3MF;
  exportMap[FileFormat::OBJ] = this->fileActionExportOBJ;
  exportMap[FileFormat::OFF] = this->fileActionExportOFF;
  exportMap[FileFormat::WRL] = this->fileActionExportWRL;
  exportMap[FileFormat::POV] = this->fileActionExportPOV;
  exportMap[FileFormat::AMF] = this->fileActionExportAMF;
  exportMap[FileFormat::DXF] = this->fileActionExportDXF;
  exportMap[FileFormat::SVG] = this->fileActionExportSVG;
  exportMap[FileFormat::PDF] = this->fileActionExportPDF;
  exportMap[FileFormat::CSG] = this->fileActionExportCSG;
  exportMap[FileFormat::PNG] = this->fileActionExportImage;

  for (auto& [format, action] : exportMap) {
    connect(action, &QAction::triggered, this->exportFormatMapper, QOverload<>::of(&QSignalMapper::map));
    this->exportFormatMapper->setMapping(action, int(format));
  }

#ifndef ENABLE_LIB3MF
  this->fileActionExport3MF->setVisible(false);
#endif

  //
  // View menu
  //
  previewModeGroup = new QActionGroup(this);
  previewModeGroup->setExclusive(true);
  previewModeGroup->addAction(this->viewActionPreview);
  previewModeGroup->addAction(this->viewActionThrownTogether);
  if (this->qglview->hasOpenCSGSupport()) {
    this->viewActionPreview->setChecked(true);
  } else {
    this->viewActionThrownTogether->setChecked(true);
  }

  viewActionProjectionGroup = new QActionGroup(this);
  viewActionProjectionGroup->setExclusive(true);
  viewActionProjectionGroup->addAction(this->viewActionPerspective);
  viewActionProjectionGroup->addAction(this->viewActionOrthogonal);


  connect(this->viewActionZoomIn, &QAction::triggered, qglview, &QGLView::ZoomIn);
  connect(this->viewActionZoomOut, &QAction::triggered, qglview, &QGLView::ZoomOut);

  //
  // Help menu
  //

  // Checks if the Documentation has been downloaded and hides the Action otherwise
  if (!UIUtils::hasOfflineUserManual()) {
    this->helpActionOfflineManual->setVisible(false);
  }
  if (!UIUtils::hasOfflineCheatSheet()) {
    this->helpActionOfflineCheatSheet->setVisible(false);
  }
#ifdef OPENSCAD_UPDATER
  this->menuBar()->addMenu(AutoUpdater::updater()->updateMenu);
#endif

  connect(this->findTypeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &MainWindow::actionSelectFind);
  connect(this->findInputField, &QWordSearchField::textChanged, this, &MainWindow::findString);
  connect(this->findInputField, &QWordSearchField::returnPressed, this->findNextButton,
          [this] { this->findNextButton->animateClick(); });
  find_panel->installEventFilter(this);
  if (QApplication::clipboard()->supportsFindBuffer()) {
    connect(this->findInputField, &QWordSearchField::textChanged, this, &MainWindow::updateFindBuffer);
    connect(QApplication::clipboard(), &QClipboard::findBufferChanged, this,
            &MainWindow::findBufferChanged);
    // With Qt 4.8.6, there seems to be a bug that often gives an incorrect findbuffer content when
    // the app receives focus for the first time
    this->findInputField->setText(QApplication::clipboard()->text(QClipboard::FindBuffer));
  }

  this->findPrevButton->setDefaultAction(editActionFindPrevious);
  this->findNextButton->setDefaultAction(editActionFindNext);
  connect(this->findDoneButton, &QPushButton::clicked, this, &MainWindow::hideFind);
  connect(this->replaceButton, &QPushButton::clicked, this, &MainWindow::replace);
  connect(this->replaceAllButton, &QPushButton::clicked, this, &MainWindow::replaceAll);
  connect(this->replaceInputField, &QLineEdit::returnPressed, this->replaceButton,
          [this] { this->replaceButton->animateClick(); });
  addKeyboardShortCut(this->viewerToolBar->actions());
  addKeyboardShortCut(this->editortoolbar->actions());

  // connect the signal of next/prev windowAction and the dedicated slot
  // hovering is connected to rubberband activation while triggering is for actual
  // activation of the corresponding dock.
  const std::vector<QAction *> actions = {windowActionNextWindow, windowActionPreviousWindow};
  for (auto& action : actions) {
    connect(action, &QAction::hovered, this, &MainWindow::onWindowActionNextPrevHovered);
    connect(action, &QAction::triggered, this, &MainWindow::onWindowActionNextPrevTriggered);
  }

  // Adds shortcut for the prev/next window switching
  shortcutNextWindow = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_K), this);
  QObject::connect(shortcutNextWindow, &QShortcut::activated, this,
                   &MainWindow::onWindowShortcutNextPrevActivated);
  shortcutPreviousWindow = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_H), this);
  QObject::connect(shortcutPreviousWindow, &QShortcut::activated, this,
                   &MainWindow::onWindowShortcutNextPrevActivated);

  auto shortcutExport3D = new QShortcut(QKeySequence("F7"), this);
  QObject::connect(shortcutExport3D, &QShortcut::activated, this,
                   &MainWindow::onWindowShortcutExport3DActivated);

  updateExportActions();
}

/**
  Restore GUI state from settings.
 */
void MainWindow::restoreWindowState()
{
  const QSettingsCached settings;
  const auto windowState = settings.value("window/state", QByteArray()).toByteArray();
  clearCurrentOutput();
  UIUtils::dumpSaveState(windowState);
  setCurrentOutput();
  restoreGeometry(settings.value("window/geometry", QByteArray()).toByteArray());
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
  // Workaround for a Qt bug (possible QTBUG-46620, but it's still there in Qt-6.5.3)
  // Blindly restoring a maximized window to a different screen resolution causes a crash
  // on the next move/resize operation on macOS:
  // https://github.com/openscad/openscad/issues/5486
  if (isMaximized()) {
    setGeometry(screen()->availableGeometry());
  }
#endif
  restoreState(windowState);

  if (windowState.size() == 0) {
    /*
     * This triggers only in case the configuration file has no
     * window state information (or no configuration file at all).
     * When this happens, the editor would default to a very ugly
     * width due to the dock widget layout. This overwrites the
     * value reported via sizeHint() to a width a bit smaller than
     * half the main window size (either the one loaded from the
     * configuration or the default value of 800).
     * The height is only a dummy value which will be essentially
     * ignored by the layouting as the editor is set to expand to
     * fill the available space.
     */
    activeEditor->setInitialSizeHint(QSize((5 * this->width() / 11), 100));
    tabifyDockWidget(consoleDock, errorLogDock);
    tabifyDockWidget(errorLogDock, fontListDock);
    tabifyDockWidget(fontListDock, colorListDock);
    tabifyDockWidget(colorListDock, animateDock);
    tabifyDockWidget(animateDock, viewportControlDock);
    tabifyDockWidget(parameterDock, aiDock);
    parameterDock->hide();
    aiDock->hide();
    errorLogDock->hide();
    fontListDock->hide();
    colorListDock->hide();
    animateDock->hide();
    viewportControlDock->hide();
    consoleDock->show();
    consoleDock->raise();
  } else {
#ifdef Q_OS_WIN
    // Try moving the main window into the display range, this
    // can occur when closing OpenSCAD on a second monitor which
    // is not available at the time the application is started
    // again.
    // On Windows that causes the main window to open in a not
    // easily reachable place.
    auto primaryScreen = QApplication::primaryScreen();
    auto desktopRect = primaryScreen->availableGeometry().adjusted(250, 150, -250, -150).normalized();
    auto windowRect = frameGeometry();
    if (!desktopRect.intersects(windowRect)) {
      windowRect.moveCenter(desktopRect.center());
      windowRect = windowRect.intersected(desktopRect);
      move(windowRect.topLeft());
      resize(windowRect.size());
    }
#endif  // ifdef Q_OS_WIN
  }

}

void MainWindow::openRemainingFiles(const QStringList& filenames)
{
  for (int i = 1; i < filenames.size(); ++i) tabManager->createTab(filenames[i]);

  activeEditor->setFocus();
}

void MainWindow::changeEvent(QEvent *event)
{
  if (event->type() == QEvent::ThemeChange) {
    setGlobalTheme();
  }
  QMainWindow::changeEvent(event);
}
