#include <cassert>
#include <cstdio>
#include <memory>
#include <ostream>

#include <algorithm>

#include "core/CameraNode.h"
#include "core/Tree.h"
#include "geometry/Geometry.h"
#include "geometry/linalg.h"
#include "glview/Camera.h"
#include "glview/CsgInfo.h"
#include "glview/OffscreenView.h"
#include "glview/RenderSettings.h"
#include "glview/Renderer.h"
#include "io/export.h"
#include "utils/camera_math.h"
#include "utils/printutils.h"

#ifndef NULLGL
#include "glview/PolySetRenderer.h"
#if not defined(USE_POLYSET_FOR_CGAL)
#include "glview/cgal/CGALRenderer.h"
#endif

#ifdef ENABLE_OPENCSG
#include <opencsg.h>

#include "glview/preview/OpenCSGRenderer.h"
#endif  // ENABLE_OPENCSG

#include "glview/preview/ThrownTogetherRenderer.h"

namespace {

void setupCamera(Camera& cam, const BoundingBox& bbox)
{
  if (cam.viewall) cam.viewAll(bbox);
}

}  // namespace

bool export_png(const std::shared_ptr<const Geometry>& root_geom, const ViewOptions& options,
                Camera& camera, std::ostream& output)
{
  assert(root_geom != nullptr);
  PRINTD("export_png geom");
  std::unique_ptr<OffscreenView> glview;
  try {
    glview = std::make_unique<OffscreenView>(camera.pixel_width, camera.pixel_height);
  } catch (const OffscreenViewException& ex) {
    fprintf(stderr, "Can't create OffscreenView: %s.\n", ex.what());
    return false;
  }
  std::shared_ptr<Renderer> geomRenderer;
#if defined(USE_POLYSET_FOR_CGAL)
  geomRenderer = std::make_shared<PolySetRenderer>(root_geom);
#else
  // Choose PolySetRenderer for PolySet and Polygon2d, and for Manifold since we
  // know that all geometries are convertible to PolySet.
  if (RenderSettings::inst()->backend3D == RenderBackend3D::ManifoldBackend ||
      std::dynamic_pointer_cast<const PolySet>(root_geom) ||
      std::dynamic_pointer_cast<const Polygon2d>(root_geom)) {
    geomRenderer = std::make_shared<PolySetRenderer>(root_geom);
  } else {
    geomRenderer = std::make_shared<CGALRenderer>(root_geom);
  }
#endif
  const BoundingBox bbox = geomRenderer->getBoundingBox();
  setupCamera(camera, bbox);

  glview->setCamera(camera);
  glview->setRenderer(geomRenderer);
  glview->setColorScheme(RenderSettings::inst()->colorscheme);
  glview->setShowCrosshairs(options["crosshairs"]);
  glview->setShowAxes(options["axes"]);
  glview->setShowScaleProportional(options["scales"]);
  glview->setShowEdges(options["edges"]);
  glview->paintGL();
  glview->save(output);
  return true;
}

std::unique_ptr<OffscreenView> prepare_preview(Tree& tree, const ViewOptions& options, Camera& camera)
{
  PRINTD("prepare_preview_common");
  CsgInfo csgInfo = CsgInfo();
  csgInfo.compile_products(tree);

  std::unique_ptr<OffscreenView> glview;
  try {
    glview = std::make_unique<OffscreenView>(camera.pixel_width, camera.pixel_height);
  } catch (const OffscreenViewException& ex) {
    LOG("Can't create OffscreenView: %1$s.", ex.what());
    return nullptr;
  }

  std::shared_ptr<Renderer> renderer;
  if (options.previewer == Previewer::OPENCSG) {
#ifdef ENABLE_OPENCSG
    PRINTD("Initializing OpenCSGRenderer");
    renderer = std::make_shared<OpenCSGRenderer>(csgInfo.root_products, csgInfo.highlights_products,
                                                 csgInfo.background_products);
#else
    fprintf(stderr, "This openscad was built without OpenCSG support\n");
    return 0;
#endif
  } else {
    PRINTD("Initializing ThrownTogetherRenderer");
    renderer = std::make_shared<ThrownTogetherRenderer>(
      csgInfo.root_products, csgInfo.highlights_products, csgInfo.background_products);
  }

  glview->setRenderer(renderer);

#ifdef ENABLE_OPENCSG
  const BoundingBox bbox = glview->getRenderer()->getBoundingBox();
  setupCamera(camera, bbox);

  glview->setCamera(camera);
  OpenCSG::setContext(0);
  OpenCSG::setOption(OpenCSG::OffscreenSetting, OpenCSG::FrameBufferObject);
#endif
  glview->setColorScheme(RenderSettings::inst()->colorscheme);
  glview->setShowAxes(options["axes"]);
  glview->setShowScaleProportional(options["scales"]);
  glview->setShowEdges(options["edges"]);
  glview->paintGL();
  return glview;
}

bool export_png(const OffscreenView& glview, std::ostream& output)
{
  PRINTD("export_png_preview_common");
  glview.save(output);
  return true;
}

