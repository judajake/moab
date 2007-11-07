#include <time.h>
#include <assert.h>
#include <iostream>
#include <sstream>
#include "MBCore.hpp"
#include "MBReadUtilIface.hpp"

  // constants
const bool dump_mesh = false;        //!< write mesh to vtk file
const int default_intervals = 25;    //!< defaul interval count for cubic structured hex mesh
const int default_query_count = 100; //!< number of times to do each query set
const int default_order[] = {0, 1, 2};
const int default_create[] = {0,1};
const int default_delete[] = {0,10,20,30,40,50,60,70,80,90};

  // input parameters
long numSideInt, numVert, numElem;   //!< total counts;
int queryCount;                      //!< number of times to do each query set

  // misc globals
MBCore moab;                         //!< moab instance
MBInterface& mb = moab;              //!< moab instance
MBEntityHandle vertStart, elemStart; //!< first handle
MBReadUtilIface *readTool = 0;       
double centroid[3];
long* queryVertPermutation = 0;      //!< pupulated by init(): "random" order for vertices
long* queryElemPermutation = 0;      //!< pupulated by init(): "random" order for elements

//! Generate random permutation of values in [0,count-1]
long* permutation( long count )
{
  srand( count );
  long* array = new long[count];
  for (long i = 0; i < count; ++i)
    array[i] = i;
  
  for (long i = 0; i < count; ++i) {
    long r = rand();
    if (count > RAND_MAX) {
      r += RAND_MAX * rand();
      if (count > ((long)RAND_MAX*RAND_MAX))
        r += RAND_MAX*RAND_MAX*rand();
    }
    std::swap( array[i], array[r%count] );
  }
  
  return array;
}

//! Initialize global variables
void init() 
{
  void* ptr;
  MBErrorCode rval = mb.query_interface( "MBReadUtilIface", &ptr );
  readTool = static_cast<MBReadUtilIface*>(ptr);
  assert( !rval && readTool );
 
  queryVertPermutation = permutation( numVert );
  queryElemPermutation = permutation( numElem );
}


void create_vertices_single( ); //!< create vertices one at a time
void create_vertices_block( );  //!< create vertices in block using MBReadUtilIface
void create_elements_single( ); //!< create elements one at a time
void create_elements_block( );  //!< create elements in block using MBReadUtilIface
 
void forward_order_query_vertices(); //!< calculate mean of all vertex coordinates
void reverse_order_query_vertices(); //!< calculate mean of all vertex coordinates
void  random_order_query_vertices(); //!< calculate mean of all vertex coordinates

void forward_order_query_elements(); //!< check all element connectivity for valid vertex handles
void reverse_order_query_elements(); //!< check all element connectivity for valid vertex handles
void  random_order_query_elements();  //!< check all element connectivity for valid vertex handles

void forward_order_query_element_verts(); //!< calculate centroid
void reverse_order_query_element_verts(); //!< calculate centroid
void  random_order_query_element_verts(); //!< calculate centroid

void forward_order_delete_vertices( int percent ); //!< delete x% of vertices
void reverse_order_delete_vertices( int percent ); //!< delete x% of vertices
void  random_order_delete_vertices( int percent ); //!< delete x% of vertices

void forward_order_delete_elements( int percent ); //!< delete x% of elements
void reverse_order_delete_elements( int percent ); //!< delete x% of elements
void  random_order_delete_elements( int percent ); //!< delete x% of elements

void create_missing_vertices( int percent ); //!< re-create deleted vertices
void create_missing_elements( int percent ); //!< re-create deleted elements

/* Build arrays of function pointers, indexed by the order the entities are traversed in */

typedef void (*naf_t)();
typedef void (*iaf_t)(int);

naf_t query_verts[3] = { &forward_order_query_vertices,
                         &reverse_order_query_vertices,
                         & random_order_query_vertices };

naf_t query_elems[3] = { &forward_order_query_elements,
                         &reverse_order_query_elements,
                         & random_order_query_elements };

naf_t query_elem_verts[3] = { &forward_order_query_element_verts,
                              &reverse_order_query_element_verts,
                              & random_order_query_element_verts };

iaf_t delete_verts[3] = { &forward_order_delete_vertices,
                          &reverse_order_delete_vertices,
                          & random_order_delete_vertices };

iaf_t delete_elems[3] = { &forward_order_delete_elements,
                          &reverse_order_delete_elements,
                          & random_order_delete_elements };

