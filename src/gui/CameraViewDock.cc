#include "gui/CameraViewDock.h"

#include <QFileDialog>
#include <QLabel>
#include <QListWidget>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QVBoxLayout>
#include <QWidget>

CameraViewDock::CameraViewDock(QWidget *parent) : Dock(parent)
{
  auto *central = new QWidget(this);
  auto *layout = new QVBoxLayout(central);
  layout->setContentsMargins(4, 4, 4, 4);

  this->cameraList = new QListWidget(central);
  this->cameraList->setMaximumHeight(96);

  this->preview = new QLabel(central);
  this->preview->setAlignment(Qt::AlignCenter);
  this->preview->setMinimumSize(160, 120);
  this->preview->setText(tr("No cameras in model."));

  this->saveButton = new QPushButton(tr("Save PNG…"), central);
  this->saveButton->setEnabled(false);

  layout->addWidget(this->cameraList);
  layout->addWidget(this->preview, 1);
  layout->addWidget(this->saveButton);
  setWidget(central);

  connect(this->cameraList, &QListWidget::currentRowChanged, this, &CameraViewDock::updatePreview);
  connect(this->saveButton, &QPushButton::clicked, this, &CameraViewDock::saveCurrent);
}

CameraViewDock::~CameraViewDock() = default;

void CameraViewDock::setSnapshots(const QList<QImage>& newImages, const QStringList& newNames)
{
  this->images = newImages;
  this->names = newNames;

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
    this->preview->setText(tr("No cameras in model."));
    this->saveButton->setEnabled(false);
  }
  updatePreview();
}

void CameraViewDock::clearSnapshots()
{
  setSnapshots({}, {});
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
  if (row < 0 || row >= this->images.size()) return;
  const QImage& img = this->images.at(row);
  if (img.isNull()) {
    this->preview->setText(tr("(empty render)"));
    return;
  }
  this->preview->setPixmap(QPixmap::fromImage(img).scaled(
    this->preview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void CameraViewDock::resizeEvent(QResizeEvent *event)
{
  Dock::resizeEvent(event);
  showCurrentImage();
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
  this->images.at(row).save(filename, "PNG");
}
