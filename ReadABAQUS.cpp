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

#include "ReadABAQUS.hpp"

#include <algorithm>
#include <time.h>
#include <string>
#include <assert.h>
#include <stdio.h>
#include <cmath>

#include "MBCN.hpp"
#include "MBRange.hpp"
#include "MBInterface.hpp"
#include "MBTagConventions.hpp"
#include "MBInternals.hpp"
#include "MBReadUtilIface.hpp"
#include "MBAffineXform.hpp"
// #include "abaqus_order.h"
#include "FileOptions.hpp"

#define ABQ_AMBIGUOUS "AMBIGUOUS"
#define ABQ_UNDEFINED "UNDEFINED"
#define DEG2RAD 0.017453292519943295769236907684886

#define MB_RETURN_IF_FAIL if (MB_SUCCESS != status) return status

MBReaderIface* ReadABAQUS::factory( MBInterface* iface )
  { return new ReadABAQUS( iface ); }



ReadABAQUS::ReadABAQUS(MBInterface* impl)
  : mdbImpl(impl)
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

  mSetTypeTag        = 0;
  mPartHandleTag     = 0;
  mInstancePIDTag    = 0;
  mInstanceGIDTag    = 0;
  mLocalIDTag        = 0;
  mInstanceHandleTag = 0;
  mAssemblyHandleTag = 0;
  mSetNameTag        = 0;
  mMatNameTag        = 0;

  mat_id             = 0;

  //! get and cache predefined tag handles
  mMaterialSetTag  = get_tag(MATERIAL_SET_TAG_NAME,   sizeof(int),MB_TAG_SPARSE,MB_TYPE_INTEGER);
  mDirichletSetTag = get_tag(DIRICHLET_SET_TAG_NAME,  sizeof(int),MB_TAG_SPARSE,MB_TYPE_INTEGER);
  mNeumannSetTag   = get_tag(NEUMANN_SET_TAG_NAME,    sizeof(int),MB_TAG_SPARSE,MB_TYPE_INTEGER);
  int def_val[4] = {0,0,0,0};
  mHasMidNodesTag  = get_tag(HAS_MID_NODES_TAG_NAME,4*sizeof(int),MB_TAG_SPARSE,MB_TYPE_INTEGER,def_val);

  mSetTypeTag        = get_tag(ABAQUS_SET_TYPE_TAG_NAME,          sizeof(int),           MB_TAG_SPARSE,MB_TYPE_INTEGER);
  mPartHandleTag     = get_tag(ABAQUS_PART_HANDLE_TAG_NAME,       sizeof(MBEntityHandle),MB_TAG_SPARSE,MB_TYPE_HANDLE);
  mInstanceHandleTag = get_tag(ABAQUS_INSTANCE_HANDLE_TAG_NAME,   sizeof(MBEntityHandle),MB_TAG_DENSE, MB_TYPE_HANDLE);
  mAssemblyHandleTag = get_tag(ABAQUS_ASSEMBLY_HANDLE_TAG_NAME,   sizeof(MBEntityHandle),MB_TAG_DENSE, MB_TYPE_HANDLE);
  mInstancePIDTag    = get_tag(ABAQUS_INSTANCE_PART_ID_TAG_NAME,  sizeof(int),           MB_TAG_SPARSE,MB_TYPE_INTEGER);
  mInstanceGIDTag    = get_tag(ABAQUS_INSTANCE_GLOBAL_ID_TAG_NAME,sizeof(int),           MB_TAG_SPARSE,MB_TYPE_INTEGER);
  mLocalIDTag        = get_tag(ABAQUS_LOCAL_ID_TAG_NAME,          sizeof(int),           MB_TAG_DENSE, MB_TYPE_INTEGER);
  mSetNameTag        = get_tag(ABAQUS_SET_NAME_TAG_NAME,         ABAQUS_SET_NAME_LENGTH, MB_TAG_SPARSE,MB_TYPE_OPAQUE,0);
  mMatNameTag        = get_tag(ABAQUS_MAT_NAME_TAG_NAME,         ABAQUS_MAT_NAME_LENGTH, MB_TAG_SPARSE,MB_TYPE_OPAQUE,0);

}

void ReadABAQUS::reset()
{

}


ReadABAQUS::~ReadABAQUS() 
{
  std::string iface_name = "MBReadUtilIface";
  mdbImpl->release_interface(iface_name, readMeshIface);
  if (NULL != abFile)
    abFile.close();
}

/* 

MBErrorCode ReadABAQUS::check_file_stats()
* check for existence of file
* initialize meshsets, and offsets if necessary

*/

MBErrorCode ReadABAQUS::read_tag_values( const char* /* file_name */,
					 const char* /* tag_name */,
					 const FileOptions& /* opts */,
					 std::vector<int>& /* tag_values_out */,
					 const IDTag* /* subset_list */,
					 int /* subset_list_length */ )
{
  return MB_NOT_IMPLEMENTED;
}



MBErrorCode ReadABAQUS::load_file(const char *abaqus_file_name,
				  MBEntityHandle file_set,
				  const FileOptions& opts,
				  const MBReaderIface::IDTag* subset_list,
				  int subset_list_length,
				  const MBTag* file_id_tag)
{
  MBErrorCode status;

  // open file
  abFile.open(abaqus_file_name);

  bool in_unsupported = false;

  next_line_type = get_next_line_type();
  while (next_line_type != abq_eof)
    {
      switch (next_line_type)
	{
	case abq_keyword_line:
	  in_unsupported=false;
	  switch (get_keyword())
	    {
	    case abq_heading:
	      // read header
	      status = read_heading(file_set);
	      break;
	      
	    case abq_part:
	      // read parts until done
	      status = read_part(file_set);
	      break;
	      
	    case abq_assembly:
	      // read assembly (or assemblies?)
	      status = read_assembly(file_set);
	      break;
	      
	    default:
	      // skip reading other content for now 
	      // (e.g. material properties, loads, surface interactions, etc)
	      in_unsupported = true;
	      // std::cout << "Ignoring unsupported keyword: " << readline << std::endl;
	    }
	  MB_RETURN_IF_FAIL;
	  break;
	case abq_comment_line:
	  break;
	case abq_data_line:
	  if (!in_unsupported)
	    {
	      std::cerr << "Internal Error: found ABAQUS data line outside a keyword block." 
			<< std::endl << readline << std::endl;
	      return MB_FAILURE;
	    }
	  break;
	default:
	  std::cerr << "Internal Error: found unrecognized ABAQUS line." 
		    << std::endl << readline << std::endl;
	  return MB_FAILURE;
	}
      next_line_type = get_next_line_type();
    }


  // temporary??? delete parts
  // get all node sets in part
  MBRange part_sets;
  int tag_val = ABQ_PART_SET;
  void* tag_data[] = {&tag_val};
  status = mdbImpl->get_entities_by_type_and_tag(file_set,
						 MBENTITYSET,
						 &mSetTypeTag,
						 tag_data, 1, part_sets);
  MB_RETURN_IF_FAIL;
  
  for (MBRange::iterator part_set = part_sets.begin();
       part_set != part_sets.end();
       part_set++)
    {
      MBRange ent_sets;
      tag_val = ABQ_NODE_SET;
      tag_data[0] = &tag_val;

      status = mdbImpl->get_entities_by_type_and_tag(*part_set,
						     MBENTITYSET,
						     &mSetTypeTag,
						     tag_data, 1, ent_sets);
      MB_RETURN_IF_FAIL;
  
      status = mdbImpl->delete_entities(ent_sets);
      MB_RETURN_IF_FAIL;

      tag_val = ABQ_ELEMENT_SET;
      tag_data[0] = &tag_val;

      status = mdbImpl->get_entities_by_type_and_tag(*part_set,
						     MBENTITYSET,
						     &mSetTypeTag,
						     tag_data, 1, ent_sets);
      MB_RETURN_IF_FAIL;
  
      status = mdbImpl->delete_entities(ent_sets);
      MB_RETURN_IF_FAIL;

      MBRange node_list,ele_list;
      status = get_set_elements(*part_set,ele_list);
      MB_RETURN_IF_FAIL;

      status = mdbImpl->delete_entities(ele_list);
      MB_RETURN_IF_FAIL;

      status = mdbImpl->get_entities_by_dimension(*part_set,0,node_list);
      MB_RETURN_IF_FAIL;

      status = mdbImpl->delete_entities(node_list);
      MB_RETURN_IF_FAIL;



    }
  

  return MB_SUCCESS;
}
  

MBErrorCode ReadABAQUS::read_heading(MBEntityHandle file_set)
{
  
  std::vector<std::string> tokens;
  
  // current line is only heading token. get next line
  next_line_type = get_next_line_type();

  // perhaps keep first line and tag gometry with title?
  
  while (abq_data_line    == next_line_type || 
	 abq_comment_line == next_line_type   )
    next_line_type = get_next_line_type();
  
  return MB_SUCCESS;
}

