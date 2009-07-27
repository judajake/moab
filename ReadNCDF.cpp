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

#ifdef WIN32
#pragma warning(disable:4786)
#endif

#include "ReadNCDF.hpp"
#include "netcdf.hh"

#include <algorithm>
#include <time.h>
#include <string>
#include <assert.h>
#include <stdio.h>
#include <cmath>

#include "MBCN.hpp"
#include "MBRange.hpp"
#include "MBInterface.hpp"
#include "ExoIIUtil.hpp"
#include "MBTagConventions.hpp"
#include "MBInternals.hpp"
#include "MBReadUtilIface.hpp"
#include "exodus_order.h"
#include "FileOptions.hpp"

#define INS_ID(stringvar, prefix, id) \
          sprintf(stringvar, prefix, id)
          
#define GET_DIM(ncdim, name, val) \
          if (dimension_exists(name)) {\
            ncdim = ncFile->get_dim(name); \
            if (!ncdim->is_valid()) {\
               readMeshIface->report_error("MBCN:: name wasn't valid.");\
               return MB_FAILURE;\
            }\
            val = ncdim->size();\
          } else val = 0;

#define GET_DIMB(ncdim, name, varname, id, val) \
          INS_ID(name, varname, id); \
          if (dimension_exists(name)) {\
            ncdim = ncFile->get_dim(name); \
            if (!ncdim->is_valid()) {\
               readMeshIface->report_error("MBCN:: name wasn't valid.");\
               return MB_FAILURE;\
            }\
            val = ncdim->size();\
          } else val = 0;

MBReaderIface* ReadNCDF::factory( MBInterface* iface )
  { return new ReadNCDF( iface ); }

ReadNCDF::ReadNCDF(MBInterface* impl)
    : mdbImpl(impl), max_line_length(-1), max_str_length(-1)
{
  assert(impl != NULL);
  reset();
  
  void* ptr = 0;
  impl->query_interface( "MBReadUtilIface", &ptr );
  readMeshIface = reinterpret_cast<MBReadUtilIface*>(ptr);

  // initialize in case tag_get_handle fails below
  mMaterialSetTag  = 0;
  mDirichletSetTag = 0;
  mNeumannSetTag   = 0;
  mHasMidNodesTag  = 0;
  mDistFactorTag   = 0;
  mQaRecordTag     = 0;
  mGlobalIdTag     = 0;

  //! get and cache predefined tag handles
  int dum_val = 0;
  MBErrorCode result = impl->tag_get_handle(MATERIAL_SET_TAG_NAME,  mMaterialSetTag);
  if (MB_TAG_NOT_FOUND == result)
    result = impl->tag_create(MATERIAL_SET_TAG_NAME, 
                              sizeof(int), 
                              MB_TAG_SPARSE, 
                              MB_TYPE_INTEGER,
                              mMaterialSetTag,
                              &dum_val);
  
  result = impl->tag_get_handle(DIRICHLET_SET_TAG_NAME, mDirichletSetTag);
  if (MB_TAG_NOT_FOUND == result)
    result = impl->tag_create(DIRICHLET_SET_TAG_NAME, 
                              sizeof(int), 
                              MB_TAG_SPARSE, 
                              MB_TYPE_INTEGER,
                              mDirichletSetTag,
                              &dum_val);
  
  result = impl->tag_get_handle(NEUMANN_SET_TAG_NAME,   mNeumannSetTag);
  if (MB_TAG_NOT_FOUND == result)
    result = impl->tag_create(NEUMANN_SET_TAG_NAME, 
                              sizeof(int), 
                              MB_TAG_SPARSE, 
                              MB_TYPE_INTEGER,
                              mNeumannSetTag,
                              &dum_val);
  
  result = impl->tag_get_handle(HAS_MID_NODES_TAG_NAME, mHasMidNodesTag);
  if (MB_TAG_NOT_FOUND == result) {
    int dum_val_array[] = {0, 0, 0, 0};
    result = impl->tag_create(HAS_MID_NODES_TAG_NAME, 
                              4*sizeof(int), 
                              MB_TAG_SPARSE, 
                              MB_TYPE_INTEGER,
                              mHasMidNodesTag,
                              dum_val_array);
  }
  
  result = impl->tag_get_handle("distFactor",           mDistFactorTag);
  if (MB_TAG_NOT_FOUND == result) {
    result = impl->tag_create_variable_length( "distFactor", 
                                               MB_TAG_SPARSE,
                                               MB_TYPE_DOUBLE,
                                               mDistFactorTag );
  }
  
  result = impl->tag_get_handle("qaRecord",             mQaRecordTag);
  if (MB_TAG_NOT_FOUND == result)
    result = impl->tag_create_variable_length( "qaRecord", 
                                               MB_TAG_SPARSE,
                                               MB_TYPE_OPAQUE,
                                               mQaRecordTag );
  
  result = impl->tag_get_handle(GLOBAL_ID_TAG_NAME,             mGlobalIdTag);
  if (MB_TAG_NOT_FOUND == result)
    result = impl->tag_create(GLOBAL_ID_TAG_NAME, sizeof(int), MB_TAG_SPARSE, 
                              MB_TYPE_INTEGER, mGlobalIdTag, &dum_val);
  

  ncFile = NULL;
}

void ReadNCDF::reset()
{
  numberDimensions_loading = -1;
  mCurrentMeshHandle = 0;
  vertexOffset = 0; 

  numberNodes_loading = 0;
  numberElements_loading = 0;
  numberElementBlocks_loading = 0;
  numberNodeSets_loading = 0;
  numberSideSets_loading = 0;
  numberDimensions_loading = 0;

  if( !blocksLoading.empty() )
    blocksLoading.clear();

  if( !nodesInLoadedBlocks.empty() )
    nodesInLoadedBlocks.clear();
}


ReadNCDF::~ReadNCDF() 
{
  std::string iface_name = "MBReadUtilIface";
  mdbImpl->release_interface(iface_name, readMeshIface);
  if (NULL != ncFile)
    delete ncFile;
}

