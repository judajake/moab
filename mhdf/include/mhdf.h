#ifndef MHDF_H
#define MHDF_H

#include <H5Ipublic.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 *\defgroup mhdf MHDF API for reading/writing TSTT-format HDF5 mesh files.
 */
/*@{*/


/**
 *\defgroup mhdf_status Error handling
 */
/*@{*/

#define MHDF_MESSAGE_BUFFER_LEN 160

/** \brief Struct used to return error status. */
typedef struct struct_mhdf_Status { char message[MHDF_MESSAGE_BUFFER_LEN]; } mhdf_Status;

/** \brief Return 1 if passed status object indicates an error.  Zero otherwise. */
int 
mhdf_isError( mhdf_Status const* );

/** \brief Get the error message given a status object.  */
const char*
mhdf_message( mhdf_Status const* );

/*@}*/

/**
 *\defgroup mhdf_type Common element type names.
 */
/*@{*/

/** \brief Name to use for edge element */
#define mhdf_EDGE_TYPE_NAME        "Edge"
/** \brief Name to use for triangle element */
#define mhdf_TRI_TYPE_NAME         "Tri"     
/** \brief Name to use for quadrilateral element */
#define mhdf_QUAD_TYPE_NAME        "Quad"    
/** \brief Name to use for general polygon element */
#define mhdf_POLYGON_TYPE_NAME     "Polygon" 
/** \brief Name to use for tetrahedral element */
#define mhdf_TET_TYPE_NAME         "Tet"     
/** \brief Name to use for quad-based pyramid element */
#define mhdf_PYRAMID_TYPE_NAME     "Pyramid" 
/** \brief Name to use for triangular prism element */
#define mhdf_PRISM_TYPE_NAME       "Prism"   
/** \brief Name to use for knife element */
#define mdhf_KNIFE_TYPE_NAME       "Knife"   
/** \brief Name to use for quad-sided hexahedral element */
#define mdhf_HEX_TYPE_NAME         "Hex"     
/** \brief Name to use for general polyhedron specified as a arbitrary-length list of faces */
#define mhdf_POLYHEDRON_TYPE_NAME  "Polyhedron" 
/** \brief Name to use for hexagonal-based pyramid */
#define mhdf_SEPTAHEDRON_TYPE_NAME "Septahedron" 

/*@}*/

/**
 *\defgroup mhdf_group Element group handle
 */
/*@{*/


/** Opaque handle to an element group. 
 * An element group is the data for a block of elements with the same 
 * TSTT type and same number of nodes in their connectivity data.
 * (e.g. all the HEX20 elements).  This function is also
 * used to create the groups for general polygon data and
 * general polyhedron data.  The requirement that all elements
 * have the same number of nodes in their connectivity does not
 * apply for poly(gons|hedra).
 */
typedef short mhdf_ElemHandle;

/** \brief Get an mhdf_ElemHandle object for the node data.  
 *
 * \return A special element group handle used when specifying adjacency or
 * tag data for nodes. 
 */
mhdf_ElemHandle
mhdf_node_type_handle(void);


/** \brief Return a special element group handle used to specify the set group.
 *
 *  \return A special element group handle used to specify the set group
 *  for reading/writing tag data on sets.
 */
mhdf_ElemHandle
mhdf_set_type_handle(void);

/*@}*/

/**
 *\defgroup mhdf_file File operations
 */
/*@{*/

/** \brief Opaque handle to an open file */
typedef void* mhdf_FileHandle;



/* Top level file operations */

/** \brief Create a new file.
 *
 * Create a new HDF mesh file.  This handle must be closed with
 * <code>mhdf_closeFile</code> to avoid resource loss.
 *
 * \param filename   The path and name of the file to create
 * \param overwrite  If zero, will fail if the specified file
 *                   already exists.  If non-zero, will overwrite
 *                   an existing file.
 * \param elem_type_list The list of element types that will be stored
 *                   in the file.  If the element name exits as a pre-
 *                   defined constant (\ref mhdf_type), that constant
 *                   should be used.  If a constant does not exist for
 *                   the type, a similar naming pattern should be used
 *                   (accepted name for type, first charaster uppercase,
 *                   subsequent characters lowercase.)  The element type
 *                   index passed to \ref mhdf_addElement is then an
 *                   index into this list.  The array may contain
 *                   null entires to allow the caller some control over
 *                   the assigned indices without creating dummy types
 *                   which may confuse readers.
 * \param elem_type_list_len The length of <code>elem_type_list</code>.
 * \param status     Passed back status of API call.
 * \return An opaque handle to the file.
 */
mhdf_FileHandle
mhdf_createFile( const char* filename,
                 int overwrite,
                 const char** elem_type_list,
                 size_t elem_type_list_len,
                 mhdf_Status* status );

/** \brief Open an existing file. 
 *
 * Open an existing HDF mesh file.  This handle must be closed with
 * <code>mhdf_closeFile</code> to avoid resource loss.
 *
 * \param filename   The path and name of the file to open
 * \param writeable  If non-zero, open read-write.  Otherwise readonly.
 * \param status     Passed back status of API call.
 * \param max_id     Used to pass back the maximum global ID used in the
 *                   file.  Provided as an indication to the caller of the
 *                   size of the mesh.
 * \return An opaque handle to the file.
 */
mhdf_FileHandle
mhdf_openFile( const char* filename,
               int writeable,
               unsigned long* max_id,
               mhdf_Status* status );

/** \brief Given an element type Id, get the name. 
 * Fails if buffer is not of sufficient size.
 * \param file_handle The file.
 * \param type_index The type index.  Corresponds to indicies into
 *                   the element type list passed to \ref mhdf_createFile.
 * \param buffer     The buffer into which to copy the name.
 * \param buffer_size The length of <code>buffer</code>.
 * \param status     Passed back status of API call.
 */
void
mhdf_getElemName( mhdf_FileHandle file_handle,
                  unsigned int type_index,
                  char* buffer, size_t buffer_size, 
                  mhdf_Status* status );

/** \brief Close the file
 * \param handle     The file to close.
 * \param status     Passed back status of API call.
 */
void
mhdf_closeFile( mhdf_FileHandle handle,
                mhdf_Status* status );

/** \brief Common close function for all data handle types.
 *
 * Close an hid_t-type handle returned from any of the following
 * functions.  Any hid_t passed-back or returnd must be closed via
 * this function to avoid resouce loss.
 *
 * \param file   The file the object pointed to by the passed data
 *               handled exists int.
 * \param handle The data object to close.
 * \param status     Passed back status of API call.
 */
void
mhdf_closeData( mhdf_FileHandle file,
                hid_t handle,
                mhdf_Status* status );


/** \brief Write the file history as a list of strings.
 *
 * Each entry is composed of four strings:
 * application, version, date, and time.
 *
 * \param file       The file.
 * \param strings    An array of null-terminated strings.
 * \param num_strings The length of <code>strings</code>
 * \param status     Passed back status of API call.
 */
void
mhdf_writeHistory( mhdf_FileHandle file, 
                   const char** strings, 
                   int num_strings, 
                   mhdf_Status* status );

/** \brief Read the file history as a list of strings.
 *
 * Each entry is composed of four strings:
 * application, version, date, and time.
 *
 * Strings and array are allocated with <code>malloc</code>.  Caller must
 * release them by calling <code>free</code>
 *
 * \param file       The file.
 * \param num_records_out  The length of the returned array.
 * \param status     Passed back status of API call.
 * \return An array of null-terminates strings.
 */
char**
mhdf_readHistory( mhdf_FileHandle file,
                  int* num_records_out, 
                  mhdf_Status* status );
/*@}*/
/**
 *\defgroup mhdf_node Node coordinate data.
 */
/*@{*/

/* Node Coordinates */

/** \brief Create new table for node coordinate data 
 *
 * \param file_handle  The file.
 * \param dimension    Number of coordinate values per node.
 * \param num_nodes    The number of nodes the table will contain.
 * \param first_node_id_out  Nodes are assigned IDs sequentially in the
 *             order they occur in the table, where the ID of the fisrt
 *             node in the table is this passed-back value.
 * \param status     Passed back status of API call.
 * \return An HDF5 handle to the coordinate table.
 */
hid_t
mhdf_createNodeCoords( mhdf_FileHandle file_handle,
                       int dimension,
                       long num_nodes,
                       long* first_node_id_out,
                       mhdf_Status* status );