const char* order_strs[] = { "Forward", "Reverse", "Random" };

//! Coordinates for ith vertex in structured hex mesh
inline void vertex_coords( long vert_index, double& x, double& y, double& z );
//! Connectivity for ith hex in structured hex mesh
inline void element_conn( long elem_index, MBEntityHandle conn[8] );
//! True if passed index is one of the x% to be deleted
inline bool deleted_vert( long index, int percent );
//! True if passed index is one of the x% to be deleted
inline bool deleted_elem( long index, int percent );
//! if (deleted_vert(index,percent)) delete vertex
inline void delete_vert( long index, int percent );
//! if (deleted_elem(index,percent)) delete element
inline void delete_elem( long index, int percent );

//! print usage and exit
void usage() {
  std::cerr << "Usage: seqperf [-i <intervals>] [-o <order>] [-d <percent>] [-b|-s] [-q <count>]" << std::endl;
  std::cerr << " -i specify size of cubic structured hex mesh in intervals.  Default: " << default_intervals << std::endl;
  std::cerr << " -o one of \"forward\", \"reverse\", or \"random\".  May be specified multiple times.  Default is all." << std::endl;
  std::cerr << " -d percent of entities to delete.  May be specified multiple times.  Default is {0,10,20,...,90}." << std::endl;
  std::cerr << " -b block creation of mesh" << std::endl;
  std::cerr << " -s single entity mesh creation" << std::endl;
  std::cerr << " -q number of times to repeat queries.  Default: " << default_query_count << std::endl;
  exit(1);
}

//! convert CPU time to string
std::string ts( clock_t t )
{
  std::ostringstream s;
  s << ((double)t)/CLOCKS_PER_SEC << 's';
  return s.str();
}

//! run function, printing time spent 
void TIME( const char* str, void (*func)() )
{ 
  std::cout << str << "... " << std::flush; 
  clock_t t = clock();
  (*func)();
  std::cout << ts(clock() - t) << std::endl;
}

//! run function query_repeat times, printing time spent 
void TIME_QRY( const char* str, void (*func)() )
{ 
  std::cout << str << "... " << std::flush; 
  clock_t t = clock();
  for (int i = 0; i < queryCount; ++i)
    (*func)();
  std::cout << ts(clock() - t) << std::endl;
}

//! run function with integer argument, printing time spent 
void TIME_DEL( const char* str, void (*func)(int), int percent )
{ 
  std::cout << str << "... " << std::flush; 
  clock_t t = clock();
  (*func)(percent);
  std::cout << ts(clock() - t) << std::endl;
}

//! call MB::delete_mesh().  function so can be passed to TIME
void delete_mesh()
{
  mb.delete_mesh();
}

//! Run a single combination of test parameters
void do_test( int create_mode, //!< 0 == single, 1 == block
              int order,       //!< 0 == forward, 1 == reverse, 2 == random
              int percent )    //!< percent of entities to delete
{
  clock_t t = clock();
  if (create_mode) {
    std::cout << "Block Entity Creation (all entities in single block of memory)" << std::endl;
    TIME ( "  Creating initial vertices", create_vertices_block );
    TIME ( "  Creating initial elements", create_elements_block );
    if (dump_mesh && !percent && mb.write_file( "seqperf.vtk" ) == MB_SUCCESS) 
      std::cout << "Wrote mesh to file: seqperf.vtk" << std::endl;
  }
  else {
    std::cout << "Single Entity Creation (entities grouped in memory blocks of constant size)" << std::endl;
    TIME ( "  Creating initial vertices", create_vertices_single );
    TIME ( "  Creating initial elements", create_elements_single );
  }
  
  std::cout << order_strs[order] <<
    " order with deletion of " << percent << "% of vertices and elements" << std::endl;

  TIME_DEL( "  Deleting vertices", delete_verts[order], percent );
  TIME_DEL( "  Deleting elements", delete_elems[order], percent );
  
  int num_vert = 0;
  int num_elem = 0;
  mb.get_number_entities_by_type( 0, MBVERTEX, num_vert );
  mb.get_number_entities_by_type( 0, MBHEX,    num_elem );
  std::cout << "  " << num_vert << " vertices and " << num_elem << " elements remaining" << std::endl;

  TIME_QRY( "  Quering vertex coordinates", query_verts[order] );
  TIME_QRY( "  Quering element connectivity", query_elems[order] );
  TIME_QRY( "  Quering element coordinates", query_elem_verts[order] );

  TIME_DEL( "  Re-creating vertices", create_missing_vertices, percent );
  TIME_DEL( "  Re-creating elements", create_missing_elements, percent );

  TIME( "  Clearing mesh instance", delete_mesh );
  
  std::cout << "Total time for test: " << ts(clock()-t) << std::endl << std::endl;
}