// should this be moved to the core?  This could apply to ANY input
// file type.  There is no exodus specific code here although the
// loaded_blocks tag does get used later on and it does save the
// current mesh handle.  But that could be handled differently.
MBErrorCode ReadNCDF::check_file_status( std::string& exodus_file_name,
                                          bool& previously_read) 
{
  MBErrorCode status = MB_FAILURE;
  char mesh_tag_buffer[256] = "";

  // assume that this is the first time reading this file
  previously_read = false;

  // look for any mesh sets tagged "__mesh" that already exist
  MBTag mesh_tag=0;
  if (mdbImpl->tag_get_handle("__mesh", mesh_tag) == MB_SUCCESS)
  {
    MBRange mesh_range;
    mdbImpl->get_entities_by_type_and_tag( 0, MBENTITYSET, &mesh_tag, NULL, 1, mesh_range,
                                           MBInterface::UNION);

    // see if any of the mesh sets came from this filename
    MBRange::iterator iter;
    for (iter = mesh_range.begin(); iter != mesh_range.end(); iter++)
    {
      mdbImpl->tag_get_data(mesh_tag, &(*iter), 1, mesh_tag_buffer);

      if (exodus_file_name.empty() || exodus_file_name == mesh_tag_buffer)
      {
        // found it, there should only be one.  Set the mesh handle and the flag
        // indicating that the mesh has been previously read and use the
        // handle later to determine what blocks need to be read.
        mCurrentMeshHandle = *iter;
        previously_read = true;

        if (exodus_file_name.empty())
          exodus_file_name = mesh_tag_buffer;
        
        return MB_SUCCESS;
      }
    }
  }

  // if we get to this point, this mesh has never been seen before; if the
  // file name is null, that means no file was read
  if (exodus_file_name.empty())
    return MB_FILE_DOES_NOT_EXIST;

    // ok, it's a valid name; make sure file exists
  FILE* infile = NULL;
  infile = fopen(exodus_file_name.c_str(), "r");
  if (!infile) 
    return MB_FILE_DOES_NOT_EXIST;
  else 
    fclose(infile);

  // get the handle if it exists otherwise create a new "__mesh" handle
  if (mdbImpl->tag_get_handle("__mesh", mesh_tag) != MB_SUCCESS)
  {
    memset( mesh_tag_buffer, 0, sizeof(mesh_tag_buffer) );
    if( mdbImpl->tag_create("__mesh", sizeof(mesh_tag_buffer), MB_TAG_SPARSE,
                            mesh_tag, mesh_tag_buffer) != MB_SUCCESS )
      return MB_FAILURE;
  }

  // \bug the offset tags need to be created, but do I need to get them?
  // get the "__vertex_offset" tag if it exists otherwise create a new one
  int offset = 0;
  MBTag tag_handle;
  if (mdbImpl->tag_get_handle("__vertex_offset", tag_handle) != MB_SUCCESS)
  {
    if (mdbImpl->tag_create("__vertex_offset", sizeof(int), MB_TAG_SPARSE,
                            tag_handle, &offset) != MB_SUCCESS )
      return MB_FAILURE;
  }


  MBEntityHandle mesh_handle;
  if( mdbImpl->create_meshset( MESHSET_SET, mesh_handle ) != MB_SUCCESS)
      return MB_FAILURE;

  MBRange temp_range;
  MBRange::iterator iter, end_iter;
  int highest_id = 0;
  int temp_id= 0;

  MBTag block_offset_tag=0;
  
  //block offset tag
  if (mdbImpl->tag_get_handle("__block_id_offset", block_offset_tag) != MB_SUCCESS)
  {
    if (mdbImpl->tag_create("__block_id_offset", sizeof(int), MB_TAG_SPARSE,
                            block_offset_tag, &offset) != MB_SUCCESS )
      return MB_FAILURE;

    //set the highest id to zero
    int highest_id = 0;
    if( mdbImpl->tag_set_data( block_offset_tag, &mesh_handle, 1, &highest_id ) != MB_SUCCESS )
      return MB_FAILURE;

  }
  else
  {

    //get all 'matrerial' meshsets
    if(mdbImpl->get_entities_by_type_and_tag( 0, MBENTITYSET, &mMaterialSetTag, NULL, 1, temp_range ) != MB_SUCCESS )
      return MB_FAILURE;

    highest_id = 0;  
    temp_id = 0;  

    if( !temp_range.empty() )
    {

      //get the highest id 
      iter = temp_range.begin();
      end_iter = temp_range.end();
  
      for(; iter != end_iter; iter++)
      {
        if(mdbImpl->tag_get_data( mMaterialSetTag, &(*iter), 1, &temp_id) != MB_SUCCESS )
          return MB_FAILURE;
       
        if( temp_id > highest_id )
          highest_id = temp_id;
      }
    }

    //set the highest id
    if( mdbImpl->tag_set_data( block_offset_tag, &mesh_handle, 1, &highest_id ) != MB_SUCCESS )
      return MB_FAILURE;

  }

  //sideset offset tag
  if (mdbImpl->tag_get_handle("__sideset_id_offset", tag_handle) != MB_SUCCESS)
  {
    if (mdbImpl->tag_create("__sideset_id_offset", sizeof(int), MB_TAG_SPARSE,
                            tag_handle, &offset) != MB_SUCCESS )
      return MB_FAILURE;

    //set the highest id to zero
    int highest_id = 0;
    if( mdbImpl->tag_set_data( tag_handle, &mesh_handle, 1, &highest_id ) != MB_SUCCESS )
      return MB_FAILURE;
  }
  else
  {

    temp_range.clear(); 
    //get all sideset meshsets
    if(mdbImpl->get_entities_by_type_and_tag( 0, MBENTITYSET, &mNeumannSetTag, NULL, 1, temp_range ) != MB_SUCCESS )
      return MB_FAILURE;

    highest_id = 0;  
    temp_id = 0;  

    if( !temp_range.empty() ) 
    {

      //get the highest id 
      iter = temp_range.begin();
      end_iter = temp_range.end();
  
      for(; iter != end_iter; iter++)
      {
        if(mdbImpl->tag_get_data( mNeumannSetTag, &(*iter), 1, &temp_id) != MB_SUCCESS )
          return MB_FAILURE;
       
        if( temp_id > highest_id )
          highest_id = temp_id;
      }
    }

    if( mdbImpl->tag_get_handle( "__sideset_id_offset", tag_handle) != MB_SUCCESS )
      return MB_FAILURE;

    //set the highest id
    if( mdbImpl->tag_set_data( tag_handle, &mesh_handle, 1, &highest_id ) != MB_SUCCESS )
      return MB_FAILURE;

  }

  //nodeset offset tag
  if (mdbImpl->tag_get_handle("__nodeset_id_offset", tag_handle) != MB_SUCCESS)
  {
    if (mdbImpl->tag_create("__nodeset_id_offset", sizeof(int), MB_TAG_SPARSE,
                            tag_handle, &offset) != MB_SUCCESS )
      return MB_FAILURE;

    //set the highest id to zero
    int highest_id = 0;
    if( mdbImpl->tag_set_data( tag_handle, &mesh_handle, 1, &highest_id ) != MB_SUCCESS )
      return MB_FAILURE;
  }
  else
  {

    temp_range.clear(); 
    //get all nodeset meshsets
    if(mdbImpl->get_entities_by_type_and_tag( 0, MBENTITYSET, &mDirichletSetTag, NULL, 1, temp_range ) != MB_SUCCESS )
      return MB_FAILURE;

    //get the highest id 
    iter = temp_range.begin();
    end_iter = temp_range.end();
  
    highest_id = 0;  
    temp_id = 0;  

    if( !temp_range.empty() ) 
    {

      //get the highest id 
      iter = temp_range.begin();
      end_iter = temp_range.end();
  
      for(; iter != end_iter; iter++)
      {
        if(mdbImpl->tag_get_data( mDirichletSetTag, &(*iter), 1, &temp_id) != MB_SUCCESS )
          return MB_FAILURE;
       
        if( temp_id > highest_id )
          highest_id = temp_id;
      }
    }

    if( mdbImpl->tag_get_handle( "__nodeset_id_offset", tag_handle) != MB_SUCCESS )
      return MB_FAILURE;

    //set the highest id
    if( mdbImpl->tag_set_data( tag_handle, &mesh_handle, 1, &highest_id ) != MB_SUCCESS )
      return MB_FAILURE;

  }


  // save the filename on the mesh_tag
  strncpy( mesh_tag_buffer, exodus_file_name.c_str(), sizeof(mesh_tag_buffer) - 1 );
  mesh_tag_buffer[sizeof(mesh_tag_buffer)-1] = '\0';
  status = mdbImpl->tag_set_data(mesh_tag, &mesh_handle, 1, mesh_tag_buffer);
  if (status != MB_SUCCESS )
    return MB_FAILURE;


  // save this mesh handle as the current one being read
  mCurrentMeshHandle = mesh_handle;

  return MB_SUCCESS;
}
  

MBErrorCode ReadNCDF::read_tag_values(const char* file_name,
                                      const char* tag_name,
                                      const FileOptions& opts,
                                      std::vector<int>& id_array,
                                      const IDTag* subset_list,
                                      int subset_list_length )
{
  if (subset_list && subset_list_length) {
    readMeshIface->report_error( "ExodusII reader supports subset read only by material ID." );
    return MB_UNSUPPORTED_OPERATION;
  }

    // 1. Read the header
  MBErrorCode rval = read_exodus_header( file_name );
  if (MB_FAILURE == rval) 
    return rval;
  
  int count = 0;
  const char* prop;
  const char* blocks   = "eb_prop1";
  const char* nodesets = "ns_prop1";
  const char* sidesets = "ss_prop1";
  
  if (!strcmp(tag_name, MATERIAL_SET_TAG_NAME)) {
    count = numberElementBlocks_loading;
    prop = blocks;
  }
  else if (!strcmp(tag_name, DIRICHLET_SET_TAG_NAME)) {
    count = numberNodeSets_loading;
    prop = nodesets;
  }
  else if (!strcmp(tag_name, NEUMANN_SET_TAG_NAME)) {
    count = numberSideSets_loading;
    prop = sidesets;
  }
  else {  
    delete ncFile;
    ncFile = 0;
    return MB_TAG_NOT_FOUND;
  }
  
  if (count) {
    NcVar *nc_var = ncFile->get_var( prop );
    if (NULL == nc_var || !nc_var->is_valid()) {
      readMeshIface->report_error("Problem getting prop variable.");
      rval = MB_FAILURE;
    }
    else {
      id_array.resize( count );
      NcBool status = nc_var->get(&id_array[0], count);
      if (0 == status) {
        readMeshIface->report_error("Problem getting element id vector.");
        rval = MB_FAILURE;
      }
    }
  }
  
  delete ncFile;
  ncFile = 0;
  return rval;
}



MBErrorCode ReadNCDF::load_file(const char *exodus_file_name,
                                MBEntityHandle& file_set,
                                const FileOptions& opts,
                                const MBReaderIface::IDTag* subset_list,
                                int subset_list_length,
                                const MBTag* file_id_tag)
{
  MBErrorCode status;

  int num_blocks = 0;
  const int* blocks_to_load = 0;
  if (subset_list && subset_list_length) {
    if (subset_list_length > 1 && !strcmp( subset_list[0].tag_name, MATERIAL_SET_TAG_NAME) ) {
      readMeshIface->report_error( "ExodusII reader supports subset read only by material ID." );
      return MB_UNSUPPORTED_OPERATION;
    }
    if (subset_list[0].num_parts) {
      readMeshIface->report_error( "ExodusII reader does not support mesh partitioning");
      return MB_UNSUPPORTED_OPERATION;
    }
    blocks_to_load = subset_list[0].tag_values;
    num_blocks = subset_list[0].num_tag_values;
  }
  
  file_set = 0;
    // this function directs the reading of an exoii file, but doesn't do any of
    // the actual work
  
  //See if opts has tdata.
  MBErrorCode rval;
  std::string s;
  rval = opts.get_str_option("tdata", s ); 
  if(MB_SUCCESS == rval && !s.empty())
    return update(exodus_file_name, opts); 

    // 0. Check for previously read file.
  reset();
  bool previously_loaded = false;
  std::string filename( exodus_file_name );
  exodusFile = filename;
  status = check_file_status(filename, previously_loaded);
  if (MB_SUCCESS != status) 
    return status;

    // 1. Read the header
  status = read_exodus_header(exodus_file_name);
  if (MB_FAILURE == status) return status;
  
  status = mdbImpl->get_entities_by_handle(0, initRange);
  if (MB_FAILURE == status) return status;

    // 2. Read the nodes unless they've already been read before
  if (!previously_loaded)
  {
    status = read_nodes(file_id_tag);
    if (MB_FAILURE == status) return status;
  }
 
    //3. 
  status = read_block_headers(blocks_to_load, num_blocks);
  if (MB_FAILURE == status) return status;

    // 4. Read elements (might not read them, depending on active blocks)
  status = read_elements(file_id_tag);
  if (MB_FAILURE == status) return status;
  
    // 5. Read global ids
  status = read_global_ids();
  if (MB_FAILURE == status) return status;
  
    // 6. Read nodesets
  status = read_nodesets();
  if (MB_FAILURE == status) return status;
  
    // 7. Read sidesets
  status = read_sidesets();
  if (MB_FAILURE == status) return status;

    // 8. Read qa records
  if (!previously_loaded)
  {
    status = read_qa_records();
    if (MB_FAILURE == status) return status;
  }

  
  MBRange loaded_range;
  status = mdbImpl->get_entities_by_handle(0, loaded_range);
  if (MB_FAILURE == status) return status;
  loaded_range = subtract( loaded_range, initRange);
  status = mdbImpl->add_entities(mCurrentMeshHandle, loaded_range);
  if (MB_FAILURE == status) return status;
  
    // what about properties???

  file_set = mCurrentMeshHandle;
  delete ncFile;
  ncFile = 0;
  return MB_SUCCESS;
}