/** \brief Open table containing node coordinate data 
 *
 * \param file_handle       The file.
 * \param dimension_out    Number of coordinate values per node.
 * \param num_nodes_out    The number of nodes the table contains.
 * \param first_node_id_out  Nodes are assigned IDs sequentially in the
 *             order they occur in the table, where the ID of the fisrt
 *             node in the table is this passed-back value.
 * \param status     Passed back status of API call.
 * \return An HDF5 handle to the coordinate table.
 */
hid_t
mhdf_openNodeCoords( mhdf_FileHandle file_handle,
                     long* num_nodes_out,
                     int* dimension_out,
                     long* first_node_id_out,
                     mhdf_Status* status );

/** \brief Write node coordinate data
 *
 * Write interleaved coordinate data for a block of nodes
 *
 * \param data_handle  Handle returned from <code>mhdf_createNodeCoords</code>
 *                     or <code>mhdf_openNodeCoords</code>.
 * \param offset       Table row (node index) at which to start writing.
 * \param count        Number of rows (number of nodes) to write.
 * \param coords       Interleaved node coordinate data.
 * \param status     Passed back status of API call.
 */
void
mhdf_writeNodeCoords( hid_t data_handle,
                      long offset,
                      long count,
                      const double* coords,
                      mhdf_Status* status );

/** \brief Write node coordinate data
 *
 * Write a single coordinate value (e.g. the 'x' coordinate) for a 
 * block of nodes.
 *
 * \param data_handle  Handle returned from <code>mhdf_createNodeCoords</code>
 *                     or <code>mhdf_openNodeCoords</code>.
 * \param offset       Table row (node index) at which to start writing.
 * \param count        Number of rows (number of nodes) to write.
 * \param dimension    The coordinate to write (0->x, 1->y, ...)
 * \param coords       Coordinate list.
 * \param status     Passed back status of API call.
 */
void
mhdf_writeNodeCoord( hid_t data_handle,
                     long offset,
                     long count,
                     int dimension,
                     const double* coords,
                     mhdf_Status* status );

/** \brief Read node coordinate data
 *
 * Read interleaved coordinate data for a block of nodes
 *
 * \param data_handle  Handle returned from <code>mhdf_createNodeCoords</code>
 *                     or <code>mhdf_openNodeCoords</code>.
 * \param offset       Table row (node index) at which to start reading.
 * \param count        Number of rows (number of nodes) to read.
 * \param coordinates  Buffer in which to write node coordinate data.
 * \param status       Passed back status of API call.
 */
void
mhdf_readNodeCoords( hid_t data_handle,
                     long offset,
                     long count,
                     double* coordinates,
                     mhdf_Status* status );


/** \brief Read node coordinate data
 *
 * Read a single coordinate value (e.g. the 'x' coordinate) for a 
 * block of nodes.
 *
 * \param data_handle  Handle returned from <code>mhdf_createNodeCoords</code>
 *                     or <code>mhdf_openNodeCoords</code>.
 * \param offset       Table row (node index) at which to start reading.
 * \param count        Number of rows (number of nodes) to read.
 * \param dimension    The coordinate to read (0->x, 1->y, ...)
 * \param coords       Buffer in which to write node coordinate data.
 * \param status     Passed back status of API call.
 */
void
mhdf_readNodeCoord( hid_t data_handle,
                    long offset,
                    long count,
                    int dimension,
                    double* coords,
                    mhdf_Status* status );

/*@}*/
/**
 *\defgroup mhdf_conn Element connectivity data.
 */
/*@{*/

/* Element Connectivity */

/** \brief Add a new table of element data to the file.
 *
 * Add a element group to the file. 
 * An element group is the data for a block of elements with the same 
 * TSTT type and same number of nodes in their connectivity data.
 * (e.g. all the HEX20 elements).  This function is also
 * used to create the groups for general polygon data and
 * general polyhedron data.  The requirement that all elements
 * have the same number of nodes in their connectivity does not
 * apply for poly(gons|hedra).
 *
 * \param file_handle  File in which to create the element type.
 * \param group_name   The name to use for the element data.  This
 *                     name is not important, but should be something
 *                     descriptive of the element type such as the
 *                     'base type' and number of nodes (e.g. "Hex20").
 * \param named_elem_type An index into the list of named element types
 *                     passed to \ref mhdf_createFile .
 * \param status     Passed back status of API call.
 * \return An opaque handle for the created element type.
 */
mhdf_ElemHandle
mhdf_addElement( mhdf_FileHandle file_handle,
                 const char* group_name,
                 unsigned int named_elem_type,
                 mhdf_Status* status );

/** \brief Get the number of element groups in the file.
 *
 * Get the count of element groups in the file.
 * An element group is the data for a block of elements with the same 
 * TSTT type and same number of nodes in their connectivity data.
 * (e.g. all the HEX20 elements).  This function is also
 * used to create the groups for general polygon data and
 * general polyhedron data.  The requirement that all elements
 * have the same number of nodes in their connectivity does not
 * apply for poly(gons|hedra).
 *
 * \param file_handle The file.
 * \param status      Passed back status of API call.
 */
int
mhdf_numElemGroups( mhdf_FileHandle file_handle,
                    mhdf_Status* status );

/** \brief Get the list of element groups in the file.
 *
 * Get the list of element groups in the file.
 * An element group is the data for a block of elements with the same 
 * TSTT type and same number of nodes in their connectivity data.
 * (e.g. all the HEX20 elements).  This function is also
 * used to create the groups for general polygon data and
 * general polyhedron data.  The requirement that all elements
 * have the same number of nodes in their connectivity does not
 * apply for poly(gons|hedra).
 *
 * \param file_handle      The file.
 * \param elem_handles_out A pointer to memory in which to write the
 *                         list of handles.  <code>mhdf_numElemGroups</code>
 *                         should be called first so the caller can ensure
 *                         this array is of sufficient size.
 * \param status      Passed back status of API call.
 */
void
mhdf_getElemGroups( mhdf_FileHandle file_handle,
                    mhdf_ElemHandle* elem_handles_out,
                    mhdf_Status* status );

/** 
 * \brief Get the element type name for a given element group handle.
 *
 * Fails if name is longer than <code>buf_len</code>.
 *
 * \param file_handle The file.
 * \param elem_handle One of the values passed back from \ref mhdf_getElemGroups
 * \param buffer      A buffer to copy the name into.
 * \param buf_len     The length of <code>buffer</code>.
 * \param status      Passed back status of API call.
 */
void
mhdf_getElemTypeName( mhdf_FileHandle file_handle,
                      mhdf_ElemHandle elem_handle,
                      char* buffer, size_t buf_len,
                      mhdf_Status* status );

/** \brief Check if an element group contains polygon or polyhedron
 *
 * Check if an element group contains general polyhon or polyhedrons
 * rather than typically fixed-connectivity elements.  
 *
 * \param file_handle The file.
 * \param elem_handle The element group.
 * \param status      Passed back status of API call.
 * \return Zero if normal fixed-connectivity element data.  Non-zero if
 *         poly(gon/hedron) general-connectivity data.
 */
int
mhdf_isPolyElement( mhdf_FileHandle file_handle,
                    mhdf_ElemHandle elem_handle,
                    mhdf_Status* status );

/** \brief Create connectivity table for an element group
 * 
 * Create fixed-connectivity data for an element group.
 * Do NOT use this function for poly(gon/hedron) data.
 *
 * \param file_handle  The file.
 * \param elem_type    The element group.
 * \param num_nodes_per_elem The number of nodes in the connectivity data
 *                     for each element.
 * \param num_elements The number of elements to be written to the table.
 * \param first_elem_id_out Elements are assigned global IDs in 
 *                     sequential blocks where the block is the table in
 *                     which their connectivity data is written and the 
 *                     sequence is the sequence in which they are written
 *                     in that table.  The global ID for the first element
 *                     in this group is passed back at this address.  The
 *                     global IDs for all other elements in the table are
 *                     assigned in the sequence in which they are written
 *                     in the table.
 * \param status      Passed back status of API call.
 * \return The HDF5 handle to the connectivity data.
 */
hid_t 
mhdf_createConnectivity( mhdf_FileHandle file_handle,
                         mhdf_ElemHandle elem_type,
                         int num_nodes_per_elem,
                         long num_elements,
                         long* first_elem_id_out,
                         mhdf_Status* status );


