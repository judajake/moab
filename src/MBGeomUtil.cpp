/*
 * MOAB, a Mesh-Oriented datABase, is a software component for creating,
 * storing and accessing finite element mesh data.
 * 
 * Copyright 2004 Sandia Corporation.  Under the terms of Contract
 * DE-AC04-94AL85000 with Sandia Coroporation, the U.S. Government
 * retains certain rights in this software.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 */

/**\file MBGeometry.cpp
 *\author Jason Kraftcheck (kraftche@cae.wisc.edu)
 *\date 2006-07-27
 */

#include "MBCartVect.hpp"
#include "MBCN.hpp"
#include "MBGeomUtil.hpp"
#include "MBMatrix3.hpp"
#include <cmath>
#include <algorithm>
#include <assert.h>
#include <iostream>
#include <limits>

#if defined(_MSC_VER) || defined(__MINGW32__)
#  include <float.h>
#  define finite(A) _finite(A)
#endif

namespace MBGeomUtil {

static inline 
void min_max_3( double a, double b, double c, double& min, double& max )
{
  if (a < b) {
    if (a < c) {
      min = a;
      max = b > c ? b : c;
    }
    else {
      min = c;
      max = b;
    }
  }
  else if (b < c) {
    min = b;
    max = a > c ? a : c;
  }
  else {
    min = c;
    max = a;
  }
}

static inline
double dot_abs( const MBCartVect& u, const MBCartVect& v )
  { return fabs(u[0]*v[0]) + fabs(u[1]*v[1]) + fabs(u[2]*v[2]); }

bool segment_box_intersect( MBCartVect box_min,
                            MBCartVect box_max,
                            const MBCartVect& seg_pt,
                            const MBCartVect& seg_unit_dir,
                            double& seg_start, double& seg_end )
{
    // translate so that seg_pt is at origin
  box_min -= seg_pt;
  box_max -= seg_pt;
  
  for (unsigned i = 0; i < 3; ++i) {  // X, Y, and Z slabs

      // intersect line with slab planes
    const double t_min = box_min[i] / seg_unit_dir[i];
    const double t_max = box_max[i] / seg_unit_dir[i];
    
      // check if line is parallel to planes
    if (!finite(t_min)) {
      if (box_min[i] > 0.0 || box_max[i] < 0.0)
        return false; 
      continue;
    }

    if (seg_unit_dir[i] < 0) {
      if (t_min < seg_end) 
        seg_end = t_min;
      if (t_max > seg_start)
        seg_start = t_max;
    }
    else { // seg_unit_dir[i] > 0
      if (t_min > seg_start)
        seg_start = t_min; 
      if (t_max < seg_end)
        seg_end = t_max;
    }
  }

  return seg_start <= seg_end;
}


/* Impelementation copied from cgmMC ray_tri_contact (overlap.C) */
bool ray_tri_intersect( const MBCartVect vertices[3],
                        const MBCartVect& b,
                        const MBCartVect& v,
                        double /*tolerance*/,
                        double& t_out,
                        const double* ray_length)
{
  const MBCartVect p0 = vertices[0] - vertices[1]; // abc
  const MBCartVect p1 = vertices[0] - vertices[2]; // def
                                                   // ghi<-v
  const MBCartVect p = vertices[0] - b;            // jkl
  const MBCartVect c = p1 * v;                     // eiMinushf,gfMinusdi,dhMinuseg
  const double mP = p0 % c;
  const double betaP = p % c;
  if (mP > 0) {
    if (betaP < 0)
      return false;
  }
  else if (mP < 0) {
    if (betaP > 0)
      return false;
  }
  else {
    return false;
  }
  
  const MBCartVect d = p0 * p; // jcMinusal,blMinuskc,akMinusjb
  double gammaP = v % d;
  if (mP > 0) {
    if (gammaP < 0 || betaP + gammaP > mP)
      return false;
  }
  else if (betaP + gammaP < mP || gammaP > 0)
    return false;
  
  const double tP = p1 % d;
  const double m = 1.0 / mP;
  const double beta = betaP * m;
  const double gamma = gammaP * m;
  const double t = -tP * m;
  if (ray_length && t > *ray_length)
    return false;
  
  if (beta < 0 || gamma < 0 ||
      beta + gamma > 1 ||
      t < 0.0)
    return false;
  
  t_out = t;
  return true;
}

bool ray_box_intersect( const MBCartVect& box_min,
                        const MBCartVect& box_max,
                        const MBCartVect& ray_pt,
                        const MBCartVect& ray_dir,
                        double& t_enter, double& t_exit )
{
  const double epsilon = 1e-12;
  double t1, t2;

  // Use 'slabs' method from 13.6.1 of Akenine-Moller
  t_enter = 0.0;
  t_exit  = std::numeric_limits<double>::infinity();
  
  // Intersect with each pair of axis-aligned planes bounding
  // opposite faces of the leaf box
  bool ray_is_valid = false; // is ray direction vector zero?
  for (int axis = 0; axis < 3; ++axis) {
    if (fabs(ray_dir[axis]) < epsilon) { // ray parallel to planes
      if (ray_pt[axis] >= box_min[axis] &&
          ray_pt[axis] <= box_max[axis])
        continue;
      else
        return false;
    }
      
      // find t values at which ray intersects each plane
    ray_is_valid = true;
    t1 = (box_min[axis] - ray_pt[axis]) / ray_dir[axis];
    t2 = (box_max[axis] - ray_pt[axis]) / ray_dir[axis];
    
      // t_enter = max( t_enter_x, t_enter_y, t_enter_z )
      // t_exit  = min( t_exit_x, t_exit_y, t_exit_z )
      //   where
      // t_enter_x = min( t1_x, t2_x );
      // t_exit_x  = max( t1_x, t2_x )
    if (t1 < t2) {
      if (t_enter < t1)
        t_enter = t1;
      if (t_exit > t2)
        t_exit = t2;
    }
    else {
      if (t_enter < t2)
        t_enter = t2;
      if (t_exit > t1)
        t_exit = t1;
    }
  }
  
  return ray_is_valid && (t_enter <= t_exit);
}


bool box_plane_overlap( const MBCartVect& normal,
                        double d,
                        MBCartVect min,
                        MBCartVect max )
{
  if (normal[0] < 0.0)
    std::swap( min[0], max[0] );
  if (normal[1] < 0.0)
    std::swap( min[1], max[1] );
  if (normal[2] < 0.0)
    std::swap( min[2], max[2] );
  
  return (normal % min <= -d) && (normal % max >= -d);
}


#define CHECK_RANGE( A, B, R ) \
  if ((A) < (B)) { \
    if ((A) > (R) || (B) < -(R)) \
      return false; \
  } \
  else if ((B) > (R) || (A) < -(R)) \
    return false

/* Adapted from: http://jgt.akpeters.com/papers/AkenineMoller01/tribox.html
 * Use separating axis theorem to test for overlap between triangle
 * and axis-aligned box.
 *
 * Test for overlap in these directions:
 * 1) {x,y,z}-directions 
 * 2) normal of triangle
 * 3) crossprod of triangle edge with {x,y,z}-direction
 */
bool box_tri_overlap( const MBCartVect vertices[3],
                      const MBCartVect& box_center,
                      const MBCartVect& box_dims )
{
    // translate everthing such that box is centered at origin
  const MBCartVect v0( vertices[0] - box_center );
  const MBCartVect v1( vertices[1] - box_center );
  const MBCartVect v2( vertices[2] - box_center );

  // do case 1) tests
  if (v0[0] > box_dims[0] && v1[0] > box_dims[0] && v2[0] > box_dims[0])
    return false;
  if (v0[1] > box_dims[1] && v1[1] > box_dims[1] && v2[1] > box_dims[1])
    return false;
  if (v0[2] > box_dims[2] && v1[2] > box_dims[2] && v2[2] > box_dims[2])
    return false;
  if (v0[0] < -box_dims[0] && v1[0] < -box_dims[0] && v2[0] < -box_dims[0])
    return false;
  if (v0[1] < -box_dims[1] && v1[1] < -box_dims[1] && v2[1] < -box_dims[1])
    return false;
  if (v0[2] < -box_dims[2] && v1[2] < -box_dims[2] && v2[2] < -box_dims[2])
    return false;
  
    // compute triangle edge vectors
  const MBCartVect e0( vertices[1] - vertices[0] );
  const MBCartVect e1( vertices[2] - vertices[1] );
  const MBCartVect e2( vertices[0] - vertices[2] );
  
    // do case 3) tests 
  double fex, fey, fez, p0, p1, p2, rad;
  fex = fabs(e0[0]);
  fey = fabs(e0[1]);
  fez = fabs(e0[2]);
  
  p0 = e0[2]*v0[1] - e0[1]*v0[2];
  p2 = e0[2]*v2[1] - e0[1]*v2[2];
  rad = fez * box_dims[1] + fey * box_dims[2];
  CHECK_RANGE( p0, p2, rad );
  
  p0 = -e0[2]*v0[0] + e0[0]*v0[2];
  p2 = -e0[2]*v2[0] + e0[0]*v2[2];
  rad = fez * box_dims[0] + fex * box_dims[2];
  CHECK_RANGE( p0, p2, rad );
    
  p1 = e0[1]*v1[0] - e0[0]*v1[1];
  p2 = e0[1]*v2[0] - e0[0]*v2[1];
  rad = fey * box_dims[0] + fex * box_dims[1];
  CHECK_RANGE( p1, p2, rad );
  
  fex = fabs(e1[0]);
  fey = fabs(e1[1]);
  fez = fabs(e1[2]);
  
  p0 = e1[2]*v0[1] - e1[1]*v0[2];
  p2 = e1[2]*v2[1] - e1[1]*v2[2];
  rad = fez * box_dims[1] + fey * box_dims[2];
  CHECK_RANGE( p0, p2, rad );
  
  p0 = -e1[2]*v0[0] + e1[0]*v0[2];
  p2 = -e1[2]*v2[0] + e1[0]*v2[2];
  rad = fez * box_dims[0] + fex * box_dims[2];
  CHECK_RANGE( p0, p2, rad );
  
  p0 = e1[1]*v0[0] - e1[0]*v0[1];
  p1 = e1[1]*v1[0] - e1[0]*v1[1];
  rad = fey * box_dims[0] + fex * box_dims[1];
  CHECK_RANGE( p0, p1, rad );
  
  fex = fabs(e2[0]);
  fey = fabs(e2[1]);
  fez = fabs(e2[2]);
  
  p0 = e2[2]*v0[1] - e2[1]*v0[2];
  p1 = e2[2]*v1[1] - e2[1]*v1[2];
  rad = fez * box_dims[1] + fey * box_dims[2];
  CHECK_RANGE( p0, p1, rad );
  
  p0 = -e2[2]*v0[0] + e2[0]*v0[2];
  p1 = -e2[2]*v1[0] + e2[0]*v1[2];
  rad = fez * box_dims[0] + fex * box_dims[2];
  CHECK_RANGE( p0, p1, rad );
  
  p1 = e2[1]*v1[0] - e2[0]*v1[1];
  p2 = e2[1]*v2[0] - e2[0]*v2[1];
  rad = fey * box_dims[0] + fex * box_dims[1];
  CHECK_RANGE( p1, p2, rad );
  
  // do case 2) test
  MBCartVect n = e0 * e1;
  return box_plane_overlap( n, -(n % v0), -box_dims, box_dims );
}
  

bool box_tri_overlap( const MBCartVect  triangle_corners[3],
                      const MBCartVect& box_min_corner,
                      const MBCartVect& box_max_corner,
                      double            tolerance )
{
  const MBCartVect box_center = 0.5 * (box_max_corner + box_min_corner);
  const MBCartVect box_hf_dim = 0.5 * (box_max_corner - box_min_corner);
  return box_tri_overlap( triangle_corners,
                          box_center,
                          box_hf_dim + MBCartVect(tolerance) );
} 

bool box_elem_overlap( const MBCartVect *elem_corners,
                       MBEntityType elem_type,
                       const MBCartVect& center,
                       const MBCartVect& dims )
{

  switch (elem_type) {
    case MBTRI:
      return box_tri_overlap( elem_corners, center, dims );
    case MBTET:
      return box_tet_overlap( elem_corners, center, dims );
    case MBHEX:
      return box_hex_overlap( elem_corners, center, dims );
    case MBPOLYGON:
    case MBPOLYHEDRON:
      assert(false);
      return false;
    default:
      return box_linear_elem_overlap( elem_corners, elem_type, center, dims );
  }
}

static inline MBCartVect quad_norm( const MBCartVect& v1,
                                    const MBCartVect& v2,
                                    const MBCartVect& v3,
                                    const MBCartVect& v4 )
{ return (-v1+v2+v3-v4) * (-v1-v2+v3+v4); }

static inline MBCartVect tri_norm( const MBCartVect& v1,
                                   const MBCartVect& v2,
                                   const MBCartVect& v3 )
{ return (v2-v1) * (v3-v1); }


bool box_linear_elem_overlap( const MBCartVect *elem_corners,
                              MBEntityType type,
                              const MBCartVect& box_center,
                              const MBCartVect& box_halfdims )
{
  MBCartVect corners[8];
  const unsigned num_corner = MBCN::VerticesPerEntity( type );
  assert( num_corner <= sizeof(corners)/sizeof(corners[0]) );
  for (unsigned i = 0; i < num_corner; ++i)
    corners[i] = elem_corners[i] - box_center;
  return box_linear_elem_overlap( corners, type, box_halfdims );
}
        

bool box_linear_elem_overlap( const MBCartVect *elem_corners,
                              MBEntityType type,
                              const MBCartVect& dims )
{
    // Do Separating Axis Theorem:
    // If the element and the box overlap, then the 1D projections
    // onto at least one of the axes in the following three sets
    // must overlap (assuming convex polyhedral element).
    // 1) The normals of the faces of the box (the principal axes)
    // 2) The crossproduct of each element edge with each box edge
    //    (crossproduct of each edge with each principal axis)
    // 3) The normals of the faces of the element

  unsigned i, e, f;             // loop counters
  double dot, cross[2], tmp;
  MBCartVect norm;
  int indices[4]; // element edge/face vertex indices
  
    // test box face normals (principal axes)
  const unsigned num_corner = MBCN::VerticesPerEntity( type );
  int not_less[3] = { num_corner, num_corner, num_corner }; 
  int not_greater[3] = { num_corner, num_corner, num_corner };
  int not_inside;
  for (i = 0; i < num_corner; ++i) { // for each element corner
    not_inside = 3;
    
    if (elem_corners[i][0] < -dims[0])
      --not_less[0];
    else if (elem_corners[i][0] > dims[0])
      --not_greater[0];
    else
      --not_inside;
      
    if (elem_corners[i][1] < -dims[1])
      --not_less[1];
    else if (elem_corners[i][1] > dims[1])
      --not_greater[1];
    else
      --not_inside;
      
    if (elem_corners[i][2] < -dims[2])
      --not_less[2];
    else if (elem_corners[i][2] > dims[2])
      --not_greater[2];
    else
      --not_inside;
    
    if (!not_inside)
      return true;
  }
    // If all points less than min_x of box, then
    // not_less[0] == 0, and therfore
    // the following product is zero.
  if (not_greater[0] * not_greater[1] * not_greater[2] * 
         not_less[0] *    not_less[1] *    not_less[2] == 0)
    return false;
 
    // Test edge-edge crossproducts
    
    // Edge directions for box are principal axis, so 
    // for each element edge, check along the cross-product
    // of that edge with each of the tree principal axes.
  const unsigned num_edge = MBCN::NumSubEntities( type, 1 );
  for (e = 0; e < num_edge; ++e) { // for each element edge
      // get which element vertices bound the edge
    MBCN::SubEntityVertexIndices( type, 1, e, indices );

      // X-Axis

      // calculate crossproduct: axis x (v1 - v0),
      // where v1 and v0 are edge vertices.
    cross[0] = elem_corners[indices[0]][2] - elem_corners[indices[1]][2];
    cross[1] = elem_corners[indices[1]][1] - elem_corners[indices[0]][1];
      // skip if parallel
    if ((cross[0]*cross[0] + cross[1]*cross[1]) >= std::numeric_limits<double>::epsilon()) {
      dot = fabs(cross[0] * dims[1]) + fabs(cross[1] * dims[2]);
      not_less[0] = not_greater[0] = num_corner - 1;
      for (i = (unsigned)(indices[0]+1)%num_corner; i != (unsigned)indices[0]; i = (i+1)%num_corner) { // for each element corner
        tmp = cross[0] * elem_corners[i][1] + cross[1] * elem_corners[i][2];
        not_less[0] -= (tmp < -dot);
        not_greater[0] -= (tmp > dot);
      }

      if (not_less[0] * not_greater[0] == 0)
        return false;
    }

      // Y-Axis

      // calculate crossproduct: axis x (v1 - v0),
      // where v1 and v0 are edge vertices.
    cross[0] = elem_corners[indices[0]][0] - elem_corners[indices[1]][0];
    cross[1] = elem_corners[indices[1]][2] - elem_corners[indices[0]][2];
      // skip if parallel
    if ((cross[0]*cross[0] + cross[1]*cross[1]) >= std::numeric_limits<double>::epsilon()) {
      dot = fabs(cross[0] * dims[2]) + fabs(cross[1] * dims[0]);
      not_less[0] = not_greater[0] = num_corner - 1;
      for (i = (unsigned)(indices[0]+1)%num_corner; i != (unsigned)indices[0]; i = (i+1)%num_corner) { // for each element corner
        tmp = cross[0] * elem_corners[i][2] + cross[1] * elem_corners[i][0];
        not_less[0] -= (tmp < -dot);
        not_greater[0] -= (tmp > dot);
      }

      if (not_less[0] * not_greater[0] == 0)
        return false;
    }

      // Z-Axis

      // calculate crossproduct: axis x (v1 - v0),
      // where v1 and v0 are edge vertices.
    cross[0] = elem_corners[indices[0]][1] - elem_corners[indices[1]][1];
    cross[1] = elem_corners[indices[1]][0] - elem_corners[indices[0]][0];
      // skip if parallel
    if ((cross[0]*cross[0] + cross[1]*cross[1]) >= std::numeric_limits<double>::epsilon()) {
      dot = fabs(cross[0] * dims[0]) + fabs(cross[1] * dims[1]);
      not_less[0] = not_greater[0] = num_corner - 1;
      for (i = (unsigned)(indices[0]+1)%num_corner; i != (unsigned)indices[0]; i = (i+1)%num_corner) { // for each element corner
        tmp = cross[0] * elem_corners[i][0] + cross[1] * elem_corners[i][1];
        not_less[0] -= (tmp < -dot);
        not_greater[0] -= (tmp > dot);
      }

      if (not_less[0] * not_greater[0] == 0)
        return false;
    }
  }
  
  
    // test element face normals
  const unsigned num_face = MBCN::NumSubEntities( type, 2 );
  for (f = 0; f < num_face; ++f) {
    MBCN::SubEntityVertexIndices( type, 2, f, indices );
    switch (MBCN::SubEntityType( type, 2, f )) {
      case MBTRI:
        norm = tri_norm( elem_corners[indices[0]], 
                         elem_corners[indices[1]], 
                         elem_corners[indices[2]] );
        break;
      case MBQUAD:
        norm = quad_norm( elem_corners[indices[0]], 
                          elem_corners[indices[1]], 
                          elem_corners[indices[2]], 
                          elem_corners[indices[3]] );
        break;
      default:
        assert(false);
        continue;
    }
    
    dot = dot_abs(norm, dims);
    
    // for each element vertex
    not_less[0] = not_greater[0] = num_corner;
    for (i = 0; i < num_corner; ++i) { 
      tmp = norm % elem_corners[i];
      not_less[0] -= (tmp < -dot);
      not_greater[0] -= (tmp > dot);
    }

    if (not_less[0] * not_greater[0] == 0)
      return false;
  }

    // Overlap on all tested axes.
  return true;
}
 

bool box_hex_overlap( const MBCartVect *elem_corners,
                      const MBCartVect& center,
                      const MBCartVect& dims )
{
    // Do Separating Axis Theorem:
    // If the element and the box overlap, then the 1D projections
    // onto at least one of the axes in the following three sets
    // must overlap (assuming convex polyhedral element).
    // 1) The normals of the faces of the box (the principal axes)
    // 2) The crossproduct of each element edge with each box edge
    //    (crossproduct of each edge with each principal axis)
    // 3) The normals of the faces of the element

  unsigned i, e, f;             // loop counters
  double dot, cross[2], tmp;
  MBCartVect norm;
  const MBCartVect corners[8] = { elem_corners[0] - center,
                                  elem_corners[1] - center,
                                  elem_corners[2] - center,
                                  elem_corners[3] - center,
                                  elem_corners[4] - center,
                                  elem_corners[5] - center,
                                  elem_corners[6] - center,
                                  elem_corners[7] - center };
  
    // test box face normals (principal axes)
  int not_less[3] = { 8, 8, 8 }; 
  int not_greater[3] = { 8, 8, 8 };
  int not_inside;
  for (i = 0; i < 8; ++i) { // for each element corner
    not_inside = 3;
    
    if (corners[i][0] < -dims[0])
      --not_less[0];
    else if (corners[i][0] > dims[0])
      --not_greater[0];
    else
      --not_inside;
      
    if (corners[i][1] < -dims[1])
      --not_less[1];
    else if (corners[i][1] > dims[1])
      --not_greater[1];
    else
      --not_inside;
      
    if (corners[i][2] < -dims[2])
      --not_less[2];
    else if (corners[i][2] > dims[2])
      --not_greater[2];
    else
      --not_inside;
    
    if (!not_inside)
      return true;
  }
    // If all points less than min_x of box, then
    // not_less[0] == 0, and therfore
    // the following product is zero.
  if (not_greater[0] * not_greater[1] * not_greater[2] * 
         not_less[0] *    not_less[1] *    not_less[2] == 0)
    return false;
 
    // Test edge-edge crossproducts
  const unsigned edges[12][2] = { { 0, 1 }, { 0, 4 }, { 0, 3 },
                                  { 2, 3 }, { 2, 1 }, { 2, 6 },
                                  { 5, 6 }, { 5, 1 }, { 5, 4 },
                                  { 7, 4 }, { 7, 3 }, { 7, 6 } };
                             
    // Edge directions for box are principal axis, so 
    // for each element edge, check along the cross-product
    // of that edge with each of the tree principal axes.
  for (e = 0; e < 12; ++e) { // for each element edge
      // get which element vertices bound the edge
    const MBCartVect& v0 = corners[ edges[e][0] ];
    const MBCartVect& v1 = corners[ edges[e][1] ];

      // X-Axis

      // calculate crossproduct: axis x (v1 - v0),
      // where v1 and v0 are edge vertices.
    cross[0] = v0[2] - v1[2];
    cross[1] = v1[1] - v0[1];
      // skip if parallel
    if ((cross[0]*cross[0] + cross[1]*cross[1]) >= std::numeric_limits<double>::epsilon()) {
      dot = fabs(cross[0] * dims[1]) + fabs(cross[1] * dims[2]);
      not_less[0] = not_greater[0] = 7;
      for (i = (edges[e][0]+1)%8; i != edges[e][0]; i = (i+1)%8) { // for each element corner
        tmp = cross[0] * corners[i][1] + cross[1] * corners[i][2];
        not_less[0] -= (tmp < -dot);
        not_greater[0] -= (tmp > dot);
      }

      if (not_less[0] * not_greater[0] == 0)
        return false;
    }

      // Y-Axis

      // calculate crossproduct: axis x (v1 - v0),
      // where v1 and v0 are edge vertices.
    cross[0] = v0[0] - v1[0];
    cross[1] = v1[2] - v0[2];
      // skip if parallel
    if ((cross[0]*cross[0] + cross[1]*cross[1]) >= std::numeric_limits<double>::epsilon()) {
      dot = fabs(cross[0] * dims[2]) + fabs(cross[1] * dims[0]);
      not_less[0] = not_greater[0] = 7;
      for (i = (edges[e][0]+1)%8; i != edges[e][0]; i = (i+1)%8) { // for each element corner
        tmp = cross[0] * corners[i][2] + cross[1] * corners[i][0];
        not_less[0] -= (tmp < -dot);
        not_greater[0] -= (tmp > dot);
      }

      if (not_less[0] * not_greater[0] == 0)
        return false;
    }

      // Z-Axis

      // calculate crossproduct: axis x (v1 - v0),
      // where v1 and v0 are edge vertices.
    cross[0] = v0[1] - v1[1];
    cross[1] = v1[0] - v0[0];
      // skip if parallel
    if ((cross[0]*cross[0] + cross[1]*cross[1]) >= std::numeric_limits<double>::epsilon()) {
      dot = fabs(cross[0] * dims[0]) + fabs(cross[1] * dims[1]);
      not_less[0] = not_greater[0] = 7;
      for (i = (edges[e][0]+1)%8; i != edges[e][0]; i = (i+1)%8) { // for each element corner
        tmp = cross[0] * corners[i][0] + cross[1] * corners[i][1];
        not_less[0] -= (tmp < -dot);
        not_greater[0] -= (tmp > dot);
      }

      if (not_less[0] * not_greater[0] == 0)
        return false;
    }
  }
  
  
    // test element face normals
  const unsigned faces[6][4] = { { 0, 1, 2, 3 },
                                 { 0, 1, 5, 4 },
                                 { 1, 2, 6, 5 },
                                 { 2, 6, 7, 3 },
                                 { 3, 7, 4, 0 },
                                 { 7, 4, 5, 6 } };
  for (f = 0; f < 6; ++f) {
    norm = quad_norm( corners[faces[f][0]], 
                      corners[faces[f][1]], 
                      corners[faces[f][2]], 
                      corners[faces[f][3]] );
    
    dot = dot_abs(norm, dims);
   
    // for each element vertex
    not_less[0] = not_greater[0] = 8;
    for (i = 0; i < 8; ++i) { 
      tmp = norm % corners[i];
      not_less[0] -= (tmp < -dot);
      not_greater[0] -= (tmp > dot);
    }

    if (not_less[0] * not_greater[0] == 0)
      return false;
  }

    // Overlap on all tested axes.
  return true;
}

static inline 
bool box_tet_overlap_edge( const MBCartVect& dims,
                           const MBCartVect& edge,
                           const MBCartVect& ve,
                           const MBCartVect& v1,
                           const MBCartVect& v2 )
{
  double dot, dot1, dot2, dot3, min, max;
  
    // edge x X
  if (fabs(edge[1]*edge[2]) > std::numeric_limits<double>::epsilon()) {
    dot = fabs(edge[2]) * dims[1] + fabs(edge[1]) * dims[2];
    dot1 = edge[2] * ve[1] - edge[1] * ve[2];
    dot2 = edge[2] * v1[1] - edge[1] * v1[2];
    dot3 = edge[2] * v2[1] - edge[1] * v2[2];
    min_max_3( dot1, dot2, dot3, min, max );
    if (max < -dot || min > dot)
      return false;
  }
  
    // edge x Y
  if (fabs(edge[1]*edge[2]) > std::numeric_limits<double>::epsilon()) {
    dot = fabs(edge[2]) * dims[0] + fabs(edge[0]) * dims[2];
    dot1 = -edge[2] * ve[0] + edge[0] * ve[2];
    dot2 = -edge[2] * v1[0] + edge[0] * v1[2];
    dot3 = -edge[2] * v2[0] + edge[0] * v2[2];
    min_max_3( dot1, dot2, dot3, min, max );
    if (max < -dot || min > dot)
      return false;
  }
  
    // edge x Z
  if (fabs(edge[1]*edge[2]) > std::numeric_limits<double>::epsilon()) {
    dot = fabs(edge[1]) * dims[0] + fabs(edge[0]) * dims[1];
    dot1 = edge[1] * ve[0] - edge[0] * ve[1];
    dot2 = edge[1] * v1[0] - edge[0] * v1[1];
    dot3 = edge[1] * v2[0] - edge[0] * v2[1];
    min_max_3( dot1, dot2, dot3, min, max );
    if (max < -dot || min > dot)
      return false;
  }

  return true;
}

bool box_tet_overlap( const MBCartVect *corners_in,
                      const MBCartVect& center,
                      const MBCartVect& dims )
{
    // Do Separating Axis Theorem:
    // If the element and the box overlap, then the 1D projections
    // onto at least one of the axes in the following three sets
    // must overlap (assuming convex polyhedral element).
    // 1) The normals of the faces of the box (the principal axes)
    // 2) The crossproduct of each element edge with each box edge
    //    (crossproduct of each edge with each principal axis)
    // 3) The normals of the faces of the element

    // Translate problem such that box center is at origin.
  const MBCartVect corners[4] = { corners_in[0] - center,
                                  corners_in[1] - center,
                                  corners_in[2] - center,
                                  corners_in[3] - center };

    // 0) Check if any vertex is within the box
  if (fabs(corners[0][0]) <= dims[0] &&
      fabs(corners[0][1]) <= dims[1] &&
      fabs(corners[0][2]) <= dims[2])
    return true;
  if (fabs(corners[1][0]) <= dims[0] &&
      fabs(corners[1][1]) <= dims[1] &&
      fabs(corners[1][2]) <= dims[2])
    return true;
  if (fabs(corners[2][0]) <= dims[0] &&
      fabs(corners[2][1]) <= dims[1] &&
      fabs(corners[2][2]) <= dims[2])
    return true;
  if (fabs(corners[3][0]) <= dims[0] &&
      fabs(corners[3][1]) <= dims[1] &&
      fabs(corners[3][2]) <= dims[2])
    return true;
  

    // 1) Check for overlap on each principal axis (box face normal)
    // X
  if (corners[0][0] < -dims[0] &&
      corners[1][0] < -dims[0] &&
      corners[2][0] < -dims[0] &&
      corners[3][0] < -dims[0])
    return false;
  if (corners[0][0] >  dims[0] &&
      corners[1][0] >  dims[0] &&
      corners[2][0] >  dims[0] &&
      corners[3][0] >  dims[0])
    return false;
    // Y
  if (corners[0][1] < -dims[1] &&
      corners[1][1] < -dims[1] &&
      corners[2][1] < -dims[1] &&
      corners[3][1] < -dims[1])
    return false;
  if (corners[0][1] >  dims[1] &&
      corners[1][1] >  dims[1] &&
      corners[2][1] >  dims[1] &&
      corners[3][1] >  dims[1])
    return false;
    // Z
  if (corners[0][2] < -dims[2] &&
      corners[1][2] < -dims[2] &&
      corners[2][2] < -dims[2] &&
      corners[3][2] < -dims[2])
    return false;
  if (corners[0][2] >  dims[2] &&
      corners[1][2] >  dims[2] &&
      corners[2][2] >  dims[2] &&
      corners[3][2] >  dims[2])
    return false;
 
    // 3) test element face normals
  MBCartVect norm;
  double dot, dot1, dot2;
  
  const MBCartVect v01 = corners[1] - corners[0];
  const MBCartVect v02 = corners[2] - corners[0];
  norm = v01 * v02;
  dot = dot_abs(norm, dims);
  dot1 = norm % corners[0];
  dot2 = norm % corners[3];
  if (dot1 > dot2)
    std::swap(dot1, dot2);
  if (dot2 < -dot || dot1 > dot)
    return false;
  
  const MBCartVect v03 = corners[3] - corners[0];
  norm = v03 * v01;
  dot = dot_abs(norm, dims);
  dot1 = norm % corners[0];
  dot2 = norm % corners[2];
  if (dot1 > dot2)
    std::swap(dot1, dot2);
  if (dot2 < -dot || dot1 > dot)
    return false;
  
  norm = v02 * v03;
  dot = dot_abs(norm, dims);
  dot1 = norm % corners[0];
  dot2 = norm % corners[1];
  if (dot1 > dot2)
    std::swap(dot1, dot2);
  if (dot2 < -dot || dot1 > dot)
    return false;
  
  const MBCartVect v12 = corners[2] - corners[1];
  const MBCartVect v13 = corners[3] - corners[1];
  norm = v13 * v12;
  dot = dot_abs(norm, dims);
  dot1 = norm % corners[0];
  dot2 = norm % corners[1];
  if (dot1 > dot2)
    std::swap(dot1, dot2);
  if (dot2 < -dot || dot1 > dot)
    return false;
  

    // 2) test edge-edge cross products
    
  const MBCartVect v23 = corners[3] - corners[2];
  return box_tet_overlap_edge( dims, v01, corners[0], corners[2], corners[3] )
      && box_tet_overlap_edge( dims, v02, corners[0], corners[1], corners[3] )
      && box_tet_overlap_edge( dims, v03, corners[0], corners[1], corners[2] )
      && box_tet_overlap_edge( dims, v12, corners[1], corners[0], corners[3] )
      && box_tet_overlap_edge( dims, v13, corners[1], corners[0], corners[2] )
      && box_tet_overlap_edge( dims, v23, corners[2], corners[0], corners[1] );
}
    



//from: http://www.geometrictools.com/Documentation/DistancePoint3Triangle3.pdf#search=%22closest%20point%20on%20triangle%22
/*       t
 *   \(2)^
 *    \  |
 *     \ |
 *      \|
 *       \
 *       |\
 *       | \
 *       |  \  (1)
 *  (3)  tv  \
 *       |    \
 *       | (0) \
 *       |      \
 *-------+---sv--\----> s
 *       |        \ (6)
 *  (4)  |   (5)   \
 */
// Worst case is either 61 flops and 5 compares or 53 flops and 6 compares,
// depending on relative costs.  For all paths that do not return one of the
// corner vertices, exactly one of the flops is a divide.
void closest_location_on_tri( const MBCartVect& location,
                              const MBCartVect* vertices,
                              MBCartVect& closest_out )
{                                                     // ops      comparisons
  const MBCartVect sv( vertices[1] - vertices[0] );   // +3 = 3
  const MBCartVect tv( vertices[2] - vertices[0] );   // +3 = 6
  const MBCartVect pv( vertices[0] - location );      // +3 = 9
  const double ss = sv % sv;                          // +5 = 14
  const double st = sv % tv;                          // +5 = 19
  const double tt = tv % tv;                          // +5 = 24
  const double sp = sv % pv;                          // +5 = 29
  const double tp = tv % pv;                          // +5 = 34
  const double det = ss*tt - st*st;                   // +3 = 37
  double s = st*tp - tt*sp;                           // +3 = 40
  double t = st*sp - ss*tp;                           // +3 = 43
  if (s+t < det) {                                    // +1 = 44, +1 = 1
    if (s < 0) {                                      //          +1 = 2
      if (t < 0) {                                    //          +1 = 3
        // region 4
        if (sp < 0) {                                 //          +1 = 4
          if (-sp > ss)                               //          +1 = 5
            closest_out = vertices[1];                //      44       5
          else
            closest_out = vertices[0] - (sp/ss) * sv; // +7 = 51,      5
        }
        else if (tp < 0) {                            //          +1 = 5
          if (-tp > tt)                               //          +1 = 6
            closest_out = vertices[2];                //      44,      6
          else
            closest_out = vertices[0] - (tp/tt) * tv; // +7 = 51,      6
        }
        else {
          closest_out = vertices[0];                  //      44,      5
        }
      }
      else {
        // region 3
        if (tp >= 0)                                  //          +1 = 4
          closest_out = vertices[0];                  //      44,      4
        else if (-tp >= tt)                           //          +1 = 5
          closest_out = vertices[2];                  //      44,      5
        else
          closest_out = vertices[0] - (tp/tt) * tv;   // +7 = 51,      5
      }
    }
    else if (t < 0) {                                 //          +1 = 3
      // region 5;
      if (sp >= 0.0)                                  //          +1 = 4
        closest_out = vertices[0];                    //      44,      4
      else if (-sp >= ss)                             //          +1 = 5
        closest_out = vertices[1];                    //      44       5
      else
        closest_out = vertices[0] - (sp/ss) * sv;     // +7 = 51,      5
    }
    else {
      // region 0
      const double inv_det = 1.0 / det;               // +1 = 45
      s *= inv_det;                                   // +1 = 46
      t *= inv_det;                                   // +1 = 47
      closest_out = vertices[0] + s*sv + t*tv;        //+12 = 59,      3  
    }
  }
  else {
    if (s < 0) {                                      //          +1 = 2
      // region 2
      s = st + sp;                                    // +1 = 45
      t = tt + tp;                                    // +1 = 46
      if (t > s) {                                    //          +1 = 3
        const double num = t - s;                     // +1 = 47
        const double den = ss - 2*st + tt;            // +3 = 50
        if (num > den)                                //          +1 = 4
          closest_out = vertices[1];                  //      50,      4
        else {
          s = num/den;                                // +1 = 51
          t = 1 - s;                                  // +1 = 52
          closest_out = s*vertices[1] + t*vertices[2];// +9 = 61,      4
        }
      }
      else if (t <= 0)                                //          +1 = 4
        closest_out = vertices[2];                    //      46,      4
      else if (tp >= 0)                               //          +1 = 5
        closest_out = vertices[0];                    //      46,      5
      else
        closest_out = vertices[0] - (tp/tt) * tv;     // +7 = 53,      5
    }
    else if (t < 0) {                                 //          +1 = 3
      // region 6
      t = st + tp;                                    // +1 = 45
      s = ss + sp;                                    // +1 = 46
      if (s > t) {                                    //          +1 = 4
        const double num = t - s;                     // +1 = 47
        const double den = tt - 2*st + ss;            // +3 = 50
        if (num > den)                                //          +1 = 5
          closest_out = vertices[2];                  //      50,      5
        else {
          t = num/den;                                // +1 = 51
          s = 1 - t;                                  // +1 = 52
          closest_out = s*vertices[1] + t*vertices[2];// +9 = 61,      5
        }
      }
      else if (s <= 0)                                //          +1 = 5
        closest_out = vertices[1];                    //      46,      5
      else if (sp >= 0)                               //          +1 = 6
        closest_out = vertices[0];                    //      46,      6
      else
        closest_out = vertices[0] - (sp/ss) * sv;     // +7 = 53,      6
    }
    else {
      // region 1
      const double num = tt + tp - st - sp;           // +3 = 47
      if (num <= 0) {                                 //          +1 = 4
        closest_out = vertices[2];                    //      47,      4
      }
      else {
        const double den = ss - 2*st + tt;            // +3 = 50
        if (num >= den)                               //          +1 = 5
          closest_out = vertices[1];                  //      50,      5
        else {
          s = num/den;                                // +1 = 51
          t = 1 - s;                                  // +1 = 52
          closest_out = s*vertices[1] + t*vertices[2];// +9 = 61,      5
        }
      }
    }
  }
}

void closest_location_on_tri( const MBCartVect& location,
                              const MBCartVect* vertices,
                              double tolerance,
                              MBCartVect& closest_out,
                              int& closest_topo )
{
  const double tsqr = tolerance*tolerance;
  int i;
  MBCartVect pv[3], ev, ep;
  double t;

  closest_location_on_tri( location, vertices, closest_out );
  
  for (i = 0; i < 3; ++i) {
    pv[i] = vertices[i] - closest_out;
    if ((pv[i] % pv[i]) <= tsqr) {
      closest_topo = i;
      return;
    }
  }
  
  for (i = 0; i < 3; ++i) {
    ev = vertices[(i+1)%3] - vertices[i];
    t = (ev % pv[i]) / (ev % ev);
    ep = closest_out - (vertices[i] + t * ev);
    if ((ep % ep) <= tsqr) {
      closest_topo = i+3;
      return;
    }
  }
  
  closest_topo = 6;
}
 
    
// We assume polygon is *convex*, but *not* planar.
void closest_location_on_polygon( const MBCartVect& location,
                                  const MBCartVect* vertices,
                                  int num_vertices,
                                  MBCartVect& closest_out )
{
  const int n = num_vertices;
  MBCartVect d, p, v;
  double shortest_sqr, dist_sqr, t_closest, t;
  int i, e;
  
    // Find closest edge of polygon.
  e = n - 1;
  v = vertices[0] - vertices[e];
  t_closest = (v % (location - vertices[e])) / (v % v);
  if (t_closest < 0.0)
    d = location - vertices[e];
  else if (t_closest > 1.0)
    d = location - vertices[0];
  else 
    d = location - vertices[e] - t_closest * v;
  shortest_sqr = d % d;
  for (i = 0; i < n - 1; ++i) {
    v = vertices[i+1] - vertices[i];
    t = (v % (location - vertices[i])) / (v % v);
    if (t < 0.0)
      d = location - vertices[i];
    else if (t > 1.0)
      d = location - vertices[i+1];
    else
      d = location - vertices[i] - t * v;
    dist_sqr = d % d;
    if (dist_sqr < shortest_sqr) {
      e = i;
      shortest_sqr = dist_sqr;
      t_closest = t;
    }
  }
  
    // If we are beyond the bounds of the edge, then
    // the point is outside and closest to a vertex
  if (t_closest <= 0.0) {
    closest_out = vertices[e];
    return;
  }
  else if (t_closest >= 1.0) {
    closest_out = vertices[(e+1)%n];
    return;
  }
  
    // Now check which side of the edge we are one
  const MBCartVect v0 = vertices[e] - vertices[(e+n-1)%n];
  const MBCartVect v1 = vertices[(e+1)%n] - vertices[e];
  const MBCartVect v2 = vertices[(e+2)%n] - vertices[(e+1)%n];
  const MBCartVect norm = (1.0 - t_closest) * (v0 * v1) + t_closest * (v1 * v2);
    // if on outside of edge, result is closest point on edge
  if ((norm % ((vertices[e] - location) * v1)) <= 0.0) { 
    closest_out = vertices[e] + t_closest * v1;
    return;
  }
  
    // Inside.  Project to plane defined by point and normal at
    // closest edge
  const double D = -(norm % (vertices[e] + t_closest * v1));
  closest_out = (location - (norm % location + D) * norm)/(norm % norm);
}

void closest_location_on_box( const MBCartVect& min,
                              const MBCartVect& max,
                              const MBCartVect& point,
                              MBCartVect& closest )
{
  closest[0] = point[0] < min[0] ? min[0] : point[0] > max[0] ? max[0] : point[0];
  closest[1] = point[1] < min[1] ? min[1] : point[1] > max[1] ? max[1] : point[1];
  closest[2] = point[2] < min[2] ? min[2] : point[2] > max[2] ? max[2] : point[2];
}

bool box_point_overlap( const MBCartVect& box_min_corner,
                        const MBCartVect& box_max_corner,
                        const MBCartVect& point,
                        double tolerance )
{
  MBCartVect closest;
  closest_location_on_box( box_min_corner, box_max_corner, point, closest );
  closest -= point;
  return closest % closest < tolerance * tolerance;
}


/**\brief Class representing a 3-D mapping function (e.g. shape function for volume element) */
class VolMap {
  public:
      /**\brief Return $\vec \xi$ corresponding to logical center of element */
    virtual MBCartVect center_xi() const = 0;
      /**\brief Evaluate mapping function (calculate $\vec x = F($\vec \xi)$ )*/
    virtual MBCartVect evaluate( const MBCartVect& xi ) const = 0;
      /**\brief Evaluate Jacobian of mapping function */
    virtual MBMatrix3 jacobian( const MBCartVect& xi ) const = 0;
      /**\brief Evaluate inverse of mapping function (calculate $\vec \xi = F^-1($\vec x)$ )*/
    bool solve_inverse( const MBCartVect& x, MBCartVect& xi, double tol ) const ;
};

bool VolMap::solve_inverse( const MBCartVect& x, MBCartVect& xi, double tol ) const
{
  const double error_tol_sqr = tol*tol;
  double det;
  xi = center_xi();
  MBCartVect delta = evaluate(xi) - x;
  MBMatrix3 J;
  while (delta % delta > error_tol_sqr) {
    J = jacobian(xi);
    det = J.determinant();
    if (det < std::numeric_limits<double>::epsilon())
      return false;
    xi -= J.inverse(1.0/det) * delta;
    delta = evaluate( xi ) - x;
  }
  return true;
}

/**\brief Shape function for trilinear hexahedron */
class LinearHexMap : public VolMap {
  public:
    LinearHexMap( const MBCartVect* corner_coords ) : corners(corner_coords) {}
    virtual MBCartVect center_xi() const;
    virtual MBCartVect evaluate( const MBCartVect& xi ) const;
    virtual MBMatrix3 jacobian( const MBCartVect& xi ) const;
  private:
    const MBCartVect* corners;
    static const double corner_xi[8][3];
};

const double LinearHexMap::corner_xi[8][3] = { { -1, -1, -1 },
                                               {  1, -1, -1 },
                                               {  1,  1, -1 },
                                               { -1,  1, -1 },
                                               { -1, -1,  1 },
                                               {  1, -1,  1 },
                                               {  1,  1,  1 },
                                               { -1,  1,  1 } };
MBCartVect LinearHexMap::center_xi() const
  { return MBCartVect(0.0); }

MBCartVect LinearHexMap::evaluate( const MBCartVect& xi ) const
{
  MBCartVect x(0.0);
  for (unsigned i = 0; i < 8; ++i) {
    const double N_i = (1 + xi[0]*corner_xi[i][0])
                     * (1 + xi[1]*corner_xi[i][1])
                     * (1 + xi[2]*corner_xi[i][2]);
    x += N_i * corners[i];
  }
  x *= 0.125;
  return x;
}

MBMatrix3 LinearHexMap::jacobian( const MBCartVect& xi ) const
{
  MBMatrix3 J(0.0);
  for (unsigned i = 0; i < 8; ++i) {
    const double   xi_p = 1 + xi[0]*corner_xi[i][0];
    const double  eta_p = 1 + xi[1]*corner_xi[i][1];
    const double zeta_p = 1 + xi[2]*corner_xi[i][2];
    const double dNi_dxi   = corner_xi[i][0] * eta_p * zeta_p;
    const double dNi_deta  = corner_xi[i][1] *  xi_p * zeta_p;
    const double dNi_dzeta = corner_xi[i][2] *  xi_p *  eta_p;
    J(0,0) += dNi_dxi   * corners[i][0];
    J(1,0) += dNi_dxi   * corners[i][1];
    J(2,0) += dNi_dxi   * corners[i][2];
    J(0,1) += dNi_deta  * corners[i][0];
    J(1,1) += dNi_deta  * corners[i][1];
    J(2,1) += dNi_deta  * corners[i][2];
    J(0,2) += dNi_dzeta * corners[i][0];
    J(1,2) += dNi_dzeta * corners[i][1];
    J(2,2) += dNi_dzeta * corners[i][2];
  }
  return J *= 0.125;
}

bool nat_coords_trilinear_hex( const MBCartVect* corner_coords,
                               const MBCartVect& x,
                               MBCartVect& xi,
                               double tol )
{
  return LinearHexMap( corner_coords ).solve_inverse( x, xi, tol );
}


bool point_in_trilinear_hex(const MBCartVect *hex, 
                            const MBCartVect& xyz,
                            double etol) 
{
  MBCartVect xi;
  return nat_coords_trilinear_hex( hex, xyz, xi, etol )
      && fabs(xi[0])-1 < etol 
      && fabs(xi[1])-1 < etol 
      && fabs(xi[2])-1 < etol;
}



} // namespace MBGeoemtry