MBErrorCode ReadABAQUS::read_assembly(MBEntityHandle file_set)
{
  MBErrorCode status = MB_SUCCESS;

  std::vector<std::string> tokens;
  std::map<std::string,std::string> params;
  std::map<std::string,abaqus_assembly_params> requiredParams;
  requiredParams["NAME"]         = abq_assembly_name;
  
  std::map<std::string,abaqus_assembly_params> allowableParams;
  allowableParams[ABQ_AMBIGUOUS] = abq_assembly_ambiguous;
  
  abaqus_assembly_params param;
  
  std::string assembly_name;

  // tokenize last line read
  tokenize(readline,tokens,",\n");
  extract_keyword_parameters(tokens,params);

  // search for required parameters
  for (std::map<std::string,abaqus_assembly_params>::iterator thisParam=requiredParams.begin();
       thisParam != requiredParams.end();
       thisParam++)
    {
      std::string param_key = match( (*thisParam).first,params );
      param = requiredParams[param_key];
      switch (param)
	{
	case abq_assembly_name:
	  assembly_name = params[param_key];
	  params.erase(param_key);
	  // std::cout << "Adding ASSEMBLY with name: " << assembly_name << std::endl; // REMOVE
	  break;
	default:
	  // std::cout << "Missing required ASSEMBLY parameter " << (*thisParam).first << std::endl;
	  return MB_FAILURE;
	}
    }
  
  // process parameters
  for (std::map<std::string,std::string>::iterator thisParam=params.begin();
       thisParam != params.end();
       thisParam++)
    {
      // look for unambiguous match with this node parameter
      param = allowableParams[match( (*thisParam).first, allowableParams )];
      switch (param)
	{
	case abq_assembly_ambiguous:
	  // std::cout << "\tIgnoring ambiguous ASSEMBLY parameter: " << (*thisParam).first
	  //	    << "=" << (*thisParam).second << std::endl;
	  break;
	default:
	  // std::cout << "\tIgnoring unsupported ASSEMBLY parameter: " << (*thisParam).first
	  //	    << "=" << (*thisParam).second << std::endl;
	  break;
	}
    }


  MBEntityHandle assembly_set;
  
  status = add_entity_set(file_set,ABQ_ASSEMBLY_SET,assembly_name,assembly_set);

  next_line_type = get_next_line_type();

  bool end_assembly = false;
  bool in_unsupported = false;

  while (next_line_type != abq_eof && !end_assembly)
    {
      switch(next_line_type)
	{
	case abq_keyword_line:
	  in_unsupported = false;
	  switch(get_keyword())
	    {
	    case abq_end_assembly:
	      end_assembly = true;
	      break;
	    case abq_instance:
	      status = read_instance(assembly_set,file_set);
	      break;
	    case abq_nset:
	      status = read_node_set(assembly_set,file_set);
	      break;
	    default:
	      in_unsupported = true;
	      // std::cout << "\tIgnoring unsupported keyword in this ASSEMBLY: "
	      //		<< readline << std::endl;
	      next_line_type = get_next_line_type();
	      break;
	    }
	  break;
	case abq_comment_line:
	  next_line_type = get_next_line_type();
	  break;
	case abq_data_line:
	  if (!in_unsupported)
	    {
	      // std::cout << "Internal Error: Data lines not allowed in ASSEMBLY keyword."
	      //		<< std::endl << readline << std::endl;
	      return MB_FAILURE;
	    }
	  next_line_type = get_next_line_type();
	  break;
	case abq_blank_line:
	  // std::cout << "Error: Blank lines are not allowed." << std::endl;
	  return MB_FAILURE;
	default:
	  // std::cout << "Error reading ASSEMBLY " << assembly_name << std::endl;
	  return MB_FAILURE;
	  
	}
      MB_RETURN_IF_FAIL;
	  
    }


  num_assembly_instances[assembly_set] = 0;

  return MB_SUCCESS;
}


MBErrorCode ReadABAQUS::read_instance(MBEntityHandle assembly_set,MBEntityHandle file_set)
{
  MBErrorCode status = MB_SUCCESS;

  std::vector<std::string> tokens;
  std::map<std::string,std::string> params;
  std::map<std::string,abaqus_instance_params> requiredParams;
  requiredParams["NAME"]         = abq_instance_name;
  requiredParams["PART"]         = abq_instance_part;
  
  std::map<std::string,abaqus_instance_params> allowableParams;
  allowableParams[ABQ_AMBIGUOUS] = abq_instance_ambiguous;
  
  abaqus_instance_params param;
  
  std::string instance_name,part_name;

  // tokenize last line read
  tokenize(readline,tokens,",\n");
  extract_keyword_parameters(tokens,params);

  // search for required parameters
  for (std::map<std::string,abaqus_instance_params>::iterator thisParam=requiredParams.begin();
       thisParam != requiredParams.end();
       thisParam++)
    {
      std::string param_key = match( (*thisParam).first,params );
      param = requiredParams[param_key];
      switch (param)
	{
	case abq_instance_name:
	  instance_name = params[param_key];
	  params.erase(param_key);
	  break;
	case abq_instance_part:
	  part_name = params[param_key];
	  params.erase(param_key);
	  break;
	default:
	  // std::cout << "Missing required INSTANCE parameter " << (*thisParam).first << std::endl;
	  return MB_FAILURE;
	}
    }
  // std::cout << "\tAdding INSTANCE with name: " << instance_name << " of PART wit name: " << part_name <<  std::endl; // REMOVE

  // process parameters
  for (std::map<std::string,std::string>::iterator thisParam=params.begin();
       thisParam != params.end();
       thisParam++)
    {
      // look for unambiguous match with this node parameter
      param = allowableParams[match( (*thisParam).first, allowableParams )];
      switch (param)
	{
	case abq_instance_ambiguous:
	  // std::cout << "\t\tIgnoring ambiguous INSTANCE parameter: " << (*thisParam).first
	  //	    << "=" << (*thisParam).second << std::endl;
	  break;
	default:
	  // std::cout << "\t\tIgnoring unsupported INSTANCE parameter: " << (*thisParam).first
	  //	    << "=" << (*thisParam).second << std::endl;
	  break;
	}
    }


  next_line_type = get_next_line_type();

  bool read_translation = false;
  bool read_rotation = false;
  std::vector<double> translation(3,0);
  std::vector<double> rotation(7,0);
  bool end_instance = false;
  bool in_unsupported = false;

  while (next_line_type != abq_eof && !end_instance)
    {
      switch(next_line_type)
	{
	case abq_keyword_line:
	  in_unsupported=false;
	  switch(get_keyword())
	    {
	    case abq_end_instance:
	      end_instance = true;
	      break;
	    default:
	      in_unsupported = true;
	      // std::cout << "\t\tIgnoring unsupported keyword in this INSTANCE: "
	      //		<< readline << std::endl;
	      next_line_type = get_next_line_type();
	      break;
	    }
	case abq_comment_line:
	  next_line_type = get_next_line_type();
	  break;
	case abq_data_line:
	  if (!in_unsupported)
	    {
	      tokenize(readline,tokens,", \n");
	      if (!read_translation)
		{
		  if (tokens.size() != 3)
		    {
		      std::cerr << "Wrong number of entries on INSTANCE translation line:" 
				<< std::endl << readline << std::endl;
		      return MB_FAILURE;
		    }
		  
		  for (unsigned int i=0;i<3;i++)
		    translation[i] = atof(tokens[i].c_str());
		  
		  read_translation = true;
		}
	      else
		if (!read_rotation)
		  {
		    if (tokens.size() != 7)
		      {
			std::cerr << "Wrong number of entries on INSTANCE rotation line:" 
				  << std::endl << readline << std::endl;
			return MB_FAILURE;
		      }
		    for (unsigned int i=0;i<7;i++)
		      rotation[i] = atof(tokens[i].c_str());
		    
		    read_rotation = true;
		  }
		else
		  {
		    std::cerr << "Too many data lines for this INSTANCE: " << instance_name << std::endl;
		    return MB_FAILURE;
		  }
	    }
	  next_line_type = get_next_line_type();
	  break;
	case abq_blank_line:
	  std::cerr << "Error: Blank lines are not allowed." << std::endl;
	  return MB_FAILURE;
	default:
	  std::cerr << "Error reading INSTANCE " << instance_name << std::endl;
	  return MB_FAILURE;
	  
	}
      
   }

  MBEntityHandle instance_set;
  
  status = create_instance_of_part(file_set,assembly_set,part_name,
				   instance_name,instance_set,translation,rotation);
  MB_RETURN_IF_FAIL;

  return MB_SUCCESS;

}