/** \brief Open connectivity table for an element group
 * 
 * Open fixed-connectivity data for an element group.
 * Do NOT use this function for poly(gon/hedron) data.  Use
 * <code>mhdf_isPolyElement</code> or <code>mhdf_getTsttElemType</code>
 * to check if the data is poly(gon|hedron) data before calling this
 * function to open the data.
 *
 * \param file_handle  The file.
 * \param elem_handle  The element group.
 * \param num_nodes_per_elem_out Used to pass back the number of nodes
 *                     in each element.
 * \param num_elements_out Pass back the number of elements in the table.
 * \param first_elem_id_out Elements are assigned global IDs in 
 *                     sequential blocks where the block is the table in
 *                     which their connectivity data is written and the 
 *                     sequence is the sequence in which they are written
 *                     in that table.  The global ID for the first element
 *                     in this group is passed back at this address.  The
 *                     global IDs for all other elements in the table are
 *                     assigned in the sequence in which they are written
 *                     in the table.
 * \param status      Passed back status of API call.
 * \return The HDF5 handle to the connectivity data.
 */
hid_t
mhdf_openConnectivity( mhdf_FileHandle file_handle,
                       mhdf_ElemHandle elem_handle,
                       int* num_nodes_per_elem_out,
                       long* num_elements_out,
                       long* first_elem_id_out,
                       mhdf_Status* status );

/** \brief Write element coordinate data
 *
 * Write interleaved fixed-connectivity element data for a block of elements.
 * Note: Do not use this for polygon or polyhedron data. 
 *
 * \param data_handle  Handle returned from <code>mhdf_createConnectivity</code>
 *                     or <code>mhdf_openConnectivity</code>.
 * \param offset       Table row (element index) at which to start writing.
 * \param count        Number of rows (number of elements) to write.
 * \param hdf_integer_type The type of the integer data in node_id_list.
 *                     Typically <code>H5T_NATIVE_INT</code> or
 *                     <code>N5T_NATIVE_LONG</code> as defined in <i>H5Tpublic.h</i>.
 *                     The HDF class of this type object <em>must</em> be H5T_INTEGER
 * \param node_id_list Interleaved connectivity data specified as global node IDs.
 * \param status       Passed back status of API call.
 */
void
mhdf_writeConnectivity( hid_t data_handle,
                        long offset,
                        long count,
                        hid_t hdf_integer_type,
                        const void* node_id_list,
                        mhdf_Status* status );

/** \brief Read element coordinate data
 *
 * Read interleaved fixed-connectivity element data for a block of elements.
 * Note: Do not use this for polygon or polyhedron data. 
 *
 * \param data_handle  Handle returned from <code>mhdf_createConnectivity</code>
 *                     or <code>mhdf_openConnectivity</code>.
 * \param offset       Table row (element index) at which to start read.
 * \param count        Number of rows (number of elements) to read.
 * \param hdf_integer_type The type of the integer data in node_id_list.
 *                     Typically <code>H5T_NATIVE_INT</code> or
 *                     <code>N5T_NATIVE_LONG</code> as defined in <i>H5Tpublic.h</i>.
 *                     The HDF class of this type object <em>must</em> be H5T_INTEGER
 * \param node_id_list Pointer to memory at which to write interleaved
 *                     connectivity data specified as global node IDs.
 * \param status       Passed back status of API call.
 */
void 
mhdf_readConnectivity( hid_t data_handle,
                       long offset,
                       long count,
                       hid_t hdf_integer_type,
                       void* node_id_list,
                       mhdf_Status* status );

/* Poly(gon|hedra) */


/** \brief Create a new table for polygon or polyhedron connectivity data.
 *
 * Poly (polygon or polyhedron) connectivity is stored as two lists.
 * One list is the concatenation of the the connectivity data for
 * all the polys in the group.  The other contains one value per
 * poly where that value is the index of the last entry in the
 * connectivity of the corresponding poly.  The ID
 * list for polygons contains global node IDs.  The ID list for polyhedra
 * contains the global IDs of faces (either polygons or 2D fixed-connectivity
 * elements.)
 *
 * \param file_handle  The file to write.
 * \param elem_handle  The element group.
 * \param num_poly     The total number number of polygons or polyhedra to
 *                     be written in the table.
 * \param data_list_length The tolal number of values to be written to the
 *                     table (the number of polys plus the sum of the number
 *                     of entities in each poly's connectivity data.)
 * \param first_id_out Elements are assigned global IDs in 
 *                     sequential blocks where the block is the table in
 *                     which their connectivity data is written and the 
 *                     sequence is the sequence in which they are written
 *                     in that table.  The global ID for the first element
 *                     in this group is passed back at this address.  The
 *                     global IDs for all other elements in the table are
 *                     assigned in the sequence in which they are written
 *                     in the table.
 * \param idx_and_id_handles_out The handles for the index list and
 *                     connectivity list, respectively.
 * \param status       Passed back status of API call.
 */
void
mhdf_createPolyConnectivity( mhdf_FileHandle file_handle,
                             mhdf_ElemHandle elem_handle,
                             long num_poly,
                             long data_list_length,
                             long* first_id_out,
                             hid_t idx_and_id_handles_out[2],
                             mhdf_Status* status );

/** \brief Open a table of polygon or polyhedron connectivity data.
 *
 * Poly (polygon or polyhedron) connectivity is stored as two lists.
 * One list is the concatenation of the the connectivity data for
 * all the polys in the group.  The other contains one value per
 * poly where that value is the index of the last entry in the
 * connectivity of the corresponding poly.  The ID
 * list for polygons contains global node IDs.  The ID list for polyhedra
 * contains the global IDs of faces (either polygons or 2D fixed-connectivity
 * elements.)
 *
 * \param file_handle  The file to write.
 * \param elem_handle  The element group.
 * \param num_poly_out The total number number of polygons or polyhedra to
 *                     be written in the table.
 * \param data_list_length_out The tolal number of values to be written to the
 *                     table (the number of polys plus the sum of the number
 *                     of entities in each poly's connectivity data.)
 * \param first_id_out Elements are assigned global IDs in 
 *                     sequential blocks where the block is the table in
 *                     which their connectivity data is written and the 
 *                     sequence is the sequence in which they are written
 *                     in that table.  The global ID for the first element
 *                     in this group is passed back at this address.  The
 *                     global IDs for all other elements in the table are
 *                     assigned in the sequence in which they are written
 *                     in the table.
 * \param idx_and_id_handles_out The handles for the index list and
 *                     connectivity list, respectively.
 * \param status       Passed back status of API call.
 */
void
mhdf_openPolyConnectivity( mhdf_FileHandle file_handle,
                           mhdf_ElemHandle elem_handle,
                           long* num_poly_out,
                           long* data_list_length_out,
                           long* first_id_out,
                           hid_t idx_and_id_handles_out[2],
                           mhdf_Status* status );

/** \brief Write polygon or polyhedron index data.
 *
 * Poly (polygon or polyhedron) connectivity is stored as two lists.
 * One list is the concatenation of the the connectivity data for
 * all the polys in the group.  The other contains one value per
 * poly where that value is the index of the last entry in the
 * connectivity of the corresponding poly.  The ID
 * list for polygons contains global node IDs.  The ID list for polyhedra
 * contains the global IDs of faces (either polygons or 2D fixed-connectivity
 * elements.)
 *
 * This function writes the index list.
 *
 * \param poly_handle  The handle returnded from 
 *                     <code>mhdf_createPolyConnectivity</code> or
 *                     <code>mhdf_openPolyConnectivity</code>.
 * \param offset       The offset in the table at which to write.  The
 *                     offset is in terms of the integer valus in the table,
 *                     not the count of polys.
 * \param count        The size of the integer list to write.
 * \param hdf_integer_type The type of the integer data in <code>id_list</code>.
 *                     Typically <code>H5T_NATIVE_INT</code> or
 *                     <code>N5T_NATIVE_LONG</code> as defined in <i>H5Tpublic.h</i>.
 *                     The HDF class of this type object <em>must</em> be H5T_INTEGER
 * \param index_list   The index list for the polys.
 * \param status       Passed back status of API call.
 */
