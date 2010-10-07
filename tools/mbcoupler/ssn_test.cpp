// A test file for Subset Normalization
#include "moab/ParallelComm.hpp"
#include "MBParallelConventions.h"
#include "moab/Core.hpp"
#include "FileOptions.hpp"
#include "ReadParallel.hpp"
#include "Coupler.hpp"
#include "iMesh_extensions.h"
#include "DebugOutput.hpp"
#include "ElemUtil.hpp"
#include <iostream>
#include <iomanip>
#include <cstring>

extern "C" 
{
#include "types.h"
#include "minmax.h"
#include "gs.h"
#include "errmem.h"
#include "sort.h"
#include "tuple_list.h"
}

#include "moab/Types.hpp"
#ifndef IS_BUILDING_MB
#  define IS_BUILDING_MB
#  include "Internals.hpp"
#  undef IS_BUILDING_MB
#else
#  include "Internals.hpp"
#endif

using namespace moab;

bool debug = true;

// -----------copied from iMesh_MOAB.hpp------------------
// Error routines for use with iMesh API

extern "C" const iBase_ErrorType iBase_ERROR_MAP[MB_FAILURE+1];

extern "C" iBase_Error iMesh_LAST_ERROR;

static inline int iMesh_processError( int code, const char* desc ) 
{
  strncpy( iMesh_LAST_ERROR.description, desc,
                sizeof(iMesh_LAST_ERROR.description) );
  iMesh_LAST_ERROR.description[sizeof(iMesh_LAST_ERROR.description)-1] = '\0';
  std::cerr << iMesh_LAST_ERROR.description << " - " << code << std::endl;
  return (iMesh_LAST_ERROR.error_type = (iBase_ErrorType)code);
}

#define ERROR(CODE,MSG) do { int tmperr = iMesh_setLastError( mbi, (CODE), (MSG) ); MPI_Finalize(); return tmperr; } while(false)

#define IBASE_ERROR(CODE,MSG) do { int tmperr = iMesh_processError( (CODE), (MSG) ); return tmperr; } while(false)

static inline int iMesh_setLastError( Interface*, int code, const char* msg )
  { return iMesh_processError( code, msg ); }  

static inline int iMesh_setLastError( Interface* mbi, ErrorCode code, const char* msg )
  { 
    std::string message(msg);
    message += "  (MOAB Error Code: ";
    message += mbi->get_error_string(code);
    message += ")";
    return iMesh_processError( iBase_ERROR_MAP[code], message.c_str() ); 
  }

#define CHKERR(CODE, MSG) \
  if (iMesh_isError((CODE))) ERROR((CODE),(MSG))

static inline bool iMesh_isError(int code)
  { return (iBase_SUCCESS != code); }

static inline bool iMesh_isError(ErrorCode code)
  { return (MB_SUCCESS != code); }
// -------------------------------------------------------


// Forward declarations
void get_file_options(int argc, char **argv, 
                      std::vector<const char *> &filenames,
                      std::string &norm_tag,
                      std::vector<const char *> &tag_names,
                      std::vector<const char *> &tag_values,
                      std::string &file_opts,
                      int *err);

void print_tuples(tuple_list *tlp);

int print_vertex_fields(Interface* mbi,
                        iMesh_Instance iMeshInst,
                        std::vector< std::vector<iBase_EntityHandle> > &groups,
                        iBase_TagHandle                                &norm_hdl,
                        Coupler::IntegType                             integ_type);

double const_field(double x, double y, double z);
double field_1(double x, double y, double z);
double field_2(double x, double y, double z);
double field_3(double x, double y, double z);
double physField(double x, double y, double z);

int pack_tuples(tuple_list *tl, void **ptr);
void unpack_tuples(void *ptr, tuple_list** tlp);

