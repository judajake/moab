! PushParMeshIntoMoabF90: push parallel mesh into moab, F90 version
! 
! This program shows how to push a mesh into MOAB in parallel from Fortran90, with sufficient
! information to resolve boundary sharing and exchange a layer of ghost information.
! To successfully link this example, you need to specify FCFLAGS that include:
!    a) -DUSE_MPI, and
!    b) flags required to link Fortran90 MPI programs with the C++ compiler; these flags
!       can often be found on your system by inspecting the output of 'mpif90 -show'
! For example, using gcc, the link line looks like:
!   make MOAB_DIR=<moab install dir> FCFLAGS="-DUSE_MPI -I/usr/lib/openmpi/include -pthread -I/usr/lib/openmpi/lib -L/usr/lib/openmpi/lib -lmpi_f90 -lmpi_f77 -lmpi -lopen-rte -lopen-pal -ldl -Wl,--export-dynamic -lnsl -lutil -lm -ldl" PushParMeshIntoMoabF90
!
! Usage: PushParMeshIntoMoab

program PushParMeshIntoMoab

  use ISO_C_BINDING
  implicit none

#include "mpif.h"

#ifdef USE_MPI
#  include "iMeshP_f.h"
#else
#  include "iMesh_f.h"
#endif

  ! declarations
  ! imesh is the instance handle
  iMesh_Instance imesh
  ! NUMV, NUME, NVPERE are the hardwired here; these are for the whole mesh,
  ! local mesh determined later
  integer NUMV, NUME, NVPERE
  parameter (NUMV = 8)   ! # vertices in whole mesh
  parameter (NUME = 6)   ! # elements in whole mesh
  parameter (NVPERE = 4) ! # vertices per element
  ! ents, verts will be arrays storing vertex/entity handles
  iBase_EntityHandle, pointer :: ents, verts
  iBase_EntitySetHandle root_set
  TYPE(C_PTR) :: vertsPtr, entsPtr
  ! storage for vertex positions, element connectivity indices, global vertex ids
  real*8 coords(0:3*NUMV-1)
  integer iconn(0:4*NUME-1), gids(0:NUMV-1)
  !
  ! local variables
  integer lgids(0:NUMV-1), lconn(0:4*NUME-1)
  real*8 lcoords(0:3*NUMV-1)
  integer lnumv, lvids(0:NUMV-1), gvids(0:NUMV-1)
  integer lvpe, ltp ! lvpe = # vertices per entity, ltp = element type
  integer ic, ie, iv, istart, iend, ierr, indv, lnume, rank, sz

#ifdef USE_MPI
  ! local variables for parallel runs
  iMeshP_PartitionHandle imeshp
!    integer MPI_COMM_WORLD
#endif

  ! vertex positions, latlon coords, (lat, lon, lev), fortran ordering
  ! (first index varying fastest)
  data coords / &
       0.0, -45.0,   0.0,  90.0, -45.0,   0.0, 180.0, -45.0,   0.0, 270.0, -45.0,   0.0, &
       0.0,  45.0,   0.0,  90.0,  45.0,   0.0, 180.0,  45.0,   0.0, 270.0,  45.0,   0.0 /

  ! quad index numbering, each quad ccw, sides then bottom then top
  data iconn / & 
       0, 1, 5, 4,  & 
       1, 2, 6, 5,  & 
       2, 3, 7, 6,  & 
       3, 0, 4, 7,  & 
       0, 3, 2, 1,  & 
       4, 5, 6, 7 /

  data lvpe /4/ ! quads in this example
  data ltp / iMesh_QUADRILATERAL / ! from iBase_f.h

  ! initialize global vertex ids
  do iv = 0, NUMV-1
     lgids(iv) = iv+1
  end do

#ifdef USE_MPI
  ! init the parallel partition
  call MPI_INIT(ierr)
  call MPI_COMM_SIZE(MPI_COMM_WORLD, sz, ierr)
  call MPI_COMM_RANK(MPI_COMM_WORLD, rank, ierr)
  ! compute starting/ending element numbers
  lnume = NUME / sz
  istart = rank * lnume
  iend = istart + lnume - 1
  if (rank .eq. sz-1) then
     iend = NUME-1
     lnume = iend - istart + 1
  endif
