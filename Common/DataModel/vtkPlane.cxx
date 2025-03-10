// SPDX-FileCopyrightText: Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
// SPDX-License-Identifier: BSD-3-Clause
#include "vtkPlane.h"

#include <vtkPoints.h>

#include "vtkArrayDispatch.h"
#include "vtkDataArrayRange.h"
#include "vtkMath.h"
#include "vtkObjectFactory.h"
#include "vtkSMPThreadLocal.h"
#include "vtkSMPTools.h"

#include <algorithm>
#include <array>

VTK_ABI_NAMESPACE_BEGIN
vtkStandardNewMacro(vtkPlane);

//------------------------------------------------------------------------------
void vtkPlane::ComputeInternalNormal()
{
  if (this->AxisAligned)
  {
    this->InternalNormal[0] = std::fabs(this->Normal[0]) >= std::fabs(this->Normal[1]) &&
        std::fabs(this->Normal[0]) >= std::fabs(this->Normal[2])
      ? 1.0
      : 0.0;
    this->InternalNormal[1] = std::fabs(this->Normal[1]) >= std::fabs(this->Normal[0]) &&
        std::fabs(this->Normal[1]) >= std::fabs(this->Normal[2])
      ? 1.0
      : 0.0;
    this->InternalNormal[2] = std::fabs(this->Normal[2]) >= std::fabs(this->Normal[0]) &&
        std::fabs(this->Normal[2]) >= std::fabs(this->Normal[1])
      ? 1.0
      : 0.0;
  }
  else
  {
    this->InternalNormal[0] = this->Normal[0];
    this->InternalNormal[1] = this->Normal[1];
    this->InternalNormal[2] = this->Normal[2];
  }
}

//------------------------------------------------------------------------------
void vtkPlane::ComputeInternalOrigin()
{
  this->InternalOrigin[0] = this->Origin[0];
  this->InternalOrigin[1] = this->Origin[1];
  this->InternalOrigin[2] = this->Origin[2];
  if (this->Offset != 0.0)
  {
    for (int i = 0; i < 3; i++)
    {
      this->InternalOrigin[i] += this->Offset * this->InternalNormal[i];
    }
  }
}

//------------------------------------------------------------------------------
void vtkPlane::InternalUpdates()
{
  this->ComputeInternalNormal();
  this->ComputeInternalOrigin();
}

//------------------------------------------------------------------------------
void vtkPlane::SetOrigin(double x, double y, double z)
{
  if ((this->Origin[0] != x) || (this->Origin[1] != y) || (this->Origin[2] != z))
  {
    this->Origin[0] = x;
    this->Origin[1] = y;
    this->Origin[2] = z;
    this->Modified();
    this->InternalUpdates();
  }
}

//------------------------------------------------------------------------------
void vtkPlane::SetOrigin(const double origin[3])
{
  this->SetOrigin(origin[0], origin[1], origin[2]);
}

//------------------------------------------------------------------------------
void vtkPlane::SetNormal(double x, double y, double z)
{
  if ((this->Normal[0] != x) || (this->Normal[1] != y) || (this->Normal[2] != z))
  {
    this->Normal[0] = x;
    this->Normal[1] = y;
    this->Normal[2] = z;
    this->Modified();
    this->InternalUpdates();
  }
}

//------------------------------------------------------------------------------
void vtkPlane::SetNormal(const double normal[3])
{
  this->SetNormal(normal[0], normal[1], normal[2]);
}

//------------------------------------------------------------------------------
void vtkPlane::SetOffset(double _arg)
{
  if (this->Offset != _arg)
  {
    this->Offset = _arg;
    this->Modified();
    this->InternalUpdates();
  }
}

//------------------------------------------------------------------------------
void vtkPlane::SetAxisAligned(bool _arg)
{
  if (this->AxisAligned != _arg)
  {
    this->AxisAligned = _arg;
    this->Modified();
    this->InternalUpdates();
  }
}

