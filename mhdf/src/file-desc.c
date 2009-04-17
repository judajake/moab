#include "mhdf.h"
#include "util.h"
#include "status.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <H5Tpublic.h>

static struct mhdf_FileDesc* alloc_file_desc( mhdf_Status* status );
static void* realloc_data( struct mhdf_FileDesc** data, size_t append_bytes, mhdf_Status* status );
static char buffer[512];

static struct mhdf_FileDesc* alloc_file_desc( mhdf_Status* status )
{
  struct mhdf_FileDesc* result;
  /* allocate a little short of a page */
  result = (struct mhdf_FileDesc*)mhdf_malloc(4000, status);
  if (mhdf_isError( status ))
    return 0;
  
  memset( result, 0, sizeof(struct mhdf_FileDesc) );
  result->total_size = 4000;
  result->offset = ((char*)result) + sizeof(struct mhdf_FileDesc);
  return result;
}

static void* realloc_data( struct mhdf_FileDesc** data, size_t append_bytes, mhdf_Status* status )
{
  void* result_ptr;
  struct mhdf_FileDesc* const input_ptr = *data;
  unsigned char* const mem_ptr = (unsigned char*)input_ptr;
  size_t new_size;
  
  if (mem_ptr + input_ptr->total_size < input_ptr->offset + append_bytes) {
    if (append_bytes < input_ptr->total_size)
      new_size = 2 * input_ptr->total_size;
    else
      new_size = input_ptr->total_size + append_bytes;
    *data = (struct mhdf_FileDesc*)mhdf_realloc( *data, new_size, status );
    if (mhdf_isError( status ))
      return 0;

    if (*data != input_ptr)
      mhdf_fixFileDesc( *data, input_ptr );
  }
  
  result_ptr = input_ptr->offset;
  input_ptr->offset += append_bytes;
  return result_ptr;
}

#define FIX_OFFSET( TYPE, FIELD ) \
  copy_ptr->FIELD = (TYPE)( ((char*)(copy_ptr->FIELD) - (char*)orig_addr) + (char*)copy_ptr )  

  
void 
mhdf_fixFileDesc( struct mhdf_FileDesc* copy_ptr, const struct mhdf_FileDesc* orig_addr )
{
  int i;
  
  API_BEGIN;
  FIX_OFFSET( int*, nodes.dense_tag_indices );
  FIX_OFFSET( int*, sets.dense_tag_indices );
  FIX_OFFSET( struct mhdf_ElemDesc*, elems );
  FIX_OFFSET( struct mhdf_TagDesc*, tags );
  
  for (i = 0; i < copy_ptr->num_elem_desc; ++i) {
    FIX_OFFSET( const char*, elems[i].handle );
    FIX_OFFSET( const char*, elems[i].type );
    FIX_OFFSET(        int*, elems[i].desc.dense_tag_indices );
  }
  
  for (i = 0; i < copy_ptr->num_tag_desc; ++i) {
    FIX_OFFSET( const char*, tags[i].name );
    FIX_OFFSET(       void*, tags[i].default_value );
    FIX_OFFSET(       void*, tags[i].global_value );
    FIX_OFFSET(        int*, tags[i].dense_elem_indices );
  }
  API_END;
}

