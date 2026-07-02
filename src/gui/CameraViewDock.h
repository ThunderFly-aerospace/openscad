#pragma once

#include <QImage>
#include <QList>
#include <QStringList>

#include "gui/Dock.h"

class QListWidget;
class QLabel;
class QPushButton;
class QResizeEvent;

/*!
   Dock that shows offscreen snapshots rendered from the camera() markers in the
   model. It is a passive viewer: MainWindow renders the images (after each
   preview) and hands them here via setSnapshots(). A list selects the camera, a
   label shows the snapshot, and a button saves the current one as PNG.
 */
class CameraViewDock : public Dock
{
  Q_OBJECT

public:
  CameraViewDock(QWidget *parent = nullptr);
  ~CameraViewDock() override;

  void setSnapshots(const QList<QImage>& images, const QStringList& names);
  void clearSnapshots();

protected:
  void resizeEvent(QResizeEvent *event) override;

private slots:
  void updatePreview();
  void saveCurrent();

private:
  void showCurrentImage();

  QListWidget *cameraList;
  QLabel *preview;
  QPushButton *saveButton;
  QList<QImage> images;
  QStringList names;
};