//------------------------------------------------------------------------------
void vtkPlane::DeepCopy(vtkPlane* plane)
{
  this->SetNormal(plane->GetNormal());
  this->SetOrigin(plane->GetOrigin());
  this->SetAxisAligned(plane->GetAxisAligned());
  this->SetOffset(plane->GetOffset());
}

//------------------------------------------------------------------------------
double vtkPlane::DistanceToPlane(double x[3])
{
  return this->DistanceToPlane(x, this->GetNormal(), this->GetOrigin());
}

//------------------------------------------------------------------------------
void vtkPlane::ProjectPoint(
  const double x[3], const double origin[3], const double normal[3], double xproj[3])
{
  double t, xo[3];

  xo[0] = x[0] - origin[0];
  xo[1] = x[1] - origin[1];
  xo[2] = x[2] - origin[2];

  t = vtkMath::Dot(normal, xo);

  xproj[0] = x[0] - t * normal[0];
  xproj[1] = x[1] - t * normal[1];
  xproj[2] = x[2] - t * normal[2];
}

//------------------------------------------------------------------------------
void vtkPlane::ProjectPoint(const double x[3], double xproj[3])
{
  this->ProjectPoint(x, this->GetOrigin(), this->GetNormal(), xproj);
}

//------------------------------------------------------------------------------
void vtkPlane::ProjectVector(
  const double v[3], const double vtkNotUsed(origin)[3], const double normal[3], double vproj[3])
{
  double t = vtkMath::Dot(v, normal);
  double n2 = vtkMath::Dot(normal, normal);
  if (n2 == 0)
  {
    n2 = 1.0;
  }
  vproj[0] = v[0] - t * normal[0] / n2;
  vproj[1] = v[1] - t * normal[1] / n2;
  vproj[2] = v[2] - t * normal[2] / n2;
}

//------------------------------------------------------------------------------
void vtkPlane::ProjectVector(const double v[3], double vproj[3])
{
  this->ProjectVector(v, this->GetOrigin(), this->GetNormal(), vproj);
}

//------------------------------------------------------------------------------
void vtkPlane::Push(double distance)
{
  int i;

  if (distance == 0.0)
  {
    return;
  }

  this->ComputeInternalNormal();
  for (i = 0; i < 3; i++)
  {
    this->Origin[i] += distance * this->InternalNormal[i];
  }
  this->ComputeInternalOrigin();
  this->Modified();
}

//------------------------------------------------------------------------------
// Project a point x onto plane defined by origin and normal. The
// projected point is returned in xproj. NOTE : normal NOT required to
// have magnitude 1.
void vtkPlane::GeneralizedProjectPoint(
  const double x[3], const double origin[3], const double normal[3], double xproj[3])
{
  double t, xo[3], n2;

  xo[0] = x[0] - origin[0];
  xo[1] = x[1] - origin[1];
  xo[2] = x[2] - origin[2];

  t = vtkMath::Dot(normal, xo);
  n2 = vtkMath::SquaredNorm(normal);

  if (n2 != 0)
  {
    xproj[0] = x[0] - t * normal[0] / n2;
    xproj[1] = x[1] - t * normal[1] / n2;
    xproj[2] = x[2] - t * normal[2] / n2;
  }
  else
  {
    xproj[0] = x[0];
    xproj[1] = x[1];
    xproj[2] = x[2];
  }
}

//------------------------------------------------------------------------------
void vtkPlane::GeneralizedProjectPoint(const double x[3], double xproj[3])
{
  this->GeneralizedProjectPoint(x, this->GetOrigin(), this->GetNormal(), xproj);
}

//------------------------------------------------------------------------------
// Evaluate plane equation for point x[3].
double vtkPlane::EvaluateFunction(double x[3])
{
  return (this->InternalNormal[0] * (x[0] - this->InternalOrigin[0]) +
    this->InternalNormal[1] * (x[1] - this->InternalOrigin[1]) +
    this->InternalNormal[2] * (x[2] - this->InternalOrigin[2]));
}

