#include "gui/DrawingViewDock.h"

#include "gui/FlowLayout.h"
#include "utils/printutils.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <utility>
#include <vector>

#include <QFileDialog>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFont>
#include <QFontMetricsF>
#include <QGridLayout>
#include <QGroupBox>
#include <QMarginsF>
#include <QPageSize>
#include <QPdfWriter>
#include <QSvgGenerator>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QImage>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPolygonF>
#include <QPushButton>
#include <QSet>
#include <QTimer>
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
  // Fires ~150 ms after the last wheel step: leaves the responsive scaled-blit mode
  // and repaints, which rebuilds the per-tile pixmaps crisply at the new zoom.
  this->zoomSettleTimer = new QTimer(this);
  this->zoomSettleTimer->setSingleShot(true);
  connect(this->zoomSettleTimer, &QTimer::timeout, this, [this] {
    this->zooming = false;
    update();
  });
}

void DrawingSheetWidget::setSheet(const QVector<PlacedView>& placements, double wMm, double hMm)
{
  this->places = placements;
  this->sheetWmm = wMm;
  this->sheetHmm = hMm;
  this->dragPlace = -1;
  this->draggingDim = -1;
  this->draggingLabel = -1;
  this->draggingText = -1;
  this->tileDrag = -1;
  // Prune cached layers for tiles no longer on the sheet (toggled-off views); the
  // survivors are reused as long as geomVersion / scale match. Kept across setSheet
  // on purpose — that is what makes a dimension edit cheap.
  QSet<QString> live;
  for (const auto& pl : this->places) {
    const QString id = tileId(pl);
    if (!id.isEmpty()) live.insert(id);
  }
  for (auto it = this->tileLayers.begin(); it != this->tileLayers.end();)
    it = live.contains(it.key()) ? std::next(it) : this->tileLayers.erase(it);
  update();
}