static
struct mhdf_FileDesc* 
get_elem_desc( mhdf_FileHandle file_handle,
               struct mhdf_FileDesc* result,
               const char* elem_handle,
               int index,
               mhdf_Status* status )
{
  hid_t id_pair[2];
  int poly;
  void* ptr;
  long junk;
  
  ptr = realloc_data( &result, strlen(elem_handle)+1, status );
  if (!ptr) return NULL;
  strcpy( ptr, elem_handle );
  result->elems[index].handle = ptr;

  mhdf_getElemTypeName( file_handle, elem_handle,
                        buffer, sizeof(buffer), status );
  if (mhdf_isError(status)) {
    free( result );
    return NULL;
  }

  ptr = realloc_data( &result, strlen(buffer)+1, status );
  if (!ptr) return NULL;
  strcpy( ptr, buffer );
  result->elems[index].type = ptr;

  poly = mhdf_isPolyElement( file_handle, elem_handle, status );
  if (mhdf_isError(status)) {
    free( result );
    return NULL;
  }

  if (!poly) {
    id_pair[0] = mhdf_openConnectivity( file_handle, 
                                      elem_handle,
                                      &result->elems[index].desc.vals_per_ent,
                                      &result->elems[index].desc.count,
                                      &result->elems[index].desc.start_id,
                                      status );
    if (id_pair[0] < 0) {
      free( result );
      return NULL;
    }
    mhdf_closeData( file_handle, id_pair[0], status );
  }
  else {
    result->elems[index].desc.vals_per_ent = -1;
    mhdf_openPolyConnectivity( file_handle,
                               elem_handle,
                               &result->elems[index].desc.count,
                               &junk,
                               &result->elems[index].desc.start_id,
                               id_pair, 
                               status );
    if (id_pair[0] < 0) {
      free( result );
      return NULL;
    }
    mhdf_closeData( file_handle, id_pair[0], status );
    mhdf_closeData( file_handle, id_pair[1], status );
  }

  result->elems[index].desc.dense_tag_indices = NULL;
  result->elems[index].desc.num_dense_tags = 0;
  result->elems[index].have_adj = mhdf_haveAdjacency( file_handle, 
                                      result->elems[index].handle,
                                      status );
  if (mhdf_isError(status)) {
    free( result );
    return 0;
  }
  
  return result;
}
                   
static
unsigned get_file_id_size( hid_t file_id_type, mhdf_Status* status )
{
  if (H5Tget_class( file_id_type ) != H5T_INTEGER) {
    mhdf_setFail( status, "Invalid handle or type class for file ID type." );
    return 0;
  }
  
  return H5Tget_size( file_id_type );
}
  
static
struct mhdf_FileDesc* 
get_tag_desc( mhdf_FileHandle file_handle,
              struct mhdf_FileDesc* result,
              const char* name,
              int index,
              hid_t type,
              mhdf_Status* status )
{
  void* ptr;
  int have_default, have_global;
  int valsize, size;
  
  ptr = realloc_data( &result, strlen(name)+1, status );
  if (NULL == ptr) return NULL;
  strcpy( ptr, name );
  result->tags[index].name = ptr;
  
  mhdf_getTagInfo( file_handle, 
                   name,
                   &result->tags[index].type,
                   &result->tags[index].size,
                   &result->tags[index].storage,
                   &have_default,
                   &have_global,
                   &result->tags[index].have_sparse,
                   status );
  if (mhdf_isError(status)) {
    free( result );
    return NULL;
  }
  
  /* For variable lengt tags, have_default and have_global will
     contain the size of the respective values.  For fixed-length
     tags, they are either zero or one.  Simplify later code by
     making them contain the size for both cases. */
  valsize = result->tags[index].size;
  if (result->tags[index].size >= 0) { 
    if (have_default)
      have_default = valsize;
    if (have_global)
      have_global = valsize;
  }
  
  result->tags[index].default_value = NULL;
  result->tags[index].default_value_size = have_default;
  result->tags[index].global_value  = NULL;
  result->tags[index].global_value_size = have_global;

  switch (result->tags[index].type) {
    case mhdf_OPAQUE:  
      type = 0;
      break;
    case mhdf_BOOLEAN:
      type = H5T_NATIVE_UCHAR;
      break;
    case mhdf_INTEGER: 
      type = H5T_INTEGER;
      have_default *= sizeof(int);
      have_global *= sizeof(int);
      valsize *= sizeof(int);
      break;
    case mhdf_FLOAT:  
      type = H5T_NATIVE_DOUBLE;
      have_default *= sizeof(double);
      have_global *= sizeof(double);
      valsize *= sizeof(double); 
      break;
    case mhdf_BITFIELD:
      have_default = (have_default+7)/8;
      have_global = (have_global+7)/8;
      valsize = (valsize+7)/8;
      switch (valsize) {
      case 1:
        type = H5Tcopy( H5T_NATIVE_B8 );
        break;
      case 2:
        type = H5Tcopy( H5T_NATIVE_B16 );
        break;
      case 3:
        ++valsize;
      case 4:
        type = H5Tcopy( H5T_NATIVE_B32 );
        break;
      case 5:
        ++valsize;
      case 6:
        ++valsize;
      case 7:
        ++valsize;
      case 8:
        type = H5Tcopy( H5T_NATIVE_B64 );
        break;
      default:
        free( result );
        mhdf_setFail( status, "Cannot create a bit tag larger than 64-bits.  %d bits requested.\n", (int)valsize);
        return NULL;
      }
      break;
    case mhdf_ENTITY_ID:
      if (0 == type)
        type = H5T_NATIVE_ULONG;
      size = get_file_id_size( type, status );
      if (!size) {
        free( result );
        return NULL;
      }
      have_default *= size;
      have_global *= size;
      valsize *= size; 
      break;
    default:
      mhdf_setFail( status, "Unknown mhdf_TagDataType value (%d) for tag (\"%s\")", 
                    (int)result->tags[index].type, name );
      free( result );
      return NULL;
  }
  result->tags[index].bytes = valsize;

  if (have_default || have_global) {
    if (have_default) {
      ptr = realloc_data( &result, have_default, status );
      if (NULL == ptr) {
        if (result->tags[index].type == mhdf_BITFIELD)
          H5Tclose( type );
        return NULL;
      }
      result->tags[index].default_value = ptr;
    }
    if (have_global) {
      ptr = realloc_data( &result, have_global, status );
      if (NULL == ptr) {
        if (result->tags[index].type == mhdf_BITFIELD)
          H5Tclose( type );
        return NULL;
      }
      result->tags[index].global_value = ptr;
    }
    mhdf_getTagValues( file_handle,
                       name,
                       type,
                       result->tags[index].default_value,
                       result->tags[index].global_value,
                       status );
    if (result->tags[index].type == mhdf_BITFIELD)
      H5Tclose( type );
    if (mhdf_isError(status)) {
      free( result );
      return NULL;
    }
  }
  
  return result;
}

