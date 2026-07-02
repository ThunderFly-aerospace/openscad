#include "utils/camera_math.h"

#include <cmath>

namespace camera_math {

namespace {

// Rotation part of the fixed base view L = lookAt(eye=(0,-d,0), center=0, up=+Z)
// used by GLView::setupCamera(). Maps world axes to view axes:
//   world +X -> view +X, world +Y -> view -Z (forward), world +Z -> view +Y.
Eigen::Matrix3d baseViewRotation()
{
  Eigen::Matrix3d rl;
  rl << 1, 0, 0,
        0, 0, 1,
        0, -1, 0;
  return rl;
}

Eigen::Matrix4d translation4(const Eigen::Vector3d& t)
{
  Eigen::Matrix4d m = Eigen::Matrix4d::Identity();
  m.block<3, 1>(0, 3) = t;
  return m;
}

Eigen::Matrix4d rotation4(const Eigen::Matrix3d& r)
{
  Eigen::Matrix4d m = Eigen::Matrix4d::Identity();
  m.block<3, 3>(0, 0) = r;
  return m;
}

}  // namespace

double focalToFovDegrees(double focalMM, double sensorMM)
{
  const double fovRad = 2.0 * std::atan2(sensorMM, 2.0 * focalMM);
  return fovRad * 180.0 / M_PI;
}

Gimbal gimbalFromPose(const Eigen::Vector3d& eye, const Eigen::Vector3d& forward,
                      const Eigen::Vector3d& up, double distance)
{
  const Eigen::Vector3d f = forward.normalized();
  const Eigen::Vector3d s = f.cross(up).normalized();  // right
  const Eigen::Vector3d u = s.cross(f);                // orthonormal up

  // World -> view rotation of a look-at view (rows are the view axes).
  Eigen::Matrix3d rview;
  rview.row(0) = s;
  rview.row(1) = u;
  rview.row(2) = -f;

  // setupCamera builds modelview = L * Rx*Ry*Rz * Translate(object_trans).
  // Rotation match: R_L * (Rx*Ry*Rz) = rview  =>  Rx*Ry*Rz = R_L^T * rview.
  const Eigen::Matrix3d q = baseViewRotation().transpose() * rview;
  const Eigen::Vector3d euler = q.eulerAngles(0, 1, 2);  // XYZ, radians

  Gimbal g;
  g.rotation = euler * (180.0 / M_PI);
  g.distance = distance;
  // Translation match (see derivation): object_trans = -(eye + distance*forward),
  // which keeps the eye fixed at `eye` for any positive distance.
  g.translation = -(eye + distance * f);
  return g;
}

Gimbal gimbalFromMatrix(const Eigen::Matrix4d& world, double distance)
{
  const Eigen::Matrix3d r = world.block<3, 3>(0, 0);
  const Eigen::Vector3d eye = world.block<3, 1>(0, 3);
  const Eigen::Vector3d forward = r * Eigen::Vector3d(0, 0, -1);
  const Eigen::Vector3d up = r * Eigen::Vector3d(0, 1, 0);
  return gimbalFromPose(eye, forward, up, distance);
}

Eigen::Matrix4d modelviewFromGimbal(const Gimbal& g)
{
  const double d = g.distance;
  // L = lookAt((0,-d,0), 0, +Z): rotation R_L, translation (0,0,-d).
  Eigen::Matrix4d l = rotation4(baseViewRotation());
  l.block<3, 1>(0, 3) = Eigen::Vector3d(0, 0, -d);

  const double deg = M_PI / 180.0;
  const Eigen::Matrix3d rx =
    Eigen::AngleAxisd(g.rotation.x() * deg, Eigen::Vector3d::UnitX()).toRotationMatrix();
  const Eigen::Matrix3d ry =
    Eigen::AngleAxisd(g.rotation.y() * deg, Eigen::Vector3d::UnitY()).toRotationMatrix();
  const Eigen::Matrix3d rz =
    Eigen::AngleAxisd(g.rotation.z() * deg, Eigen::Vector3d::UnitZ()).toRotationMatrix();

  return l * rotation4(rx) * rotation4(ry) * rotation4(rz) * translation4(g.translation);
}

}  // namespace camera_math