void
mhdf_writePolyConnIndices( hid_t poly_handle,
                           long offset,
                           long count,
                           hid_t hdf_integer_type,
                           const void* index_list,
                           mhdf_Status* status );

/** \brief Write polygon or polyhedron connectivity data.
 *
 * Poly (polygon or polyhedron) connectivity is stored as two lists.
 * One list is the concatenation of the the connectivity data for
 * all the polys in the group.  The other contains one value per
 * poly where that value is the index of the last entry in the
 * connectivity of the corresponding poly.  The ID
 * list for polygons contains global node IDs.  The ID list for polyhedra
 * contains the global IDs of faces (either polygons or 2D fixed-connectivity
 * elements.)
 *
 * This function writes the connectivity/ID list.
 *
 * \param poly_handle  The handle returnded from 
 *                     <code>mhdf_createPolyConnectivity</code> or
 *                     <code>mhdf_openPolyConnectivity</code>.
 * \param offset       The offset in the table at which to write.  The
 *                     offset is in terms of the integer valus in the table,
 *                     not the count of polys.
 * \param count        The size of the integer list to write.
 * \param hdf_integer_type The type of the integer data in <code>id_list</code>.
 *                     Typically <code>H5T_NATIVE_INT</code> or
 *                     <code>N5T_NATIVE_LONG</code> as defined in <i>H5Tpublic.h</i>.
 *                     The HDF class of this type object <em>must</em> be H5T_INTEGER
 * \param id_list      The count/global ID list specifying the connectivity 
 *                     of the polys.
 * \param status       Passed back status of API call.
 */
void
mhdf_writePolyConnIDs( hid_t poly_handle,
                       long offset,
                       long count,
                       hid_t hdf_integer_type,
                       const void* id_list,
                       mhdf_Status* status );

/** \brief Read polygon or polyhedron index data.
 *
 * Poly (polygon or polyhedron) connectivity is stored as two lists.
 * One list is the concatenation of the the connectivity data for
 * all the polys in the group.  The other contains one value per
 * poly where that value is the index of the last entry in the
 * connectivity of the corresponding poly.  The ID
 * list for polygons contains global node IDs.  The ID list for polyhedra
 * contains the global IDs of faces (either polygons or 2D fixed-connectivity
 * elements.)
 *
 * \param poly_handle  The handle returnded from 
 *                     <code>mhdf_createPolyConnectivity</code> or
 *                     <code>mhdf_openPolyConnectivity</code>.
 * \param offset       The offset in the table at which to read.  The
 *                     offset is in terms of the integer valus in the table,
 *                     not the count of polys.
 * \param count        The size of the integer list to read.
 * \param hdf_integer_type The type of the integer data as written into memory.
 *                     Typically <code>H5T_NATIVE_INT</code> or
 *                     <code>N5T_NATIVE_LONG</code> as defined in <i>H5Tpublic.h</i>.
 *                     The HDF class of this type object <em>must</em> be H5T_INTEGER
 * \param index_list   The memory location at which to write the indices.
 * \param status       Passed back status of API call.
 */
void 
mhdf_readPolyConnIndices( hid_t poly_handle,
                          long offset,
                          long count,
                          hid_t hdf_integer_type,
                          void* index_list,
                          mhdf_Status* status );

/** \brief Read polygon or polyhedron connectivity data.
 *
 * Poly (polygon or polyhedron) connectivity is stored as two lists.
 * One list is the concatenation of the the connectivity data for
 * all the polys in the group.  The other contains one value per
 * poly where that value is the index of the last entry in the
 * connectivity of the corresponding poly.  The ID
 * list for polygons contains global node IDs.  The ID list for polyhedra
 * contains the global IDs of faces (either polygons or 2D fixed-connectivity
 * elements.)
 *
 * \param poly_handle  The handle returnded from 
 *                     <code>mhdf_createPolyConnectivity</code> or
 *                     <code>mhdf_openPolyConnectivity</code>.
 * \param offset       The offset in the table at which to read.  The
 *                     offset is in terms of the integer valus in the table,
 *                     not the count of polys.
 * \param count        The size of the integer list to read.
 * \param hdf_integer_type The type of the integer data as written into memory.
 *                     Typically <code>H5T_NATIVE_INT</code> or
 *                     <code>N5T_NATIVE_LONG</code> as defined in <i>H5Tpublic.h</i>.
 *                     The HDF class of this type object <em>must</em> be H5T_INTEGER
 * \param id_list      The memory location at which to write the connectivity data.
 * \param status       Passed back status of API call.
 */
void 
mhdf_readPolyConnIDs( hid_t poly_handle,
                      long offset,
                      long count,
                      hid_t hdf_integer_type,
                      void* id_list,
                      mhdf_Status* status );
/*@}*/
/**
 *\defgroup mhdf_adj Adjacency data.
 *
 * Adjacency data is formated as a sequence of integer groups where
 * the first entry in each group is the ID of the element for which
 * adjacencies are being specified, the second value is the count of
 * adjacent entities, and the remainder of the group is the list of
 * IDs of the adjacent entities.
 */
/*@{*/
                     

/** \brief Create adjacency data table for nodes, elements, polys, etc. 
 * 
 * Create file object for adjacency data for a nodes or a specified
 * element group.
 *
 * Adjacency data is formated as a sequence of integer groups where
 * the first entry in each group is the ID of the element for which
 * adjacencies are being specified, the second value is the count of
 * adjacent entities, and the remainder of the group is the list of
 * IDs of the adjacent entities.
 *
 * \param file_handle The file.
 * \param elem_handle The element group (or the result of
 *                    <code>mhdf_node_type_handle</code> for nodes) for
 *                    which the adjacency data is to be specified.
 * \param adj_list_size The total number of integer values contained
 *                    in the adjacency data for the specified element group.
 * \param status       Passed back status of API call.
 * \return The HDF5 handle to the connectivity data.
 */
hid_t
mhdf_createAdjacency( mhdf_FileHandle file_handle,
                      mhdf_ElemHandle elem_handle,
                      long adj_list_size,
                      mhdf_Status* status );

/** \brief Check if adjacency data is present in the file for the specified
 *  element group.
 *
 * \param file         The file.
 * \param elem_handle  A handle to an element group.  
 * \param status       Passed back status of API call.
 */
int
mhdf_haveAdjacency( mhdf_FileHandle file,
                    mhdf_ElemHandle elem_handle,
                    mhdf_Status* status );

/** \brief Open adjacency data table for nodes, elements, polys, etc. 
 * 
 * Open the file object containing adjacency data for a nodes or a specified
 * element group.
 *
 * Adjacency data is formated as a sequence of integer groups where
 * the first entry in each group is the ID of the element for which
 * adjacencies are being specified, the second value is the count of
 * adjacent entities, and the remainder of the group is the list of
 * IDs of the adjacent entities.
 *
 * \param file_handle The file.
 * \param elem_handle The element group (or the result of
 *                    <code>mhdf_node_type_handle</code> for nodes) for
 *                    which the adjacency data is to be specified.
 * \param adj_list_size The total number of integer values contained
 *                    in the adjacency data for the specified element group.
 * \param status       Passed back status of API call.
 * \return The HDF5 handle to the connectivity data.
 */
hid_t
mhdf_openAdjacency( mhdf_FileHandle file_handle,
                    mhdf_ElemHandle elem_handle,
                    long* adj_list_size,
                    mhdf_Status* status );

/** \brief Write node/element adjacency data
 *
 * Write adjacency data.
 *
 * Adjacency data is formated as a sequence of integer groups where
 * the first entry in each group is the ID of the element for which
 * adjacencies are being specified, the second value is the count of
 * adjacent entities, and the remainder of the group is the list of
 * IDs of the adjacent entities.
 *
 * \param data_handle  Handle returned from <code>mhdf_createAdjacency</code>
 *                     or <code>mhdf_openAdjacency</code>.
 * \param offset       List position at which to start writing.  Offset is
 *                     from the count if integer values written, NOT a count
 *                     of the number of elements for which adjacency data
 *                     is written.
 * \param count        Number of integer values to write.
 * \param hdf_integer_type The type of the integer data in <code>adj_list_data</code>.
 *                     Typically <code>H5T_NATIVE_INT</code> or
 *                     <code>N5T_NATIVE_LONG</code> as defined in <i>H5Tpublic.h</i>.
 *                     The HDF class of this type object <em>must</em> be H5T_INTEGER
 * \param adj_list_data Adjacency data to write.
 * \param status       Passed back status of API call.
 */
