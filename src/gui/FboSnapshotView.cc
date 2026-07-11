#include "gui/FboSnapshotView.h"

#include <cstdint>
#include <string>

#include <QOpenGLFramebufferObject>
#include <QOpenGLFramebufferObjectFormat>

#include "utils/printutils.h"

FboSnapshotView::FboSnapshotView(uint32_t width, uint32_t height)
  : width_(width), height_(height)
{
  // A combined depth+stencil buffer is required: GLView::paintGL() clears the
  // stencil buffer and the OpenCSG previewer uses stencil for CSG operations.
  QOpenGLFramebufferObjectFormat format;
  format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
  fbo_ = std::make_unique<QOpenGLFramebufferObject>(static_cast<int>(width),
                                                    static_cast<int>(height), format);
  if (!fbo_->isValid()) {
    LOG("FboSnapshotView: framebuffer object is not valid.");
    fbo_.reset();
    return;
  }

  // Direct all subsequent rendering into our FBO rather than the interactive
  // widget's default framebuffer. The GL wrangler is already initialized on this
  // (shared) context by the interactive view, so we only replay GLView's per-view
  // GL state setup — not GLEW/GLAD initialization.
  fbo_->bind();
  GLView::initializeGL();
  GLView::resizeGL(static_cast<int>(width), static_cast<int>(height));
}

FboSnapshotView::~FboSnapshotView()
{
  if (fbo_) {
    if (fbo_->isBound()) fbo_->release();
    fbo_.reset();
  }
}

bool FboSnapshotView::isValid() const
{
  return fbo_ && fbo_->isValid();
}

QImage FboSnapshotView::toImage() const
{
  if (!fbo_) return {};
  return fbo_->toImage();
}

bool FboSnapshotView::save(const char *filename) const
{
  const QImage img = toImage();
  if (img.isNull()) return false;
  return img.save(filename, "PNG");
}

std::string FboSnapshotView::getRendererInfo() const
{
  return "FboSnapshotView (reusing interactive GL context)";
}

#ifdef ENABLE_OPENCSG
void FboSnapshotView::display_opencsg_warning()
{
  LOG("OpenSCAD recommended OpenGL version is 2.0.");
}
#endif
