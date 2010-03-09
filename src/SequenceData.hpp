#ifndef SEQUENCE_DATA_HPP
#define SEQUENCE_DATA_HPP


#include "TypeSequenceManager.hpp"

#include <vector>
#include <stdlib.h>
#include <string.h>

class TagServer;

class SequenceData
{
public:

  typedef std::vector<MBEntityHandle>* AdjacencyDataType;

  /**\param num_sequence_arrays Number of data arrays needed by the EntitySequence
   * \param start               First handle in this SequenceData
   * \param end                 Last handle in this SequenceData
   */
  inline SequenceData( int num_sequence_arrays, 
                       MBEntityHandle start,
                       MBEntityHandle end );
  
  virtual ~SequenceData();
  
  /**\return first handle in this sequence data */
  MBEntityHandle start_handle() const 
    { return startHandle; }
  
  /**\return last handle in this sequence data */
  MBEntityHandle end_handle() const
    { return endHandle; }
    
  MBEntityID size() const
    { return endHandle + 1 - startHandle; }
  
  /**\return ith array of EnitySequence-specific data */
  void*       get_sequence_data( int array_num )       
                { return arraySet[-1-array_num]; }
  /**\return ith array of EnitySequence-specific data */
  void const* get_sequence_data( int array_num ) const 
                { return arraySet[-1-array_num]; }
  
  /**\return array of adjacency data, or NULL if none. */
  AdjacencyDataType*       get_adjacency_data( )       
                { return reinterpret_cast<AdjacencyDataType*>(arraySet[0]); }
  /**\return array of adjacency data, or NULL if none. */
  AdjacencyDataType const* get_adjacency_data( ) const 
                { return reinterpret_cast<AdjacencyDataType const*>(arraySet[0]); }
  
  /**\return array of dense tag data, or NULL if none. */
  void*       get_tag_data( MBTagId tag_num )              
                { return tag_num < numTagData  ? arraySet[tag_num+1] : 0; }
  /**\return array of dense tag data, or NULL if none. */
  void const* get_tag_data( MBTagId tag_num ) const        
                { return tag_num < numTagData  ? arraySet[tag_num+1] : 0; }
  
  /**\brief Allocate array of sequence-specific data
   *
   * Allocate an array of EntitySequence-specific data.
   *\param array_num Index for which to allocate array.  
   *                 Must be in [0,num_sequence_arrays], where 
   *                 num_sequence_arrays is constructor argument.
   *\param bytes_per_ent  Bytes to allocate for each entity.
   *\param initial_val Value to initialize array with.  If non-null, must
   *                   be bytes_per_ent long.  If NULL, array will be zeroed.
   *\return The newly allocated array, or NULL if error.
   */
  void* create_sequence_data( int array_num, 
                              int bytes_per_ent,
                              const void* initial_val = 0 );
                             
  /**\brief Allocate array of sequence-specific data
   *
   * Allocate an array of EntitySequence-specific data.
   *\param array_num Index for which to allocate array.  
   *                 Must be in [0,num_sequence_arrays], where 
   *                 num_sequence_arrays is constructor argument.
   *\return The newly allocated array, or NULL if error.
   */
  void* create_custom_data( int array_num, size_t total_bytes );
  
  /**\brief Allocate array for storing adjacency data.
   *
   * Allocate array for storing adjacency data.
   *\return The newly allocated array, or NULL if already allocated.
   */
  AdjacencyDataType* allocate_adjacency_data();
  
  /**\brief Allocate array of dense tag data
   *
   * Allocate an array of dense tag data.
   *\param tag_num        Dense tag ID for which to allocate array.
   *\param bytes_per_ent  Bytes to allocate for each entity.
   *\param initial_val    Value to initialize array with.  If non-null, must
   *                      be bytes_per_ent long.  If NULL, array will be zeroed.
   *\return The newly allocated array, or NULL if error.
   */
  void* create_tag_data( MBTagId tag_num, int bytes_per_ent, const void* initial_val = 0 );
  
  /**\brief Create new SequenceData that is a copy of a subset of this one
    *
    * Create a new SequenceData that is a copy of a subset of this one.
    * This function is intended for use in subdividing a SequenceData
    * for operations such as changing the number of nodes in a block of
    * elements.
    *\param start  First handle for resulting subset
    *\param end    Last handle for resulting subset
    *\param sequence_data_sizes Bytes-per-entity for sequence-specific data.
    *\NOTE Does not copy tag data.
    */
  SequenceData* subset( MBEntityHandle start, 
                        MBEntityHandle end,
                        const int* sequence_data_sizes ) const;
  
  /**\brief SequenceManager data */
  TypeSequenceManager::SequenceDataPtr seqManData;
  
  /**\brief Move tag data for a subset of this sequences to specified sequence */
  void move_tag_data( SequenceData* destination, TagServer* tag_server );
  
  /**\brief Free all tag data arrays */
  void release_tag_data(const int* tag_sizes, int num_tag_sizes);
  /**\brief Free specified tag data array */
  void release_tag_data( MBTagId tag_num, int tag_size );
  
protected:

  SequenceData( const SequenceData* subset_from,
                MBEntityHandle start, 
                MBEntityHandle end,
                const int* sequence_data_sizes );

private:

  void increase_tag_count( unsigned by_this_many );

  void* create_data( int index, int bytes_per_ent, const void* initial_val = 0 );
  void copy_data_subset( int index, 
                         int size_per_ent, 
                         const void* source, 
                         size_t offset, 
                         size_t count );

  const int numSequenceData;
  unsigned numTagData;
  void** arraySet;
  MBEntityHandle startHandle, endHandle;
};

inline SequenceData::SequenceData( int num_sequence_arrays, 
                                   MBEntityHandle start,
                                   MBEntityHandle end )
  : numSequenceData(num_sequence_arrays),
    numTagData(0),
    startHandle(start),
    endHandle(end)
{
  const size_t size = sizeof(void*) * (num_sequence_arrays + 1);
  void** data = (void**)malloc( size );
  memset( data, 0, size );
  arraySet = data + num_sequence_arrays;
}


#endif