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

#ifndef MOAB_IMPL_GENERAL_HPP
#define MOAB_IMPL_GENERAL_HPP

#include "moab/Interface.hpp"
#include "moab/ReaderIface.hpp"
#include <map>

namespace moab {

class WriteUtil;
class ReadUtil;
class AEntityFactory;
class SequenceManager;
class TagServer;
class Error;
class HomCoord;
class ReaderWriterSet;
class EntitySequence;
class FileOptions;

#ifdef XPCOM_MB

#define MBCORE_CID \
{ 0x7cb5b7a0, 0x7d7, 0x11d3, { 0xba, 0xb2, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74 } }

#define MBCORE_CONTRACTID "@sandia.gov/MB;1"

#endif


class Core : public Interface 
{

public:

  //!constructor
  MB_DLL_EXPORT Core();

  //! depricated constructor -- values are ignored
  MB_DLL_EXPORT Core( int rank, int num_cpu );

  //!destructor
  MB_DLL_EXPORT ~Core();
  
  //! query an MB internal interface
  virtual ErrorCode query_interface(const std::string& iface_name, void** iface);
 
  //! release an MB internal interface 
  virtual ErrorCode release_interface(const std::string& iface_name, void* iface);

#if defined(XPCOM_MB)
  // this macro expands to all the nsISupports interface functions
  NS_DECL_ISUPPORTS
#endif

  virtual int QueryInterface (const MBuuid& uuid, UnknownInterface** iface );

    //! Returns the major.minor version number of the implementation
    /**
       \param iface_name If non-NULL, will be filled in with a string, possibly 
       containing implementation-specific information
    */
  virtual float impl_version(std::string *version_string = NULL);

  //! get the type from a handle, returns type
  virtual EntityType type_from_handle(const EntityHandle handle) const;
  
  //! get the id from a handle, returns id
  virtual EntityID id_from_handle(const EntityHandle handle) const;
  
  //! get a handle from an id and type
  virtual ErrorCode handle_from_id(const EntityType type, 
                                      const EntityID id, 
                                      EntityHandle& handle) const;
  
  virtual int dimension_from_handle( const EntityHandle ) const;

  //! load mesh from data in file
  //! NOTE: if there is mesh already present, the new mesh will be added
  virtual ErrorCode load_mesh(const char *file_name,
                                 const int *active_block_id_list = NULL,
                                 const int num_blocks = 0);

  /**Load or import a file. */
  virtual ErrorCode load_file( const char* file_name,
                                 const EntityHandle* file_set = 0,
                                 const char* options = 0,
                                 const char* set_tag_name = 0,
                                 const int* set_tag_values = 0,
                                 int num_set_tag_values = 0 );

  /**Load or import a file. */
  ErrorCode serial_load_file( const char* file_name,
                         const EntityHandle* file_set,
                         const FileOptions& opts,
                         const ReaderIface::IDTag* subset_list = 0,
                         int subset_list_length = 0,
                         const Tag* file_id_tag = 0 );
                         
  ErrorCode serial_read_tag( const char* file_name,
                               const char* tag_name,
                               const FileOptions& opts,
                               std::vector<int>& tag_vals,
                               const ReaderIface::IDTag* subset_list = 0,
                               int subset_list_length = 0 );
  
  virtual ErrorCode write_mesh(const char *file_name,
                                  const EntityHandle *output_list = NULL,
                                  const int num_sets = 0);
  /** Write or export a file. */
  virtual ErrorCode write_file( const char* file_name,
                                  const char* file_type = 0,
                                  const char* options = 0,
                                  const EntityHandle* output_sets = 0,
                                  int num_output_sets = 0,
                                  const Tag* tag_list = 0,
                                  int num_tags = 0 );

  /** Write or export a file */
  virtual ErrorCode write_file( const char* file_name,
                                  const char* file_type,
                                  const char* options,
                                  const Range& output_sets,
                                  const Tag* tag_list = 0,
                                  int num_tags = 0 );

  //! deletes all mesh entities from this datastore
  virtual ErrorCode delete_mesh();

  //! get overall geometric dimension
  virtual ErrorCode get_dimension(int &dim) const;

  //! set overall geometric dimension
  /** Returns error if setting to 3 dimensions, mesh has been created, and 
   *  there are only 2 dimensions on that mesh
   */
  virtual ErrorCode set_dimension(const int dim);

  //! get blocked vertex coordinates for all vertices
  /** Blocked = all x, then all y, etc. 
   */
  virtual ErrorCode get_vertex_coordinates(std::vector<double> &coords) const;

  //! get the coordinate information for this handle if it is of type Vertex
  //! otherwise, return an error
  virtual ErrorCode  get_coords(const Range &entity_handles, 
                                   double *coords) const;
  
  virtual ErrorCode  get_coords(const EntityHandle *entity_handles, 
                                   const int num_entities, 
                                   double *coords) const;
  
  virtual ErrorCode  get_coords(const EntityHandle entity_handle, 
                                   const double *& x, const double *& y, const double *& z) const;
 
  virtual ErrorCode get_coords( const Range& entity_handles,
                                  double* x_coords,
                                  double* y_coords,
                                  double* z_coords ) const;

  //! set the coordinate information for this handle if it is of type Vertex
  //! otherwise, return an error
  virtual ErrorCode  set_coords( const EntityHandle *entity_handles, 
                                   const int num_entities,
                                   const double *coords);

  //! set the coordinate information for this handle if it is of type Vertex
  //! otherwise, return an error
  virtual ErrorCode  set_coords(Range entity_handles,
                                  const double *coords);

      //! get global connectivity array for specified entity type
      /**  Assumes just vertices, no higher order nodes
       */
    virtual ErrorCode get_connectivity_by_type(const EntityType type, 
                                                  std::vector<EntityHandle> &connect) const;

