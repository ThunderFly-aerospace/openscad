#include "gui/CameraViewDock.h"

#include <algorithm>
#include <cmath>
#include <functional>

#include <QFileDialog>
#include <QFontMetrics>
#include <QListWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPushButton>
#include <QResizeEvent>
#include <QVBoxLayout>
#include <QWidget>

// ---------------------------------------------------------------------------
// CameraImageView: the snapshot + draggable // @label callouts.
// ---------------------------------------------------------------------------

CameraImageView::CameraImageView(QWidget *parent) : QWidget(parent)
{
  setMinimumSize(160, 120);
  this->placeholder = tr("No cameras in model.");
}

void CameraImageView::setContent(const QImage& img, const QList<CameraLabel>& labs,
                                 const QList<CameraLabel>& marks)
{
  this->image = img;
  this->labels = labs;
  this->markers = marks;
  this->draggingLabel = -1;
  update();
}

void CameraImageView::setPlaceholder(const QString& text)
{
  this->image = QImage();
  this->labels.clear();
  this->markers.clear();
  this->placeholder = text;
  update();
}

void CameraImageView::recomputeFit()
{
  if (this->image.isNull() || this->image.width() <= 0 || this->image.height() <= 0) {
    this->fitScale = 1.0;
    this->fitOffset = QPointF(0, 0);
    return;
  }
  const double sx = static_cast<double>(width()) / this->image.width();
  const double sy = static_cast<double>(height()) / this->image.height();
  this->fitScale = std::min(sx, sy);
  this->fitOffset = QPointF((width() - this->image.width() * this->fitScale) / 2.0,
                            (height() - this->image.height() * this->fitScale) / 2.0);
}

QPointF CameraImageView::imageToWidget(const QPointF& p) const
{
  return QPointF(p.x() * this->fitScale + this->fitOffset.x(),
                 p.y() * this->fitScale + this->fitOffset.y());
}

QPointF CameraImageView::widgetToImage(const QPointF& p) const
{
  if (this->fitScale < 1e-9) return p;
  return QPointF((p.x() - this->fitOffset.x()) / this->fitScale,
                 (p.y() - this->fitOffset.y()) / this->fitScale);
}

// Paint the label overlay with `painter` in the target space defined by
// `toTarget` (image px -> painter px) and `fontPx` text height. Shared by the
// on-screen widget (fit transform) and the saved PNG (identity, image res).
static void paintCameraLabels(QPainter& painter, const QList<CameraLabel>& labels,
                              const std::function<QPointF(const QPointF&)>& toTarget,
                              double fontPx, double clampW, double clampH)
{
  QFont font = painter.font();
  font.setPixelSize(std::max(6, static_cast<int>(std::round(fontPx))));
  painter.setFont(font);
  const QFontMetrics fm(font);
  for (const auto& lb : labels) {
    const QPointF anc = toTarget(lb.anchor);
    const QPointF ctr = toTarget(lb.anchor + lb.off);
    QRectF tr = fm.boundingRect(lb.text).adjusted(-4, -2, 4, 2);
    QRectF box(ctr.x() - tr.width() / 2.0, ctr.y() - tr.height() / 2.0, tr.width(), tr.height());
    if (clampW > 0 && clampH > 0) {
      box.moveTo(std::clamp(box.x(), 1.0, clampW - box.width() - 1.0),
                 std::clamp(box.y(), 1.0, clampH - box.height() - 1.0));
    }
    if (lb.leader &&
        std::hypot(box.center().x() - anc.x(), box.center().y() - anc.y()) > box.height()) {
      QPen leaderPen(QColor(230, 230, 230));
      leaderPen.setWidthF(std::max(1.0, fontPx * 0.08));
      painter.setPen(leaderPen);
      painter.drawLine(box.center(), anc);
    }
    const double dotR = std::max(2.0, fontPx * 0.18);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(20, 200, 90));
    painter.drawEllipse(anc, dotR, dotR);
    painter.fillRect(box, QColor(0, 0, 0, 170));
    painter.setPen(Qt::white);
    painter.drawText(box, Qt::AlignCenter, lb.text);
  }
}

