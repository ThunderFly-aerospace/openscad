#include "gui/DrawingViewDock.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <utility>
#include <vector>

#include <QFileDialog>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QMarginsF>
#include <QPageSize>
#include <QPdfWriter>
#include <QSvgGenerator>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPolygonF>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

// ---------------------------------------------------------------------------
// OrthoViewWidget
// ---------------------------------------------------------------------------

namespace {
// Closest point on segment [a,b] to p.
QPointF closestOnSegment(const QPointF& p, const QPointF& a, const QPointF& b)
{
  const QPointF ab = b - a;
  const double len2 = ab.x() * ab.x() + ab.y() * ab.y();
  double t = 0.0;
  if (len2 > 1e-9) {
    t = ((p.x() - a.x()) * ab.x() + (p.y() - a.y()) * ab.y()) / len2;
    t = std::clamp(t, 0.0, 1.0);
  }
  return QPointF(a.x() + t * ab.x(), a.y() + t * ab.y());
}

// Distance from point p to segment [a,b].
double distToSegment(const QPointF& p, const QPointF& a, const QPointF& b)
{
  const QPointF proj = closestOnSegment(p, a, b);
  return std::hypot(p.x() - proj.x(), p.y() - proj.y());
}
}  // namespace

OrthoViewWidget::OrthoViewWidget(QWidget *parent) : QWidget(parent)
{
  setMinimumSize(120, 90);
  setMouseTracking(false);
}

void OrthoViewWidget::setData(const OrthoViewData& newData)
{
  this->data = newData;
  this->draggingDim = -1;
  this->draggingLabel = -1;
  update();
}

void OrthoViewWidget::clear()
{
  this->data = OrthoViewData{};
  this->draggingDim = -1;
  this->draggingLabel = -1;
  update();
}

void OrthoViewWidget::setShowAnchors(bool show)
{
  this->showAnchors = show;
  update();
}

void OrthoViewWidget::setShowLeaders(bool show)
{
  this->showLeaders = show;
  update();
}

void OrthoViewWidget::setScale(double scale)
{
  this->data.scale = scale;
  update();
}

double OrthoViewWidget::mmToPx() const
{
  return this->fitScale * this->userZoom;
}

double OrthoViewWidget::absPx(double drawingMm) const
{
  const double sc = this->data.scale > 1e-9 ? this->data.scale : 1.0;
  return drawingMm / sc * mmToPx();
}

QFont OrthoViewWidget::overlayFont() const
{
  // Dimension text ~3 mm on the drawing (a common technical-drawing text height).
  QFont f = font();
  const double px = absPx(3.0);
  f.setPixelSize(std::max(1, static_cast<int>(std::round(px))));
  return f;
}

QPointF OrthoViewWidget::mapToWidget(const QPointF& imgPt) const
{
  const double s = this->fitScale * this->userZoom;
  return QPointF(imgPt.x() * s + this->fitOffset.x() + this->userPan.x(),
                 imgPt.y() * s + this->fitOffset.y() + this->userPan.y());
}

QPointF OrthoViewWidget::mapToImage(const QPointF& widgetPt) const
{
  const double s = this->fitScale * this->userZoom;
  if (s < 1e-9) return QPointF();
  return QPointF((widgetPt.x() - this->fitOffset.x() - this->userPan.x()) / s,
                 (widgetPt.y() - this->fitOffset.y() - this->userPan.y()) / s);
}

QPointF OrthoViewWidget::dimPerp(const OrthoDimension& d) const
{
  QPointF dir = d.dir;
  const double len = std::hypot(dir.x(), dir.y());
  if (len < 1e-9) return QPointF(0, -1);
  dir /= len;
  return QPointF(dir.y(), -dir.x());  // rotate -90°
}

// Foot of an image-space point projected onto the (offset) dimension line: the
// line through midpoint(a,b)+perp*offset running along the measurement
// direction dir. Used for both drawing and hit-testing so they stay in sync.
static QPointF dimFoot(const OrthoDimension& d, const QPointF& perp, const QPointF& t)
{
  QPointF dir = d.dir;
  const double len = std::hypot(dir.x(), dir.y());
  if (len > 1e-9) dir /= len;
  else dir = QPointF(1, 0);
  const QPointF base = (d.a + d.b) / 2.0 + perp * d.offsetPx;
  const QPointF rel = t - base;
  const double s = rel.x() * dir.x() + rel.y() * dir.y();
  return base + dir * s;
}

// Free versions of the dimension geometry so both the widget paint and the
// vector export compute identical positions (WYSIWYG).
static QPointF perpOf(const OrthoDimension& d)
{
  QPointF dir = d.dir;
  const double len = std::hypot(dir.x(), dir.y());
  if (len < 1e-9) return QPointF(0, -1);
  dir /= len;
  return QPointF(dir.y(), -dir.x());
}

static QPointF unitDirOf(const OrthoDimension& d)
{
  QPointF dir = d.dir;
  const double len = std::hypot(dir.x(), dir.y());
  return len > 1e-9 ? dir / len : QPointF(1, 0);
}

// The drawn dimension line's endpoints in image space. A linear dimension runs
// between the feet of its anchors on the offset line; a radial one runs straight
// through the feature centre (the whole chord for a diameter, centre->rim for a
// radius). Paint, export and hit-testing all go through this so they stay WYSIWYG.
static void dimLineEnds(const OrthoDimension& d, QPointF& p0, QPointF& p1)
{
  if (d.radial) {
    p0 = d.radiusHalf ? d.center : d.a;
    p1 = d.b;
  } else {
    const QPointF perp = perpOf(d);
    p0 = dimFoot(d, perp, d.a);
    p1 = dimFoot(d, perp, d.b);
  }
}