      //! Gets the connectivity for an element EntityHandle. 
      /** For non-element handles (ie, MeshSets), 
          returns an error. Connectivity data is copied from the database into the vector 
          <em>connectivity</em>. The nodes in <em>connectivity</em> are properly ordered.
          \param entity_handle EntityHandle to get connectivity of.
          \param connectivity Vector in which connectivity of <em>entity_handle</em> is returned.  
          Should contain MeshVertices.
          \param topological_connectivity If true, higher order nodes are ignored. 

          Example: \code 
          std::vector<EntityHandle> conn;
          get_connectivity( entity_handle, conn ); \endcode */
    virtual ErrorCode  get_connectivity(const EntityHandle *entity_handles, 
                                           const int num_handles,
                                           std::vector<EntityHandle> &connectivity, 
                                           bool topological_connectivity = false) const;
 
    //! Gets the connectivity for a vector of elements
    /** Same as vector-based version except range is returned (unordered!)
     */
  virtual ErrorCode  get_connectivity(const EntityHandle *entity_handles, 
                                        const int num_handles,
                                        Range &connectivity, 
                                        bool topological_connectivity = false) const;

    //! Gets the connectivity for elements
    /** Same as vector-based version except range is returned (unordered!)
    */
  virtual ErrorCode get_connectivity( const Range& entity_handles, 
                                        Range &connectivity, 
                                        bool topological_connectivity = false) const;
 
    //! Gets a pointer to constant connectivity data of <em>entity_handle</em> 
      /** Sets <em>number_nodes</em> equal to the number of nodes of the <em> 
          entity_handle </em>.  Faster then the other <em>get_connectivity</em> function. 
          The nodes in 'connectivity' are properly ordered. 
          \param entity_handle EntityHandle to get connectivity of.
          \param connectivity Array in which connectivity of <em>entity_handle</em> is returned.
          Should contain MeshVertex's.
          \param num_nodes Number of MeshVertices in array <em>connectivity</em>. 

          Example: \code 
          const EntityHandle* conn;
          int number_nodes = 0;
          get_connectivity( entity_handle, conn, number_nodes ); \endcode 
          
          Example2: \code
          std::vector<EntityHandle> sm_storage;
          const EntityHandle* conn;
          int number_nodes;
          get_connectivity( handle, conn, number_nodes, false, &sm_storage );
          if (conn == &sm_storage[0])
            std::cout << "Structured mesh element" << std::endl;
          \endcode
        */
    virtual ErrorCode  get_connectivity( const EntityHandle entity_handle, 
                                           const EntityHandle *&connectivity, 
                                           int &num_nodes, 
                                           bool topological_connectivity = false,
                                           std::vector<EntityHandle>* storage = 0
                                          ) const;

      //! Sets the connectivity for an EntityHandle.  For non-element handles, return an error.
      /** Connectivity is stored exactly as it is ordered in vector <em>connectivity</em>. 
          \param entity_handle EntityHandle to set connectivity of.
          \param connect Vector containing new connectivity of <em>entity_handle</em>.
          \param num_connect Number of vertices in <em>connect</em>
   
          Example: \code 
          std::vector<EntityHandle> conn(3);
          conn[0] = node1;
          conn[1] = node2;
          conn[2] = node3;
          set_connectivity( entity_handle, conn, 3 ); \endcode */
    virtual ErrorCode  set_connectivity(const EntityHandle entity_handle, 
                                          EntityHandle *connect,
                                          const int num_connect);

      //! get the adjacencies associated with a set of entities
      /** \param from_entities vector of EntityHandle to get adjacencies of.
          \param to_dimension Dimension of desired adjacency information.
          \param adj_entities Vector in which adjacent EntityHandles are returned. 
          \param operation_type enum of INTERSECT or UNION.  Defines whether to take
          the intersection or union of the set of adjacencies recovered for the from_entities.

          The adjacent entities in vector <em>adjacencies</em> are not in any particular 
          order. 

          Example: \code
            // get the set of edges that are adjacent to all entities in the from_entities list
            std::vector<EntityHandle> from_entities = {hex1, hex2};
            std::vector<EntityHandle> adjacencies;
            get_adjacencies( from_entities, MB_1D_ENTITY, adjacencies ); 
            \endcode */

    virtual ErrorCode get_adjacencies(const EntityHandle *from_entities,
                                         const int num_entities,
                                         const int to_dimension,
                                         const bool create_if_missing,
                                         std::vector<EntityHandle>& adj_entities,
                                         const int operation_type = Interface::INTERSECT);

    virtual ErrorCode get_adjacencies(const EntityHandle *from_entities,
                                        const int num_entities,
                                         const int to_dimension,
                                         const bool create_if_missing,
                                         Range &adj_entities,
                                         const int operation_type = Interface::INTERSECT);

    virtual ErrorCode get_adjacencies(const Range &from_entities,
                                         const int to_dimension,
                                         const bool create_if_missing,
                                         Range &adj_entities,
                                         const int operation_type = Interface::INTERSECT);

      /**\brief Get all vertices for input entities
       *
       * Special case of get_adjacencies where to_dimension == 0
       * and operation_type == Interface::UNION.  
       *\Note This is not a variation of get_connectivity because
       *      the behavior is different for polyhedra.
       */
    virtual ErrorCode get_vertices( const Range& from_entities,
                                      Range& vertices );

      //! Adds adjacencies
      /** \param from_handle entities 
          \param both_ways add the adjacency information to both the
          to_handle and and the from_from :handle

          Example: \code
      */
    virtual ErrorCode add_adjacencies(const EntityHandle from_handle, 
                                         const EntityHandle *to_handles,
                                         const int num_handles,
                                         bool both_ways);