//------------------------------------------------------------------------------
// Evaluate function gradient at point x[3].
void vtkPlane::EvaluateGradient(double vtkNotUsed(x)[3], double n[3])
{
  for (int i = 0; i < 3; i++)
  {
    n[i] = this->InternalNormal[i];
  }
}

#define VTK_PLANE_TOL 1.0e-06

//------------------------------------------------------------------------------
// Given a line defined by the two points p1,p2; and a plane defined by the
// normal n and point p0, compute an intersection. The parametric
// coordinate along the line is returned in t, and the coordinates of
// intersection are returned in x. A zero is returned if the plane and line
// do not intersect between (0<=t<=1). If the plane and line are parallel,
// zero is returned and t is set to VTK_LARGE_DOUBLE.
int vtkPlane::IntersectWithLine(
  const double p1[3], const double p2[3], double n[3], double p0[3], double& t, double x[3])
{
  double num, den, p21[3];
  double fabsden, fabstolerance;

  // Compute line vector
  //
  p21[0] = p2[0] - p1[0];
  p21[1] = p2[1] - p1[1];
  p21[2] = p2[2] - p1[2];

  // Compute denominator.  If ~0, line and plane are parallel.
  //
  num = vtkMath::Dot(n, p0) - (n[0] * p1[0] + n[1] * p1[1] + n[2] * p1[2]);
  den = n[0] * p21[0] + n[1] * p21[1] + n[2] * p21[2];
  //
  // If denominator with respect to numerator is "zero", then the line and
  // plane are considered parallel.
  //

  // trying to avoid an expensive call to fabs()
  if (den < 0.0)
  {
    fabsden = -den;
  }
  else
  {
    fabsden = den;
  }
  if (num < 0.0)
  {
    fabstolerance = -num * VTK_PLANE_TOL;
  }
  else
  {
    fabstolerance = num * VTK_PLANE_TOL;
  }
  if (fabsden <= fabstolerance)
  {
    t = VTK_DOUBLE_MAX;
    return 0;
  }

  // valid intersection
  t = num / den;

  x[0] = p1[0] + t * p21[0];
  x[1] = p1[1] + t * p21[1];
  x[2] = p1[2] + t * p21[2];

  if (t >= 0.0 && t <= 1.0)
  {
    return 1;
  }
  else
  {
    return 0;
  }
}

// Accelerate plane cutting operation
namespace
{
template <typename InputArrayType, typename OutputArrayType>
struct CutWorker
{
  using OutputValueType = vtk::GetAPIType<OutputArrayType>;

  InputArrayType* Input;
  OutputArrayType* Output;
  OutputValueType Normal[3];
  OutputValueType Origin[3];

  CutWorker(InputArrayType* in, OutputArrayType* out)
    : Input(in)
    , Output(out)
  {
  }
  void operator()(vtkIdType begin, vtkIdType end)
  {
    const auto srcTuples = vtk::DataArrayTupleRange<3>(this->Input);
    auto dstValues = vtk::DataArrayValueRange<1>(this->Output);

    double tuple[3];
    for (vtkIdType pointId = begin; pointId < end; ++pointId)
    {
      // GetTuple creates a copy of the tuple using GetTypedTuple if it's not a vktDataArray
      // we do that since the input points can be implicit points, and GetTypedTuple is faster
      // than accessing the component of the TupleReference using GetTypedComponent internally.
      srcTuples.GetTuple(pointId, tuple);
      dstValues[pointId] = this->Normal[0] * (tuple[0] - this->Origin[0]) +
        this->Normal[1] * (tuple[1] - this->Origin[1]) +
        this->Normal[2] * (tuple[2] - this->Origin[2]);
    }
  }
};

struct CutFunctionWorker
{
  double Normal[3];
  double Origin[3];
  CutFunctionWorker(double n[3], double o[3])
  {
    std::copy_n(n, 3, this->Normal);
    std::copy_n(o, 3, this->Origin);
  }
  template <typename InputArrayType, typename OutputArrayType>
  void operator()(InputArrayType* input, OutputArrayType* output)
  {
    VTK_ASSUME(input->GetNumberOfComponents() == 3);
    VTK_ASSUME(output->GetNumberOfComponents() == 1);
    vtkIdType numTuples = input->GetNumberOfTuples();
    CutWorker<InputArrayType, OutputArrayType> cut(input, output);
    std::copy_n(this->Normal, 3, cut.Normal);
    std::copy_n(this->Origin, 3, cut.Origin);
    vtkSMPTools::For(0, numTuples, cut);
  }
};
} // end anon namespace