void
mhdf_writeAdjacency( hid_t data_handle,
                     long offset,
                     long count,
                     hid_t hdf_integer_type,
                     const void* adj_list_data,
                     mhdf_Status* status );

/** \brief Read node/element adjacency data
 *
 * Read adjacency data.
 *
 * Adjacency data is formated as a sequence of integer groups where
 * the first entry in each group is the ID of the element for which
 * adjacencies are being specified, the second value is the count of
 * adjacent entities, and the remainder of the group is the list of
 * IDs of the adjacent entities.
 *
 * \param data_handle  Handle returned from <code>mhdf_createAdjacency</code>
 *                     or <code>mhdf_openAdjacency</code>.
 * \param offset       List position at which to start reading.  Offset is
 *                     from the count if integer values written, NOT a count
 *                     of the number of elements for which adjacency data
 *                     is written.
 * \param count        Number of integer values to reading.
 * \param hdf_integer_type The type of the integer data in <code>adj_list_data_out</code>.
 *                     Typically <code>H5T_NATIVE_INT</code> or
 *                     <code>N5T_NATIVE_LONG</code> as defined in <i>H5Tpublic.h</i>.
 *                     The HDF class of this type object <em>must</em> be H5T_INTEGER
 * \param adj_list_data_out Pointer to memory at which to write adjacency data.
 * \param status       Passed back status of API call.
 */
void
mhdf_readAdjacency( hid_t data_handle,
                    long offset,
                    long count,
                    hid_t hdf_integer_type,
                    void* adj_list_data_out,
                    mhdf_Status* status );

/*@}*/

/**
 *\defgroup mhdf_set Meshset data.
 *
 * Meshset data is divided into three groups of data.  The set-list/meta-information table,
 * the set contents table and the set children table.  Each is written and read independently.
 *
 * The set list table contains one row for each set.  Each row contains three values:
 * {content list end index, child list end index, flags}.  The flags is a collection of bits with
 * values from \ref mhdf_set_flag .  The all the flags except \ref mhdf_SET_RANGE_BIT are
 * saved properties of the mesh data and are not relevant to the actual file in any way.  The
 * \ref mhdf_SET_RANGE_BIT flag is a toggle for how the meshset contents (not children) are saved.
 * It is an internal property of the file format and should not be passed on to the mesh database.
 * The content list end index and child list end index are the indices of the last entry for the
 * set in the contents and children tables respectively.  In the case where a set has either no
 * children or no contents, the last index of should be the same as the last index of the previous
 * set in the table, or -1 for the first set in the table.  Thus the first index is always one
 * greater than the last index of the previous set.  If the first index, calculated as one greater
 * that the last index of the previous set is greater than the last index of the current set, then
 * there are no values in the corresponding contents or children table for that set.
 *
 * The set contents table is a vector of integer global IDs that is the concatenation of the contents
 * data for all of the mesh sets.  The values are stored corresponding to the order of the sets
 * in the set list table.  Depending on the value of \ref mhdf_SET_RANGE_BIT in the flags field of
 * the set list table, the contents for a specific set may be stored in one of two formats.  If the
 * flag is set, the contents list is a list of pairs where each pair is a starting global Id and a 
 * count.  For each pair, the set contains the range of global Ids beginning at the start value. 
 * If the \ref mhdf_SET_RANGE_BIT flag is not set, the meshset contents are a simple list of global Ids.
 *
 * The meshset child table is a vector of integer global IDs.  It is a concatenation of the child
 * lists for all the mesh sets, in the order the sets occur in the meshset list table.  The values
 * are always simple lists.  The child table may never contain ranges of IDs.
 */
/*@{*/


/**
 *\defgroup mhdf_set_flag Set flag bits
 */
/*@{*/

/** \brief Make entities in set aware of owning set (MOAB-specific?)*/
#define mhdf_SET_OWNER_BIT 0x1  
/** \brief Set cannot contain duplicates */ 
#define mhdf_SET_UNIQE_BIT 0x2  
/** \brief Set order is preserved */
#define mhdf_SET_ORDER_BIT 0x4  

/** \brief The bit specifying set storage format.
 *
 * If this bit is set, then the contents of a set (not the children) 
 * is written as set of ranges, where each range is of the form
 * {global start id, count}.  For such a range, the set contains the
 * <code>count</code> entities with sequential global IDs beginning
 * with the specified start ID.  If this bit is not set in the set flags,
 * the contents of the set are stored as a simple list of global IDs.
 */
#define mhdf_SET_RANGE_BIT 0x8

/*@}*/

/** \brief Create table holding list of meshsets and their properties.
 * 
 * The set table contains description of sets, but not contents or
 * children.  The table is a <code>n x 3</code> matrix of values.  
 * One row for each of <code>n</code> sets.  Each row contains the end index
 * for the set in the contents table, the end index for the set in the children
 * table, and the set flags, respectively. The \ref mhdf_SET_RANGE_BIT
 * bit in the flags specifies the format of the contents list for each set.
 * See a description of the \ref mhdf_SET_RANGE_BIT flag for a description
 * of the two possbile data formats.  The index values in the first two columns
 * of the table are the index of the <em>last</em> value for the set in the corresponding
 * contents and children lists.  The first index is always one greater than the last index
 * for the previous set in the table.  The first index of the first set in the table is
 * implicitly zero.  A special value of -1 in the appropraite column should be used to 
 * indicate that the first set contains no contents or has no children.  For any other set,
 * if the last index for the set is the same as that of the previous set, it has no data
 * in the corresponding list. 
 *
 *\param file_handle  The file.
 *\param num_sets     The number of sets in the table.
 *\param first_set_id_out  The global ID that will be assigned to the first
 *                    set in the table.  All subsequent sets in the table
 *                    will be assigned sequential global IDs.
 * \param status       Passed back status of API call.
 *\return The handle to the set metadata table.  
 */
hid_t
mhdf_createSetMeta( mhdf_FileHandle file_handle,
                    long num_sets,
                    long* first_set_id_out,
                    mhdf_Status* status );

/** \brief Check if file contains any sets
 *
 *\param file               The file.
 *\param have_set_data_out  If non-null set to 1 if file contains table
 *                          of set contents, zero otherwise.
 *\param have_set_child_out If non-null set to 1 if file contains table
 *                          of set children, zero otherwise.
 * \param status       Passed back status of API call.
 *\return Zero if the file does not contain any sets, one if it does.
 */
int
mhdf_haveSets( mhdf_FileHandle file,
               int* have_set_data_out,
               int* have_set_child_out,
               mhdf_Status* status );

/** \brief Open table holding list of meshsets and their properties.
 * 
 * Open set list.  
 * See \ref mhdf_createSetMeta or \ref mhdf_set for a description of this data.
 *
 *\param file_handle  The file.
 *\param num_sets_out The number of sets in the table.
 *\param first_set_id_out  The global ID that will of the first
 *                    set in the table.  All subsequent sets in the table
 *                    have sequential global IDs.
 * \param status       Passed back status of API call.
 *\return The handle to the set metadata table.  
 */
hid_t
mhdf_openSetMeta( mhdf_FileHandle file_handle,
                  long* num_sets_out,
                  long* first_set_id_out,
                  mhdf_Status* status );

/** \brief Read list of sets and meta-information about sets.
 *
 * Read set descriptions.  See \ref mhdf_createSetMeta or \ref mhdf_set 
 * for a description of this data.
 *
 *\param data_handle The handle returned from \ref mhdf_createSetMeta or
 *                   \ref mhdf_openSetMeta.
 *\param offset      The offset (set index) to begin reading at.
 *\param count       The number of rows (sets, integer triples) to read.
 *\param hdf_integer_type The type of the integer data in <code>set_desc_data</code>.
 *                   Typically <code>H5T_NATIVE_INT</code> or
 *                   <code>N5T_NATIVE_LONG</code> as defined in <i>H5Tpublic.h</i>.
 *                   The HDF class of this type object <em>must</em> be H5T_INTEGER
 * \param status       Passed back status of API call.
 *\param set_desc_data The memory location at which to write the data.
 */
void
mhdf_readSetMeta( hid_t data_handle,
                  long offset,
                  long count,
                  hid_t hdf_integer_type,
                  void* set_desc_data,  
                  mhdf_Status* status );