static QPointF labelPosOf(const OrthoDimension& d)
{
  QPointF p0, p1;
  dimLineEnds(d, p0, p1);
  const QPointF perp = perpOf(d);
  const QPointF dir = unitDirOf(d);
  const QPointF mid = (p0 + p1) / 2.0;
  return mid + dir * d.labAlongPx + perp * d.labPerpPx;
}

QPointF OrthoViewWidget::dimLabelPos(const OrthoDimension& d) const
{
  return labelPosOf(d);
}

void OrthoViewWidget::paintEvent(QPaintEvent *)
{
  QPainter painter(this);
  painter.fillRect(rect(), Qt::white);

  if (!this->data.valid()) {
    painter.setPen(Qt::gray);
    painter.drawText(rect(), Qt::AlignCenter, tr("(no preview)"));
    return;
  }

  // Fit the view's model-mm bounding box into the widget keeping aspect, centered;
  // the user's wheel-zoom and drag-pan are applied on top (see mapToWidget).
  const QSizeF sz = this->data.sizeMm;
  this->fitScale = std::min(static_cast<double>(width()) / sz.width(),
                            static_cast<double>(height()) / sz.height());
  const double drawW = sz.width() * this->fitScale;
  const double drawH = sz.height() * this->fitScale;
  this->fitOffset = QPointF((width() - drawW) / 2.0, (height() - drawH) / 2.0);

  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setFont(overlayFont());

  // The line drawing itself: silhouette + feature edges (model-mm -> widget).
  QPen edgePen(Qt::black);
  edgePen.setWidthF(std::max(0.6, absPx(0.3)));
  edgePen.setCapStyle(Qt::RoundCap);
  painter.setPen(edgePen);
  for (const auto& e : this->data.edges) {
    painter.drawLine(mapToWidget(e.p1()), mapToWidget(e.p2()));
  }

  // Dimensions: witness lines from the anchors out to an offset dimension line
  // with arrowheads, and a length label riding on it.
  QPen dimPen(QColor(20, 20, 20));
  dimPen.setWidthF(std::max(0.6, absPx(0.25)));
  QPen witnessPen(QColor(120, 120, 120));
  witnessPen.setWidthF(std::max(0.4, absPx(0.15)));
  const double arrowS = std::max(4.0, absPx(2.5));  // arrowhead length (~2.5 mm)
  // Arrowhead with apex at `tip`, body extending toward `along` (unit), width from nrm.
  auto drawArrow = [&](const QPointF& tip, const QPointF& along, const QPointF& nrm) {
    QPolygonF head;
    head << tip << (tip + along * arrowS + nrm * (arrowS * 0.4))
         << (tip + along * arrowS - nrm * (arrowS * 0.4));
    painter.setBrush(QColor(20, 20, 20));
    painter.setPen(Qt::NoPen);
    painter.drawPolygon(head);
  };
  // Draw a label box (with a white backing) centred at `center`, kept on-screen,
  // and — when leaders are on and the label sits away from its dimension line — a
  // thin leader from the box to the nearest point of the segment [seg0,seg1].
  auto drawLabel = [&](const QPointF& center, const QString& text, const QPointF& seg0,
                       const QPointF& seg1) {
    const QRectF tr = painter.fontMetrics().boundingRect(text).adjusted(-3, -2, 3, 2);
    QRectF box(center.x() - tr.width() / 2.0, center.y() - tr.height() / 2.0,
               tr.width(), tr.height());
    const double bx = std::clamp(box.x(), 1.0, width() - box.width() - 1.0);
    const double by = std::clamp(box.y(), 1.0, height() - box.height() - 1.0);
    box.moveTo(bx, by);
    if (this->showLeaders) {
      const QPointF foot = closestOnSegment(box.center(), seg0, seg1);
      if (std::hypot(box.center().x() - foot.x(), box.center().y() - foot.y()) >
          box.height()) {
        painter.setPen(witnessPen);
        painter.drawLine(box.center(), foot);
      }
    }
    painter.fillRect(box, QColor(255, 255, 255, 220));
    painter.setPen(QColor(150, 30, 30));
    painter.drawText(box, Qt::AlignCenter, text);
  };

  // Radial (⌀ / R) dimensions: line straight through the centre, no witness lines.
  for (const auto& dim : this->data.dims) {
    if (!dim.radial) continue;
    QPointF p0, p1;
    dimLineEnds(dim, p0, p1);
    const QPointF a2 = mapToWidget(p0), b2 = mapToWidget(p1);
    QPointF dir = b2 - a2;
    const double len = std::hypot(dir.x(), dir.y());
    if (len < 1e-6) continue;
    dir /= len;
    const QPointF nrm(-dir.y(), dir.x());
    painter.setPen(dimPen);
    painter.setBrush(Qt::NoBrush);
    if (dim.arrowsOut) {
      // Arrows outside the rim pointing in (small features): stubs + inward heads.
      painter.drawLine(a2, b2);
      painter.drawLine(b2, b2 + dir * (arrowS * 1.5));
      drawArrow(b2, dir, nrm);
      if (!dim.radiusHalf) {
        painter.drawLine(a2, a2 - dir * (arrowS * 1.5));
        drawArrow(a2, -dir, nrm);
      }
    } else {
      painter.drawLine(a2, b2);
      drawArrow(b2, -dir, nrm);                    // head at the rim, pointing out
      if (!dim.radiusHalf) drawArrow(a2, dir, nrm);  // diameter: head at both rims
    }
    drawLabel(mapToWidget(dimLabelPos(dim)), dim.label, a2, b2);
  }

  for (const auto& dim : this->data.dims) {
    if (dim.radial) continue;
    const QPointF a = mapToWidget(dim.a);
    const QPointF b = mapToWidget(dim.b);
    const QPointF perp = dimPerp(dim);
    // Endpoints of the dimension line: feet of the anchors on the offset line
    // along the measurement direction (image space, then mapped to widget).
    const QPointF a2 = mapToWidget(dimFoot(dim, perp, dim.a));
    const QPointF b2 = mapToWidget(dimFoot(dim, perp, dim.b));

    // Witness (extension) lines from each anchor to its foot on the dimension
    // line (skip the degenerate case where the foot coincides with the anchor).
    painter.setPen(witnessPen);
    if (std::hypot((a2 - a).x(), (a2 - a).y()) > 0.5) painter.drawLine(a, a2);
    if (std::hypot((b2 - b).x(), (b2 - b).y()) > 0.5) painter.drawLine(b, b2);

    // Dimension line with arrowheads.
    painter.setPen(dimPen);
    painter.drawLine(a2, b2);
    QPointF dir = b2 - a2;
    const double len = std::hypot(dir.x(), dir.y());
    if (len > 1e-6) {
      dir /= len;
      const QPointF nrm(-dir.y(), dir.x());
      const double s = std::max(4.0, absPx(2.5));  // arrowhead length (~2.5 mm)
      auto arrow = [&](const QPointF& tip, const QPointF& along) {
        QPolygonF head;
        head << tip << (tip + along * s + nrm * (s * 0.4))
             << (tip + along * s - nrm * (s * 0.4));
        painter.setBrush(QColor(20, 20, 20));
        painter.setPen(Qt::NoPen);
        painter.drawPolygon(head);
        painter.setPen(dimPen);
      };
      if (dim.arrowsOut) {
        // Arrowheads outside the endpoints pointing inward (for tight dims);
        // extend the dimension line a little past each end so they have a stem.
        painter.drawLine(a2, a2 - dir * (s * 1.5));
        painter.drawLine(b2, b2 + dir * (s * 1.5));
        arrow(a2, -dir);
        arrow(b2, dir);
      } else {
        arrow(a2, dir);
        arrow(b2, -dir);
      }
    }

    // Label at its own position (dimension midpoint plus label offset), with a
    // leader to the dimension line when it has been dragged away.
    drawLabel(mapToWidget(dimLabelPos(dim)), dim.label, a2, b2);
  }

  // Angle dimensions: two rays from the vertex, an arc between them, and a label.
  for (const auto& ang : this->data.angles) {
    const QPointF v = mapToWidget(ang.vertex);
    const QPointF a = mapToWidget(ang.a);
    const QPointF b = mapToWidget(ang.b);
    QPointF ua = a - v, ub = b - v;
    const double la = std::hypot(ua.x(), ua.y());
    const double lb = std::hypot(ub.x(), ub.y());
    if (la < 1e-6 || lb < 1e-6) continue;
    ua /= la;
    ub /= lb;
    // Arc radius: a fixed fraction of the shorter ray, clamped to a sane range.
    const double arcR = std::clamp(std::min(la, lb) * 0.5, 14.0, 42.0);

    painter.setPen(witnessPen);
    painter.drawLine(v, v + ua * std::max(la, arcR + 6));
    painter.drawLine(v, v + ub * std::max(lb, arcR + 6));

    // Arc from ray a to ray b (Qt angles: degrees CCW from +x, y up; widget y is
    // down, so negate). Sweep the short way between the two rays.
    double angA = std::atan2(-ua.y(), ua.x()) * 180.0 / M_PI;
    double angB = std::atan2(-ub.y(), ub.x()) * 180.0 / M_PI;
    double span = angB - angA;
    while (span <= -180.0) span += 360.0;
    while (span > 180.0) span -= 360.0;
    const QRectF arcRect(v.x() - arcR, v.y() - arcR, 2 * arcR, 2 * arcR);
    painter.setPen(dimPen);
    painter.drawArc(arcRect, static_cast<int>(angA * 16), static_cast<int>(span * 16));

    // Label along the bisector, just outside the arc.
    QPointF bis = ua + ub;
    const double lbis = std::hypot(bis.x(), bis.y());
    bis = lbis > 1e-6 ? bis / lbis : QPointF(0, -1);
    const QPointF labelCenter = v + bis * (arcR + 12);
    const QRectF tr = painter.fontMetrics().boundingRect(ang.label).adjusted(-3, -2, 3, 2);
    QRectF box(labelCenter.x() - tr.width() / 2.0, labelCenter.y() - tr.height() / 2.0,
               tr.width(), tr.height());
    const double bx = std::clamp(box.x(), 1.0, width() - box.width() - 1.0);
    const double by = std::clamp(box.y(), 1.0, height() - box.height() - 1.0);
    box.moveTo(bx, by);
    painter.fillRect(box, QColor(255, 255, 255, 220));
    painter.setPen(QColor(150, 30, 30));
    painter.drawText(box, Qt::AlignCenter, ang.label);
  }

  // Anchor dots with names (reference points) — optional.
  if (this->showAnchors)
  for (const auto& anchor : this->data.anchors) {
    const QPointF p = mapToWidget(anchor.pos);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(30, 90, 200));
    painter.drawEllipse(p, 3.5, 3.5);
    painter.setPen(QColor(30, 90, 200));
    painter.drawText(p + QPointF(5, -4), anchor.name);
  }
}