MBErrorCode ReadABAQUS::read_part(MBEntityHandle file_set)
{
  MBErrorCode status = MB_SUCCESS;

  std::vector<std::string> tokens;
  std::map<std::string,std::string> params;
  std::map<std::string,abaqus_part_params> requiredParams;
  requiredParams["NAME"]         = abq_part_name;
  
  std::map<std::string,abaqus_part_params> allowableParams;
  allowableParams[ABQ_AMBIGUOUS] = abq_part_ambiguous;
  
  abaqus_part_params param;
  
  std::string part_name;

  // tokenize last line read
  tokenize(readline,tokens,",\n");
  extract_keyword_parameters(tokens,params);

  // search for required parameters
  for (std::map<std::string,abaqus_part_params>::iterator thisParam=requiredParams.begin();
       thisParam != requiredParams.end();
       thisParam++)
    {
      std::string param_key = match( (*thisParam).first,params );
      param = requiredParams[param_key];
      switch (param)
	{
	case abq_part_name:
	  part_name = params[param_key];
	  params.erase(param_key);
	  // std::cout << "Adding PART with name: " << part_name << std::endl; // REMOVE
	  break;
	default:
	  std::cerr << "Missing required PART parameter " << (*thisParam).first << std::endl;
	  return MB_FAILURE;
	}
    }
  
  // process parameters
  for (std::map<std::string,std::string>::iterator thisParam=params.begin();
       thisParam != params.end();
       thisParam++)
    {
      // look for unambiguous match with this node parameter
      param = allowableParams[match( (*thisParam).first, allowableParams )];
      switch (param)
	{
	case abq_part_ambiguous:
	  // std::cout << "\tIgnoring ambiguous PART parameter: " << (*thisParam).first
	  //	    << "=" << (*thisParam).second << std::endl;
	  break;
	default:
	  // std::cout << "\tIgnoring unsupported PART parameter: " << (*thisParam).first
	  //	    << "=" << (*thisParam).second << std::endl;
	  break;
	}
    }


  MBEntityHandle part_set;
  
  status = add_entity_set(file_set,ABQ_PART_SET,part_name,part_set);

  next_line_type = get_next_line_type();

  bool end_part = false;
  bool in_unsupported = false;

  while (next_line_type != abq_eof && !end_part)
    {
      switch(next_line_type)
	{
	case abq_keyword_line:
	  in_unsupported=false;
	  switch(get_keyword())
	    {
	    case abq_end_part:
	      end_part = true;
	      break;
	    case abq_node:
	      status = read_node_list(part_set);
	      break;
	    case abq_element:
	      status = read_element_list(part_set);
	      break;
	    case abq_nset:
	      status = read_node_set(part_set);
	      break;
	    case abq_elset:
	      status = read_element_set(part_set);
	      break;
	    case abq_solid_section:
	      status = read_solid_section(part_set);
	      break;
	    default:
	      in_unsupported = true;
	      // std::cout << "\tIgnoring unsupported keyword in this PART: "
	      //		<< readline << std::endl;
	      next_line_type = get_next_line_type();
	      break;
	    }
	  MB_RETURN_IF_FAIL;
	  break;
	case abq_comment_line:
	  next_line_type = get_next_line_type();
	  break;
	case abq_data_line:
	  if (!in_unsupported)
	    {
	      std::cerr << "Internal Error: Data lines not allowed in PART keyword."
	      		<< std::endl << readline << std::endl;
	      return MB_FAILURE;
	    }
	  next_line_type = get_next_line_type();
	  break;
	case abq_blank_line:
	  std::cerr << "Error: Blank lines are not allowed." << std::endl;
	  return MB_FAILURE;
	default:
	  std::cerr << "Error reading PART " << part_name << std::endl;
	  return MB_FAILURE;
	  
	}
    }

  num_part_instances[part_set] = 0;

  return MB_SUCCESS;
}  

MBErrorCode ReadABAQUS::read_solid_section(MBEntityHandle parent_set)
{
  MBErrorCode status;

  std::vector<std::string> tokens;
  std::map<std::string,std::string> params;
  std::map<std::string,abaqus_solid_section_params> requiredParams;
  requiredParams["ELSET"]         = abq_solid_section_elset;
  requiredParams["MATERIAL"]      = abq_solid_section_matname;

  std::map<std::string,abaqus_solid_section_params> allowableParams;
  allowableParams[ABQ_AMBIGUOUS] = abq_solid_section_ambiguous;

  abaqus_solid_section_params param;

  // tokenize last line read
  tokenize(readline,tokens,",\n");
  extract_keyword_parameters(tokens,params);
  
  std::string elset_name,mat_name;
  
  // search for required parameters
  for (std::map<std::string,abaqus_solid_section_params>::iterator thisParam=requiredParams.begin();
       thisParam != requiredParams.end();
       thisParam++)
    {
      std::string param_key = match( (*thisParam).first,params );
      param = requiredParams[param_key];
      switch (param)
	{
	case abq_solid_section_elset:
	  elset_name =  params[param_key];
	  params.erase(param_key);
	  break;
	case abq_solid_section_matname:
	  mat_name =  params[param_key];
	  params.erase(param_key);
	  break;
	default:
	  std::cerr << "Missing required SOLID SECTION parameter " << (*thisParam).first << std::endl;
	  return MB_FAILURE;
	}
    }
  // std::cout << "\tAdding SOLID SECTION with to ELEMENT SET: " << elset_name << " with material: " << mat_name << std::endl; // REMOVE

  // process parameters
  for (std::map<std::string,std::string>::iterator thisParam=params.begin();
       thisParam != params.end();
       thisParam++)
    {
      // look for unambiguous match with this node parameter
      param = allowableParams[match( (*thisParam).first, allowableParams )];
      switch (param)
	{
	case abq_solid_section_ambiguous:
	  // std::cout << "\t\tIgnoring ambiguous SOLID_SECTION parameter: " << (*thisParam).first
	  //	    << "=" << (*thisParam).second << std::endl;
	  break;
	default:
	  // std::cout << "\t\tIgnoring unsupported SOLID_SECTION parameter: " << (*thisParam).first
	  //	    << "=" << (*thisParam).second << std::endl;
	  break;
	}
    }
  
  MBEntityHandle set_handle;
  status = get_set_by_name(parent_set,ABQ_ELEMENT_SET,elset_name,set_handle);

  status = mdbImpl->tag_set_data(mMatNameTag,&set_handle,1,mat_name.c_str());
  MB_RETURN_IF_FAIL;

  if (0 == matIDmap[mat_name])
    matIDmap[mat_name] = ++mat_id;

  status = mdbImpl->tag_set_data(mMaterialSetTag,&set_handle,1,&(matIDmap[mat_name]));
  MB_RETURN_IF_FAIL;

  next_line_type = get_next_line_type();
  
  while (next_line_type != abq_eof &&
	 next_line_type != abq_keyword_line)
    next_line_type = get_next_line_type();
    
  return MB_SUCCESS;

}

MBErrorCode ReadABAQUS::read_element_set(MBEntityHandle parent_set, MBEntityHandle file_set, MBEntityHandle assembly_set)
{
  MBErrorCode status;

  std::vector<std::string> tokens;
  std::map<std::string,std::string> params;
  std::map<std::string,abaqus_elset_params> requiredParams;
  requiredParams["ELSET"]         = abq_elset_elset;

  std::map<std::string,abaqus_elset_params> allowableParams;
  allowableParams[ABQ_AMBIGUOUS] = abq_elset_ambiguous;
  allowableParams["GENERATE"]    = abq_elset_generate;
  allowableParams["INSTANCE"]    = abq_elset_instance;

  abaqus_elset_params param;

  std::string elset_name;
  bool generate_elset = false;
  std::string instance_name;
  MBEntityHandle element_container_set = parent_set;

  // tokenize last line read
  tokenize(readline,tokens,",\n");
  extract_keyword_parameters(tokens,params);
  
  MBRange element_range;
  
  // search for required parameters
  for (std::map<std::string,abaqus_elset_params>::iterator thisParam=requiredParams.begin();
       thisParam != requiredParams.end();
       thisParam++)
    {
      std::string param_key = match( (*thisParam).first,params );
      param = requiredParams[param_key];
      switch (param)
	{
	case abq_elset_elset:
	  elset_name = params[param_key];
	  params.erase(param_key);
	  // std::cout << "\tAdding ELSET with name: " << elset_name << std::endl; // REMOVE
	  break;
	default:
	  std::cerr << "Missing required ELSET parameter " << (*thisParam).first << std::endl;
	  return MB_FAILURE;
	}
    }
  
  // process parameters
  for (std::map<std::string,std::string>::iterator thisParam=params.begin();
       thisParam != params.end();
       thisParam++)
    {
      // look for unambiguous match with this node parameter
      param = allowableParams[match( (*thisParam).first, allowableParams )];
      switch (param)
	{
	case abq_elset_generate:
	  generate_elset = true;
	  break;
	case abq_elset_instance:
	  instance_name = (*thisParam).second;
	  status = get_set_by_name(parent_set,ABQ_INSTANCE_SET,instance_name,element_container_set);
	  MB_RETURN_IF_FAIL;
	  break;
	case abq_elset_ambiguous:
	  // std::cout << "\t\tIgnoring ambiguous ELSET parameter: " << (*thisParam).first
	  //	    << "=" << (*thisParam).second << std::endl;
	  break;
	default:
	  // std::cout << "\t\tIgnoring unsupported ELSET parameter: " << (*thisParam).first
	  //	    << "=" << (*thisParam).second << std::endl;
	  break;
	}
    }
  
  
  std::vector<int> element_list;
  MBRange tmp_element_range;

  next_line_type = get_next_line_type();
  
  while (next_line_type != abq_eof &&
	 next_line_type != abq_keyword_line)
    {
      if (abq_data_line == next_line_type)
	{
	  tokenize(readline,tokens,", \n");
	  if (generate_elset)
	    {
	      if (tokens.size() != 3)
		{
		  std::cerr << "Wrong number (" << tokens.size() << ") of entries on GENERATE element set data line:" 
			    << std::endl << readline << std::endl;
		  return MB_FAILURE;
		}
	      int e1 = atoi(tokens[0].c_str());
	      int e2 = atoi(tokens[1].c_str());
	      int incr = atoi(tokens[2].c_str());
	      if ( ( (e2-e1)%incr ) != 0)
		{
		  std::cerr << "Invalid data on GENERATE element set data line:" 
			    << std::endl << readline << std::endl;
		  return MB_FAILURE;
		}
	      for (int element_id=e1; element_id<=e2;element_id+=incr)
		element_list.push_back(element_id);
	    }
	  else
	    for (unsigned int idx=0;idx<tokens.size();idx++)
	      if(isalpha(tokens[idx][0]))
		{
		  tmp_element_range.clear();
		  status = get_set_elements_by_name(element_container_set,ABQ_ELEMENT_SET,tokens[idx],tmp_element_range);
		  MB_RETURN_IF_FAIL;
		  
		  element_range.merge(tmp_element_range);
		}
	      else
		element_list.push_back(atoi(tokens[idx].c_str()));
	}
      
      next_line_type = get_next_line_type();
    }
  
  tmp_element_range.clear();
  status = get_elements_by_id(element_container_set,element_list,tmp_element_range);
  MB_RETURN_IF_FAIL;
  
  element_range.merge(tmp_element_range);

  MBEntityHandle element_set;
  
  status = add_entity_set(parent_set,ABQ_ELEMENT_SET,elset_name,element_set);
  MB_RETURN_IF_FAIL;
  
  status = mdbImpl->add_entities(element_set,element_range);
  MB_RETURN_IF_FAIL;
  
  // SHOULD WE EVER DO THIS???
  if (file_set)
    {
      status = mdbImpl->add_entities(file_set,&element_set,1);
      MB_RETURN_IF_FAIL;
    }
 
  // SHOULD WE EVER DO THIS???
  if (assembly_set)
    {
      status = mdbImpl->add_entities(assembly_set,&element_set,1);
      MB_RETURN_IF_FAIL;
      
      status = mdbImpl->tag_set_data(mAssemblyHandleTag,&element_set,1,&assembly_set);
      MB_RETURN_IF_FAIL;
    }
  
  return MB_SUCCESS;

}



