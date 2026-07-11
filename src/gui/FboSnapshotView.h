#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include <QImage>

#include "glview/GLView.h"

class QOpenGLFramebufferObject;

// A GLView that renders into a Qt framebuffer object using the *currently bound*
// GL context, instead of creating its own GLX/EGL offscreen context the way
// OffscreenView does.
//
// Rationale: creating a standalone GLX offscreen context fails on some setups —
// notably the AppImage, whose bundled Mesa libGLX/libGLdispatch shadow the
// nvidia proprietary driver, so glXCreate*Context can't obtain a context
// ("Unable to obtain GL Context"). The interactive QOpenGLWidget context, on the
// other hand, is negotiated by Qt through the platform and is known to work.
// Reusing it sidesteps the whole class of failure.
//
// Precondition: a valid GL context must already be current when the view is
// constructed and rendered (the caller does QGLView::makeContextCurrent()). The
// GL wrangler (GLEW/GLAD) is assumed already initialized on that context by the
// interactive view — we deliberately do not re-initialize it.
class FboSnapshotView : public GLView
{
public:
  FboSnapshotView(uint32_t width, uint32_t height);
  ~FboSnapshotView() override;

  // Whether the FBO was successfully created in the current context.
  [[nodiscard]] bool isValid() const;

  // Read the rendered framebuffer back as an image. Call after the view has been
  // configured and painted (see configure_camera_snapshot). Returns a null image
  // if the FBO is invalid.
  QImage toImage() const;

  // GLView overrides
  bool save(const char *filename) const override;
  [[nodiscard]] std::string getRendererInfo() const override;
#ifdef ENABLE_OPENCSG
  void display_opencsg_warning() override;
#endif

private:
  uint32_t width_;
  uint32_t height_;
  std::unique_ptr<QOpenGLFramebufferObject> fbo_;
};