void CameraImageView::paintEvent(QPaintEvent *)
{
  QPainter painter(this);
  painter.fillRect(rect(), palette().window());
  if (this->image.isNull()) {
    painter.setPen(palette().windowText().color());
    painter.drawText(rect(), Qt::AlignCenter, this->placeholder);
    return;
  }
  recomputeFit();
  const QRectF target(this->fitOffset.x(), this->fitOffset.y(),
                      this->image.width() * this->fitScale,
                      this->image.height() * this->fitScale);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
  painter.drawImage(target, this->image);

  painter.setRenderHint(QPainter::Antialiasing, true);
  // Font ~2.6% of the image height (as it will read on the saved PNG), scaled to
  // the widget by the fit factor so it stays legible when the dock is small.
  const double fontPx = std::max(11.0, this->image.height() * 0.026 * this->fitScale);
  paintCameraLabels(painter, this->labels,
                    [this](const QPointF& p) { return imageToWidget(p); }, fontPx,
                    width(), height());

  // label() marker dots (clickable to create a // @label on this camera). Live
  // editing affordance only — not burned into the saved PNG.
  QFont mfont = painter.font();
  mfont.setPixelSize(std::max(9, static_cast<int>(std::round(fontPx * 0.9))));
  painter.setFont(mfont);
  const double dotR = std::max(3.0, fontPx * 0.22);
  for (const auto& m : this->markers) {
    const QPointF p = imageToWidget(m.anchor);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(30, 200, 110));
    painter.drawEllipse(p, dotR, dotR);
    painter.setPen(QColor(15, 120, 70));
    painter.drawText(p + QPointF(dotR + 2, -dotR), m.text);
  }
}

int CameraImageView::labelAt(const QPointF& pt) const
{
  const double fontPx = std::max(11.0, this->image.height() * 0.026 * this->fitScale);
  QFont font = this->font();
  font.setPixelSize(std::max(6, static_cast<int>(std::round(fontPx))));
  const QFontMetrics fm(font);
  for (int i = 0; i < this->labels.size(); ++i) {
    const QPointF ctr = imageToWidget(this->labels.at(i).anchor + this->labels.at(i).off);
    QRectF tr = fm.boundingRect(this->labels.at(i).text).adjusted(-4, -2, 4, 2);
    QRectF box(ctr.x() - tr.width() / 2.0, ctr.y() - tr.height() / 2.0, tr.width(), tr.height());
    if (box.contains(pt)) return i;
  }
  return -1;
}

int CameraImageView::markerAt(const QPointF& pt) const
{
  const double fontPx = std::max(11.0, this->image.height() * 0.026 * this->fitScale);
  const double r = std::max(3.0, fontPx * 0.22) + 4.0;  // dot radius + a little slack
  for (int i = 0; i < this->markers.size(); ++i) {
    const QPointF p = imageToWidget(this->markers.at(i).anchor);
    if (std::hypot(p.x() - pt.x(), p.y() - pt.y()) <= r) return i;
  }
  return -1;
}

void CameraImageView::mousePressEvent(QMouseEvent *event)
{
  const int i = labelAt(event->position());
  if (i >= 0) {
    if (event->button() == Qt::RightButton) {
      if (this->labels.at(i).id >= 0) emit jumpToSource(this->labels.at(i).id);
      return;
    }
    if (event->button() == Qt::LeftButton) { this->draggingLabel = i; return; }
  }
  // Otherwise a click on a label() marker dot creates a // @label for this camera.
  if (event->button() == Qt::LeftButton) {
    const int m = markerAt(event->position());
    // Copy the reference out: the slot re-renders the snapshots, which replaces
    // `markers` while the signal is still on the stack (a reference into the list
    // would dangle).
    if (m >= 0) {
      const QString ref = this->markers.at(m).ref;
      emit labelAnchorClicked(ref);
    }
  }
}

void CameraImageView::mouseMoveEvent(QMouseEvent *event)
{
  if (this->draggingLabel < 0 || this->draggingLabel >= this->labels.size()) return;
  CameraLabel& lb = this->labels[this->draggingLabel];
  lb.off = widgetToImage(event->position()) - lb.anchor;
  update();
  emit labelTextMoved(lb.id, lb.off.x(), lb.off.y(), false);
}

void CameraImageView::mouseReleaseEvent(QMouseEvent *)
{
  if (this->draggingLabel < 0 || this->draggingLabel >= this->labels.size()) return;
  const CameraLabel& lb = this->labels[this->draggingLabel];
  emit labelTextMoved(lb.id, lb.off.x(), lb.off.y(), true);
  this->draggingLabel = -1;
}

void CameraImageView::mouseDoubleClickEvent(QMouseEvent *event)
{
  const int i = labelAt(event->position());
  if (i >= 0 && this->labels.at(i).id >= 0) emit labelDoubleClicked(this->labels.at(i).id);
}

// ---------------------------------------------------------------------------
// CameraViewDock
// ---------------------------------------------------------------------------

// Composite the snapshot with its label overlay at native resolution, for saving.
static QImage compositeSnapshot(const QImage& image, const QList<CameraLabel>& labels)
{
  if (image.isNull() || labels.isEmpty()) return image;
  QImage out = image.convertToFormat(QImage::Format_ARGB32);
  QPainter painter(&out);
  painter.setRenderHint(QPainter::Antialiasing, true);
  paintCameraLabels(painter, labels, [](const QPointF& p) { return p; },
                    std::max(11.0, out.height() * 0.026), out.width(), out.height());
  return out;
}