MBErrorCode ReadABAQUS::read_node_set(MBEntityHandle parent_set,MBEntityHandle file_set, MBEntityHandle assembly_set)
{
  MBErrorCode status;

  std::vector<std::string> tokens;
  std::map<std::string,std::string> params;
  std::map<std::string,abaqus_nset_params> requiredParams;
  requiredParams["NSET"]         = abq_nset_nset;

  std::map<std::string,abaqus_nset_params> allowableParams;
  allowableParams[ABQ_AMBIGUOUS] = abq_nset_ambiguous;
  allowableParams["ELSET"]       = abq_nset_elset;
  allowableParams["GENERATE"]    = abq_nset_generate;
  allowableParams["INSTANCE"]    = abq_nset_instance;

  abaqus_nset_params param;

  std::string nset_name;
  bool make_from_elset = false;
  bool generate_nset = false;
  std::string elset_name, instance_name;
  MBEntityHandle node_container_set = parent_set;

  // tokenize last line read
  tokenize(readline,tokens,",\n");
  extract_keyword_parameters(tokens,params);

  MBRange node_range;
  
  // search for required parameters
  for (std::map<std::string,abaqus_nset_params>::iterator thisParam=requiredParams.begin();
       thisParam != requiredParams.end();
       thisParam++)
    {
      std::string param_key = match( (*thisParam).first,params );
      param = requiredParams[param_key];
      switch (param)
	{
	case abq_nset_nset:
	  nset_name =  params[param_key];
	  params.erase(param_key);
	  // std::cout << "\tAdding NSET with name: " << nset_name << std::endl; // REMOVE
	  break;
	default:
	  std::cerr << "Missing required NSET parameter " << (*thisParam).first << std::endl;
	  return MB_FAILURE;
	}
    }

  // process parameters
  for (std::map<std::string,std::string>::iterator thisParam=params.begin();
       thisParam != params.end();
       thisParam++)
    {
      // look for unambiguous match with this node parameter
      param = allowableParams[match( (*thisParam).first, allowableParams )];
      switch (param)
	{
	case abq_nset_elset:
	  make_from_elset = true;
	  elset_name = (*thisParam).second;
	  break;
	case abq_nset_generate:
	  generate_nset = true;
	  break;
	case abq_nset_instance:
	  instance_name = (*thisParam).second;
	  status = get_set_by_name(parent_set, ABQ_INSTANCE_SET, instance_name,node_container_set);
	  MB_RETURN_IF_FAIL;
	  break;
	case abq_nset_ambiguous:
	  // std::cout << "\t\tIgnoring ambiguous NSET parameter: " << (*thisParam).first
	  // 	    << "=" << (*thisParam).second << std::endl;
	  break;
	default:
	  // std::cout << "\t\tIgnoring unsupported NSET parameter: " << (*thisParam).first
	  //	    << "=" << (*thisParam).second << std::endl;
	  break;
	}
    }
  
  if (make_from_elset && generate_nset)
    {
      std::cerr << "Incompatible NSET parameters ELSET & GENERATE " << std::endl;
      return MB_FAILURE;
    }

  if (make_from_elset)
    {
      status = get_set_nodes(parent_set,ABQ_ELEMENT_SET,elset_name,node_range);
      MB_RETURN_IF_FAIL;
    }
  else
    {
      std::vector<int> node_list;
      MBRange tmp_node_range;

      next_line_type = get_next_line_type();
      
      while (next_line_type != abq_eof &&
	     next_line_type != abq_keyword_line)
	{
	  if (abq_data_line == next_line_type)
	    {
	      tokenize(readline,tokens,", \n");
	      if (generate_nset)
		{
		  if (tokens.size() != 3)
		    {
		      std::cerr << "Wrong number of entries on GENERATE node set data line:" 
				<< std::endl << readline << std::endl;
		      return MB_FAILURE;
		    }
		  int n1 = atoi(tokens[0].c_str());
		  int n2 = atoi(tokens[1].c_str());
		  int incr = atoi(tokens[2].c_str());
		  if ( ( (n2-n1) % incr ) != 0)
		    {
		      std::cerr << "Invalid data on GENERATE node set data line:" 
				<< std::endl << readline << std::endl;
		      return MB_FAILURE;
		    }
		  for (int node_id=n1; node_id<=n2;node_id+=incr)
		    node_list.push_back(node_id);
		}
	      else
		for (unsigned int idx=0;idx<tokens.size();idx++)
		  if(isalpha(tokens[idx][0]))
		    {
		      tmp_node_range.clear();
		      status = get_set_nodes(parent_set,ABQ_NODE_SET,tokens[idx],tmp_node_range);
		      MB_RETURN_IF_FAIL;
		      
		      node_range.merge(tmp_node_range);
		    }
		  else
		    node_list.push_back(atoi(tokens[idx].c_str()));
	    }
	  
	  next_line_type = get_next_line_type();
	}
     
      tmp_node_range.clear();

      status = get_nodes_by_id(node_container_set,node_list,tmp_node_range);
      MB_RETURN_IF_FAIL;

      node_range.merge(tmp_node_range);
      
    }
  
  MBEntityHandle node_set;
  
  status = add_entity_set(parent_set,ABQ_NODE_SET,nset_name,node_set);
  MB_RETURN_IF_FAIL;
  
  status = mdbImpl->add_entities(node_set,node_range);
  MB_RETURN_IF_FAIL;
  
  if (file_set)
    {
      status = mdbImpl->add_entities(file_set,&node_set,1);
      MB_RETURN_IF_FAIL;
    }

  if (assembly_set)
    {
      status = mdbImpl->add_entities(assembly_set,&node_set,1);
      MB_RETURN_IF_FAIL;

      status = mdbImpl->tag_set_data(mAssemblyHandleTag,&node_set,1,&assembly_set);
      MB_RETURN_IF_FAIL;
    }

  return MB_SUCCESS;

}