      //! Adds adjacencies; same as vector-based, but with range instead
    virtual ErrorCode add_adjacencies(const EntityHandle from_handle, 
                                        Range &adjacencies,
                                        bool both_ways);

      //! Removes adjacencies
      /** \param handle EntityHandle to get adjacencies of.

      Example: \code
      */
    virtual ErrorCode remove_adjacencies(const EntityHandle from_handle, 
                                            const EntityHandle *to_handles,
                                            const int num_handles);

      //! Retrieves all entities in the database of given dimension.  
      /** \param dimension Dimension of entities desired.
          \param entities Range in which entities of dimension <em>dimension</em> are returned.

          Example: \code
          int dimension = 2;
          Range entities;
          get_entities_by_dimension( dimension, entities ); //get 2D EntityHandles in the database
          \endcode */
    virtual ErrorCode get_entities_by_dimension( const EntityHandle meshset,
                                                   const int dimension, 
                                                   Range &entities,
                                                   const bool recursive = false) const;

    virtual ErrorCode get_entities_by_dimension( const EntityHandle meshset,
                                                   const int dimension, 
                                                   std::vector<EntityHandle> &entities,
                                                   const bool recursive = false) const;

      //! Retrieves all entities in the data base of given type.  
      /** \param type EntityType of entities desired (ie, MeshHex, MeshEdge, MeshTri, etc )
          \param entities Range in which entities of EntityType <em>type</em> are returned.

          Example: \code
          EntityType type = MeshTet;
          Range entities;
          get_entities_by_dimension( type, entities ); //get MeshTet type EntityHandles in the database
          \endcode */
    virtual ErrorCode get_entities_by_type( const EntityHandle meshset,
                                              const EntityType type, 
                                              Range &entities,
                                              const bool recursive = false) const;

    virtual ErrorCode get_entities_by_type( const EntityHandle meshset,
                                              const EntityType type, 
                                              std::vector<EntityHandle> &entities,
                                              const bool recursive = false) const;

    virtual ErrorCode get_entities_by_type_and_tag(const EntityHandle meshset,
                                                      const EntityType type,
                                                      const Tag *tag_handles,
                                                      const void* const* values,
                                                      const int num_tags,
                                                      Range &entities,
                                                      const int condition = Interface::INTERSECT,
                                                      const bool recursive = false) const;

      //! Retrieves all entities in the data base
      /** \param entities Range in which entities of EntityType <em>type</em> are returned.

      Example: \code
      Range entities;
      get_entities( entities ); //get MeshTet type EntityHandles in the database
      \endcode */
    virtual ErrorCode get_entities_by_handle(const EntityHandle meshset,
                                      Range &entities,
                                      const bool recursive = false) const;

      //! Retrieves all entities in the data base
      /** \param entities Range in which entities of EntityType <em>type</em> are returned.

      Example: \code
      Range entities;
      get_entities( entities ); //get MeshTet type EntityHandles in the database
      \endcode */
    virtual ErrorCode get_entities_by_handle(const EntityHandle meshset,
                                      std::vector<EntityHandle> &entities,
                                      const bool recursive = false) const;

      //! Retrieves all entities in the database of given dimension.  
      /** \param dimension Dimension of entities desired.
          \param entities Range in which entities of dimension <em>dimension</em> are returned.

          Example: \code
          int dimension = 2;
          Range entities;
          get_entities_by_dimension( dimension, entities ); //get 2D EntityHandles in the database
          \endcode */
    virtual ErrorCode get_number_entities_by_dimension(const EntityHandle meshset,
                                                          const int dimension, 
                                                          int &num_entities,
                                                          const bool recursive = false) const;

      //! Retrieves all entities in the data base of given type.  
      /** \param type EntityType of entities desired (ie, MeshHex, MeshEdge, MeshTri, etc )
          \param entities Range in which entities of EntityType <em>type</em> are returned.

          Example: \code
          EntityType type = MeshTet;
          Range entities;
          get_entities_by_dimension( type, entities ); //get MeshTet type EntityHandles in the database
          \endcode */
    virtual ErrorCode get_number_entities_by_type(const EntityHandle meshset,
                                                     const EntityType type, 
                                                     int &num_entities,
                                                     const bool recursive = false) const;

    virtual ErrorCode get_number_entities_by_type_and_tag(const EntityHandle meshset,
                                                             const EntityType type,
                                                             const Tag *tag_handles,
                                                             const void* const* values,
                                                             const int num_tags,
                                                             int &num_entities,
                                                             const bool recursive = false) const;

      //! Retrieves all entities in the data base
      /** \param entities Range in which entities of EntityType <em>type</em> are returned.

      Example: \code
      Range entities;
      get_entities( entities ); //get MeshTet type EntityHandles in the database
      \endcode */
    virtual ErrorCode get_number_entities_by_handle(const EntityHandle meshset,
                                             int &num_entities,
                                             const bool recursive = false) const;

      //! Creates an element based on the type and connectivity. 
      /** If connectivity vector is not correct for EntityType <em>type</em> (ie, a vector with 
          3 vertices is passed in to make an MeshQuad), the function returns MB_FAILURE. 
          \param type Type of element to create. (MeshTet, MeshTri, MeshKnife, etc.) 
          \param connectivity Vector containing connectivity of element to create.
          \param handle EntityHandle representing the newly created element in the database.

          Example: \code
          EntityType type = MeshQuad;
          std::vector<EntityHandle> connectivity(4);
          quad_conn[0] = vertex0;
          quad_conn[1] = vertex1;
          quad_conn[2] = vertex2;
          quad_conn[3] = vertex3;
          EntityHandle element_handle;
          create_element( type, connectivity, element_handle ); \endcode */
    virtual ErrorCode create_element(const EntityType type, 
                                        const EntityHandle *connectivity,
                                        const int num_nodes, 
                                        EntityHandle &element_handle);