MBErrorCode ReadNCDF::read_exodus_header(const char *exodus_file_name)
{

  if (NULL == ncFile) {
    
      // open netcdf/exodus file
    CPU_WORD_SIZE = sizeof(double);  // With ExodusII version 2, all floats
    IO_WORD_SIZE = sizeof(double);   // should be changed to doubles

    ncFile = new NcFile(exodus_file_name);
    if (NULL == ncFile || !ncFile->is_valid())
    {
      readMeshIface->report_error("MBCN:: problem opening Netcdf/Exodus II file %s",exodus_file_name);
      return MB_FAILURE;
    }
  }
  
    // get the attributes

    // get the word size, scalar value
  NcAtt *temp_att = ncFile->get_att("floating_point_word_size");
  if (!temp_att->is_valid() || temp_att->type() != ncInt || temp_att->num_vals() != 1) {
    readMeshIface->report_error("MBCN:: Word size didn't have type int or size 1.");
    return MB_FAILURE;
  }
  IO_WORD_SIZE = temp_att->as_int(0);
  delete temp_att;

    // exodus version
  temp_att = ncFile->get_att("version");
  if (!temp_att->is_valid() || temp_att->type() != ncFloat || temp_att->num_vals() != 1) {
    readMeshIface->report_error("MBCN:: Version didn't have type float or size 1.");
    return MB_FAILURE;
  }
  delete temp_att;

    // float version = temp_att->as_float(0);
  

    // read in initial variables
  NcDim *temp_dim;
  GET_DIM(temp_dim, "num_dim", numberDimensions_loading);
  GET_DIM(temp_dim, "num_nodes", numberNodes_loading);
  GET_DIM(temp_dim, "num_elem", numberElements_loading);
  GET_DIM(temp_dim, "num_el_blk", numberElementBlocks_loading);
  GET_DIM(temp_dim, "num_elem", numberElements_loading);
  GET_DIM(temp_dim, "num_node_sets", numberNodeSets_loading);
  GET_DIM(temp_dim, "num_side_sets", numberSideSets_loading);
  GET_DIM(temp_dim, "len_string", max_str_length);
  GET_DIM(temp_dim, "len_line", max_line_length);

    // title
  char *title = new char[max_line_length+1];
  temp_att = ncFile->get_att("title");
  if (temp_att->is_valid() && temp_att->num_vals() == 1) {
    char *dum_str = temp_att->as_string(0);
    strcpy(title, dum_str);
    delete dum_str;
  }
  delete [] title;
  delete temp_att;

  return MB_SUCCESS;
}
 
MBErrorCode ReadNCDF::read_nodes(const MBTag* file_id_tag)
{

    // read the nodes into memory

  assert(NULL != ncFile);

  // create a sequence to hold the node coordinates
  // get the current number of entities and start at the next slot

  MBEntityHandle node_handle = 0;
  std::vector<double*> arrays;
  readMeshIface->get_node_arrays(3, numberNodes_loading, 
      MB_START_ID, node_handle, arrays);
  
  vertexOffset = ID_FROM_HANDLE( node_handle ) - MB_START_ID;

  // save the start id in the vertex offset tag.
  MBTag offset_tag;
  if (mdbImpl->tag_get_handle("__vertex_offset", offset_tag) != MB_SUCCESS)
    return MB_FAILURE;

  if( mdbImpl->tag_set_data(offset_tag, &mCurrentMeshHandle, 1, &vertexOffset) != MB_SUCCESS)
    return MB_FAILURE;

  // read in the coordinates
  NcBool status;
  NcVar *coords = ncFile->get_var("coord");
  if (NULL == coords || !coords->is_valid()) {
    readMeshIface->report_error("MBCN:: Problem getting coords variable.");
    return MB_FAILURE;
  }
  status = coords->get(arrays[0], 1, numberNodes_loading);
  if (0 == status) {
    readMeshIface->report_error("MBCN:: Problem getting x coord array.");
    return MB_FAILURE;
  }
  status = coords->set_cur(1, 0);
  if (0 == status) {
    readMeshIface->report_error("MBCN:: Problem getting y coord array.");
    return MB_FAILURE;
  }
  status = coords->get(arrays[1], 1, numberNodes_loading);
  if (0 == status) {
    readMeshIface->report_error("MBCN:: Problem getting y coord array.");
    return MB_FAILURE;
  }
  if (numberDimensions_loading == 2 )
  {
    // if no z coords, fill with 0's
    for (int i = 0; i < numberNodes_loading; i++)
      arrays[2][i] = 0.0;
  }
  else {
    status = coords->set_cur(2, 0);
    if (0 == status) {
      readMeshIface->report_error("MBCN:: Problem getting z coord array.");
      return MB_FAILURE;
    }
    status = coords->get(arrays[2], 1, numberNodes_loading);
    if (0 == status) {
      readMeshIface->report_error("MBCN:: Problem getting z coord array.");
      return MB_FAILURE;
    }
  }
  
  if (file_id_tag) {
    MBRange nodes;
    nodes.insert( node_handle, node_handle + numberNodes_loading - 1 );
    readMeshIface->assign_ids( *file_id_tag, nodes, vertexOffset );
  }
  
  return MB_SUCCESS;
}

MBErrorCode ReadNCDF::read_block_headers(const int *blocks_to_load,
                                           const int num_blocks)
{
     
    // get the element block ids; keep this in a separate list, 
    // which is not offset by blockIdOffset; this list used later for
    // reading block connectivity


  // get the ids of all the blocks of this file we're reading in
  std::vector<int> block_ids(numberElementBlocks_loading);
  NcVar *nc_block_ids = ncFile->get_var("eb_prop1");
  if (NULL == nc_block_ids || !nc_block_ids->is_valid()) {
    readMeshIface->report_error("MBCN:: Problem getting eb_prop1 variable.");
    return MB_FAILURE;
  }
  NcBool status = nc_block_ids->get(&block_ids[0], numberElementBlocks_loading);
  if (0 == status) {
    readMeshIface->report_error("MBCN:: Problem getting element block id vector.");
    return MB_FAILURE;
  }

  int exodus_id = 1;

  // if the active_block_id_list is NULL all blocks are active.
  int temp_num_blocks = num_blocks;
  if (NULL == blocks_to_load || 0 == num_blocks) {
    blocks_to_load = &block_ids[0];
    temp_num_blocks = numberElementBlocks_loading;
  }
  
    // if we're only reading active blocks, go through the block list
    // removing inactive ones and update the list of currently read blocks.
  std::vector<int> new_blocks;
  MBErrorCode result = remove_previously_loaded_blocks(blocks_to_load,
                                                        temp_num_blocks,
                                                        new_blocks);
  if (result != MB_SUCCESS)
    return result;


  std::vector<int>::iterator iter, end_iter;
  iter = block_ids.begin();
  end_iter = block_ids.end();
 

  //get block offset tag
  MBTag tag_handle;
  if (mdbImpl->tag_get_handle("__block_id_offset", tag_handle) != MB_SUCCESS)
    return MB_FAILURE;
  int block_offset = 0;
  if( mdbImpl->tag_get_data( tag_handle, &mCurrentMeshHandle, 1, &block_offset ) != MB_SUCCESS )
    return MB_FAILURE;

    // read header information and initialize header-type block information
  NcDim *temp_dim;
  std::vector<char> temp_string_storage(max_str_length+1);
  char *temp_string = &temp_string_storage[0];
  int block_seq_id = 1;
  
  for(; iter != end_iter; iter++, block_seq_id++ )
  {
    int num_elements, num_nodes_per_element, num_attribs;

    GET_DIMB(temp_dim, temp_string, "num_el_in_blk%d", block_seq_id, num_elements);
    GET_DIMB(temp_dim, temp_string, "num_nod_per_el%d", block_seq_id, num_nodes_per_element);
    GET_DIMB(temp_dim, temp_string, "num_att_in_blk%d", block_seq_id, num_attribs);

      // don't read element type string for now, since it's an attrib
      // on the connectivity
      // get the entity type corresponding to this ExoII element type
      //ExoIIElementType elem_type = 
      //ExoIIUtil::static_element_name_to_type(element_type_string);

    // tag each element block(mesh set) with enum for ElementType (SHELL, QUAD4, ....etc)
    ReadBlockData block_data;
    block_data.elemType = EXOII_MAX_ELEM_TYPE;
    block_data.blockId = *iter;
    block_data.startExoId = exodus_id; 
    block_data.numElements = num_elements;

    //if block is in 'blocks_to_load'----load it!
    if( std::find(new_blocks.begin(), new_blocks.end(), *iter) 
        != new_blocks.end()) 
    { 
      block_data.reading_in = true;
    }
    else
    {
      block_data.reading_in = false;
    }

    blocksLoading.push_back( block_data );
    exodus_id += num_elements;

  }

  return MB_SUCCESS;
}

bool ReadNCDF::dimension_exists(const char *attrib_name) 
{
  int num_dim = ncFile->num_dims();
  for (int i = 0; i < num_dim; i++) {
    NcDim *temp_dim = ncFile->get_dim(i);
    if (NULL != temp_dim && temp_dim->is_valid() &&
        0 == strcmp(attrib_name, temp_dim->name()))
      return true;
  }
  
  return false;
}

