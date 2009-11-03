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

//-------------------------------------------------------------------------
// Filename      : ReadABAQUS.hpp
//
// Purpose       : ABAQUS inp file reader
//
// Special Notes : Started with NetCDF EXODUS II reader
//
// Creator       : Paul Wilson & Patrick Snouffer
//
// Date          : 08/2009
//
// Owner         : Paul Wilson
//-------------------------------------------------------------------------

#ifndef READABAQUS_HPP
#define READABAQUS_HPP

#ifndef IS_BUILDING_MB
  #error "ReadABAQUS.hpp isn't supposed to be included into an application"
#endif


#include <vector>
#include <map>
#include <string>
#include <iostream>
#include <fstream>

#include "MBForward.hpp"
#include "MBReaderIface.hpp"
#include "MBRange.hpp"

#define ABAQUS_SET_TYPE_TAG_NAME "abaqus_set_type"
#define ABAQUS_SET_NAME_TAG_NAME "abaqus_set_name"
#define ABAQUS_SET_NAME_LENGTH   100
#define ABAQUS_LOCAL_ID_TAG_NAME "abaqus_local_id"

// many sets should know who contains them
#define ABAQUS_INSTANCE_HANDLE_TAG_NAME   "abaqus_instance_handle"
#define ABAQUS_ASSEMBLY_HANDLE_TAG_NAME "abaqus_assembly_handle"
#define ABAQUS_PART_HANDLE_TAG_NAME       "abaqus_part_handle"

// instances should know things about themselves:
//  * which part they derive from (see ABAQUS_PART_HANDLE_TAG_NAME above)
//  * which instance of a part this is
//  * which instance of an assembly this is
#define ABAQUS_INSTANCE_PART_ID_TAG_NAME   "abaqus_instance_part_id"
#define ABAQUS_INSTANCE_GLOBAL_ID_TAG_NAME "abaqus_instance_global_id"

// element sets have material name
// using MOAB's general MATERIAL_SET to store material id
#define ABAQUS_MAT_NAME_TAG_NAME "abaqus_mat_name"
#define ABAQUS_MAT_NAME_LENGTH   100

#define ABQ_ASSEMBLY_SET 1
#define ABQ_PART_SET     2
#define ABQ_INSTANCE_SET 3
#define ABQ_NODE_SET     4
#define ABQ_ELEMENT_SET  5


enum abaqus_line_types { abq_undefined_line = 0,
			 abq_blank_line, 
			 abq_comment_line, 
			 abq_keyword_line, 
			 abq_data_line, 
			 abq_eof };

enum abaqus_keyword_type { abq_undefined = 0,
			   abq_unsupported,
			   abq_ambiguous,
			   abq_heading,
			   abq_part,
			   abq_end_part,
			   abq_assembly,
                    	   abq_end_assembly,
			   abq_node,
			   abq_element,
			   abq_nset,
			   abq_elset,
			   abq_instance,
			   abq_end_instance,
			   abq_solid_section};

enum abaqus_part_params { abq_part_undefined = 0,
			  abq_part_ambiguous,
			  abq_part_name};

enum abaqus_instance_params { abq_instance_undefined = 0,
			      abq_instance_ambiguous,
			      abq_instance_name,
			      abq_instance_part};

enum abaqus_assembly_params { abq_assembly_undefined = 0,
			      abq_assembly_ambiguous,
			      abq_assembly_name};

enum abaqus_node_params { abq_node_undefined = 0,
			  abq_node_ambiguous,
			  abq_node_nset,
			  abq_node_system};

enum abaqus_element_params { abq_element_undefined = 0,
			     abq_element_ambiguous,
			     abq_element_elset,
			     abq_element_type};

enum abaqus_element_type { abq_eletype_unsupported = 0,
			   abq_eletype_dc3d8,
			   abq_eletype_c3d8r,
			   abq_eletype_dcc3d8, 
			   abq_eletype_c3d4,
                           abq_eletype_ds4};

enum abaqus_nset_params { abq_nset_undefined = 0,
			  abq_nset_ambiguous,
			  abq_nset_nset,
			  abq_nset_elset,
			  abq_nset_generate,
                          abq_nset_instance};			

enum abaqus_elset_params { abq_elset_undefined = 0,
			   abq_elset_ambiguous,
			   abq_elset_elset,
			   abq_elset_generate,
                           abq_elset_instance};

enum abaqus_solid_section_params { abq_solid_section_undefined = 0,
				   abq_solid_section_ambiguous,
				   abq_solid_section_elset,
				   abq_solid_section_matname};




class MBReadUtilIface;

class ReadABAQUS : public MBReaderIface
{
public:
  
  static MBReaderIface* factory( MBInterface* );
  
  void tokenize( const std::string& str,
                 std::vector<std::string>& tokens,
                 const char* delimiters );
  
