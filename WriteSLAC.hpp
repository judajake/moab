//-------------------------------------------------------------------------
// Filename      : WriteSLAC.hpp
//
// Purpose       : ExodusII writer
//
// Special Notes : Lots of code taken from verde implementation
//
// Creator       : Corey Ernst 
//
// Date          : 8/02
//
// Owner         : Corey Ernst 
//-------------------------------------------------------------------------

#ifndef WRITESLAC_HPP
#define WRITESLAC_HPP

#ifndef IS_BUILDING_MB
#error "WriteSLAC.hpp isn't supposed to be included into an application"
#endif

#include "netcdf.hh"

#include <set>
#include <map>
#include <vector>
#include <deque>
#include <functional>
#include <string>

#include "MBRange.hpp"
#include "MBInterface.hpp"
#include "MBWriteUtilIface.hpp"
#include "ExoIIInterface.hpp"

class MB_DLL_EXPORT WriteSLAC
{
 
public:

   //! Constructor
   WriteSLAC(MBInterface *impl);

   //! Destructor
  virtual ~WriteSLAC();

    //! writes out a file
  MBErrorCode write_file(const char *file_name,
                          const MBEntityHandle *output_list,
                          const int num_sets);
  
//! struct used to hold data for each block to be output; used by
//! initialize_file to initialize the file header for increased speed
  struct MaterialSetData
  {
    int id;
    int number_elements;
    int number_nodes_per_element;
    int number_attributes;
    ExoIIElementType element_type;
    MBEntityType moab_type;
    MBRange *elements;
  };

//! struct used to hold data for each nodeset to be output; used by
//! initialize_file to initialize the file header for increased speed
  struct DirichletSetData
  {
    int id;
    int number_nodes;
    std::vector< MBEntityHandle > nodes;
    std::vector< double > node_dist_factors;
  
  };

//! struct used to hold data for each sideset to be output; used by
//! initialize_file to initialize the file header for increased speed
  struct NeumannSetData
  {
    int id;
    int number_elements;
    std::vector<MBEntityHandle> elements;
    std::vector<int> side_numbers;
    MBEntityHandle mesh_set_handle;
  };


protected:

    //! number of dimensions in this file
  //int number_dimensions();

    //! open a file for writing
  MBErrorCode open_file(const char *filename);

  //! contains the general information about a mesh
  class MeshInfo
  {
  public:
    unsigned int num_dim;
    unsigned int num_nodes;
    unsigned int num_elements;
    unsigned int num_matsets;
    unsigned int num_int_hexes;
    unsigned int num_int_tets;
    MBRange bdy_hexes, bdy_tets;
    MBRange nodes;

    MeshInfo() 
        : num_dim(0), num_nodes(0), num_elements(0), num_matsets(0), 
          num_int_hexes(0), num_int_tets(0) 
      {}
    
  };
  
private:

    //! interface instance
  MBInterface *mbImpl;
  MBWriteUtilIface* mWriteIface;
  
    //! file name
  std::string fileName;
  NcFile *ncFile;

    //! Meshset Handle for the mesh that is currently being read
  MBEntityHandle mCurrentMeshHandle;

  //! Cached tags for reading.  Note that all these tags are defined when the
  //! core is initialized.
  MBTag mMaterialSetTag;
  MBTag mDirichletSetTag;
  MBTag mNeumannSetTag;
  MBTag mHasMidNodesTag;
  MBTag mGlobalIdTag;
  MBTag mMatSetIdTag;

  MBTag mEntityMark;   //used to say whether an entity will be exported

  MBErrorCode gather_mesh_information(MeshInfo &mesh_info,
                                      std::vector<MaterialSetData> &matset_info,
                                      std::vector<NeumannSetData> &neuset_info,
                                      std::vector<DirichletSetData> &dirset_info,
                                      std::vector<MBEntityHandle> &matsets,
                                      std::vector<MBEntityHandle> &neusets,
                                      std::vector<MBEntityHandle> &dirsets);
  
  MBErrorCode initialize_file(MeshInfo &mesh_info);

  MBErrorCode write_nodes(const int num_nodes, const MBRange& nodes, 
                          const int dimension );

  MBErrorCode write_matsets(MeshInfo &mesh_info, 
                            std::vector<MaterialSetData> &matset_data,
                            std::vector<NeumannSetData> &neuset_data);
  
  MBErrorCode get_valid_sides(MBRange &elems, const int sense,
                              WriteSLAC::NeumannSetData &sideset_data);
  
  void reset_matset(std::vector<MaterialSetData> &matset_info);
  
  MBErrorCode get_neuset_elems(MBEntityHandle neuset, int current_sense,
                               MBRange &forward_elems, MBRange &reverse_elems);
  
  MBErrorCode gather_interior_exterior(MeshInfo &mesh_info,
                                       std::vector<MaterialSetData> &matset_data,
                                       std::vector<NeumannSetData> &neuset_data);
  
};

#endif