void DrawingSheetWidget::setGeometryVersion(int version)
{
  if (version == this->geomVersion) return;
  this->geomVersion = version;
  this->tileLayers.clear();  // mesh changed: every cached pixmap is stale
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
  this->tileLayers.clear();
  this->sheetWmm = this->sheetHmm = 0.0;
  this->userZoom = 1.0;
  this->userPan = QPointF();
  this->dragPlace = this->draggingDim = this->draggingLabel = this->draggingText = -1;
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

QTransform DrawingSheetWidget::tileTransform(const PlacedView& pl) const
{
  // widget = (originMm + local*drawScale) * s + fitOffset + userPan, with
  // s = fitScale*userZoom — i.e. a uniform scale k plus a translation.
  const double s = this->fitScale * this->userZoom;
  const double sc = pl.drawScale > 1e-9 ? pl.drawScale : 1.0;
  const double k = sc * s;
  return QTransform(k, 0, 0, k,
                    pl.originMm.x() * s + this->fitOffset.x() + this->userPan.x(),
                    pl.originMm.y() * s + this->fitOffset.y() + this->userPan.y());
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
  // Antialiasing costs real time on edge-heavy sheets, so drop it while a drag is
  // in flight and put it back on the frame after the mouse is released. Smooth pixmap
  // transform keeps the scaled-blit zoom phase from looking blocky.
  painter.setRenderHint(QPainter::Antialiasing, !interacting());
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
  // Rasterising the tile pixmaps is the one heavy step; cap how much of it runs per
  // frame so a full rebuild (F5 / zoom-settle / view toggle) never freezes input.
  // Tiles past the budget keep this frame short and are finished on the follow-up.
  this->paintBudget.restart();
  this->deferredRebuild = false;
  for (const auto& pl : this->places) paintPlacement(painter, pl);
  // More pixmaps still to build: come back and continue next event-loop turn, so
  // mouse events queued in between are handled (dragging stays live during a rebuild).
  if (this->deferredRebuild) QTimer::singleShot(0, this, [this] { update(); });

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

QString DrawingSheetWidget::tileId(const PlacedView& pl)
{
  if (pl.viewIndex >= 0) return QStringLiteral("v%1").arg(pl.viewIndex);
  if (!pl.data.placeKey.isEmpty()) return "k:" + pl.data.placeKey;
  return QString();  // unnamed section etc. — not cacheable, direct-drawn
}

void DrawingSheetWidget::renderTileGeometry(QPainter& painter, const PlacedView& pl, bool cull,
                                            const QRectF& visLocal) const
{
  // Runs with `painter` already transformed to this tile's view-local mm; pens are
  // cosmetic so their widths stay in device px under that transform. `cull` clips to
  // visLocal (the direct-draw path when zoomed in); the pixmap-build path passes the
  // whole tile (cull=false), so all segments go to QPainter in one batch, no copy.
  const OrthoViewData& data = pl.data;
  auto drawSegs = [&](const QList<QLineF>& segs) {
    if (segs.isEmpty()) return;
    if (!cull) {
      painter.drawLines(segs.constData(), segs.size());
      return;
    }
    QVector<QLineF> keep;
    keep.reserve(segs.size());
    for (const auto& e : segs) {
      if (std::max(e.x1(), e.x2()) < visLocal.left() ||
          std::min(e.x1(), e.x2()) > visLocal.right() ||
          std::max(e.y1(), e.y2()) < visLocal.top() ||
          std::min(e.y1(), e.y2()) > visLocal.bottom())
        continue;
      keep.append(e);
    }
    if (!keep.isEmpty()) painter.drawLines(keep.constData(), keep.size());
  };

  // Coloured face fills (fill-colors mode), painted first so edges sit on top.
  if (!data.fills.isEmpty()) {
    painter.setPen(Qt::NoPen);
    for (const auto& f : data.fills) {
      if (cull && !visLocal.intersects(f.poly.boundingRect())) continue;
      painter.setBrush(f.color);
      painter.drawPolygon(f.poly);
    }
  }

  // Section cut-face hatching next, so edges sit on top of it.
  if (!data.hatch.isEmpty()) {
    QPen hatchPen(QColor(90, 90, 90));
    hatchPen.setWidthF(std::max(0.4, absPx(0.13)));
    hatchPen.setCosmetic(true);
    painter.setPen(hatchPen);
    painter.setBrush(Qt::NoBrush);
    drawSegs(data.hatch);
  }

  // The line drawing itself: silhouette + feature edges.
  QPen edgePen(Qt::black);
  edgePen.setWidthF(std::max(0.6, absPx(0.3)));
  edgePen.setCapStyle(Qt::RoundCap);
  edgePen.setCosmetic(true);
  painter.setPen(edgePen);
  drawSegs(data.edges);
}

bool DrawingSheetWidget::blitTileGeometry(QPainter& painter, const PlacedView& pl, double k)
{
  const OrthoViewData& data = pl.data;
  const QString id = tileId(pl);
  const QSizeF sz = data.sizeMm;
  // A tile's pixmap at scale k is sz*k device-independent px. Beyond a cap it would
  // be huge (deep zoom on a big tile) — fall back to direct-draw there; culling keeps
  // that bounded, and you don't reposition tiles at deep zoom anyway.
  const double capPx = 6000.0;
  const bool tooBig = sz.width() * k > capPx || sz.height() * k > capPx;
  if (id.isEmpty() || tooBig) return false;

  TileLayer& L = this->tileLayers[id];
  const bool stale = L.px.isNull() || L.builtVersion != this->geomVersion ||
                     (!this->zooming && std::abs(L.builtScale - k) > 1e-9);
  if (stale) {
    // Build only while this frame's rasterisation budget lasts; otherwise defer to
    // the follow-up so the frame stays short. A tile with an existing (stale-scale or
    // stale-version) pixmap still blits it meanwhile — deferral only shows blank for a
    // brand-new tile (geometry change clears the cache, so those fill in over a few
    // frames instead of freezing).
    constexpr qint64 kBuildBudgetMs = 8;
    const bool budgetLeft =
      !this->paintBudget.isValid() || this->paintBudget.elapsed() < kBuildBudgetMs;
    if (!budgetLeft) {
      this->deferredRebuild = true;
      if (L.px.isNull()) return true;  // nothing to show yet — skip geometry this frame
    } else {
      const double dpr = devicePixelRatioF();
      const int wpx = std::max(1, static_cast<int>(std::ceil(sz.width() * k * dpr)));
      const int hpx = std::max(1, static_cast<int>(std::ceil(sz.height() * k * dpr)));
      QPixmap px(wpx, hpx);
      px.setDevicePixelRatio(dpr);
      px.fill(Qt::transparent);
      QPainter pp(&px);
      pp.setRenderHint(QPainter::Antialiasing, true);  // cached once; keep it crisp
      pp.setTransform(QTransform(k, 0, 0, k, 0, 0), /*combine=*/false);  // tile-mm -> pixmap px
      renderTileGeometry(pp, pl, /*cull=*/false, QRectF());
      pp.end();
      L.px = px;
      L.builtScale = k;
      L.builtVersion = this->geomVersion;
    }
  }

  // Blit at the tile's widget origin. If the current scale differs from the built one
  // (a zoom in flight, or a deferred rebuild), scale the pixmap about that origin so
  // it tracks until rebuilt.
  const QPointF origin = mapToWidget(pl, QPointF(0, 0));
  const double ratio = L.builtScale > 1e-9 ? k / L.builtScale : 1.0;
  if (std::abs(ratio - 1.0) < 1e-6) {
    painter.drawPixmap(origin, L.px);
  } else {
    painter.save();
    painter.translate(origin);
    painter.scale(ratio, ratio);
    painter.drawPixmap(QPointF(0, 0), L.px);
    painter.restore();
  }
  return true;
}

void DrawingSheetWidget::paintPlacement(QPainter& painter, const PlacedView& pl)
{
  const OrthoViewData& data = pl.data;
  if (!data.valid()) return;

  // Geometry: from the per-tile pixmap cache when possible, else direct-draw.
  const double k = this->fitScale * this->userZoom * (pl.drawScale > 1e-9 ? pl.drawScale : 1.0);
  if (!blitTileGeometry(painter, pl, k)) {
    painter.save();
    const QTransform T = tileTransform(pl);
    painter.setTransform(T, /*combine=*/false);
    bool ok = false;
    const QTransform inv = T.inverted(&ok);
    const QRectF visLocal = ok ? inv.mapRect(QRectF(rect())) : QRectF();
    const QRectF tileBox(0, 0, data.sizeMm.width(), data.sizeMm.height());
    renderTileGeometry(painter, pl, /*cull=*/ok && !visLocal.contains(tileBox), visLocal);
    painter.restore();
  }

  paintTileOverlay(painter, pl);
}

void DrawingSheetWidget::paintTileOverlay(QPainter& painter, const PlacedView& pl)
{
  const OrthoViewData& data = pl.data;
  auto map = [&](const QPointF& p) { return mapToWidget(pl, p); };

  painter.setFont(overlayFont());

  // View title above the tile, standing off the geometry by ~one font height
  // (3 drawing-mm) so it doesn't crowd the image.
  if (!data.title.isEmpty()) {
    painter.setPen(Qt::black);
    painter.drawText(map(QPointF(0, 0)) - QPointF(0, absPx(3.0)), data.title);
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
    // The box sits where the label was dragged to — deliberately NOT clamped to the
    // widget: on the single sheet canvas the label has to pan/zoom away with its
    // view, and clamping also made a drag near the edge snap back on refresh (the
    // unclamped offset is what gets written to the source). The vector export has
    // never clamped, so this keeps the dock WYSIWYG.
    QRectF box(center.x() - tr.width() / 2.0, center.y() - tr.height() / 2.0,
               tr.width(), tr.height());
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
               tr.width(), tr.height());  // not viewport-clamped, see drawLabel above
    painter.fillRect(box, QColor(255, 255, 255, 220));
    painter.setPen(QColor(150, 30, 30));
    painter.drawText(box, Qt::AlignCenter, ang.label);
  }

  // Text labels (// @label): a dot at the projected 3D anchor, a leader from the
  // text box to it, and the text (black — an annotation, not a dimension value).
  for (const auto& lb : data.labels) {
    const QPointF anc = map(lb.anchor);
    const QPointF center = map(lb.anchor + lb.off);
    const QRectF tr = painter.fontMetrics().boundingRect(lb.text).adjusted(-3, -2, 3, 2);
    QRectF box(center.x() - tr.width() / 2.0, center.y() - tr.height() / 2.0,
               tr.width(), tr.height());  // not viewport-clamped, see drawLabel above
    // Leader from the text box centre to the anchor (drawn first, then the white
    // box backing hides the portion inside the box).
    if (lb.leader &&
        std::hypot(box.center().x() - anc.x(), box.center().y() - anc.y()) > box.height()) {
      painter.setPen(witnessPen);
      painter.drawLine(box.center(), anc);
    }
    const double dotR = std::max(1.6, absPx(0.5));
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(20, 20, 20));
    painter.drawEllipse(anc, dotR, dotR);
    painter.fillRect(box, QColor(255, 255, 255, 220));
    painter.setPen(Qt::black);
    painter.drawText(box, Qt::AlignCenter, lb.text);
  }

  // Anchor dots with names (reference points) — optional. Includes label() markers
  // (shown as clickable dots for the "new label" gesture) in a distinct colour.
  if (this->showAnchors) {
    for (const auto& anchor : data.anchors) {
      const QPointF p = map(anchor.pos);
      painter.setPen(Qt::NoPen);
      painter.setBrush(QColor(30, 90, 200));
      painter.drawEllipse(p, 3.5, 3.5);
      painter.setPen(QColor(30, 90, 200));
      painter.drawText(p + QPointF(5, -4), anchor.name);
    }
    for (const auto& anchor : data.labelAnchors) {
      const QPointF p = map(anchor.pos);
      painter.setPen(Qt::NoPen);
      painter.setBrush(QColor(30, 160, 90));
      painter.drawEllipse(p, 3.5, 3.5);
      painter.setPen(QColor(20, 110, 60));
      painter.drawText(p + QPointF(5, -4), anchor.name);
    }
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

  // Right button: jump to the source of the dimension / dimref under the cursor.
  if (event->button() == Qt::RightButton) {
    int hp, hd; bool onLabel;
    if (hitDim(click, /*editableOnly=*/false, hp, hd, onLabel) &&
        this->places.at(hp).data.dims.at(hd).jumpLine >= 0) {
      emit jumpToSource(this->places.at(hp).data.dims.at(hd).jumpLine);
      return;
    }
    int lp, ll;
    if (hitLabel(click, lp, ll) && this->places.at(lp).data.labels.at(ll).id >= 0) {
      emit jumpToSource(this->places.at(lp).data.labels.at(ll).id);
      return;
    }
    if (pi >= 0) {  // else the nearest dimref anchor in the tile under the cursor
      const PlacedView& pl = this->places.at(pi);
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

  // Dimension / label drag first, searched across ALL views (a dimension line or a
  // dragged-away label often sits outside its tile's box; gating by the tile under
  // the cursor would miss it and the click would pan instead).
  {
    int hp, hd; bool onLabel;
    if (hitDim(click, /*editableOnly=*/true, hp, hd, onLabel)) {
      this->dragPlace = hp;
      if (onLabel) this->draggingLabel = hd; else this->draggingDim = hd;
      return;
    }
    int lp, ll;
    if (hitLabel(click, lp, ll)) {
      this->dragPlace = lp;
      this->draggingText = ll;
      return;
    }
  }

  // Anchor click (create dimensions) — in the tile under the cursor; anchors are
  // feature points, always inside their tile. This must precede the movable-tile
  // drag below: a section tile is `movable`, so a click on one of its anchor dots
  // would otherwise start a tile drag and never create a dimension. Anchors have a
  // small hit radius, so a click that misses them still falls through to the drag.
  if (pi >= 0 && !this->places.at(pi).data.projKey.isEmpty() && this->showAnchors) {
    const PlacedView& pl = this->places.at(pi);
    const double radius = 12.0;
    QStringList hits;  // distinct anchor refs within the hit radius
    for (const auto& anchor : pl.data.anchors) {
      const QPointF p = mapToWidget(pl, anchor.pos);
      if (std::hypot(p.x() - click.x(), p.y() - click.y()) <= radius &&
          !hits.contains(anchor.ref)) {
        hits << anchor.ref;
      }
    }
    if (hits.size() == 1) { emit anchorClicked(pl.data.projKey, hits.first()); return; }
    if (hits.size() > 1) {
      QMenu menu(this);
      for (const QString& ref : hits) menu.addAction(ref);
      if (QAction *chosen = menu.exec(event->globalPosition().toPoint()))
        emit anchorClicked(pl.data.projKey, chosen->text());
      return;
    }
  }

  // Label marker click (create a // @label) — in the tile under the cursor.
  if (pi >= 0 && !this->places.at(pi).data.projKey.isEmpty() && this->showAnchors) {
    const PlacedView& pl = this->places.at(pi);
    for (const auto& a : pl.data.labelAnchors) {
      const QPointF p = mapToWidget(pl, a.pos);
      if (std::hypot(p.x() - click.x(), p.y() - click.y()) <= 12.0) {
        // Copy the reference out: the slot refreshes the sheet, which replaces
        // `places` while the signal is still on the stack.
        const QString projKey = pl.data.projKey;
        const QString ref = a.ref;
        emit labelAnchorClicked(projKey, ref);
        return;
      }
    }
  }

  // Left-drag a movable section / detail tile to reposition it in free space. After
  // the anchor / label-marker checks above, so clicking a dot in a section creates a
  // dimension rather than dragging the tile.
  if (pi >= 0 && this->places.at(pi).data.movable) {
    this->tileDrag = pi;
    this->tileGrabMm = widgetToSheet(click) - this->places.at(pi).originMm;
    setCursor(Qt::ClosedHandCursor);
    return;
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

  // Dragging a movable tile: move its origin to follow the cursor (sheet-mm).
  if (this->tileDrag >= 0 && this->tileDrag < this->places.size()) {
    this->places[this->tileDrag].originMm = widgetToSheet(event->position()) - this->tileGrabMm;
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

  // // @label text drag: the offset is simply the text position relative to the
  // projected anchor, in the view's local model-mm (y-down).
  if (this->draggingText >= 0 && this->draggingText < pl.data.labels.size()) {
    OrthoLabel& lb = pl.data.labels[this->draggingText];
    lb.off = mapToViewLocal(pl, event->position()) - lb.anchor;
    update();
    emit labelTextMoved(pl.viewIndex, lb.id, lb.off.x(), lb.off.y(), false);
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

bool DrawingSheetWidget::hitDim(const QPointF& pt, bool editableOnly, int& place, int& dim,
                                bool& onLabel) const
{
  const QFontMetrics fm(overlayFont());
  // Label boxes first, across all standard views.
  for (int p = 0; p < this->places.size(); ++p) {
    const PlacedView& pl = this->places.at(p);
    if (pl.viewIndex < 0 && !pl.data.annotated) continue;  // detail tiles: display-only
    for (int i = 0; i < pl.data.dims.size(); ++i) {
      const auto& d = pl.data.dims.at(i);
      if (editableOnly && d.id < 0) continue;
      QRectF box = fm.boundingRect(d.label).adjusted(-3, -2, 3, 2);
      box.moveCenter(mapToWidget(pl, labelPosOf(d)));
      if (box.contains(pt)) { place = p; dim = i; onLabel = true; return true; }
    }
  }
  // Then the nearest dimension line within the hit threshold.
  double best = 8.0;
  int bp = -1, bd = -1;
  for (int p = 0; p < this->places.size(); ++p) {
    const PlacedView& pl = this->places.at(p);
    if (pl.viewIndex < 0 && !pl.data.annotated) continue;  // detail tiles: display-only
    for (int i = 0; i < pl.data.dims.size(); ++i) {
      const auto& d = pl.data.dims.at(i);
      if (editableOnly && d.id < 0) continue;
      QPointF p0, p1;
      dimLineEnds(d, p0, p1);
      const double dist = distToSegment(pt, mapToWidget(pl, p0), mapToWidget(pl, p1));
      if (dist < best) { best = dist; bp = p; bd = i; }
    }
  }
  if (bp >= 0) { place = bp; dim = bd; onLabel = false; return true; }
  return false;
}

bool DrawingSheetWidget::hitLabel(const QPointF& pt, int& place, int& label) const
{
  const QFontMetrics fm(overlayFont());
  for (int p = 0; p < this->places.size(); ++p) {
    const PlacedView& pl = this->places.at(p);
    if (pl.viewIndex < 0 && !pl.data.annotated) continue;  // detail tiles: display-only
    for (int i = 0; i < pl.data.labels.size(); ++i) {
      const auto& lb = pl.data.labels.at(i);
      QRectF box = fm.boundingRect(lb.text).adjusted(-3, -2, 3, 2);
      box.moveCenter(mapToWidget(pl, lb.anchor + lb.off));
      if (box.contains(pt)) { place = p; label = i; return true; }
    }
  }
  return false;
}

void DrawingSheetWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
  int lp, ll;
  if (hitLabel(event->position(), lp, ll) && this->places.at(lp).data.labels.at(ll).id >= 0) {
    emit labelDoubleClicked(this->places.at(lp).data.labels.at(ll).id);
    return;
  }
  int hp, hd; bool onLabel;
  if (hitDim(event->position(), /*editableOnly=*/true, hp, hd, onLabel))
    emit dimensionDoubleClicked(this->places.at(hp).data.dims.at(hd).id);
}

void DrawingSheetWidget::mouseReleaseEvent(QMouseEvent *)
{
  // Finish a "new detail" gesture: emit the circle in the parent view's local mm.
  if (this->detailDragging) {
    const int pi = this->detailPlace;
    if (pi >= 0 && pi < this->places.size() &&
        !this->places.at(pi).data.projKey.isEmpty()) {
      const PlacedView& pl = this->places.at(pi);
      const double rpx = std::hypot(this->detailCur.x() - this->detailCenter.x(),
                                    this->detailCur.y() - this->detailCenter.y());
      if (rpx > 4.0) {
        const QPointF c = mapToViewLocal(pl, this->detailCenter);
        const QPointF rimPt = mapToViewLocal(pl, this->detailCur);
        const double r = std::hypot(rimPt.x() - c.x(), rimPt.y() - c.y());
        emit detailRequested(pl.data.projKey, c.x(), c.y(), r);
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

  // Finish a movable-tile drag: persist its new origin via placeKey.
  if (this->tileDrag >= 0) {
    if (this->tileDrag < this->places.size()) {
      const PlacedView& pl = this->places.at(this->tileDrag);
      emit tilePlaced(pl.data.placeKey, pl.originMm.x(), pl.originMm.y());
    }
    this->tileDrag = -1;
    setCursor(Qt::ArrowCursor);
    update();  // interacting() just went false: repaint antialiased
    return;
  }

  if (this->panning) {
    this->panning = false;
    update();
    return;
  }
  if (this->dragPlace < 0 || this->dragPlace >= this->places.size()) {
    this->draggingLabel = this->draggingDim = this->draggingText = -1;
    update();
    return;
  }
  PlacedView& pl = this->places[this->dragPlace];
  if (this->draggingText >= 0 && this->draggingText < pl.data.labels.size()) {
    const OrthoLabel& lb = pl.data.labels[this->draggingText];
    emit labelTextMoved(pl.viewIndex, lb.id, lb.off.x(), lb.off.y(), true);
  } else if (this->draggingLabel >= 0 && this->draggingLabel < pl.data.dims.size()) {
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
  this->dragPlace = this->draggingLabel = this->draggingDim = this->draggingText = -1;
  update();  // interacting() just went false: repaint antialiased
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
  // Two-phase zoom: paint blits the existing pixmaps scaled (responsive but soft) and
  // the pixmaps are rebuilt at the new scale only once the wheel settles.
  this->zooming = true;
  this->zoomSettleTimer->start(150);
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
  // Auto-grid sizing uses only the auto-placed tiles; tiles with an explicit at=
  // (hasAt) float freely and are excluded from the row/column extents.
  for (const auto& t : tiles) if (!t.v->hasAt) maxR = std::max(maxR, t.gridR);
  std::vector<double> colW(4, 0.0), rowH(maxR + 1, 0.0), colX(4, 0.0), rowY(maxR + 1, 0.0);
  for (const auto& t : tiles) {
    if (t.v->hasAt) continue;
    colW[t.gridC] = std::max(colW[t.gridC], t.v->sizeMm.width() * t.drawScale);
    rowH[t.gridR] = std::max(rowH[t.gridR], t.v->sizeMm.height() * t.drawScale);
  }
  double x = marginMm, y = marginMm;
  for (int c = 0; c < 4; ++c) { colX[c] = x; if (colW[c] > 0) x += colW[c] + gapMm; }
  // Rows 0..2 are the standard-view block; rows >=3 are section / detail tiles.
  // Add extra breathing room before the extras block so the sheet reads in groups.
  const double blockGapMm = gapMm * 1.6;
  for (int rr = 0; rr <= maxR; ++rr) {
    if (rr == 3) y += blockGapMm;  // separate the sections/details from the views
    rowY[rr] = y;
    if (rowH[rr] > 0) y += titleMm + rowH[rr] + gapMm;
  }
  double wMm = std::max(2 * marginMm, x - gapMm + marginMm);
  double hMm = std::max(2 * marginMm, y - gapMm + marginMm);
  for (int i = 0; i < tiles.size(); ++i) {
    const auto& t = tiles.at(i);
    const double w = t.v->sizeMm.width() * t.drawScale, h = t.v->sizeMm.height() * t.drawScale;
    if (t.v->hasAt) {  // floating tile: origin straight from source, grow the sheet
      r.origins[i] = t.v->atMm;
      wMm = std::max(wMm, t.v->atMm.x() + w + marginMm);
      hMm = std::max(hMm, t.v->atMm.y() + h + marginMm);
    } else {
      r.origins[i] = QPointF(colX[t.gridC] + (colW[t.gridC] - w) / 2.0,
                             rowY[t.gridR] + titleMm + (rowH[t.gridR] - h) / 2.0);
    }
  }
  r.wMm = wMm;
  r.hMm = hMm;
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
  // Queued: creating the label rewrites the source and refreshes the sheet, which
  // must not run inside the widget's mouse handler (it replaces the very data the
  // handler is walking).
  connect(this->sheet, &DrawingSheetWidget::labelAnchorClicked, this,
          &DrawingViewDock::labelAnchorClicked, Qt::QueuedConnection);
  connect(this->sheet, &DrawingSheetWidget::labelTextMoved, this,
          &DrawingViewDock::labelTextMoved);
  connect(this->sheet, &DrawingSheetWidget::labelDoubleClicked, this,
          &DrawingViewDock::labelDoubleClicked);
  connect(this->sheet, &DrawingSheetWidget::dimensionDoubleClicked, this,
          &DrawingViewDock::dimensionDoubleClicked);
  connect(this->sheet, &DrawingSheetWidget::jumpToSource, this,
          &DrawingViewDock::jumpToSource);
  connect(this->sheet, &DrawingSheetWidget::detailRequested, this,
          &DrawingViewDock::detailRequested);
  connect(this->sheet, &DrawingSheetWidget::tilePlaced, this,
          &DrawingViewDock::tilePlaced);

  // Row of per-view enable checkboxes. Toggling reprojects (viewSelectionChanged →
  // owner reprojects → setViews) and rebuilds the sheet around the ticked views.
  const char *viewNames[kViewCount] = {QT_TR_NOOP("Front"), QT_TR_NOOP("Back"),
                                       QT_TR_NOOP("Left"),  QT_TR_NOOP("Right"),
                                       QT_TR_NOOP("Top"),   QT_TR_NOOP("Bottom")};
  // FlowLayout, not QHBoxLayout: a horizontal row's minimum width is the sum of
  // all its controls (~620px here), which set the dock's minimum width. Wrapping
  // brings that down to the widest single control.
  auto *viewsRow = new QWidget(central);
  auto *viewsLayout = new FlowLayout(viewsRow, 0, -1, 2);
  viewsLayout->addWidget(new QLabel(tr("Views:"), viewsRow));
  for (int i = 0; i < kViewCount; ++i) {
    auto *cb = new QCheckBox(tr(viewNames[i]), viewsRow);
    cb->setChecked(kViewDefault[i]);
    connect(cb, &QCheckBox::toggled, this, [this](bool) {
      emit sheetSettingsChanged();
      updateExportEnabled();
    });
    viewsLayout->addWidget(cb);
    this->viewChecks.append(cb);
  }

  // Inter-view spacing (mm) + an "auto" button that clears the dimensions. Both live
  // on the views row; the value is persisted in // @sheet as gap=.
  viewsLayout->addWidget(new QLabel(tr("Gap:"), viewsRow));
  this->gapSpin = new QDoubleSpinBox(viewsRow);
  this->gapSpin->setRange(0.0, 200.0);
  this->gapSpin->setDecimals(1);
  this->gapSpin->setSingleStep(1.0);
  this->gapSpin->setValue(16.0);
  this->gapSpin->setSuffix(tr(" mm"));
  this->gapSpin->setToolTip(tr("Spacing between the standard views on the sheet."));
  connect(this->gapSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          [this](double) { emit sheetSettingsChanged(); });
  viewsLayout->addWidget(this->gapSpin);
  this->autoGapButton = new QPushButton(tr("auto"), viewsRow);
  this->autoGapButton->setToolTip(
    tr("Set the gap so the dimensions of adjacent views don't collide."));
  connect(this->autoGapButton, &QPushButton::clicked, this, [this] {
    const double g = computeAutoGap();
    QSignalBlocker block(this->gapSpin);
    this->gapSpin->setValue(g);
    emit sheetSettingsChanged();
  });
  viewsLayout->addWidget(this->autoGapButton);

  this->placeholder = new QLabel(tr("Render (F5) to generate orthographic views."), central);
  this->placeholder->setAlignment(Qt::AlignCenter);
  // Wraps rather than forcing the dock as wide as the sentence.
  this->placeholder->setWordWrap(true);

  this->showRefsCheck = new QCheckBox(tr("Reference points"), central);
  this->showRefsCheck->setChecked(true);
  this->showRefsCheck->setToolTip(
    tr("Show the dimref anchor dots and their names (dimension labels stay)."));

  this->separateFilesCheck = new QCheckBox(tr("Separate files"), central);
  this->separateFilesCheck->setChecked(false);
  this->separateFilesCheck->setToolTip(
    tr("Export each shown view to its own file (name_front.svg, …) instead of "
       "one composed sheet."));

  this->fillColorsCheck = new QCheckBox(tr("Fill colors"), central);
  this->fillColorsCheck->setChecked(false);
  this->fillColorsCheck->setToolTip(
    tr("Fill the views with the model's color() colors (edges stay black). "
       "Shown live and carried into the SVG/PDF export."));

  this->modifiersCheck = new QCheckBox(tr("Slicer modifiers"), central);
  this->modifiersCheck->setChecked(true);
  this->modifiersCheck->setToolTip(
    tr("Fill the part of the model inside slicer_modifier() regions with the region "
       "color (visible with Fill colors on)."));

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

  // Both control rows wrap too. They stay two separate rows (rather than one big
  // flow) so the dock's *preferred* width is unchanged — only its minimum drops.
  auto *buttonRow = new QWidget(central);
  auto *buttonLayout = new FlowLayout(buttonRow, 0, -1, 2);
  buttonLayout->addWidget(this->materializeButton);
  buttonLayout->addWidget(this->detailButton);
  buttonLayout->addWidget(this->saveButton);

  auto *checkRow = new QWidget(central);
  auto *checkLayout = new FlowLayout(checkRow, 0, -1, 2);
  checkLayout->addWidget(this->showRefsCheck);
  checkLayout->addWidget(this->fillColorsCheck);
  checkLayout->addWidget(this->modifiersCheck);
  checkLayout->addWidget(this->separateFilesCheck);
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
  // Fill mode, scale and gap are all persisted sheet settings: a user change writes
  // // @sheet and reprojects (fill needs faces; the projection cache keeps it cheap).
  connect(this->fillColorsCheck, &QCheckBox::toggled, this,
          [this](bool) { emit sheetSettingsChanged(); });
  connect(this->modifiersCheck, &QCheckBox::toggled, this,
          [this](bool) { emit sheetSettingsChanged(); });
  connect(this->scaleSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
          [this](double) { emit sheetSettingsChanged(); });
}

double DrawingViewDock::exportScale() const
{
  const double denom = this->scaleSpin->value();
  return denom > 1e-6 ? 1.0 / denom : 1.0;  // drawing-mm per model-mm
}

double DrawingViewDock::scaleDenom() const
{
  return this->scaleSpin ? this->scaleSpin->value() : 1.0;
}

bool DrawingViewDock::fillColors() const
{
  return this->fillColorsCheck && this->fillColorsCheck->isChecked();
}

bool DrawingViewDock::showSlicerModifiers() const
{
  return !this->modifiersCheck || this->modifiersCheck->isChecked();
}

double DrawingViewDock::sheetGap() const
{
  return this->gapSpin ? this->gapSpin->value() : 16.0;
}

void DrawingViewDock::applySheetSettings(double gapMm, double scaleDenominator, bool fill,
                                         bool modifiers, const std::vector<bool>& viewsShown)
{
  // Set the controls from the parsed // @sheet without emitting sheetSettingsChanged
  // (blocked), so reading source never writes it straight back.
  if (this->gapSpin) {
    QSignalBlocker b(this->gapSpin);
    this->gapSpin->setValue(gapMm);
  }
  if (this->scaleSpin && scaleDenominator > 1e-6) {
    QSignalBlocker b(this->scaleSpin);
    this->scaleSpin->setValue(scaleDenominator);
  }
  if (this->fillColorsCheck) {
    QSignalBlocker b(this->fillColorsCheck);
    this->fillColorsCheck->setChecked(fill);
  }
  if (this->modifiersCheck) {
    QSignalBlocker b(this->modifiersCheck);
    this->modifiersCheck->setChecked(modifiers);
  }
  for (int i = 0; i < this->viewChecks.size() && i < static_cast<int>(viewsShown.size()); ++i) {
    QSignalBlocker b(this->viewChecks.at(i));
    this->viewChecks.at(i)->setChecked(viewsShown[i]);
  }
}

double DrawingViewDock::computeAutoGap() const
{
  // Clearance to leave between the drawn extents (geometry + dimensions + labels) of
  // adjacent tiles, in drawing-mm.
  constexpr double kAutoGapTargetMm = 10.0;

  // The extent a tile actually draws beyond its geometry bounding box, on each side,
  // in drawing-mm. Dimensions sit off the geometry by their offset; // @label text
  // and dragged dimension labels sit at their own offsets — all in view-local mm, so
  // multiply by the tile's drawScale to get drawing-mm. The px-sized bits (arrowheads,
  // label boxes) are ignored: they don't scale with mm and are small next to the
  // offsets that drive collisions.
  double maxL = 0, maxR = 0, maxT = 0, maxB = 0;  // overhang beyond bbox per side
  auto consider = [&](const OrthoViewData& d, double drawScale) {
    if (!d.valid()) return;
    const double w = d.sizeMm.width(), h = d.sizeMm.height();
    auto include = [&](const QPointF& p) {
      maxL = std::max(maxL, (0.0 - p.x()) * drawScale);
      maxR = std::max(maxR, (p.x() - w) * drawScale);
      maxT = std::max(maxT, (0.0 - p.y()) * drawScale);
      maxB = std::max(maxB, (p.y() - h) * drawScale);
    };
    for (const auto& dim : d.dims) {
      QPointF p0, p1;
      dimLineEnds(dim, p0, p1);
      include(p0);
      include(p1);
      include(labelPosOf(dim));
    }
    for (const auto& lb : d.labels) include(lb.anchor + lb.off);
    for (const auto& a : d.angles) { include(a.a); include(a.b); }
  };
  for (int i = 0; i < this->views.size(); ++i)
    if (isViewSelected(i)) consider(this->views.at(i), exportScale());
  for (const auto& s : this->sectionViews) consider(s, exportScale());

  // One gap value: size it for the tighter of the two directions (a horizontal gap
  // must clear the right overhang of one view plus the left overhang of its neighbour;
  // likewise vertical), so the tightest adjacency still clears the target.
  const double needH = kAutoGapTargetMm + maxL + maxR;
  const double needV = kAutoGapTargetMm + maxT + maxB;
  return std::clamp(std::max(needH, needV), kAutoGapTargetMm, 200.0);
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

void DrawingViewDock::setGeometryVersion(int version)
{
  if (this->sheet) this->sheet->setGeometryVersion(version);
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
  const double marginMm = 10.0, gapMm = sheetGap(), titleMm = 7.0;
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

namespace {

// One placed tile on the sheet: its view data, drawing-mm origin, slug, and this
// tile's own drawing-mm-per-model-mm scale.
struct Placement { const OrthoViewData *v; double ox, oy; QString slug; double drawScale; };

// Build the placed views' primitives (drawing-mm, y-down) and render to `path`.
// Format is chosen from the extension (.dxf / .pdf / .png / .svg); every format
// consumes the same primitives, so all of them are truly WYSIWYG. File-scope (no
// dock state) so the GUI and the CLI share one export path.
static void renderSheetToFile(const QVector<Placement>& places, double sheetWmm,
                              double sheetHmm, const QString& path, double rasterDpi)
{
  const bool isPng = path.endsWith(".png", Qt::CaseInsensitive);
  // Vector formats keep their physical size whatever the dpi; only PNG uses it.
  double dpi = isPng && rasterDpi > 0 ? rasterDpi : 96.0;
  const double fontMm = 3.0, edgeMm = 0.3, dimMm = 0.25, witMm = 0.15, arrowMm = 2.5;
  {
    std::vector<QLineF> edgeSegs, hatchSegs, dimSegs, witSegs;
    std::vector<QPolygonF> arrows;
    std::vector<std::pair<QPolygonF, QColor>> fillPolys;  // coloured face fills (behind edges)
    std::vector<std::pair<QPointF, QString>> labels, titles;
    std::vector<std::pair<QPointF, QString>> textLabels;  // // @label callouts (black)
    for (const auto& pl : places) {
      const auto& v = *pl.v;
      const double oxmm = pl.ox, oymm = pl.oy;
      const double sc = pl.drawScale > 1e-9 ? pl.drawScale : 1.0;  // this tile's scale
      auto map = [&](const QPointF& mp) {
      return QPointF(oxmm + mp.x() * sc, oymm + mp.y() * sc);  // -> drawing-mm
    };
    titles.emplace_back(QPointF(oxmm, oymm - fontMm), v.title);  // ~one font height off
    for (const auto& f : v.fills) {
      QPolygonF poly;
      for (const auto& p : f.poly) poly << map(p);
      fillPolys.emplace_back(poly, f.color);
    }
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
    for (const auto& lb : v.labels) {
      const QPointF anc = map(lb.anchor);
      const QPointF pos = map(lb.anchor + lb.off);
      if (lb.leader && std::hypot((pos - anc).x(), (pos - anc).y()) > fontMm)
        witSegs.emplace_back(pos, anc);
      const double c = arrowMm * 0.4;  // small cross at the anchor point
      edgeSegs.emplace_back(anc - QPointF(c, 0), anc + QPointF(c, 0));
      edgeSegs.emplace_back(anc - QPointF(0, c), anc + QPointF(0, c));
      textLabels.emplace_back(pos, lb.text);
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

  // Fit the sheet to what is actually drawn. The layout sizes the sheet from the
  // views' geometry boxes only, so dimensions, labels and free at= tiles can reach
  // past it (even to negative coordinates) and would be clipped in every format.
  {
    double x0 = 0, y0 = 0, x1 = sheetWmm, y1 = sheetHmm;
    bool any = false;
    double cx0 = 0, cy0 = 0, cx1 = 0, cy1 = 0;  // drawn content extents
    auto addPt = [&](const QPointF& p) {
      if (!std::isfinite(p.x()) || !std::isfinite(p.y())) return;
      if (!any) { cx0 = cx1 = p.x(); cy0 = cy1 = p.y(); any = true; return; }
      cx0 = std::min(cx0, p.x()); cx1 = std::max(cx1, p.x());
      cy0 = std::min(cy0, p.y()); cy1 = std::max(cy1, p.y());
    };
    for (const auto *segs : {&edgeSegs, &hatchSegs, &dimSegs, &witSegs})
      for (const auto& l : *segs) { addPt(l.p1()); addPt(l.p2()); }
    for (const auto& h : arrows) for (const auto& p : h) addPt(p);
    for (const auto& [poly, color] : fillPolys) for (const auto& p : poly) addPt(p);
    // Text extents in mm, measured with the same font the painter uses.
    QFont f;
    f.setPixelSize(100);
    const QFontMetricsF fm(f);
    auto textW = [&](const QString& t) { return fm.horizontalAdvance(t) / 100.0 * fontMm; };
    for (const auto *ls : {&labels, &textLabels})
      for (const auto& [pos, txt] : *ls) {  // centred boxes
        const double hw = textW(txt) / 2 + 1.0, hh = fontMm;
        addPt(pos - QPointF(hw, hh));
        addPt(pos + QPointF(hw, hh));
      }
    for (const auto& [pos, txt] : titles) {  // baseline-left text
      addPt(pos - QPointF(0, fontMm));
      addPt(pos + QPointF(textW(txt), fontMm * 0.4));
    }
    if (any) {
      constexpr double kFitMarginMm = 5.0;
      x0 = std::min(x0, cx0 - kFitMarginMm); y0 = std::min(y0, cy0 - kFitMarginMm);
      x1 = std::max(x1, cx1 + kFitMarginMm); y1 = std::max(y1, cy1 + kFitMarginMm);
    }
    const QPointF shift(-x0, -y0);
    if (!shift.isNull()) {
      for (auto *segs : {&edgeSegs, &hatchSegs, &dimSegs, &witSegs})
        for (auto& l : *segs) l.translate(shift);
      for (auto& h : arrows) h.translate(shift);
      for (auto& fp : fillPolys) fp.first.translate(shift);
      for (auto *ls : {&labels, &textLabels, &titles})
        for (auto& lp : *ls) lp.first += shift;
    }
    sheetWmm = x1 - x0;
    sheetHmm = y1 - y0;
  }

  // Keep the raster within what QImage/QPainter handle (and a sane amount of
  // memory); a large 1:1 sheet at a high dpi would otherwise fail or be clipped.
  if (isPng) {
    constexpr double kMaxSidePx = 32000.0, kMaxPixels = 268435456.0;  // 2^28 px = 1 GiB ARGB
    const double wpx = sheetWmm * dpi / 25.4, hpx = sheetHmm * dpi / 25.4;
    double k = std::min({1.0, kMaxSidePx / std::max(wpx, 1.0), kMaxSidePx / std::max(hpx, 1.0),
                         std::sqrt(kMaxPixels / std::max(wpx * hpx, 1.0))});
    if (k < 1.0) {
      const double reduced = std::floor(dpi * k);
      LOG(message_group::Warning,
          "Drawing export: a %1$.0f x %2$.0f mm sheet at %3$g dpi is too large for PNG; "
          "using %4$g dpi.",
          sheetWmm, sheetHmm, dpi, reduced);
      dpi = reduced;
    }
  }
  const double mmToPx = dpi / 25.4;

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
    for (const auto& [p, s] : textLabels) text(p, s, fontMm);
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

    // Coloured face fills first, so the black edges (and everything else) sit on top.
    p.setPen(Qt::NoPen);
    for (const auto& [poly, color] : fillPolys) {
      QPolygonF q;
      for (const auto& pt : poly) q << P(pt);
      p.setBrush(color);
      p.drawPolygon(q);
    }

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
    // // @label callouts: black text (annotations, not dimension values).
    for (const auto& [pos, txt] : textLabels) {
      const QPointF c = P(pos);
      const QRectF tr = p.fontMetrics().boundingRect(txt).adjusted(-2, -1, 2, 1);
      QRectF box(c.x() - tr.width() / 2, c.y() - tr.height() / 2, tr.width(), tr.height());
      p.fillRect(box, QColor(255, 255, 255, 220));
      p.setPen(Qt::black);
      p.drawText(box, Qt::AlignCenter, txt);
    }
  };

  if (isPng) {
    QImage img(std::max(1, sheetWpx), std::max(1, sheetHpx), QImage::Format_ARGB32);
    if (img.isNull()) {
      LOG(message_group::Error, "Drawing export: cannot allocate a %1$d x %2$d px image.",
          sheetWpx, sheetHpx);
      return;
    }
    img.setDotsPerMeterX(qRound(dpi / 0.0254));
    img.setDotsPerMeterY(qRound(dpi / 0.0254));
    img.fill(Qt::white);
    QPainter p(&img);
    paintSheet(p);
    p.end();
    img.save(path, "PNG");
    return;
  }
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
  }  // scoped body block
}  // renderSheetToFile

}  // namespace

bool DrawingViewDock::exportDrawingSheet(const QList<OrthoViewData>& views,
                                         const QList<OrthoViewData>& sectionViews,
                                         const QList<OrthoViewData>& detailViews,
                                         const std::vector<bool>& selectedViews, double scale,
                                         double gapMm, bool separateFiles, const QString& path,
                                         double dpi)
{
  const double marginMm = 10.0, titleMm = 7.0;
  std::vector<bool> sel(kViewCount, false);
  for (int i = 0; i < kViewCount && i < static_cast<int>(selectedViews.size()); ++i)
    sel[i] = selectedViews[i];
  const QVector<SheetTile> tiles = collectSheetTiles(views, sectionViews, detailViews, sel, scale);
  if (tiles.isEmpty()) return false;

  if (separateFiles) {
    // One file per tile (name_slug.ext), each a single-view sheet.
    for (const auto& t : tiles) {
      const double w = t.v->sizeMm.width() * t.drawScale, h = t.v->sizeMm.height() * t.drawScale;
      QString p = path;
      const int dot = p.lastIndexOf('.');
      const QString slug = "_" + t.slug;
      if (dot >= 0) p.insert(dot, slug); else p += slug;
      renderSheetToFile({{t.v, marginMm, marginMm + titleMm, t.slug, t.drawScale}},
                        w + 2 * marginMm, h + titleMm + 2 * marginMm, p, dpi);
    }
  } else {
    // Composed first-angle sheet, laid out by the shared engine.
    const SheetLayoutResult lay = computeSheetLayout(tiles, marginMm, gapMm, titleMm);
    QVector<Placement> places;
    for (int i = 0; i < tiles.size(); ++i)
      places.push_back({tiles.at(i).v, lay.origins.at(i).x(), lay.origins.at(i).y(),
                        tiles.at(i).slug, tiles.at(i).drawScale});
    renderSheetToFile(places, lay.wMm, lay.hMm, path, dpi);
  }
  return true;
}

bool DrawingViewDock::exportToPath(const QString& path, double dpi)
{
  bool any = false;
  for (int i = 0; i < this->views.size(); ++i)
    if (isViewSelected(i) && this->views.at(i).valid()) any = true;
  for (const auto& s : this->sectionViews)
    if (s.valid()) any = true;
  for (const auto& d : this->detailViews)
    if (d.valid()) any = true;
  if (!any || path.isEmpty()) return false;

  std::vector<bool> sel(kViewCount, false);
  for (int i = 0; i < kViewCount; ++i) sel[i] = isViewSelected(i);
  return exportDrawingSheet(this->views, this->sectionViews, this->detailViews, sel, exportScale(),
                            sheetGap(), this->separateFilesCheck->isChecked(), path, dpi);
}

void DrawingViewDock::exportDrawing()
{
  const QString filename = QFileDialog::getSaveFileName(
    this, tr("Export drawing"), "drawing.svg",
    tr("SVG (*.svg);;PDF (*.pdf);;PNG (*.png);;DXF (*.dxf)"));
  if (filename.isEmpty()) return;
  exportToPath(filename);
}
