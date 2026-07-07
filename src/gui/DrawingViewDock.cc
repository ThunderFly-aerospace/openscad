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
// DrawingSheetWidget
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

DrawingSheetWidget::DrawingSheetWidget(QWidget *parent) : QWidget(parent)
{
  setMinimumSize(160, 120);
  setMouseTracking(false);
}

void DrawingSheetWidget::setSheet(const QVector<PlacedView>& placements, double wMm, double hMm)
{
  this->places = placements;
  this->sheetWmm = wMm;
  this->sheetHmm = hMm;
  this->dragPlace = -1;
  this->draggingDim = -1;
  this->draggingLabel = -1;
  update();
}

void DrawingSheetWidget::setDetailMode(bool on)
{
  this->detailMode = on;
  this->detailDragging = false;
  setCursor(on ? Qt::CrossCursor : Qt::ArrowCursor);
  update();
}

void DrawingSheetWidget::clear()
{
  this->places.clear();
  this->sheetWmm = this->sheetHmm = 0.0;
  this->userZoom = 1.0;
  this->userPan = QPointF();
  this->dragPlace = this->draggingDim = this->draggingLabel = -1;
  update();
}

void DrawingSheetWidget::setShowAnchors(bool show)
{
  this->showAnchors = show;
  update();
}

void DrawingSheetWidget::recomputeFit()
{
  if (this->sheetWmm <= 1e-9 || this->sheetHmm <= 1e-9) {
    this->fitScale = 1.0;
    this->fitOffset = QPointF();
    return;
  }
  this->fitScale = std::min(static_cast<double>(width()) / this->sheetWmm,
                            static_cast<double>(height()) / this->sheetHmm);
  const double drawW = this->sheetWmm * this->fitScale;
  const double drawH = this->sheetHmm * this->fitScale;
  this->fitOffset = QPointF((width() - drawW) / 2.0, (height() - drawH) / 2.0);
}

QPointF DrawingSheetWidget::sheetToWidget(const QPointF& sheetMm) const
{
  const double s = this->fitScale * this->userZoom;
  return QPointF(sheetMm.x() * s + this->fitOffset.x() + this->userPan.x(),
                 sheetMm.y() * s + this->fitOffset.y() + this->userPan.y());
}

QPointF DrawingSheetWidget::widgetToSheet(const QPointF& widgetPt) const
{
  const double s = this->fitScale * this->userZoom;
  if (s < 1e-9) return QPointF();
  return QPointF((widgetPt.x() - this->fitOffset.x() - this->userPan.x()) / s,
                 (widgetPt.y() - this->fitOffset.y() - this->userPan.y()) / s);
}

QPointF DrawingSheetWidget::mapToWidget(const PlacedView& pl, const QPointF& viewLocalMm) const
{
  // view-local model-mm -> sheet drawing-mm (this tile's own scale) -> widget px.
  const double sc = pl.drawScale > 1e-9 ? pl.drawScale : 1.0;
  const QPointF sheetMm(pl.originMm.x() + viewLocalMm.x() * sc,
                        pl.originMm.y() + viewLocalMm.y() * sc);
  return sheetToWidget(sheetMm);
}

QPointF DrawingSheetWidget::mapToViewLocal(const PlacedView& pl, const QPointF& widgetPt) const
{
  const QPointF sheetMm = widgetToSheet(widgetPt);
  const double sc = pl.drawScale > 1e-9 ? pl.drawScale : 1.0;
  return QPointF((sheetMm.x() - pl.originMm.x()) / sc,
                 (sheetMm.y() - pl.originMm.y()) / sc);
}

double DrawingSheetWidget::absPx(double drawingMm) const
{
  // Sheet coordinates are drawing-mm, so a drawing-mm length is just scaled by the
  // current px-per-drawing-mm (fit * zoom).
  return drawingMm * this->fitScale * this->userZoom;
}