/** \brief Write list of sets and meta-information about sets.
 *
 * Write set descriptions.  See \ref mhdf_createSetMeta or \ref mhdf_set for a 
 * description of the data format.
 *
 *\param data_handle The handle returned from \ref mhdf_createSetMeta or
 *                   \ref mhdf_openSetMeta.
 *\param offset      The offset (set index) to begin writing at.
 *\param count       The number of rows (sets, integer triples) to write.
 *\param hdf_integer_type The type of the integer data in <code>set_desc_data</code>.
 *                   Typically <code>H5T_NATIVE_INT</code> or
 *                   <code>N5T_NATIVE_LONG</code> as defined in <i>H5Tpublic.h</i>.
 *                   The HDF class of this type object <em>must</em> be H5T_INTEGER
 *\param set_desc_data The data to write.
 * \param status       Passed back status of API call.
 */
void
mhdf_writeSetMeta( hid_t data_handle,
                   long offset,
                   long count,
                   hid_t hdf_integer_type,
                   const void* set_desc_data,  
                   mhdf_Status* status );

/** \brief Create file object to hold list of meshset contents. 
 *
 * Create set contents data object.
 * The format of this data is a vector of integer values which is the
 * concatenation of the contents list for all the meshsets.  The length
 * and format of the data for each set is stored in the set metatable.
 * See \ref mhdf_createSetMeta and \ref mhdf_SET_RANGE_BIT for a 
 * description of that data.
 *
 *\param file_handle The file.
 *\param data_list_size The total length (number of integer values) to
 *                   be written for all the sets.
 *\param status      Passed back status of API call.
 *\return A handle to the table.
 */
hid_t
mhdf_createSetData( mhdf_FileHandle file_handle,
                    long data_list_size,
                    mhdf_Status* status );

/** \brief Open the file object for the meshset contents. 
 *
 * Open set contents data object.
 * The format of this data is a vector of integer values which is the
 * concatenation of the contents list for all the meshsets.  The length
 * and format of the data for each set is stored in the set metatable.
 * See \ref mhdf_createSetMeta and \ref mhdf_SET_RANGE_BIT for a 
 * description of that data.
 *
 *\param file_handle        The file.
 *\param data_list_size_out The length of the table.
 *\param status             Passed back status of API call.
 *\return                   A handle to the table.
 */
hid_t
mhdf_openSetData( mhdf_FileHandle file_handle,
                  long* data_list_size_out,
                  mhdf_Status* status );

/** \brief Write set contents.
 *
 * Write data specifying entities contained in sets.
 * The format of this data is a vector of integer values which is the
 * concatenation of the contents list for all the meshsets.  The length
 * and format of the data for each set is stored in the set metatable.
 * See \ref mhdf_createSetMeta and \ref mhdf_SET_RANGE_BIT for a 
 * description of that data.
 *
 *\param set_handle   The handle returned from \ref mhdf_createSetData
 *                    or \ref mhdf_openSetData .
 *\param offset       The position at which to write into the integer vector.
 *\param count        The number of values to write into the data vector.
 *\param hdf_integer_type The type of the integer data in <code>set_data</code>.
 *                   Typically <code>H5T_NATIVE_INT</code> or
 *                   <code>N5T_NATIVE_LONG</code> as defined in <i>H5Tpublic.h</i>.
 *                   The HDF class of this type object <em>must</em> be H5T_INTEGER
 *\param set_data    The data to write.
 *\param status      Passed back status of API call.
 */
void
mhdf_writeSetData( hid_t set_handle,
                   long offset,
                   long count,
                   hid_t hdf_integer_type,
                   const void* set_data,
                   mhdf_Status* status );

/** \brief Read set contents.
 *
 * Read data specifying entities contained in sets.
 * The format of this data is a vector of integer values which is the
 * concatenation of the contents list for all the meshsets.  The length
 * and format of the data for each set is stored in the set metatable.
 * See \ref mhdf_createSetMeta and \ref mhdf_SET_RANGE_BIT for a 
 * description of that data.
 *
 *\param set_handle   The handle returned from \ref mhdf_createSetData
 *                    or \ref mhdf_openSetData .
 *\param offset       The position at which to read from the integer vector.
 *\param count        The number of values to read from the data vector.
 *\param hdf_integer_type The type of the integer data in <code>set_data</code>.
 *                   Typically <code>H5T_NATIVE_INT</code> or
 *                   <code>N5T_NATIVE_LONG</code> as defined in <i>H5Tpublic.h</i>.
 *                   The HDF class of this type object <em>must</em> be H5T_INTEGER
 *\param set_data    A pointer to memory at which to store the read data.
 *\param status      Passed back status of API call.
 */
void
mhdf_readSetData( hid_t set_handle,
                  long offset,
                  long count,
                  hid_t hdf_integer_type,
                  void* set_data,
                  mhdf_Status* status );

/** \brief Create file object for storing the set child list 
 *
 * Create a data group for the list of set children.  
 * The format of this data is the concatenation of the lists of
 * global IDs of child sets for each set.  The order of the sets and
 * the number of children for each set is contained in teh set metatable.
 * (See \ref mhdf_createSetMeta ).
 *
 *\param file_handle      The file
 *\param child_list_size  The total length of the data (the sum of the 
 *                        number of children for each set.)
 *\param status      Passed back status of API call.
 *\return A handle to the data object in the file.
 */
hid_t
mhdf_createSetChildren( mhdf_FileHandle file_handle,
                        long child_list_size,
                        mhdf_Status* status );

/** \brief Open the file object containing the set child list 
 *
 * Open the data group conjtaining the list of set children.  
 * See \ref mhdf_createSetChildren and \ref mhdf_createSetMeta for 
 * a description of this data.
 *
 *\param file_handle      The file
 *\param child_list_size  The total length of the data (the sum of the 
 *                        number of children for each set.)
 *\param status      Passed back status of API call.
 *\return A handle to the data object in the file.
 */
hid_t
mhdf_openSetChildren( mhdf_FileHandle file_handle,
                      long* child_list_size,
                      mhdf_Status* status );

/** \brief Write set child list
 *
 * Write the list of child IDs for sets.
 * See \ref mhdf_createSetChildren and \ref mhdf_createSetMeta for 
 * a description of this data.
 * 
 *\param data_handle The value returned from \ref mhdf_createSetChildren
 *                   or \ref mhdf_openSetChildren.
 *\param offset      The offset into the list of global IDs.
 *\param count       The number of global IDs to write.
 *\param hdf_integer_type The type of the integer data in <code>child_id_list</code>.
 *                   Typically <code>H5T_NATIVE_INT</code> or
 *                   <code>N5T_NATIVE_LONG</code> as defined in <i>H5Tpublic.h</i>.
 *                   The HDF class of this type object <em>must</em> be H5T_INTEGER
 *\param child_id_list The data to write.
 *\param status      Passed back status of API call.
 */
void
mhdf_writeSetChildren( hid_t data_handle,
                       long offset,
                       long count,
                       hid_t hdf_integer_type,
                       const void* child_id_list,
                       mhdf_Status* status );

/** \brief Read set child list
 *
 * Read from the list of child IDs for sets.
 * See \ref mhdf_createSetChildren and \ref mhdf_createSetMeta for 
 * a description of this data.
 * 
 *\param data_handle The value returned from \ref mhdf_createSetChildren
 *                   or \ref mhdf_openSetChildren.
 *\param offset      The offset into the list of global IDs.
 *\param count       The number of global IDs to read.
 *\param hdf_integer_type The type of the integer data in <code>child_id_list</code>.
 *                   Typically <code>H5T_NATIVE_INT</code> or
 *                   <code>N5T_NATIVE_LONG</code> as defined in <i>H5Tpublic.h</i>.
 *                   The HDF class of this type object <em>must</em> be H5T_INTEGER
 *\param child_id_list Pointer to memory at which to store read data.
 *\param status      Passed back status of API call.
 */
void
mhdf_readSetChildren( hid_t data_handle,
                      long offset,
                      long count,
                      hid_t hdf_integer_type,
                      void* child_id_list,
                      mhdf_Status* status );

/*@}*/