struct mhdf_FileDesc* 
mhdf_getFileSummary( mhdf_FileHandle file_handle, 
                     hid_t file_id_type,
                     mhdf_Status* status )
{
  struct mhdf_FileDesc* result;
  hid_t table_id;
  int i, j, k, size, *indices, have;
  void* ptr;
  char **elem_handles = 0, **tag_names = 0;
  unsigned char *array, *matrix;
  
  API_BEGIN;
  
  mhdf_setOkay( status );
  result = alloc_file_desc( status );
  if (NULL == result) return NULL;
  
    /* get node info */
  have = mhdf_haveNodes( file_handle, status );
  if (mhdf_isError(status)) {
    free( result );
    return NULL;
  }                      
  if (have) {
    table_id = mhdf_openNodeCoords( file_handle, 
                                    &result->nodes.count,
                                    &result->nodes.vals_per_ent,
                                    &result->nodes.start_id,
                                    status );
    if (table_id < 0) {
      free( result );
      return NULL;
    }
    mhdf_closeData( file_handle, table_id, status );
  }
  else {
    result->nodes.count = 0;
    result->nodes.vals_per_ent = 0;
    result->nodes.start_id = 0;
  }
  
  
  
    /* get set info */
  result->sets.vals_per_ent = -1;
  have = mhdf_haveSets( file_handle, 
                        &result->have_set_contents, 
                        &result->have_set_children, 
                        &result->have_set_parents, 
                        status );
  if (mhdf_isError(status)) {
    free( result );
    return NULL;
  }                      
  if (have) {
    table_id = mhdf_openSetMeta( file_handle,
                                 &result->sets.count,
                                 &result->sets.start_id,
                                 status );
    if (table_id < 0) {
      free( result );
      return NULL;
    }
    mhdf_closeData( file_handle, table_id, status );
  }
  else {
    result->sets.count = 0;
    result->sets.start_id = 0;
  }

  
    /* get element list */
  elem_handles = mhdf_getElemHandles( file_handle, 
                                      &result->num_elem_desc, 
                                      status );
  if (elem_handles == NULL) {
    free( result );
    return NULL;
  }
  
    /* allocate array of element descriptors */
  size = result->num_elem_desc * sizeof(struct mhdf_ElemDesc);
  ptr = realloc_data( &result, size, status );
  if (NULL == ptr) {
    free( elem_handles );
    return NULL;
  }
  memset( ptr, 0, size );
  result->elems = ptr;
  
    /* Initialize each element descriptor */
  for (i = 0; i < result->num_elem_desc; ++i) {
    result = get_elem_desc( file_handle, result, elem_handles[i], i, status );
    if (NULL == result) {
      free( elem_handles );
      return NULL;
    }
  }
  
    /* get tag list */
  tag_names = mhdf_getTagNames( file_handle, &result->num_tag_desc, status );
  if (tag_names == NULL) {
    free( elem_handles );
    free( result );
    return NULL;
  }
  
    /* allocate array of tag descriptors */
  size = result->num_tag_desc * sizeof(struct mhdf_TagDesc);
  ptr = realloc_data( &result, size, status );
  if (NULL == ptr) {
    free( elem_handles );
    free( tag_names );
    return NULL;
  }
  memset( ptr, 0, size );
  result->tags = ptr;
  
    /* Initialize each tag descriptor */
  for (i = 0; i < result->num_tag_desc; ++i) {
    result = get_tag_desc( file_handle, result, tag_names[i], i, file_id_type, status );
    if (NULL == result) {
      free( elem_handles );
      free( tag_names );
      return NULL;
    }
  }
  
    /* Determine which dense tags are present */
    
  size = (2 + result->num_elem_desc) * result->num_tag_desc;
  array = mhdf_malloc( size, status );
  if (NULL == array) {
    free( elem_handles );
    free( tag_names );
    free( result );
    return NULL;
  }
  memset( array, 0, size );
  matrix = array + (2 * result->num_tag_desc);
  
  for (j = 0; j < result->num_tag_desc; ++j) {
    if (mhdf_haveDenseTag( file_handle, tag_names[j], mhdf_node_type_handle(), status ))
      matrix[-1*result->num_tag_desc+j] = 1;
    if (mhdf_haveDenseTag( file_handle, tag_names[j], mhdf_set_type_handle(), status ))
      matrix[-2*result->num_tag_desc+j] = 1;
    for (i = 0; i < result->num_elem_desc; ++i) 
      if (mhdf_haveDenseTag( file_handle, tag_names[j], elem_handles[i], status ))
        matrix[i*result->num_tag_desc+j] = 1;
  }
  free( elem_handles );
  free( tag_names );
  
    /* Populate dense tag lists for element types */
  for (i = -2; i < result->num_elem_desc; ++i) {
    size = 0;
    for (j = 0; j < result->num_tag_desc; ++j) 
      size += matrix[i*result->num_tag_desc+j];
    if (!size) {
      indices = NULL;
    }
    else {
      indices = realloc_data( &result, size*sizeof(int), status );
      if (NULL == indices) {
        free( array );
        return NULL;
      }
    
      k = 0;
      for (j = 0; j < result->num_tag_desc; ++j)
        if (matrix[i*result->num_tag_desc+j])
          indices[k++] = j;
      assert( k == size );
    }
    
    if (i == -2) {
      result->sets.dense_tag_indices = indices;
      result->sets.num_dense_tags = size;
    }
    else if (i == -1) {
      result->nodes.dense_tag_indices = indices;
      result->nodes.num_dense_tags = size;
    }
    else {
      result->elems[i].desc.dense_tag_indices = indices;
      result->elems[i].desc.num_dense_tags = size;
    }
  }
  
    /* Populate dense tag lists for each tag */
  for (j = 0; j < result->num_tag_desc; ++j) {
    size = 0;
    for (i = -2; i < result->num_elem_desc; ++i) 
      size += matrix[i*result->num_tag_desc+j];
    if (!size) {
      indices = 0;
    }
    else {
      indices = realloc_data( &result, size*sizeof(int), status );
      if (NULL == ptr) {
        free( array );
        return NULL;
      }
      
      k = 0;
      for (i = -2; i < result->num_elem_desc; ++i) 
        if (matrix[i*result->num_tag_desc+j])
          indices[k++] = i;
      assert( k == size );
    }
    
    result->tags[i].num_dense_indices = size;
    result->tags[i].dense_elem_indices = indices;
  }
  
    /* Compact memory and return */
  free( array );
  result->total_size = result->offset - (unsigned char*)result;
  
  API_END;
  return result;
}

  
  
  
  