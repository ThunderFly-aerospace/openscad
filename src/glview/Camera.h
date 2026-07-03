#pragma once

/*

   Camera

   For usage, see QGLView.cc, GLView.cc, export_png.cc, openscad.cc

   There are two different types of cameras represented in this class:

 * Gimbal camera - uses Euler Angles, object translation, and viewer distance
 * Vector camera - uses 'eye', 'center', and 'up' vectors ('lookat' style)

   They are not necessarily kept in sync. There are two modes of
   projection, Perspective and Orthogonal.

 */

#include "geometry/linalg.h"
#include "core/ScopeContext.h"
#include <memory>
#include <string>
#include <vector>
#include <Eigen/Geometry>

class Camera
{
public:
  enum class ProjectionType { ORTHOGONAL, PERSPECTIVE } projection{ProjectionType::PERSPECTIVE};
  Camera();
  void setup(std::vector<double> params);
  // Position the (gimbal) camera from a 4x4 world transform of a camera()
  // marker that looks along local -Z with local +Y up. `distance` only affects
  // clip planes / orthographic scale; the eye world position is preserved.
  void setFromMatrix(const Transform3d& world, double distance);
  void gimbalDefaultTranslate();
  void setProjection(ProjectionType type);
  void zoom(int delta, bool relative);
  [[nodiscard]] double zoomValue() const;
  [[nodiscard]] double fovValue() const;
  void resetView();
  void updateView(const std::shared_ptr<const class FileContext>& context, bool enableWarning);
  void viewAll(const BoundingBox& bbox);
  [[nodiscard]] std::string statusText() const;

  // accessors to get and set camera settings in the user space format (different for historical reasons)
  [[nodiscard]] Eigen::Vector3d getVpt() const;
  void setVpt(double x, double y, double z);
  [[nodiscard]] Eigen::Vector3d getVpr() const;
  void setVpr(double x, double y, double z);
  void setVpd(double d);
  void setVpf(double d);

  // Gimbalcam
  Eigen::Vector3d object_trans;
  Eigen::Vector3d object_rot;

  // Perspective settings
  double fov;  // Field of view

  // true if camera should try to view everything in a given
  // bounding box.
  bool viewall{false};

  // true if camera should point at center of bounding box
  // (normally it points at 0,0,0 or at given coordinates)
  bool autocenter{false};

  unsigned int pixel_width;
  unsigned int pixel_height;

  // true if camera settings are fixed
  // (--camera option in commandline mode)
  bool locked;

  // Perspective settings
  double viewer_distance;

  // Optional explicit near/far clip planes (world units). When
  // clip_far > clip_near > 0 they override the default 0.1*dist..100*dist for
  // the PERSPECTIVE projection. Used by offscreen camera snapshots, where the
  // camera can sit close to (or inside) the model and the default near plane
  // would clip geometry in front of the camera.
  double clip_near{-1.0};
  double clip_far{-1.0};
};