MBErrorCode ReadNCDF::remove_previously_loaded_blocks(const int *blocks_to_load,
                                                        const int num_blocks,
                                                        std::vector<int> &new_blocks) 
{
  // if this mesh has been previously read, also remove the block ids
  // that were read before.

  //get all the blocks of mCurrentMeshHandle
  MBRange child_meshsets; 
  if(mdbImpl->get_entities_by_type(0, MBENTITYSET, child_meshsets ) != MB_SUCCESS )
    return MB_FAILURE;

  child_meshsets = subtract( child_meshsets, initRange);

  MBTag tag_handle;

  MBRange::iterator iter, end_iter;
 
  //get the block id offset 
  int id_offset = 0;
  if( mdbImpl->tag_get_handle( "__block_id_offset", tag_handle) != MB_SUCCESS )
    return MB_FAILURE;

  if( mdbImpl->tag_get_data( tag_handle, &mCurrentMeshHandle, 1, &id_offset) != MB_SUCCESS )
    return MB_FAILURE;

  //make a vector of currently loaded block
  std::vector< int > loaded_blocks;
  iter = child_meshsets.begin(); 
  end_iter = child_meshsets.end(); 
  int block_id = 0;
  for(; iter != end_iter; iter++)
  {
    if(mdbImpl->tag_get_data( mMaterialSetTag, &(*iter), 1, &block_id ) != MB_SUCCESS ) 
      continue;

    //strip the offset
    loaded_blocks.push_back( block_id /*- id_offset*/ ); 
  }


  // takes already loaded blocks out of 'blocks_to_load'
  std::copy(blocks_to_load, blocks_to_load+num_blocks, std::back_inserter(new_blocks));
  std::sort(new_blocks.begin(), new_blocks.end());
  std::sort(loaded_blocks.begin(), loaded_blocks.end()); 
  
  std::vector<int> tmp;

  std::set_difference(new_blocks.begin(), new_blocks.end(),
                      loaded_blocks.begin(), loaded_blocks.end(),
                      std::back_inserter(tmp)); 

  new_blocks.swap(tmp);
    
  return MB_SUCCESS;
}


MBErrorCode ReadNCDF::read_elements(const MBTag* file_id_tag)
{
    // read in elements
  
  int result = 0;

  // intialize the nodeInLoadedBlocks vector
  nodesInLoadedBlocks.resize(numberNodes_loading+1);
  memset(&nodesInLoadedBlocks[0], 0, (numberNodes_loading+1)*sizeof(char));

  std::vector<ReadBlockData>::iterator this_it;
    this_it = blocksLoading.begin();


  //here we have to offset the block id

  //get the block id offset 
  MBTag tag_handle;
  if( mdbImpl->tag_get_handle( "__block_id_offset", tag_handle) != MB_SUCCESS )
    return MB_FAILURE;

  int id_offset = 0;
  if( mdbImpl->tag_get_data( tag_handle, &mCurrentMeshHandle, 1, &id_offset) != MB_SUCCESS )
    return MB_FAILURE;

  //set the id on this block

  //reset vertexOffset 
  if (mdbImpl->tag_get_handle("__vertex_offset", tag_handle) != MB_SUCCESS)
    return MB_FAILURE;

  if( mdbImpl->tag_get_data(tag_handle, &mCurrentMeshHandle, 1, &vertexOffset) != MB_SUCCESS)
    return MB_FAILURE;

  std::vector<char> temp_string_storage(max_str_length+1);
  char *temp_string = &temp_string_storage[0];
  NcVar *temp_var;
  NcAtt *temp_att;
  int block_seq_id = 1;
  
  for (; this_it != blocksLoading.end(); this_it++, block_seq_id++) 
  {
    // if this block isn't to be read in --- continue
    if( !(*this_it).reading_in )
      continue;

    // get some information about this block
    int block_id = (*this_it).blockId;
    MBEntityHandle *conn = 0;

      // get the ncdf connect variable and the element type
    INS_ID(temp_string, "connect%d", block_seq_id);
    temp_var = ncFile->get_var(temp_string);
    if (NULL == temp_var || !temp_var->is_valid()) {
      readMeshIface->report_error("MBCN:: Problem getting connect variable.");
      return MB_FAILURE;
    }
    temp_att = temp_var->get_att("elem_type");
    if (NULL == temp_att || !temp_att->is_valid()) {
      readMeshIface->report_error("MBCN:: Problem getting elem type attribute.");
      delete temp_att;
      return MB_FAILURE;
    }
    char *dum_str = temp_att->as_string(0);
    delete temp_att;
    ExoIIElementType elem_type = 
      ExoIIUtil::static_element_name_to_type(dum_str);
    delete [] dum_str;
    (*this_it).elemType = elem_type;
    
    int verts_per_element = ExoIIUtil::VerticesPerElement[(*this_it).elemType];
    int number_nodes = (*this_it).numElements*verts_per_element;
    const MBEntityType mb_type = ExoIIUtil::ExoIIElementMBEntity[(*this_it).elemType];

    // allocate an array to read in connectivity data
    readMeshIface->get_element_array(
        (*this_it).numElements,
        verts_per_element,
        mb_type,
        (*this_it).startExoId,
        (*this_it).startMBId,
        conn);
        
        // create a range for this sequence of elements
      MBEntityHandle start_range, end_range;
      start_range = (*this_it).startMBId;
      end_range   = start_range + (*this_it).numElements-1;

      MBRange new_range(start_range, end_range);
      //MBRange<MBEntityHandle> new_range((*this_it).startMBId, 
      //                                  (*this_it).startMBId+(*this_it).numElements-1);
    
        // create a MBSet for this block and set the material tag

      MBEntityHandle ms_handle;
      if( mdbImpl->create_meshset( MESHSET_SET | MESHSET_TRACK_OWNER, ms_handle ) != MB_SUCCESS)
        return MB_FAILURE;

      if( mdbImpl->add_entities( ms_handle, new_range) != MB_SUCCESS )
        return MB_FAILURE;
        
      int mid_nodes[4];
      MBCN::HasMidNodes( mb_type, verts_per_element, mid_nodes );
      if( mdbImpl->tag_set_data( mHasMidNodesTag, &ms_handle, 1, mid_nodes ) != MB_SUCCESS )
        return MB_FAILURE;
      
    // just a check because the following code won't work if this case fails
    assert(sizeof(MBEntityHandle) >= sizeof(int));

    // tmp_ptr is of type int* and points at the same place as conn
    int* tmp_ptr = reinterpret_cast<int*>(conn);
    
    // read the connetivity into that memory,  this will take only part of the array
    // 1/2 if sizeof(MBEntityHandle) == 64 bits.
    NcBool status = temp_var->get(tmp_ptr, this_it->numElements, verts_per_element);
    if (status == 0) {
      readMeshIface->report_error("MBCN:: Problem getting connectivity.");
      return MB_FAILURE;
    }
      // Convert from exodus indices to vertex handles.
      // Iterate backwards in case handles are larger than ints.
    for (int i = number_nodes - 1; i >= 0; --i)
    {
      if ((unsigned)tmp_ptr[i] >= nodesInLoadedBlocks.size()) {
        readMeshIface->report_error( "Invalid node ID in block connectivity\n");
        return MB_FAILURE;
      }
      nodesInLoadedBlocks[tmp_ptr[i]] = 1;
      conn[i] = static_cast<MBEntityHandle>(tmp_ptr[i]) + vertexOffset;
    }
    
    // Adjust connectivity order if necessary
    const int* reorder = exodus_elem_order_map[mb_type][verts_per_element];
    if (reorder)
      MBReadUtilIface::reorder( reorder, conn, this_it->numElements, verts_per_element );
    

      
    readMeshIface->update_adjacencies((*this_it).startMBId, (*this_it).numElements,
                                      ExoIIUtil::VerticesPerElement[(*this_it).elemType], conn);
    
    if ( result == -1 )
    {
      readMeshIface->report_error("ReadNCDF:: error getting element connectivity for block %i",
          block_id);
      return MB_FAILURE;
    }

    //set the block id with an offset
    block_id += id_offset;
    if( mdbImpl->tag_set_data( mMaterialSetTag, &ms_handle, 1, &block_id ) != MB_SUCCESS )
      return MB_FAILURE;
    if( mdbImpl->tag_set_data( mGlobalIdTag, &ms_handle, 1, &block_id ) != MB_SUCCESS )
      return MB_FAILURE;
      
      
    if (file_id_tag) {
      MBRange range;
      range.insert( this_it->startMBId, this_it->startMBId + this_it->numElements - 1 );
      readMeshIface->assign_ids( *file_id_tag, range, this_it->startExoId );
    }
  }

  return MB_SUCCESS;
}
  
MBErrorCode ReadNCDF::read_global_ids()
{
    // read in the map from the exodus file
  int* ptr = new int [numberElements_loading];

  NcVar *temp_var = ncFile->get_var("elem_map");
  if (NULL == temp_var || !temp_var->is_valid()) {
    readMeshIface->report_error("ReadNCDF:: Problem getting element number map variable.");
    delete [] ptr;
    return MB_FAILURE;
  }
  NcBool status = temp_var->get(ptr, numberElements_loading);
  if (0 == status) {
    readMeshIface->report_error("ReadNCDF:: Problem getting element number map data.");
    delete [] ptr;
    return MB_FAILURE;
  }

  std::vector<ReadBlockData>::iterator iter;
  int ptr_pos = 0;
  for(iter = blocksLoading.begin(); iter != blocksLoading.end(); ++iter)
  {
    if (iter->reading_in)
    {
      if (iter->startMBId != 0)
      {
        MBRange range(iter->startMBId, iter->startMBId+iter->numElements-1);
        MBErrorCode error = mdbImpl->tag_set_data(mGlobalIdTag, 
                                                  range, &ptr[ptr_pos]);
        if (error != MB_SUCCESS)
        {
          delete [] ptr;
          return error;
        }
        ptr_pos += iter->numElements;
      }
      else
      {
        delete [] ptr;
        return MB_FAILURE;
      }
    }
  }

    // read in node map next
  if (numberNodes_loading > numberElements_loading) {
    delete [] ptr;
    ptr = new int [numberNodes_loading];
  }

  int varid = -1;
  int cstatus = nc_inq_varid (ncFile->id(), "node_num_map", &varid);
  if (cstatus == NC_NOERR && varid != -1) {
    temp_var = ncFile->get_var("node_num_map");
    status = temp_var->get(ptr, numberNodes_loading);
    if (0 == status) {
      readMeshIface->report_error("ReadNCDF:: Problem getting node number map data.");
      delete [] ptr;
      return MB_FAILURE;
    } 
    MBRange range(MB_START_ID+vertexOffset, 
                  MB_START_ID+vertexOffset+numberNodes_loading-1);
    MBErrorCode error = mdbImpl->tag_set_data(mGlobalIdTag, 
                                              range, &ptr[0]);
    if (MB_SUCCESS != error)
      readMeshIface->report_error("ReadNCDF:: Problem setting node global ids.");
  }
  
  delete [] ptr;
  
  return MB_SUCCESS;
}