void parse_order( const char* str, std::vector<int>& list )
{
  if (str[0] == 'f') {
    if (strncmp( str, "forward", strlen(str) ))
      usage();
    list.push_back( 0 );
  }
  else if (str[0] != 'r') 
    usage();
  else if (str[1] == 'e') {
    if (strncmp( str, "reverse", strlen(str) ))
      usage();
    list.push_back( 0 );
  }
  else {
    if (strncmp( str, "random", strlen(str) ))
      usage();
    list.push_back( 0 );
  }
}

void parse_percent( const char* str, std::vector<int>& list )
{
  char* endptr;
  long p = strtol( str, &endptr, 0 );
  if (!endptr || *endptr || p < 0 || p > 100)
    usage();
  
  list.push_back((int)p);
}

int parse_positive_int( const char* str )
{
  char* endptr;
  long p = strtol( str, &endptr, 0 );
  if (!endptr || *endptr || p < 1)
    usage();
  int result = p;
  if (p != (long)result) // overflow
    usage();
  
  return result;
}

void check_default( std::vector<int>& list, const int* array, size_t array_len )
{
  if (list.empty())
    std::copy( array, array+array_len, std::back_inserter(list) );
}
    
  

#define ARRSIZE(A) (sizeof(A)/sizeof(A[0]))
int main( int argc, char* argv[] )
{
    // Parse arguments
  std::vector<int> createList, orderList, deleteList;
  numSideInt = default_intervals;
  queryCount = default_query_count;
  
  for (int i = 1; i < argc; ++i) {
      // check that arg is a '-' followed by a single character
    if (argv[i][0] != '-' || argv[i][1] == '\0' || argv[i][2] != '\0')
      usage();
  
    const char flag = argv[i][1];
    switch (flag) {
      case 'b': createList.push_back( 1 ); break;
      case 's': createList.push_back( 0 ); break;
      default: 
        if (++i == argc)
          usage();
        switch (flag) {
          case 'i': 
            numSideInt = parse_positive_int( argv[i] );
            break;
          case 'o':
            parse_order( argv[i], orderList );
            break;
          case 'p':
            parse_percent( argv[i], deleteList );
            break;
          case 'q':
            queryCount = parse_positive_int( argv[i] );
            break;
          default:
            usage();
        }
    }
  }
  check_default( createList, default_create, ARRSIZE(default_create) );
  check_default( orderList, default_order, ARRSIZE(default_order) );
  check_default( deleteList, default_delete, ARRSIZE(default_delete) );
  
    // Do some initialization.
  
  int numSideVert = numSideInt + 1;
  numVert = numSideVert * numSideVert * numSideVert;
  numElem = numSideInt * numSideInt * numSideInt;
  if (numVert / numSideVert / numSideVert != numSideVert) // overflow
    usage();
  init();
  
    // Echo input args
  
  std::cout << numSideInt << "x" << numSideInt << "x" << numSideInt << " hex grid: " 
            << numElem << " elements and " << numVert << " vertices" << std::endl;
  
    // Run tests
  
  std::vector<int>::const_iterator i,j,k;
  clock_t t = clock();
  for (std::vector<int>::iterator i = createList.begin(); i != createList.end(); ++i) {
    for (std::vector<int>::iterator j = deleteList.begin(); j != deleteList.end(); ++j) {
      for (std::vector<int>::iterator k = orderList.begin(); k != orderList.end(); ++k) {
        do_test( *i, *k, *j );
      }
    }
  }
  
    // Clean up
  
  std::cout << "TOTAL: " << ts(clock() - t) << std::endl << std::endl;
  delete [] queryVertPermutation;
  delete [] queryElemPermutation;
  return 0;
}


void create_vertices_single( )
{
  double coords[3];
  vertex_coords( 0, coords[0], coords[1], coords[2] );
  MBErrorCode rval = mb.create_vertex( coords, vertStart );
  assert(!rval);
  
  MBEntityHandle h;
  for (long i = 1; i < numVert; ++i) {
    vertex_coords( i, coords[0], coords[1], coords[2] );
    rval = mb.create_vertex( coords, h );
    assert(!rval);
    assert(h - vertStart == (MBEntityHandle)i);
  }
}