CameraViewDock::CameraViewDock(QWidget *parent) : Dock(parent)
{
  auto *central = new QWidget(this);
  auto *layout = new QVBoxLayout(central);
  layout->setContentsMargins(4, 4, 4, 4);

  this->cameraList = new QListWidget(central);
  this->cameraList->setMaximumHeight(96);

  this->preview = new CameraImageView(central);

  this->saveButton = new QPushButton(tr("Save PNG…"), central);
  this->saveButton->setEnabled(false);

  layout->addWidget(this->cameraList);
  layout->addWidget(this->preview, 1);
  layout->addWidget(this->saveButton);
  setWidget(central);

  connect(this->cameraList, &QListWidget::currentRowChanged, this, &CameraViewDock::updatePreview);
  connect(this->saveButton, &QPushButton::clicked, this, &CameraViewDock::saveCurrent);
  // Forward the label-overlay interaction signals to the owner (MainWindow).
  connect(this->preview, &CameraImageView::labelTextMoved, this, &CameraViewDock::labelTextMoved);
  connect(this->preview, &CameraImageView::labelDoubleClicked, this,
          &CameraViewDock::labelDoubleClicked);
  connect(this->preview, &CameraImageView::jumpToSource, this, &CameraViewDock::jumpToSource);
  // A marker click carries the reference; attach the currently shown camera (its
  // row and name; the name is empty for an unnamed camera(), which the owner then
  // names in the source).
  // Queued: creating the label rewrites the source and re-renders the snapshots
  // (which replaces this view's content), so it must not run inside the image
  // view's mouse handler.
  connect(
    this->preview, &CameraImageView::labelAnchorClicked, this,
    [this](const QString& ref) {
      const int row = this->cameraList->currentRow();
      const QString camName =
        row >= 0 && row < this->names.size() ? this->names.at(row) : QString();
      emit labelAnchorClicked(row, camName, ref);
    },
    Qt::QueuedConnection);
}

CameraViewDock::~CameraViewDock() = default;

void CameraViewDock::setSnapshots(const QList<QImage>& newImages, const QStringList& newNames,
                                  const QList<QList<CameraLabel>>& newLabels,
                                  const QList<QList<CameraLabel>>& newMarkers)
{
  this->images = newImages;
  this->names = newNames;
  this->overlays = newLabels;
  this->markerOverlays = newMarkers;

  this->cameraList->blockSignals(true);
  this->cameraList->clear();
  for (int i = 0; i < this->names.size(); ++i) {
    const QString& n = this->names.at(i);
    this->cameraList->addItem(n.isEmpty() ? tr("camera %1").arg(i + 1) : n);
  }
  this->cameraList->blockSignals(false);

  if (!this->images.isEmpty()) {
    this->cameraList->setCurrentRow(0);
  } else {
    this->preview->setPlaceholder(tr("No cameras in model."));
    this->saveButton->setEnabled(false);
  }
  updatePreview();
}

void CameraViewDock::clearSnapshots()
{
  setSnapshots({}, {}, {}, {});
}

void CameraViewDock::updatePreview()
{
  const int row = this->cameraList->currentRow();
  const bool valid = row >= 0 && row < this->images.size();
  this->saveButton->setEnabled(valid);
  showCurrentImage();
}

void CameraViewDock::showCurrentImage()
{
  const int row = this->cameraList->currentRow();
  if (row < 0 || row >= this->images.size()) {
    this->preview->setPlaceholder(tr("No cameras in model."));
    return;
  }
  const QImage& img = this->images.at(row);
  if (img.isNull()) {
    this->preview->setPlaceholder(tr("(empty render)"));
    return;
  }
  const QList<CameraLabel> labs = row < this->overlays.size() ? this->overlays.at(row)
                                                              : QList<CameraLabel>{};
  const QList<CameraLabel> marks =
    row < this->markerOverlays.size() ? this->markerOverlays.at(row) : QList<CameraLabel>{};
  this->preview->setContent(img, labs, marks);
}

void CameraViewDock::saveCurrent()
{
  const int row = this->cameraList->currentRow();
  if (row < 0 || row >= this->images.size()) return;

  QString suggested = row < this->names.size() && !this->names.at(row).isEmpty()
                        ? this->names.at(row)
                        : QString("camera%1").arg(row + 1);
  const QString filename = QFileDialog::getSaveFileName(this, tr("Save Camera Snapshot"),
                                                        suggested + ".png", tr("PNG image (*.png)"));
  if (filename.isEmpty()) return;
  const QList<CameraLabel> labs = row < this->overlays.size() ? this->overlays.at(row)
                                                              : QList<CameraLabel>{};
  compositeSnapshot(this->images.at(row), labs).save(filename, "PNG");
}