MBErrorCode ReadABAQUS::read_element_list(MBEntityHandle parent_set)
{
  MBErrorCode status;

  std::vector<std::string> tokens;
  std::map<std::string,std::string> params;
  std::map<std::string,abaqus_element_params> requiredParams;
  requiredParams["TYPE"]         = abq_element_type;

  std::map<std::string,abaqus_element_params> allowableParams;
  allowableParams[ABQ_AMBIGUOUS] = abq_element_ambiguous;
  allowableParams["ELSET"]       = abq_element_elset;

  abaqus_element_params param;

  std::map<std::string,abaqus_element_type> elementTypes;
  std::map<abaqus_element_type,unsigned int> nodes_per_element;
  std::map<abaqus_element_type,MBEntityType> entityTypeMap;
  elementTypes["DC3D8"]                 = abq_eletype_dc3d8;
  nodes_per_element[abq_eletype_dc3d8]  = 8;
  entityTypeMap[abq_eletype_dc3d8]      = MBHEX;

  elementTypes["DCC3D8"]                = abq_eletype_dcc3d8;
  nodes_per_element[abq_eletype_dcc3d8] = 8;
  entityTypeMap[abq_eletype_dcc3d8]     = MBHEX;

  elementTypes["C3D4"]                  = abq_eletype_c3d4;
  nodes_per_element[abq_eletype_c3d4]   = 4;
  entityTypeMap[abq_eletype_c3d4]       = MBTET;
  
  elementTypes["C3D8R"]                 = abq_eletype_c3d8r;
  nodes_per_element[abq_eletype_c3d8r]  = 8;
  entityTypeMap[abq_eletype_c3d8r]      = MBHEX;
  
  elementTypes["DS4"]                   = abq_eletype_ds4;
  nodes_per_element[abq_eletype_ds4]    = 4;
  entityTypeMap[abq_eletype_ds4]        = MBQUAD;
  
  abaqus_element_type element_type;
  
  bool make_element_set = false;
  std::string element_set_name;
  
  // tokenize last line read
  tokenize(readline,tokens,",\n");
  extract_keyword_parameters(tokens,params);
  
  // search for required parameters
  for (std::map<std::string,abaqus_element_params>::iterator thisParam=requiredParams.begin();
       thisParam != requiredParams.end();
       thisParam++)
    {
      std::string param_key = match( (*thisParam).first,params );
      param = requiredParams[param_key];
      switch (param)
	{
	case abq_element_type:
	  element_type = elementTypes[ params[param_key]];
	  if (abq_eletype_unsupported == element_type)
	    {
	      std::cerr << "MOAB doesn't currently support this element type: "
			<< (*thisParam).second << std::endl;
	      return MB_FAILURE;
	    }
	  // std::cout << "\tAdding ELEMENTS of type: " << params[param_key] << std::endl; // REMOVE
	  params.erase(param_key);
	  break;
	case abq_element_undefined:
	  std::cerr << "Missing required ELEMENT parameter " << (*thisParam).first << std::endl
		    << readline << std::endl;
	  return MB_FAILURE;
	default:
	  break;
	}
    }

  // process parameters
  for (std::map<std::string,std::string>::iterator thisParam=params.begin();
       thisParam != params.end();
       thisParam++)
    {
      // look for unambiguous match with this node parameter
      param = allowableParams[match( (*thisParam).first, allowableParams )];
      switch (param)
	{
	case abq_element_elset:
	  make_element_set = true;
	  element_set_name = (*thisParam).second;
	  break;
	case abq_element_ambiguous:
	  // std::cout << "\t\tIgnoring ambiguous ELEMENT parameter: " << (*thisParam).first
	  //	    << "=" << (*thisParam).second << std::endl;
	  break;
	default:
	  // std::cout << "\t\tIgnoring unsupported ELEMENT parameter: " << (*thisParam).first
	  //	    << "=" << (*thisParam).second << std::endl;
	  break;
	}
    }
  
  std::vector<int> connect_list, element_ids;

  next_line_type = get_next_line_type();

  while (next_line_type != abq_eof &&
	 next_line_type != abq_keyword_line)
    {
      if (abq_data_line == next_line_type)
	{
	  tokenize(readline,tokens,", \n");
	  if (tokens.size() < nodes_per_element[element_type]+1)
	    {
	      std::cerr << "Not enough data on node data line:" 
	      		<< std::endl << readline << std::endl;
	      return MB_FAILURE;
	    }
	  element_ids.push_back(atoi(tokens[0].c_str()));
	  for (unsigned int i=1;i<nodes_per_element[element_type]+1;i++)
	    connect_list.push_back(atoi(tokens[i].c_str()));
	}
      next_line_type = get_next_line_type();
    }
  
  int num_elements = element_ids.size();

  // get and fill element arrays
  MBEntityHandle start_element = 0;
  MBEntityHandle *connect;

  status = readMeshIface->get_element_array( num_elements, nodes_per_element[element_type],
					     entityTypeMap[element_type], MB_START_ID,
					     start_element, connect);
  MB_RETURN_IF_FAIL;
  if (0 == start_element) return MB_FAILURE;

  // ASSUME: elements must be defined after nodes!
  // get list of node entity handles and node IDs
  MBRange node_list;
  status = mdbImpl->get_entities_by_dimension(parent_set, 0, node_list);
  MB_RETURN_IF_FAIL;
  
  std::vector<int> node_ids(node_list.size());
  status = mdbImpl->tag_get_data(mLocalIDTag,node_list,&node_ids[0]);
  MB_RETURN_IF_FAIL;

  std::map<int,MBEntityHandle> nodeIdMap;
  for (unsigned int idx=0;idx<node_list.size();idx++)
    nodeIdMap[node_ids[idx]] = node_list[idx];

  for (unsigned int node=0;node<connect_list.size();node++)
    connect[node] = nodeIdMap[connect_list[node]];

  MBRange element_range(start_element, start_element+num_elements-1);

  // add elements to file_set
  // status = mdbImpl->add_entities(file_set,element_range);
  // MB_RETURN_IF_FAIL;

  // add elements to this parent_set
  status = mdbImpl->add_entities(parent_set,element_range);
  MB_RETURN_IF_FAIL;

  // tag elements with their local ID's
  status = mdbImpl->tag_set_data(mLocalIDTag,element_range,&element_ids[0]);
  MB_RETURN_IF_FAIL;

  // these elements don't know their instance_set (probably not defined)

  if (make_element_set)
    {
      MBEntityHandle element_set;

      status = add_entity_set(parent_set,ABQ_ELEMENT_SET,element_set_name,element_set);
      MB_RETURN_IF_FAIL;

      status = mdbImpl->add_entities(element_set,element_range);
      MB_RETURN_IF_FAIL;

      // this ad-hoc element set doesn't know its: 
      // * part_set (probably parent_set)
      // * instance_set (probably not defined)
      // * assembly_set (probably not defined)

    }      
  
  return MB_SUCCESS;
  

}
 
MBErrorCode ReadABAQUS::read_node_list(MBEntityHandle parent_set)
{
  MBErrorCode status;

  std::vector<std::string> tokens;
  std::map<std::string,std::string> params;
  std::map<std::string,abaqus_node_params> allowableParams;
  
  allowableParams[ABQ_AMBIGUOUS] = abq_node_ambiguous;
  allowableParams["NSET"]        = abq_node_nset;
  allowableParams["SYSTEM"]      = abq_node_system;

  abaqus_node_params param;

  bool make_node_set = false;
  std::string node_set_name;

  char coord_system = 'R';

  // tokenize last line read
  tokenize(readline,tokens,",\n");
  extract_keyword_parameters(tokens,params);
  
  // std::cout << "\tAdding NODES"  << std::endl; // REMOVE


  // process parameters
  for (std::map<std::string,std::string>::iterator thisParam=params.begin();
       thisParam != params.end();
       thisParam++)
    {
      // look for unambiguous match with this node parameter
      param = allowableParams[match( (*thisParam).first, allowableParams )];
      switch (param)
	{
	case abq_node_nset:
	  make_node_set = true;
	  node_set_name = (*thisParam).second;
	  break;
	case abq_node_system:
	  // store coordinate system
	  coord_system = (*thisParam).second[0];
	  break;
	case abq_node_ambiguous:
	  // std::cout << "\t\tIgnoring ambiguous NODE parameter: " << (*thisParam).first
	  //	    << "=" << (*thisParam).second << std::endl;
	  break;
	default:
	  // std::cout << "\t\tIgnoring unsupported NODE parameter: " << (*thisParam).first
	  //	    << "=" << (*thisParam).second << std::endl;
	  break;
	}
    }
  
  std::vector<double> coord_list;
  std::vector<int> node_ids;
  
  next_line_type = get_next_line_type();
  
  while (next_line_type != abq_eof &&
	 next_line_type != abq_keyword_line)
    {
      if (abq_data_line == next_line_type)
	{
	  tokenize(readline,tokens,", \n");
	  if (tokens.size() < 4)
	    {
	      std::cerr << "Not enough data on node data line:" 
			<< std::endl << readline << std::endl;
	      return MB_FAILURE;
	    }
	  node_ids.push_back(atoi(tokens[0].c_str()));
	  for (unsigned int i=1;i<4;i++)
	    coord_list.push_back(atof(tokens[i].c_str()));
	}
      next_line_type = get_next_line_type();
    }
  
  unsigned int num_nodes = node_ids.size();
  
  // transform coordinate systems
  switch (coord_system)
    {
    case 'R':
      break;
    case 'C':
      cyl2rect(coord_list);
      break;
    case 'S':
      sph2rect(coord_list);
      break;
    default:
      // std::cout << "Treating undefined coordinate system: " << coord_system 
      //		<< " as rectangular/Cartesian." << std::endl;
      break;
    }

  // get and fill coordinate arrays
  std::vector<double*> coord_arrays(3);
  MBEntityHandle start_node = 0;
  status = readMeshIface->get_node_arrays(3, num_nodes,MB_START_ID,
					  start_node,coord_arrays);
  MB_RETURN_IF_FAIL;

  if (0 == start_node) return MB_FAILURE;

  for (unsigned int idx=0;idx<num_nodes;idx++)
    {
      coord_arrays[0][idx] = coord_list[idx*3];
      coord_arrays[1][idx] = coord_list[idx*3+1];
      coord_arrays[2][idx] = coord_list[idx*3+2];
    }
  
  MBRange node_range(start_node, start_node+num_nodes-1);
  // add nodes to file_set
  // status = mdbImpl->add_entities(file_set,node_range);
  // MB_RETURN_IF_FAIL;

  // add nodes to this parent_set
  status = mdbImpl->add_entities(parent_set,node_range);
  MB_RETURN_IF_FAIL;

  // tag nodes with their local ID's
  status = mdbImpl->tag_set_data(mLocalIDTag,node_range,&node_ids[0]);
  MB_RETURN_IF_FAIL;

  // these nodes don't know their instance_set (probably not defined)

  if (make_node_set)
    {
      MBEntityHandle node_set;
      
      status = add_entity_set(parent_set,ABQ_NODE_SET,node_set_name,node_set);
      MB_RETURN_IF_FAIL;

      status = mdbImpl->add_entities(node_set,node_range);
      MB_RETURN_IF_FAIL;

      // this ad-hoc node set doesn't know its: 
      // * part_set (probably parent_set)
      // * instance_set (probably not defined)
      // * assembly_set (probably not defined)

    }      
  
  return MB_SUCCESS;
}