MBErrorCode ReadNCDF::read_nodesets() 
{
    // read in the nodesets for the model

  if(0 == numberNodeSets_loading) return MB_SUCCESS;
  std::vector<int> id_array(numberNodeSets_loading);

    // read in the nodeset ids
  NcVar *temp_var = ncFile->get_var("ns_prop1");
  if (NULL == temp_var || !temp_var->is_valid()) {
    readMeshIface->report_error("MBCN:: Problem getting ns_prop1 variable.");
    return MB_FAILURE;
  }
  NcBool status = temp_var->get(&id_array[0], numberNodeSets_loading);
  if (0 == status) {
    readMeshIface->report_error("MBCN:: Problem getting nodeset id vector.");
    return MB_FAILURE;
  }

    //get the node_id_offset
  MBTag tag_handle;
  if( mdbImpl->tag_get_handle( "__nodeset_id_offset", tag_handle ) != MB_SUCCESS )
    return MB_FAILURE;

  int nodeset_id_offset = 0; 
  if( mdbImpl->tag_get_data( tag_handle, &mCurrentMeshHandle, 1, &nodeset_id_offset ) != MB_SUCCESS )
    return MB_FAILURE;


    // use a vector of ints to read node handles
  std::vector<int> node_handles;

  int i;
  std::vector<char> temp_string_storage(max_str_length+1);
  char *temp_string = &temp_string_storage[0];
  NcDim *temp_dim;
  for(i = 0; i < numberNodeSets_loading; i++)
  {
      // get nodeset parameters
    int number_nodes_in_set;
    int number_dist_factors_in_set;

    GET_DIMB(temp_dim, temp_string, "num_nod_ns%d", i+1, number_nodes_in_set);
    GET_DIMB(temp_dim, temp_string, "num_df_ns%d", i+1, number_dist_factors_in_set);

      // need to new a vector to store dist. factors 
      // this vector * gets stored as a tag on the sideset meshset
    std::vector<double> temp_dist_factor_vector( number_nodes_in_set );
    if( number_dist_factors_in_set != 0)
    {
      INS_ID(temp_string, "dist_fact_ns%d", i+1);
      temp_var = ncFile->get_var(temp_string);
      if (NULL == temp_var || !temp_var->is_valid()) {
        readMeshIface->report_error("MBCN:: Problem getting dist fact variable.");
        return MB_FAILURE;
      }
      NcBool status = temp_var->get(&(temp_dist_factor_vector[0]),
                                    number_dist_factors_in_set);
      if (0 == status) {
        readMeshIface->report_error("MBCN:: Problem getting dist factors.");
        return MB_FAILURE;
      }
    }

      // size new arrays and get ids and distribution factors
    if (node_handles.size() < (unsigned int)number_nodes_in_set) {
      node_handles.reserve(number_nodes_in_set);
      node_handles.resize(number_nodes_in_set);
    }

    INS_ID(temp_string, "node_ns%d", i+1);
    temp_var = ncFile->get_var(temp_string);
    if (NULL == temp_var || !temp_var->is_valid()) {
      readMeshIface->report_error("MBCN:: Problem getting nodeset node variable.");
      return MB_FAILURE;
    }
    NcBool status = temp_var->get(&node_handles[0],
                                  number_nodes_in_set);
    if (0 == status) {
      readMeshIface->report_error("MBCN:: Problem getting nodeset nodes data.");
      return MB_FAILURE;
    }

      // Maybe there is already a nodesets meshset here we can append to 
    MBRange child_meshsets;
    if( mdbImpl->get_entities_by_handle(0, child_meshsets ) != MB_SUCCESS ) 
      return MB_FAILURE;

    child_meshsets = subtract( child_meshsets, initRange);

    MBRange::iterator iter, end_iter;
    iter = child_meshsets.begin();
    end_iter = child_meshsets.end();

    MBEntityHandle ns_handle = 0;
    for(; iter != end_iter; iter++)
    {
      int nodeset_id;
      if( mdbImpl->tag_get_data( mDirichletSetTag, &(*iter), 1, &nodeset_id ) != MB_SUCCESS )
        continue; 

      if((id_array[i]+nodeset_id_offset) == nodeset_id )
      {
          //found the meshset
        ns_handle = *iter;
        break;
      }
    } 

    std::vector< MBEntityHandle > nodes_of_nodeset;
    if( ns_handle )
      if( mdbImpl->get_entities_by_handle( ns_handle, nodes_of_nodeset, true) != MB_SUCCESS )
        return MB_FAILURE;

      // make these into entity handles
      // TODO: could we have read it into MBEntityHandle sized array in the first place?
    int j, temp;
    std::vector<MBEntityHandle> nodes;
    std::vector<double> dist_factor_vector;
    for (j = 0; j < number_nodes_in_set; j++)
    {
        //see if this node is one we're currently reading in
      if( nodesInLoadedBlocks[node_handles[j]] == 1 )
      {
          //make sure that it already isn't in a nodeset
        unsigned int node_id = CREATE_HANDLE(MBVERTEX, node_handles[j]+vertexOffset, temp);
        if( !ns_handle || 
            std::find(nodes_of_nodeset.begin(), nodes_of_nodeset.end(), node_id) == nodes_of_nodeset.end() )
        { 
          nodes.push_back( node_id );
        
          if( number_dist_factors_in_set != 0)
          {
            dist_factor_vector.push_back( temp_dist_factor_vector[j] );
          }
        }
      }
    }
    
      // no nodes to add 
    if( nodes.empty() )
      continue; 

      //if there was no meshset found --> create one
    if( ns_handle == 0)
    {
      if( mdbImpl->create_meshset( MESHSET_ORDERED | MESHSET_TRACK_OWNER, ns_handle ) != MB_SUCCESS) 
        return MB_FAILURE;

        // set a tag signifying dirichlet bc
        // TODO: create this tag another way

      int nodeset_id = id_array[i] + nodeset_id_offset;
      if( mdbImpl->tag_set_data(mDirichletSetTag, &ns_handle, 1, &nodeset_id ) != MB_SUCCESS )
        return MB_FAILURE;
      if( mdbImpl->tag_set_data(mGlobalIdTag, &ns_handle, 1, &nodeset_id ) != MB_SUCCESS )
        return MB_FAILURE;

      if( !dist_factor_vector.empty() )
      {
        int size = dist_factor_vector.size() * sizeof(double);  
        const void* data = &dist_factor_vector[0];    
        if( mdbImpl->tag_set_data( mDistFactorTag, &ns_handle, 1, &data, &size ) != MB_SUCCESS )
          return MB_FAILURE;
      }
    }
    else if( !dist_factor_vector.empty() )
    {
        // append dist factors to vector 
      const void* ptr = 0;
      int size = 0;
      if( mdbImpl->tag_get_data( mDistFactorTag, &ns_handle, 1, &ptr, &size ) != MB_SUCCESS )
        return MB_FAILURE;
      
      size /= sizeof(double);
      const double* data = reinterpret_cast<const double*>(ptr);
      dist_factor_vector.reserve( dist_factor_vector.size() + size );
      std::copy( data, data+size, std::back_inserter( dist_factor_vector ) );
      size = dist_factor_vector.size() * sizeof(double);  
      ptr = &dist_factor_vector[0];    
      if( mdbImpl->tag_set_data( mDistFactorTag, &ns_handle, 1, &ptr, &size ) != MB_SUCCESS )
        return MB_FAILURE;
    }

      // add the nodes to the meshset
    if( mdbImpl->add_entities(ns_handle, &nodes[0], nodes.size()) != MB_SUCCESS )
      return MB_FAILURE;

  }

  return MB_SUCCESS;
}