void OrthoViewWidget::mousePressEvent(QMouseEvent *event)
{
  if (!this->data.valid()) return;
  const QPointF click = event->position();

  // Right button: jump to the source of the dimension or dimref under the cursor
  // (left button stays reserved for creating/dragging dimensions and panning).
  if (event->button() == Qt::RightButton) {
    const int di = dimAt(click, /*editableOnly=*/false);
    if (di >= 0 && this->data.dims.at(di).jumpLine >= 0) {
      emit jumpToSource(this->data.dims.at(di).jumpLine);
      return;
    }
    double best = 12.0;
    int line = -1;
    for (const auto& anchor : this->data.anchors) {
      const QPointF p = mapToWidget(anchor.pos);
      const double d = std::hypot(p.x() - click.x(), p.y() - click.y());
      if (d <= best && anchor.sourceLine >= 0) { best = d; line = anchor.sourceLine; }
    }
    if (line >= 0) emit jumpToSource(line);
    return;
  }
  if (event->button() != Qt::LeftButton) return;

  // First: start dragging a label if the click is on one (editable dims only).
  const QFontMetrics fm(overlayFont());
  for (int i = 0; i < this->data.dims.size(); ++i) {
    const auto& dim = this->data.dims.at(i);
    if (dim.id < 0) continue;
    QRectF box = fm.boundingRect(dim.label).adjusted(-3, -2, 3, 2);
    box.moveCenter(mapToWidget(dimLabelPos(dim)));
    if (box.contains(click)) {
      this->draggingLabel = i;
      return;
    }
  }

  // Next: start dragging a dimension line if the click is near one (editable
  // dimensions only, i.e. those with a source id).
  double bestLine = 8.0;
  int hitDim = -1;
  for (int i = 0; i < this->data.dims.size(); ++i) {
    const auto& dim = this->data.dims.at(i);
    if (dim.id < 0) continue;
    QPointF p0, p1;
    dimLineEnds(dim, p0, p1);
    const double d = distToSegment(click, mapToWidget(p0), mapToWidget(p1));
    if (d < bestLine) {
      bestLine = d;
      hitDim = i;
    }
  }
  if (hitDim >= 0) {
    this->draggingDim = hitDim;
    return;
  }

  // Next: report an anchor click (used to create dimensions). Hidden reference
  // points are not clickable, so clicking then just pans.
  if (this->showAnchors) {
    const double radius = 12.0;
    QStringList hits;  // distinct anchor refs within the hit radius
    for (const auto& anchor : this->data.anchors) {
      const QPointF p = mapToWidget(anchor.pos);
      if (std::hypot(p.x() - click.x(), p.y() - click.y()) <= radius &&
          !hits.contains(anchor.ref)) {
        hits << anchor.ref;
      }
    }
    if (hits.size() == 1) {
      emit anchorClicked(hits.first());
      return;
    }
    if (hits.size() > 1) {
      // Several reference points coincide here: let the user pick which one (the
      // menu shows the ref token, e.g. A#0 / A#1, so duplicates are distinguishable).
      QMenu menu(this);
      for (const QString& ref : hits) menu.addAction(ref);
      if (QAction *chosen = menu.exec(event->globalPosition().toPoint())) {
        emit anchorClicked(chosen->text());
      }
      return;
    }
  }

  // Otherwise the click is on empty space: start panning the view.
  this->panning = true;
  this->panLast = click;
}

