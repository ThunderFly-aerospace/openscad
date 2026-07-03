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
class QCheckBox;
class QResizeEvent;
class QMouseEvent;
class QPaintEvent;

// A dimref anchor projected into one ortho view, in render-image pixel coords.
struct OrthoAnchor {
  QPointF pos;
  QString name;
};

// A dimension (kvóta) to draw in one ortho view: two anchor endpoints
// (render-image pixel coords), a text label, a perpendicular offset in image
// pixels (the dimension line is pulled this far off the anchor line), and an id
// used to write the offset back to source (source line index; -1 = not editable,
// e.g. kvota= sugar dimensions).
struct OrthoDimension {
  QPointF a;
  QPointF b;
  QString label;
  double offsetPx{0.0};
  bool arrowsOut{false};  // arrowheads outside the extension lines
  // Label position offset from the dimension-line midpoint, in image px, in the
  // dimension's own frame (along the line, and perpendicular to it).
  double labAlongPx{0.0};
  double labPerpPx{0.0};
  int id{-1};
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
  void setShowAnchors(bool show);

signals:
  void anchorClicked(const QString& name);
  // Emitted while dragging a dimension line and once more on release, with the
  // dimension id and its new perpendicular offset in image pixels.
  void dimensionOffsetChanged(int id, double offsetPx, bool finished);
  // Emitted while dragging a dimension's label, with the new label offset in the
  // dimension's (along, perp) frame in image pixels.
  void labelOffsetChanged(int id, double alongPx, double perpPx, bool finished);

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;

private:
  // Maps a render-image pixel point to a widget point using the current fit.
  QPointF mapToWidget(const QPointF& imgPt) const;
  QPointF mapToImage(const QPointF& widgetPt) const;
  // Unit perpendicular (image space) of dimension i, pointing along +offset.
  QPointF dimPerp(const OrthoDimension& d) const;
  // Label anchor point (image space): midpoint of the offset dimension line plus
  // the label's own along/perp offset.
  QPointF dimLabelPos(const OrthoDimension& d) const;

  OrthoViewData data;
  // Fit transform (recomputed each paint): widget = image * scale + offset.
  double fitScale{1.0};
  QPointF fitOffset;
  // Index into data.dims currently being dragged (line), or -1.
  int draggingDim{-1};
  // Index into data.dims whose label is being dragged, or -1.
  int draggingLabel{-1};
  // Whether to draw reference-point dots and their names.
  bool showAnchors{true};
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
  // Emitted when a dimension's offset is dragged (view index, dim id, offset px,
  // finished=true on mouse release).
  void dimensionOffsetChanged(int viewIndex, int id, double offsetPx, bool finished);
  // Emitted when a dimension's label is dragged (view index, dim id, along/perp
  // px in the dimension's frame, finished=true on release).
  void labelOffsetChanged(int viewIndex, int id, double alongPx, double perpPx, bool finished);
  // Emitted when the user clicks "Materialize kvóty".
  void materializeRequested();

private slots:
  void saveSheet();
  void setShowAnchors(bool show);

private:
  QVector<OrthoViewWidget *> viewWidgets;
  QLabel *placeholder;
  QPushButton *saveButton;
  QPushButton *materializeButton;
  QCheckBox *showRefsCheck;
  QList<OrthoViewData> views;
};