MBErrorCode ReadNCDF::read_sidesets() 
{
  // uhhh if you read the same file (previously_read==true) then blow away the 
  // sidesets pertaining to this file?  How do you do that?  If you read a 
  // new file make sure all the offsets are correct.
  
  // if not loading any sidesets -- exit
  if(0 == numberSideSets_loading) 
    return MB_SUCCESS;
  
    // read in the sideset ids
  std::vector<int> id_array(numberSideSets_loading);
  NcVar *temp_var = ncFile->get_var("ss_prop1");
  if (NULL == temp_var || !temp_var->is_valid()) {
    readMeshIface->report_error("MBCN:: Problem getting ss_prop1 variable.");
    return MB_FAILURE;
  }
  NcBool status = temp_var->get(&id_array[0], numberSideSets_loading);
  if (0 == status) {
    readMeshIface->report_error("MBCN:: Problem getting sideset id vector.");
    return MB_FAILURE;
  }

  // create a side set for each one
  int number_sides_in_set;
  int number_dist_factors_in_set;


  //get the sideset_id_offset
  MBTag tag_handle;
  if( mdbImpl->tag_get_handle( "__sideset_id_offset", tag_handle ) != MB_SUCCESS )
    return MB_FAILURE;

  int sideset_id_offset = 0; 
  if( mdbImpl->tag_get_data( tag_handle, &mCurrentMeshHandle, 1, &sideset_id_offset ) != MB_SUCCESS )
    return MB_FAILURE;

  // Maybe there is already a sidesets meshset here we can append to 
  MBRange child_meshsets;
  if( mdbImpl->get_entities_by_type(0, MBENTITYSET, 
                                    child_meshsets ) != MB_SUCCESS ) 
    return MB_FAILURE;

  child_meshsets = subtract( child_meshsets, initRange);

  MBRange::iterator iter, end_iter;

  int i;
  std::vector<char> temp_string_storage(max_str_length+1);
  char *temp_string = &temp_string_storage[0];
  NcDim *temp_dim;
  for(i = 0; i < numberSideSets_loading; i++)
  {

    // get sideset parameters
    GET_DIMB(temp_dim, temp_string, "num_side_ss%d", i+1, number_sides_in_set);
    GET_DIMB(temp_dim, temp_string, "num_df_ss%d", i+1, number_dist_factors_in_set);

    // size new arrays and get element and side lists
    std::vector<int> side_list(number_sides_in_set);
    std::vector<int> element_list(number_sides_in_set);
    INS_ID(temp_string, "side_ss%d", i+1);
    temp_var = ncFile->get_var(temp_string);
    if (NULL == temp_var || !temp_var->is_valid()) {
      readMeshIface->report_error("MBCN:: Problem getting sideset side variable.");
      return MB_FAILURE;
    }
    NcBool status = temp_var->get(&side_list[0],
                                  number_sides_in_set);
    if (0 == status) {
      readMeshIface->report_error("MBCN:: Problem getting sideset sides data.");
      return MB_FAILURE;
    }

    INS_ID(temp_string, "elem_ss%d", i+1);
    temp_var = ncFile->get_var(temp_string);
    if (NULL == temp_var || !temp_var->is_valid()) {
      readMeshIface->report_error("MBCN:: Problem getting sideset elem variable.");
      return MB_FAILURE;
    }
    status = temp_var->get(&element_list[0],
                           number_sides_in_set);
    if (0 == status) {
      readMeshIface->report_error("MBCN:: Problem getting sideset elems data.");
      return MB_FAILURE;
    }

    std::vector<double> temp_dist_factor_vector;
    std::vector<MBEntityHandle> entities_to_add, reverse_entities;
    // create the sideset entities 
    if( create_ss_elements( &element_list[0], &side_list[0], number_sides_in_set, number_dist_factors_in_set, 
                            entities_to_add, reverse_entities, temp_dist_factor_vector, 
                            i+1) != MB_SUCCESS )
      return MB_FAILURE;

      //if there are elements to add
    if( !entities_to_add.empty() || !reverse_entities.empty())
    {
      iter = child_meshsets.begin();
      end_iter = child_meshsets.end();

      MBEntityHandle ss_handle = 0;
      for(; iter != end_iter; iter++)
      {
        int sideset_id;
        if( mdbImpl->tag_get_data( mNeumannSetTag, &(*iter), 1, &sideset_id ) != MB_SUCCESS )
          continue; 

        if( (id_array[i] + sideset_id_offset) == sideset_id )
        {
          //found the meshset
          ss_handle = *iter;
          break;
        }
      } 

      //if we didn't find a sideset already 
      if( ss_handle == 0 )
      {
        if( mdbImpl->create_meshset( MESHSET_ORDERED | MESHSET_TRACK_OWNER, ss_handle ) != MB_SUCCESS )
          return MB_FAILURE;
    
        if (ss_handle == 0) 
        {
          return MB_FAILURE;
        }

        int sideset_id = id_array[i] + sideset_id_offset;
        if( mdbImpl->tag_set_data(mNeumannSetTag, &ss_handle, 1, &sideset_id ) != MB_SUCCESS)
          return MB_FAILURE;
        if( mdbImpl->tag_set_data(mGlobalIdTag, &ss_handle, 1, &sideset_id ) != MB_SUCCESS)
          return MB_FAILURE;

        if (!reverse_entities.empty()) {
            // also make a reverse set to put in this set
          MBEntityHandle reverse_set;
          if (mdbImpl->create_meshset(MESHSET_SET | MESHSET_TRACK_OWNER, reverse_set) != MB_SUCCESS)
            return MB_FAILURE;


            // add the reverse set to the sideset set and the entities to the reverse set
          MBErrorCode result = mdbImpl->add_entities(ss_handle, &reverse_set, 1);
          if (MB_SUCCESS != result) return result;

          result = mdbImpl->add_entities(reverse_set, &reverse_entities[0], reverse_entities.size());
          if (MB_SUCCESS != result) return result;

            // set the reverse tag
          MBTag sense_tag;
          result = mdbImpl->tag_get_handle("SENSE", sense_tag);
          int dum_sense = 0;
          if (result == MB_TAG_NOT_FOUND) {
            result = mdbImpl->tag_create("SENSE", sizeof(int), MB_TAG_SPARSE, sense_tag, &dum_sense);
          }
          if (result != MB_SUCCESS) return result;
          dum_sense = -1;
          result = mdbImpl->tag_set_data(sense_tag, &reverse_set, 1, &dum_sense);
          if (result != MB_SUCCESS) return result;
        }
      }

      if( mdbImpl->add_entities( ss_handle, &entities_to_add[0], 
                                 entities_to_add.size()) != MB_SUCCESS ) 
        return MB_FAILURE; 

      //distribution factor stuff
      if( number_dist_factors_in_set )
      {
        //if this sideset does not already has a distribution factor array...set one
        const void* ptr = 0;
        int size = 0;
        if (MB_SUCCESS == mdbImpl->tag_get_data( mDistFactorTag, &ss_handle, 1, &ptr, &size )) {
          const double* data = reinterpret_cast<const double*>(ptr);
          size /= sizeof(double);
          std::copy( data, data+size, std::back_inserter( temp_dist_factor_vector ) );
        }
        
        ptr = &temp_dist_factor_vector[0];
        size =  temp_dist_factor_vector.size() * sizeof(double);
        if (mdbImpl->tag_set_data( mDistFactorTag, &ss_handle, 1, &ptr, &size ) != MB_SUCCESS)
          return MB_FAILURE;
      }
    }
  }

  return MB_SUCCESS;

}