QFont DrawingSheetWidget::overlayFont() const
{
  // Dimension text ~3 mm on the drawing (a common technical-drawing text height).
  QFont f = font();
  const double px = absPx(3.0);
  f.setPixelSize(std::max(1, static_cast<int>(std::round(px))));
  return f;
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

void DrawingSheetWidget::paintEvent(QPaintEvent *)
{
  QPainter painter(this);
  painter.fillRect(rect(), Qt::white);

  if (this->places.isEmpty()) {
    painter.setPen(Qt::gray);
    painter.drawText(rect(), Qt::AlignCenter, tr("(no preview)"));
    return;
  }

  recomputeFit();  // fit the whole sheet (drawing-mm) into the widget
  painter.setRenderHint(QPainter::Antialiasing, true);
  for (const auto& pl : this->places) paintPlacement(painter, pl);

  // "New detail" rubber-band circle (widget space).
  if (this->detailDragging) {
    const double rpx = std::hypot(this->detailCur.x() - this->detailCenter.x(),
                                  this->detailCur.y() - this->detailCenter.y());
    QPen p(QColor(200, 60, 60));
    p.setWidthF(1.5);
    p.setStyle(Qt::DashLine);
    painter.setPen(p);
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(this->detailCenter, rpx, rpx);
  }
}

void DrawingSheetWidget::paintPlacement(QPainter& painter, const PlacedView& pl)
{
  const OrthoViewData& data = pl.data;
  if (!data.valid()) return;
  auto map = [&](const QPointF& p) { return mapToWidget(pl, p); };

  painter.setFont(overlayFont());

  // View title just above the tile (at its drawing-mm origin corner).
  if (!data.title.isEmpty()) {
    painter.setPen(Qt::black);
    painter.drawText(map(QPointF(0, 0)) - QPointF(0, absPx(1.5)), data.title);
  }

  // Section cut-face hatching first, so edges sit on top of it.
  if (!data.hatch.isEmpty()) {
    QPen hatchPen(QColor(90, 90, 90));
    hatchPen.setWidthF(std::max(0.4, absPx(0.13)));
    painter.setPen(hatchPen);
    for (const auto& hh : data.hatch)
      painter.drawLine(map(hh.p1()), map(hh.p2()));
  }

  // The line drawing itself: silhouette + feature edges (model-mm -> widget).
  QPen edgePen(Qt::black);
  edgePen.setWidthF(std::max(0.6, absPx(0.3)));
  edgePen.setCapStyle(Qt::RoundCap);
  painter.setPen(edgePen);
  for (const auto& e : data.edges) {
    painter.drawLine(map(e.p1()), map(e.p2()));
  }

  // Detail markers: a thin circle around each enlarged region, labelled with its
  // letter (ISO 128-34). center/r are this view's view-local model-mm.
  for (const auto& dm : data.detailMarks) {
    const QPointF c = map(dm.center);
    const QPointF rim = map(dm.center + QPointF(dm.r, 0));
    const double rpx = std::hypot(rim.x() - c.x(), rim.y() - c.y());
    QPen thin(QColor(20, 20, 20));
    thin.setWidthF(std::max(0.6, absPx(0.3)));
    painter.setPen(thin);
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(c, rpx, rpx);
    QFont f = overlayFont();
    f.setBold(true);
    painter.setFont(f);
    painter.setPen(QColor(20, 20, 20));
    painter.drawText(c + QPointF(rpx * 0.72, -rpx * 0.72), dm.label);
    painter.setFont(overlayFont());
  }

  // Cutting-plane lines (section markers seen edge-on in this view): a thick chain
  // line across the view with arrowheads and the section letter at both ends.
  for (const auto& sl : data.sectionLines) {
    const QPointF a = map(sl.p0), b = map(sl.p1);
    QPen chain(QColor(20, 20, 20));
    chain.setWidthF(std::max(0.8, absPx(0.5)));
    chain.setStyle(Qt::DashDotLine);
    painter.setPen(chain);
    painter.setBrush(Qt::NoBrush);
    painter.drawLine(a, b);
    const double s = std::max(5.0, absPx(3.0));
    QPointF ad = sl.arrowDir;
    const double adl = std::hypot(ad.x(), ad.y());
    if (adl > 1e-6) {
      ad /= adl;
      const QPointF nrm(-ad.y(), ad.x());
      painter.setPen(Qt::NoPen);
      painter.setBrush(QColor(20, 20, 20));
      QFont f = overlayFont();
      f.setBold(true);
      for (const QPointF& end : {a, b}) {
        // Short stem outward from the line end, arrowhead, then the letter.
        painter.setPen(chain);
        painter.drawLine(end, end + ad * s);
        const QPointF tip = end + ad * s;
        painter.setPen(Qt::NoPen);
        QPolygonF head;
        head << tip << (tip - ad * s * 0.9 + nrm * s * 0.35) << (tip - ad * s * 0.9 - nrm * s * 0.35);
        painter.drawPolygon(head);
        painter.setPen(QColor(20, 20, 20));
        painter.setFont(f);
        painter.drawText(tip + ad * (s * 0.4) - QPointF(s * 0.3, -s * 0.3), sl.label);
      }
      painter.setFont(overlayFont());
    }
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
                       const QPointF& seg1, bool leader) {
    const QRectF tr = painter.fontMetrics().boundingRect(text).adjusted(-3, -2, 3, 2);
    QRectF box(center.x() - tr.width() / 2.0, center.y() - tr.height() / 2.0,
               tr.width(), tr.height());
    const double bx = std::clamp(box.x(), 1.0, width() - box.width() - 1.0);
    const double by = std::clamp(box.y(), 1.0, height() - box.height() - 1.0);
    box.moveTo(bx, by);
    if (leader) {
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
  for (const auto& dim : data.dims) {
    if (!dim.radial) continue;
    QPointF p0, p1;
    dimLineEnds(dim, p0, p1);
    const QPointF a2 = map(p0), b2 = map(p1);
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
    drawLabel(map(labelPosOf(dim)), dim.label, a2, b2, dim.leader);
  }

  for (const auto& dim : data.dims) {
    if (dim.radial) continue;
    const QPointF a = map(dim.a);
    const QPointF b = map(dim.b);
    const QPointF perp = perpOf(dim);
    // Endpoints of the dimension line: feet of the anchors on the offset line
    // along the measurement direction (image space, then mapped to widget).
    const QPointF a2 = map(dimFoot(dim, perp, dim.a));
    const QPointF b2 = map(dimFoot(dim, perp, dim.b));

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
    drawLabel(map(labelPosOf(dim)), dim.label, a2, b2, dim.leader);
  }

  // Angle dimensions: two rays from the vertex, an arc between them, and a label.
  for (const auto& ang : data.angles) {
    const QPointF v = map(ang.vertex);
    const QPointF a = map(ang.a);
    const QPointF b = map(ang.b);
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
  for (const auto& anchor : data.anchors) {
    const QPointF p = map(anchor.pos);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(30, 90, 200));
    painter.drawEllipse(p, 3.5, 3.5);
    painter.setPen(QColor(30, 90, 200));
    painter.drawText(p + QPointF(5, -4), anchor.name);
  }
}

int DrawingSheetWidget::placementAt(const QPointF& widgetPt) const
{
  int anyHit = -1;
  for (int i = 0; i < this->places.size(); ++i) {
    const PlacedView& pl = this->places.at(i);
    if (!pl.data.valid()) continue;
    const QPointF a = mapToWidget(pl, QPointF(0, 0));
    const QPointF b = mapToWidget(pl, QPointF(pl.data.sizeMm.width(), pl.data.sizeMm.height()));
    // A small margin so anchors sitting exactly on the outline stay grabbable.
    const QRectF box = QRectF(a, b).normalized().adjusted(-8, -8, 8, 8);
    if (box.contains(widgetPt)) {
      if (pl.viewIndex >= 0) return i;  // prefer interactive standard views on overlap
      if (anyHit < 0) anyHit = i;
    }
  }
  return anyHit;
}

void DrawingSheetWidget::mousePressEvent(QMouseEvent *event)
{
  if (this->places.isEmpty()) return;
  const QPointF click = event->position();
  const int pi = placementAt(click);

  // "New detail" mode: a left-drag on a standard view rubber-bands the circle.
  if (this->detailMode && event->button() == Qt::LeftButton) {
    if (pi >= 0 && this->places.at(pi).viewIndex >= 0) {
      this->detailDragging = true;
      this->detailPlace = pi;
      this->detailCenter = click;
      this->detailCur = click;
      update();
    }
    return;
  }

  // Right button: jump to the source of the dimension or dimref under the cursor.
  if (event->button() == Qt::RightButton) {
    if (pi >= 0) {
      const PlacedView& pl = this->places.at(pi);
      const int di = dimAt(pl, click, /*editableOnly=*/false);
      if (di >= 0 && pl.data.dims.at(di).jumpLine >= 0) {
        emit jumpToSource(pl.data.dims.at(di).jumpLine);
        return;
      }
      double best = 12.0;
      int line = -1;
      for (const auto& anchor : pl.data.anchors) {
        const QPointF p = mapToWidget(pl, anchor.pos);
        const double d = std::hypot(p.x() - click.x(), p.y() - click.y());
        if (d <= best && anchor.sourceLine >= 0) { best = d; line = anchor.sourceLine; }
      }
      if (line >= 0) emit jumpToSource(line);
    }
    return;
  }
  if (event->button() != Qt::LeftButton) return;

  // Interactive editing only on standard-view placements; section / detail tiles
  // and empty space fall through to panning the whole sheet.
  if (pi >= 0 && this->places.at(pi).viewIndex >= 0) {
    const PlacedView& pl = this->places.at(pi);

    // First: start dragging a label if the click is on one (editable dims only).
    const QFontMetrics fm(overlayFont());
    for (int i = 0; i < pl.data.dims.size(); ++i) {
      const auto& dim = pl.data.dims.at(i);
      if (dim.id < 0) continue;
      QRectF box = fm.boundingRect(dim.label).adjusted(-3, -2, 3, 2);
      box.moveCenter(mapToWidget(pl, labelPosOf(dim)));
      if (box.contains(click)) { this->dragPlace = pi; this->draggingLabel = i; return; }
    }

    // Next: start dragging a dimension line if the click is near one.
    double bestLine = 8.0;
    int hitDim = -1;
    for (int i = 0; i < pl.data.dims.size(); ++i) {
      const auto& dim = pl.data.dims.at(i);
      if (dim.id < 0) continue;
      QPointF p0, p1;
      dimLineEnds(dim, p0, p1);
      const double d = distToSegment(click, mapToWidget(pl, p0), mapToWidget(pl, p1));
      if (d < bestLine) { bestLine = d; hitDim = i; }
    }
    if (hitDim >= 0) { this->dragPlace = pi; this->draggingDim = hitDim; return; }

    // Next: report an anchor click (used to create dimensions).
    if (this->showAnchors) {
      const double radius = 12.0;
      QStringList hits;  // distinct anchor refs within the hit radius
      for (const auto& anchor : pl.data.anchors) {
        const QPointF p = mapToWidget(pl, anchor.pos);
        if (std::hypot(p.x() - click.x(), p.y() - click.y()) <= radius &&
            !hits.contains(anchor.ref)) {
          hits << anchor.ref;
        }
      }
      if (hits.size() == 1) { emit anchorClicked(pl.viewIndex, hits.first()); return; }
      if (hits.size() > 1) {
        QMenu menu(this);
        for (const QString& ref : hits) menu.addAction(ref);
        if (QAction *chosen = menu.exec(event->globalPosition().toPoint()))
          emit anchorClicked(pl.viewIndex, chosen->text());
        return;
      }
    }
  }

  // Otherwise the click is on empty space / a display-only tile: pan the sheet.
  this->panning = true;
  this->panLast = click;
}

void DrawingSheetWidget::mouseMoveEvent(QMouseEvent *event)
{
  // "New detail" rubber-band: grow the circle from its centre.
  if (this->detailDragging) {
    this->detailCur = event->position();
    update();
    return;
  }

  // Pan: translate the sheet by the mouse delta (widget space).
  if (this->panning) {
    this->userPan += event->position() - this->panLast;
    this->panLast = event->position();
    update();
    return;
  }
  if (this->dragPlace < 0 || this->dragPlace >= this->places.size()) return;
  PlacedView& pl = this->places[this->dragPlace];

  // Label drag: decompose the mouse position (relative to the dimension-line
  // midpoint) into the along/perp frame of the dimension.
  if (this->draggingLabel >= 0 && this->draggingLabel < pl.data.dims.size()) {
    OrthoDimension& dim = pl.data.dims[this->draggingLabel];
    const QPointF perp = perpOf(dim);
    QPointF dir = dim.dir;
    const double len = std::hypot(dir.x(), dir.y());
    if (len > 1e-9) dir /= len;
    else dir = QPointF(1, 0);
    const QPointF base = (dim.a + dim.b) / 2.0 + perp * dim.offsetPx;
    const QPointF rel = mapToViewLocal(pl, event->position()) - base;
    dim.labAlongPx = rel.x() * dir.x() + rel.y() * dir.y();
    dim.labPerpPx = rel.x() * perp.x() + rel.y() * perp.y();
    update();
    emit labelOffsetChanged(pl.viewIndex, dim.id, dim.labAlongPx, dim.labPerpPx, false);
    return;
  }

  if (this->draggingDim < 0 || this->draggingDim >= pl.data.dims.size()) return;
  OrthoDimension& dim = pl.data.dims[this->draggingDim];
  const QPointF mouseImg = mapToViewLocal(pl, event->position());

  // Radial dimension: dragging rotates the line about the feature centre.
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
    emit dimensionAngleChanged(pl.viewIndex, dim.id, angleDeg, false);
    return;
  }

  const QPointF perp = perpOf(dim);
  const QPointF mid = (dim.a + dim.b) / 2.0;
  const QPointF rel = mouseImg - mid;
  dim.offsetPx = rel.x() * perp.x() + rel.y() * perp.y();  // project onto perp
  update();
  emit dimensionOffsetChanged(pl.viewIndex, dim.id, dim.offsetPx, false);
}

int DrawingSheetWidget::dimAt(const PlacedView& pl, const QPointF& pt, bool editableOnly) const
{
  const QFontMetrics fm(overlayFont());
  // Label box first, then the dimension line.
  for (int i = 0; i < pl.data.dims.size(); ++i) {
    const auto& dim = pl.data.dims.at(i);
    if (editableOnly && dim.id < 0) continue;
    QRectF box = fm.boundingRect(dim.label).adjusted(-3, -2, 3, 2);
    box.moveCenter(mapToWidget(pl, labelPosOf(dim)));
    if (box.contains(pt)) return i;
  }
  for (int i = 0; i < pl.data.dims.size(); ++i) {
    const auto& dim = pl.data.dims.at(i);
    if (editableOnly && dim.id < 0) continue;
    QPointF p0, p1;
    dimLineEnds(dim, p0, p1);
    if (distToSegment(pt, mapToWidget(pl, p0), mapToWidget(pl, p1)) < 8.0) return i;
  }
  return -1;
}

void DrawingSheetWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
  const int pi = placementAt(event->position());
  if (pi < 0 || this->places.at(pi).viewIndex < 0) return;
  const PlacedView& pl = this->places.at(pi);
  const int i = dimAt(pl, event->position());
  if (i >= 0) emit dimensionDoubleClicked(pl.data.dims.at(i).id);
}

void DrawingSheetWidget::mouseReleaseEvent(QMouseEvent *)
{
  // Finish a "new detail" gesture: emit the circle in the parent view's local mm.
  if (this->detailDragging) {
    const int pi = this->detailPlace;
    if (pi >= 0 && pi < this->places.size() && this->places.at(pi).viewIndex >= 0) {
      const PlacedView& pl = this->places.at(pi);
      const double rpx = std::hypot(this->detailCur.x() - this->detailCenter.x(),
                                    this->detailCur.y() - this->detailCenter.y());
      if (rpx > 4.0) {
        const QPointF c = mapToViewLocal(pl, this->detailCenter);
        const QPointF rimPt = mapToViewLocal(pl, this->detailCur);
        const double r = std::hypot(rimPt.x() - c.x(), rimPt.y() - c.y());
        emit detailRequested(pl.viewIndex, c.x(), c.y(), r);
      }
    }
    this->detailDragging = false;
    this->detailMode = false;
    this->detailPlace = -1;
    setCursor(Qt::ArrowCursor);
    emit detailModeFinished();
    update();
    return;
  }

  if (this->panning) {
    this->panning = false;
    return;
  }
  if (this->dragPlace < 0 || this->dragPlace >= this->places.size()) {
    this->draggingLabel = this->draggingDim = -1;
    return;
  }
  PlacedView& pl = this->places[this->dragPlace];
  if (this->draggingLabel >= 0 && this->draggingLabel < pl.data.dims.size()) {
    const OrthoDimension& dim = pl.data.dims[this->draggingLabel];
    emit labelOffsetChanged(pl.viewIndex, dim.id, dim.labAlongPx, dim.labPerpPx, true);
  } else if (this->draggingDim >= 0 && this->draggingDim < pl.data.dims.size()) {
    const OrthoDimension& dim = pl.data.dims[this->draggingDim];
    if (dim.radial) {
      const QPointF u = unitDirOf(dim);
      emit dimensionAngleChanged(pl.viewIndex, dim.id,
                                 -std::atan2(u.y(), u.x()) * 180.0 / M_PI, true);
    } else {
      emit dimensionOffsetChanged(pl.viewIndex, dim.id, dim.offsetPx, true);
    }
  }
  this->dragPlace = this->draggingLabel = this->draggingDim = -1;
}

void DrawingSheetWidget::wheelEvent(QWheelEvent *event)
{
  if (this->places.isEmpty()) return;
  const double steps = event->angleDelta().y() / 120.0;
  if (steps == 0.0) return;
  recomputeFit();
  const QPointF cursor = event->position();
  const QPointF sheetAt = widgetToSheet(cursor);  // sheet-mm under the cursor (old zoom)
  const double factor = std::pow(1.2, steps);
  this->userZoom = std::clamp(this->userZoom * factor, 0.2, 40.0);
  // Keep sheetAt under the cursor: solve userPan from sheetToWidget(sheetAt) == cursor.
  const double s = this->fitScale * this->userZoom;
  this->userPan = cursor - this->fitOffset - sheetAt * s;
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
// Filename slug per view (for the "separate files" export).
const char *const kViewSlug[kViewCount] = {"front", "back", "left", "right", "top", "bottom"};

// One tile to place on the sheet: its data, its first-angle grid cell, a filename
// slug, the interactive view index (0..5, or -1 for display-only section / detail
// tiles), whether it is a section tile, and its drawing-mm-per-model-mm scale (the
// sheet scale for standard / section tiles, the detail's own scale for a detail).
struct SheetTile {
  const OrthoViewData *v;
  int gridR, gridC;
  QString slug;
  int viewIndex;
  bool isSection;
  double drawScale;
};

// The selected standard views (at their first-angle cells) plus the valid section
// tiles and detail tiles (rows below the 3-row grid, four per row). Shared by the
// interactive canvas and the composed export so both lay out identically. Standard
// and section tiles draw at `sheetScale`; each detail tile draws at its own scale
// (data.scale), so it appears enlarged on the sheet.
QVector<SheetTile> collectSheetTiles(const QList<OrthoViewData>& views,
                                     const QList<OrthoViewData>& sections,
                                     const QList<OrthoViewData>& details,
                                     const std::vector<bool>& viewSelected,
                                     double sheetScale)
{
  QVector<SheetTile> tiles;
  for (int i = 0; i < views.size() && i < kViewCount; ++i)
    if (i < static_cast<int>(viewSelected.size()) && viewSelected[i] && views.at(i).valid())
      tiles.push_back({&views.at(i), kGridPos[i][0], kGridPos[i][1], kViewSlug[i], i, false,
                       sheetScale});
  int cell = 0;
  for (int i = 0; i < sections.size(); ++i) {
    if (!sections.at(i).valid()) continue;
    tiles.push_back({&sections.at(i), 3 + cell / 4, cell % 4,
                     QString("section%1").arg(i + 1), -1, true, sheetScale});
    ++cell;
  }
  // Details continue in the rows after the sections (never sharing a row with them).
  const int detailRowBase = 3 + (cell + 3) / 4;
  for (int i = 0, dcell = 0; i < details.size(); ++i) {
    if (!details.at(i).valid()) continue;
    const double ds = details.at(i).scale > 1e-9 ? details.at(i).scale : 1.0;
    tiles.push_back({&details.at(i), detailRowBase + dcell / 4, dcell % 4,
                     QString("detail%1").arg(i + 1), -1, false, ds});
    ++dcell;
  }
  return tiles;
}

// First-angle sheet layout: origin (drawing-mm top-left of the view content) per
// tile plus the total sheet size. Same-axis views share equal-width/height cells so
// centring aligns them (top under front, sides level). Row height includes a title
// band. Each tile is sized by its own drawScale. Result origins parallel `tiles`.
struct SheetLayoutResult { QVector<QPointF> origins; double wMm; double hMm; };

SheetLayoutResult computeSheetLayout(const QVector<SheetTile>& tiles,
                                     double marginMm, double gapMm, double titleMm)
{
  SheetLayoutResult r;
  r.origins.resize(tiles.size());
  int maxR = 2;
  for (const auto& t : tiles) maxR = std::max(maxR, t.gridR);
  std::vector<double> colW(4, 0.0), rowH(maxR + 1, 0.0), colX(4, 0.0), rowY(maxR + 1, 0.0);
  for (const auto& t : tiles) {
    colW[t.gridC] = std::max(colW[t.gridC], t.v->sizeMm.width() * t.drawScale);
    rowH[t.gridR] = std::max(rowH[t.gridR], t.v->sizeMm.height() * t.drawScale);
  }
  double x = marginMm, y = marginMm;
  for (int c = 0; c < 4; ++c) { colX[c] = x; if (colW[c] > 0) x += colW[c] + gapMm; }
  for (int rr = 0; rr <= maxR; ++rr) { rowY[rr] = y; if (rowH[rr] > 0) y += titleMm + rowH[rr] + gapMm; }
  r.wMm = std::max(2 * marginMm, x - gapMm + marginMm);
  r.hMm = std::max(2 * marginMm, y - gapMm + marginMm);
  for (int i = 0; i < tiles.size(); ++i) {
    const auto& t = tiles.at(i);
    const double w = t.v->sizeMm.width() * t.drawScale, h = t.v->sizeMm.height() * t.drawScale;
    r.origins[i] = QPointF(colX[t.gridC] + (colW[t.gridC] - w) / 2.0,
                           rowY[t.gridR] + titleMm + (rowH[t.gridR] - h) / 2.0);
  }
  return r;
}
}  // namespace

DrawingViewDock::DrawingViewDock(QWidget *parent) : Dock(parent)
{
  auto *central = new QWidget(this);
  auto *layout = new QVBoxLayout(central);
  layout->setContentsMargins(4, 4, 4, 4);

  // Single canvas: the whole first-angle sheet with one shared fit + zoom/pan.
  // A WYSIWYG of the vector export. Its signals already carry the hit view index,
  // so they forward straight through to the dock's own signals.
  this->sheet = new DrawingSheetWidget(central);
  connect(this->sheet, &DrawingSheetWidget::anchorClicked, this,
          &DrawingViewDock::anchorClicked);
  connect(this->sheet, &DrawingSheetWidget::dimensionOffsetChanged, this,
          &DrawingViewDock::dimensionOffsetChanged);
  connect(this->sheet, &DrawingSheetWidget::dimensionAngleChanged, this,
          &DrawingViewDock::dimensionAngleChanged);
  connect(this->sheet, &DrawingSheetWidget::labelOffsetChanged, this,
          &DrawingViewDock::labelOffsetChanged);
  connect(this->sheet, &DrawingSheetWidget::dimensionDoubleClicked, this,
          &DrawingViewDock::dimensionDoubleClicked);
  connect(this->sheet, &DrawingSheetWidget::jumpToSource, this,
          &DrawingViewDock::jumpToSource);
  connect(this->sheet, &DrawingSheetWidget::detailRequested, this,
          &DrawingViewDock::detailRequested);

  // Row of per-view enable checkboxes. Toggling reprojects (viewSelectionChanged →
  // owner reprojects → setViews) and rebuilds the sheet around the ticked views.
  const char *viewNames[kViewCount] = {QT_TR_NOOP("Front"), QT_TR_NOOP("Back"),
                                       QT_TR_NOOP("Left"),  QT_TR_NOOP("Right"),
                                       QT_TR_NOOP("Top"),   QT_TR_NOOP("Bottom")};
  auto *viewsRow = new QWidget(central);
  auto *viewsLayout = new QHBoxLayout(viewsRow);
  viewsLayout->setContentsMargins(0, 0, 0, 0);
  viewsLayout->addWidget(new QLabel(tr("Views:"), viewsRow));
  for (int i = 0; i < kViewCount; ++i) {
    auto *cb = new QCheckBox(tr(viewNames[i]), viewsRow);
    cb->setChecked(kViewDefault[i]);
    connect(cb, &QCheckBox::toggled, this, [this](bool) {
      emit viewSelectionChanged();
      updateExportEnabled();
    });
    viewsLayout->addWidget(cb);
    this->viewChecks.append(cb);
  }
  viewsLayout->addStretch(1);

  this->placeholder = new QLabel(tr("Render (F5) to generate orthographic views."), central);
  this->placeholder->setAlignment(Qt::AlignCenter);

  this->showRefsCheck = new QCheckBox(tr("Reference points"), central);
  this->showRefsCheck->setChecked(true);
  this->showRefsCheck->setToolTip(
    tr("Show the dimref anchor dots and their names (dimension labels stay)."));

  this->separateFilesCheck = new QCheckBox(tr("Separate files"), central);
  this->separateFilesCheck->setChecked(false);
  this->separateFilesCheck->setToolTip(
    tr("Export each shown view to its own file (name_front.svg, …) instead of "
       "one composed sheet."));

  this->materializeButton = new QPushButton(tr("Materialize kvóty"), central);
  this->materializeButton->setToolTip(
    tr("Write a // @dim line for each kvota= pair, making them editable."));

  this->detailButton = new QPushButton(tr("New detail"), central);
  this->detailButton->setCheckable(true);
  this->detailButton->setToolTip(
    tr("Draw a circle over a view to add an enlarged detail (writes a // @detail line)."));

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
  buttonLayout->addWidget(this->detailButton);
  buttonLayout->addWidget(this->saveButton);

  auto *checkRow = new QWidget(central);
  auto *checkLayout = new QHBoxLayout(checkRow);
  checkLayout->setContentsMargins(0, 0, 0, 0);
  checkLayout->addWidget(this->showRefsCheck);
  checkLayout->addWidget(this->separateFilesCheck);
  checkLayout->addStretch(1);
  checkLayout->addWidget(this->scaleSpin);
  checkLayout->addWidget(this->exportButton);

  layout->addWidget(viewsRow);
  layout->addWidget(this->sheet, 1);
  layout->addWidget(this->placeholder);
  layout->addWidget(checkRow);
  layout->addWidget(buttonRow);
  setWidget(central);

  connect(this->saveButton, &QPushButton::clicked, this, &DrawingViewDock::saveSheet);
  connect(this->exportButton, &QPushButton::clicked, this, &DrawingViewDock::exportDrawing);
  connect(this->materializeButton, &QPushButton::clicked, this,
          &DrawingViewDock::materializeRequested);
  connect(this->detailButton, &QPushButton::toggled, this,
          [this](bool on) { if (this->sheet) this->sheet->setDetailMode(on); });
  // The gesture (or a cancel) turns the mode off; sync the button back.
  connect(this->sheet, &DrawingSheetWidget::detailModeFinished, this,
          [this] { this->detailButton->setChecked(false); });
  connect(this->showRefsCheck, &QCheckBox::toggled, this, &DrawingViewDock::setShowAnchors);
  // The scale changes the sheet layout (view sizes and spacing) but not the
  // geometry, so just recompute the layout — no reprojection / re-render.
  connect(this->scaleSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          [this](double) { rebuildSheet(); });
}

double DrawingViewDock::exportScale() const
{
  const double denom = this->scaleSpin->value();
  return denom > 1e-6 ? 1.0 / denom : 1.0;  // drawing-mm per model-mm
}

void DrawingViewDock::setShowAnchors(bool show)
{
  if (this->sheet) this->sheet->setShowAnchors(show);
}

DrawingViewDock::~DrawingViewDock() = default;

void DrawingViewDock::setViews(const QList<OrthoViewData>& newViews, const QStringList& titles)
{
  this->views = newViews;
  // Titles (parallel to the views) become the tile captions drawn on the sheet.
  for (int i = 0; i < this->views.size() && i < titles.size(); ++i)
    if (!titles.at(i).isEmpty()) this->views[i].title = titles.at(i);
  rebuildSheet();
  updateExportEnabled();
}

void DrawingViewDock::setSectionViews(const QList<OrthoViewData>& sections)
{
  this->sectionViews = sections;
  rebuildSheet();
  updateExportEnabled();
}

void DrawingViewDock::setDetailViews(const QList<OrthoViewData>& details)
{
  this->detailViews = details;
  rebuildSheet();
  updateExportEnabled();
}

void DrawingViewDock::rebuildSheet()
{
  if (!this->sheet) return;
  std::vector<bool> sel(kViewCount, false);
  for (int i = 0; i < kViewCount; ++i) sel[i] = isViewSelected(i);
  const double scale = exportScale();
  const double marginMm = 10.0, gapMm = 12.0, titleMm = 5.0;
  const QVector<SheetTile> tiles =
    collectSheetTiles(this->views, this->sectionViews, this->detailViews, sel, scale);
  const SheetLayoutResult lay = computeSheetLayout(tiles, marginMm, gapMm, titleMm);
  QVector<PlacedView> places;
  places.reserve(tiles.size());
  for (int i = 0; i < tiles.size(); ++i) {
    PlacedView pv;
    pv.data = *tiles.at(i).v;
    pv.originMm = lay.origins.at(i);
    pv.viewIndex = tiles.at(i).viewIndex;
    pv.isSection = tiles.at(i).isSection;
    pv.drawScale = tiles.at(i).drawScale;
    places.push_back(pv);
  }
  this->sheet->setSheet(places, lay.wMm, lay.hMm);
}

bool DrawingViewDock::isViewSelected(int viewIndex) const
{
  return viewIndex >= 0 && viewIndex < this->viewChecks.size() &&
         this->viewChecks.at(viewIndex)->isChecked();
}

void DrawingViewDock::updateExportEnabled()
{
  bool any = false;
  for (int i = 0; i < this->views.size(); ++i)
    if (isViewSelected(i) && this->views.at(i).valid()) any = true;
  for (const auto& s : this->sectionViews)
    if (s.valid()) any = true;
  for (const auto& d : this->detailViews)
    if (d.valid()) any = true;
  this->saveButton->setEnabled(any);
  this->exportButton->setEnabled(any);
  this->placeholder->setVisible(!any);
}

void DrawingViewDock::clearViews()
{
  setViews({}, {});
  setSectionViews({});
  setDetailViews({});
}

void DrawingViewDock::saveSheet()
{
  if (!this->sheet || !this->sheet->hasContent()) return;
  const QString filename = QFileDialog::getSaveFileName(
    this, tr("Save Drawing Sheet"), "drawing.png", tr("PNG image (*.png)"));
  if (filename.isEmpty()) return;
  // Grab the composed sheet as shown (single canvas → single raster).
  const QPixmap shot = this->sheet->grab();
  shot.save(filename, "PNG");
}

void DrawingViewDock::exportDrawing()
{
  bool any = false;
  for (int i = 0; i < this->views.size(); ++i)
    if (isViewSelected(i) && this->views.at(i).valid()) any = true;
  for (const auto& s : this->sectionViews)
    if (s.valid()) any = true;
  if (!any) return;

  const double scale = exportScale();  // drawing-mm per model-mm

  const QString filename = QFileDialog::getSaveFileName(
    this, tr("Export drawing"), "drawing.svg",
    tr("SVG (*.svg);;PDF (*.pdf);;DXF (*.dxf)"));
  if (filename.isEmpty()) return;

  const double dpi = 96.0;
  const double mmToPx = dpi / 25.4;
  const double marginMm = 10.0, gapMm = 12.0, titleMm = 5.0;
  const double fontMm = 3.0, edgeMm = 0.3, dimMm = 0.25, witMm = 0.15, arrowMm = 2.5;

  struct Placement { const OrthoViewData *v; double ox, oy; QString slug; double drawScale; };
  // Build the placed views' primitives (drawing-mm, y-down) and render to `path`.
  // Both the QPainter (SVG/PDF) and the DXF writer consume the same primitives, so
  // every format is truly WYSIWYG.
  auto renderToFile = [&](const QVector<Placement>& places, double sheetWmm, double sheetHmm,
                          const QString& path) {
    std::vector<QLineF> edgeSegs, hatchSegs, dimSegs, witSegs;
    std::vector<QPolygonF> arrows;
    std::vector<std::pair<QPointF, QString>> labels, titles;
    for (const auto& pl : places) {
      const auto& v = *pl.v;
      const double oxmm = pl.ox, oymm = pl.oy;
      const double sc = pl.drawScale > 1e-9 ? pl.drawScale : 1.0;  // this tile's scale
      auto map = [&](const QPointF& mp) {
      return QPointF(oxmm + mp.x() * sc, oymm + mp.y() * sc);  // -> drawing-mm
    };
    titles.emplace_back(QPointF(oxmm, oymm - 1.5), v.title);
    for (const auto& hh : v.hatch) hatchSegs.emplace_back(map(hh.p1()), map(hh.p2()));
    for (const auto& e : v.edges) edgeSegs.emplace_back(map(e.p1()), map(e.p2()));
    // Detail circles on this (parent) view: a 48-gon polyline + the letter.
    for (const auto& dm : v.detailMarks) {
      QPointF prev;
      for (int k = 0; k <= 48; ++k) {
        const double a = k * (2.0 * M_PI / 48.0);
        const QPointF p = map(dm.center + QPointF(std::cos(a) * dm.r, std::sin(a) * dm.r));
        if (k > 0) edgeSegs.emplace_back(prev, p);
        prev = p;
      }
      titles.emplace_back(map(dm.center + QPointF(dm.r * 0.72, -dm.r * 0.72)), dm.label);
    }

    auto addArrow = [&](const QPointF& tip, const QPointF& along, const QPointF& nrm) {
      QPolygonF h;
      h << tip << (tip + along * arrowMm + nrm * arrowMm * 0.4)
        << (tip + along * arrowMm - nrm * arrowMm * 0.4);
      arrows.push_back(h);
    };
    // Leader from a dragged-away label to the nearest point of its dimension line
    // (per-dimension: only when d.leader is on).
    auto addLeader = [&](const QPointF& lp, const QPointF& s0, const QPointF& s1, bool leader) {
      if (!leader) return;
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
        addLeader(labelPos, a2, b2, d.leader);
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
      addLeader(labelPos, a2, b2, d.leader);
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
    for (const auto& sl : v.sectionLines) {
      const QPointF a = map(sl.p0), b = map(sl.p1);
      dimSegs.emplace_back(a, b);  // chain style is lost in export; drawn solid
      QPointF ad = sl.arrowDir;
      const double adl = std::hypot(ad.x(), ad.y());
      if (adl < 1e-9) continue;
      ad /= adl;
      const QPointF nrm(-ad.y(), ad.x());
      for (const QPointF& e : {a, b}) {
        dimSegs.emplace_back(e, e + ad * arrowMm);
        addArrow(e + ad * arrowMm, -ad, nrm);
        titles.emplace_back(e + ad * (arrowMm * 1.6), sl.label);  // black letter
      }
    }
  }

  if (path.endsWith(".dxf", Qt::CaseInsensitive)) {
    std::ofstream out(path.toStdString());
    if (!out.is_open()) return;
    auto Y = [&](double y) { return sheetHmm - y; };  // DXF is y-up
    auto line = [&](const QPointF& a, const QPointF& b) {
      out << "0\nLINE\n8\n0\n10\n" << a.x() << "\n20\n" << Y(a.y()) << "\n30\n0\n11\n"
          << b.x() << "\n21\n" << Y(b.y()) << "\n31\n0\n";
    };
    out << "0\nSECTION\n2\nENTITIES\n";
    for (const auto& hh : hatchSegs) line(hh.p1(), hh.p2());
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

    QPen hatchPen(QColor(90, 90, 90));
    hatchPen.setWidthF(0.13 * mmToPx);
    p.setPen(hatchPen);
    for (const auto& hh : hatchSegs) p.drawLine(P(hh.p1()), P(hh.p2()));

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

  if (path.endsWith(".pdf", Qt::CaseInsensitive)) {
      QPdfWriter pdf(path);
      pdf.setResolution(static_cast<int>(dpi));
      pdf.setPageSize(QPageSize(QSizeF(sheetWmm, sheetHmm), QPageSize::Millimeter));
      pdf.setPageMargins(QMarginsF(0, 0, 0, 0));
      QPainter p(&pdf);
      paintSheet(p);
    } else {
      QSvgGenerator svg;
      svg.setFileName(path);
      svg.setResolution(static_cast<int>(dpi));
      svg.setSize(QSize(sheetWpx, sheetHpx));
      svg.setViewBox(QRect(0, 0, sheetWpx, sheetHpx));
      QPainter p(&svg);
      paintSheet(p);
    }
  };  // renderToFile

  // Tiles to export: the selected standard views at their first-angle cells plus
  // the valid section and detail tiles — the very same layout the canvas uses.
  std::vector<bool> sel(kViewCount, false);
  for (int i = 0; i < kViewCount; ++i) sel[i] = isViewSelected(i);
  const QVector<SheetTile> tiles =
    collectSheetTiles(this->views, this->sectionViews, this->detailViews, sel, scale);
  if (tiles.isEmpty()) return;

  if (this->separateFilesCheck->isChecked()) {
    // One file per tile (name_slug.ext), each a single-view sheet.
    for (const auto& t : tiles) {
      const double w = t.v->sizeMm.width() * t.drawScale, h = t.v->sizeMm.height() * t.drawScale;
      QString path = filename;
      const int dot = path.lastIndexOf('.');
      const QString slug = "_" + t.slug;
      if (dot >= 0) path.insert(dot, slug); else path += slug;
      renderToFile({{t.v, marginMm, marginMm + titleMm, t.slug, t.drawScale}}, w + 2 * marginMm,
                   h + titleMm + 2 * marginMm, path);
    }
  } else {
    // Composed first-angle sheet, laid out by the shared engine.
    const SheetLayoutResult lay = computeSheetLayout(tiles, marginMm, gapMm, titleMm);
    QVector<Placement> places;
    for (int i = 0; i < tiles.size(); ++i)
      places.push_back({tiles.at(i).v, lay.origins.at(i).x(), lay.origins.at(i).y(),
                        tiles.at(i).slug, tiles.at(i).drawScale});
    renderToFile(places, lay.wMm, lay.hMm, filename);
  }
}