void create_vertices_block( )
{
  std::vector<double*> arrays;
  MBErrorCode rval = readTool->get_node_arrays( 3, numVert, 0, 0, vertStart, arrays );
  assert(!rval && arrays.size() == 3);
  double *x = arrays[0], *y = arrays[1], *z = arrays[2];
  assert( x && y && z );
  
  for (long i = 0; i < numVert; ++i) 
    vertex_coords( i, *x++, *y++, *z++ );
}

void create_elements_single( )
{
  MBEntityHandle conn[8];
  element_conn( 0, conn );
  MBErrorCode rval = mb.create_element( MBHEX, conn, 8, elemStart );
  assert(!rval);
  
  MBEntityHandle h;
  for (long i = 1; i < numElem; ++i) {
    element_conn( i, conn );
    rval = mb.create_element( MBHEX, conn, 8, h );
    assert(!rval);
    assert(h - elemStart == (MBEntityHandle)i);
  }
}


void create_elements_block( )
{
  MBEntityHandle* conn = 0;
  MBErrorCode rval = readTool->get_element_array( numElem, 8, MBHEX, 0, 0, elemStart, conn );
  assert(!rval && conn);
  
  for (long i = 0; i < numElem; ++i) 
    element_conn( i, conn + 8*i );
}
 
void forward_order_query_vertices()
{
  double coords[3], sum[3] = {0,0,0};
  const MBEntityHandle last = vertStart + numVert;
  for (MBEntityHandle h = vertStart; h < last; ++h) {
    mb.get_coords( &h, 1, coords );
    sum[0] += coords[0];
    sum[1] += coords[1];
    sum[2] += coords[2];
  }
  
  centroid[0] = sum[0] / numVert;
  centroid[1] = sum[1] / numVert;
  centroid[2] = sum[2] / numVert;
}

void reverse_order_query_vertices()
{
  double coords[3], sum[3] = {0,0,0};
  const MBEntityHandle last = vertStart + numVert;
  for (MBEntityHandle h = last-1; h >= vertStart; --h) {
    mb.get_coords( &h, 1, coords );
    sum[0] += coords[0];
    sum[1] += coords[1];
    sum[2] += coords[2];
  }
  
  centroid[0] = sum[0] / numVert;
  centroid[1] = sum[1] / numVert;
  centroid[2] = sum[2] / numVert;
}

void  random_order_query_vertices()
{
  MBEntityHandle h;
  double coords[3], sum[3] = {0,0,0};
  for (long i = 0; i < numVert; ++i) {
    h = vertStart + queryVertPermutation[i];
    mb.get_coords( &h, 1, coords );
    sum[0] += coords[0];
    sum[1] += coords[1];
    sum[2] += coords[2];
  }
  
  centroid[0] = sum[0] / numVert;
  centroid[1] = sum[1] / numVert;
  centroid[2] = sum[2] / numVert;
}

void forward_order_query_elements()
{
  const MBEntityHandle* conn;
  int len;
  const MBEntityHandle lastVert = vertStart + numVert - 1;
  const MBEntityHandle last = elemStart + numElem;
  for (MBEntityHandle h = elemStart; h < last; ++h) {
    if (MB_SUCCESS == mb.get_connectivity( h, conn, len )) {
      assert( 8 == len );
      for (int j = 0; j < 8; ++j) 
        if (conn[j] < vertStart || conn[j] > lastVert)
          std::cerr << "Invalid vertex handle: " << conn[j] << std::endl;
    }
  }
}

void reverse_order_query_elements()
{
  const MBEntityHandle* conn;
  int len;
  const MBEntityHandle lastVert = vertStart + numVert - 1;
  const MBEntityHandle last = elemStart + numElem;
  for (MBEntityHandle h = last-1; h >= elemStart; --h) {
    if (MB_SUCCESS == mb.get_connectivity( h, conn, len )) {
      assert( 8 == len );
      for (int j = 0; j < 8; ++j) 
        if (conn[j] < vertStart || conn[j] > lastVert)
          std::cerr << "Invalid vertex handle: " << conn[j] << std::endl;
    }
  }
}