  //! load an ABAQUS file
  MBErrorCode load_file( const char *exodus_file_name,
                         MBEntityHandle file_set,
                         const FileOptions& opts,
                         const MBReaderIface::IDTag* subset_list = 0,
                         int subset_list_length = 0,
                         const MBTag* file_id_tag = 0 );
  
  MBErrorCode read_tag_values( const char* file_name,
                               const char* tag_name,
                               const FileOptions& opts,
                               std::vector<int>& tag_values_out,
                               const IDTag* subset_list = 0,
                               int subset_list_length = 0 );
  
  //! Constructor
  ReadABAQUS(MBInterface* impl = NULL);
  
  //! Destructor
  virtual ~ReadABAQUS();
  
private:
  
  void reset();
  
  MBErrorCode read_heading(MBEntityHandle file_set);
  MBErrorCode read_part(MBEntityHandle file_set);
  MBErrorCode read_assembly(MBEntityHandle file_set);
  MBErrorCode read_unsupported(MBEntityHandle file_set);
  MBErrorCode read_node_list(MBEntityHandle parent_set);
  MBErrorCode read_element_list(MBEntityHandle parent_set);
  MBErrorCode read_node_set(MBEntityHandle parent_set,
			    MBEntityHandle file_set = 0,
			    MBEntityHandle assembly_set = 0);
  MBErrorCode read_element_set(MBEntityHandle parent_set,
			    MBEntityHandle file_set = 0,
			    MBEntityHandle assembly_set = 0);
  MBErrorCode read_solid_section(MBEntityHandle parent_set);
  MBErrorCode read_instance(MBEntityHandle assembly_set,
			    MBEntityHandle file_set);
  

  MBErrorCode get_elements_by_id(MBEntityHandle parent_set,
				 std::vector<int> element_ids_subset,
				 MBRange &element_range);
  
  MBErrorCode get_nodes_by_id(MBEntityHandle parent_set,
			      std::vector<int> node_ids_subset,
			      MBRange &node_range);
    
  MBErrorCode get_set_by_name(MBEntityHandle parent_set,
			      int ABQ_set_type,
			      std::string set_name,
			      MBEntityHandle &set_handle);

  MBErrorCode get_set_elements(MBEntityHandle set_handle,
			       MBRange &element_range);

  MBErrorCode get_set_elements_by_name(MBEntityHandle parent_set,
				       int ABQ_set_type,
				       std::string set_name,
				       MBRange &element_range);
    
			      
  MBErrorCode get_set_nodes(MBEntityHandle parent_set,
			    int ABQ_set_type,
			    std::string set_name,
			    MBRange &node_range);
  
  MBErrorCode add_entity_set(MBEntityHandle parent_set,
			     int ABQ_set_type,
			     std::string set_name,
			     MBEntityHandle &entity_set);

  MBErrorCode create_instance_of_part(const MBEntityHandle file_set,
				      const MBEntityHandle parent_set,
				      const std::string part_name,
				      const std::string instance_name,
				      MBEntityHandle &entity_set,
				      const std::vector<double> &translation,
				      const std::vector<double> &rotation);

  MBTag get_tag(const char* tag_name, int tag_size, MBTagType tag_type,
                MBDataType tag_data_type);
  MBTag get_tag(const char* tag_name, int tag_size, MBTagType tag_type,
                MBDataType tag_data_type, const void* def_val);
  
  void cyl2rect(std::vector<double> coord_list);

  void sph2rect(std::vector<double> coord_list);

  abaqus_line_types get_next_line_type();
  abaqus_keyword_type get_keyword();
    
  template <class T>
  std::string match(const std::string &token, 
		    std::map<std::string,T> &tokenList);

  void stringToUpper(std::string toBeConverted,std::string& converted);

  
  void extract_keyword_parameters(std::vector<std::string> tokens,
				  std::map<std::string,std::string>& params);

  //! interface instance
  MBInterface* mdbImpl;

  // read mesh interface
  MBReadUtilIface* readMeshIface;

  std::ifstream abFile;        // abaqus file

  std::string readline;

  //! Cached tags for reading.  Note that all these tags are defined when the
  //! core is initialized.
  MBTag mMaterialSetTag;
  MBTag mDirichletSetTag;
  MBTag mNeumannSetTag;
  MBTag mHasMidNodesTag;

  MBTag mSetTypeTag;
  MBTag mPartHandleTag;
  MBTag mInstancePIDTag;
  MBTag mInstanceGIDTag;

  MBTag mLocalIDTag;
  MBTag mInstanceHandleTag;
  MBTag mAssemblyHandleTag;

  MBTag mSetNameTag;
  MBTag mMatNameTag;

  abaqus_line_types  next_line_type;

  std::map<MBEntityHandle,unsigned int> num_part_instances;
  std::map<MBEntityHandle,unsigned int> num_assembly_instances;
  std::map<std::string,unsigned int> matIDmap;
  unsigned mat_id;
  
};


#endif