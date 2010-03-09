/**
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



#include "AEntityFactory.hpp"
#include "MBInternals.hpp"
#include "MBCore.hpp"
#include "MBRange.hpp"
#include "MBError.hpp"
#include "MBCN.hpp"
#include "MeshTopoUtil.hpp"
#include "EntitySequence.hpp"
#include "SequenceData.hpp"
#include "SequenceManager.hpp"
#include "MBRangeSeqIntersectIter.hpp"

#include <assert.h>
#include <algorithm>
#include <set>

MBErrorCode AEntityFactory::get_vertices( MBEntityHandle h,
                                          const MBEntityHandle*& vect_out,
                                          int& count_out,
                                          std::vector<MBEntityHandle>& storage )
{
  MBErrorCode result;
  if (MBPOLYHEDRON == TYPE_FROM_HANDLE(h)) {
    storage.clear();
    result = thisMB->get_adjacencies( &h, 1, 0, false, storage );
    vect_out = &storage[0];
    count_out = storage.size();
  }
  else {
    result = thisMB->get_connectivity( h, vect_out, count_out, false, &storage );
  }
  return result;
}

AEntityFactory::AEntityFactory(MBCore *mdb) 
{
  assert(NULL != mdb);
  thisMB = mdb;
  mVertElemAdj = false;
}


AEntityFactory::~AEntityFactory()
{
  // clean up all the adjacency information that was created
  MBEntityType ent_type;

  // iterate through each element type
  for (ent_type = MBVERTEX; ent_type != MBENTITYSET; ent_type++) {
    TypeSequenceManager::iterator i;
    TypeSequenceManager& seqman = thisMB->sequence_manager()->entity_map( ent_type );
    for (i = seqman.begin(); i != seqman.end(); ++i) {
      std::vector<MBEntityHandle>** adj_list = (*i)->data()->get_adjacency_data();
      if (!adj_list)
        continue;
      adj_list += (*i)->start_handle() - (*i)->data()->start_handle();
      
      for (MBEntityID j = 0; j < (*i)->size(); ++j) {
        delete adj_list[j];
        adj_list[j] = 0;
      }
    }
  }
}

//! get the elements contained by source_entity, of
//! type target_type, passing back in target_entities; if create_if_missing
//! is true and no entity is found, one is created; if create_adjacency_option
//! is >= 0, adjacencies from entities of that dimension to each target_entity
//! are created (this function uses AEntityFactory::get_element for each element)
MBErrorCode AEntityFactory::get_elements(MBEntityHandle source_entity,
                                          const unsigned int target_dimension,
                                          std::vector<MBEntityHandle> &target_entities,
                                          const bool create_if_missing,
                                          const int create_adjacency_option)
{
  // check for trivial case first
  const MBEntityType source_type = TYPE_FROM_HANDLE(source_entity);
  const unsigned source_dimension = MBCN::Dimension(source_type);

  if (source_type >= MBENTITYSET || target_dimension < 1 || target_dimension > 3) {
    return MB_TYPE_OUT_OF_RANGE;
  }
  else if (source_dimension == target_dimension) {
    target_entities.push_back( source_entity );
    return MB_SUCCESS;
  }

  MBErrorCode result;
  if(mVertElemAdj == false) {
    result = create_vert_elem_adjacencies();
    if (MB_SUCCESS != result) return result;
  }

  if(source_dimension == 0)
  {
    result = get_zero_to_n_elements(source_entity, target_dimension,
      target_entities, create_if_missing, create_adjacency_option);
  }
  else if(source_dimension > target_dimension)
  {
    result = get_down_adjacency_elements(source_entity, target_dimension,
                                         target_entities, create_if_missing, create_adjacency_option);
  }
  else //if(source_dimension < target_dimension)
  {
    result = get_up_adjacency_elements( source_entity, target_dimension,
           target_entities, create_if_missing, create_adjacency_option);
  }

  return result;
}

MBErrorCode AEntityFactory::get_polyhedron_vertices(const MBEntityHandle source_entity, 
                                                    std::vector<MBEntityHandle> &target_entities) 
{
    // get the connectivity array pointer
  const MBEntityHandle *connect;
  int num_connect;
  MBErrorCode result = thisMB->get_connectivity(source_entity, connect, num_connect);
  if (MB_SUCCESS != result) return result;
  
    // now get the union of those polygons' vertices
  result = thisMB->get_adjacencies(connect, num_connect, 0, false, target_entities, 
                                   MBInterface::UNION);
  return result;
}

MBErrorCode AEntityFactory::get_associated_meshsets( MBEntityHandle source_entity, 
                                                      std::vector<MBEntityHandle> &target_entities )
{

  MBErrorCode result;
  
  const MBEntityHandle* adj_vec;
  int num_adj;
  result = get_adjacencies( source_entity, adj_vec, num_adj );
  if(result != MB_SUCCESS || adj_vec == NULL)
    return result;

  // find the meshsets in this vector 
  MBDimensionPair dim_pair = MBCN::TypeDimensionMap[4];
  int dum;
  const MBEntityHandle* start_ent =
    std::lower_bound(adj_vec, adj_vec+num_adj, CREATE_HANDLE(dim_pair.first, MB_START_ID, dum));
  const MBEntityHandle* end_ent =
    std::lower_bound(start_ent, adj_vec+num_adj, CREATE_HANDLE(dim_pair.second, MB_END_ID, dum));

  // copy the the meshsets 
  target_entities.insert( target_entities.end(), start_ent, end_ent );

  return result; 

}


//! get the element defined by the vertices in vertex_list, of the
//! type target_type, passing back in target_entity; if create_if_missing
//! is true and no entity is found, one is created; if create_adjacency_option
//! is >= 0, adjacencies from entities of that dimension to target_entity
//! are created (only create_adjacency_option=0 is supported right now,
//! so that never creates other ancillary entities)
MBErrorCode AEntityFactory::get_element(const MBEntityHandle *vertex_list,
                                         const int vertex_list_size,
                                         const MBEntityType target_type,
                                         MBEntityHandle &target_entity,
                                         const bool create_if_missing,
                                         const MBEntityHandle source_entity,
                                         const int /*create_adjacency_option*/) 
{

  // look over nodes to see if this entity already exists
  target_entity = 0;
  MBErrorCode result;
  const MBEntityHandle *i_adj, *end_adj;

  target_entity = 0;
  
  // need vertex adjacencies, so create if necessary
  if(mVertElemAdj == false)
    create_vert_elem_adjacencies();
  
  // get the adjacency list
  const MBEntityHandle* adj_vec;
  int num_adj;
  result = get_adjacencies( vertex_list[0], adj_vec, num_adj );
  if(result != MB_SUCCESS || adj_vec == NULL)
    return result;

  // check to see if any of these are equivalent to the vertex list
  int dum;

    // use a fixed-size array, for speed; there should never be more than 5 equivalent entities
  MBEntityHandle temp_vec[15];
  int temp_vec_size = 0;
  
  i_adj = std::lower_bound(adj_vec, adj_vec+num_adj, CREATE_HANDLE(target_type, MB_START_ID, dum));
  end_adj = std::lower_bound(i_adj, adj_vec+num_adj, CREATE_HANDLE(target_type, MB_END_ID, dum));
  for (; i_adj != end_adj; ++i_adj)
  {
    if (TYPE_FROM_HANDLE(*i_adj) != target_type) continue;

    if (true == entities_equivalent(*i_adj, vertex_list, vertex_list_size, target_type)) 
    {
      temp_vec[temp_vec_size++] = *i_adj;
    }
  }

  if (temp_vec_size == 0 && !create_if_missing)
    return result;
  
    // test for size against fixed-size array
  assert(temp_vec_size <= 15);
  
    // test for empty first, 'cuz it's cheap
  if (temp_vec_size == 0 && true == create_if_missing) {
    
    // Create the element with this handle (handle is a return type and should be the last parameter)
    result = thisMB->create_element(target_type, vertex_list, vertex_list_size,
                                     target_entity);
  }

    // next most likely is one entity
  else if (temp_vec_size == 1)
    target_entity = temp_vec[0];

    // least likely, most work - leave for last test
  else {
      // multiple entities found - look for direct adjacencies
    if (0 != source_entity) {
      
      const MBEntityHandle *adj_vec, *adj_iter;
      int num_adjs;
      for (dum = 0; dum < temp_vec_size; dum++) {
        result = get_adjacencies(temp_vec[dum], adj_vec, num_adjs);
        if ((adj_iter = std::find(adj_vec, (adj_vec+num_adjs), source_entity)) != (adj_vec+num_adjs)) {
            // found it, return it
          target_entity = temp_vec[dum];
          break;
        }
      }

      if (0 == target_entity && 
          thisMB->dimension_from_handle(source_entity) > MBCN::Dimension(target_type)+1) {
          // still have multiple entities, and source dimension is two greater than target,
          // so there may not be any explicit adjacencies between the two; look for common
          // entities of the intermediate dimension
        MeshTopoUtil mtu(thisMB);
        int intermed_dim = MBCN::Dimension(target_type)+1;
        for (dum = 0; dum < temp_vec_size; dum++) {
          if (0 != mtu.common_entity(temp_vec[dum], source_entity, intermed_dim)) {
            target_entity = temp_vec[dum];
            break;
          }
        }
      }
    }
    
    if (target_entity == 0) {
        // if we get here, we didn't find a matching adjacency; just take the first one, but
        // return a non-success result
      target_entity = temp_vec[0];
      result = MB_MULTIPLE_ENTITIES_FOUND;
    }
  }

  return result;
}