// SET CREATION & ACCESS UTILITIES

MBErrorCode ReadABAQUS::get_elements_by_id(MBEntityHandle parent_set,
					   std::vector<int> element_ids_subset,
					   MBRange &element_range)
{
  MBErrorCode status;
  MBRange all_elements;

  status = get_set_elements(parent_set,all_elements);
  MB_RETURN_IF_FAIL;

  std::vector<int> element_ids(all_elements.size());
  status = mdbImpl->tag_get_data(mLocalIDTag,all_elements,&element_ids[0]);
  MB_RETURN_IF_FAIL;

  std::map<int,MBEntityHandle> elementIdMap;
  for (unsigned int idx=0;idx<all_elements.size();idx++)
    elementIdMap[element_ids[idx]] = all_elements[idx];

  for (std::vector<int>::iterator element=element_ids_subset.begin();
       element != element_ids_subset.end();
       element++)
    element_range.insert(elementIdMap[*element]);

  return MB_SUCCESS;

}
MBErrorCode ReadABAQUS::get_nodes_by_id(MBEntityHandle parent_set,
					std::vector<int> node_ids_subset,
					MBRange &node_range)
{
  MBErrorCode status;

  MBRange all_nodes;
  status = mdbImpl->get_entities_by_type(parent_set,MBVERTEX,all_nodes);
  MB_RETURN_IF_FAIL;

  std::vector<int> node_ids(all_nodes.size());
  status = mdbImpl->tag_get_data(mLocalIDTag,all_nodes,&node_ids[0]);
  MB_RETURN_IF_FAIL;
  
  std::map<int,MBEntityHandle> nodeIdMap;
  for (unsigned int idx=0;idx<all_nodes.size();idx++)
    nodeIdMap[node_ids[idx]] = all_nodes[idx];

  for (std::vector<int>::iterator node=node_ids_subset.begin();
       node != node_ids_subset.end();
       node++)
    {
      node_range.insert(nodeIdMap[*node]);
    }

  return MB_SUCCESS;

}


MBErrorCode ReadABAQUS::get_set_by_name(MBEntityHandle parent_set,
					int ABQ_set_type,
					std::string set_name,
					MBEntityHandle &set_handle)
{
  MBErrorCode status;
  
  char this_set_name[ABAQUS_SET_NAME_LENGTH];
  
  set_handle = 0;

  MBRange sets;
  void* tag_data[] = {&ABQ_set_type};
  status = mdbImpl->get_entities_by_type_and_tag(parent_set,
						 MBENTITYSET,
						 &mSetTypeTag,
						 tag_data, 1, sets);
  if (MB_SUCCESS != status)
    {
      std::cerr << "Did not find any sets of that type" << std::endl;
      return status;
    }
  
  for (MBRange::iterator this_set=sets.begin();
       this_set != sets.end() && 0 == set_handle;
       this_set++)
    {
      std::fill(this_set_name,this_set_name+ABAQUS_SET_NAME_LENGTH,'\0');
      status = mdbImpl->tag_get_data(mSetNameTag, &(*this_set), 1, &this_set_name[0]);
      if (MB_SUCCESS != status && MB_TAG_NOT_FOUND != status) return status;
      
      if (set_name == std::string(this_set_name))
	set_handle = *this_set;
    }
  
  if (0 == set_handle)
    {
      std::cerr << "Did not find requested set: " << set_name 
		<< std::endl;
      return MB_FAILURE;
    }

  return MB_SUCCESS;
}

MBErrorCode ReadABAQUS::get_set_elements(MBEntityHandle set_handle,
					 MBRange &element_range)
{
  MBErrorCode status;
  
  MBRange dim_ent_list;

  // could have elements of multiple dimensions in this set???
  for (int dim=1;dim<=3;dim++)
    {
      dim_ent_list.clear();
      status = mdbImpl->get_entities_by_dimension(set_handle,dim,dim_ent_list);
      MB_RETURN_IF_FAIL;
      
      element_range.merge(dim_ent_list);
    }

  return MB_SUCCESS;
}

MBErrorCode ReadABAQUS::get_set_elements_by_name(MBEntityHandle parent_set,
						 int ABQ_set_type,
						 std::string set_name,
						 MBRange &element_range)
{
  MBErrorCode status;
  
  MBEntityHandle set_handle;
  status = get_set_by_name(parent_set,ABQ_set_type,set_name,set_handle);
  MB_RETURN_IF_FAIL;
  
  status = get_set_elements(set_handle,element_range);
  MB_RETURN_IF_FAIL;

  if (element_range.size() == 0)
    {
      //std::cout << "No elements were found in set " << set_name << std::endl;
    }

  return MB_SUCCESS;

}
  

MBErrorCode ReadABAQUS::get_set_nodes(MBEntityHandle parent_set,
				      int ABQ_set_type,
				      std::string set_name,
				      MBRange &node_range)
{
  MBErrorCode status;
  
  MBEntityHandle set_handle;
  status = get_set_by_name(parent_set,ABQ_set_type,set_name,set_handle);
  MB_RETURN_IF_FAIL;

  MBRange ent_list;
  MBRange dim_ent_list;
  // could have elements of multiple dimensions in this set???
  for (int dim=0;dim<=3;dim++)
    {
      dim_ent_list.clear();
      status = mdbImpl->get_entities_by_dimension(set_handle,dim,dim_ent_list);
      MB_RETURN_IF_FAIL;
      
      ent_list.merge(dim_ent_list);
    }
  
  status = mdbImpl->get_adjacencies(ent_list,0,false,node_range);
  MB_RETURN_IF_FAIL;
  
  if (node_range.size() == 0)
    {
      std::cout << "No nodes were found in set " << set_name << std::endl;
    }

  return MB_SUCCESS;
}
  

MBTag ReadABAQUS::get_tag(const char* tag_name, 
			  int tag_size,
			  MBTagType tag_type,
			  MBDataType tag_data_type)
{
  int def_val = 0;
  
  return get_tag(tag_name,tag_size,tag_type,tag_data_type,&def_val);

}

MBTag ReadABAQUS::get_tag(const char* tag_name, 
			  int tag_size,
			  MBTagType tag_type,
			  MBDataType tag_data_type,
			  const void* def_val)
{
  MBTag retval;

  if (MB_TAG_NOT_FOUND == mdbImpl->tag_get_handle(tag_name, retval) )
    mdbImpl->tag_create(tag_name,tag_size,tag_type,tag_data_type,retval,def_val);

  return retval;
  
  
}