//------------------------------------------------------------------------------
void vtkPlane::EvaluateFunction(vtkDataArray* input, vtkDataArray* output)
{
  CutFunctionWorker worker(this->InternalNormal, this->InternalOrigin);
  typedef vtkTypeList::Create<float, double> InputTypes;
  typedef vtkTypeList::Create<float, double> OutputTypes;
  typedef vtkArrayDispatch::Dispatch2ByValueTypeUsingArrays<vtkArrayDispatch::AllArrays, InputTypes,
    OutputTypes>
    MyDispatch;
  if (!MyDispatch::Execute(input, output, worker))
  {
    worker(input, output); // Use vtkDataArray API if dispatch fails.
  }
}

//------------------------------------------------------------------------------
int vtkPlane::IntersectWithLine(const double p1[3], const double p2[3], double& t, double x[3])
{
  return this->IntersectWithLine(p1, p2, this->GetNormal(), this->GetOrigin(), t, x);
}

//------------------------------------------------------------------------------
int vtkPlane::IntersectWithFinitePlane(double n[3], double o[3], double pOrigin[3], double px[3],
  double py[3], double x0[3], double x1[3])
{
  // Since we are dealing with convex shapes, if there is an intersection a
  // single line is produced as output. So all this is necessary is to
  // intersect the four bounding lines of the finite line and find the two
  // intersection points.
  int numInts = 0;
  double t, *x = x0;
  double xr0[3], xr1[3];

  // First line
  xr0[0] = pOrigin[0];
  xr0[1] = pOrigin[1];
  xr0[2] = pOrigin[2];
  xr1[0] = px[0];
  xr1[1] = px[1];
  xr1[2] = px[2];
  if (vtkPlane::IntersectWithLine(xr0, xr1, n, o, t, x))
  {
    numInts++;
    x = x1;
  }

  // Second line
  xr1[0] = py[0];
  xr1[1] = py[1];
  xr1[2] = py[2];
  if (vtkPlane::IntersectWithLine(xr0, xr1, n, o, t, x))
  {
    numInts++;
    x = x1;
  }
  if (numInts == 2)
  {
    return 1;
  }

  // Third line
  xr0[0] = px[0] + py[0] - pOrigin[0];
  xr0[1] = px[1] + py[1] - pOrigin[1];
  xr0[2] = px[2] + py[2] - pOrigin[2];
  if (vtkPlane::IntersectWithLine(xr0, xr1, n, o, t, x))
  {
    numInts++;
    x = x1;
  }
  if (numInts == 2)
  {
    return 1;
  }

  // Fourth and last line
  xr1[0] = px[0];
  xr1[1] = px[1];
  xr1[2] = px[2];
  if (vtkPlane::IntersectWithLine(xr0, xr1, n, o, t, x))
  {
    numInts++;
  }
  if (numInts == 2)
  {
    return 1;
  }

  // No intersection has occurred, or a single degenerate point
  return 0;
}

namespace
{ // anonymous
// This code supports the method ComputeBestFittingPlane()

// Determine the origin of the points.
struct ComputePointsOrigin
{
  vtkPoints* Points;
  double Origin[3];
  vtkSMPThreadLocal<std::array<double, 3>> Sum;