bool AEntityFactory::entities_equivalent(const MBEntityHandle this_entity, 
                                         const MBEntityHandle *vertex_list, 
                                         const int vertex_list_size,
                                         const MBEntityType target_type) 
{
    // compare vertices of this_entity with those in the list, returning true if they
    // represent the same element
  MBEntityType this_type = TYPE_FROM_HANDLE(this_entity);
  
  if (this_type != target_type) 
    return false;
  
  else if (this_type == MBVERTEX && (vertex_list_size > 1 || vertex_list[0] != this_entity)) 
    return false;
  
    // need to compare the actual vertices
  const MBEntityHandle *this_vertices;
  int num_this_vertices;
  std::vector<MBEntityHandle> storage;
  thisMB->get_connectivity(this_entity, this_vertices, num_this_vertices, false, &storage);
  
  // see if we can get one node id to match
  assert(vertex_list_size > 0);
  int num_corner_verts = ((this_type == MBPOLYGON || this_type == MBPOLYHEDRON) ?
                          num_this_vertices : MBCN::VerticesPerEntity(target_type));
  const MBEntityHandle *iter = 
    std::find(this_vertices, (this_vertices+num_corner_verts), vertex_list[0]);
  if(iter == (this_vertices+num_corner_verts))
    return false;

  // now lets do connectivity matching
  bool they_match = true;

  // line up our vectors
  int i;
  int offset = iter - this_vertices;
  
  // first compare forward
  for(i = 1; i<num_corner_verts; ++i)
  {
    if (i >= vertex_list_size) {
      they_match = false;
      break;
    }
    
    if(vertex_list[i] != this_vertices[(offset+i)%num_corner_verts])
    {
      they_match = false;
      break;
    }
  }

  if(they_match == true)
    return true;

  they_match = true;

  // then compare reverse
  // offset iter to avoid addition inside loop; this just makes sure we don't
  // go off beginning of this_vertices with an index < 0
  offset += num_corner_verts;
  for(i = 1; i < vertex_list_size; i++)
  {
    if(vertex_list[i] != this_vertices[(offset-i)%num_corner_verts])
    {
      they_match = false;
      break;
    }
  }
  return they_match;

}


//! add an adjacency from from_ent to to_ent; if both_ways is true, add one
//! in reverse too
//! NOTE: this function is defined even though we may only be implementing
//! vertex-based up-adjacencies
MBErrorCode AEntityFactory::add_adjacency(MBEntityHandle from_ent,
                                           MBEntityHandle to_ent,
                                           const bool both_ways) 
{
  MBEntityType to_type = TYPE_FROM_HANDLE(to_ent);

  if (to_type == MBVERTEX) 
    return MB_ALREADY_ALLOCATED;
  
  
  
  MBAdjacencyVector *adj_list_ptr = NULL;
  MBErrorCode result = get_adjacencies( from_ent, adj_list_ptr, true );
  if (MB_SUCCESS != result) 
    return result;

    // get an iterator to the right spot in this sorted vector
  MBAdjacencyVector::iterator adj_iter;
  if (!adj_list_ptr->empty()) 
  {
    adj_iter = std::lower_bound(adj_list_ptr->begin(), adj_list_ptr->end(),
                                to_ent);

    if ( adj_iter == adj_list_ptr->end() || to_ent != *adj_iter )
    {
      adj_list_ptr->insert(adj_iter, to_ent);
    }
  }
  else
    adj_list_ptr->push_back(to_ent);

    // if both_ways is true, recursively call this function
  if (true == both_ways && to_type != MBVERTEX)
    result = add_adjacency(to_ent, from_ent, false);
  
  return result;
}

//! remove an adjacency from from the base_entity.
MBErrorCode AEntityFactory::remove_adjacency(MBEntityHandle base_entity,
                              MBEntityHandle adj_to_remove)
{
  MBErrorCode result;

  if (TYPE_FROM_HANDLE(base_entity) == MBENTITYSET) 
    return thisMB->remove_entities(base_entity, &adj_to_remove, 1);

  // get the adjacency tag
  MBAdjacencyVector *adj_list = NULL;
  result = get_adjacencies( base_entity, adj_list );
  if (adj_list == NULL || MB_SUCCESS != result)
    return result;

  // remove the specified entity from the adjacency list and truncate
  // the list to the new length
  adj_list->erase(std::remove(adj_list->begin(), adj_list->end(), adj_to_remove), 
                  adj_list->end());

  return result;
}