MBErrorCode ReadABAQUS::create_instance_of_part(const MBEntityHandle file_set,
						const MBEntityHandle assembly_set,
						const std::string part_name,
						const std::string instance_name,
						MBEntityHandle &instance_set,
						const std::vector<double> &translation,
						const std::vector<double> &rotation)
{
  MBErrorCode status;

  MBEntityHandle part_set;
  status = get_set_by_name(file_set,ABQ_PART_SET,part_name,part_set);
  MB_RETURN_IF_FAIL;

  status = add_entity_set(assembly_set,ABQ_INSTANCE_SET,instance_name,instance_set);
  MB_RETURN_IF_FAIL;

  // cross-reference
  status = mdbImpl->tag_set_data(mPartHandleTag,&instance_set,1,&part_set);
  MB_RETURN_IF_FAIL;

  int instance_id = ++num_part_instances[part_set];
  status = mdbImpl->tag_set_data(mInstancePIDTag,&instance_set,1,&instance_id);
  MB_RETURN_IF_FAIL;

  status = mdbImpl->tag_set_data(mAssemblyHandleTag,&instance_set,1,&assembly_set);
  MB_RETURN_IF_FAIL;

  instance_id = ++num_assembly_instances[assembly_set];
  status = mdbImpl->tag_set_data(mInstanceGIDTag,&instance_set,1,&instance_id);

  // ---- NODES ---- 

  // get all nodes and IDs
  MBRange part_node_list;
  status = mdbImpl->get_entities_by_dimension(part_set,0,part_node_list);
  MB_RETURN_IF_FAIL;

  std::vector<int> node_ids(part_node_list.size());
  status = mdbImpl->tag_get_data(mLocalIDTag,part_node_list,&node_ids[0]);
  MB_RETURN_IF_FAIL;

  std::map<int,MBEntityHandle> nodeIdMap;
  for (unsigned int idx=0;idx<part_node_list.size();idx++)
    nodeIdMap[node_ids[idx]] = part_node_list[idx];

  // create new nodes
  std::vector<double*> coord_arrays(3);
  MBEntityHandle start_node = 0;
  status = readMeshIface->get_node_arrays(3, part_node_list.size(),MB_START_ID,
					  start_node,coord_arrays);
  MB_RETURN_IF_FAIL;

  if (0 == start_node) return MB_FAILURE;

  // copy coordinates into new coord_arrays
  status = mdbImpl->get_coords(part_node_list,coord_arrays[0],coord_arrays[1],coord_arrays[2]);

  // rotate to new position
  double rot_axis[3];
  rot_axis[0] = rotation[3]-rotation[0];
  rot_axis[1] = rotation[4]-rotation[1];
  rot_axis[2] = rotation[5]-rotation[2];
  
  MBAffineXform rotationXform;
  if (rotation[6] != 0)
    rotationXform = MBAffineXform::rotation(rotation[6]*DEG2RAD,rot_axis);

  // translate to new position
  for (unsigned int idx=0;idx<part_node_list.size();idx++)
    {
      double coords[3];

      // transform to new location and then shift origin of rotation
      for (unsigned int dim=0;dim<3;dim++)
	coords[dim] = coord_arrays[dim][idx] + translation[dim] - rotation[dim];

      // rotate around this origin
      if (rotation[6] != 0)
	rotationXform.xform_vector(coords);

      // transform irigin of rotation back
      for (unsigned int dim=0;dim<3;dim++)
	coord_arrays[dim][idx] = coords[dim] + rotation[dim];

    }

  MBRange instance_node_list(start_node, start_node+part_node_list.size()-1);

  // (DO NOT) add nodes to file_set
  // status = mdbImpl->add_entities(file_set,instance_node_list);
  // MB_RETURN_IF_FAIL;

  // add nodes to this instance_set
  status = mdbImpl->add_entities(instance_set,instance_node_list);
  MB_RETURN_IF_FAIL;

  // add nodes to this assembly_set
  status = mdbImpl->add_entities(assembly_set,instance_node_list);
  MB_RETURN_IF_FAIL;

  // tag nodes with their local ID's
  status = mdbImpl->tag_set_data(mLocalIDTag,instance_node_list,&node_ids[0]);
  MB_RETURN_IF_FAIL;

  // tag nodes with their instance handle
  std::vector<MBEntityHandle> tmp_instance_handles;
  tmp_instance_handles.assign(part_node_list.size(),instance_set);
  status = mdbImpl->tag_set_data(mInstanceHandleTag,instance_node_list,&tmp_instance_handles[0]);
  MB_RETURN_IF_FAIL;

  // create a map of old handles to new handles!!!
  std::map<MBEntityHandle,MBEntityHandle> p2i_nodes;
  for (unsigned int idx=0;idx<part_node_list.size();idx++)
    p2i_nodes[part_node_list[idx]]=instance_node_list[idx];
  
  //  ---- ELEMENTS ----
  
  MBRange part_element_list;
  status = get_set_elements(part_set,part_element_list);
  MB_RETURN_IF_FAIL;

  std::vector<int> part_element_ids(part_element_list.size());
  status = mdbImpl->tag_get_data(mLocalIDTag,part_element_list,&part_element_ids[0]);
  MB_RETURN_IF_FAIL;

  std::map<int,MBEntityHandle> elementIdMap;
  for (unsigned int idx=0;idx<part_element_list.size();idx++)
      elementIdMap[part_element_ids[idx]] = part_element_list[idx];

  // create new elements
  MBRange instance_element_list;
  instance_element_list.clear();

  // cross-referencing storage and pointers/iterators
  std::map<MBEntityHandle,MBEntityHandle> p2i_elements;
  std::vector<int> instance_element_ids;
  std::vector<int>::iterator part_element_id = part_element_ids.begin();

  for (MBRange::iterator part_element=part_element_list.begin();
       part_element != part_element_list.end();
       part_element++,part_element_id++)
    {
      MBEntityType element_type = mdbImpl->type_from_handle(*part_element);
      std::vector<MBEntityHandle> part_connectivity,instance_connectivity;
      MBEntityHandle new_element;
      status = mdbImpl->get_connectivity(&(*part_element),1,part_connectivity);
      MB_RETURN_IF_FAIL;

      instance_connectivity.clear();
      for (std::vector<MBEntityHandle>::iterator connectivity_node=part_connectivity.begin();
	   connectivity_node != part_connectivity.end();
	   connectivity_node++)
	instance_connectivity.push_back(p2i_nodes[*connectivity_node]);
      
      status = mdbImpl->create_element(element_type,&instance_connectivity[0],instance_connectivity.size(),new_element);
      MB_RETURN_IF_FAIL;

      instance_element_list.insert(new_element);
      p2i_elements[*part_element] = new_element;
      instance_element_ids.push_back(*part_element_id);
    }

  // (DO NOT) add elements to file_set
  // status = mdbImpl->add_entities(file_set,instance_element_list);
  // MB_RETURN_IF_FAIL;

  // add elements to this instance_set
  status = mdbImpl->add_entities(instance_set,instance_element_list);
  MB_RETURN_IF_FAIL;

  // add elements to this assembly_set
  status = mdbImpl->add_entities(assembly_set,instance_element_list);
  MB_RETURN_IF_FAIL;

  // tag elements with their local ID's
  status = mdbImpl->tag_set_data(mLocalIDTag,instance_element_list,&(instance_element_ids[0]));
  MB_RETURN_IF_FAIL;

  // tag elements with their instance handle
  tmp_instance_handles.assign(part_element_list.size(),instance_set);
  status = mdbImpl->tag_set_data(mInstanceHandleTag,instance_element_list,&(tmp_instance_handles[0]));
  MB_RETURN_IF_FAIL;
  
  // ----- NODE SETS -----

  // get all node sets in part
  MBRange part_node_sets;
  int tag_val = ABQ_NODE_SET;
  void* tag_data[] = {&tag_val};
  status = mdbImpl->get_entities_by_type_and_tag(part_set,
						 MBENTITYSET,
						 &mSetTypeTag,
						 tag_data, 1, part_node_sets);
  MB_RETURN_IF_FAIL;

  MBRange part_node_set_list, instance_node_set_list;
  for (MBRange::iterator part_node_set = part_node_sets.begin();
       part_node_set != part_node_sets.end();
       part_node_set++)
    {
      char node_set_name[ABAQUS_SET_NAME_LENGTH];
      std::fill(node_set_name,node_set_name+ABAQUS_SET_NAME_LENGTH,'\0');
      status = mdbImpl->tag_get_data(mSetNameTag, &(*part_node_set), 1, &node_set_name[0]);
      if (MB_SUCCESS != status && MB_TAG_NOT_FOUND != status) return status;
      
      part_node_set_list.clear();
      status = mdbImpl->get_entities_by_dimension(*part_node_set,0,part_node_set_list);

      instance_node_set_list.clear();
      for (MBRange::iterator set_node = part_node_set_list.begin();
	   set_node != part_node_set_list.end();
	   set_node++)
	instance_node_set_list.insert(p2i_nodes[*set_node]);

      MBEntityHandle instance_node_set;
      
      status = add_entity_set(instance_set,ABQ_NODE_SET,node_set_name,instance_node_set);
      MB_RETURN_IF_FAIL;
      
      status = mdbImpl->add_entities(instance_node_set,instance_node_set_list);
      MB_RETURN_IF_FAIL;

      status = mdbImpl->add_entities(assembly_set,&instance_node_set,1);
      MB_RETURN_IF_FAIL;
      
      status = mdbImpl->tag_set_data(mPartHandleTag,&instance_node_set,1,&part_set);
      MB_RETURN_IF_FAIL;

      status = mdbImpl->tag_set_data(mInstanceHandleTag,&instance_node_set,1,&instance_set);
      MB_RETURN_IF_FAIL;

      status = mdbImpl->tag_set_data(mAssemblyHandleTag,&instance_node_set,1,&assembly_set);
      MB_RETURN_IF_FAIL;

    }

  // ----- ELEMENT SETS -----

  // get all element sets in part
  MBRange part_element_sets;
  tag_val = ABQ_ELEMENT_SET;
  tag_data[0] = &tag_val;
  status = mdbImpl->get_entities_by_type_and_tag(part_set,
						 MBENTITYSET,
						 &mSetTypeTag,
						 tag_data, 1, part_element_sets);
  MB_RETURN_IF_FAIL;

  MBRange part_element_set_list, instance_element_set_list;
  for (MBRange::iterator part_element_set = part_element_sets.begin();
       part_element_set != part_element_sets.end();
       part_element_set++)
    {
      char element_set_name[ABAQUS_SET_NAME_LENGTH];
      std::fill(element_set_name,element_set_name+ABAQUS_SET_NAME_LENGTH,'\0');
      status = mdbImpl->tag_get_data(mSetNameTag, &(*part_element_set), 1, &element_set_name[0]);
      if (MB_SUCCESS != status && MB_TAG_NOT_FOUND != status) return status;
      
      part_element_set_list.clear();
      status = get_set_elements(*part_element_set,part_element_set_list);

      instance_element_set_list.clear();
      for (MBRange::iterator set_element = part_element_set_list.begin();
	   set_element != part_element_set_list.end();
	   set_element++)
	instance_element_set_list.insert(p2i_elements[*set_element]);

      MBEntityHandle instance_element_set;
      status = add_entity_set(instance_set,ABQ_ELEMENT_SET,element_set_name,instance_element_set);
      MB_RETURN_IF_FAIL;

      std::cerr << instance_set << "\t" << instance_element_set << std::endl;
      status = mdbImpl->add_entities(instance_element_set,instance_element_set_list);
      MB_RETURN_IF_FAIL;

      status = mdbImpl->add_entities(assembly_set,&instance_element_set,1);
      MB_RETURN_IF_FAIL;

      // status = mdbImpl->add_entities(file_set,&instance_element_set,1);
      //MB_RETURN_IF_FAIL;

      status = mdbImpl->tag_set_data(mPartHandleTag,&instance_element_set,1,&part_set);
      MB_RETURN_IF_FAIL;

      status = mdbImpl->tag_set_data(mInstanceHandleTag,&instance_element_set,1,&instance_set);
      MB_RETURN_IF_FAIL;

      status = mdbImpl->tag_set_data(mAssemblyHandleTag,&instance_element_set,1,&assembly_set);
      MB_RETURN_IF_FAIL;

      char element_set_matname[ABAQUS_SET_NAME_LENGTH];
      std::fill(element_set_matname,element_set_matname+ABAQUS_SET_NAME_LENGTH,'\0');
      status = mdbImpl->tag_get_data(mMatNameTag, &(*part_element_set), 1, &element_set_matname[0]);
      if (MB_SUCCESS != status && MB_TAG_NOT_FOUND != status) return status;

      if (MB_TAG_NOT_FOUND != status)
	{
	  status = mdbImpl->tag_set_data(mMatNameTag,&instance_element_set,1,element_set_matname);
	  MB_RETURN_IF_FAIL;
	}

      int element_set_mat_id;
      status = mdbImpl->tag_get_data(mMaterialSetTag,&(*part_element_set), 1, &element_set_mat_id);
      if (MB_SUCCESS != status && MB_TAG_NOT_FOUND != status) return status;

      if (MB_TAG_NOT_FOUND != status)
	{
	  status = mdbImpl->tag_set_data(mMaterialSetTag,&instance_element_set,1,&element_set_mat_id);
	  MB_RETURN_IF_FAIL;
	}
	  


    }

  

  return MB_SUCCESS;
}


