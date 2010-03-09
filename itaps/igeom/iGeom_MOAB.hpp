#ifndef IGEOM_MOAB_HPP
#define IGEOM_MOAB_HPP

#include "iGeom.h"
#include "MBForward.hpp"

class GeomTopoTool;

/* map from MB's entity type to TSTT's entity type */
extern const iBase_EntityType tstt_type_table[MBMAXTYPE+1];

/* map to MB's entity type from TSTT's entity topology */
extern const MBEntityType mb_topology_table[MBMAXTYPE+1];

/* map from TSTT's tag types to MOAB's */
extern const MBDataType mb_data_type_table[4];

/* map from MOAB's tag types to tstt's */
extern const iBase_TagValueType tstt_data_type_table[MB_MAX_DATA_TYPE+1];

/* map from MOAB's MBErrorCode to tstt's */
extern "C" const iBase_ErrorType iBase_ERROR_MAP[MB_FAILURE+1];

/* Create ITAPS iterator */
iGeom_EntityIterator create_itaps_iterator( MBRange& swap_range,
                                            int array_size = 1 ); 

/* Define macro for quick reference to MBInterface instance */
static inline MBInterface* MBI_cast( iGeom_Instance i )  
  { return reinterpret_cast<MBInterface*>(i); }         
#define MBI MBI_cast(instance)

/* Define macro for quick reference to MBInterface instance */
static inline MBEntityHandle MBH_cast( iBase_EntityHandle h )  
  { return reinterpret_cast<MBEntityHandle>(h); }         

#define GETGTT(a) {if (_my_geomTopoTool == NULL) _my_geomTopoTool =	\
	new GeomTopoTool(reinterpret_cast<MBInterface*>(a));}

/* Most recently returned error code */
//extern "C" iBase_Error iGeom_LAST_ERROR;

#define ERRORR(a) {if (iBase_SUCCESS != *err) {iGeom_processError((iBase_ErrorType) *err, a); return;}}

#define MBERRORR(a) {if (MB_SUCCESS != rval) {		\
      iGeom_processError(iBase_FAILURE, a);		\
      RETURN(iBase_FAILURE);}}

#define RETURN(a) do {iGeom_LAST_ERROR.error_type = *err = (a); return;} while(false)

#define MBRTN(a) RETURN(iBase_ERROR_MAP[(a)])

#define CHKERR(err) if (MB_SUCCESS != (err)) MBRTN(err)

#define MIN(a,b) (a > b ? b : a)

#define IMESH_INSTANCE(a) reinterpret_cast<iMesh_Instance>(a)

#define CHECK_SIZE(array, allocated, size, type, retval)	\
  if (0 != allocated && NULL != array && allocated < (size)) {	\
    iGeom_processError(iBase_MEMORY_ALLOCATION_FAILED,			\
		       "Allocated array not large enough to hold returned contents."); \
    RETURN(iBase_MEMORY_ALLOCATION_FAILED);				\
  }									\
  if ((size) && ((allocated) == 0 || NULL == (array))) {		\
    array = (type*)malloc((size)*sizeof(type));				\
    allocated=(size);							\
    if (NULL == array) {iGeom_processError(iBase_MEMORY_ALLOCATION_FAILED, \
					   "Couldn't allocate array.");RETURN(iBase_MEMORY_ALLOCATION_FAILED); } \
  }

#endif // IGEOM_MOAB_HPP