//! remove all adjacencies from from the base_entity.
MBErrorCode AEntityFactory::remove_all_adjacencies(MBEntityHandle base_entity,
                                                   const bool delete_adj_list)
{
  MBErrorCode result;
  MBEntityType base_type = TYPE_FROM_HANDLE(base_entity);

  if (base_type == MBENTITYSET) 
    return thisMB->clear_meshset(&base_entity, 1);
  const int base_ent_dim = MBCN::Dimension( base_type );

    // Remove adjacencies from element vertices back to 
    // this element.  Also check any elements adjacent
    // to the vertex and of higher dimension than this
    // element for downward adjacencies to this element.
  if (vert_elem_adjacencies() && base_type != MBVERTEX) {
    MBEntityHandle const *connvect = 0, *adjvect = 0;
    int numconn = 0, numadj = 0;
    std::vector<MBEntityHandle> connstorage;
    result = get_vertices( base_entity, connvect, numconn, connstorage );
    if (MB_SUCCESS != result) 
      return result;
    
    for (int i = 0; i < numconn; ++i) {
      result = get_adjacencies( connvect[i], adjvect, numadj );
      if (MB_SUCCESS != result)
        return result;
      
      bool remove_this = false;
      for (int j = 0; j < numadj; ++j) {
        if (adjvect[j] == base_entity)
          remove_this = true;
        
        if (MBCN::Dimension(TYPE_FROM_HANDLE(adjvect[j])) != base_ent_dim 
         && explicitly_adjacent( adjvect[j], base_entity )) 
          remove_adjacency( adjvect[j], base_entity );
      }
      
      if (remove_this)
        remove_adjacency( connvect[i], base_entity );
    }
  }
  
  // get the adjacency tag
  MBAdjacencyVector *adj_list = 0;
  result = get_adjacencies( base_entity, adj_list );
  if (MB_SUCCESS != result || !adj_list)
    return result;
  
  
    // check adjacent entities for references back to this entity
  for (MBAdjacencyVector::reverse_iterator it = adj_list->rbegin(); it != adj_list->rend(); ++it) 
    remove_adjacency( *it, base_entity );
  
  if (delete_adj_list)
    result = set_adjacency_ptr( base_entity, NULL );
  else
    adj_list->clear();

  return MB_SUCCESS;
}

MBErrorCode AEntityFactory::create_vert_elem_adjacencies()
{

  mVertElemAdj = true;

  MBEntityType ent_type;
  MBRange::iterator i_range;
  const MBEntityHandle *connectivity;
  std::vector<MBEntityHandle> aux_connect;
  int number_nodes;
  MBErrorCode result;
  MBRange handle_range;
  
  // 1. over all element types, for each element, create vertex-element adjacencies
  for (ent_type = MBEDGE; ent_type != MBENTITYSET; ent_type++) 
  {
    handle_range.clear();

    // get this type of entity
    result = thisMB->get_entities_by_type(0, ent_type, handle_range);
    if (result != MB_SUCCESS)
      return result;

    for (i_range = handle_range.begin(); i_range != handle_range.end(); ++i_range) 
    {
      result = get_vertices( *i_range, connectivity, number_nodes, aux_connect );
      if (MB_SUCCESS != result)
        return result;
      
        // add the adjacency
      for( int k=0; k<number_nodes; k++)
        if ((result = add_adjacency(connectivity[k], *i_range)) != MB_SUCCESS)
          return result;
    }
  }

  return MB_SUCCESS;
}


MBErrorCode AEntityFactory::get_adjacencies(MBEntityHandle entity,
                                            const MBEntityHandle *&adjacent_entities,
                                            int &num_entities) const
{
  MBAdjacencyVector const* vec_ptr = 0;
  MBErrorCode result = get_adjacency_ptr( entity, vec_ptr );
  if (MB_SUCCESS != result || !vec_ptr) {
    adjacent_entities = 0;
    num_entities = 0;
    return result;
  }
  
  num_entities = vec_ptr->size();
  adjacent_entities = &((*vec_ptr)[0]);
  return MB_SUCCESS;
}

MBErrorCode AEntityFactory::get_adjacencies(MBEntityHandle entity,
                                            std::vector<MBEntityHandle>& adjacent_entities) const
{
  MBAdjacencyVector const* vec_ptr = 0;
  MBErrorCode result = get_adjacency_ptr( entity, vec_ptr );
  if (MB_SUCCESS != result || !vec_ptr) {
    adjacent_entities.clear();
    return result;
  }
  
  adjacent_entities = *vec_ptr;
  return MB_SUCCESS;
}

MBErrorCode AEntityFactory::get_adjacencies( MBEntityHandle entity,
                                             std::vector<MBEntityHandle>*& adj_vec,
                                             bool create )
{
  adj_vec = 0;
  MBErrorCode result = get_adjacency_ptr( entity, adj_vec );
  if (MB_SUCCESS == result && !adj_vec && create) {
    adj_vec = new MBAdjacencyVector;
    result = set_adjacency_ptr( entity, adj_vec );
    if (MB_SUCCESS != result) {
      delete adj_vec;
      adj_vec = 0;
    }
  }
  return result;
}

MBErrorCode AEntityFactory::get_adjacencies( const MBEntityHandle source_entity,
                                             const unsigned int target_dimension,
                                             bool create_if_missing,
                                             std::vector<MBEntityHandle> &target_entities )
{
  const MBEntityType source_type = TYPE_FROM_HANDLE(source_entity);
  const unsigned source_dimension = MBCN::Dimension(source_type);

  MBErrorCode result;
  if (target_dimension == 4) { //get meshsets 'source' is in
    result = get_associated_meshsets( source_entity, target_entities ); 
  }
  else if (target_dimension == (source_type != MBPOLYHEDRON ? 0 : 2)) {
    result = thisMB->get_connectivity(&source_entity, 1, target_entities);
  }
  else if (target_dimension == 0 && source_type == MBPOLYHEDRON) {
    result = get_polyhedron_vertices(source_entity, target_entities);
  }
  else if (source_dimension == target_dimension) {
    target_entities.push_back( source_entity );
    result = MB_SUCCESS;
  }
  else {
    if(mVertElemAdj == false) {
      result = create_vert_elem_adjacencies();
      if (MB_SUCCESS != result) return result;
    }

    if(source_dimension == 0)
    {
      result = get_zero_to_n_elements(source_entity, target_dimension,
        target_entities, create_if_missing);
    }
    else if(source_dimension > target_dimension)
    {
      result = get_down_adjacency_elements(source_entity, target_dimension,
                                           target_entities, create_if_missing);
    }
    else //if(source_dimension < target_dimension)
    {
      result = get_up_adjacency_elements( source_entity, target_dimension,
             target_entities, create_if_missing);
    }
  }

  return result;
}


MBErrorCode AEntityFactory::notify_create_entity(const MBEntityHandle entity, 
                                                 const MBEntityHandle *node_array,
                                                 const int number_nodes)
{
  MBErrorCode result = MB_SUCCESS, tmp_result;
  if( vert_elem_adjacencies())
  {
    //iterate through nodes and add adjacency information
    if (TYPE_FROM_HANDLE(entity) == MBPOLYHEDRON) {
        // polyhedron - get real vertex connectivity
      std::vector<MBEntityHandle> verts;
      tmp_result = get_adjacencies(entity, 0, false, verts);
      if (MB_SUCCESS != tmp_result) return tmp_result;
      for (std::vector<MBEntityHandle>::iterator vit = verts.begin(); 
           vit != verts.end(); vit++) 
      {
        tmp_result = add_adjacency(*vit, entity);
        if (MB_SUCCESS != tmp_result) result = tmp_result;
      }
    }
    else {
      for(unsigned int i=number_nodes; i--;)
      {
        tmp_result = add_adjacency(node_array[i], entity);
        if (MB_SUCCESS != tmp_result) result = tmp_result;
      }
    }
  }
  
  return result;
}

