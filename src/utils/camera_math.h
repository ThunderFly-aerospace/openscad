#pragma once

#include <Eigen/Core>
#include <Eigen/Geometry>

/*
   Pure (GL-free, Camera-free) math for the camera() marker feature.

   Kept in src/utils so the unit-test glob (src/utils/*_test.cc) picks up the
   tests, and so the tricky world-matrix -> OpenSCAD gimbal conversion can be
   validated deterministically without a GL context.
 */
namespace camera_math {

// Vertical field of view (degrees) from focal length and vertical sensor size
// (both in mm): fov = 2*atan(sensor / (2*focal)).
double focalToFovDegrees(double focalMM, double sensorMM);

// OpenSCAD "gimbal" camera parameters (see glview/Camera.h).
struct Gimbal {
  Eigen::Vector3d translation;  // Camera::object_trans
  Eigen::Vector3d rotation;     // Camera::object_rot, Euler XYZ in degrees
  double distance;              // Camera::viewer_distance
};

// Convert a camera pose (world eye position, forward = viewing direction, up)
// into gimbal parameters reproducing a look-at view. The eye world position is
// preserved for any positive `distance` (distance only affects clip planes /
// orthographic scale, not the perspective framing).
Gimbal gimbalFromPose(const Eigen::Vector3d& eye, const Eigen::Vector3d& forward,
                      const Eigen::Vector3d& up, double distance);

// Convert a 4x4 world transform of a camera marker into gimbal parameters.
// Convention: the marker looks along its local -Z with local +Y as up.
Gimbal gimbalFromMatrix(const Eigen::Matrix4d& world, double distance);

// Reconstruct the modelview matrix that GLView::setupCamera() would build from
// the given gimbal parameters. Exposed mainly so tests can verify the round
// trip (pose -> gimbal -> modelview) independent of OpenGL.
Eigen::Matrix4d modelviewFromGimbal(const Gimbal& g);

}  // namespace camera_math