/**
 *\defgroup mhdf_tag Tag data.
 *
 * The data for each tag can be stored in two places/formats:  sparse and/or
 * dense.  The data may be stored in both, but there should not be redundant 
 * values for the same entity.  
 *
 * Dense tag data is stored as multiple tables of tag values, one for each
 * element group.  (Note:  special \ref mhdf_ElemHandle values are available
 * for accessing dense tag data on nodes or meshsets via the \ref mhdf_node_type_handle
 * and \ref mhdf_set_type_handle functions.)  Each dense tag table should contain
 * the same number of entries as the element connectivity table.  The tag values 
 * are associated with the corresponding element in the connectivity table.
 *
 * Sparse tag data is stored as a global table pair for each tag type.  The first
 * if the pair of tables is a list of Global IDs.  The second is the correspoding
 * tag value for each entity in the ID list.
 */
/*@{*/


/**
 *\defgroup mhdf_tag_flag Tag type values   (MOAB-specific)
 */
/*@{*/

/** \brief Was dense tag data in mesh database */
#define mhdf_DENSE_TYPE   0 
/** \brief Was sparse tag data in mesh database */
#define mhdf_SPARSE_TYPE  1 
/** \brief Was bit-field tag data in mesh database */
#define mhdf_BIT_TYPE     2 
/** \brief Unused */
#define mhdf_MESH_TYPE    3 

/*@}*/

/** \brief Make type native-endian.
 *
 * Given an atomic HDF5 data type, return the built-in type
 * that matches the class of the passed type and is the specified
 * size. 
 *
 * This function is providied to allow converting the stored tag
 * type in a file to the preferred type for it's representation 
 * in memory when reading tag values.
 *
 * This function works only for atomic types.  The returned type
 * will be a pre-defined HDF5 object and does not need to be
 * closed/released.
 *
 *\param input_type   The type to convert.
 *\param size         The desired size in bytes.
 *\param status       Passed back status of API call.
 *\return             The converted type.
 */
hid_t
mhdf_getNativeType( hid_t input_type,
                    int size,
                    mhdf_Status* status );

/** \brief Add a tag to the file for which the HDF5 type is known
 *
 * Add a new tag to the file.  If the HDF5 type of the tag is 
 * not known, use \ref mhdf_createOpaqueTag instead.  This function
 * must be called to define the tag characteristics before values
 * for the tag can be written for entities.
 *
 *\param file_handle   The file.
 *\param tag_name      The tag name.
 *\param hdf5_tag_type The type of the tag data.
 *\param default_value The default value for the tag.
 *\param global_value  If the tag has a value on the entire mesh
 *                     that value, otherwise NULL.
 *\param tstt_tag_type The TSTT enum value for the tag type (sparse, dense, etc.)
 *\param status        Passed back status of API call.
 */
void
mhdf_createTypeTag( mhdf_FileHandle file_handle,
                    const char* tag_name,
                    hid_t hdf5_tag_type,
                    void* default_value,
                    void* global_value,
                    int tstt_tag_type,
                    mhdf_Status* status );

/** \brief Add a tag to the file for which the tag data is opaque.
 *
 * Add a new tag to the file.  This function
 * must be called to define the tag characteristics before values
 * for the tag can be written for entities.
 *
 *\param file_handle   The file.
 *\param tag_name      The tag name.
 *\param tag_size      The size of the tag data in bytes. 
 *\param default_value The default value for the tag.
 *\param global_value  If the tag has a value on the entire mesh
 *                     that value, otherwise NULL.
 *\param tstt_tag_type The TSTT enum value for the tag type (sparse, dense, etc.)
 *\param status        Passed back status of API call.
 */
void
mhdf_createOpaqueTag( mhdf_FileHandle file_handle,
                      const char* tag_name,
                      size_t tag_size,
                      void* default_value,
                      void* global_value,
                      int tstt_tag_type,
                      mhdf_Status* status );

/** \brief Add a tag to the file for which the tag data is a bit sequence
 *
 * Add a new tag to the file.  This function
 * must be called to define the tag characteristics before values
 * for the tag can be written for entities.
 *
 *\param file_handle   The file.
 *\param tag_name      The tag name.
 *\param tag_size      The size of the tag data in <em>bits</em>
 *\param default_value The default value for the tag.
 *\param global_value  If the tag has a value on the entire mesh
 *                     that value, otherwise NULL.
 *\param tstt_tag_type The TSTT enum value for the tag type (sparse, dense, etc.)
 *\param status        Passed back status of API call.
 */
void
mhdf_createBitTag( mhdf_FileHandle file_handle,
                   const char* tag_name,
                   size_t tag_size,
                   void* default_value,
                   void* global_value,
                   int tstt_tag_type,
                   mhdf_Status* status );

/** \brief Get the number of tags in the file.
 *
 *\param file_handle   The file.
 *\param status        Passed back status of API call.
 *\return The number of tags.
 */
int
mhdf_getNumberTags( mhdf_FileHandle file_handle,
                    mhdf_Status* status );

/** \brief Get the name for each tag defined in the file.
 *
 *\param file_handle   The file.
 *\param num_names_out The length of the returned array of strings.
 *\param status        Passed back status of API call.
 *\return An array of null-terminated strings.  The array
 *        and each string is allocated with <code>malloc</code>.
 *        The caller should release this memory by calling 
 *        <code>free</code> for each string and the array.
 */                 
char**
mhdf_getTagNames( mhdf_FileHandle file_handle,
                  int* num_names_out,
                  mhdf_Status* status );

/** \brief Get the description of a specified tag.
 *
 *\param file_handle        The file.
 *\param tag_name           The name of the tag to retreive the data for.
 *\param tag_data_len_out   The size of the tag value.
 *\param have_default_out   Non-zero if there is a default value for the tag.
 *                          Zero otherwise.
 *\param have_global_out    Non-zero if there is a global/mesh value for the tag.
 *                          Zero otherwise.
 *\param is_opaque_type_out Non-zero if the tag data is opaque.  Zero if
 *                          The HDF5 type of the tag value is known.
 *\param have_sparse_data_out Non-zero if there is a sparse data list for this tag.
 *\param tstt_tag_class_out The TSTT enum for the tag type (dense, sparse, etc.)
 *\param bit_tag_bits_out   If a BITFIELD type, the number of bits, otherwise zero.
 *\param hdf_type_out       A handle to the hdf5 type of the tag, if
 *                          <code>is_opaque_type_out</code> is zero.
 *\param status             Passed back status of API call.
 */
void
mhdf_getTagInfo( mhdf_FileHandle file_handle,
                 const char* tag_name,
                 int* tag_data_len_out,
                 int* have_default_out,
                 int* have_global_out,
                 int* is_opaque_type_out,
                 int* have_sparse_data_out,
                 int* tstt_tag_class_out,
                 int* bit_tag_bits_out,
                 hid_t* hdf_type_out,
                 mhdf_Status* status );

/** \brief Get the default and global values of the tag.
 *
 *\param file_handle      The file.
 *\param tag_name         The tag name.
 *\param output_data_type The HDF5 type for the memory into which the
 *                        tag data is to be written.  If zero, then 
 *                        the value(s) will be read as opaque data.
 *\param default_value    Memory location at which to write the default
 *                        value of the tag.
 *\param global_value     If the tag has a global value, the memory location
 *                        at which to write that value.
 *\param status           Passed back status of API call.
 */
void
mhdf_getTagValues( mhdf_FileHandle file_handle,
                   const char* tag_name,
                   hid_t output_data_type,
                   void* default_value,
                   void* global_value,
                   mhdf_Status* status );

/** \brief Check if the file contains dense tag data for the specified tag and element group.
 *
 * Check if there is dense tag data for a given element type for the specfiied
 * tag.  
 *
 *\param file_handle  The file.
 *\param tag_name     The tag.
 *\param elem_group   The element group handle, or the return value of
 *                    \ref mhdf_node_type_handle or \ref mhdf_set_type_handle
 *                    for nodes or sets respectively.
 *\param status       Passed back status of API call.
 *\return Non-zero if file contains specified data.  Zero otherwise.
 */
int
mhdf_haveDenseTag( mhdf_FileHandle file_handle,
                   const char* tag_name,
                   mhdf_ElemHandle elem_group,
                   mhdf_Status* status );