MBErrorCode AEntityFactory::get_zero_to_n_elements(MBEntityHandle source_entity,
                            const unsigned int target_dimension,
                            std::vector<MBEntityHandle> &target_entities,
                            const bool create_if_missing,
                            const int /*create_adjacency_option = -1*/)
{
  MBAdjacencyVector::iterator start_ent, end_ent;

  // get the adjacency vector
  MBAdjacencyVector *adj_vec = NULL;
  MBErrorCode result = get_adjacencies( source_entity, adj_vec );
  if(result != MB_SUCCESS || adj_vec == NULL)
    return result;
  
  if (target_dimension < 3 && create_if_missing) {
      std::vector<MBEntityHandle> tmp_ents;
      
      start_ent = std::lower_bound(adj_vec->begin(), adj_vec->end(), 
                         FIRST_HANDLE(MBCN::TypeDimensionMap[target_dimension+1].first));

      end_ent = std::lower_bound(start_ent, adj_vec->end(), 
                         LAST_HANDLE(MBCN::TypeDimensionMap[3].second));
      
      std::vector<MBEntityHandle> elems(start_ent, end_ent);
 
      // make target_dimension elements from all adjacient higher-dimension elements
      for(start_ent = elems.begin(); start_ent != elems.end(); ++start_ent)
      {
        tmp_ents.clear();
        get_down_adjacency_elements(*start_ent, target_dimension, tmp_ents, create_if_missing, 0);
      }
  }
    
  MBDimensionPair dim_pair = MBCN::TypeDimensionMap[target_dimension];
  start_ent = std::lower_bound(adj_vec->begin(), adj_vec->end(), FIRST_HANDLE(dim_pair.first ));
  end_ent   = std::lower_bound(start_ent,        adj_vec->end(), LAST_HANDLE (dim_pair.second));
  target_entities.insert( target_entities.end(), start_ent, end_ent );
  return MB_SUCCESS;  
}

MBErrorCode AEntityFactory::get_down_adjacency_elements(MBEntityHandle source_entity,
                                                         const unsigned int target_dimension,
                                                         std::vector<MBEntityHandle> &target_entities,
                                                         const bool create_if_missing,
                                                         const int create_adjacency_option)
{

  MBEntityType source_type = TYPE_FROM_HANDLE(source_entity);

  if (source_type == MBPOLYHEDRON ||
      source_type == MBPOLYGON) 
    return get_down_adjacency_elements_poly(source_entity, target_dimension,
                                            target_entities, create_if_missing, 
                                            create_adjacency_option);
  
    // make this a fixed size to avoid cost of working with STL vectors
  MBEntityHandle vertex_array[27];
  MBErrorCode temp_result;

  const MBEntityHandle *vertices;
  int num_verts;
  
    // I know there are already vertex adjacencies for this - call
    // another function to get them
  std::vector<MBEntityHandle> storage;
  MBErrorCode result = thisMB->get_connectivity(source_entity, vertices, num_verts, false, &storage);
  if (MB_SUCCESS != result) return result;

  int has_mid_nodes[4];
  MBCN::HasMidNodes(source_type, num_verts, has_mid_nodes);
  
  std::vector<int> index_list;
  int num_sub_ents = MBCN::NumSubEntities(source_type, target_dimension);
  
  for( int j=0; j<num_sub_ents; j++)
  {
    const MBCN::ConnMap &cmap = 
      MBCN::mConnectivityMap[source_type][target_dimension-1];
  
    int verts_per_sub = cmap.num_corners_per_sub_element[j];

      // get the corner vertices
    for (int i = 0; i < verts_per_sub; i++)
      vertex_array[i] = vertices[cmap.conn[j][i]];

      // get the ho nodes for sub-subfacets
    if (has_mid_nodes[1] && target_dimension > 1) {
        // has edge mid-nodes; for each edge, get the right mid-node and put in vertices
        // first get the edge indices
      index_list.clear();
      int int_result = MBCN::AdjacentSubEntities(source_type, &j, 1, 
                                                   target_dimension, 1, index_list);
      if (0 != int_result) return MB_FAILURE;
      for (unsigned int k = 0; k < index_list.size(); k++) {
        int tmp_index = MBCN::HONodeIndex(source_type, num_verts, 1,
                                            index_list[k]);
        if (tmp_index >= (int) num_verts) return MB_INDEX_OUT_OF_RANGE;

          // put this vertex on the end; reuse verts_per_sub as an index
        vertex_array[verts_per_sub++] = vertices[tmp_index];
      }
    }
      // get the ho nodes for the target dimension
    if (has_mid_nodes[target_dimension]) {
        // get the ho node index for this subfacet
      int tmp_index = MBCN::HONodeIndex(source_type, num_verts,
                                          target_dimension, j);
      if (tmp_index >= num_verts) return MB_INDEX_OUT_OF_RANGE;
      vertex_array[verts_per_sub++] = vertices[tmp_index];
    }

    MBEntityHandle tmp_target = 0;
    temp_result = get_element(vertex_array, verts_per_sub,
                              cmap.target_type[j], tmp_target, 
                              create_if_missing, source_entity, create_adjacency_option);
        
    if (temp_result != MB_SUCCESS) result = temp_result;
    else if (0 != tmp_target) target_entities.push_back(tmp_target);

      // make sure we're not writing past the end of our fixed-size array
    if (verts_per_sub > 27) return MB_INDEX_OUT_OF_RANGE;
  }
  
  return result;
}