      //! Creates a vertex based on coordinates.  
      /**
         \param coordinates Array that has 3 doubles in it.
         \param entity_handle EntityHandle representing the newly created vertex in the database.

         Example: \code
         double *coordinates = double[3];
         coordinates[0] = 1.034;
         coordinates[1] = 23.23; 
         coordinates[2] = -0.432; 
         EntityHandle entity_handle = 0;
         create_vertex( coordinates, entity_handle ); \endcode */
    virtual ErrorCode create_vertex(const double coordinates[3], 
                                       EntityHandle &entity_handle );

    //! Create a set of vertices with the specified coordinates
    /**
       \param coordinates Array that has 3*n doubles in it.
       \param nverts Number of vertices to create
       \param entity_handles Range passed back with new vertex handles
    */
  virtual ErrorCode create_vertices(const double *coordinates, 
                                      const int nverts,
                                      Range &entity_handles );

      //! merges two entities
    virtual ErrorCode merge_entities(EntityHandle entity_to_keep, 
                                        EntityHandle entity_to_remove,
                                        bool auto_merge,
                                        bool delete_removed_entity);

      //! Removes entities in a vector from the data base.  
      /** If any of the entities are contained in any meshsets, it is removed from those meshsets 
          which were created with MESHSET_TRACK_OWNER option bit set.  Tags for <em>entity<\em> are 
          removed as part of this function.
          \param entities 1d vector of entities to delete
          \param num_entities Number of entities in 1d vector
      */ 
    virtual ErrorCode delete_entities(const EntityHandle *entities,
                                         const int num_entities);

      //! Removes entities in a range from the data base.  
      /** If any of the entities are contained in any meshsets, it is removed from those meshsets 
          which were created with MESHSET_TRACK_OWNER option bit set.  Tags for <em>entity<\em> are 
          removed as part of this function.
          \param entities Range of entities to delete
      */ 
    virtual ErrorCode delete_entities(const Range &entities);

  virtual ErrorCode list_entities(const Range &entities) const;
  
  virtual ErrorCode list_entities(const EntityHandle *entities,
                                    const int num_entities) const;

  virtual ErrorCode list_entity(const EntityHandle entity) const;

      //! function object for recieving events from MB of higher order nodes
      //! added to entities
    class HONodeAddedRemoved
    {
    public:
      HONodeAddedRemoved(){}
      virtual ~HONodeAddedRemoved(){}
        //! node_added called when a node was added to an element's connectivity array
        //! note: connectivity array of element may be incomplete (corner nodes will exist always)
      virtual void node_added(EntityHandle node, EntityHandle element);
      virtual void node_removed(EntityHandle node);
    };
  
    virtual ErrorCode convert_entities(const EntityHandle meshset, 
                                          const bool mid_side,
                                          const bool mid_face, 
                                          const bool mid_volume, 
                                          Interface::HONodeAddedRemoved* function_object = 0);

      //! function to get the side number given two elements; returns
      //! MB_FAILURE if child not related to parent; does *not* create adjacencies
      //! between parent and child
    virtual ErrorCode side_number(const EntityHandle parent,
                                     const EntityHandle child,
                                     int &side_number,
                                     int &sense,
                                     int &offset) const;

      //! given an entity and the connectivity and type of one of its subfacets, find the
      //! high order node on that subfacet, if any
    virtual ErrorCode high_order_node(const EntityHandle parent_handle,
                                         const EntityHandle *subfacet_conn,
                                         const EntityType subfacet_type,
                                         EntityHandle &high_order_node) const;

      //! given an entity and a target dimension & side number, get that entity
    virtual ErrorCode side_element(const EntityHandle source_entity,
                                      const int dim, 
                                      const int side_number,
                                      EntityHandle &target_entity) const;

      //-------------------------Tag Stuff-------------------------------------//

  //! Creates a dense tag with a name.
  /** Use to store data that is larger than 8 bits, on many 
      EntityHandles across all entity types.  Allows for storage of data of 
      <em>tag_size</em> bytes on any arbitrary entity.  
      \param tag_name String name of Tag.
      \param tag_size Size of data to store on tag, in bytes.  For storing data 
      1 byte or less in size, use tag_create_bits(...)
      \param tag_handle Tag to be created.
      \param default_value Default value tag data is set to when initially created.

      Example: \code
      std::string tag_name = "my_meshset_tag";
      int tag_size = sizeof(double); 
      Tag tag_handle = 0;
      double value = 100.5;
      const void *default_value = &value;
      tag_create_dense( tag_name, 
              tag_size, default_value ); //Create a dense tag. 
                                         //The tag will hold data the size of a 
                                         //double and that data will initially be 
                                         //set to 100.5  \endcode */
  virtual ErrorCode tag_create(const char *tag_name,
                                  const int tag_size, 
                                  const TagType type,
                                  Tag &tag_handle, 
                                  const void *default_value);

    /** \brief Define a new tag.
     *
     * Define a new tag for storing application-defined data on MB entities.  
     *
     * \param name    The name of the tag.
     * \param size    The size of the tag data in bytes.
     * \param storage The tag storage type.
     * \param data    The tag data type.
     * \param handle  The tag handle (output)
     * \param def_val Optional default value for tag.
     * \param use_existing  If true, and a tag with the same name and
     *                same description exists, successfully return the
     *                handle of the tag.
     * \return - MB_ALREADY_ALLOCATED if a tag with name already exists.
     *         - MB_FAILURE if inconsistant arguments
     *         - MB_SUCCESS otherwise.
     */
  virtual ErrorCode tag_create( const      char* name,
                                  const        int size,
                                  const  TagType storage,
                                  const DataType data,
                                            Tag& handle,
                                  const      void* def_val,
                                              bool use_existing);

