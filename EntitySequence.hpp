#ifndef ENTITY_SEQUENCE_HPP
#define ENTITY_SEQUENCE_HPP

#include "MBTypes.h"
#include "MBInternals.hpp"

class SequenceData;

class EntitySequence {
private:
  MBEntityHandle startHandle, endHandle;
  SequenceData* sequenceData;

protected:

  EntitySequence( MBEntityHandle h )
    : startHandle(h), endHandle(h) {}

  EntitySequence( EntitySequence& split_from, MBEntityHandle here )
    : startHandle( here ),
      endHandle( split_from.endHandle ),
      sequenceData( split_from.sequenceData )
  {
    split_from.endHandle = here - 1;
  }

  SequenceData* create_data_subset( MBEntityHandle start_handle,
                                    MBEntityHandle end_handle,
                                    int num_sequence_arrays,
                                    unsigned const* bytes_per_element ) const;

  MBErrorCode prepend_entities( MBEntityID count );
  MBErrorCode append_entities( MBEntityID count );

public:

  EntitySequence( MBEntityHandle start, MBEntityID count, SequenceData* data )
    : startHandle(start), endHandle( start + count - 1 ), sequenceData( data )
    {}

  virtual ~EntitySequence() {}

  MBEntityType type() const
    { return TYPE_FROM_HANDLE(start_handle()); }

  MBEntityHandle start_handle() const
    { return startHandle; }
  
  MBEntityHandle end_handle() const
    { return endHandle; }
  
  SequenceData* data() const
    { return sequenceData; }
    
  void data( SequenceData* ptr )
    { sequenceData = ptr; }
  
  MBEntityID size() const
    { return endHandle - startHandle + 1; }
    
    /**\brief True if SequenceData has no holes and is used only 
     *        by this EntitySequence */
  bool using_entire_data() const;
  
    /**\brief Integer value used in finding appropriate SequenceData
     *
     * This value is matched to input values by TypeSequenceManager to
     * determine if an available, unused portino of a SequenceData can
     * be used for a specific entity allocation.  For example, it is
     * used to find a SequenceData with the appropriate number of vertices
     * per element when allocating elements.  The default value is zero.
     */
  virtual int values_per_entity() const;
  
    /**\brief Split this sequence into two consecutive sequences
     *
     * Split this sequence into two sequences.
     *\param here New sequences should be [start_handle(),here) & [here,end_handle()]
     *\return New sequence containing [here,end_handle()]
     */
  virtual EntitySequence* split( MBEntityHandle here ) = 0;

    /**\brief Merge this sequence with another
     *
     * Combine two adjacent sequences.  Sequence handle blocks must be
     * consective and sequences must share a common SequenceData.
     */
  virtual MBErrorCode merge( EntitySequence& other );
  
    /**\brief Erase entities in range: (end_handle()-count, end_handle()] */
  virtual MBErrorCode pop_back( MBEntityID count );
  
    /**\brief Erase entities in range: [start_handle(), start_handle()+count) */
  virtual MBErrorCode pop_front( MBEntityID count );
  
    /**\brief Create a new SequenceData that is a copy of a subset of 
    *         the one referenced by this sequence.
    *
    * Create a new SequenceData that is a copy of a subset of the 
    * SequenceData referenced by this EntitySequence.  Do not make any
    * changes to this EntitySequence or the current SequenceData.
    */
  virtual SequenceData* create_data_subset( MBEntityHandle start_handle,
                                            MBEntityHandle end_handle ) const = 0;

    /**\brief Get memory characteristcs that are the same for all entities
     *
     * Get charactersitic constant memory use for all entities in sequence.
     *\param bytes_per_entity The total bytes consumed for each entity in
     *                        the underlying SequenceData.  It is assumed
     *                        that the same amount of memory is consumed
     *                        for unused portions of the SequenceData.
     *\param size_of_sequence The size of the leaf subclass of this class
     */
  virtual void get_const_memory_use( unsigned long& bytes_per_entity,
                                     unsigned long& size_of_sequence ) const = 0;
    /**\brief Get portion of memory use that varies per entity
     *
     *\return Any per-entity memory use not accounted for in the results
     *        of get_const_memory_use.
     */
  virtual unsigned long get_per_entity_memory_use( MBEntityHandle first,
                                                   MBEntityHandle last ) const;
};

#endif