namespace {

Camera cameraFromNode(const CameraNode& node)
{
  Camera cam;
  cam.pixel_width = node.pixel_width;
  cam.pixel_height = node.pixel_height;
  cam.projection = node.projection == CameraNode::Projection::ORTHOGONAL
                     ? Camera::ProjectionType::ORTHOGONAL
                     : Camera::ProjectionType::PERSPECTIVE;
  // fov is the vertical field of view; the horizontal extent follows from the
  // pixel aspect ratio in gluPerspective/glOrtho.
  cam.fov = node.fov >= 0 ? node.fov : camera_math::focalToFovDegrees(node.focal, node.sensor);
  cam.viewall = false;
  cam.autocenter = false;
  return cam;
}

// Pick a viewer_distance that keeps the scene within the [0.1*d, 100*d] clip
// planes measured from the eye. The eye world position is preserved regardless.
double distanceForScene(const Eigen::Vector3d& eye, const BoundingBox& bbox)
{
  if (bbox.isEmpty()) return 140.0;
  const double toCenter = (bbox.center() - eye).norm();
  const double radius = bbox.diagonal().norm() / 2.0;
  return std::max({toCenter, 2.0 * radius, 1.0});
}

}  // namespace

std::unique_ptr<OffscreenView> render_camera_preview(Tree& tree, const ViewOptions& options,
                                                     const CameraInfo& info)
{
  if (!info.node) return nullptr;
  Camera camera = cameraFromNode(*info.node);

  CsgInfo csgInfo = CsgInfo();
  csgInfo.compile_products(tree);

  std::unique_ptr<OffscreenView> glview;
  try {
    glview = std::make_unique<OffscreenView>(camera.pixel_width, camera.pixel_height);
  } catch (const OffscreenViewException& ex) {
    LOG("Can't create OffscreenView: %1$s.", ex.what());
    return nullptr;
  }

  std::shared_ptr<Renderer> renderer;
  if (options.previewer == Previewer::OPENCSG) {
#ifdef ENABLE_OPENCSG
    renderer = std::make_shared<OpenCSGRenderer>(csgInfo.root_products, csgInfo.highlights_products,
                                                 csgInfo.background_products);
#else
    renderer = std::make_shared<ThrownTogetherRenderer>(
      csgInfo.root_products, csgInfo.highlights_products, csgInfo.background_products);
#endif
  } else {
    renderer = std::make_shared<ThrownTogetherRenderer>(
      csgInfo.root_products, csgInfo.highlights_products, csgInfo.background_products);
  }
  glview->setRenderer(renderer);

  const BoundingBox bbox = glview->getRenderer()->getBoundingBox();
  const Eigen::Vector3d eye = info.world.translation();
  camera.setFromMatrix(info.world, distanceForScene(eye, bbox));

  // Set near/far from the actual scene so geometry right in front of (or around)
  // the camera is not clipped by the default 0.1*dist near plane. The eye and
  // view direction are already fixed by setFromMatrix; near/far only bound depth.
  if (!bbox.isEmpty()) {
    const double toCenter = (bbox.center() - eye).norm();
    const double radius = bbox.diagonal().norm() / 2.0;
    const double farthest = toCenter + radius;
    // Nearest scene point ahead of the eye; if the camera is inside/at the model
    // (nearest <= 0) fall back to a small fraction of the extent to keep the
    // near plane positive and the far/near ratio finite for depth precision.
    const double nearest = toCenter - radius;
    camera.clip_near = std::max(nearest, farthest * 1e-3);
    camera.clip_far = farthest * 1.05 + 1e-3;
  }
  glview->setCamera(camera);

#ifdef ENABLE_OPENCSG
  OpenCSG::setContext(0);
  OpenCSG::setOption(OpenCSG::OffscreenSetting, OpenCSG::FrameBufferObject);
#endif
  glview->setColorScheme(RenderSettings::inst()->colorscheme);
  glview->setShowAxes(options["axes"]);
  glview->setShowScaleProportional(options["scales"]);
  glview->setShowEdges(options["edges"]);
  glview->setShowCrosshairs(options["crosshairs"]);
  glview->paintGL();
  return glview;
}

std::unique_ptr<OffscreenView> render_ortho_view(Tree& tree, const ViewOptions& options,
                                                 const Eigen::Vector3d& object_rot,
                                                 unsigned int pixel_width, unsigned int pixel_height)
{
  // An orthographic view down a fixed gimbal angle (front/side/top), framing the
  // whole model. Reuses the preview path so color() colors are preserved.
  Camera camera;
  camera.projection = Camera::ProjectionType::ORTHOGONAL;
  camera.object_rot = object_rot;
  camera.viewall = true;
  camera.autocenter = true;
  camera.pixel_width = pixel_width;
  camera.pixel_height = pixel_height;
  // Leave margin around the model so dimension overlays pulled off the object
  // stay inside the frame. For an orthographic viewAll the framed half-height is
  // radius / cos(fov/2), so a larger fov simply zooms out; fov=75 ~= 29% margin.
  // The overlay reads the resulting GL matrices, so projection stays consistent.
  camera.fov = 75.0;
  return prepare_preview(tree, options, camera);
}

#else  // NULLGL

bool export_png(const std::shared_ptr<const Geometry>& root_geom, const ViewOptions& options,
                Camera& camera, std::ostream& output)
{
  return false;
}
std::unique_ptr<OffscreenView> prepare_preview(Tree& tree, const ViewOptions& options, Camera& camera)
{
  return nullptr;
}
std::unique_ptr<OffscreenView> render_camera_preview(Tree& tree, const ViewOptions& options,
                                                     const CameraInfo& info)
{
  return nullptr;
}
std::unique_ptr<OffscreenView> render_ortho_view(Tree& tree, const ViewOptions& options,
                                                 const Eigen::Vector3d& object_rot,
                                                 unsigned int pixel_width, unsigned int pixel_height)
{
  return nullptr;
}
bool export_png(const OffscreenView& glview, std::ostream& output)
{
  return false;
}

#endif  // NULLGL
