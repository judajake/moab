#include "ReadNC.hpp"
#include "NCHelper.hpp"

#include "moab/ReadUtilIface.hpp"
#include "MBTagConventions.hpp"
#include "moab/FileOptions.hpp"

#define ERRORR(rval, str) \
  if (MB_SUCCESS != rval) { readMeshIface->report_error("%s", str); return rval; }

#define ERRORS(err, str) \
  if (err) { readMeshIface->report_error("%s", str); return MB_FAILURE; }

namespace moab {

ReaderIface* ReadNC::factory(Interface* iface)
{
  return new ReadNC(iface);
}

ReadNC::ReadNC(Interface* impl) :
  mbImpl(impl), fileId(-1), mGlobalIdTag(0), mpFileIdTag(NULL), dbgOut(stderr), isParallel(false),
  partMethod(ScdParData::ALLJORKORI), scdi(NULL),
#ifdef USE_MPI
  myPcomm(NULL),
#endif
  noMesh(false), noVars(false), spectralMesh(false), noMixedElements(false), noEdges(false),
  gatherSetRank(-1), myHelper(NULL)
{
  assert(impl != NULL);
  impl->query_interface(readMeshIface);
}

ReadNC::~ReadNC()
{
  mbImpl->release_interface(readMeshIface);
  if (myHelper != NULL)
    delete myHelper;
}

ErrorCode ReadNC::load_file(const char* file_name, const EntityHandle* file_set, const FileOptions& opts,
                            const ReaderIface::SubsetList* /*subset_list*/, const Tag* file_id_tag)
{
  ErrorCode rval = MB_SUCCESS;

  // See if opts has variable(s) specified
  std::vector<std::string> var_names;
  std::vector<int> tstep_nums;
  std::vector<double> tstep_vals;

  // Get and cache predefined tag handles
  int dum_val = 0;
  rval = mbImpl->tag_get_handle(GLOBAL_ID_TAG_NAME, 1, MB_TYPE_INTEGER, mGlobalIdTag, MB_TAG_DENSE | MB_TAG_CREAT, &dum_val);
  if (MB_SUCCESS != rval)
    return rval;

  // Store the pointer to the tag; if not null, set when global id tag
  // is set too, with the same data, duplicated
  mpFileIdTag = file_id_tag;

  rval = parse_options(opts, var_names, tstep_nums, tstep_vals);
  ERRORR(rval, "Trouble parsing option string.");

  // Open the file
  dbgOut.tprintf(1, "Opening file %s\n", file_name);
  fileName = std::string(file_name);
  int success;

#ifdef PNETCDF_FILE
  if (isParallel)
    success = NCFUNC(open)(myPcomm->proc_config().proc_comm(), file_name, 0, MPI_INFO_NULL, &fileId);
  else
    success = NCFUNC(open)(MPI_COMM_SELF, file_name, 0, MPI_INFO_NULL, &fileId);
#else
  success = NCFUNC(open)(file_name, 0, &fileId);
#endif

  ERRORS(success, "Trouble opening file.");

  // Read the header (num dimensions, dimensions, num variables, global attribs)
  rval = read_header();
  ERRORR(rval, "Trouble reading file header.");

  // Make sure there's a file set to put things in
  EntityHandle tmp_set;
  if (noMesh && !file_set) {
    ERRORR(MB_FAILURE, "NOMESH option requires non-NULL file set on input.\n");
  }
  else if (!file_set || (file_set && *file_set == 0)) {
    rval = mbImpl->create_meshset(MESHSET_SET, tmp_set);
    ERRORR(rval, "Trouble creating file set.");
  }
  else
    tmp_set = *file_set;

  // Get the scd interface
  scdi = NULL;
  rval = mbImpl->query_interface(scdi);
  if (NULL == scdi)
    return MB_FAILURE;

  if (NULL != myHelper)
    delete myHelper;

  // Get appropriate NC helper instance based on information read from the header
  myHelper = NCHelper::get_nc_helper(this, fileId, opts, tmp_set);
  if (NULL == myHelper) {
    ERRORR(MB_FAILURE, "Failed to get NCHelper class instance.");
  }

  // Initialize mesh values
  rval = myHelper->init_mesh_vals();
  ERRORR(rval, "Trouble initializing mesh values.");

  // Check existing mesh from last read
  if (noMesh && !noVars) {
    rval = myHelper->check_existing_mesh();
    ERRORR(rval, "Trouble checking mesh from last read.\n");
  }

  // Create mesh vertex/edge/face sequences
  Range faces;
  if (!noMesh) {
    rval = myHelper->create_mesh(faces);
    ERRORR(rval, "Trouble creating mesh.");
  }

  // Read variables onto grid
  if (!noVars) {
    rval = myHelper->read_variables(var_names, tstep_nums);
    if (MB_FAILURE == rval)
      return rval;
  }
  else {
    // Read dimension variables by default (the dimensions that are also variables)
    std::vector<std::string> dim_var_names;
    for (unsigned int i = 0; i < dimNames.size(); i++) {
      std::map<std::string, VarData>::iterator mit = varInfo.find(dimNames[i]);
      if (mit != varInfo.end())
        dim_var_names.push_back(dimNames[i]);
    }

    if (!dim_var_names.empty()) {
      rval = myHelper->read_variables(dim_var_names, tstep_nums);
      if (MB_FAILURE == rval)
        return rval;
    }
  }

#ifdef USE_MPI
  // Create partition set, and populate with elements
  if (isParallel) {
    EntityHandle partn_set;
    rval = mbImpl->create_meshset(MESHSET_SET, partn_set);
    ERRORR(rval, "Trouble creating partition set.");

    rval = mbImpl->add_entities(partn_set, faces);
    ERRORR(rval, "Couldn't add new faces to partition set.");

    Range verts;
    rval = mbImpl->get_connectivity(faces, verts);
    ERRORR(rval, "Couldn't get verts of faces");

    rval = mbImpl->add_entities(partn_set, verts);
    ERRORR(rval, "Couldn't add new verts to partition set.");

    myPcomm->partition_sets().insert(partn_set);

    // Write partition tag name on partition set
    Tag part_tag = myPcomm->partition_tag();
    int dum_rank = myPcomm->proc_config().proc_rank();
    rval = mbImpl->tag_set_data(part_tag, &partn_set, 1, &dum_rank);
    if (MB_SUCCESS != rval)
      return rval;
  }
#endif

  // Create NC conventional tags when loading header info only
  if (noMesh && noVars) {
    rval = myHelper->create_conventional_tags(tstep_nums);
    ERRORR(rval, "Trouble creating NC conventional tags.");
  }

  mbImpl->release_interface(scdi);
  scdi = NULL;

  // Close the file
  success = NCFUNC(close)(fileId);
  ERRORS(success, "Trouble closing file.");

  return MB_SUCCESS;
}

ErrorCode ReadNC::parse_options(const FileOptions& opts, std::vector<std::string>& var_names, std::vector<int>& tstep_nums,
                                std::vector<double>& tstep_vals)
{
  int tmpval;
  if (MB_SUCCESS == opts.get_int_option("DEBUG_IO", 1, tmpval)) {
    dbgOut.set_verbosity(tmpval);
    dbgOut.set_prefix("NC ");
  }

  ErrorCode rval = opts.get_strs_option("VARIABLE", var_names);
  if (MB_TYPE_OUT_OF_RANGE == rval)
    noVars = true;
  else
    noVars = false;
  opts.get_ints_option("TIMESTEP", tstep_nums);
  opts.get_reals_option("TIMEVAL", tstep_vals);
  rval = opts.get_null_option("NOMESH");
  if (MB_SUCCESS == rval)
    noMesh = true;

  rval = opts.get_null_option("SPECTRAL_MESH");
  if (MB_SUCCESS == rval)
    spectralMesh = true;

  rval = opts.get_null_option("NO_MIXED_ELEMENTS");
  if (MB_SUCCESS == rval)
    noMixedElements = true;

  rval = opts.get_null_option("NO_EDGES");
  if (MB_SUCCESS == rval)
    noEdges = true;

  if (2 <= dbgOut.get_verbosity()) {
    if (!var_names.empty()) {
      std::cerr << "Variables requested: ";
      for (unsigned int i = 0; i < var_names.size(); i++)
        std::cerr << var_names[i];
      std::cerr << std::endl;
    }
    if (!tstep_nums.empty()) {
      std::cerr << "Timesteps requested: ";
      for (unsigned int i = 0; i < tstep_nums.size(); i++)
        std::cerr << tstep_nums[i];
      std::cerr << std::endl;
    }
    if (!tstep_vals.empty()) {
      std::cerr << "Time vals requested: ";
      for (unsigned int i = 0; i < tstep_vals.size(); i++)
        std::cerr << tstep_vals[i];
      std::cerr << std::endl;
    }
  }

  rval = opts.get_int_option("GATHER_SET", 0, gatherSetRank);
  if (MB_TYPE_OUT_OF_RANGE == rval) {
    readMeshIface->report_error("Invalid value for GATHER_SET option");
    return rval;
  }

#ifdef USE_MPI
  isParallel = (opts.match_option("PARALLEL","READ_PART") != MB_ENTITY_NOT_FOUND);

  if (!isParallel)
  // Return success here, since rval still has _NOT_FOUND from not finding option
  // in this case, myPcomm will be NULL, so it can never be used; always check for isParallel 
  // before any use for myPcomm
    return MB_SUCCESS;

  int pcomm_no = 0;
  rval = opts.get_int_option("PARALLEL_COMM", pcomm_no);
  if (rval == MB_TYPE_OUT_OF_RANGE) {
    readMeshIface->report_error("Invalid value for PARALLEL_COMM option");
    return rval;
  }
  myPcomm = ParallelComm::get_pcomm(mbImpl, pcomm_no);
  if (0 == myPcomm) {
    myPcomm = new ParallelComm(mbImpl, MPI_COMM_WORLD);
  }
  const int rank = myPcomm->proc_config().proc_rank();
  dbgOut.set_rank(rank);

  int dum;
  rval = opts.match_option("PARTITION_METHOD", ScdParData::PartitionMethodNames, dum);
  if (rval == MB_FAILURE) {
    readMeshIface->report_error("Unknown partition method specified.");
    partMethod = ScdParData::ALLJORKORI;
  }
  else if (rval == MB_ENTITY_NOT_FOUND)
    partMethod = ScdParData::ALLJORKORI;
  else
    partMethod = dum;
#endif

  return MB_SUCCESS;
}

ErrorCode ReadNC::read_header()
{
  dbgOut.tprint(1, "Reading header...\n");

  // Get the global attributes
  int numgatts;
  int success;
  success = NCFUNC(inq_natts )(fileId, &numgatts);
  ERRORS(success, "Couldn't get number of global attributes.");

  // Read attributes into globalAtts
  ErrorCode result = get_attributes(NC_GLOBAL, numgatts, globalAtts);
  ERRORR(result, "Getting attributes.");
  dbgOut.tprintf(1, "Read %u attributes\n", (unsigned int) globalAtts.size());

  // Read in dimensions into dimNames and dimLens
  result = get_dimensions(fileId, dimNames, dimLens);
  ERRORR(result, "Getting dimensions.");
  dbgOut.tprintf(1, "Read %u dimensions\n", (unsigned int) dimNames.size());

  // Read in variables into varInfo
  result = get_variables();
  ERRORR(result, "Getting variables.");
  dbgOut.tprintf(1, "Read %u variables\n", (unsigned int) varInfo.size());

  return MB_SUCCESS;
}

ErrorCode ReadNC::get_attributes(int var_id, int num_atts, std::map<std::string, AttData>& atts, const char* prefix)
{
  char dum_name[120];

  for (int i = 0; i < num_atts; i++) {
    // Get the name
    int success = NCFUNC(inq_attname)(fileId, var_id, i, dum_name);
    ERRORS(success, "Trouble getting attribute name.");

    AttData &data = atts[std::string(dum_name)];
    data.attName = std::string(dum_name);
    success = NCFUNC(inq_att)(fileId, var_id, dum_name, &data.attDataType, &data.attLen);
    ERRORS(success, "Trouble getting attribute info.");
    data.attVarId = var_id;

    dbgOut.tprintf(2, "%sAttribute %s: length=%u, varId=%d, type=%d\n", (prefix ? prefix : ""), data.attName.c_str(),
        (unsigned int) data.attLen, data.attVarId, data.attDataType);
  }

  return MB_SUCCESS;
}

ErrorCode ReadNC::get_dimensions(int file_id, std::vector<std::string>& dim_names, std::vector<int>& dim_lens)
{
  // Get the number of dimensions
  int num_dims;
  int success = NCFUNC(inq_ndims)(file_id, &num_dims);
  ERRORS(success, "Trouble getting number of dimensions.");

  if (num_dims > NC_MAX_DIMS) {
    readMeshIface->report_error("ReadNC: File contains %d dims but NetCDF library supports only %d\n", num_dims, (int) NC_MAX_DIMS);
    return MB_FAILURE;
  }

  char dim_name[NC_MAX_NAME + 1];
  NCDF_SIZE dim_len;
  dim_names.resize(num_dims);
  dim_lens.resize(num_dims);

  for (int i = 0; i < num_dims; i++) {
    success = NCFUNC(inq_dim)(file_id, i, dim_name, &dim_len);
    ERRORS(success, "Trouble getting dimension info.");

    dim_names[i] = std::string(dim_name);
    dim_lens[i] = dim_len;

    dbgOut.tprintf(2, "Dimension %s, length=%u\n", dim_name, (unsigned int) dim_len);
  }

  return MB_SUCCESS;
}

ErrorCode ReadNC::get_variables()
{
  // First cache the number of time steps
  std::vector<std::string>::iterator vit = std::find(dimNames.begin(), dimNames.end(), "time");
  if (vit == dimNames.end())
    vit = std::find(dimNames.begin(), dimNames.end(), "t");

  int ntimes = 0;
  if (vit != dimNames.end())
    ntimes = dimLens[vit - dimNames.begin()];
  if (!ntimes)
    ntimes = 1;

  // Get the number of variables
  int num_vars;
  int success = NCFUNC(inq_nvars)(fileId, &num_vars);
  ERRORS(success, "Trouble getting number of variables.");

  if (num_vars > NC_MAX_VARS) {
    readMeshIface->report_error("ReadNC: File contains %d vars but NetCDF library supports only %d\n", num_vars, (int) NC_MAX_VARS);
    return MB_FAILURE;
  }

  char var_name[NC_MAX_NAME + 1];
  int var_ndims;

  for (int i = 0; i < num_vars; i++) {
    // Get the name first, so we can allocate a map iterate for this var
    success = NCFUNC(inq_varname )(fileId, i, var_name);
    ERRORS(success, "Trouble getting var name.");
    VarData &data = varInfo[std::string(var_name)];
    data.varName = std::string(var_name);
    data.varId = i;
    data.varTags.resize(ntimes, 0);

    // Get the data type
    success = NCFUNC(inq_vartype)(fileId, i, &data.varDataType);
    ERRORS(success, "Trouble getting variable data type.");

    // Get the number of dimensions, then the dimensions
    success = NCFUNC(inq_varndims)(fileId, i, &var_ndims);
    ERRORS(success, "Trouble getting number of dims of a variable.");
    data.varDims.resize(var_ndims);

    success = NCFUNC(inq_vardimid)(fileId, i, &data.varDims[0]);
    ERRORS(success, "Trouble getting variable dimensions.");

    // Finally, get the number of attributes, then the attributes
    success = NCFUNC(inq_varnatts)(fileId, i, &data.numAtts);
    ERRORS(success, "Trouble getting number of dims of a variable.");

    // Print debug info here so attribute info comes afterwards
    dbgOut.tprintf(2, "Variable %s: Id=%d, numAtts=%d, datatype=%d, num_dims=%u\n", data.varName.c_str(), data.varId, data.numAtts,
        data.varDataType, (unsigned int) data.varDims.size());

    ErrorCode rval = get_attributes(i, data.numAtts, data.varAtts, "   ");
    ERRORR(rval, "Trouble getting attributes for a variable.");
  }

  return MB_SUCCESS;
}

ErrorCode ReadNC::read_tag_values(const char*, const char*, const FileOptions&, std::vector<int>&, const SubsetList*)
{
  return MB_FAILURE;
}

} // namespace moab