/** \brief Create an object to hold dense tag values for a given element group.
 *
 *\param file_handle  The file.
 *\param tag_name     The tag.
 *\param elem_group   The element group handle, or the return value of
 *                    \ref mhdf_node_type_handle or \ref mhdf_set_type_handle
 *                    for nodes or sets respectively.
 *\param num_values   The number of tag values to be written.  Must be
 *                    The same as the number of elements in the group. 
 *                    Specified here to allow tag values to be written
 *                    before node coordinates, element connectivity or meshsets.
 *\param status       Passed back status of API call.
 *\return             Handle to data object in file.
 */
hid_t
mhdf_createDenseTagData( mhdf_FileHandle file_handle,
                         const char* tag_name,
                         mhdf_ElemHandle elem_group,
                         long num_values,
                         mhdf_Status* status );

/** \brief Open the object containing dense tag values for a given element group.
 *
 *\param file_handle    The file.
 *\param tag_name       The tag.
 *\param elem_group     The element group handle, or the return value of
 *                      \ref mhdf_node_type_handle or \ref mhdf_set_type_handle
 *                      for nodes or sets respectively.
 *\param num_values_out The number of tag values to be written.  Must be
 *                      The same as the number of elements in the group. 
 *\param status         Passed back status of API call.
 *\return               Handle to data object in file.
 */
hid_t
mhdf_openDenseTagData( mhdf_FileHandle file_handle,
                       const char* tag_name,
                       mhdf_ElemHandle elem_group,
                       long* num_values_out,
                       mhdf_Status* status );

/** \brief Write dense tag values 
 *
 *\param tag_handle    Handle to the data object to write to.  The return
 *                     value of either \ref mhdf_createDenseTagData or
 *                     \ref mhdf_openDenseTagData.
 *\param offset        The offset into the list of tag values at which to
 *                     begin writing.
 *\param count         The number of tag values to write.
 *\param hdf_data_type The type of the data in memory.  If this is specified,
 *                     it must be possible for the HDF library to convert
 *                     between this type and the type the tag data is stored
 *                     as.  If zero, the tag storage type will be assumed.
 *                     This should always be zero for opaque data.
 *\param tag_data      The tag values to write.
 *\param status        Passed back status of API call.
 */
void
mhdf_writeDenseTag( hid_t tag_handle,
                    long offset,
                    long count,
                    hid_t hdf_data_type,
                    const void* tag_data,
                    mhdf_Status* status );

/** \brief Read dense tag values 
 *
 *\param tag_handle    Handle to the data object to read from.  The return
 *                     value of either \ref mhdf_createDenseTagData or
 *                     \ref mhdf_openDenseTagData.
 *\param offset        The offset into the list of tag values at which to
 *                     begin reading.
 *\param count         The number of tag values to read.
 *\param hdf_data_type The type of the data in memory.  If this is specified,
 *                     it must be possible for the HDF library to convert
 *                     between this type and the type the tag data is stored
 *                     as.  If zero, the data will be read as opaque data.
 *\param tag_data      The memory location at which to store the tag values.
 *\param status        Passed back status of API call.
 */
void
mhdf_readDenseTag( hid_t tag_handle,
                   long offset,
                   long count,
                   hid_t hdf_data_type,
                   void* tag_data,
                   mhdf_Status* status );

/** \brief Create file objects to store sparse tag data 
 *
 * Create the file objects to store all sparse data for a given tag in.  The 
 * sparse data is stored in a pair of objects.  The first is a vector of
 * global IDs.  The second is a vector of tag values for each entity specified
 * in the list of global IDs.
 *
 *\param file_handle    The file.
 *\param tag_name       The tag.
 *\param num_values     The number of tag values to be written.
 *\param entities_and_values_out The handles to the pair of file objects.
 *                      The first is the vector of global IDs.  The second
 *                      is the list of corresponding tag values.
 *\param status         Passed back status of API call.
 */
void
mhdf_createSparseTagData( mhdf_FileHandle file_handle,
                          const char* tag_name,
                          long num_values,
                          hid_t entities_and_values_out[2],
                          mhdf_Status* status );

/** \brief Create file objects to read sparse tag data 
 *
 * Open the file objects containing all sparse data for a given tag in.  The 
 * sparse data is stored in a pair of objects.  The first is a vector of
 * global IDs.  The second is a vector of tag values for each entity specified
 * in the list of global IDs.
 *
 *\param file_handle    The file.
 *\param tag_name       The tag.
 *\param num_values_out The number of tag values.
 *\param entities_and_values_out The handles to the pair of file objects.
 *                      The first is the vector of global IDs.  The second
 *                      is the list of corresponding tag values.
 *\param status         Passed back status of API call.
 */
void
mhdf_openSparseTagData( mhdf_FileHandle file_handle,
                        const char* tag_name,
                        long* num_values_out,
                        hid_t entities_and_values_out[2],
                        mhdf_Status* status );

/** \brief Write Global ID list for sparse tag data
 *
 *\param id_handle   The first handle passed back from either
 *                   \ref mhdf_createSparseTagData or 
 *                   \ref mhdf_openSparseTagData.
 *\param offset      The offset at which to begin writing.
 *\param count       The number of global IDs to write.
 *\param hdf_integer_type The type of the integer data in <code>id_list</code>.
 *                   Typically <code>H5T_NATIVE_INT</code> or
 *                   <code>N5T_NATIVE_LONG</code> as defined in <i>H5Tpublic.h</i>.
 *                   The HDF class of this type object <em>must</em> be H5T_INTEGER
 *\param id_list     The list of global IDs to write.
 *\param status      Passed back status of API call.
 */
void
mhdf_writeSparseTagEntities( hid_t id_handle,
                             long offset,
                             long count,
                             hid_t hdf_integer_type,
                             const void* id_list,
                             mhdf_Status* status );

/** \brief Write tag value list for sparse tag data
 *
 *\param value_handle  The second handle passed back from either
 *                     \ref mhdf_createSparseTagData or 
 *                     \ref mhdf_openSparseTagData.
 *\param offset        The offset at which to begin writing.
 *\param count         The number of tag values to write.
 *\param hdf_tag_data_type The type of the data in memory.  If this is specified,
 *                     it must be possible for the HDF library to convert
 *                     between this type and the type the tag data is stored
 *                     as.  If zero, the tag storage type will be assumed.
 *                     This should always be zero for opaque data.
 *\param tag_data      The list of tag values to write.
 *\param status        Passed back status of API call.
 */
void
mhdf_writeSparseTagValues( hid_t value_handle,
                           long offset,
                           long count,
                           hid_t hdf_tag_data_type,
                           const void* tag_data,
                           mhdf_Status* status );

/** \brief Read Global ID list for sparse tag data
 *
 *\param id_handle  The first handle passed back from either
 *                  \ref mhdf_createSparseTagData or 
 *                  \ref mhdf_openSparseTagData.
 *\param offset     The offset at which to begin reading.
 *\param count      The number of global IDs to read.
 *\param hdf_integer_type The type of the integer data in <code>id_list</code>.
 *                   Typically <code>H5T_NATIVE_INT</code> or
 *                   <code>N5T_NATIVE_LONG</code> as defined in <i>H5Tpublic.h</i>.
 *                   The HDF class of this type object <em>must</em> be H5T_INTEGER
 *\param id_list     The memory location at which to store the global IDs.
 *\param status      Passed back status of API call.
 */
void
mhdf_readSparseTagEntities( hid_t id_handle,
                            long offset,
                            long count,
                            hid_t hdf_integer_type,
                            void* id_list,
                            mhdf_Status* status );

/** \brief Read tag value list for sparse tag data
 *
 *\param value_handle  The second handle passed back from either
 *                  \ref mhdf_createSparseTagData or 
 *                  \ref mhdf_openSparseTagData.
 *\param offset     The offset at which to begin reading.
 *\param count      The number of tag values to read.
 *\param hdf_integer_type The type of the data in memory.  If this is specified,
 *                  it must be possible for the HDF library to convert
 *                  between this type and the type the tag data is stored
 *                  as.  If zero, the values will be read as opaque data.
 *\param id_list    Memory location at which to store tag values.
 *\param status     Passed back status of API call.
 */
void
mhdf_readSparseTagValues( hid_t value_handle,
                          long offset,
                          long count,
                          hid_t hdf_integer_type,
                          void* id_list,
                          mhdf_Status* status );


/*@}*/


/*@}*/


#ifdef __cplusplus
} // extern "C"
#endif

#endif