void OrthoViewWidget::mouseMoveEvent(QMouseEvent *event)
{
  // Pan: translate the view by the mouse delta (widget space).
  if (this->panning) {
    this->userPan += event->position() - this->panLast;
    this->panLast = event->position();
    update();
    return;
  }

  // Label drag: decompose the mouse position (relative to the dimension-line
  // midpoint) into the along/perp frame of the dimension.
  if (this->draggingLabel >= 0 && this->draggingLabel < this->data.dims.size()) {
    OrthoDimension& dim = this->data.dims[this->draggingLabel];
    const QPointF perp = dimPerp(dim);
    QPointF dir = dim.dir;
    const double len = std::hypot(dir.x(), dir.y());
    if (len > 1e-9) dir /= len;
    else dir = QPointF(1, 0);
    const QPointF base = (dim.a + dim.b) / 2.0 + perp * dim.offsetPx;
    const QPointF rel = mapToImage(event->position()) - base;
    dim.labAlongPx = rel.x() * dir.x() + rel.y() * dir.y();
    dim.labPerpPx = rel.x() * perp.x() + rel.y() * perp.y();
    update();
    emit labelOffsetChanged(dim.id, dim.labAlongPx, dim.labPerpPx, false);
    return;
  }

  if (this->draggingDim < 0 || this->draggingDim >= this->data.dims.size()) return;
  OrthoDimension& dim = this->data.dims[this->draggingDim];
  const QPointF mouseImg = mapToImage(event->position());

  // Radial dimension: dragging rotates the line about the feature centre, keeping
  // the rim radius. The source angle is the screen angle negated (view-local y is
  // down, source angle is measured CCW in the y-up view plane).
  if (dim.radial) {
    QPointF rel = mouseImg - dim.center;
    const double L = std::hypot(rel.x(), rel.y());
    if (L < 1e-6) return;
    const double r = std::hypot((dim.b - dim.center).x(), (dim.b - dim.center).y());
    const QPointF u = rel / L;
    dim.dir = u;
    dim.b = dim.center + u * r;
    dim.a = dim.radiusHalf ? dim.center : dim.center - u * r;
    update();
    const double angleDeg = -std::atan2(u.y(), u.x()) * 180.0 / M_PI;
    emit dimensionAngleChanged(dim.id, angleDeg, false);
    return;
  }

  const QPointF perp = dimPerp(dim);
  const QPointF mid = (dim.a + dim.b) / 2.0;
  const QPointF rel = mouseImg - mid;
  dim.offsetPx = rel.x() * perp.x() + rel.y() * perp.y();  // project onto perp
  update();
  emit dimensionOffsetChanged(dim.id, dim.offsetPx, false);
}

int OrthoViewWidget::dimAt(const QPointF& pt, bool editableOnly) const
{
  const QFontMetrics fm(overlayFont());
  // Label box first, then the dimension line.
  for (int i = 0; i < this->data.dims.size(); ++i) {
    const auto& dim = this->data.dims.at(i);
    if (editableOnly && dim.id < 0) continue;
    QRectF box = fm.boundingRect(dim.label).adjusted(-3, -2, 3, 2);
    box.moveCenter(mapToWidget(dimLabelPos(dim)));
    if (box.contains(pt)) return i;
  }
  for (int i = 0; i < this->data.dims.size(); ++i) {
    const auto& dim = this->data.dims.at(i);
    if (editableOnly && dim.id < 0) continue;
    QPointF p0, p1;
    dimLineEnds(dim, p0, p1);
    if (distToSegment(pt, mapToWidget(p0), mapToWidget(p1)) < 8.0) return i;
  }
  return -1;
}

void OrthoViewWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
  if (!this->data.valid()) return;
  const int i = dimAt(event->position());
  if (i >= 0) emit dimensionDoubleClicked(this->data.dims.at(i).id);
}

void OrthoViewWidget::mouseReleaseEvent(QMouseEvent *)
{
  if (this->panning) {
    this->panning = false;
    return;
  }
  if (this->draggingLabel >= 0 && this->draggingLabel < this->data.dims.size()) {
    const OrthoDimension& dim = this->data.dims[this->draggingLabel];
    emit labelOffsetChanged(dim.id, dim.labAlongPx, dim.labPerpPx, true);
    this->draggingLabel = -1;
    return;
  }
  if (this->draggingDim < 0 || this->draggingDim >= this->data.dims.size()) return;
  const OrthoDimension& dim = this->data.dims[this->draggingDim];
  if (dim.radial) {
    const QPointF u = unitDirOf(dim);
    emit dimensionAngleChanged(dim.id, -std::atan2(u.y(), u.x()) * 180.0 / M_PI, true);
  } else {
    emit dimensionOffsetChanged(dim.id, dim.offsetPx, true);
  }
  this->draggingDim = -1;
}

void OrthoViewWidget::wheelEvent(QWheelEvent *event)
{
  if (!this->data.valid()) return;
  const double steps = event->angleDelta().y() / 120.0;
  if (steps == 0.0) return;
  const QPointF cursor = event->position();
  const QPointF imgAt = mapToImage(cursor);  // image point under the cursor (old zoom)
  const double factor = std::pow(1.2, steps);
  this->userZoom = std::clamp(this->userZoom * factor, 0.2, 20.0);
  // Keep imgAt under the cursor: solve userPan from mapToWidget(imgAt) == cursor.
  const double s = this->fitScale * this->userZoom;
  this->userPan = cursor - this->fitOffset - imgAt * s;
  update();
}

// ---------------------------------------------------------------------------
// DrawingViewDock
// ---------------------------------------------------------------------------

namespace {
// Grid position (row, col) for each ortho view, laid out like a drawing sheet:
//   nárys (front)  | bokorys (side)
//   půdorys (top)  |
// The six standard views, order matching io/export_line_drawing.cc standardViews():
// Front, Back, Left, Right, Top, Bottom. Grid positions follow first-angle (ISO-E)
// projection around the centred front view: top view below, bottom above, left
// view to the right, right view to the left, back to the far right.
constexpr int kViewCount = 6;
constexpr int kGridPos[kViewCount][2] = {
  {1, 1},  // Front  (centre)
  {1, 3},  // Back   (far right)
  {1, 2},  // Left   (right of front)
  {1, 0},  // Right  (left of front)
  {2, 1},  // Top    (below front)
  {0, 1},  // Bottom (above front)
};
// Default-visible set: Front, Right, Top (the historical three).
constexpr bool kViewDefault[kViewCount] = {true, false, false, true, true, false};
}  // namespace