#else
  ! set the starting/ending element numbers
  istart = 0
  iend = NUME-1
  lnume = NUME
#endif

  ! for my elements, figure out which vertices I use and accumulate local indices and coords
  ! lvids stores the local 0-based index for each vertex; -1 means vertex i isn't used locally
  ! also build up connectivity indices for local elements, in lconn
  do iv = 0, NUMV-1
     lvids(iv) = -1
  end do
  lnumv = -1
  do ie = istart, iend
     do iv = 0, lvpe-1
        indv = iconn(lvpe*ie + iv)
        if (lvids(indv) .eq. -1) then
           lnumv = lnumv + 1 ! increment local # verts
           do ic = 0, 2 ! cache local coords
              lcoords(3*lnumv+ic) = coords(3*indv+ic)
           end do
           lvids(indv) = lnumv
           gvids(lnumv) = 1+indv
        end if
        lconn(lvpe*(ie-istart)+iv) = lvids(indv)
     end do  ! do iv
  end do  ! do ie
  
  lnumv = lnumv + 1

  ! now create the mesh; this also initializes parallel sharing and ghost exchange
  imesh = 0
  imeshp = 0
  call create_mesh(imesh, imeshp, MPI_COMM_WORLD, lnumv, lnume, gvids, lvpe, ltp, lcoords, lconn, &
       vertsPtr, entsPtr, ierr)
  call c_f_pointer(vertsPtr, verts, [lnumv])
  call c_f_pointer(entsPtr, ents, [lnume])

  ! get/report number of vertices, elements
  call iMesh_getRootSet(%VAL(imesh), root_set, ierr)
  iv = 0
  ie = 0
#ifdef USE_MPI
  call iMeshP_getNumOfTypeAll(%VAL(imesh), %VAL(imeshp), %VAL(root_set), %VAL(iBase_VERTEX), iv, ierr)
  call iMeshP_getNumOfTypeAll(%VAL(imesh), %VAL(imeshp), %VAL(root_set), %VAL(iBase_FACE), ie, ierr)
  if (rank .eq. 0) then
     write(0,*) "Number of vertices = ", iv
     write(0,*) "Number of entities = ", ie
  endif
#else
  call iMesh_getNumOfTypeAll(%VAL(imesh), %VAL(root_set), %VAL(iBase_VERTEX), iv, ierr)
  call iMesh_getNumOfTypeAll(%VAL(imesh), %VAL(root_set), %VAL(iBase_FACE), ie, ierr)
  write(0,*) "Number of vertices = ", iv
  write(0,*) "Number of entities = ", ie
#endif

  ! from here, can use verts and ents as (1-based) arrays of entity handles for input to other iMesh functions

  call MPI_FINALIZE(ierr)
  stop
end program PushParMeshIntoMoab

subroutine create_mesh( &
  !     interfaces
     imesh, imeshp, &
  !     input
     comm, numv, nume, vgids, nvpe, tp, posn, iconn, &
  !     output
     vertsPtr, entsPtr, ierr)
  !
  ! create a mesh with numv vertices and nume elements, with elements of type tp
  ! vertices have positions in posn (3 coordinates each, interleaved xyzxyz...), indexed from 0
  ! elements have nvpe vertices per entity, with connectivity indices stored in iconn, referencing
  ! vertices using 0-based indices; vertex and entity handles are output in arrays passed in
  !
  ! if imesh/imeshp are 0, imesh/imeshp are initialized in this subroutine
  !

  use ISO_C_BINDING
  implicit none

#ifdef USE_MPI
#  include "iMeshP_f.h"
#  include "mpif.h"
#else
#  include "iMesh_f.h"
#endif

  ! subroutine arguments
  iMesh_Instance imesh
  TYPE(C_PTR) :: vertsPtr, entsPtr
  integer numv, nume, nvpe, vgids(0:*), iconn(0:*), ierr, tp
  real*8 posn(0:*)
#ifdef USE_MPI
  iMeshP_PartitionHandle imeshp
  integer comm
#endif

  ! local variables
  integer comm_sz, comm_rank, numa, numo, iv, ie
  TYPE(C_PTR) :: statsPtr
  integer, allocatable, target :: stats(:)
  iBase_TagHandle tagh
  integer i
  iBase_EntityHandle, pointer :: verts(:), ents(:)
  iBase_EntityHandle, allocatable :: conn(:)
  iBase_EntitySetHandle root_set