MBErrorCode ReadNCDF::create_ss_elements( int *element_ids, 
                                            int *side_list, 
                                            int num_sides,  
                                            int num_dist_factors, 
                                            std::vector<MBEntityHandle> &entities_to_add,
                                            std::vector<MBEntityHandle> &reverse_entities,
                                            std::vector<double> &dist_factor_vector, 
                                           int ss_seq_id)
{
  //determine entity type from element id
  int i, k;

  // if there are dist. factors, create a vector to hold the array
  // and place this array as a tag onto the sideset meshset
  
  std::vector<double> temp_dist_factor_vector(num_dist_factors);
  std::vector<char> temp_string_storage(max_str_length+1);
  char *temp_string = &temp_string_storage[0];
  NcVar *temp_var;
  if( num_dist_factors )
  {
    INS_ID(temp_string, "dist_fact_ss%d", ss_seq_id);
    temp_var = ncFile->get_var(temp_string);
    if (NULL == temp_var || !temp_var->is_valid()) {
      readMeshIface->report_error("MBCN:: Problem getting dist fact variable.");
      return MB_FAILURE;
    }
    NcBool status = temp_var->get(&(temp_dist_factor_vector[0]),
                                  num_dist_factors);
    if (0 == status) {
      readMeshIface->report_error("MBCN:: Problem getting dist factors.");
      return MB_FAILURE;
    }
  }

  MBEntityType subtype;
  int num_side_nodes, num_elem_nodes;
  const MBEntityHandle* nodes;
  std::vector<MBEntityHandle> connectivity;
  int side_node_idx[32];

  int df_index = 0;
  for(i=0; i < num_sides; i++)
  {
    ExoIIElementType exoii_type;
    ReadBlockData block_data;
    block_data.elemType = EXOII_MAX_ELEM_TYPE;

    if( find_side_element_type( element_ids[i], exoii_type, block_data, df_index, side_list[i]) != MB_SUCCESS )
      continue; //isn't being read in this time

    MBEntityType type = ExoIIUtil::ExoIIElementMBEntity[exoii_type];

    MBEntityHandle ent_handle = element_ids[i] - block_data.startExoId + block_data.startMBId;

    const int side_num = side_list[i] - 1;
    if(type == MBHEX)
    {
      //get the nodes of the element
      if( mdbImpl->get_connectivity( ent_handle, nodes, num_elem_nodes ) != MB_SUCCESS )
        return MB_FAILURE;

      MBCN::SubEntityNodeIndices( type, num_elem_nodes, 2, side_num, subtype, num_side_nodes, side_node_idx );
      if (num_side_nodes <= 0)
        return MB_FAILURE;
      
      connectivity.resize( num_side_nodes );
      for (k = 0; k < num_side_nodes; ++k)
        connectivity[k] = nodes[ side_node_idx[k] ];
        
      if (MB_SUCCESS != create_sideset_element( connectivity, subtype, ent_handle ))
        return MB_FAILURE;
      entities_to_add.push_back( ent_handle );

      //read in distribution factor array
      if( num_dist_factors )
      {
        for(k=0; k<4; k++)
          dist_factor_vector.push_back( temp_dist_factor_vector[df_index++] );
      }
    }

    // if it is a Tet
    else if(type == MBTET)
    {
      //get the nodes of the element
      if( mdbImpl->get_connectivity( ent_handle, nodes, num_elem_nodes ) != MB_SUCCESS )
        return MB_FAILURE;

      MBCN::SubEntityNodeIndices( type, num_elem_nodes, 2, side_num, subtype, num_side_nodes, side_node_idx );
      if (num_side_nodes <= 0)
        return MB_FAILURE;
      
      connectivity.resize( num_side_nodes );
      for (k = 0; k < num_side_nodes; ++k)
        connectivity[k] = nodes[ side_node_idx[k] ];
        
      if (MB_SUCCESS != create_sideset_element( connectivity, subtype, ent_handle ))
        return MB_FAILURE;
      entities_to_add.push_back( ent_handle );

      //read in distribution factor array
      if( num_dist_factors )
      {
        for(k=0; k<3; k++)
          dist_factor_vector.push_back( temp_dist_factor_vector[df_index++] );
      }

    }
    else if( type == MBQUAD &&
             exoii_type >= EXOII_SHELL && exoii_type <= EXOII_SHELL9 )
    {

      //ent_handle = CREATE_HANDLE(MBQUAD, base_id, error );

      //just use this quad
      if( side_list[i] == 1)
      {
        entities_to_add.push_back( ent_handle );

        if( num_dist_factors )
        {
          for(k=0; k<4; k++)
            dist_factor_vector.push_back( temp_dist_factor_vector[df_index++] );
        }

        continue;
      }
      else if ( side_list[i] == 2)
      {
        reverse_entities.push_back(ent_handle);
        
        if( num_dist_factors )
        {
          for(k=0; k<4; k++)
            dist_factor_vector.push_back( temp_dist_factor_vector[df_index++] );
        }

        continue;
      }
      else
      {
        //get the nodes of the element
        if( mdbImpl->get_connectivity( ent_handle, nodes, num_elem_nodes ) != MB_SUCCESS )
          return MB_FAILURE;

        MBCN::SubEntityNodeIndices( type, num_elem_nodes, 1, side_num-2, subtype, num_side_nodes, side_node_idx );
        if (num_side_nodes <= 0)
          return MB_FAILURE;

        connectivity.resize( num_side_nodes );
        for (k = 0; k < num_side_nodes; ++k)
          connectivity[k] = nodes[ side_node_idx[k] ];

        if (MB_SUCCESS != create_sideset_element( connectivity, subtype, ent_handle ))
          return MB_FAILURE;
        entities_to_add.push_back( ent_handle );

        if( num_dist_factors )
        {
          for(k=0; k<2; k++)
            dist_factor_vector.push_back( temp_dist_factor_vector[df_index++] );
        }
      }
    }
    // if it is a Quad
    else if(type == MBQUAD)
    {
      //get the nodes of the element
      if( mdbImpl->get_connectivity( ent_handle, nodes, num_elem_nodes ) != MB_SUCCESS )
        return MB_FAILURE;

      MBCN::SubEntityNodeIndices( type, num_elem_nodes, 1, side_num, subtype, num_side_nodes, side_node_idx );
      if (num_side_nodes <= 0)
        return MB_FAILURE;
      
      connectivity.resize( num_side_nodes );
      for (k = 0; k < num_side_nodes; ++k)
        connectivity[k] = nodes[ side_node_idx[k] ];
        
      if (MB_SUCCESS != create_sideset_element( connectivity, subtype, ent_handle ))
        return MB_FAILURE;
      entities_to_add.push_back( ent_handle );

      //read in distribution factor array
      if( num_dist_factors )
      {
        for(k=0; k<2; k++)
          dist_factor_vector.push_back( temp_dist_factor_vector[df_index++] );
      }
    }
    else if(type == MBTRI)
    {
      int side_offset = 0;
      if(number_dimensions() == 3 && side_list[i] <= 2)
      {
        entities_to_add.push_back(ent_handle);
        if( num_dist_factors )
        {
          for(k=0; k<3; k++)
            dist_factor_vector.push_back( temp_dist_factor_vector[df_index++] );
        }
      }
      else
      {
        if(number_dimensions() == 3)
        {
          if(side_list[i] > 2)
            side_offset = 2;
        }

        //get the nodes of the element
        if( mdbImpl->get_connectivity( ent_handle, nodes, num_elem_nodes ) != MB_SUCCESS )
          return MB_FAILURE;

        MBCN::SubEntityNodeIndices( type, num_elem_nodes, 1, side_num-side_offset, subtype, num_side_nodes, side_node_idx );
        if (num_side_nodes <= 0)
          return MB_FAILURE;

        connectivity.resize( num_side_nodes );
        for (k = 0; k < num_side_nodes; ++k)
          connectivity[k] = nodes[ side_node_idx[k] ];

        if (MB_SUCCESS != create_sideset_element( connectivity, subtype, ent_handle ))
          return MB_FAILURE;
        entities_to_add.push_back( ent_handle );

        if( num_dist_factors )
        {
          for(k=0; k<2; k++)
            dist_factor_vector.push_back( temp_dist_factor_vector[df_index++] );
        }
      }

    }

  }

  return MB_SUCCESS; 
}

MBErrorCode ReadNCDF::create_sideset_element( const std::vector<MBEntityHandle>& connectivity, 
                                              MBEntityType type, MBEntityHandle& handle)
{
  // get adjacent entities
  MBErrorCode error = MB_SUCCESS;
  int to_dim = MBCN::Dimension(type);
  std::vector<MBEntityHandle> adj_ent;
  mdbImpl->get_adjacencies(&(connectivity[0]), 1, to_dim, false, adj_ent);

  // for each entity, see if we can find a match
  // if we find a match, return it
  bool match_found = false;
  std::vector<MBEntityHandle> match_conn;
  for(unsigned int i=0; i<adj_ent.size() && match_found == false; i++)
  {
    // get the connectivity
    error = mdbImpl->get_connectivity(&( adj_ent[i]), 1, match_conn );
    if(error != MB_SUCCESS)
      continue;

    // make sure they have the same number of vertices (higher order elements ?)
    if(match_conn.size() != connectivity.size())
      continue;

    // find a matching node
    std::vector<MBEntityHandle>::iterator iter;
    iter = std::find(match_conn.begin(), match_conn.end(), connectivity[0]);
    if(iter == match_conn.end())
      continue;
   
    // rotate to match connectivity 
    std::rotate(match_conn.begin(), iter, match_conn.end());

    bool they_match = true;
    unsigned int j;
    for(j=1; j<connectivity.size(); j++)
    {
      if(connectivity[j] != match_conn[j])
      {
        they_match = false;
        break;
      }
    }

    // if we didn't get a match
    if(they_match != true)
    {
      // try the opposite sense
      they_match = true;

      unsigned int k = connectivity.size() - 1;
      for(j=1; j<connectivity.size(); )
      {
        if(connectivity[j] != match_conn[k])
        {
          they_match = false;
          break;
        }
        ++j;
        --k;
      }
    }
    match_found = they_match;
    if(match_found == true)
      handle = adj_ent[i];
  }

  // if we didn't find a match, create an element
  if(match_found == false)
    error = mdbImpl->create_element(type, &connectivity[0], connectivity.size(), handle);

  return error;
}


MBErrorCode ReadNCDF::find_side_element_type( const int exodus_id, ExoIIElementType &elem_type, 
                                                ReadBlockData &block_data, int &df_index, int side_id)
{

  std::vector<ReadBlockData>::iterator iter, end_iter;
  iter = blocksLoading.begin();
  end_iter = blocksLoading.end();
  elem_type = EXOII_MAX_ELEM_TYPE;

  for(; iter != end_iter; ++iter )
  {
    if( exodus_id >= (*iter).startExoId && 
         exodus_id < (*iter).startExoId + (*iter).numElements )
    {
      elem_type = (*iter).elemType;

      //if we're not reading this block in
      if( !(*iter).reading_in ) 
      {
        //offset df_indes according to type
         
        if( elem_type >= EXOII_HEX && elem_type <= EXOII_HEX27 )
          df_index += 4;
        else if( elem_type >= EXOII_TETRA && elem_type <= EXOII_TETRA14 )
          df_index += 3;
        else if( elem_type >= EXOII_QUAD && elem_type <= EXOII_QUAD9 )
          df_index += 2;
        else if( elem_type >= EXOII_SHELL && elem_type <= EXOII_SHELL9)
        {
         if(side_id == 1 || side_id == 2)
           df_index += 4;
         else
           df_index += 2;
        }
        else if( elem_type >= EXOII_TRI && elem_type <= EXOII_TRI7 )
          df_index += 3;

        return MB_FAILURE;

      }

      block_data = *iter;

      return MB_SUCCESS;
    } 
  }
  return MB_FAILURE;
}

MBErrorCode ReadNCDF::read_qa_records()
{
  std::vector<std::string> qa_records;
  read_qa_information( qa_records );
  
  std::vector<char> tag_data;
  for (std::vector<std::string>::iterator i = qa_records.begin(); i != qa_records.end(); ++i) {
    std::copy( i->begin(), i->end(), std::back_inserter(tag_data) );
    tag_data.push_back( '\0' );
  }

  //if there were qa_records...tag them to the mCurrentMeshHandle
  if( !tag_data.empty() )
  {
    const void* ptr = &tag_data[0];
    int size = tag_data.size();
    if( mdbImpl->tag_set_data( mQaRecordTag, &mCurrentMeshHandle, 1, &ptr, &size  ) != MB_SUCCESS ) {
      return MB_FAILURE;
    }
  }

  return MB_SUCCESS;
}

MBErrorCode ReadNCDF::read_qa_information(std::vector<std::string> &qa_record_list)
{
  // inquire on the genesis file to find the number of qa records

  int number_records = 0;

  NcDim *temp_dim;
  GET_DIM(temp_dim, "num_qa_rec", number_records);
  std::vector<char> data(max_str_length+1);
  
  for(int i = 0; i < number_records; i++)
  {
    for(int j = 0; j < 4; j++)
    {
      data[max_str_length] = '\0';
      if (read_qa_string(&data[0], i, j) != MB_SUCCESS)
      {
        return MB_FAILURE;
      }
      qa_record_list.push_back(&data[0]);
    }
  }
  return MB_SUCCESS;
}