DrawingViewDock::DrawingViewDock(QWidget *parent) : Dock(parent)
{
  auto *central = new QWidget(this);
  auto *layout = new QVBoxLayout(central);
  layout->setContentsMargins(4, 4, 4, 4);

  auto *sheet = new QWidget(central);
  auto *grid = new QGridLayout(sheet);
  grid->setContentsMargins(0, 0, 0, 0);

  const char *defaultTitles[kViewCount] = {
    QT_TR_NOOP("Front (nárys)"), QT_TR_NOOP("Back (zadní)"), QT_TR_NOOP("Left (levý bok)"),
    QT_TR_NOOP("Right (pravý bok)"), QT_TR_NOOP("Top (půdorys)"), QT_TR_NOOP("Bottom (spodní)")};

  for (int i = 0; i < kViewCount; ++i) {
    auto *box = new QGroupBox(tr(defaultTitles[i]), sheet);
    // Checkable title = per-view on/off. Unchecked hides the view (the empty
    // titled box stays so the first-angle grid keeps its alignment).
    box->setCheckable(true);
    box->setChecked(kViewDefault[i]);
    auto *boxLayout = new QVBoxLayout(box);
    boxLayout->setContentsMargins(2, 2, 2, 2);
    auto *view = new OrthoViewWidget(box);
    view->setVisible(kViewDefault[i]);
    boxLayout->addWidget(view);
    grid->addWidget(box, kGridPos[i][0], kGridPos[i][1]);
    this->viewWidgets.append(view);
    this->viewBoxes.append(box);
    connect(box, &QGroupBox::toggled, this, [this, i, view](bool on) {
      view->setVisible(on);
      emit viewSelectionChanged();
      this->updateExportEnabled();
    });

    connect(view, &OrthoViewWidget::anchorClicked, this,
            [this, i](const QString& name) { emit anchorClicked(i, name); });
    connect(view, &OrthoViewWidget::dimensionOffsetChanged, this,
            [this, i](int id, double offsetPx, bool finished) {
              emit dimensionOffsetChanged(i, id, offsetPx, finished);
            });
    connect(view, &OrthoViewWidget::dimensionAngleChanged, this,
            [this, i](int id, double angleDeg, bool finished) {
              emit dimensionAngleChanged(i, id, angleDeg, finished);
            });
    connect(view, &OrthoViewWidget::labelOffsetChanged, this,
            [this, i](int id, double alongPx, double perpPx, bool finished) {
              emit labelOffsetChanged(i, id, alongPx, perpPx, finished);
            });
    connect(view, &OrthoViewWidget::dimensionDoubleClicked, this,
            &DrawingViewDock::dimensionDoubleClicked);
    connect(view, &OrthoViewWidget::jumpToSource, this, &DrawingViewDock::jumpToSource);
  }
  for (int r = 0; r < 3; ++r) grid->setRowStretch(r, 1);
  for (int c = 0; c < 4; ++c) grid->setColumnStretch(c, 1);

  this->placeholder = new QLabel(tr("Render (F5) to generate orthographic views."), central);
  this->placeholder->setAlignment(Qt::AlignCenter);

  this->showRefsCheck = new QCheckBox(tr("Reference points"), central);
  this->showRefsCheck->setChecked(true);
  this->showRefsCheck->setToolTip(
    tr("Show the dimref anchor dots and their names (dimension labels stay)."));

  this->showLeadersCheck = new QCheckBox(tr("Leader lines"), central);
  this->showLeadersCheck->setChecked(true);
  this->showLeadersCheck->setToolTip(
    tr("Draw a thin leader from a dragged-away label to its dimension line."));

  this->materializeButton = new QPushButton(tr("Materialize kvóty"), central);
  this->materializeButton->setToolTip(
    tr("Write a // @dim line for each kvota= pair, making them editable."));

  this->scaleSpin = new QDoubleSpinBox(central);
  this->scaleSpin->setPrefix(tr("Scale 1:"));
  this->scaleSpin->setRange(0.01, 10000.0);
  this->scaleSpin->setDecimals(2);
  this->scaleSpin->setValue(1.0);
  this->scaleSpin->setToolTip(tr("Drawing scale for export (model units : drawing mm)."));

  this->exportButton = new QPushButton(tr("Export…"), central);
  this->exportButton->setEnabled(false);
  this->exportButton->setToolTip(
    tr("Export exactly what is shown (edges + dimensions) as scaled vector "
       "SVG, PDF or DXF."));

  this->saveButton = new QPushButton(tr("Save Sheet…"), central);
  this->saveButton->setEnabled(false);
  this->saveButton->setToolTip(tr("Save the sheet as a PNG image (raster)."));

  auto *buttonRow = new QWidget(central);
  auto *buttonLayout = new QHBoxLayout(buttonRow);
  buttonLayout->setContentsMargins(0, 0, 0, 0);
  buttonLayout->addWidget(this->materializeButton);
  buttonLayout->addWidget(this->saveButton);

  auto *checkRow = new QWidget(central);
  auto *checkLayout = new QHBoxLayout(checkRow);
  checkLayout->setContentsMargins(0, 0, 0, 0);
  checkLayout->addWidget(this->showRefsCheck);
  checkLayout->addWidget(this->showLeadersCheck);
  checkLayout->addStretch(1);
  checkLayout->addWidget(this->scaleSpin);
  checkLayout->addWidget(this->exportButton);

  layout->addWidget(sheet, 1);
  layout->addWidget(this->placeholder);
  layout->addWidget(checkRow);
  layout->addWidget(buttonRow);
  setWidget(central);

  connect(this->saveButton, &QPushButton::clicked, this, &DrawingViewDock::saveSheet);
  connect(this->exportButton, &QPushButton::clicked, this, &DrawingViewDock::exportDrawing);
  connect(this->materializeButton, &QPushButton::clicked, this,
          &DrawingViewDock::materializeRequested);
  connect(this->showRefsCheck, &QCheckBox::toggled, this, &DrawingViewDock::setShowAnchors);
  connect(this->showLeadersCheck, &QCheckBox::toggled, this, &DrawingViewDock::setShowLeaders);
  // Changing the scale only affects text / line-weight sizing relative to the
  // geometry (and the export size), so push it to the views without a re-render.
  connect(this->scaleSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          [this](double) {
            for (auto *view : this->viewWidgets) view->setScale(exportScale());
          });
}

double DrawingViewDock::exportScale() const
{
  const double denom = this->scaleSpin->value();
  return denom > 1e-6 ? 1.0 / denom : 1.0;  // drawing-mm per model-mm
}

void DrawingViewDock::setShowAnchors(bool show)
{
  for (auto *view : this->viewWidgets) view->setShowAnchors(show);
}

void DrawingViewDock::setShowLeaders(bool show)
{
  for (auto *view : this->viewWidgets) view->setShowLeaders(show);
}

DrawingViewDock::~DrawingViewDock() = default;

void DrawingViewDock::setViews(const QList<OrthoViewData>& newViews, const QStringList& titles)
{
  this->views = newViews;

  for (int i = 0; i < this->viewWidgets.size(); ++i) {
    if (i < this->views.size()) {
      this->viewWidgets.at(i)->setData(this->views.at(i));
    } else {
      this->viewWidgets.at(i)->clear();
    }
    if (i < titles.size() && !titles.at(i).isEmpty()) {
      if (auto *box = qobject_cast<QGroupBox *>(this->viewWidgets.at(i)->parentWidget())) {
        box->setTitle(titles.at(i));
      }
    }
  }

  updateExportEnabled();
}

bool DrawingViewDock::isViewSelected(int viewIndex) const
{
  return viewIndex >= 0 && viewIndex < this->viewBoxes.size() &&
         this->viewBoxes.at(viewIndex)->isChecked();
}

void DrawingViewDock::updateExportEnabled()
{
  bool any = false;
  for (int i = 0; i < this->views.size(); ++i) {
    if (isViewSelected(i) && this->views.at(i).valid()) any = true;
  }
  this->saveButton->setEnabled(any);
  this->exportButton->setEnabled(any);
  this->placeholder->setVisible(!any);
}

void DrawingViewDock::clearViews()
{
  setViews({}, {});
}