void  random_order_query_elements()
{
  const MBEntityHandle* conn;
  int len;
  const MBEntityHandle lastVert = vertStart + numVert - 1;
  for (long i = 0; i < numElem; ++i) {
    MBEntityHandle h = elemStart + queryElemPermutation[i];
    if (MB_SUCCESS == mb.get_connectivity( h, conn, len )) {
      assert( 8 == len );
      for (int j = 0; j < 8; ++j) 
        if (conn[j] < vertStart || conn[j] > lastVert)
          std::cerr << "Invalid vertex handle: " << conn[j] << std::endl;
    }
  }
}


static double hex_centroid( double coords[24], double cent[3] )
{
  double a[3], b[3], c[3], vol;
  cent[0] += 0.125*(coords[0] + coords[3] + coords[6] + coords[ 9] + coords[12] + coords[15] + coords[18] + coords[21]);
  cent[1] += 0.125*(coords[1] + coords[4] + coords[7] + coords[10] + coords[13] + coords[16] + coords[19] + coords[22]);
  cent[2] += 0.125*(coords[2] + coords[5] + coords[8] + coords[11] + coords[14] + coords[17] + coords[20] + coords[23]);
  a[0] = coords[0] + coords[3] + coords[15] + coords[12] - coords[ 9] - coords[6] - coords[18] - coords[21];
  a[1] = coords[1] + coords[4] + coords[16] + coords[13] - coords[10] - coords[7] - coords[19] - coords[22];
  a[2] = coords[2] + coords[5] + coords[17] + coords[14] - coords[11] - coords[8] - coords[20] - coords[23];
  b[0] = coords[0] + coords[ 9] + coords[21] + coords[12] - coords[3] - coords[6] - coords[18] - coords[15];
  b[1] = coords[1] + coords[10] + coords[22] + coords[13] - coords[4] - coords[7] - coords[19] - coords[16];
  b[2] = coords[2] + coords[11] + coords[23] + coords[14] - coords[5] - coords[8] - coords[20] - coords[17];
  c[0] = coords[0] + coords[3] + coords[6] + coords[ 9] - coords[12] - coords[15] - coords[18] - coords[21];
  c[1] = coords[1] + coords[4] + coords[7] + coords[10] - coords[13] - coords[16] - coords[19] - coords[22];
  c[2] = coords[2] + coords[5] + coords[8] + coords[11] - coords[14] - coords[17] - coords[20] - coords[23];
  vol = c[0]*(a[1]*b[2] - a[2]*b[1]) + c[1]*(a[2]*b[0] - a[0]*b[2]) + c[2]*(a[0]*b[1] - a[1]*b[0]);
  return (1./64.) * vol;
}

void forward_order_query_element_verts()
{
  const MBEntityHandle* conn;
  int len;
  const MBEntityHandle last = elemStart + numElem;
  double coords[24];
  double cent[3], vol, sum_vol = 0.0;
  centroid[0] = centroid[1] = centroid[2] = 0.0;
  for (MBEntityHandle h = elemStart; h < last; ++h) {
    if (MB_SUCCESS == mb.get_connectivity( h, conn, len )) {
      assert( 8 == len );
      if (MB_SUCCESS == mb.get_coords( conn, len, coords )) {
        vol = hex_centroid( coords, cent );
        centroid[0] += cent[0] * vol;
        centroid[1] += cent[1] * vol;
        centroid[2] += cent[2] * vol;
        sum_vol += vol;
      }
    }
  }
  
  centroid[0] /= sum_vol;
  centroid[1] /= sum_vol;
  centroid[2] /= sum_vol;
}

void reverse_order_query_element_verts()
{
  const MBEntityHandle* conn;
  int len;
  const MBEntityHandle last = elemStart + numElem;
  double coords[24];
  double cent[3], vol, sum_vol = 0.0;
  centroid[0] = centroid[1] = centroid[2] = 0.0;
  for (MBEntityHandle h = last-1; h >= elemStart; --h) {
    if (MB_SUCCESS == mb.get_connectivity( h, conn, len )) {
      assert( 8 == len );
      if (MB_SUCCESS == mb.get_coords( conn, len, coords )) {
        vol = hex_centroid( coords, cent );
        centroid[0] += cent[0] * vol;
        centroid[1] += cent[1] * vol;
        centroid[2] += cent[2] * vol;
        sum_vol += vol;
      }
    }
  }
  
  centroid[0] /= sum_vol;
  centroid[1] /= sum_vol;
  centroid[2] /= sum_vol;
}