MBErrorCode AEntityFactory::get_down_adjacency_elements_poly(MBEntityHandle source_entity,
                                                             const unsigned int target_dimension,
                                                             std::vector<MBEntityHandle> &target_entities,
                                                             const bool /*create_if_missing*/,
                                                             const int /*create_adjacency_option*/)
{

  MBEntityType source_type = TYPE_FROM_HANDLE(source_entity);

  if (!(source_type == MBPOLYHEDRON && target_dimension > 0 && target_dimension < 3) &&
      (!source_type == MBPOLYGON && target_dimension == 1)) 
    return MB_TYPE_OUT_OF_RANGE;
  
    // make this a fixed size to avoid cost of working with STL vectors
  std::vector<MBEntityHandle> vertex_array;

    // I know there are already vertex adjacencies for this - call
    // another function to get them
  MBErrorCode result = get_adjacencies(source_entity, 0, false, vertex_array);
  if (MB_SUCCESS != result) return result;

  if (target_dimension == 0) {
    target_entities.insert( target_entities.end(), vertex_array.begin(), vertex_array.end() );
    return MB_SUCCESS;
  }

  MBErrorCode tmp_result;
  if (source_type == MBPOLYGON) {
    result = MB_SUCCESS;
      // put the first vertex on the end so we have a ring
    vertex_array.push_back(*vertex_array.begin());
    for (unsigned int i = 0; i < vertex_array.size()-1; i++) {
      MBRange vrange, adj_edges;
      vrange.insert(vertex_array[i]);
      vrange.insert(vertex_array[i+1]);
      tmp_result = thisMB->get_adjacencies(vrange, 1, false, adj_edges);
      if (MB_SUCCESS != tmp_result) result = tmp_result;
      if (adj_edges.size() == 1) {
          // single edge - don't check adjacencies
        target_entities.push_back(*adj_edges.begin());
      }
      else if (adj_edges.size() != 0) {
          // multiple ones - need to check for explicit adjacencies
        unsigned int start_sz = target_entities.size();
        const MBEntityHandle *explicit_adjs;
        int num_exp;
        for (MBRange::iterator rit = adj_edges.begin(); rit != adj_edges.end(); rit++) {
          this->get_adjacencies(*rit, explicit_adjs, num_exp);
          if (NULL != explicit_adjs &&
              std::find(explicit_adjs, explicit_adjs+num_exp, source_entity) != 
              explicit_adjs+num_exp)
            target_entities.push_back(*rit);
        }
        if (target_entities.size() == start_sz) {
          result = MB_MULTIPLE_ENTITIES_FOUND;
          target_entities.push_back(*adj_edges.begin());
        }
      }
    }
    return result;
  }
  
  else {
    if (target_dimension == 2) {
      result = thisMB->get_connectivity(&source_entity, 1, target_entities);
    }
    else {
      std::vector<MBEntityHandle> dum_vec;
      result = thisMB->get_connectivity(&source_entity, 1, dum_vec);
      if (MB_SUCCESS != result) return result;
      result = thisMB->get_adjacencies(&dum_vec[0], dum_vec.size(), 1, false,
                                       target_entities, MBInterface::UNION);
      return result;
    }
  }

  return MB_SUCCESS;
}

#if 0
// Do in-place set intersect of two *sorted* containers.
// First container is modifed.  Second is not.  
// First container must allow assignment through iterators (in practice,
// must be a container that does not enforce ordering, such
// as std::vector, std::list, or a c-style array)
template <typename T1, typename T2>
static inline T1 intersect( T1 set1_begin, T1 set1_end,
                            T2 set2_begin, T2 set2_end )
{
  T1 set1_write = set1_begin;
  while (set1_begin != set1_end) {
    if (set2_begin == set2_end)
      return set1_write;
    while (*set2_begin < *set1_begin)
      if (++set2_begin == set2_end)
        return set1_write;
    if (!(*set1_begin < *set2_begin)) {
      *set1_write = *set1_begin;
      ++set1_write;
      ++set2_begin;
    }
    ++set1_begin;
  }
  return set1_write;
}
      