    /**\brief Define a new tag that can store variable-length data.
     *
     * Define a new tag for storing application-defined data on MB entities.  
     *
     * \param name          The name of the tag.
     * \param storage       The tag storage type (MB_TAG_BIT not supported).
     * \param data_type     The tag data type.
     * \param handle_out    The tag handle (output)
     * \param default_value Optional default value for tag.
     * \param default_val_len Length of default value.  Required if
     *                      default value is specified.
     * \return - MB_ALREADY_ALLOCATED if a tag with name already exists.
     *         - MB_FAILURE if inconsistant arguments
     *         - MB_SUCCESS otherwise.
     */
  virtual ErrorCode tag_create_variable_length( const char* name,
                                                  TagType   storage,
                                                  DataType  data_type,
                                                  Tag&      handle_out,
                                                  const void* default_value = 0,
                                                  int         default_val_len = 0 
                                                 );

  //! Gets the tag name string of the tag_handle.
  /** \param tag_handle Tag you want the name of.  
      \param tag_name Name string of <em>tag_handle</em>. 

      Example: \code
      Tag tag_handle = 0;
      std::string tag_name = "my_special_tag";
      tag_get_name( tag_handle, tag_name );  //gets the Tag from the tag's name string
      \endcode */
  virtual ErrorCode  tag_get_name(const Tag tag_handle, 
                                     std::string& tag_name) const;

  //! Gets tag handle from the tag's string name. 
  /**
      \param tag_name Name string of desired tag. 
      \param tag_handle Tag to be retrieved.

      Example: \code
      Tag tag_handle = 0;
      std::string tag_name = "quad_data_flag";
      tag_get_handle( tag_name, tag_handle ); \endcode */ 
  virtual ErrorCode  tag_get_handle(const char *tag_name, 
                                       Tag &tag_handle) const;

  //! Get handles for all tags defined on this entity
  virtual ErrorCode tag_get_tags_on_entity(const EntityHandle entity,
                                             std::vector<Tag> &tag_handles) const;

  //! get size of tag in bytes
  virtual ErrorCode tag_get_size(const Tag, int &tag_size) const;

    //! Get the default value of the specified tag
  virtual ErrorCode tag_get_default_value(const Tag tag, void *def_val) const;
  virtual ErrorCode tag_get_default_value( Tag tag, const void*& def_val, int& size) const;

  //! get type of tag (sparse, dense, etc.; 0 = dense, 1 = sparse, 2 = bit, 3 = mesh)
  virtual ErrorCode tag_get_type(const Tag, TagType &tag_type) const;

   /** \brief Get data type of tag.
    *
    * Get the type of the tag data.  The tag is data is assumed to
    * be a vector of this type.  If the tag data vetcor contains 
    * more than one value, then the tag size must be a multiple of
    * the size of this type.
    * \param tag  The tag 
    * \param type The type of the specified tag (output).
    */
  virtual ErrorCode tag_get_data_type(const Tag tag, DataType& type) const;

  //! get handles for all tags defined
  virtual ErrorCode tag_get_tags(std::vector<Tag> &tag_handles) const;

  virtual ErrorCode  tag_get_data(const Tag tag_handle, 
                                     const EntityHandle* entity_handles, 
                                     const int num_entities, 
                                     void *tag_data) const;

  virtual ErrorCode  tag_get_data(const Tag tag_handle, 
                                     const Range& entity_handles, 
                                     void *tag_data) const;

  //! Sets the data of a given EntityHandle and Tag.  
  /** If the <em>tag_handle</em> and the entity type of <em>entity_handle</em> are not 
      compatible, data of <em>entity_handle</em> never existed and MB_FAILURE 
      is returned. 
      \param tag_handle Tag indicating what data is to be set.
      \param entity_handle EntityHandle on which to set tag's data. 
      \param tag_data Data to set the <em>entity_handle</em>'s tag data to.

      Example: \code
      int tag_data = 1004;
      tag_set_data( tag_handle, entity_handle, &tag_data ); \endcode */
  virtual ErrorCode  tag_set_data(const Tag tag_handle, 
                                     const EntityHandle* entity_handles, 
                                     const int num_entities,
                                     const void *tag_data );
  
  virtual ErrorCode  tag_set_data(const Tag tag_handle, 
                                     const Range& entity_handles,
                                     const void *tag_data );


    /**\brief Get pointers to tag data
     *
     * For a tag, get the values for a list of passed entity handles.
     *\param tag_handle     The tag
     *\param entity_handles An array of entity handles for which to retreive tag values.
     *\param num_entities   The length of the 'entity_handles' array.
     *\param tag_data       An array of 'const void*'.  Array must be at least
     *                      'num_entitities' long.  Array is populated (output)
     *                      with pointers to the internal storage for the
     *                      tag value corresponding to each entity handle.
     *\param tag_sizes      The length of each tag value.  Optional for 
     *                      fixed-length tags.  Required for variable-length tags.
     */
  virtual ErrorCode  tag_get_data(const Tag tag_handle, 
                                    const EntityHandle* entity_handles, 
                                    const int num_entities, 
                                    const void** tag_data,
                                    int* tag_sizes = 0 ) const;