  ComputePointsOrigin(vtkPoints* pts)
    : Points(pts)
    , Origin{ 0, 0, 0 }
  {
  }
  void GetOrigin(double origin[3]) { std::copy_n(this->Origin, 3, origin); }

  // Initialize thread average
  void Initialize() { this->Sum.Local().fill(0); }

  // Sum up coordinates
  void operator()(vtkIdType ptId, vtkIdType endPtId)
  {
    double* sum = this->Sum.Local().data();
    double x[3];
    for (; ptId < endPtId; ++ptId)
    {
      this->Points->GetPoint(ptId, x);
      vtkMath::Add(sum, x, sum);
    }
  }

  // Compose the coordinate sums and then average them (to
  // compute the origin).
  void Reduce()
  {
    double sum[3] = { 0, 0, 0 };
    auto iEnd = this->Sum.end();
    for (auto itr = this->Sum.begin(); itr != iEnd; ++itr)
    {
      vtkMath::Add(sum, (*itr).data(), sum);
    }

    vtkIdType npts = this->Points->GetNumberOfPoints();
    vtkMath::Add(this->Origin, sum, this->Origin);
    vtkMath::MultiplyScalar(this->Origin, (1.0 / npts));
  }
};

// Determine the points covariance matrix
struct ComputeCovariance
{
  vtkPoints* Points;
  const double Origin[3];
  double Covariance[6];
  vtkSMPThreadLocal<std::array<double, 6>> Sum;

  ComputeCovariance(vtkPoints* pts, double origin[3])
    : Points(pts)
    , Origin{ origin[0], origin[1], origin[2] }
    , Covariance{ 0, 0, 0, 0, 0, 0 }
  {
  }

  void GetCovariance(double& xx, double& xy, double& xz, double& yy, double& yz, double& zz)
  {
    xx = this->Covariance[0];
    xy = this->Covariance[1];
    xz = this->Covariance[2];
    yy = this->Covariance[3];
    yz = this->Covariance[4];
    zz = this->Covariance[5];
  }

  void Initialize() { this->Sum.Local().fill(0); }

  void operator()(vtkIdType ptId, vtkIdType endPtId)
  {
    const double* origin = this->Origin;
    double r[3], x[3];
    double* sum = this->Sum.Local().data();

    for (; ptId < endPtId; ++ptId)
    {
      this->Points->GetPoint(ptId, x);
      vtkMath::Subtract(x, origin, r);
      sum[0] += r[0] * r[0];
      sum[1] += r[0] * r[1];
      sum[2] += r[0] * r[2];
      sum[3] += r[1] * r[1];
      sum[4] += r[1] * r[2];
      sum[5] += r[2] * r[2];
    }
  }

  void Reduce()
  {
    double cov[6] = { 0, 0, 0, 0, 0, 0 };
    auto iEnd = this->Sum.end();
    for (auto itr = this->Sum.begin(); itr != iEnd; ++itr)
    {
      double* sum = (*itr).data();
      for (auto i = 0; i < 6; ++i)
      {
        cov[i] += sum[i];
      }
    }

    vtkIdType npts = this->Points->GetNumberOfPoints();
    for (auto i = 0; i < 6; ++i)
    {
      this->Covariance[i] = cov[i] / npts;
    }
  }
};

} // anonymous