void DrawingViewDock::saveSheet()
{
  if (this->viewWidgets.isEmpty()) return;
  const QString filename = QFileDialog::getSaveFileName(
    this, tr("Save Drawing Sheet"), "drawing.png", tr("PNG image (*.png)"));
  if (filename.isEmpty()) return;

  // Compose the shown views (with overlays) side by side by grabbing each widget.
  QVector<QPixmap> shots;
  int totalW = 0, maxH = 0;
  for (int i = 0; i < this->viewWidgets.size(); ++i) {
    if (!isViewSelected(i)) continue;
    QPixmap shot = this->viewWidgets.at(i)->grab();
    shots.append(shot);
    totalW += shot.width();
    maxH = std::max(maxH, shot.height());
  }
  if (totalW == 0 || maxH == 0) return;
  QImage sheet(totalW, maxH, QImage::Format_ARGB32);
  sheet.fill(Qt::white);
  QPainter painter(&sheet);
  int x = 0;
  for (const auto& shot : shots) {
    painter.drawPixmap(x, 0, shot);
    x += shot.width();
  }
  painter.end();
  sheet.save(filename, "PNG");
}

void DrawingViewDock::exportDrawing()
{
  bool any = false;
  for (int i = 0; i < this->views.size(); ++i)
    if (isViewSelected(i) && this->views.at(i).valid()) any = true;
  if (!any) return;

  const double scale = exportScale();  // drawing-mm per model-mm
  const bool leaders = this->showLeadersCheck->isChecked();

  const QString filename = QFileDialog::getSaveFileName(
    this, tr("Export drawing"), "drawing.svg",
    tr("SVG (*.svg);;PDF (*.pdf);;DXF (*.dxf)"));
  if (filename.isEmpty()) return;

  const double dpi = 96.0;
  const double mmToPx = dpi / 25.4;
  const double marginMm = 10.0, gapMm = 12.0, titleMm = 5.0;
  const double fontMm = 3.0, edgeMm = 0.3, dimMm = 0.25, witMm = 0.15, arrowMm = 2.5;

  // Vertical layout: title + view stacked, sizes in drawing-mm (geometry * scale).
  QVector<double> viewYmm;
  double sheetWmm = 0.0, sheetHmm = marginMm;
  for (int i = 0; i < this->views.size(); ++i) {
    const auto& v = this->views.at(i);
    if (!v.valid() || !isViewSelected(i)) { viewYmm << 0; continue; }
    const double vh = v.sizeMm.height() * scale;
    sheetHmm += titleMm;
    viewYmm << sheetHmm;
    sheetHmm += vh + gapMm;
    sheetWmm = std::max(sheetWmm, v.sizeMm.width() * scale);
  }
  sheetWmm += 2 * marginMm;
  sheetHmm += marginMm;

  // Build the whole sheet once as primitives in drawing-mm (y-down, origin at the
  // top-left of the sheet). Both the raster/vector (QPainter) and the DXF writer
  // consume the same primitives, so all three formats are truly WYSIWYG.
  std::vector<QLineF> edgeSegs, dimSegs, witSegs;
  std::vector<QPolygonF> arrows;
  std::vector<std::pair<QPointF, QString>> labels, titles;
  for (int i = 0; i < this->views.size(); ++i) {
    const auto& v = this->views[i];
    if (!v.valid() || !isViewSelected(i)) continue;
    const double oxmm = marginMm, oymm = viewYmm[i];
    auto map = [&](const QPointF& mp) {
      return QPointF(oxmm + mp.x() * scale, oymm + mp.y() * scale);  // -> drawing-mm
    };
    titles.emplace_back(QPointF(oxmm, oymm - 1.5), v.title);
    for (const auto& e : v.edges) edgeSegs.emplace_back(map(e.p1()), map(e.p2()));

    auto addArrow = [&](const QPointF& tip, const QPointF& along, const QPointF& nrm) {
      QPolygonF h;
      h << tip << (tip + along * arrowMm + nrm * arrowMm * 0.4)
        << (tip + along * arrowMm - nrm * arrowMm * 0.4);
      arrows.push_back(h);
    };
    // Leader from a dragged-away label to the nearest point of its dimension line.
    auto addLeader = [&](const QPointF& lp, const QPointF& s0, const QPointF& s1) {
      if (!leaders) return;
      const QPointF foot = closestOnSegment(lp, s0, s1);
      if (std::hypot((lp - foot).x(), (lp - foot).y()) > fontMm) witSegs.emplace_back(lp, foot);
    };
    for (const auto& d : v.dims) {
      QPointF e0, e1;
      dimLineEnds(d, e0, e1);
      const QPointF a2 = map(e0), b2 = map(e1);
      const QPointF labelPos = map(labelPosOf(d));
      QPointF sdir = b2 - a2;
      const double sl = std::hypot(sdir.x(), sdir.y());
      if (d.radial) {
        // Line through the centre, no witness lines; arrow at the rim (and the
        // opposite rim for a diameter); a leader from a dragged-away label.
        dimSegs.emplace_back(a2, b2);
        if (sl > 1e-6) {
          sdir /= sl;
          const QPointF nrm(-sdir.y(), sdir.x());
          if (d.arrowsOut) {
            dimSegs.emplace_back(b2, b2 + sdir * (arrowMm * 1.5));
            addArrow(b2, sdir, nrm);
            if (!d.radiusHalf) {
              dimSegs.emplace_back(a2, a2 - sdir * (arrowMm * 1.5));
              addArrow(a2, -sdir, nrm);
            }
          } else {
            addArrow(b2, -sdir, nrm);
            if (!d.radiusHalf) addArrow(a2, sdir, nrm);
          }
        }
        addLeader(labelPos, a2, b2);
        labels.emplace_back(labelPos, d.label);
        continue;
      }
      const QPointF perp = perpOf(d);
      const QPointF a = map(d.a), b = map(d.b);
      if (std::hypot((a2 - a).x(), (a2 - a).y()) > 0.5) witSegs.emplace_back(a, a2);
      if (std::hypot((b2 - b).x(), (b2 - b).y()) > 0.5) witSegs.emplace_back(b, b2);
      dimSegs.emplace_back(a2, b2);
      if (sl > 1e-6) {
        sdir /= sl;
        const QPointF nrm(-sdir.y(), sdir.x());
        if (d.arrowsOut) { addArrow(a2, -sdir, nrm); addArrow(b2, sdir, nrm); }
        else { addArrow(a2, sdir, nrm); addArrow(b2, -sdir, nrm); }
      }
      addLeader(labelPos, a2, b2);
      labels.emplace_back(labelPos, d.label);
    }
    for (const auto& ang : v.angles) {
      const QPointF vtx = map(ang.vertex), pa = map(ang.a), pb = map(ang.b);
      witSegs.emplace_back(vtx, pa);
      witSegs.emplace_back(vtx, pb);
      QPointF bis = (pa - vtx) + (pb - vtx);
      const double bl = std::hypot(bis.x(), bis.y());
      bis = bl > 1e-6 ? bis / bl : QPointF(0, -1);
      labels.emplace_back(vtx + bis * (arrowMm * 4), ang.label);
    }
  }

  if (filename.endsWith(".dxf", Qt::CaseInsensitive)) {
    std::ofstream out(filename.toStdString());
    if (!out.is_open()) return;
    auto Y = [&](double y) { return sheetHmm - y; };  // DXF is y-up
    auto line = [&](const QPointF& a, const QPointF& b) {
      out << "0\nLINE\n8\n0\n10\n" << a.x() << "\n20\n" << Y(a.y()) << "\n30\n0\n11\n"
          << b.x() << "\n21\n" << Y(b.y()) << "\n31\n0\n";
    };
    out << "0\nSECTION\n2\nENTITIES\n";
    for (const auto& e : edgeSegs) line(e.p1(), e.p2());
    for (const auto& w : witSegs) line(w.p1(), w.p2());
    for (const auto& d : dimSegs) line(d.p1(), d.p2());
    for (const auto& h : arrows)
      for (int k = 0; k < h.size(); ++k) line(h[k], h[(k + 1) % h.size()]);
    auto text = [&](const QPointF& p, const QString& s, double h) {
      // Horizontally centred (group code 72 = 1); alignment point in 11/21.
      out << "0\nTEXT\n8\n0\n10\n" << p.x() << "\n20\n" << Y(p.y()) << "\n30\n0\n40\n" << h
          << "\n72\n1\n11\n" << p.x() << "\n21\n" << Y(p.y()) << "\n31\n0\n1\n"
          << s.toStdString() << "\n";
    };
    for (const auto& [p, s] : labels) text(p, s, fontMm);
    for (const auto& [p, s] : titles) text(QPointF(p.x() + 10, p.y()), s, fontMm);
    out << "0\nENDSEC\n0\nEOF\n";
    return;
  }

  // SVG / PDF via QPainter: draw the same primitives in device px.
  const int sheetWpx = qRound(sheetWmm * mmToPx);
  const int sheetHpx = qRound(sheetHmm * mmToPx);
  auto paintSheet = [&](QPainter& p) {
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(QRectF(0, 0, sheetWpx, sheetHpx), Qt::white);
    QFont font = p.font();
    font.setPixelSize(std::max(1, static_cast<int>(std::round(fontMm * mmToPx))));
    p.setFont(font);
    auto P = [&](const QPointF& mm) { return QPointF(mm.x() * mmToPx, mm.y() * mmToPx); };

    p.setPen(Qt::black);
    for (const auto& [pos, txt] : titles) p.drawText(P(pos), txt);

    QPen edgePen(Qt::black);
    edgePen.setWidthF(edgeMm * mmToPx);
    edgePen.setCapStyle(Qt::RoundCap);
    p.setPen(edgePen);
    for (const auto& e : edgeSegs) p.drawLine(P(e.p1()), P(e.p2()));

    QPen witPen(QColor(120, 120, 120));
    witPen.setWidthF(witMm * mmToPx);
    p.setPen(witPen);
    for (const auto& w : witSegs) p.drawLine(P(w.p1()), P(w.p2()));

    QPen dimPen(Qt::black);
    dimPen.setWidthF(dimMm * mmToPx);
    p.setPen(dimPen);
    for (const auto& d : dimSegs) p.drawLine(P(d.p1()), P(d.p2()));

    p.setPen(Qt::NoPen);
    p.setBrush(Qt::black);
    for (const auto& h : arrows) {
      QPolygonF q;
      for (const auto& pt : h) q << P(pt);
      p.drawPolygon(q);
    }

    for (const auto& [pos, txt] : labels) {
      const QPointF c = P(pos);
      const QRectF tr = p.fontMetrics().boundingRect(txt).adjusted(-2, -1, 2, 1);
      QRectF box(c.x() - tr.width() / 2, c.y() - tr.height() / 2, tr.width(), tr.height());
      p.fillRect(box, QColor(255, 255, 255, 220));
      p.setPen(QColor(150, 30, 30));
      p.drawText(box, Qt::AlignCenter, txt);
    }
  };

  if (filename.endsWith(".pdf", Qt::CaseInsensitive)) {
    QPdfWriter pdf(filename);
    pdf.setResolution(static_cast<int>(dpi));
    pdf.setPageSize(QPageSize(QSizeF(sheetWmm, sheetHmm), QPageSize::Millimeter));
    pdf.setPageMargins(QMarginsF(0, 0, 0, 0));
    QPainter p(&pdf);
    paintSheet(p);
  } else {
    QSvgGenerator svg;
    svg.setFileName(filename);
    svg.setResolution(static_cast<int>(dpi));
    svg.setSize(QSize(sheetWpx, sheetHpx));
    svg.setViewBox(QRect(0, 0, sheetWpx, sheetHpx));
    QPainter p(&svg);
    paintSheet(p);
  }
}