MBErrorCode ReadNCDF::read_qa_string(char *temp_string,
                                     int record_number,
                                     int record_position)
{
  NcVar *temp_var = ncFile->get_var("qa_records");
  if (NULL == temp_var || !temp_var->is_valid()) {
    readMeshIface->report_error("MBCN:: Problem getting qa record variable.");
    return MB_FAILURE;
  }
  NcBool status = temp_var->set_cur(record_number, record_position);
  if (0 == status) {
    readMeshIface->report_error("MBCN:: Problem setting current record number variable position.");
    return MB_FAILURE;
  }
  status = temp_var->get(temp_string,
                         1, 1, max_str_length);
  if (0 == status) {
    readMeshIface->report_error("MBCN:: Problem getting qa record string.");
    return MB_FAILURE;
  }
    // get the variable id in the exodus file

  return MB_SUCCESS;
}

MBErrorCode ReadNCDF::update(const char *exodus_file_name, 
                             const FileOptions& opts)
{
  //Function : updating current database from new exodus_file. 
  //Creator:   Jane Hu
  //opts is currently designed as following
  //tdata = <var_name>[, time][,op][,destination]
  //where var_name show the tag name to be updated, this version just takes
  //coord.
  //time is the optional, and it gives time step of each of the mesh
  //info in exodus file. It start from 1.
  //op is the operation that is going to be performed on the var_name info.
  //currently support 'set'
  //destination shows where to store the updated info, currently assume it is
  //stored in the same database by replacing the old info if there's no input
  //for destination, or the destination data is given in exodus format and just
  //need to update the coordinates.
  //Assumptions:
  //1. Assume the num_el_blk's in both DB and update exodus file are the same. 
  //2. Assume num_el_in_blk1...num_el_in_blk(num_el_blk) numbers are matching, may in 
  //different order. example: num_el_in_blk11 = num_el_in_blk22 && num_el_in_blk12 = 
  //num_el_in_blk21.
  //3. In exodus file, get node_num_map
  //4. loop through the node_num_map, use it to find the node in the cub file.
  //5. Replace coord[0][n] with coordx[m]+vals_nod_var1(time_step, m) for all directions for matching nodes.   
  
  MBErrorCode rval;
  std::string s;
  rval = opts.get_str_option("tdata", s ); 
  if(MB_SUCCESS != rval)
  {
    readMeshIface->report_error("MBCN:: Problem reading file options.");
    return MB_FAILURE;
  }
  std::vector< std::string > tokens;
  tokenize(s, tokens,",");
    
  //1. check for time step to find the match time
  int time_step = 1;
  if(tokens.size() > 1 && !tokens[1].empty())
  {
    const char* time_s = tokens[1].c_str();
    char* endptr;
    long int pval = strtol( time_s, &endptr, 0 );
    std::string end = endptr;
    if (!end.empty()) // syntax error
      return MB_TYPE_OUT_OF_RANGE;

    // check for overflow (parsing long int, returning int)
    time_step = pval;
    if (pval != (long int)time_step)
      return MB_TYPE_OUT_OF_RANGE;
    if(time_step <= 0)
      return MB_TYPE_OUT_OF_RANGE;
  }

  //2. check for the operations, currently support set.
  const char* op;
  if(tokens.size() > 2 && !tokens[2].empty())
    op = tokens[2].c_str();

  //3. check for destination, current only not implemented
  const char* des ;
  if(tokens.size() > 3 && !tokens[3].empty())
    des = tokens[3].c_str();
  else
    des = "";

  // a. read in the node_num_map and coords from the input exodus file.
  bool previously_loaded = false;
  std::string filename( exodus_file_name );
  ncFile = NULL;
  rval = check_file_status(filename, previously_loaded);
  if (MB_SUCCESS != rval)
    return rval;

  read_exodus_header(exodus_file_name);
  
  //read in the node_num_map .
  std::vector<int> ptr(numberNodes_loading);

  int varid = -1;
  int cstatus = nc_inq_varid (ncFile->id(), "node_num_map", &varid);
  if (cstatus == NC_NOERR && varid != -1) {
    NcVar *temp_var = ncFile->get_var("node_num_map");
    NcBool status = temp_var->get(&ptr[0], numberNodes_loading);
    if (0 == status) {
      readMeshIface->report_error("ReadNCDF:: Problem getting node number map data.");
      return MB_FAILURE;
    }
  }

  // read in the deformations.
  std::vector< std::vector<double> > deformed_arrays(3) ;
  std::vector< std::vector<double> >  orig_coords(3) ;
  deformed_arrays[0].reserve(numberNodes_loading);
  deformed_arrays[1].reserve(numberNodes_loading);
  deformed_arrays[2].reserve(numberNodes_loading);
  orig_coords[0].reserve(numberNodes_loading);
  orig_coords[1].reserve(numberNodes_loading);
  orig_coords[2].reserve(numberNodes_loading);

  NcVar *coordx = ncFile->get_var("vals_nod_var1");
  NcVar *coordy = ncFile->get_var("vals_nod_var2");
  NcVar *coordz;
  if(numberDimensions_loading == 3)
    coordz = ncFile->get_var("vals_nod_var3");
  if (NULL == coordx || !coordx->is_valid() ||
      NULL == coordy || !coordy->is_valid() ||
      (numberDimensions_loading == 3 && (NULL == coordz || !coordz->is_valid())) ) {
     readMeshIface->report_error("MBCN:: Problem getting coords variable.");
     return MB_FAILURE;
   }

  NcBool status = coordx->set_cur(time_step-1, 0);
  if (0 == status) {
    readMeshIface->report_error("MBCN:: Problem getting x deformation array.");
    return MB_FAILURE;
  }
  status = coordx->get(&deformed_arrays[0][0], 1,  numberNodes_loading);
  if (0 == status) {
    readMeshIface->report_error("MBCN:: Problem getting x deformation array.");
    return MB_FAILURE;
  }
  status = coordy->set_cur(time_step-1, 0);
  if (0 == status) {
    readMeshIface->report_error("MBCN:: Problem getting y deformation array.");
    return MB_FAILURE;
  }
  status = coordy->get(&deformed_arrays[1][0],  1, numberNodes_loading);
  if (0 == status) {
    readMeshIface->report_error("MBCN:: Problem getting y deformation array.");
    return MB_FAILURE;
  } 
  if (numberDimensions_loading == 3 )
  {
    status = coordz->set_cur(time_step-1, 0);
    if (0 == status) {
      readMeshIface->report_error("MBCN:: Problem getting z deformation array.");
      return MB_FAILURE;
    }
    status = coordz->get(&deformed_arrays[2][0], 1,numberNodes_loading);
    if (0 == status) {
      readMeshIface->report_error("MBCN:: Problem getting z deformation array.");
      return MB_FAILURE;
    }
  }

  NcVar *coord1 = ncFile->get_var("coordx");
  NcVar *coord2 = ncFile->get_var("coordy");
  NcVar *coord3;
  if(numberDimensions_loading == 3)
    coord3 = ncFile->get_var("coordz");
  if (NULL == coord1 || !coord1->is_valid() ||
      NULL == coord2 || !coord2->is_valid() ||
      (numberDimensions_loading == 3 && (NULL == coord3 || !coord3->is_valid())) ) {
    readMeshIface->report_error("MBCN:: Problem getting coords variable.");
    return MB_FAILURE;
   }

  status = coord1->get(&orig_coords[0][0],  numberNodes_loading);
  if (0 == status) {
    readMeshIface->report_error("MBCN:: Problem getting x coord array.");
    return MB_FAILURE;
  }
  status = coord2->get(&orig_coords[1][0],  numberNodes_loading);
  if (0 == status) {
    readMeshIface->report_error("MBCN:: Problem getting y coord array.");
    return MB_FAILURE;
  }
  if (numberDimensions_loading == 3 )
  {
    status = coord3->get(&orig_coords[2][0],  numberNodes_loading);
    if (0 == status) {
      readMeshIface->report_error("MBCN:: Problem getting z coord array.");
      return MB_FAILURE;
    }
  }

  //b. Deal with DB file : get node info. according to node_num_map.
  if (tokens[0] != "coord" && tokens[0] != "COORD")
    return MB_NOT_IMPLEMENTED;

  if( strcmp (op, "set") && strcmp (op, " set"))
    return MB_NOT_IMPLEMENTED;
  
  MBTag *globalId = &mGlobalIdTag; 
  MBEntityHandle entity_h = 0;
  MBRange entities;
  for(int i = 0; i < numberNodes_loading; i++)
  {
    int id = ptr[i];
    void * data[1] = {&id};
    double coords[3] = {0., 0., 0.};

    entities.clear();
    mdbImpl->get_entities_by_type_and_tag(entity_h, MBVERTEX, globalId, data, 1, entities);
    if(entities.empty())
      continue;
    else if(entities.size() > 1)
    {
      readMeshIface->report_error("ReadNCDF:: Multiple nodes share the same id.");
      return MB_FAILURE;
    } 

    //update the coordinates for this entity.
    coords[0] = orig_coords[0][i] + deformed_arrays[0][i];
    coords[1] = orig_coords[1][i] + deformed_arrays[1][i];
    if(numberDimensions_loading == 3 )
      coords[2] = orig_coords[2][i] + deformed_arrays[2][i];

    mdbImpl->set_coords(entities, coords);
  }
  return MB_SUCCESS;
}
 
void ReadNCDF::tokenize( const std::string& str,
                         std::vector<std::string>& tokens,
                         const char* delimiters )
{
  std::string::size_type last = str.find_first_not_of( delimiters, 0 );
  std::string::size_type pos  = str.find_first_of( delimiters, last );
  while (std::string::npos != pos && std::string::npos != last) {
    tokens.push_back( str.substr( last, pos - last ) );
    last = str.find_first_not_of( delimiters, pos );
    pos  = str.find_first_of( delimiters, last );
    if(std::string::npos == pos)
      pos = str.size();
  }
}