MBErrorCode ReadABAQUS::add_entity_set(MBEntityHandle parent_set,
				       int ABQ_Set_Type,
				       std::string set_name,
				       MBEntityHandle &entity_set)
{
  MBErrorCode status;
  
  status = mdbImpl->create_meshset(MESHSET_SET, entity_set);
  MB_RETURN_IF_FAIL;
  
  status = mdbImpl->tag_set_data(mSetTypeTag,&entity_set,1,&ABQ_Set_Type);
  MB_RETURN_IF_FAIL;
  
  status = mdbImpl->tag_set_data(mSetNameTag,&entity_set,1,set_name.c_str());
  MB_RETURN_IF_FAIL;
  
  status = mdbImpl->add_entities(parent_set,&entity_set,1);
  MB_RETURN_IF_FAIL;

  return MB_SUCCESS;
}
  
					   


void ReadABAQUS::cyl2rect(std::vector<double> coord_list)
{
  int num_nodes = coord_list.size()/3;
  double x,y,r,t;

  for (int node=0;node<num_nodes;node++)
    {
      r = coord_list[3*node];
      t = coord_list[3*node+1]*DEG2RAD;

      x = r*cos(t);
      y = r*sin(t);
      
      coord_list[3*node] = x;
      coord_list[3*node+1] = y;
    }
}

void ReadABAQUS::sph2rect(std::vector<double> coord_list)
{
  int num_nodes = coord_list.size()/3;
  double x,y,z,r,t,p;

  for (int node=0;node<num_nodes;node++)
    {
      r = coord_list[3*node];
      t = coord_list[3*node+1]*DEG2RAD;
      p = coord_list[3*node+2]*DEG2RAD;

      x = r*cos(p)*cos(t);
      y = r*cos(p)*sin(t);
      z = r*sin(p);
      
      coord_list[3*node] = x;
      coord_list[3*node+1] = y;
      coord_list[3*node+2] = z;
    }
}


// PARSING RECOGNITION

abaqus_line_types ReadABAQUS::get_next_line_type()
{
  
  readline.clear();
  std::getline(abFile,readline);

  if (abFile.eof())
    return abq_eof;

  std::string::size_type pos = readline.find_first_not_of(' ');
  
  if (std::string::npos == pos)
    return abq_blank_line;
  
  if ('*' == readline[pos])
    if ('*' == readline[pos+1])
      return abq_comment_line;
    else
      return abq_keyword_line;
  else
    return abq_data_line;
}


abaqus_keyword_type ReadABAQUS::get_keyword()
{
  
  std::vector<std::string> tokens;
  std::map<std::string,abaqus_keyword_type> keywords;

  // set up list of supported keywords
  // Note: any attempt to match something not in the keyword list 
  //       using the [] operator will create a new entry in the map
  //       but that entry will have value abq_undefined based on the
  //       definition of the abaqus_keyword_type enum.
  keywords[ABQ_AMBIGUOUS]   = abq_ambiguous;
  keywords["HEADING"]       = abq_heading;
  keywords["PART"]          = abq_part;
  keywords["END PART"]      = abq_end_part;
  keywords["ASSEMBLY"]      = abq_assembly;
  keywords["END ASSEMBLY"]  = abq_end_assembly;
  keywords["NODE"]          = abq_node;
  keywords["ELEMENT"]       = abq_element;
  keywords["NSET"]          = abq_nset;
  keywords["ELSET"]         = abq_elset;
  keywords["SOLID SECTION"] = abq_solid_section;
  keywords["INSTANCE"]      = abq_instance;
  keywords["END INSTANCE"]  = abq_end_instance;
  
  tokenize(readline,tokens,"*,\n");
  
  // convert to upper case and test for unambiguous match/partial match
  stringToUpper(tokens[0],tokens[0]);
  return keywords[match(tokens[0],keywords)];
  
}



// PARSING UTILITY FUNCTIONS

// for a map of strings to values of type T
// search the key list of the map for an unambiguous partial match with the token
template <typename T>
std::string ReadABAQUS::match(const std::string &token, 
			      std::map<std::string,T> &tokenList)
{
  // initialize with no match and ABQ_UNDEFINED as return string
  bool found_match = false;
  std::string best_match = ABQ_UNDEFINED;

  // search the map
  for (typename std::map<std::string,T>::iterator thisToken=tokenList.begin();
       thisToken != tokenList.end();
       thisToken++)
    {
      // if a perfect match break the loop (assume keyword list is unambiguous)
      if (token == (*thisToken).first)
	{
	  best_match = token;
	  break;
	}
      else
	{
	  int short_length = ( token.length()<(*thisToken).first.length()?token.length():(*thisToken).first.length() );
	  // if the token matches the first token.length() characters of the keyword
	  // consider this a match
	  if ( token.substr(short_length) == (*thisToken).first.substr(short_length) )
	    {
	      if (!found_match)
		{
		  // if no match already, record match and matching keyword
		  found_match = true;
		  best_match = (*thisToken).first;
		}
	      else
		// if match already set matching keyword to ABQ_AMBIGUOUS
		best_match = ABQ_AMBIGUOUS;
	    }
	}
    }

  // Possible return values: ABQ_UNDEFINED, keyword from list, ABQ_AMBIGUOUS
  return best_match;
}

// convert a string to upper case
void ReadABAQUS::stringToUpper(std::string toBeConverted, std::string& converted)
{
  converted = toBeConverted;

  for (unsigned int i=0;i<toBeConverted.length();i++)
    converted[i] = toupper(toBeConverted[i]);

}

// extract key/value pairs from parameter list
void ReadABAQUS::extract_keyword_parameters(std::vector<std::string> tokens,
					 std::map<std::string,std::string>& params)
{

  std::string key, value;

  
  // NOTE: skip first token - it is the keyword
  for (std::vector<std::string>::iterator token=tokens.begin()+1;
       token!=tokens.end(); token++)
    {
      std::string::size_type pos = token->find('=');
      stringToUpper(token->substr(0,pos),key);
      if(std::string::npos != pos)
	value = token->substr(pos+1);
      else
	value = "";
      pos = key.find_first_not_of(' ',0);
      key = key.substr(pos);
      params[key] = value;
    }
}

// tokenize a string based on a set of possible delimiters
void ReadABAQUS::tokenize( const std::string& str,
                         std::vector<std::string>& tokens,
                         const char* delimiters )
{
  tokens.clear();

  std::string::size_type pos, last = str.find_first_not_of( delimiters, 0 );

  while ( std::string::npos != last )
    {
      pos = str.find_first_of( delimiters, last );
      if ( std::string::npos == pos )
	{
	  tokens.push_back(str.substr(last));
	  last = std::string::npos;
	}
      else
	{
	  tokens.push_back( str.substr( last, pos - last ) );
	  last = str.find_first_not_of( delimiters, pos );
	}
    }
}