#ifdef USE_MPI
  IBASE_HANDLE_T mpi_comm_c
  TYPE(C_PTR) :: partsPtr
  iMeshP_PartHandle, pointer :: parts(:)
  iMeshP_PartHandle part
  integer partsa, partso
#endif

  ! create the Mesh instance
  if (imesh .eq. 0) then
     call iMesh_newMesh("MOAB", imesh, ierr)
  end if

#ifdef USE_MPI
  if (imeshp .eq. 0) then
     call iMeshP_getCommunicator(%VAL(imesh), MPI_COMM_WORLD, mpi_comm_c, ierr)
     call iMeshP_createPartitionAll(%VAL(imesh), %VAL(mpi_comm_c), imeshp, ierr)
     call iMeshP_createPart(%VAL(imesh), %VAL(imeshp), part, ierr)
  else 
     partsa = 0
     call iMeshP_getLocalParts(%VAL(imesh), %VAL(imeshp), partsPtr, partsa, partso, ierr)
     call c_f_pointer(partsPtr, parts, [partso])
     part = parts(1)
  end if
  call MPI_COMM_RANK(comm, comm_rank, ierr)
  call MPI_COMM_SIZE(comm, comm_sz, ierr)
#endif

  ! create the vertices, all in one call
  numa = 0
  call iMesh_createVtxArr(%VAL(imesh), %VAL(numv), %VAL(iBase_INTERLEAVED), posn, %VAL(3*numv), &
       vertsPtr, numa, numo, ierr)

  ! fill in the connectivity array, based on indexing from iconn
  allocate (conn(0:nvpe*nume-1))
  call c_f_pointer(vertsPtr, verts, [numv])
  do i = 0, nvpe*nume-1
     conn(i) = verts(1+iconn(i))
  end do
  ! create the elements
  numa = 0
  allocate(stats(0:nume-1))
  statsPtr = C_LOC(stats(0))
  call iMesh_createEntArr(%VAL(imesh), %VAL(tp), conn, %VAL(nvpe*nume), &
       entsPtr, numa, numo, statsPtr, numa, numo, ierr)
  deallocate(stats)
  deallocate(conn)

#ifdef USE_MPI
  ! take care of parallel stuff

  ! add entities to part, using iMesh
  call c_f_pointer(entsPtr, ents, [numo])
  call iMesh_addEntArrToSet(%VAL(imesh), ents, %VAL(numo), %VAL(part), ierr)
  ! set global ids on vertices, needed for sharing between procs
  call iMesh_getTagHandle(%VAL(imesh), "GLOBAL_ID", tagh, ierr, %VAL(9))
  if (iBase_SUCCESS .ne. ierr) then
     ! didn't get handle, need to create the tag
     call iMesh_createTag(%VAL(imesh), "GLOBAL_ID", %VAL(iBase_INTEGER), tagh, ierr, %VAL(9))
  end if
  call iMesh_setIntArrData(%VAL(imesh), verts, %VAL(numv), %VAL(tagh), vgids, %VAL(numv), ierr)

  ! now resolve shared verts and exchange ghost cells
  call iMeshP_syncMeshAll(%VAL(imesh), %VAL(imeshp), ierr)
  call iMesh_getRootSet(%VAL(imesh), root_set, ierr)
  call iMeshP_getNumOfTypeAll(%VAL(imesh), %VAL(imeshp), %VAL(root_set), %VAL(iBase_VERTEX), iv, ierr)
  call iMeshP_getNumOfTypeAll(%VAL(imesh), %VAL(imeshp), %VAL(root_set), %VAL(iBase_FACE), ie, ierr)
  if (comm_rank .eq. 0) then
     write(0,*) "After syncMeshAll:"
     write(0,*) "   Number of vertices = ", iv
     write(0,*) "   Number of entities = ", ie
  endif

  call iMeshP_createGhostEntsAll(%VAL(imesh), %VAL(imeshp), %VAL(2), %VAL(1), %VAL(1), %VAL(0), ierr)
  if (comm_rank .eq. 0) then
     write(0,*) "After createGhostEntsAll:"
     write(0,*) "   Number of vertices = ", iv
     write(0,*) "   Number of entities = ", ie
  endif
#endif

  return
end subroutine create_mesh