void  random_order_query_element_verts()
{
  const MBEntityHandle* conn;
  int len;
  double coords[24];
  double cent[3], vol, sum_vol = 0.0;
  centroid[0] = centroid[1] = centroid[2] = 0.0;
  for (long i = 0; i < numElem; ++i) {
    MBEntityHandle h = elemStart + queryElemPermutation[i];
    if (MB_SUCCESS == mb.get_connectivity( h, conn, len )) {
      assert( 8 == len );
      if (MB_SUCCESS == mb.get_coords( conn, len, coords )) {
        vol = hex_centroid( coords, cent );
        centroid[0] += cent[0] * vol;
        centroid[1] += cent[1] * vol;
        centroid[2] += cent[2] * vol;
        sum_vol += vol;
      }
    }
  }
  
  centroid[0] /= sum_vol;
  centroid[1] /= sum_vol;
  centroid[2] /= sum_vol;
}

void forward_order_delete_vertices( int percent )
{
  for (long i = 0; i < numVert; ++i)
    delete_vert( i, percent );
}

void reverse_order_delete_vertices( int percent )
{
  for (long i = numVert-1; i >= 0; --i)
    delete_vert( i, percent );
}

void  random_order_delete_vertices( int percent )
{
  for (long i = 0; i < numVert; ++i)
    delete_vert( queryVertPermutation[i], percent );
}

void forward_order_delete_elements( int percent )
{
  for (long i = 0; i < numElem; ++i)
    delete_elem( i, percent );
}

void reverse_order_delete_elements( int percent )
{
  for (long i = numElem-1; i >= 0; --i)
    delete_elem( i, percent );
}

void  random_order_delete_elements( int percent )
{
  for (long i = 0; i < numElem; ++i)
    delete_elem( queryElemPermutation[i], percent );
}


void create_missing_vertices( int percent )
{
  MBEntityHandle h;
  MBErrorCode rval;
  double coords[3];
  for (long i = 0; i < numVert; ++i)
    if (deleted_vert( i, percent )) {
      vertex_coords( i, coords[0], coords[1], coords[2] );
      rval = mb.create_vertex( coords, h );
      assert(!rval);
    }
}

void create_missing_elements( int percent )
{
  MBEntityHandle h;
  MBErrorCode rval;
  MBEntityHandle conn[8];
  for (long i = 0; i < numElem; ++i)
    if (deleted_elem( i, percent )) {
      element_conn( i, conn );
      rval = mb.create_element( MBHEX, conn, 8, h );
      assert(!rval);
    }
}

inline void vertex_coords( long vert_index, double& x, double& y, double& z )
{
  const long vs = numSideInt + 1;
  x = vert_index % vs;
  y = (vert_index / vs) % vs;
  z = (vert_index / vs / vs);
}

inline long vert_index( long x, long y, long z )
{
  const long vs = numSideInt + 1;
  return x + vs * (y + vs * z);
}

inline void element_conn( long elem_index, MBEntityHandle conn[8] )
{
  const long x = elem_index % numSideInt;
  const long y = (elem_index / numSideInt) % numSideInt;
  const long z = (elem_index / numSideInt / numSideInt);
  conn[0] = vertStart + vert_index(x  ,y  ,z  );
  conn[1] = vertStart + vert_index(x+1,y  ,z  );
  conn[2] = vertStart + vert_index(x+1,y+1,z  );
  conn[3] = vertStart + vert_index(x  ,y+1,z  );
  conn[4] = vertStart + vert_index(x  ,y  ,z+1);
  conn[5] = vertStart + vert_index(x+1,y  ,z+1);
  conn[6] = vertStart + vert_index(x+1,y+1,z+1);
  conn[7] = vertStart + vert_index(x  ,y+1,z+1);
}


inline bool deleted_vert( long index, int percent )
{
  return index % 100 >= (100-percent);
}

inline bool deleted_elem( long index, int percent )
{
  return index % 100 >= (100-percent);
}

inline void delete_vert( long index, int percent )
{
  if (deleted_vert(index, percent)) {
    MBEntityHandle h = index + vertStart;
    MBErrorCode rval = mb.delete_entities( &h, 1 );
    assert(!rval);
  }
}

inline void delete_elem( long index, int percent )
{
  if (deleted_elem(index, percent)) {
    MBEntityHandle h = index + elemStart;
    MBErrorCode rval = mb.delete_entities( &h, 1 );
    assert(!rval);
  }
}

