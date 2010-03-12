#ifndef ELEMENT_SEQUENCE_HPP
#define ELEMENT_SEQUENCE_HPP

#include "EntitySequence.hpp"
#include "SequenceData.hpp"
#include "moab/MBCN.hpp"

namespace moab {

class ElementSequence : public EntitySequence
{
public:
  
  ElementSequence( EntityHandle start,
                   EntityID count,
                   unsigned int nodes_per_element,
                   SequenceData* data )
    : EntitySequence( start, count, data ), 
      nodesPerElement(nodes_per_element)
    {}
                   
  virtual ~ElementSequence() {}
  
  inline unsigned int nodes_per_element() const { return nodesPerElement; }
  
  virtual ErrorCode get_connectivity( EntityHandle handle,
                                        std::vector<EntityHandle>& connect,
                                        bool topological = false ) const = 0;
  
  virtual ErrorCode get_connectivity( EntityHandle handle,
                                        EntityHandle const*& connect,
                                        int &connect_length,
                                        bool topological = false,
                                        std::vector<EntityHandle>* storage = 0
                                       ) const = 0;

  virtual ErrorCode set_connectivity( EntityHandle handle,
                                        EntityHandle const* connect,
                                        int connect_length ) = 0;

  inline EntityHandle const* get_connectivity_array() const;
  
  virtual EntityHandle* get_connectivity_array() = 0;
  
  inline bool has_mid_edge_nodes() const;
  inline bool has_mid_face_nodes() const;
  inline bool has_mid_volume_nodes() const;

protected:

  ElementSequence( ElementSequence& split_from, EntityHandle here )
    : EntitySequence( split_from, here ),
      nodesPerElement( split_from.nodesPerElement )
    {}

private:
  
  unsigned nodesPerElement;
};

inline EntityHandle const*
ElementSequence::get_connectivity_array() const
  { return const_cast<ElementSequence*>(this)->get_connectivity_array(); }

inline bool
ElementSequence::has_mid_edge_nodes() const
  { return MBCN::HasMidEdgeNodes( type(), nodes_per_element() ); }

inline bool
ElementSequence::has_mid_face_nodes() const
  { return MBCN::HasMidFaceNodes( type(), nodes_per_element() ); }

inline bool
ElementSequence::has_mid_volume_nodes() const
  { return MBCN::HasMidRegionNodes( type(), nodes_per_element() ); }
  
} // namespace moab

#endif