MBErrorCode AEntityFactory::get_up_adjacency_elements( 
                                   MBEntityHandle source_entity,
                                   const unsigned int target_dimension,
                                   std::vector<MBEntityHandle>& target_entities,
                                   const bool create_if_missing,
                                   const int option )
{
  MBErrorCode rval;
  const std::vector<MBEntityHandle> *vtx_adj, *vtx2_adj;
  std::vector<MBEntityHandle> duplicates;
  
    // Handle ranges
  const size_t in_size = target_entities.size();
  const MBEntityType src_type = TYPE_FROM_HANDLE(source_entity);
  MBDimensionPair target_types = MBCN::TypeDimensionMap[target_dimension];
  const MBEntityHandle src_beg_handle = CREATE_HANDLE( src_type, 0 );
  const MBEntityHandle src_end_handle = CREATE_HANDLE( src_type+1, 0 );
  const MBEntityHandle tgt_beg_handle = CREATE_HANDLE( target_types.first, 0 );
  const MBEntityHandle tgt_end_handle = CREATE_HANDLE( target_types.second+1, 0 );
  
    // get vertices
  assert(TYPE_FROM_HANDLE(source_entity) != MBPOLYHEDRON); // can't go up from a region
  std::vector<MBEntityHandle> conn_storage;
  const MBEntityHandle* conn;
  int conn_len;
  rval = thisMB->get_connectivity( source_entity, conn, conn_len, true, &conn_storage );
  if (MB_SUCCESS != rval)
    return rval;
  
    // shouldn't be here if source entity is not an element
  assert(conn_len > 1);

    // create necessary entities. this only makes sense if there exists of a
    // dimension greater than the target dimension.
  if (create_if_missing && target_dimension < 3 && MBCN::Dimension(src_type) < 2) {
    for (size_t i = 0; i < conn_len; ++i) {
      rval = get_adjacency_ptr( conn[i], vtx_adj );
      if (MB_SUCCESS != rval)
        return rval;
      assert(vtx_adj != NULL); // should contain at least source_entity

      std::vector<MBEntityHandle> tmp2, tmp(*vtx_adj); // copy in case adjacency vector is changed
      for (size_t j = 0; j < tmp.size(); ++j) {
        if (MBCN::Dimension(TYPE_FROM_HANDLE(tmp[j])) <= (int)target_dimension)
          continue;
        if (TYPE_FROM_HANDLE(tmp[j]) == MBENTITYSET)
          break;
    
        tmp2.clear();
        rval = get_down_adjacency_elements( tmp[j], target_dimension, tmp2, true, option );
        if (MB_SUCCESS != rval)
          return rval;
      }
    }
  }
  
    // get elements adjacent to first vertex 
  rval = get_adjacency_ptr( conn[0], vtx_adj );
  if (MB_SUCCESS != rval)
    return rval;
  assert(vtx_adj != NULL); // should contain at least source_entity
    // get elements adjacent to second vertex
  rval = get_adjacency_ptr( conn[1], vtx2_adj );
  if (MB_SUCCESS != rval)
    return rval;
  assert(vtx2_adj != NULL);

    // Put intersect of all entities except source entity with
    // the same type as the source entity in 'duplicates'
  std::vector<MBEntityHandle>::const_iterator it1, it2, end1, end2;
  it1 = std::lower_bound( vtx_adj->begin(), vtx_adj->end(), src_beg_handle );
  it2 = std::lower_bound( vtx2_adj->begin(), vtx2_adj->end(), src_beg_handle );
  end1 = std::lower_bound( it1, vtx_adj->end(), src_end_handle );
  end2 = std::lower_bound( it2, vtx2_adj->end(), src_end_handle );
  assert(end1 != it1); // should at least contain source entity
  duplicates.resize( end1 - it1 - 1 );
  std::vector<MBEntityHandle>::iterator ins = duplicates.begin();
  for (; it1 != end1; ++it1) {
    if (*it1 != source_entity) {
      *ins = *it1;
      ++ins;
    }
  }
  duplicates.erase( intersect( duplicates.begin(), duplicates.end(), it2, end2 ), duplicates.end() );
    
    // Append to input list any entities of the desired target dimension
  it1 = std::lower_bound( end1, vtx_adj->end(), tgt_beg_handle );
  it2 = std::lower_bound( end2, vtx2_adj->end(), tgt_beg_handle );
  end1 = std::lower_bound( it1, vtx_adj->end(), tgt_end_handle );
  end2 = std::lower_bound( it2, vtx2_adj->end(), tgt_end_handle );
  std::set_intersection( it1, end1, it2, end2, std::back_inserter( target_entities ) );
  
    // for each additional vertex
  for (int i = 2; i < conn_len; ++i) {
    rval = get_adjacency_ptr( conn[i], vtx_adj );
    if (MB_SUCCESS != rval)
      return rval;
    assert(vtx_adj != NULL); // should contain at least source_entity
    
    it1 = std::lower_bound( vtx_adj->begin(), vtx_adj->end(), src_beg_handle );
    end1 = std::lower_bound( it1, vtx_adj->end(), src_end_handle );
    duplicates.erase( intersect( duplicates.begin(), duplicates.end(), it1, end1 ), duplicates.end() );
    
    it1 = std::lower_bound( end1, vtx_adj->end(), tgt_beg_handle );
    end1 = std::lower_bound( it1, vtx_adj->end(), tgt_end_handle );
    target_entities.erase( intersect( target_entities.begin()+in_size, target_entities.end(),
                           it1, end1 ), target_entities.end() );
  }
  
    // if no duplicates, we're done
  if (duplicates.empty())
    return MB_SUCCESS;
  
    // Check for explicit adjacencies.  If an explicit adjacency
    // connects candidate target entity to an entity equivalent
    // to the source entity, then assume that source entity is *not*
    // adjacent
  const std::vector<MBEntityHandle>* adj_ptr;
    // check adjacencies from duplicate entities to candidate targets
  for (size_t i = 0; i < duplicates.size(); ++i) {
    rval = get_adjacency_ptr( duplicates[i], adj_ptr );
    if (MB_SUCCESS != rval)
      return rval;
    if (!adj_ptr)
      continue;
    
    for (size_t j = 0; j < adj_ptr->size(); ++j) {
      std::vector<MBEntityHandle>::iterator k = 
        std::find( target_entities.begin()+in_size, target_entities.end(), (*adj_ptr)[j] );
      if (k != target_entities.end())
        target_entities.erase(k);
    }
  }
  
  // If target dimension is 3 and source dimension is 1, also need to
  // check for explicit adjacencies to intermediate faces
  if (MBCN::Dimension(src_type) > 1 || target_dimension < 3)
    return MB_SUCCESS;
  
    // Get faces adjacent to each element and check for explict 
    // adjacencies from duplicate entities to faces
  for (size_t i = 0; i < duplicates.size(); ++i) {
    rval = get_adjacency_ptr( duplicates[i], adj_ptr );
    if (MB_SUCCESS != rval)
      return rval;
    if (!adj_ptr)
      continue;
    
    size_t j;
    for (j = 0; j < adj_ptr->size(); ++j) {
      const std::vector<MBEntityHandle>* adj_ptr2;
      rval = get_adjacency_ptr( (*adj_ptr)[j], adj_ptr2 );
      if (MB_SUCCESS != rval)
        return rval;
      if (!adj_ptr2)
        continue;
      
      for (size_t k = 0; k < adj_ptr2->size(); ++k) {
        std::vector<MBEntityHandle>::iterator it;
        it = std::find( target_entities.begin()+in_size, target_entities.end(), (*adj_ptr2)[k] );
        if (it != target_entities.end()) { 
          target_entities.erase(it);
          j = adj_ptr->size(); // break outer loop
          break;
        }
      }
    }
  }
  
  return MB_SUCCESS;
}
#else
MBErrorCode AEntityFactory::get_up_adjacency_elements(MBEntityHandle source_entity,
                                                       const unsigned int target_dimension,
                                                       std::vector<MBEntityHandle> &target_entities,
                                                       const bool create_if_missing,
                                                       const int /*create_adjacency_option = -1*/)
{

  MBEntityType source_type = TYPE_FROM_HANDLE(source_entity);

  const MBEntityHandle *source_vertices;
  int num_source_vertices;
  std::vector<MBEntityHandle> conn_storage;
  
    // check to see whether there are any equivalent entities (same verts, different entity);
    // do this by calling get_element with a 0 source_entity, and look for a MB_MULTIPLE_ENTITIES_FOUND
    // return code

    // NOTE: we only want corner vertices here, and for the code below which also uses
    // source_vertices
  MBErrorCode result = 
    thisMB->get_connectivity(source_entity, source_vertices, num_source_vertices, true, &conn_storage);
  if (MB_SUCCESS != result) return result;
  MBEntityHandle temp_entity;
  result = get_element(source_vertices, num_source_vertices,
                       source_type, temp_entity,
                       false, 0);

  bool equiv_entities = (result == MB_MULTIPLE_ENTITIES_FOUND) ? true : false;
  
  std::vector<MBEntityHandle> tmp_vec;
  if (!equiv_entities) {
      // get elems adjacent to each node
    std::vector< std::vector<MBEntityHandle> > elems(num_source_vertices);
    int i;
    for(i=0; i < num_source_vertices; i++)
    {
        // get elements
        // see comment above pertaining to source_vertices; these are corner vertices only
      get_zero_to_n_elements(source_vertices[i], target_dimension, elems[i], create_if_missing, 0);
        // sort this element list
      std::sort(elems[i].begin(), elems[i].end());
    }

      // perform an intersection between all the element lists
      // see comment above pertaining to source_vertices; these are corner vertices only
    for(i=1; i<num_source_vertices; i++)
    {
      tmp_vec.clear();
      
        // intersection between first list and ith list, put result in tmp
      std::set_intersection(elems[0].begin(), elems[0].end(), elems[i].begin(), elems[i].end(),
                            std::back_insert_iterator< std::vector<MBEntityHandle> >(tmp_vec));
        // tmp has elems[0] contents and elems[0] contents has tmp's contents
        // so that elems[0] always has the intersection of previous operations
      elems[0].swap(tmp_vec);
    }

      // elems[0] contains the intersection, swap with target_entities
    target_entities.insert( target_entities.end(), elems[0].begin(), elems[0].end() );
  }
  else if (source_type == MBPOLYGON) {
      // get adjacencies using polyhedra's connectivity vectors
      // first get polyhedra neighboring vertices
    result = thisMB->get_adjacencies(source_vertices, num_source_vertices, 3, false,
                                     tmp_vec);
    if (MB_SUCCESS != result) return result;
    
      // now filter according to whether each is adjacent to the polygon
    const MBEntityHandle *connect;
    int num_connect;
    std::vector<MBEntityHandle> storage;
    for (unsigned int i = 0; i < tmp_vec.size(); i++) {
      result = thisMB->get_connectivity(tmp_vec[i], connect, num_connect, false, &storage);
      if (MB_SUCCESS != result) return result;
      if (std::find(connect, connect+num_connect, source_entity) != connect+num_connect)
        target_entities.push_back(tmp_vec[i]);
    }
  }
  
  else {
      // else get up-adjacencies directly; code copied from get_zero_to_n_elements

      // get the adjacency vector
    MBAdjacencyVector *adj_vec = NULL;
    result = get_adjacencies( source_entity, adj_vec );
                    
    if(result != MB_SUCCESS)
      return result;
    else if (adj_vec == NULL)
      return MB_SUCCESS;

    MBDimensionPair dim_pair_dp1 = MBCN::TypeDimensionMap[MBCN::Dimension(source_type)+1],
      dim_pair_td = MBCN::TypeDimensionMap[target_dimension];
    int dum;

    MBRange tmp_ents, target_ents;

      // get iterators for start handle of source_dim+1 and target_dim, and end handle
      // of target_dim
    MBAdjacencyVector::iterator 
      start_ent_dp1 = std::lower_bound(adj_vec->begin(), adj_vec->end(), 
                                       CREATE_HANDLE(dim_pair_dp1.first, MB_START_ID, dum)),
       
      start_ent_td = std::lower_bound(adj_vec->begin(), adj_vec->end(), 
                                      CREATE_HANDLE(dim_pair_td.first, MB_START_ID, dum)),
       
      end_ent_td = std::lower_bound(adj_vec->begin(), adj_vec->end(), 
                                    CREATE_HANDLE(dim_pair_td.second, MB_END_ID, dum));

      // get the adjacencies for source_dim+1 to target_dim-1, and the adjacencies from
      // those to target_dim
    std::copy(start_ent_dp1, start_ent_td, mb_range_inserter(tmp_ents));
    MBErrorCode result = thisMB->get_adjacencies(tmp_ents, target_dimension, false,
                                                 target_ents, MBInterface::UNION);
    if (MB_SUCCESS != result) return result;
    
      // now copy the explicit adjacencies to target_dimension
    std::copy(start_ent_td, end_ent_td, mb_range_inserter(target_ents));
    
      // now insert the whole thing into the argument vector
    target_entities.insert( target_entities.end(), target_ents.begin(), target_ents.end() );
  }

  return result;
}
#endif


