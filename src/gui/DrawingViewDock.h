#pragma once

#include <QImage>
#include <QList>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

#include "gui/Dock.h"

class QLabel;
class QPushButton;
class QResizeEvent;
class QMouseEvent;
class QPaintEvent;

// A dimref anchor projected into one ortho view, in render-image pixel coords.
struct OrthoAnchor {
  QPointF pos;
  QString name;
};

// A dimension (kvóta) to draw in one ortho view: two endpoints (render-image
// pixel coords) and a text label (computed length or manual override).
struct OrthoDimension {
  QPointF a;
  QPointF b;
  QString label;
};

// Everything needed to render one ortho view with its overlay.
struct OrthoViewData {
  QImage image;
  QList<OrthoAnchor> anchors;
  QList<OrthoDimension> dims;
};

/*!
   Widget that shows one orthographic view (the rendered image) with a dimension
   overlay drawn on top: dimref anchor dots and dimension lines with labels. The
   image is scaled to fit while keeping aspect; overlay coordinates are given in
   the render image's native pixel space and mapped through the same fit
   transform. Clicking near an anchor emits anchorClicked() (used in Phase 2B-3).
 */
class OrthoViewWidget : public QWidget
{
  Q_OBJECT

public:
  OrthoViewWidget(QWidget *parent = nullptr);

  void setData(const OrthoViewData& data);
  void clear();

signals:
  void anchorClicked(const QString& name);

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;

private:
  // Maps a render-image pixel point to a widget point using the current fit.
  QPointF mapToWidget(const QPointF& imgPt) const;

  OrthoViewData data;
  // Fit transform (recomputed each paint): widget = image * scale + offset.
  double fitScale{1.0};
  QPointF fitOffset;
};

/*!
   Dock that shows orthographic views of the model (front / side / top =
   nárys / bokorys / půdorys) laid out like a technical drawing sheet, each with
   a dimension overlay. Passive: MainWindow renders the images, projects the
   dimref anchors / dimensions, and hands the result here via setViews().
 */
class DrawingViewDock : public Dock
{
  Q_OBJECT

public:
  DrawingViewDock(QWidget *parent = nullptr);
  ~DrawingViewDock() override;

  void setViews(const QList<OrthoViewData>& views, const QStringList& titles);
  void clearViews();

signals:
  // Emitted when an anchor is clicked in one of the views (view index + name).
  void anchorClicked(int viewIndex, const QString& name);

private slots:
  void saveSheet();

private:
  QVector<OrthoViewWidget *> viewWidgets;
  QLabel *placeholder;
  QPushButton *saveButton;
  QList<OrthoViewData> views;
};