    /**\brief Get pointers to tag data
     *
     * For a tag, get the values for a list of passed entity handles.
     *\param tag_handle     The tag
     *\param entity_handles The entity handles for which to retreive tag values.
     *\param tag_data       An array of 'const void*'.  Array is populated (output)
     *                      with pointers to the internal storage for the
     *                      tag value corresponding to each entity handle.
     *\param tag_sizes      The length of each tag value.  Optional for 
     *                      fixed-length tags.  Required for variable-length tags.
     */
  virtual ErrorCode  tag_get_data(const Tag tag_handle, 
                                    const Range& entity_handles, 
                                    const void** tag_data,
                                    int* tag_sizes = 0 ) const;

    /**\brief Set tag data given an array of pointers to tag values.
     *
     * For a tag, set the values for a list of passed entity handles.
     *\param tag_handle     The tag
     *\param entity_handles An array of entity handles for which to set tag values.
     *\param num_entities   The length of the 'entity_handles' array.
     *\param tag_data       An array of 'const void*'.  Array must be at least
     *                      'num_entitities' long.  Array is expected to
     *                      contain pointers to tag values for the corresponding
     *                      EntityHandle in 'entity_handles'.
     *\param tag_sizes      The length of each tag value.  Optional for 
     *                      fixed-length tags.  Required for variable-length tags.
     */
  virtual ErrorCode  tag_set_data(const Tag tag_handle, 
                                    const EntityHandle* entity_handles, 
                                    const int num_entities,
                                    void const* const* tag_data ,
                                    const int* tag_sizes = 0 );
  
    /**\brief Set tag data given an array of pointers to tag values.
     *
     * For a tag, set the values for a list of passed entity handles.
     *\param tag_handle     The tag
     *\param entity_handles The entity handles for which to set tag values.
     *\param tag_data       An array of 'const void*'.  Array is expected to
     *                      contain pointers to tag values for the corresponding
     *                      EntityHandle in 'entity_handles'.
     *\param tag_sizes      The length of each tag value.  Optional for 
     *                      fixed-length tags.  Required for variable-length tags.
     */
  virtual ErrorCode  tag_set_data(const Tag tag_handle, 
                                    const Range& entity_handles,
                                    void const* const* tag_data,
                                    const int* tag_sizes = 0 );

  //! Delete the data of a vector of entity handles and sparse tag
  /** Delete the data of a tag on a vector of entity handles.  Only sparse tag data are deleted with this
      function; dense tags are deleted by deleting the tag itself using tag_delete.
      \param tag_handle Handle of the (sparse) tag being deleted from entity
      \param entity_handles 1d vector of entity handles from which the tag is being deleted
      \param num_handles Number of entity handles in 1d vector
  */
  virtual ErrorCode  tag_delete_data(const Tag tag_handle, 
                                        const EntityHandle *entity_handles,
                                        const int num_handles);

  //! Delete the data of a range of entity handles and sparse tag
  /** Delete the data of a tag on a range of entity handles.  Only sparse tag data are deleted with this
      function; dense tags are deleted by deleting the tag itself using tag_delete.
      \param tag_handle Handle of the (sparse) tag being deleted from entity
      \param entity_range Range of entities from which the tag is being deleted
  */
  virtual ErrorCode  tag_delete_data(const Tag tag_handle, 
                                        const Range &entity_range);

  //! Removes the tag from the database and deletes all of its associated data.
  virtual ErrorCode  tag_delete(Tag tag_handle);

  /**a;dlfa;sfsdafasdfl; 
     a;dlfja;sljfl;sadfasd
     a;dlkfj;lsajdf */

  //! creates a mesh set
  virtual ErrorCode create_meshset(const unsigned int options, 
                                     EntityHandle &ms_handle,
                                     int start_id = 0);

  //! Empty a vector of mesh set
  /** Empty a mesh set.
      \param ms_handles 1d vector of handles of sets being emptied
      \param num_meshsets Number of entities in 1d vector
  */
  virtual ErrorCode clear_meshset( const EntityHandle *ms_handles, 
                                     const int num_meshsets);

  //! Empty a range of mesh set
  /** Empty a mesh set.
      \param ms_handles Range of handles of sets being emptied
  */
  virtual ErrorCode clear_meshset(const Range &ms_handles);

  //! get the options of a mesh set
  virtual ErrorCode get_meshset_options(const EntityHandle ms_handle, 
                                           unsigned int& options) const;

  //! set the options of a mesh set
  virtual ErrorCode set_meshset_options(const EntityHandle ms_handle, 
                                          const unsigned int options);

  //! subtracts meshset2 from meshset1 - modifies meshset1
  virtual ErrorCode subtract_meshset(EntityHandle meshset1, 
                                        const EntityHandle meshset2);

  //! intersects meshset2 with meshset1 - modifies meshset1
  virtual ErrorCode intersect_meshset(EntityHandle meshset1, 
                                         const EntityHandle meshset2);
    
  //! unites meshset2 with meshset1 - modifies meshset1
  virtual ErrorCode unite_meshset(EntityHandle meshset1, 
                                     const EntityHandle meshset2);

  //! add entities to meshset
  virtual ErrorCode add_entities(EntityHandle meshset, 
                                    const Range &entities);

  //! add entities to meshset
  virtual ErrorCode add_entities(EntityHandle meshset, 
                                    const EntityHandle *entities,
                                    const int num_entities);
  
  //! remove entities from meshset
  virtual ErrorCode remove_entities(EntityHandle meshset, 
                                       const Range &entities);

  //! remove entities from meshset
  virtual ErrorCode remove_entities(EntityHandle meshset, 
                                       const EntityHandle *entities,
                                       const int num_entities);

    //! return true if all entities are contained in set
  virtual bool contains_entities(EntityHandle meshset, 
                                 const EntityHandle *entities,
                                 int num_entities,
                                 const int operation_type = Interface::INTERSECT);