MBErrorCode AEntityFactory::notify_change_connectivity(MBEntityHandle entity, 
                                                        const MBEntityHandle* old_array, 
                                                        const MBEntityHandle* new_array, 
                                                        int number_verts)
{
  MBEntityType source_type = TYPE_FROM_HANDLE(entity);
  if (source_type == MBPOLYHEDRON || source_type == MBPOLYGON)
    return MB_NOT_IMPLEMENTED;

  // find out which ones to add and which to remove
  std::vector<MBEntityHandle> old_verts, new_verts;
  int i;
  for (i = 0; i < number_verts; i++) {
    if (old_array[i] != new_array[i]) {
      old_verts.push_back(old_array[i]);
      new_verts.push_back(new_array[i]);
    }
  }

  MBErrorCode result;
  
  if (mVertElemAdj == true) {
      // update the vertex-entity adjacencies
    std::vector<MBEntityHandle>::iterator adj_iter;
    for (adj_iter = old_verts.begin(); adj_iter != old_verts.end(); adj_iter++) {
      if (std::find(new_verts.begin(), new_verts.end(), *adj_iter) == new_verts.end()) {
        result = remove_adjacency(*adj_iter, entity);
        if (MB_SUCCESS != result) return result;
      }
    }
    for (adj_iter = new_verts.begin(); adj_iter != new_verts.end(); adj_iter++) {
      if (std::find(old_verts.begin(), old_verts.end(), *adj_iter) == old_verts.end()) {
        result = add_adjacency(*adj_iter, entity);
        if (MB_SUCCESS != result) return result;
      }
    }
  }

  return MB_SUCCESS;
}

    //! return true if 2 entities are explicitly adjacent
bool AEntityFactory::explicitly_adjacent(const MBEntityHandle ent1,
                                         const MBEntityHandle ent2) 
{
  const MBEntityHandle *explicit_adjs;
  int num_exp;
  get_adjacencies(ent1, explicit_adjs, num_exp);
  if (std::find(explicit_adjs, explicit_adjs+num_exp, ent2) != explicit_adjs+num_exp)
    return true;
  else
    return false;
}

MBErrorCode AEntityFactory::merge_adjust_adjacencies(MBEntityHandle entity_to_keep,
                                                     MBEntityHandle entity_to_remove) 
{
  int ent_dim = MBCN::Dimension(TYPE_FROM_HANDLE(entity_to_keep));
  MBErrorCode result;
  
    // check for newly-formed equivalent entities, and create explicit adjacencies
    // to distinguish them; this must be done before connectivity of higher-dimensional
    // entities is changed below, and only needs to be checked if merging vertices
  if (ent_dim == 0) {
    result = check_equiv_entities(entity_to_keep, entity_to_remove);
    if (MB_SUCCESS != result) return result;
  }
  
    // check adjacencies TO removed entity
  for (int dim = 1; dim < ent_dim; dim++) {
    MBRange adjs;
    result = thisMB->get_adjacencies(&entity_to_remove, 1, dim, false, adjs);
    if(result != MB_SUCCESS)
      return result;
      // for any explicit ones, make them adjacent to keeper
    for (MBRange::iterator rit = adjs.begin(); rit != adjs.end(); rit++) {
      if (this->explicitly_adjacent(*rit, entity_to_remove)) {
        result = this->add_adjacency(*rit, entity_to_keep);
        if(result != MB_SUCCESS) return result;
      }
    }
  }

    // check adjacencies FROM removed entity
  std::vector<MBEntityHandle> conn, adjs;
  result = this->get_adjacencies(entity_to_remove, adjs);
  if(result != MB_SUCCESS)
    return result;
    // set them all, and if to_entity is a set, add to that one too
  for (unsigned int i = 0; i < adjs.size(); i++) {
    if (TYPE_FROM_HANDLE(adjs[i]) == MBENTITYSET) 
    {
      //result = this->add_adjacency(entity_to_keep, adjs[i]);
      //if(result != MB_SUCCESS) return result;
      //result = thisMB->add_entities(adjs[i], &entity_to_keep, 1);
      //if(result != MB_SUCCESS) return result;
      result = thisMB->replace_entities( adjs[i], &entity_to_remove, &entity_to_keep, 1 );
      if (MB_SUCCESS != result)
        return result;
    }
    else if(ent_dim == 0)
    {
      conn.clear();
      result = thisMB->get_connectivity(&adjs[i], 1, conn);

      if(result == MB_SUCCESS)
      {
        std::replace(conn.begin(), conn.end(), entity_to_remove, entity_to_keep);
        result = thisMB->set_connectivity(adjs[i], &conn[0], conn.size());
        if (MB_SUCCESS != result) return result;
      }
      else return result;
    }
    else {
      result = this->add_adjacency(entity_to_keep, adjs[i]);
      if(result != MB_SUCCESS) return result;
    }
  }

  return MB_SUCCESS;
}

