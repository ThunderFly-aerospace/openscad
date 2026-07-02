#include "utils/camera_math.h"

#include <catch2/catch_all.hpp>
#include <cmath>

using Catch::Approx;
using Eigen::Vector3d;
using Eigen::Vector4d;
using Eigen::Matrix4d;

namespace {

Vector3d applyPoint(const Matrix4d& m, const Vector3d& p)
{
  const Vector4d r = m * Vector4d(p.x(), p.y(), p.z(), 1.0);
  return r.head<3>();
}

// Check that the gimbal reproduces a look-at from `eye` along `forward` with `up`.
void checkPose(const Vector3d& eye, const Vector3d& forward, const Vector3d& up,
               double distance = 140.0)
{
  const auto g = camera_math::gimbalFromPose(eye, forward, up, distance);
  const Matrix4d mv = camera_math::modelviewFromGimbal(g);

  const Vector3d f = forward.normalized();
  const Vector3d s = f.cross(up).normalized();  // right
  const Vector3d u = s.cross(f);                // orthonormal up

  const Vector3d viewEye = applyPoint(mv, eye);
  const Vector3d viewFwd = applyPoint(mv, eye + f) - viewEye;
  const Vector3d viewUp = applyPoint(mv, eye + u) - viewEye;
  const Vector3d viewRight = applyPoint(mv, eye + s) - viewEye;

  // Eye maps to the view-space origin.
  CHECK(viewEye.x() == Approx(0.0).margin(1e-9));
  CHECK(viewEye.y() == Approx(0.0).margin(1e-9));
  CHECK(viewEye.z() == Approx(0.0).margin(1e-9));

  // Forward maps to view -Z, up to view +Y, right to view +X.
  CHECK(viewFwd.x() == Approx(0.0).margin(1e-9));
  CHECK(viewFwd.y() == Approx(0.0).margin(1e-9));
  CHECK(viewFwd.z() == Approx(-1.0).margin(1e-9));

  CHECK(viewUp.x() == Approx(0.0).margin(1e-9));
  CHECK(viewUp.y() == Approx(1.0).margin(1e-9));
  CHECK(viewUp.z() == Approx(0.0).margin(1e-9));

  CHECK(viewRight.x() == Approx(1.0).margin(1e-9));
  CHECK(viewRight.y() == Approx(0.0).margin(1e-9));
  CHECK(viewRight.z() == Approx(0.0).margin(1e-9));
}

}  // namespace

TEST_CASE("focalToFovDegrees matches the lens formula", "[camera_math]")
{
  // fov = 2*atan(sensor/(2*focal))
  CHECK(camera_math::focalToFovDegrees(12.0, 24.0) == Approx(90.0));          // sensor/2f = 1
  CHECK(camera_math::focalToFovDegrees(35.0, 24.0) == Approx(37.849627).margin(1e-4));
  CHECK(camera_math::focalToFovDegrees(50.0, 24.0) == Approx(26.991467).margin(1e-4));
  // Longer lens -> narrower fov.
  CHECK(camera_math::focalToFovDegrees(100.0, 24.0) <
        camera_math::focalToFovDegrees(35.0, 24.0));
}

TEST_CASE("gimbalFromPose reproduces axis-aligned look-at", "[camera_math]")
{
  checkPose({0, 0, 0}, {0, 0, -1}, {0, 1, 0});   // default OpenGL-ish
  checkPose({0, 0, 0}, {0, 1, 0}, {0, 0, 1});    // looking +Y, up +Z (OpenSCAD default-ish)
  checkPose({10, 20, 30}, {1, 0, 0}, {0, 0, 1}); // off-origin, looking +X
}

TEST_CASE("gimbalFromPose handles arbitrary direction and roll", "[camera_math]")
{
  checkPose({5, -7, 3}, {1, 2, 3}, {0, 0, 1});
  // Rolled up vector (not orthogonal to forward is fine, it gets orthonormalized).
  checkPose({1, 1, 1}, {1, 0, -1}, {0.3, 1, 0.2});
  checkPose({-4, 8, 2}, {-2, 1, -0.5}, {0.1, 0.1, 1});
}

TEST_CASE("gimbalFromMatrix uses local -Z forward and +Y up", "[camera_math]")
{
  // Marker translated to (20,0,5) then rotated 45deg about Z.
  const double a = 45.0 * M_PI / 180.0;
  Matrix4d world = Matrix4d::Identity();
  world.block<3, 3>(0, 0) = Eigen::AngleAxisd(a, Vector3d::UnitZ()).toRotationMatrix();
  world.block<3, 1>(0, 3) = Vector3d(20, 0, 5);

  const auto g = camera_math::gimbalFromMatrix(world, 140.0);
  const Matrix4d mv = camera_math::modelviewFromGimbal(g);

  const Vector3d eye(20, 0, 5);
  const Vector3d localForward = world.block<3, 3>(0, 0) * Vector3d(0, 0, -1);

  const Vector3d viewEye = applyPoint(mv, eye);
  const Vector3d viewFwd = applyPoint(mv, eye + localForward) - viewEye;

  CHECK(viewEye.norm() == Approx(0.0).margin(1e-9));
  CHECK(viewFwd.x() == Approx(0.0).margin(1e-9));
  CHECK(viewFwd.y() == Approx(0.0).margin(1e-9));
  CHECK(viewFwd.z() == Approx(-1.0).margin(1e-9));
}