    //! replace entities
  virtual ErrorCode replace_entities(EntityHandle meshset, 
                                       const EntityHandle *old_entities,
                                       const EntityHandle *new_entities,
                                       int num_entities);

  //------MeshSet Parent/Child functions------
  
  //! get parent meshsets
  virtual ErrorCode get_parent_meshsets(const EntityHandle meshset,
                                           std::vector<EntityHandle> &parents, 
                                           const int num_hops = 1) const;

  //! get parent meshsets
  virtual ErrorCode get_parent_meshsets(const EntityHandle meshset,
                                          Range &parents, 
                                          const int num_hops = 1) const;

  //! get child meshsets
  virtual ErrorCode get_child_meshsets(const EntityHandle meshset, 
                                          std::vector<EntityHandle> &children, 
                                          const int num_hops = 1) const;

  //! get child meshsets
  virtual ErrorCode get_child_meshsets(const EntityHandle meshset, 
                                         Range &children, 
                                          const int num_hops = 1) const;

  //! get contained meshsets
  virtual ErrorCode get_contained_meshsets(const EntityHandle meshset, 
                                             std::vector<EntityHandle> &contained, 
                                             const int num_hops = 1) const;

  //! get contained meshsets
  virtual ErrorCode get_contained_meshsets(const EntityHandle meshset, 
                                             Range &contained, 
                                             const int num_hops = 1) const;

  //! gets number of parent meshsets
  virtual ErrorCode num_parent_meshsets(const EntityHandle meshset,  
                                          int *number,
                                          const int num_hops = 1) const;

  //! gets number of child meshsets
  virtual ErrorCode num_child_meshsets(const EntityHandle meshset, 
                                         int *number, 
                                         const int num_hops = 1) const;

  //! gets number of contained meshsets
  virtual ErrorCode num_contained_meshsets(const EntityHandle meshset, 
                                             int *number, 
                                             const int num_hops = 1) const;

  //! add a parent meshset
  virtual ErrorCode add_parent_meshset(EntityHandle meshset, 
                                          const EntityHandle parent_meshset);

  //! add parent meshsets
  virtual ErrorCode add_parent_meshsets(EntityHandle meshset, 
                                          const EntityHandle* parent_meshsets,
                                          int num_parent_meshsets);

  //! add a child meshset
  virtual ErrorCode add_child_meshset(EntityHandle meshset, 
                                         const EntityHandle child_meshset);

  //! add parent meshsets
  virtual ErrorCode add_child_meshsets(EntityHandle meshset, 
                                         const EntityHandle* child_meshsets,
                                         int num_child_meshsets);

  //! adds 'parent' to child's parent list and adds 'child' to parent's child list
  virtual ErrorCode add_parent_child( EntityHandle parent, 
                                         EntityHandle child );

  //! removes 'parent' to child's parent list and removes 'child' to parent's child list
  virtual ErrorCode remove_parent_child( EntityHandle parent, 
                                            EntityHandle child );

  //! remove parent meshset
  virtual ErrorCode remove_parent_meshset(EntityHandle meshset, 
                                             const EntityHandle parent_meshset);
  
  //! remove child meshset
  virtual ErrorCode remove_child_meshset(EntityHandle meshset, 
                                            const EntityHandle child_meshset);

  // ************************  error condition information *************** 

    //! return various specific tag handles
  Tag material_tag();
  Tag neumannBC_tag();
  Tag dirichletBC_tag();
  Tag globalId_tag();
  Tag geom_dimension_tag();

    //! get/set the number of nodes
    //int total_num_nodes() const;
    //void total_num_nodes(const int val);
  
    //! get/set the number of elements
    //int total_num_elements() const;
    //void total_num_elements(const int val);

    //! return a reference to the tag server
  TagServer* tag_server() {return tagServer;}

    //! return a reference to the sequence manager
  SequenceManager* sequence_manager() { return sequenceManager; }
  const SequenceManager* sequence_manager() const { return sequenceManager; }

    /// create structured sequence
  ErrorCode create_scd_sequence(const HomCoord &    coord_min,
                                  const HomCoord &  coord_max,
                                  EntityType  type,
                                  EntityID  start_id_hint,
                                  EntityHandle &  first_handle_out,
                                  EntitySequence *&  sequence_out );

  ErrorCode add_vsequence(EntitySequence *    vert_seq,
                            EntitySequence *  elem_seq,
                            const HomCoord &  p1,
                            const HomCoord &  q1,
                            const HomCoord &  p2,
                            const HomCoord &  q2,
                            const HomCoord &  p3,
                            const HomCoord &  q3,
                            bool  bb_input = false,
                            const HomCoord *  bb_min = NULL,
                            const HomCoord *  bb_max = NULL);
   
    //! return the a_entity_factory pointer
  AEntityFactory *a_entity_factory() { return aEntityFactory; }
  const AEntityFactory *a_entity_factory() const { return aEntityFactory; }
  
    //! return set of registered IO tools
  ReaderWriterSet* reader_writer_set() { return readerWriterSet; }

  Error* get_error_handler() { return mError; }

//-----------------MeshSet Interface Functions------------------//

  void print(const EntityHandle handle, const char *prefix,
             bool first_call = true) const;

  virtual ErrorCode get_last_error(std::string& info) const;

  virtual std::string get_error_string(const ErrorCode code) const;

    //! check all adjacencies for consistency
  ErrorCode check_adjacencies();
  
    //! check some adjacencies for consistency
  ErrorCode check_adjacencies(const EntityHandle *ents, int num_ents);
  
    //! return whether the input handle is valid or not
  bool is_valid(const EntityHandle this_ent) const;
  
//-----------------Memory Functions------------------//


