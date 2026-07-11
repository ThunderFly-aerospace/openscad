#pragma once

#include <QImage>
#include <QList>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QWidget>

#include "gui/Dock.h"

class QListWidget;
class QPushButton;
class QResizeEvent;
class QMouseEvent;
class QPaintEvent;

// A // @label text callout projected onto a camera snapshot, in image-pixel
// coordinates (y-down). `anchor` is the projected 3D point; the text sits at
// anchor + off; a leader line joins them. `id` is the 0-based // @label source
// line (for laboff= write-back / right-click jump).
//
// The same struct also carries a projected label() *marker* (a clickable dot for
// creating a // @label on this camera): then `text` is the marker name and `ref`
// its reference token (name / name#index); off/leader/id are unused.
struct CameraLabel {
  QPointF anchor;
  QPointF off;
  QString text;
  QString ref;
  bool leader{true};
  int id{-1};
};

// The snapshot image plus its label overlay, custom-painted so the callouts can
// be dragged. Letterboxes the image (KeepAspectRatio) and maps image <-> widget
// pixels for hit-testing and drag write-back.
class CameraImageView : public QWidget
{
  Q_OBJECT

public:
  CameraImageView(QWidget *parent = nullptr);

  void setContent(const QImage& image, const QList<CameraLabel>& labels,
                  const QList<CameraLabel>& markers = {});
  void setPlaceholder(const QString& text);

signals:
  // Emitted while dragging a label text (offset from the anchor in image px,
  // y-down) and once more on release.
  void labelTextMoved(int id, double dxPx, double dyPx, bool finished);
  // Emitted on double-click of a label text (its source line).
  void labelDoubleClicked(int id);
  // Emitted on right-click of a label text (its source line).
  void jumpToSource(int line);
  // Emitted when a label() marker dot is clicked (to create a // @label): the
  // marker's reference token.
  void labelAnchorClicked(const QString& ref);

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
  // Fit transform (recomputed each paint): widget = image*scale + offset.
  void recomputeFit();
  QPointF imageToWidget(const QPointF& imgPt) const;
  QPointF widgetToImage(const QPointF& widgetPt) const;
  int labelAt(const QPointF& widgetPt) const;   // index of the label box under a point
  int markerAt(const QPointF& widgetPt) const;  // index of the marker dot under a point

  QImage image;
  QList<CameraLabel> labels;
  QList<CameraLabel> markers;
  QString placeholder;
  double fitScale{1.0};
  QPointF fitOffset;
  int draggingLabel{-1};
};

/*!
   Dock that shows offscreen snapshots rendered from the camera() markers in the
   model, with // @label text callouts projected onto each snapshot. MainWindow
   renders the images and projects the callouts (after each preview) and hands
   them here via setSnapshots(). A list selects the camera, the custom image view
   shows the snapshot + draggable callouts, and a button saves the current one as
   PNG.
 */
class CameraViewDock : public Dock
{
  Q_OBJECT

public:
  CameraViewDock(QWidget *parent = nullptr);
  ~CameraViewDock() override;

  void setSnapshots(const QList<QImage>& images, const QStringList& names,
                    const QList<QList<CameraLabel>>& labels = {},
                    const QList<QList<CameraLabel>>& markers = {});
  void clearSnapshots();

signals:
  void labelTextMoved(int id, double dxPx, double dyPx, bool finished);
  void labelDoubleClicked(int id);
  void jumpToSource(int line);
  // Emitted when a label() marker dot is clicked on a snapshot: the camera (row
  // index + name, which is empty for an unnamed camera()) and the marker
  // reference, so the owner writes // @label ref proj=cam:name.
  void labelAnchorClicked(int cameraIndex, const QString& cameraName, const QString& ref);

private slots:
  void updatePreview();
  void saveCurrent();

private:
  void showCurrentImage();

  QListWidget *cameraList;
  CameraImageView *preview;
  QPushButton *saveButton;
  QList<QImage> images;
  QStringList names;
  QList<QList<CameraLabel>> overlays;
  QList<QList<CameraLabel>> markerOverlays;
};