// check for equivalent entities that may be formed when merging two entities, and
// create explicit adjacencies accordingly
MBErrorCode AEntityFactory::check_equiv_entities(MBEntityHandle entity_to_keep,
                                                 MBEntityHandle entity_to_remove) 
{
  if (thisMB->dimension_from_handle(entity_to_keep) > 0) return MB_SUCCESS;

    // get all the adjacencies for both entities for all dimensions > 0
  MBRange adjs_keep, adjs_remove;
  MBErrorCode result;
  
  for (int dim = 1; dim <= 3; dim++) {
    result = thisMB->get_adjacencies(&entity_to_keep, 1, dim, false, adjs_keep,
                                     MBInterface::UNION);
    if (MB_SUCCESS != result) return result;
    result = thisMB->get_adjacencies(&entity_to_remove, 1, dim, false, adjs_remove,
                                     MBInterface::UNION);
    if (MB_SUCCESS != result) return result;
  }

    // now look for equiv entities which will be formed
    // algorithm:
    // for each entity adjacent to removed entity:
  MBEntityHandle two_ents[2];
  for (MBRange::iterator rit_rm = adjs_remove.begin(); rit_rm != adjs_remove.end(); rit_rm++) {
    two_ents[0] = *rit_rm;
    
      // - for each entity of same dimension adjacent to kept entity:
    for (MBRange::iterator rit_kp = adjs_keep.begin(); rit_kp != adjs_keep.end(); rit_kp++) {
      if (TYPE_FROM_HANDLE(*rit_kp) != TYPE_FROM_HANDLE(*rit_rm)) continue;
      
      MBRange all_verts;
      two_ents[1] = *rit_kp;
      //   . get union of adjacent vertices to two entities
      result = thisMB->get_adjacencies(two_ents, 2, 0, false, all_verts, MBInterface::UNION);
      if (MB_SUCCESS != result) return result;

      assert(all_verts.find(entity_to_keep) != all_verts.end() && 
             all_verts.find(entity_to_remove) != all_verts.end());
      
      
      //   . if # vertices != number of corner vertices + 1, continue
      if (MBCN::VerticesPerEntity(TYPE_FROM_HANDLE(*rit_rm))+1 != (int) all_verts.size()) continue;
      
      //   . for the two entities adjacent to kept & removed entity:
      result = create_explicit_adjs(*rit_rm);
      if (MB_SUCCESS != result) return result;
      result = create_explicit_adjs(*rit_kp);
      if (MB_SUCCESS != result) return result;
      //   . (end for)
    }
      // - (end for)
  }
  
  return MB_SUCCESS;
}

MBErrorCode AEntityFactory::create_explicit_adjs(MBEntityHandle this_ent) 
{
    //     - get adjacent entities of next higher dimension
  MBRange all_adjs;
  MBErrorCode result;
  result = thisMB->get_adjacencies(&this_ent, 1, 
                                   thisMB->dimension_from_handle(this_ent)+1, 
                                   true, all_adjs, MBInterface::UNION);
  if (MB_SUCCESS != result) return result;
  
    //     - create explicit adjacency to these entities
  for (MBRange::iterator rit = all_adjs.begin(); rit != all_adjs.end(); rit++) {
    result = add_adjacency(this_ent, *rit);
    if (MB_SUCCESS != result) return result;
  }
  
  return MB_SUCCESS;
}

MBErrorCode AEntityFactory::get_adjacency_ptr( MBEntityHandle entity, 
                                               std::vector<MBEntityHandle>*& ptr )
{
  ptr = 0;
  
  EntitySequence* seq;
  MBErrorCode rval = thisMB->sequence_manager()->find( entity, seq );
  if (MB_SUCCESS != rval || !seq->data()->get_adjacency_data())
    return rval;
  
  ptr = seq->data()->get_adjacency_data()[entity - seq->data()->start_handle()];
  return MB_SUCCESS;
}

MBErrorCode AEntityFactory::get_adjacency_ptr( MBEntityHandle entity, 
                                               const std::vector<MBEntityHandle>*& ptr ) const
{
  ptr = 0;
  
  EntitySequence* seq;
  MBErrorCode rval = thisMB->sequence_manager()->find( entity, seq );
  if (MB_SUCCESS != rval || !seq->data()->get_adjacency_data())
    return rval;
  
  ptr = seq->data()->get_adjacency_data()[entity - seq->data()->start_handle()];
  return MB_SUCCESS;
}


MBErrorCode AEntityFactory::set_adjacency_ptr( MBEntityHandle entity, 
                                               std::vector<MBEntityHandle>* ptr )
{
  EntitySequence* seq;
  MBErrorCode rval = thisMB->sequence_manager()->find( entity, seq );
  if (MB_SUCCESS != rval)
    return rval;
    
  if (!seq->data()->get_adjacency_data() && !seq->data()->allocate_adjacency_data())
    return MB_MEMORY_ALLOCATION_FAILED;
  
  const MBEntityHandle index = entity - seq->data()->start_handle();
  std::vector<MBEntityHandle>*& ref = seq->data()->get_adjacency_data()[index];
  delete ref;
  ref = ptr;
  return MB_SUCCESS;
}

  
void AEntityFactory::get_memory_use( unsigned long& entity_total,
                                     unsigned long& memory_total )
{
  entity_total = memory_total = 0;

  // iterate through each element type
  SequenceData* prev_data = 0;
  for (MBEntityType t = MBVERTEX; t != MBENTITYSET; t++) {
    TypeSequenceManager::iterator i;
    TypeSequenceManager& seqman = thisMB->sequence_manager()->entity_map( t );
    for (i = seqman.begin(); i != seqman.end(); ++i) {
      if (!(*i)->data()->get_adjacency_data())
        continue;
      
      if (prev_data != (*i)->data()) {
        prev_data = (*i)->data();
        memory_total += prev_data->size() * sizeof(MBAdjacencyVector);
      }
      
      const MBAdjacencyVector* vec;
      for (MBEntityHandle h = (*i)->start_handle(); h <= (*i)->end_handle(); ++h) {
        get_adjacency_ptr( h, vec );
        if (vec) 
          entity_total += vec->capacity() * sizeof(MBEntityHandle) * sizeof(MBAdjacencyVector);
      }
    }
  }
 
  memory_total += sizeof(*this) + entity_total;
}
  
    
MBErrorCode AEntityFactory::get_memory_use( const MBRange& ents_in,
                                       unsigned long& min_per_ent,
                                       unsigned long& amortized )
{
  min_per_ent = amortized = 0;
  MBRangeSeqIntersectIter iter( thisMB->sequence_manager() );
  MBErrorCode rval = iter.init( ents_in.begin(), ents_in.end() );
  if (MB_SUCCESS != rval)
    return rval;
  
  do {
    MBAdjacencyVector** array = iter.get_sequence()->data()->get_adjacency_data();
    if (!array)
      continue;

    MBEntityID count = iter.get_end_handle() - iter.get_start_handle() + 1;
    MBEntityID data_occ = thisMB->sequence_manager()
                                ->entity_map( iter.get_sequence()->type() )
                                 .get_occupied_size( iter.get_sequence()->data() );
    
    amortized += sizeof(MBAdjacencyVector*) 
                 * iter.get_sequence()->data()->size()
                 * count / data_occ;
                 
    array += iter.get_start_handle() - iter.get_sequence()->data()->start_handle();
    for (MBEntityID i = 0; i < count; ++i) {
      if (array[i]) 
        min_per_ent += sizeof(MBEntityHandle) * array[i]->capacity() + sizeof(MBAdjacencyVector);
    }
  } while (MB_SUCCESS == (rval = iter.step()));
  
  amortized += min_per_ent;
  return (rval == MB_FAILURE) ? MB_SUCCESS : rval;
}   
  