  /**\brief Calculate amount of memory used to store MOAB data
   *
   * This function calculates the amount of memory used to store
   * MOAB data.  
   *
   * There are two possible values for each catagory of memory use.
   * The exact value and the amortized value.  The exact value is the
   * amount of memory used to store the data for the specified entities.
   * The amortized value includes the exact value and an amortized 
   * estimate of the memory consumed in overhead for storing the values
   * (indexing structures, access structures, etc.)  
   *
   * Note: If ent_array is NULL, the total memory used by MOAB for storing
   *       data will be returned in the address pointed to by
   *       total_amortized_storage, if total_amortized_storage is not NULL.
   *
   *\param ent_array Array of entities for which to estimate the memory use.
   *                 If NULL, estimate is done for all entities.
   *\param num_ents The length of ent_array.  Not used if ent_rray is NULL.
   *\param total_(amortized_)storage The sum of the memory entity, adjacency, and all tag storage.
   *\param (amortized_)entity_storage The storage for the entity definitions
   *                 (connectivity arrays for elements, coordinates for vertices,
   *                  list storage within sets, etc.)
   *\param (amortized_)adjacency_storage The storage for adjacency data.
   *\param tag_array  An array of tags for which to calculate the memory use.
   *\param num_tags   The lenght of tag_array
   *\param (amortized_)tag_storage If tag_array is not NULL, then one value
   *                   for each tag specifying the memory used for storing that
   *                   tag.  If tag_array is NULL and this value is not, the
   *                   location at which to store the total memory used for
   *                   all tags.
   */
  void estimated_memory_use( const EntityHandle* ent_array = 0,
                             unsigned long  num_ents = 0,
                             unsigned long* total_storage = 0,
                             unsigned long* total_amortized_storage = 0,
                             unsigned long* entity_storage = 0,
                             unsigned long* amortized_entity_storage = 0,
                             unsigned long* adjacency_storage = 0,
                             unsigned long* amortized_adjacency_storage = 0,
                             const Tag*   tag_array = 0,
                             unsigned       num_tags = 0,
                             unsigned long* tag_storage = 0,
                             unsigned long* amortized_tag_storage = 0 );

  /**\brief Calculate amount of memory used to store MOAB data
   *
   * This function calculates the amount of memory used to store
   * MOAB data.  
   *
   * There are two possible values for each catagory of memory use.
   * The exact value and the amortized value.  The exact value is the
   * amount of memory used to store the data for the specified entities.
   * The amortized value includes the exact value and an amortized 
   * estimate of the memory consumed in overhead for storing the values
   * (indexing structures, access structures, etc.)  
   *
   *\param ents      Entities for which to estimate the memory use.
   *\param total_(amortized_)storage The sum of the memory entity, adjacency, and all tag storage.
   *\param (amortized_)entity_storage The storage for the entity definitions
   *                 (connectivity arrays for elements, coordinates for vertices,
   *                  list storage within sets, etc.)
   *\param (amortized_)adjacency_storage The storage for adjacency data.
   *\param tag_array  An array of tags for which to calculate the memory use.
   *\param num_tags   The lenght of tag_array
   *\param (amortized_)tag_storage If tag_array is not NULL, then one value
   *                   for each tag specifying the memory used for storing that
   *                   tag.  If tag_array is NULL and this value is not, the
   *                   location at which to store the total memory used for
   *                   all tags.
   */
  void estimated_memory_use( const Range& ents,
                             unsigned long* total_storage = 0,
                             unsigned long* total_amortized_storage = 0,
                             unsigned long* entity_storage = 0,
                             unsigned long* amortized_entity_storage = 0,
                             unsigned long* adjacency_storage = 0,
                             unsigned long* amortized_adjacency_storage = 0,
                             const Tag*   tag_array = 0,
                             unsigned       num_tags = 0,
                             unsigned long* tag_storage = 0,
                             unsigned long* amortized_tag_storage = 0 );
                                     

  void print_database() const;

private:

  /**\brief Do not allow copying */
  Core( const Core& copy );
  /**\brief Do not allow copying */
  Core& operator=( const Core& copy );

  void estimated_memory_use_internal( const Range* ents,
                            unsigned long* total_storage,
                            unsigned long* total_amortized_storage,
                            unsigned long* entity_storage,
                            unsigned long* amortized_entity_storage,
                            unsigned long* adjacency_storage,
                            unsigned long* amortized_adjacency_storage,
                            const Tag*   tag_array,
                            unsigned       num_tags,
                            unsigned long* tag_storage,
                            unsigned long* amortized_tag_storage );

    //! database init and de-init routines
  ErrorCode initialize();
  void deinitialize();

    //! return the entity set representing the whole mesh
  EntityHandle get_root_set();
  
  
    //!\brief Clean up after a file reader returns failure.
    //!
    //! Delete all entities not contained in initial_entities
    //! and all tags not contained in initial_tags.
  void clean_up_failed_read( const Range& initial_entities,
                             std::vector<Tag> initial_tags );
  
    // other interfaces for MB
  WriteUtil* mMBWriteUtil;
  ReadUtil* mMBReadUtil;

    //! store the total number of elements defined in this interface
    //int totalNumElements;
  
    //! store the total number of nodes defined in this interface
    //int totalNumNodes;

    //! the overall geometric dimension of this mesh
  int geometricDimension;

  Tag materialTag;
  Tag neumannBCTag;
  Tag dirichletBCTag;
  Tag geomDimensionTag;
  Tag globalIdTag;

    //! tag server for this interface
  TagServer* tagServer;

  SequenceManager *sequenceManager;

  AEntityFactory *aEntityFactory;
  
  ReaderWriterSet* readerWriterSet;

  Error* mError;
};

} // namespace moab 

#endif   // MOAB_IMPL_GENERAL_HPP