//
// Start of main test program
//
int main(int argc, char **argv) {
  // need to init MPI first, to tell how many procs and rank
  // Used since Coupler is a parallel code.  The Coupler may be run
  // in parallel or serial mode but will need MPI either way.
  int err = MPI_Init(&argc, &argv);

  // Print usage if not enough arguments
  if (argc < 3) {
    std::cerr << "Usage: ";
    std::cerr << argv[0] << " <nfiles> <fname1> ... <fnamen> <norm_tag> <tag_select_opts> <file_opts>" << std::endl;
    std::cerr << "nfiles          : number of mesh files" << std::endl;
    std::cerr << "fname1...fnamen : mesh files" << std::endl;
    std::cerr << "norm_tag        : name of tag to normalize across meshes" << std::endl;
    std::cerr << "tag_select_opts : quoted string of tags and values for subset selection, e.g. \"TAG1=VAL1;TAG2=VAL2;TAG3;TAG4\"" << std::endl;
    std::cerr << "file_opts       : quoted string of parallel file read options, e.g. \"OPTION1=VALUE1;OPTION2;OPTION3=VALUE3\"" << std::endl;

    err = MPI_Finalize();
    
    return 1;
  }

  int nprocs, rank;
  err = MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
  err = MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  // Create the moab instance
  Interface *mbi = new Core();
  if (NULL == mbi) {
    std::cerr << "MOAB constructor failed" << std::endl;
    return 1;
  }
  
  // Get an iMesh_Instance from the Core Interface.
  iMesh_Instance iMeshInst = reinterpret_cast<iMesh_Instance>(mbi);

  // Get the input options
  std::cout << "Getting options..." << std::endl;
  std::vector<const char *> filenames;
  std::vector<const char *> tagNames;
  std::vector<const char *> tagValues;
  std::string normTag, fileOpts;
  get_file_options(argc, argv, filenames, normTag, tagNames, tagValues, fileOpts, &err);
  CHKERR(err, "get_file_options failed");

  // Print out the input parameters
  std::cout << "    Input Parameters - " << std::endl;
  std::cout << "      Filenames: ";
  for (std::vector<const char *>::iterator it = filenames.begin(); it != filenames.end(); it++)
    std::cout << *it << " ";
  std::cout << std::endl;
  std::cout << "      Norm Tag: " << normTag << std::endl;
  std::cout << "      Selection Data: NumNames=" << tagNames.size() << " NumValues=" << tagValues.size() << std::endl;
  std::cout << "                      TagNames             TagValues           " << std::endl;
  std::cout << "                      -------------------- --------------------" << std::endl;
  std::vector<const char *>::iterator nameIt = tagNames.begin();
  std::vector<const char *>::iterator valIt = tagValues.begin();
  std::cout << std::setiosflags(std::ios::left);
  for (; nameIt != tagNames.end(); nameIt++) {
    std::cout << "                      " << std::setw(20) << *nameIt;
    if (*valIt != 0) {
      std::cout << " " << std::setw(20) << *((int*)(*valIt)) << std::endl;
      valIt++;
    }
    else
      std::cout << " NULL                " << std::endl;
  }
  std::cout << std::resetiosflags(std::ios::left);
  std::cout << "      File Options: " << fileOpts << std::endl;

  // Read in mesh(es)
  std::cout << "Reading mesh file(s)..." << std::endl;
  std::vector<ParallelComm *> pcs(filenames.size()); 
  std::vector<ReadParallel *> rps(filenames.size()); 

  // Create root sets for each mesh using the iMesh API.  Then pass these
  // to the load_file functions to be populated.
  iBase_EntitySetHandle *roots = (iBase_EntitySetHandle *) malloc(sizeof(iBase_EntitySetHandle) * filenames.size());

  ErrorCode result;
  for (unsigned int i = 0; i < filenames.size(); i++) {
    pcs[i] = new ParallelComm(mbi);
    rps[i] = new ReadParallel(mbi, pcs[i]);
    
    iMesh_createEntSet(iMeshInst, 0, &(roots[i]), &err);
    CHKERR(err, "Creating root set failed");
    result = rps[i]->load_file(filenames[i], (EntityHandle *)&roots[i], FileOptions(fileOpts.c_str()));
    CHKERR(result, "iMeshInstance::load_file failed");
  }

  // Initialize the debug object for Range printing
  DebugOutput debugOut("ssn_test-", std::cerr);
  debugOut.set_rank(rank);
  debugOut.set_verbosity(10);

  // Output what is in root sets
  for (unsigned int k = 0; k < filenames.size(); k++) {
    iBase_EntityHandle *rootEnts = NULL;
    int rootEntsAlloc = 0;
    int rootEntsSize = 0;
    err = 0;
  
    iMesh_getEntities(iMeshInst, roots[k], iBase_ALL_TYPES, iMesh_ALL_TOPOLOGIES,
                      &rootEnts, &rootEntsAlloc, &rootEntsSize, &err);
    Range rootRg;
    for (int j = 0; j < rootEntsSize; j++)
      rootRg.insert((EntityHandle) rootEnts[j]);
    debugOut.print(2, "Root set entities: ", rootRg);
    rootRg.clear();

    Range partRg;
    pcs[k]->get_part_entities(partRg);
    debugOut.print(2, "Partition entities: ", partRg);
    partRg.clear();
  }

  // source is 1st mesh, target is 2nd mesh
  Range src_elems, targ_elems;

  // Create a coupler
  std::cout << "Creating Coupler..." << std::endl;
  Coupler mbc(mbi, pcs[0], src_elems, 0);

  // Get tag handles for passed in tags
  std::cout << "Getting tag handles..." << std::endl;
  int numTagNames = tagNames.size();
  err = iBase_SUCCESS;
  std::vector<iBase_TagHandle> tagHandles(numTagNames);
  int i = 0;
  while (i < numTagNames) {
    std::cout << "Getting handle for " << tagNames[i] << std::endl;
    iMesh_getTagHandle(iMeshInst, tagNames[i], &tagHandles[i], &err, strlen(tagNames[i]));
    CHKERR(err, "Retrieving tag handles failed");
    i++;
  }

  std::vector< std::vector<iBase_EntitySetHandle> > m1EntitySets;
  std::vector< std::vector<iBase_EntityHandle> > m1EntityGroups;
  std::vector< std::vector<iBase_EntitySetHandle> > m2EntitySets;
  std::vector< std::vector<iBase_EntityHandle> > m2EntityGroups;

  // ********** Test get_matching_entsets **********
  // Get matching entities for Mesh 1
  std::cout << "Get matching entities for mesh 1..." << std::endl;
  err = mbc.get_matching_entities(roots[0], &tagHandles[0], &tagValues[0], tagHandles.size(),
                                  &m1EntitySets, &m1EntityGroups);
  CHKERR(err, "get_matching_entities failed");

  std::cout << "    get_matching_entities returned " << m1EntityGroups.size() << " entity groups" << std::endl;
  
  // Print out the data in the vector of vectors
  std::vector< std::vector<iBase_EntityHandle> >::iterator iter_i;
  std::vector<iBase_EntityHandle>::iterator iter_j;
  Range entSetRg;
  int icnt;
  for (iter_i = m1EntityGroups.begin(), icnt = 1; iter_i != m1EntityGroups.end(); iter_i++, icnt++) {
    std::cout << "      Group(" << icnt << ") = ";
    std::cout.flush();
    entSetRg.clear();
    for (iter_j = (*iter_i).begin(); iter_j != (*iter_i).end(); iter_j++)
      entSetRg.insert((EntityHandle) *iter_j);
    debugOut.print(2, "Mesh1 matching EntitySets: ", entSetRg);
    std::cout.flush();
  }

  // Get matching entities for Mesh 2
  std::cout << "Get matching entities for mesh 2..." << std::endl;
  err = mbc.get_matching_entities(roots[1], &tagHandles[0], &tagValues[0], tagHandles.size(),
                                  &m2EntitySets, &m2EntityGroups);
  CHKERR(err, "get_matching_entities failed");

  std::cout << "    get_matching_entities returned " << m2EntityGroups.size() << " entity groups" << std::endl;
  for (iter_i = m2EntityGroups.begin(), icnt = 1; iter_i != m2EntityGroups.end(); iter_i++, icnt++) {
    std::cout << "      Group(" << icnt << ") = ";
    std::cout.flush();
    entSetRg.clear();
    for (iter_j = (*iter_i).begin(); iter_j != (*iter_i).end(); iter_j++)
      entSetRg.insert((EntityHandle) *iter_j);
    debugOut.print(2, "Mesh2 matching EntitySets: ", entSetRg);
    std::cout.flush();
  }

//   // ********** Test create_tuples **********
//   // Create tuple_list for each mesh's
//   std::cout << "Creating tuples for mesh 1..." << std::endl;
//   tuple_list *m1TagTuples = NULL;
//   err = mbc.create_tuples(m1EntSets, m1EntSetsSize, 
//                           &tagHandles[0], tagHandles.size(), &m1TagTuples);
//   CHKERR(err, "create_tuples failed");

//   std::cout << "   create_tuples returned" << std::endl;
//   print_tuples(m1TagTuples);

//   std::cout << "Creating tuples for mesh 2..." << std::endl;
//   tuple_list *m2TagTuples = NULL;
//   err = mbc.create_tuples(m2EntSets, m2EntSetsSize, 
//                           &tagHandles[0], tagHandles.size(), &m2TagTuples);
//   CHKERR(err, "create_tuples failed");

//   std::cout << "   create_tuples returned" << std::endl;
//   print_tuples(m2TagTuples);

//   // ********** Test consolidate_tuples **********
//   // In this serial version we only have the tuples from Mesh 1 and Mesh 2.
//   // Just consolidate those for the test.
//   std::cout << "Consolidating tuple_lists for Mesh 1 and Mesh 2..." << std::endl;
//   tuple_list **tplp_arr = (tuple_list**) malloc(2*sizeof(tuple_list*));
//   tuple_list *unique_tpl = NULL;
//   tplp_arr[0] = m1TagTuples;
//   tplp_arr[1] = m2TagTuples;

//   err = mbc.consolidate_tuples(tplp_arr, 2, &unique_tpl);
//   CHKERR(err, "consolidate_tuples failed");
//   std::cout << "    consolidate_tuples returned" << std::endl;
//   print_tuples(unique_tpl);

  if (debug) {
    // ********** Test print_tuples **********
    // temporary test funtion
    std::cout << "Testing print_tuples..." << std::endl;

    tuple_list test_tuple;
    int num_ints=3, num_longs=2, num_ulongs=4, num_reals=6, num_rows=10;

    std::cout << "    print of test_tuples zero init..." << std::endl;
    tuple_list_init_max(&test_tuple, 0, 0, 0, 0, 0);
    print_tuples(&test_tuple);

    std::cout << "    print of test_tuples after setting n to 10..." << std::endl;
    test_tuple.n = 10;
    print_tuples(&test_tuple);

    tuple_list_init_max(&test_tuple, num_ints, num_longs, num_ulongs, num_reals, num_rows);
    std::cout << "    print of test_tuples after init..." << std::endl;
    print_tuples(&test_tuple);

    std::cout << "    print of test_tuples after setting n to 10..." << std::endl;
    test_tuple.n = 10;
    print_tuples(&test_tuple);

    for (int i = 0; i < num_rows; i++) {
      int j;
      for (j = 0; j < num_ints; j++)
        test_tuple.vi[i*num_ints + j] = (int) ((j+1)*(i+1));

      for (j = 0; j < num_longs; j++)
        test_tuple.vl[i*num_longs + j] = (int) ((j+1)*(i+1));

      for (j = 0; j < num_ulongs; j++)
        test_tuple.vul[i*num_ulongs + j] = (int) ((j+1)*(i+1));

      for (j = 0; j < num_reals; j++)
        test_tuple.vr[i*num_reals + j] = (int) ((j+1)*(i+1)+(j*0.01));
    }
    std::cout << "    print of test_tuples after filling with data..." << std::endl;
    print_tuples(&test_tuple);

    // ********** Test pack_tuples and unpack_tuples **********
    void *mp_buf;
    int buf_sz;
    if (rank == 0) {
      buf_sz = pack_tuples(&test_tuple, &mp_buf);
    }

    // Send buffer size
    err = MPI_Bcast(&buf_sz, 1, MPI_INT, 0, MPI_COMM_WORLD);

    if (err != MPI_SUCCESS) {
      std::cerr << "MPI_Bcast of buffer size failed" << std::endl;
      return -1;
    }

    // Allocate a buffer in the other procs
    if (rank != 0) {
      mp_buf = malloc(buf_sz*sizeof(uint));
    }

    err = MPI_Bcast(mp_buf, buf_sz*sizeof(uint), MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
    if (err != MPI_SUCCESS) {
      std::cerr << "MPI_Bcast of buffer failed" << std::endl;
      return -1;
    }

    tuple_list *rcv_tuples;
    unpack_tuples(mp_buf, &rcv_tuples);

    std::cout << "    print of rcv_tuples after unpacking from MPI_Bcast..." << std::endl;
    print_tuples(rcv_tuples);
  }

  // Test integration function
  // Create a simple hex centered at 0,0,0 with sides of length 2.
  const CartVect biunit_cube[8] = { CartVect( -1, -1, -1 ),
                                    CartVect(  1, -1, -1 ),
                                    CartVect(  1,  1, -1 ),
                                    CartVect( -1,  1, -1 ),
                                    CartVect( -1, -1,  1 ),
                                    CartVect(  1, -1,  1 ),
                                    CartVect(  1,  1,  1 ),
                                    CartVect( -1,  1,  1 ) };

  const CartVect zerobase_cube[8] = { CartVect( 0, 0, 0 ),
                                      CartVect( 2, 0, 0 ),
                                      CartVect( 2, 2, 0 ),
                                      CartVect( 0, 2, 0 ),
                                      CartVect( 0, 0, 2 ),
                                      CartVect( 2, 0, 2 ),
                                      CartVect( 2, 2, 2 ),
                                      CartVect( 0, 2, 2 ) };

  double field_val = 0.0;
  // Calculate field values at the corners of both cubes
  double bcf[8], bf1[8], bf2[8], bf3[8], zcf[8], zf1[8], zf2[8], zf3[8];
  for (int i = 0; i < 8; i++) {
    bcf[i] = const_field(biunit_cube[i][0], biunit_cube[i][1], biunit_cube[i][2]);
    bf1[i] = field_1(biunit_cube[i][0], biunit_cube[i][1], biunit_cube[i][2]);
    bf2[i] = field_2(biunit_cube[i][0], biunit_cube[i][1], biunit_cube[i][2]);
    bf3[i] = field_3(biunit_cube[i][0], biunit_cube[i][1], biunit_cube[i][2]);

    zcf[i] = const_field(zerobase_cube[i][0], zerobase_cube[i][1], zerobase_cube[i][2]);
    zf1[i] = field_1(zerobase_cube[i][0], zerobase_cube[i][1], zerobase_cube[i][2]);
    zf2[i] = field_2(zerobase_cube[i][0], zerobase_cube[i][1], zerobase_cube[i][2]);
    zf3[i] = field_3(zerobase_cube[i][0], zerobase_cube[i][1], zerobase_cube[i][2]);
  }

  std::cout << "Integrated values:" << std::endl;

  for (int i = 1; i <=5; i++) {
    ElemUtil::integrate_trilinear_hex(biunit_cube, bcf, field_val, i);
    std::cout << "    binunit_cube, const_field(num_pts=" << i << "): field_val=" << field_val << std::endl;
    field_val = 0.0;

    ElemUtil::integrate_trilinear_hex(biunit_cube, bf1, field_val, i);
    std::cout << "    binunit_cube, field_1(num_pts=" << i << "): field_val=" << field_val << std::endl;
    field_val = 0.0;

    ElemUtil::integrate_trilinear_hex(biunit_cube, bf2, field_val, i);
    std::cout << "    binunit_cube, field_2(num_pts=" << i << "): field_val=" << field_val << std::endl;
    field_val = 0.0;

    ElemUtil::integrate_trilinear_hex(biunit_cube, bf3, field_val, i);
    std::cout << "    binunit_cube, field_3(num_pts=" << i << "): field_val=" << field_val << std::endl;
    field_val = 0.0;

    ElemUtil::integrate_trilinear_hex(zerobase_cube, zcf, field_val, i);
    std::cout << "    zerobase_cube, const_field(num_pts=" << i << "): field_val=" << field_val << std::endl;
    field_val = 0.0;

    ElemUtil::integrate_trilinear_hex(zerobase_cube, zf1, field_val, i);
    std::cout << "    zerobase_cube, field_1(num_pts=" << i << "): field_val=" << field_val << std::endl;
    field_val = 0.0;

    ElemUtil::integrate_trilinear_hex(zerobase_cube, zf2, field_val, i);
    std::cout << "    zerobase_cube, field_2(num_pts=" << i << "): field_val=" << field_val << std::endl;
    field_val = 0.0;

    ElemUtil::integrate_trilinear_hex(zerobase_cube, zf3, field_val, i);
    std::cout << "    zerobase_cube, field_3(num_pts=" << i << "): field_val=" << field_val << std::endl;
    field_val = 0.0;
  }

  // Test get_group_integ_vals
  std::cout << "Get group integrated field values..." << std::endl;

  // print the field values at the vertices before change.
  std::cout << "    print vertex field values first:" << std::endl;
  iBase_TagHandle norm_hdl;
  iMesh_getTagHandle(iMeshInst, normTag.c_str(), &norm_hdl, &err, strlen(normTag.c_str()));
  CHKERR(err, "Failed to get tag handle.");

  Coupler::IntegType integ_type = Coupler::VOLUME;
  // Mesh 1 field values
  std::cout << "  Original entity vertex field values (mesh 1): " << std::endl;
  print_vertex_fields(mbi, iMeshInst, m1EntityGroups, norm_hdl, integ_type);

  // Mesh 2 field values
  std::cout << "  Original entity vertex field values (mesh 2): " << std::endl;
  print_vertex_fields(mbi, iMeshInst, m2EntityGroups, norm_hdl, integ_type);

  // Get the field values
  std::vector<double>::iterator iter_ivals;

  std::cout << "Get group integrated field values for mesh 1..." << std::endl;
  std::vector<double> m1IntegVals(m1EntityGroups.size());
  err = mbc.get_group_integ_vals(m1EntityGroups, m1IntegVals, normTag.c_str(), 4, integ_type);
  CHKERR(err, "Failed to get the Mesh 1 group integration values.");
  std::cout << "Mesh 1 integrated field values(" << m1IntegVals.size() << "): ";
  for (iter_ivals = m1IntegVals.begin(); iter_ivals != m1IntegVals.end(); iter_ivals++) {
    std::cout << (*iter_ivals) << " ";
  }
  std::cout << std::endl;

  std::cout << "Get group integrated field values for mesh 2..." << std::endl;
  std::vector<double> m2IntegVals(m2EntityGroups.size());
  err = mbc.get_group_integ_vals(m2EntityGroups, m2IntegVals, normTag.c_str(), 4, integ_type);
  CHKERR(err, "Failed to get the Mesh 2 group integration values.");
  std::cout << "Mesh 2 integrated field values(" << m2IntegVals.size() << "): ";
  for (iter_ivals = m2IntegVals.begin(); iter_ivals != m2IntegVals.end(); iter_ivals++) {
    std::cout << (*iter_ivals) << " ";
  }
  std::cout << std::endl;

  // Test apply_group_norm_factors

  // Make the norm factors by inverting the integration values.
  double val;
  for (unsigned int i = 0; i < m1IntegVals.size(); i++) {
    val = m1IntegVals[i];
    m1IntegVals[i] = 1/val;
  }

  for (unsigned int i = 0; i < m2IntegVals.size(); i++) {
    val = m2IntegVals[i];
    m2IntegVals[i] = 1/val;
  }

  std::cout << "Mesh 1 norm factors(" << m1IntegVals.size() << "): ";
  for (iter_ivals = m1IntegVals.begin(); iter_ivals != m1IntegVals.end(); iter_ivals++) {
    std::cout << (*iter_ivals) << " ";
  }
  std::cout << std::endl;

  std::cout << "Mesh 2 norm factors(" << m2IntegVals.size() << "): ";
  for (iter_ivals = m2IntegVals.begin(); iter_ivals != m2IntegVals.end(); iter_ivals++) {
    std::cout << (*iter_ivals) << " ";
  }
  std::cout << std::endl;

  // Apply the factors and reprint the vertices
  err = mbc.apply_group_norm_factor(m1EntitySets, m1IntegVals, normTag.c_str(), integ_type);
  CHKERR(err, "Failed to apply norm factors to Mesh 1.");

  err = mbc.apply_group_norm_factor(m2EntitySets, m2IntegVals, normTag.c_str(), integ_type);
  CHKERR(err, "Failed to apply norm factors to Mesh 2.");

  // Mesh 1 field values
  std::cout << "  Normalized entity vertex field values (mesh 1): " << std::endl;
  print_vertex_fields(mbi, iMeshInst, m1EntityGroups, norm_hdl, integ_type);

  // Mesh 2 field values
  std::cout << "  Normalized entity vertex field values (mesh 2): " << std::endl;
  print_vertex_fields(mbi, iMeshInst, m2EntityGroups, norm_hdl, integ_type);

  // Now get the integrated values again.  They should all be 1 if this works.
  err = mbc.get_group_integ_vals(m1EntityGroups, m1IntegVals, normTag.c_str(), 4, integ_type);
  CHKERR(err, "Failed to get the Mesh 1 group integration values.");
  std::cout << "Mesh 1 normalized integrated field values(" << m1IntegVals.size() << "): ";
  for (iter_ivals = m1IntegVals.begin(); iter_ivals != m1IntegVals.end(); iter_ivals++) {
    std::cout << (*iter_ivals) << " ";
  }
  std::cout << std::endl;

  err = mbc.get_group_integ_vals(m2EntityGroups, m2IntegVals, normTag.c_str(), 4, integ_type);
  CHKERR(err, "Failed to get the Mesh 2 group integration values.");
  std::cout << "Mesh 2 normalized integrated field values(" << m2IntegVals.size() << "): ";
  for (iter_ivals = m2IntegVals.begin(); iter_ivals != m2IntegVals.end(); iter_ivals++) {
    std::cout << (*iter_ivals) << " ";
  }
  std::cout << std::endl;

  // Now call the Coupler::normalize_subset routine and see if we get an error.
  std::cout << "Running Coupler::normalize_subset() on mesh 1" << std::endl;
  err = mbc.normalize_subset(roots[0], 
                             normTag.c_str(), 
                             &tagNames[0], 
                             numTagNames, 
                             &tagValues[0], 
                             Coupler::VOLUME, 
                             4);
  CHKERR(err, "Failure in call to Coupler::normalize_subset() on mesh 1");

  // Print the field values after the above call.
  // Mesh 1 field values
  std::cout << "  Normalized entity vertex field values (mesh 1): " << std::endl;
  print_vertex_fields(mbi, iMeshInst, m1EntityGroups, norm_hdl, integ_type);

  std::cout << "Running Coupler::normalize_subset() on mesh 2" << std::endl;
  err = mbc.normalize_subset(roots[1], 
                             normTag.c_str(), 
                             &tagNames[0], 
                             numTagNames, 
                             &tagValues[0], 
                             Coupler::VOLUME, 
                             4);
  CHKERR(err, "Failure in call to Coupler::normalize_subset() on mesh 2");

  // Mesh 2 field values
  std::cout << "  Normalized entity vertex field values (mesh 2): " << std::endl;
  print_vertex_fields(mbi, iMeshInst, m2EntityGroups, norm_hdl, integ_type);

  // Cleanup
  MPI_Finalize();
  return 0;
}

// Function to parse input parameters
void get_file_options(int argc, char **argv, 
                      std::vector<const char *> &filenames,
                      std::string &normTag,
                      std::vector<const char *> &tagNames,
                      std::vector<const char *> &tagValues,
                      std::string &fileOpts,
                      int *err)
{
  int npos = 1;

  // get number of files
  int nfiles = atoi(argv[npos++]);
  
  // get mesh filenames
  filenames.resize(nfiles);
  for (int i = 0; i < nfiles; i++) filenames[i] = argv[npos++];

  // get normTag
  if (npos < argc) 
    normTag = argv[npos++];
  else {
    std::cerr << "Insufficient parameters:  norm_tag missing" << std::endl;
    *err = iBase_FAILURE;
    return;
  }

  // get tag selection options
  if (npos < argc) {
    char* opts = argv[npos++];
    char sep1[1] = {';'};
    char sep2[1] = {'='};
    bool end_vals_seen = false;
    std::vector<char *> tmpTagOpts;

    // first get the options
    for (char* i = strtok(opts, sep1); i; i = strtok(0, sep1)) {
      if (debug) std::cout << "get_file_options:  i=" << i << std::endl;
      tmpTagOpts.push_back(i);
    }

    // parse out the name and val or just name.
    for (unsigned int j = 0; j < tmpTagOpts.size(); j++) {
      char* e = strtok(tmpTagOpts[j], sep2);
      if (debug) std::cout << "get_file_options:    name=" << e << std::endl;
      tagNames.push_back(e);
      e = strtok(0, sep2);
      if (e != NULL) {
        if (debug) std::cout << "get_file_options:     val=" << e << std::endl;
        // We have a value
        if (end_vals_seen) {
          // ERROR we should not have a value after none are seen
          std::cerr << "Incorrect parameters:  new value seen after end of values" << std::endl;
          *err = iBase_FAILURE;
          return;
        }
        // Otherwise get the value string from e and convert it to an int
        int *valp = new int;
        *valp = atoi(e);
        tagValues.push_back((const char *) valp);
      }
      else {
        // Otherwise there is no '=' so push a null on the list
        end_vals_seen = true;
        tagValues.push_back((const char *) 0);
      }
    }
  }
  else {
    std::cerr << "Insufficient parameters:  tag_select_opts missing" << std::endl;
    *err = iBase_FAILURE;
    return;
  }

  // get fileOpts
  if (npos < argc) 
    fileOpts = argv[npos++];
  else {
    std::cerr << "Insufficient parameters:  file_opts missing" << std::endl;
    *err = iBase_FAILURE;
    return;
  }
}

// Function to print out a tuple_list.
void print_tuples(tuple_list *tlp)
{
  std::cout << "    tuple data:  (n=" << tlp->n << ")" << std::endl;
  std::cout << "      mi:" << tlp->mi
            << " ml:" << tlp->ml
            << " mul:" << tlp->mul
            << " mr:" << tlp->mr << std::endl;
  std::cout << "      ["
            << std::setw(11*tlp->mi)  << " int data"   << " |"
            << std::setw(11*tlp->ml)  << " long data"  << " |"
            << std::setw(11*tlp->mul) << " ulong data" << " |"
            << std::setw(11*tlp->mr)  << " real data"  << " "
            << std::endl << "        ";
  for (unsigned int i = 0; i < tlp->n; i++) {
    if (tlp->mi >0) {
      for (unsigned int j = 0; j < tlp->mi; j++) {
        std::cout << std::setw(10) << tlp->vi[i*tlp->mi + j] << " ";
      }
    }
    else {
      std::cout << "         ";
    }
    std::cout << "| ";

    if (tlp->ml >0) {
      for (unsigned int j = 0; j < tlp->ml; j++) {
        std::cout << std::setw(10) << tlp->vl[i*tlp->ml + j] << " ";
      }
    }
    else {
      std::cout << "          ";
    }
    std::cout << "| ";

    if (tlp->mul >0) {
      for (unsigned int j = 0; j < tlp->mul; j++) {
        std::cout << std::setw(10) << tlp->vul[i*tlp->mul + j] << " ";
      }
    }
    else {
      std::cout << "           ";
    }
    std::cout << "| ";

    if (tlp->mr >0) {
      for (unsigned int j = 0; j < tlp->mr; j++) {
        std::cout << std::setw(10) << tlp->vr[i*tlp->mr + j] << " ";
      }
    }
    else {
      std::cout << "          ";
    }

    if (i+1 < tlp->n)
      std::cout << std::endl << "        ";
  }
  std::cout << "]" << std::endl;
}

// Function to print vertex field values
int print_vertex_fields(Interface* mbi,
                        iMesh_Instance iMeshInst,
                        std::vector< std::vector<iBase_EntityHandle> > &groups,
                        iBase_TagHandle                                &norm_hdl,
                        Coupler::IntegType                             integ_type)
{
  int err = iBase_SUCCESS;
  std::vector<iBase_EntityHandle>::iterator iter_j;

  for (unsigned int i = 0; i < groups.size(); i++) {
    std::cout << "    Group - " << std::endl << "        ";
    for (iter_j = groups[i].begin(); iter_j != groups[i].end(); iter_j++) {
      // Check that the entity in iter_j is of the same dimension as the 
      // integ_type we are performing
      int j_type;
      iMesh_getEntType(iMeshInst, (*iter_j), &j_type, &err);
      CHKERR(err, "Failed to get entity type.");
      if (((integ_type == Coupler::VOLUME) && (j_type != iBase_REGION)) ||
          ((integ_type == Coupler::AREA)   && (j_type != iBase_FACE)))
        continue;

      // Retrieve the vertices from the element
      iBase_EntityHandle *verts = NULL;
      int vertsAlloc = 0;
      int vertsSize = 0;

      iMesh_getEntAdj(iMeshInst, (*iter_j), iBase_VERTEX, &verts, &vertsAlloc, &vertsSize, &err);
      CHKERR(err, "Failed to get vertices from entity.");
      for (int i = 0; i < vertsSize; i++) {
        double data = 0;
        iMesh_getDblData(iMeshInst, verts[i], norm_hdl, &data, &err);
        CHKERR(err, "Failed to get tag data.");
        std::cout << data << ", ";
      }
      std::cout << std::endl << "        ";
    }
    std::cout << std::endl;
  }

  return err;
}

// Function for a constant field value
double const_field(double x, double y, double z)
{
  //  return 5.0/40.0;
  return 5.0;
}

// Functions for a some field values
double field_1(double x, double y, double z)
{
  double f = fabs(x) + fabs(y) + fabs(z);
  //  return f/24.0;
  return f;
}

double field_2(double x, double y, double z)
{
  double f = x*x + y*y + z*z;
  //  return f/32.0;
  return f;
}

double field_3(double x, double y, double z)
{
  double f = 2*x + 2*y + 2*z;
  //  return f/48.0;
  return f;
}

// Function used to create field on mesh for testing.
double physField(double x, double y, double z)
{
  double out;

  // 1/r^2 decay from {0,0,0}

  out = x*x + y*y + z*z;
  out += 1e-1; // clamp
  out = 1/out;

  return out;
}

#define UINT_PER_X(X) ((sizeof(X)+sizeof(uint)-1)/sizeof(uint))
#define UINT_PER_REAL UINT_PER_X(real)
#define UINT_PER_LONG UINT_PER_X(slong)
#define UINT_PER_UNSIGNED UINT_PER_X(unsigned)

// Function for packing tuple_list
int pack_tuples(tuple_list* tl, void **ptr)
{
  int sz_buf = 1 + 4*UINT_PER_UNSIGNED +
               tl->n * (tl->mi + 
                        tl->ml*UINT_PER_LONG + 
                        tl->mul*UINT_PER_LONG + 
                        tl->mr*UINT_PER_REAL);
  
  uint *buf = (uint*) malloc(sz_buf*sizeof(uint));
  *ptr = (void*) buf;

  // copy n
  memcpy(buf, &(tl->n),   sizeof(uint)),                buf+=1;
  // copy mi
  memcpy(buf, &(tl->mi),  sizeof(unsigned)),            buf+=UINT_PER_UNSIGNED;
  // copy ml
  memcpy(buf, &(tl->ml),  sizeof(unsigned)),            buf+=UINT_PER_LONG;
  // copy mul
  memcpy(buf, &(tl->mul), sizeof(unsigned)),            buf+=UINT_PER_LONG;
  // copy mr
  memcpy(buf, &(tl->mr),  sizeof(unsigned)),            buf+=UINT_PER_REAL;
  // copy vi
  memcpy(buf, tl->vi,     tl->n*tl->mi*sizeof(sint)),   buf+=tl->n*tl->mi;
  // copy vl
  memcpy(buf, tl->vl,     tl->n*tl->ml*sizeof(slong)),  buf+=tl->n*tl->ml*UINT_PER_LONG;
  // copy vul
  memcpy(buf, tl->vul,    tl->n*tl->mul*sizeof(ulong)), buf+=tl->n*tl->mul*UINT_PER_LONG;
  // copy vr
  memcpy(buf, tl->vr,     tl->n*tl->mr*sizeof(real)),   buf+=tl->n*tl->mr*UINT_PER_REAL;

  return sz_buf;
}

// Function for packing tuple_list
void unpack_tuples(void *ptr, tuple_list** tlp)
{
  tuple_list *tl = (tuple_list*) malloc(sizeof(tuple_list));
  *tlp = tl;

  uint nt;
  unsigned mit, mlt, mult, mrt;
  uint *buf = (uint*)ptr;

  // get n
  memcpy(&nt,   buf, sizeof(uint)),          buf+=1;
  // get mi
  memcpy(&mit,  buf, sizeof(unsigned)),      buf+=UINT_PER_UNSIGNED;
  // get ml
  memcpy(&mlt,  buf, sizeof(unsigned)),      buf+=UINT_PER_LONG;
  // get mul
  memcpy(&mult, buf, sizeof(unsigned)),      buf+=UINT_PER_LONG;
  // get mr
  memcpy(&mrt,  buf, sizeof(unsigned)),      buf+=UINT_PER_REAL;

  // initalize tl
  tuple_list_init_max(tl, mit, mlt, mult, mrt, nt);
  tl->n = nt;

  // get vi
  memcpy(tl->vi,     buf, tl->n*tl->mi*sizeof(sint)),   buf+=tl->n*tl->mi;
  // get vl
  memcpy(tl->vl,     buf, tl->n*tl->ml*sizeof(slong)),  buf+=tl->n*tl->ml*UINT_PER_LONG;
  // get vul
  memcpy(tl->vul,    buf, tl->n*tl->mul*sizeof(ulong)), buf+=tl->n*tl->mul*UINT_PER_LONG;
  // get vr
  memcpy(tl->vr,     buf, tl->n*tl->mr*sizeof(real)),   buf+=tl->n*tl->mr*UINT_PER_REAL;

  return;
}