//------------------------------------------------------------------------------
// Threaded implementation to fit plane to set of points.
bool vtkPlane::ComputeBestFittingPlane(vtkPoints* pts, double* origin, double* normal)
{
  //
  // Note:
  // For details see https://www.ilikebigbits.com/2017_09_25_plane_from_points_2.html
  //

  origin[0] = 0;
  origin[1] = 0;
  origin[2] = 0;

  normal[0] = 0;
  normal[1] = 0;
  normal[2] = 1; // default normal direction

  vtkIdType npts = pts->GetNumberOfPoints();
  if (npts < 3)
  {
    return false;
  }

  // 1. Calculate the centroid of the points; this will become origin. Thread the
  // operation of the number of points is large.
  ComputePointsOrigin computeOrigin(pts);
  // We use THRESHOLD to test if the data size is small enough
  // to execute the functor serially.
  vtkSMPTools::For(0, npts, vtkSMPTools::THRESHOLD, computeOrigin);

  computeOrigin.GetOrigin(origin);

  // 2. Calculate the covariance matrix of the points relative to the centroid.
  ComputeCovariance computeCovariance(pts, origin);
  // We use THRESHOLD to test if the data size is small enough
  // to execute the functor serially.
  vtkSMPTools::For(0, npts, vtkSMPTools::THRESHOLD, computeCovariance);

  double xx, xy, xz, yy, yz, zz;
  computeCovariance.GetCovariance(xx, xy, xz, yy, yz, zz);

  // 3. Do linear regression along the X, Y and Z axis
  // 4. Weight he result of the linear regressions based on the square of the determinant
  double weighted_dir[3] = { 0, 0, 0 };
  {
    double det_x = yy * zz - yz * yz;
    double axis_dir[3] = { det_x, xz * yz - xy * zz, xy * yz - xz * yy };
    double weight = det_x * det_x;
    if (vtkMath::Dot(weighted_dir, axis_dir) < 0.0)
    {
      weight = -weight;
    }
    vtkMath::MultiplyScalar(axis_dir, weight);
    vtkMath::Add(weighted_dir, axis_dir, weighted_dir);
  }
  {
    double det_y = xx * zz - xz * xz;
    double axis_dir[3] = { xz * yz - xy * zz, det_y, xy * xz - yz * xx };
    double weight = det_y * det_y;
    if (vtkMath::Dot(weighted_dir, axis_dir) < 0.0)
    {
      weight = -weight;
    }
    vtkMath::MultiplyScalar(axis_dir, weight);
    vtkMath::Add(weighted_dir, axis_dir, weighted_dir);
  }
  {
    double det_z = xx * yy - xy * xy;
    double axis_dir[3] = { xy * yz - xz * yy, xy * xz - yz * xx, det_z };
    double weight = det_z * det_z;
    if (vtkMath::Dot(weighted_dir, axis_dir) < 0.0)
    {
      weight = -weight;
    }
    vtkMath::MultiplyScalar(axis_dir, weight);
    vtkMath::Add(weighted_dir, axis_dir, weighted_dir);
  }

  // normalize the weighted direction
  double nrm = vtkMath::Normalize(weighted_dir);

  // if weighted_dir is faulty then exit here without altering the default normal direction
  if (!vtkMath::IsFinite(nrm) || (nrm == 0))
  {
    return false;
  }

  // use weighted direction as normal vector
  normal[0] = weighted_dir[0];
  normal[1] = weighted_dir[1];
  normal[2] = weighted_dir[2];

  return true;
}

//------------------------------------------------------------------------------
int vtkPlane::IntersectWithFinitePlane(
  double pOrigin[3], double px[3], double py[3], double x0[3], double x1[3])
{
  return this->IntersectWithFinitePlane(
    this->GetNormal(), this->GetOrigin(), pOrigin, px, py, x0, x1);
}

//------------------------------------------------------------------------------
void vtkPlane::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "Normal: (" << this->Normal[0] << ", " << this->Normal[1] << ", "
     << this->Normal[2] << ")\n";
  os << indent << "Origin: (" << this->Origin[0] << ", " << this->Origin[1] << ", "
     << this->Origin[2] << ")\n";
  os << indent << "Offset: " << this->Offset << endl;
  os << indent << "AxisAligned: " << (this->AxisAligned ? "On" : "Off") << endl;
}
VTK_ABI_NAMESPACE_END
