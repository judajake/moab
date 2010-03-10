#include "MBMeshSet.hpp"
#include "AEntityFactory.hpp"
#include "TestUtil.hpp"
#include "MBCore.hpp"

#include <iostream>

//! test add_entities, get_entities, num_entities
void test_add_entities( unsigned flags );
//! test remove_entities, get_entities, num_entities
void test_remove_entities( unsigned flags );
//! test get_entities_by_type, num_entities_by_type
void test_entities_by_type( unsigned flags );
//! test get_entities_by_dimension, num_entities_by_dimension
void test_entities_by_dimension( unsigned flags );
//! test subtract_meshset
void test_subtract( unsigned flags1, unsigned flags2 );
//! test intersect_meshset
void test_intersect( unsigned flags1, unsigned flags2 );
//! test unite_meshset
void test_unite( unsigned flags1, unsigned flags2 );
//! test MBMeshSet::contains_entities
void test_contains_entities( unsigned flags );
//! test clear_meshset
void test_clear( unsigned flags );

//! Create 10x10x10 hex mesh
void make_mesh( MBInterface& iface );
//! Create 10x10x10 hex mesh, return ranges of handles
void make_mesh( MBInterface& iface, MBEntityHandle& vstart, MBEntityHandle& vend, MBEntityHandle& hstart, MBEntityHandle& hend );
//! Print std::vector<MBEntityHandle>
void print_handle_vect( const char* prefix, const std::vector<MBEntityHandle>& vect );
//! Print MBRange
void print_mbrange( const char* prefix, const MBRange& range );
//! Compare set contents against passed vector, and if MESHSET_TRACK_OWNER then check adjacencies
bool check_set_contents( MBCore& mb, MBEntityHandle set, const std::vector<MBEntityHandle>& expected );
//! Compare result of get_entities_by_type to result of get_entities_by_handle
bool check_set_contents( MBCore& mb, MBEntityType type, MBEntityHandle set, unsigned flags );
//! Compare result of get_entities_by_dimension to result of get_entities_by_handle
bool check_set_contents( MBCore& mb, int dim, MBEntityHandle set, unsigned flags );
//! For each entry in range, if one or more occurances in vect, remove last occurance from vect.
void remove_from_back( std::vector<MBEntityHandle>& vect, const MBRange& range );
enum BoolOp { UNITE, INTERSECT, SUBTRACT };
//! Perform boolean op on two entity sets and verify result
bool test_boolean( MBCore& mb, BoolOp op, 
                   unsigned flags1, const MBRange& set1_ents, 
                   unsigned flags2, const MBRange& set2_ents );

void test_add_entities_ordered()          { test_add_entities( MESHSET_ORDERED ); }
void test_add_entities_set()              { test_add_entities( MESHSET_SET     ); }
void test_add_entities_ordered_tracking() { test_add_entities( MESHSET_TRACK_OWNER|MESHSET_ORDERED ); }
void test_add_entities_set_tracking()     { test_add_entities( MESHSET_TRACK_OWNER|MESHSET_SET     ); }

void test_remove_entities_ordered()          { test_remove_entities( MESHSET_ORDERED ); }
void test_remove_entities_set()              { test_remove_entities( MESHSET_SET     ); }
void test_remove_entities_ordered_tracking() { test_remove_entities( MESHSET_TRACK_OWNER|MESHSET_ORDERED ); }
void test_remove_entities_set_tracking()     { test_remove_entities( MESHSET_TRACK_OWNER|MESHSET_SET     ); }

void test_entities_by_type_ordered()  { test_entities_by_type     ( MESHSET_ORDERED ); }
void test_entities_by_type_set()      { test_entities_by_type     ( MESHSET_SET     ); }
void test_entities_by_dim_ordered()   { test_entities_by_dimension( MESHSET_ORDERED ); }
void test_entities_by_dim_set()       { test_entities_by_dimension( MESHSET_SET     ); }

void test_subtract_set_set()                  { test_subtract( MESHSET_SET    , MESHSET_SET     ); }
void test_subtract_set_ordered()              { test_subtract( MESHSET_SET    , MESHSET_ORDERED ); }
void test_subtract_ordered_set()              { test_subtract( MESHSET_ORDERED, MESHSET_SET     ); }
void test_subtract_ordered_ordered()          { test_subtract( MESHSET_ORDERED, MESHSET_ORDERED ); }
void test_subtract_set_set_tracking()         { test_subtract( MESHSET_TRACK_OWNER|MESHSET_SET    , MESHSET_TRACK_OWNER|MESHSET_SET     ); }
void test_subtract_set_ordered_tracking()     { test_subtract( MESHSET_TRACK_OWNER|MESHSET_SET    , MESHSET_TRACK_OWNER|MESHSET_ORDERED ); }
void test_subtract_ordered_set_tracking()     { test_subtract( MESHSET_TRACK_OWNER|MESHSET_ORDERED, MESHSET_TRACK_OWNER|MESHSET_SET     ); }
void test_subtract_ordered_ordered_tracking() { test_subtract( MESHSET_TRACK_OWNER|MESHSET_ORDERED, MESHSET_TRACK_OWNER|MESHSET_ORDERED ); }

void test_itersect_set_set()                  { test_intersect( MESHSET_SET    , MESHSET_SET     ); }
void test_itersect_set_ordered()              { test_intersect( MESHSET_SET    , MESHSET_ORDERED ); }
void test_itersect_ordered_set()              { test_intersect( MESHSET_ORDERED, MESHSET_SET     ); }
void test_itersect_ordered_ordered()          { test_intersect( MESHSET_ORDERED, MESHSET_ORDERED ); }
void test_itersect_set_set_tracking()         { test_intersect( MESHSET_TRACK_OWNER|MESHSET_SET    , MESHSET_TRACK_OWNER|MESHSET_SET     ); }
void test_itersect_set_ordered_tracking()     { test_intersect( MESHSET_TRACK_OWNER|MESHSET_SET    , MESHSET_TRACK_OWNER|MESHSET_ORDERED ); }
void test_itersect_ordered_set_tracking()     { test_intersect( MESHSET_TRACK_OWNER|MESHSET_ORDERED, MESHSET_TRACK_OWNER|MESHSET_SET     ); }
void test_itersect_ordered_ordered_tracking() { test_intersect( MESHSET_TRACK_OWNER|MESHSET_ORDERED, MESHSET_TRACK_OWNER|MESHSET_ORDERED ); }

void test_unite_set_set()                  { test_unite( MESHSET_SET    , MESHSET_SET     ); }
void test_unite_set_ordered()              { test_unite( MESHSET_SET    , MESHSET_ORDERED ); }
void test_unite_ordered_set()              { test_unite( MESHSET_ORDERED, MESHSET_SET     ); }
void test_unite_ordered_ordered()          { test_unite( MESHSET_ORDERED, MESHSET_ORDERED ); }
void test_unite_set_set_tracking()         { test_unite( MESHSET_TRACK_OWNER|MESHSET_SET    , MESHSET_TRACK_OWNER|MESHSET_SET     ); }
void test_unite_set_ordered_tracking()     { test_unite( MESHSET_TRACK_OWNER|MESHSET_SET    , MESHSET_TRACK_OWNER|MESHSET_ORDERED ); }
void test_unite_ordered_set_tracking()     { test_unite( MESHSET_TRACK_OWNER|MESHSET_ORDERED, MESHSET_TRACK_OWNER|MESHSET_SET     ); }
void test_unite_ordered_ordered_tracking() { test_unite( MESHSET_TRACK_OWNER|MESHSET_ORDERED, MESHSET_TRACK_OWNER|MESHSET_ORDERED ); }

void test_contains_entities_ordered() { test_contains_entities( MESHSET_ORDERED ); }
void test_contains_entities_set()     { test_contains_entities( MESHSET_SET    ); }

void test_clear_ordered()             { test_clear(                     MESHSET_ORDERED ); }
void test_clear_set()                 { test_clear(                     MESHSET_SET     ); }
void test_clear_ordered_tracking()    { test_clear( MESHSET_TRACK_OWNER|MESHSET_ORDERED ); }
void test_clear_set_tracking()        { test_clear( MESHSET_TRACK_OWNER|MESHSET_SET     ); }

int main()
{
  int err = 0;
  
  err += RUN_TEST(test_add_entities_ordered);
  err += RUN_TEST(test_add_entities_set);
  err += RUN_TEST(test_add_entities_ordered_tracking);
  err += RUN_TEST(test_add_entities_set_tracking);

  err += RUN_TEST(test_remove_entities_ordered);
  err += RUN_TEST(test_remove_entities_set);
  err += RUN_TEST(test_remove_entities_ordered_tracking);
  err += RUN_TEST(test_remove_entities_set_tracking);

  err += RUN_TEST(test_entities_by_type_ordered);
  err += RUN_TEST(test_entities_by_type_set);
  err += RUN_TEST(test_entities_by_dim_ordered);
  err += RUN_TEST(test_entities_by_dim_set);

  err += RUN_TEST(test_subtract_set_set);
  err += RUN_TEST(test_subtract_set_ordered);
  err += RUN_TEST(test_subtract_ordered_set);
  err += RUN_TEST(test_subtract_ordered_ordered);
  err += RUN_TEST(test_subtract_set_set_tracking);
  err += RUN_TEST(test_subtract_set_ordered_tracking);
  err += RUN_TEST(test_subtract_ordered_set_tracking);
  err += RUN_TEST(test_subtract_ordered_ordered_tracking);

  err += RUN_TEST(test_itersect_set_set);
  err += RUN_TEST(test_itersect_set_ordered);
  err += RUN_TEST(test_itersect_ordered_set);
  err += RUN_TEST(test_itersect_ordered_ordered);
  err += RUN_TEST(test_itersect_set_set_tracking);
  err += RUN_TEST(test_itersect_set_ordered_tracking);
  err += RUN_TEST(test_itersect_ordered_set_tracking);
  err += RUN_TEST(test_itersect_ordered_ordered_tracking);

  err += RUN_TEST(test_unite_set_set);
  err += RUN_TEST(test_unite_set_ordered);
  err += RUN_TEST(test_unite_ordered_set);
  err += RUN_TEST(test_unite_ordered_ordered);
  err += RUN_TEST(test_unite_set_set_tracking);
  err += RUN_TEST(test_unite_set_ordered_tracking);
  err += RUN_TEST(test_unite_ordered_set_tracking);
  err += RUN_TEST(test_unite_ordered_ordered_tracking);

  err += RUN_TEST(test_contains_entities_ordered);
  err += RUN_TEST(test_contains_entities_set);
  
  err += RUN_TEST(test_clear_ordered);
  err += RUN_TEST(test_clear_set);
  err += RUN_TEST(test_clear_ordered_tracking);
  err += RUN_TEST(test_clear_set_tracking);
  
  if (!err) 
    printf("ALL TESTS PASSED\n");
  else
    printf("%d TESTS FAILED\n",err);
  
  return err;
}

// Create 100x100x100 hex mesh
void make_mesh( MBInterface& iface )
{
  const int dim = 10;

    // create vertices
  MBEntityHandle prev_handle = 0;
  for (int z = 0; z <= dim; ++z) {
    for (int y = 0; y <= dim; ++y) {
      for (int x = 0; x <= dim; ++x) {
        const double coords[] = {x, y, z};
        MBEntityHandle new_handle = 0;
        MBErrorCode rval = iface.create_vertex( coords, new_handle );
        CHECK_ERR(rval);
        CHECK_EQUAL( ++prev_handle, new_handle );
      }
    }
  }
  
    // create hexes
  const int dim1 = dim + 1;
  const int dimq = dim1 * dim1;
  prev_handle = FIRST_HANDLE(MBHEX);
  for (int z = 0; z < dim; ++z) {
    for (int y = 0; y < dim; ++y) {
      for (int x = 0; x < dim; ++x) {
        const MBEntityHandle off = 1 + x + dim1*y + dimq*z;
        const MBEntityHandle conn[] = { off,      off+1,      off+1+dim1,      off+dim1,
                                        off+dimq, off+1+dimq, off+1+dim1+dimq, off+dim1+dimq };
        MBEntityHandle new_handle = 0;
        MBErrorCode rval = iface.create_element( MBHEX, conn, 8, new_handle );
        CHECK_ERR(rval);
        CHECK_EQUAL( prev_handle++, new_handle );
      }
    }
  }
  
}

void make_mesh( MBInterface& mb, MBEntityHandle& first_vert, MBEntityHandle& last_vert, MBEntityHandle& first_hex, MBEntityHandle& last_hex )
{
  make_mesh( mb );
  
    // Get handle ranges, and validate assumption that handles
    // are contiguous.
  MBRange range;
  MBErrorCode rval = mb.get_entities_by_type( 0, MBVERTEX, range );
  CHECK_ERR(rval);
  first_vert = range.front();
  last_vert = range.back();
  CHECK_EQUAL( (MBEntityHandle)1331, last_vert - first_vert + 1 );
  range.clear();
  rval = mb.get_entities_by_type( 0, MBHEX, range );
  CHECK_ERR(rval);
  first_hex = range.front();
  last_hex = range.back();
  CHECK_EQUAL( (MBEntityHandle)1000, last_hex - first_hex + 1 );
}


template <typename iter_type>
void print_handles( std::ostream& str, const char* prefix, iter_type begin, iter_type end )
{
  if (prefix)
    str << prefix << ':';
  if (begin == end) {
    str << " (empty)" << std::endl;
    return;
  }
  
  iter_type i = begin;
  MBEntityType prev_type = TYPE_FROM_HANDLE(*i);
  MBEntityHandle prev_ent = *i;
  str << ' ' << MBCN::EntityTypeName(prev_type) << ' ' << ID_FROM_HANDLE(*i);
  for (;;) {
    iter_type j = i;
    for (++j, ++prev_ent; j != end && *j == prev_ent; ++j, ++prev_ent);
    --prev_ent;
    if (prev_ent - *i > 1) 
      str << "-" << ID_FROM_HANDLE(prev_ent);
    else if (prev_ent - *i == 1)
      str << ", " << ID_FROM_HANDLE(prev_ent);
    
    i = j;
    if (i == end)
      break;
    
    str << ',';
    if (TYPE_FROM_HANDLE(*i) != prev_type) 
      str << ' ' << MBCN::EntityTypeName(prev_type = TYPE_FROM_HANDLE(*i));
    str << ' ' << ID_FROM_HANDLE(*i);
    prev_ent = *i;
  }
  str << std::endl;
}

void print_handle_vect( const char* prefix, const std::vector<MBEntityHandle>& vect )
{
  print_handles(std::cout, prefix, vect.begin(), vect.end());
}

void print_mbrange( const char* prefix, const MBRange& range )
{
  print_handles(std::cout, prefix, range.begin(), range.end());
}

bool compare_set_contents( unsigned flags, 
                           const std::vector<MBEntityHandle>& expected,
                           int set_count,
                           std::vector<MBEntityHandle>& vect,
                           const MBRange& range )
{
  
  std::vector<MBEntityHandle> sorted( expected );
  std::sort( sorted.begin(), sorted.end() );
  sorted.erase( std::unique( sorted.begin(), sorted.end() ), sorted.end() );
  
  int expected_size = 0;
  if (flags&MESHSET_ORDERED) {
    if (expected != vect) {
      std::cout << "Incorrect set contents from vector-based query" << std::endl;
      print_handle_vect( "Expected", expected );
      print_handle_vect( "Actual", vect );
      return false;
    }
    expected_size = expected.size();
  }
  else {
    if (sorted != vect) {
      std::cout << "Incorrect set contents from vector-based query" << std::endl;
      print_handle_vect( "Expected", sorted );
      print_handle_vect( "Actual", vect );
      return false;
    }
    expected_size = sorted.size();
  }
  
  if (expected_size != set_count) {
    std::cout << "Incorrect size for entity set" << std::endl;
    std::cout << "Expected: " << expected_size << std::endl;
    std::cout << "Actual:   " << set_count     << std::endl;
    return false;
  }

  vect.clear();
  vect.resize( range.size() );
  std::copy( range.begin(), range.end(), vect.begin() );
  if (sorted != vect) {
    std::cout << "Incorrect set contents from mbrange-based query" << std::endl;
    print_handle_vect( "Expected", vect );
    print_mbrange( "Actual", range );
    return false;
  }
  
  return true;
}     

bool check_set_contents( MBCore& mb, MBEntityHandle set, const std::vector<MBEntityHandle>& expected )
{
  unsigned flags;
  MBErrorCode rval = mb.get_meshset_options( set, flags );
  CHECK_ERR(rval);
  
  int count;
  std::vector<MBEntityHandle> vect;
  MBRange range;
  rval = mb.get_entities_by_handle( set, vect, false );
  CHECK_ERR(rval);
  rval = mb.get_entities_by_handle( set, range, false );
  CHECK_ERR(rval);
  rval = mb.get_number_entities_by_handle( set, count, false );
  CHECK_ERR(rval);
  
  if (!compare_set_contents( flags, expected, count, vect, range ))
    return false;
 
  if (!(flags&MESHSET_TRACK_OWNER))
    return true;
  
    // get all entitites with an adjacency to the set
  std::vector<MBEntityHandle> adj;
  MBRange all, adjacent;
  MBRange::iterator in = adjacent.begin();
  mb.get_entities_by_handle( 0, all );
  for (MBRange::iterator i = all.begin(); i != all.end(); ++i) {
    adj.clear();
    rval = mb.a_entity_factory()->get_adjacencies( *i, adj );
    CHECK_ERR(rval);
    std::vector<MBEntityHandle>::iterator j = std::lower_bound( adj.begin(), adj.end(), set );
    if (j != adj.end() && *j == set)
      in = adjacent.insert( in, *i, *i );
  }
  
  if (range != adjacent) {
    std::cout << "Incorrect adjacent entities for tracking set" << std::endl;
    print_mbrange( "Expected", range );
    print_mbrange( "Actual", adjacent );
    return false;
  }
  
  return true;
}  

bool check_set_contents( MBCore& mb, MBEntityType type, MBEntityHandle set, unsigned flags )
{
  MBErrorCode rval;
  int count;
  std::vector<MBEntityHandle> vect, expected;
  MBRange range;
  
  rval = mb.get_entities_by_handle( set, expected, false );
  CHECK_ERR(rval);
  std::vector<MBEntityHandle>::iterator i = expected.begin();
  while (i != expected.end()) {
    if (TYPE_FROM_HANDLE(*i) != type)
      i = expected.erase( i );
    else
      ++i;
  }
  
  rval = mb.get_entities_by_type( set, type, range, false );
  CHECK_ERR(rval);
  rval = mb.get_number_entities_by_type( set, type, count, false );
  CHECK_ERR(rval);
  
  std::copy( range.begin(), range.end(), std::back_inserter(vect) );
  return compare_set_contents( flags, expected, count, vect, range );
}  

bool check_set_contents( MBCore& mb, int dim, MBEntityHandle set, unsigned flags )
{
  MBErrorCode rval;
  int count;
  std::vector<MBEntityHandle> vect, expected;
  MBRange range;
  
  rval = mb.get_entities_by_handle( set, expected, false );
  CHECK_ERR(rval);
  std::vector<MBEntityHandle>::iterator i = expected.begin();
  while (i != expected.end()) {
    if (MBCN::Dimension(TYPE_FROM_HANDLE(*i)) != dim)
      i = expected.erase( i );
    else
      ++i;
  }
  
  rval = mb.get_entities_by_dimension( set, dim, range, false );
  CHECK_ERR(rval);
  rval = mb.get_number_entities_by_dimension( set, dim, count, false );
  CHECK_ERR(rval);
  
  std::copy( range.begin(), range.end(), std::back_inserter(vect) );
  return compare_set_contents( flags, expected, count, vect, range );
}  

void test_add_entities( unsigned flags )
{
  MBCore mb; make_mesh( mb );
  
  MBEntityHandle set;
  MBErrorCode rval = mb.create_meshset( flags, set );
  CHECK_ERR(rval);
  
  std::vector<MBEntityHandle> contents, vect;
  MBRange range;
  
  range.clear();
  range.insert( 11, 20 );
  std::copy( range.begin(), range.end(), std::back_inserter(contents) );
  rval = mb.add_entities( set, range );
  CHECK_ERR(rval);
  // [11,20]
  CHECK( check_set_contents( mb, set, contents ) );
  
  range.clear();
  range.insert( 31, 40 );
  std::copy( range.begin(), range.end(), std::back_inserter(contents) );
  rval = mb.add_entities( set, range );
  CHECK_ERR(rval);
  // [11,20],[31,40]
  CHECK( check_set_contents( mb, set, contents ) );
  
  range.clear();
  range.insert( 51, 60 );
  std::copy( range.begin(), range.end(), std::back_inserter(contents) );
  rval = mb.add_entities( set, range );
  CHECK_ERR(rval);
  // [11,20],[31,40],[51,60]
  CHECK( check_set_contents( mb, set, contents ) );
  
  range.clear();
  range.insert( 71, 80 );
  std::copy( range.begin(), range.end(), std::back_inserter(contents) );
  rval = mb.add_entities( set, range );
  CHECK_ERR(rval);
  // [11,20],[31,40],[51,60],[71,80]
  CHECK( check_set_contents( mb, set, contents ) );
  
  range.clear();
  range.insert( 91, 100 );
  std::copy( range.begin(), range.end(), std::back_inserter(contents) );
  rval = mb.add_entities( set, range );
  CHECK_ERR(rval);
  // [11,20],[31,40],[51,60],[71,80],[91,100]
  CHECK( check_set_contents( mb, set, contents ) );
  
  range.clear();
  range.insert( 111, 120 );
  std::copy( range.begin(), range.end(), std::back_inserter(contents) );
  rval = mb.add_entities( set, range );
  CHECK_ERR(rval);
  // [11,20],[31,40],[51,60],[71,80],[91,100],[111,120]
  CHECK( check_set_contents( mb, set, contents ) );
  
  range.clear();
  range.insert( 6, 12 );
  std::copy( range.begin(), range.end(), std::back_inserter(contents) );
  rval = mb.add_entities( set, range );
  CHECK_ERR(rval);
  // [6,20],[31,40],[51,60],[71,80],[91,100],[111,120]
  CHECK( check_set_contents( mb, set, contents ) );
  
  range.clear();
  range.insert( 1, 3 );
  std::copy( range.begin(), range.end(), std::back_inserter(contents) );
  rval = mb.add_entities( set, range );
  CHECK_ERR(rval);
  // [1,3],[6,20],[31,40],[51,60],[71,80],[91,100],[111,120]
  CHECK( check_set_contents( mb, set, contents ) );
  
  range.clear();
  range.insert( 25, 25 );
  std::copy( range.begin(), range.end(), std::back_inserter(contents) );
  rval = mb.add_entities( set, range );
  CHECK_ERR(rval);
  // [1,3],[6,20],[25,25],[31,40],[51,60],[71,80],[91,100],[111,120]
  CHECK( check_set_contents( mb, set, contents ) );
  
  range.clear();
  range.insert( 30, 30 );
  std::copy( range.begin(), range.end(), std::back_inserter(contents) );
  rval = mb.add_entities( set, range );
  CHECK_ERR(rval);
  // [1,3],[6,20],[25,25],[30,40],[51,60],[71,80],[91,100],[111,120]
  CHECK( check_set_contents( mb, set, contents ) );
  
  range.clear();
  range.insert( 29, 31 );
  std::copy( range.begin(), range.end(), std::back_inserter(contents) );
  rval = mb.add_entities( set, range );
  CHECK_ERR(rval);
  // [1,3],[6,20],[25,25],[29,40],[51,60],[71,80],[91,100],[111,120]
  CHECK( check_set_contents( mb, set, contents ) );
  
  range.clear();
  range.insert( 41, 41 );
  std::copy( range.begin(), range.end(), std::back_inserter(contents) );
  rval = mb.add_entities( set, range );
  CHECK_ERR(rval);
  // [1,3],[6,20],[25,25],[29,41],[51,60],[71,80],[91,100],[111,120]
  CHECK( check_set_contents( mb, set, contents ) );
  
  range.clear();
  range.insert( 41, 45 );
  std::copy( range.begin(), range.end(), std::back_inserter(contents) );
  rval = mb.add_entities( set, range );
  CHECK_ERR(rval);
  // [1,3],[6,20],[25,25],[29,45],[51,60],[71,80],[91,100],[111,120]
  CHECK( check_set_contents( mb, set, contents ) );
  
  range.clear();
  range.insert( 47, 47 );
  std::copy( range.begin(), range.end(), std::back_inserter(contents) );
  rval = mb.add_entities( set, range );
  CHECK_ERR(rval);
  // [1,3],[6,20],[25,25],[29,45],[47,47],[51,60],[71,80],[91,100],[111,120]
  CHECK( check_set_contents( mb, set, contents ) );
  
  range.clear();
  range.insert( 51, 80 );
  std::copy( range.begin(), range.end(), std::back_inserter(contents) );
  rval = mb.add_entities( set, range );
  CHECK_ERR(rval);
  // [1,3],[6,20],[25,25],[29,45],[47,47],[51,80],[91,100],[111,120]
  CHECK( check_set_contents( mb, set, contents ) );
  
  range.clear();
  range.insert( 49, 105 );
  std::copy( range.begin(), range.end(), std::back_inserter(contents) );
  rval = mb.add_entities( set, range );
  CHECK_ERR(rval);
  // [1,3],[6,20],[25,25],[29,45],[47,47],[49,105],[111,120]
  CHECK( check_set_contents( mb, set, contents ) );
  
  vect.clear();
  for (MBEntityHandle h = 1; h < 100; ++h) {
    vect.push_back(h);
    contents.push_back(h);
  }
  rval = mb.add_entities( set, &vect[0], vect.size() );
  CHECK_ERR(rval);
  // [1,105],[111,120]
  CHECK( check_set_contents( mb, set, contents ) );
  
  vect.clear();
  vect.push_back( 106 );
  vect.push_back( 108 );
  vect.push_back( 110 );
  std::copy( vect.begin(), vect.end(), std::back_inserter(contents) );
  rval = mb.add_entities( set, &vect[0], vect.size() );
  CHECK_ERR(rval);
  // [1,106],[108,108],[110,120]
  CHECK( check_set_contents( mb, set, contents ) );
  
  range.clear();
  range.insert( 107, 200 );
  std::copy( range.begin(), range.end(), std::back_inserter(contents) );
  rval = mb.add_entities( set, range );
  CHECK_ERR(rval);
  // [1,200]
  CHECK( check_set_contents( mb, set, contents ) );
  
  range.clear();
  range.insert( 1, 1 );
  range.insert( 5, 6 );
  range.insert( 199, 200 );
  range.insert( 201, 202 );
  std::copy( range.begin(), range.end(), std::back_inserter(contents) );
  rval = mb.add_entities( set, range );
  CHECK_ERR(rval);
  // [1,202]
  CHECK( check_set_contents( mb, set, contents ) );
  
  range.clear();
  range.insert( 300, 301 );
  std::copy( range.begin(), range.end(), std::back_inserter(contents) );
  rval = mb.add_entities( set, range );
  CHECK_ERR(rval);
  // [1,202],[300,301]
  CHECK( check_set_contents( mb, set, contents ) );
  
  range.clear();
  range.insert( 203,203 );
  range.insert( 205,205 );
  range.insert( 207,207 );
  range.insert( 299,299 );
  std::copy( range.begin(), range.end(), std::back_inserter(contents) );
  rval = mb.add_entities( set, range );
  CHECK_ERR(rval);
  // [1,203],[205,205],[207,207],[299,301]
  CHECK( check_set_contents( mb, set, contents ) );
}

void remove_from_back( std::vector<MBEntityHandle>& vect, const MBRange& range )
{
  for (MBRange::const_iterator r = range.begin(); r != range.end(); ++r) {
    std::vector<MBEntityHandle>::reverse_iterator i = find( vect.rbegin(), vect.rend(), *r );
    if (i != vect.rend())
      *i = 0;
  }
  std::vector<MBEntityHandle>::iterator j = vect.begin(); 
  while (j != vect.end()) {
    if (*j == 0)
      j = vect.erase(j);
    else
      ++j;
  }
}   

void test_remove_entities( unsigned flags )
{
  MBCore mb; make_mesh( mb );
  
  MBEntityHandle set;
  MBErrorCode rval = mb.create_meshset( flags, set );
  CHECK_ERR(rval);
  
  std::vector<MBEntityHandle> contents;
  MBRange range;
  
  range.clear();
  range.insert( 1, 1000 );
  std::copy( range.begin(), range.end(), std::back_inserter(contents) );
  rval = mb.add_entities( set, range );
  CHECK_ERR(rval);
  // [1,1000]
  CHECK( check_set_contents( mb, set, contents ) );

  range.clear();
  range.insert( 1, 5 );
  remove_from_back( contents, range );
  rval = mb.remove_entities( set, range );
  CHECK_ERR(rval);
  // [6,1000]
  CHECK( check_set_contents( mb, set, contents ) );

  range.clear();
  range.insert( 6, 6 );
  remove_from_back( contents, range );
  rval = mb.remove_entities( set, range );
  CHECK_ERR(rval);
  // [7,1000]
  CHECK( check_set_contents( mb, set, contents ) );

  range.clear();
  range.insert( 7, 10 );
  remove_from_back( contents, range );
  rval = mb.remove_entities( set, range );
  CHECK_ERR(rval);
  // [11,1000]
  CHECK( check_set_contents( mb, set, contents ) );

  range.clear();
  range.insert( 1, 20 );
  remove_from_back( contents, range );
  rval = mb.remove_entities( set, range );
  CHECK_ERR(rval);
  // [21,1000]
  CHECK( check_set_contents( mb, set, contents ) );

  range.clear();
  range.insert( 1, 5 );
  remove_from_back( contents, range );
  rval = mb.remove_entities( set, range );
  CHECK_ERR(rval);
  // [21,1000]
  CHECK( check_set_contents( mb, set, contents ) );

  range.clear();
  range.insert( 22, 30 );
  remove_from_back( contents, range );
  rval = mb.remove_entities( set, range );
  CHECK_ERR(rval);
  // [21,21],[31,1000]
  CHECK( check_set_contents( mb, set, contents ) );

  range.clear();
  range.insert( 1000, 1000 );
  remove_from_back( contents, range );
  rval = mb.remove_entities( set, range );
  CHECK_ERR(rval);
  // [21,21],[31,999]
  CHECK( check_set_contents( mb, set, contents ) );

  range.clear();
  range.insert( 901, 999 );
  remove_from_back( contents, range );
  rval = mb.remove_entities( set, range );
  CHECK_ERR(rval);
  // [21,21],[31,900]
  CHECK( check_set_contents( mb, set, contents ) );

  range.clear();
  range.insert( 900, 999 );
  remove_from_back( contents, range );
  rval = mb.remove_entities( set, range );
  CHECK_ERR(rval);
  // [21,21],[31,899]
  CHECK( check_set_contents( mb, set, contents ) );

  range.clear();
  range.insert( 1000, 1001 );
  remove_from_back( contents, range );
  rval = mb.remove_entities( set, range );
  CHECK_ERR(rval);
  // [21,21],[31,899]
  CHECK( check_set_contents( mb, set, contents ) );

  range.clear();
  range.insert( 890, 898 );
  remove_from_back( contents, range );
  rval = mb.remove_entities( set, range );
  CHECK_ERR(rval);
  // [21,21],[31,889],[899,899]
  CHECK( check_set_contents( mb, set, contents ) );

  range.clear();
  range.insert( 100, 149 );
  remove_from_back( contents, range );
  rval = mb.remove_entities( set, range );
  CHECK_ERR(rval);
  // [21,21],[31,99],[150,889],[899,899]
  CHECK( check_set_contents( mb, set, contents ) );

  range.clear();
  range.insert( 31, 99 );
  remove_from_back( contents, range );
  rval = mb.remove_entities( set, range );
  CHECK_ERR(rval);
  // [21,21],[150,889],[899,899]
  CHECK( check_set_contents( mb, set, contents ) );

  range.clear();
  range.insert( 200, 249 );
  remove_from_back( contents, range );
  rval = mb.remove_entities( set, range );
  CHECK_ERR(rval);
  // [21,21],[150,199],[250,889],[899,899]
  CHECK( check_set_contents( mb, set, contents ) );

  range.clear();
  range.insert( 300, 349 );
  remove_from_back( contents, range );
  rval = mb.remove_entities( set, range );
  CHECK_ERR(rval);
  // [21,21],[150,199],[250,299],[350,889],[899,899]
  CHECK( check_set_contents( mb, set, contents ) );

  range.clear();
  range.insert( 159, 399 );
  remove_from_back( contents, range );
  rval = mb.remove_entities( set, range );
  CHECK_ERR(rval);
  // [21,21],[150,158],[400,889],[899,899]
  CHECK( check_set_contents( mb, set, contents ) );

  range.clear();
  range.insert( 450, 499 );
  remove_from_back( contents, range );
  rval = mb.remove_entities( set, range );
  CHECK_ERR(rval);
  // [21,21],[150,158],[400,449],[500,889],[899,899]
  CHECK( check_set_contents( mb, set, contents ) );

  range.clear();
  range.insert( 550, 599 );
  remove_from_back( contents, range );
  rval = mb.remove_entities( set, range );
  CHECK_ERR(rval);
  // [21,21],[150,158],[400,449],[500,549],[600,889],[899,899]
  CHECK( check_set_contents( mb, set, contents ) );

  range.clear();
  range.insert( 150, 549 );
  remove_from_back( contents, range );
  rval = mb.remove_entities( set, range );
  CHECK_ERR(rval);
  // [21,21],[600,889],[899,899]
  CHECK( check_set_contents( mb, set, contents ) );
  
  range.clear();
  range.insert( 650, 699 );
  remove_from_back( contents, range );
  rval = mb.remove_entities( set, range );
  CHECK_ERR(rval);
  // [21,21],[600,649],[700,889],[899,899]
  CHECK( check_set_contents( mb, set, contents ) );
  
  
  // test vector-based remove
  assert(contents.size() == 242);
  std::vector<MBEntityHandle> remlist(5);
  remlist[0] = contents[240]; contents.erase( contents.begin() + 240 );
  remlist[1] = contents[200]; contents.erase( contents.begin() + 200 );
  remlist[2] = contents[100]; contents.erase( contents.begin() + 100 );
  remlist[3] = contents[ 25]; contents.erase( contents.begin() +  25 );
  remlist[4] = contents[  0]; contents.erase( contents.begin() +   0 );
  rval = mb.remove_entities( set, &remlist[0], remlist.size() );
  CHECK_ERR(rval);
  // [600,623],[625,649],[700,748],[750,848],[850,888],[899,899]
  CHECK( check_set_contents( mb, set, contents ) );

  // remove everything
  std::reverse( contents.begin(), contents.begin() + contents.size()/2 ); // mix up a bit
  rval = mb.remove_entities( set, &contents[0], contents.size() );
  CHECK_ERR(rval);
  contents.clear();
  CHECK( check_set_contents( mb, set, contents ) );
  
  // try complicated range-based remove
  range.clear();
  contents.clear();
  range.insert( 100, 200 );
  std::copy( range.begin(), range.end(), std::back_inserter(contents) );
  rval = mb.add_entities( set, range );
  CHECK_ERR(rval);
  // [1000,2000]
  CHECK( check_set_contents( mb, set, contents ) );
  
  MBRange remove;
  remove.insert( 1, 3 );
  remove.insert( 10, 100 );
  remove.insert( 110, 120 );
  remove.insert( 130, 140 );
  remove.insert( 150, 160 );
  remove.insert( 190, 200 );
  remove.insert( 210, 220 );
  remove.insert( 230, 240 );
  range = subtract( range,  remove );
  
  contents.clear();
  std::copy( range.begin(), range.end(), std::back_inserter(contents) );
  rval = mb.remove_entities( set, remove );
  CHECK_ERR(rval);
  CHECK( check_set_contents( mb, set, contents ) );  
}

void test_entities_by_type( unsigned flags )
{
  MBEntityHandle first_vert, last_vert, first_hex, last_hex;
  MBCore mb; make_mesh( mb, first_vert, last_vert, first_hex, last_hex );
  
    // Create an entity set
  MBEntityHandle set;
  MBErrorCode rval = mb.create_meshset( flags, set );
  CHECK_ERR(rval);

    // Test empty set
  CHECK( check_set_contents( mb, MBVERTEX, set, flags ) );
  CHECK( check_set_contents( mb, MBEDGE  , set, flags ) );
  CHECK( check_set_contents( mb, MBHEX   , set, flags ) );
  
    // Add stuff to set
  MBRange range;
  range.insert( first_vert      , first_vert +  10 );
  range.insert( first_vert + 100, first_vert + 110 );
  range.insert( first_hex  + 200, first_hex  + 299 );
  range.insert( last_hex        , last_hex   -  99 );
  rval = mb.add_entities( set, range );
  CHECK_ERR(rval);
  
    // Test 
  CHECK( check_set_contents( mb, MBVERTEX, set, flags ) );
  CHECK( check_set_contents( mb, MBEDGE  , set, flags ) );
  CHECK( check_set_contents( mb, MBHEX   , set, flags ) );
}

void test_entities_by_dimension( unsigned flags )
{
  MBEntityHandle first_vert, last_vert, first_hex, last_hex;
  MBCore mb; make_mesh( mb, first_vert, last_vert, first_hex, last_hex );
  
    // Create an entity set
  MBEntityHandle set;
  MBErrorCode rval = mb.create_meshset( flags, set );
  CHECK_ERR(rval);

    // Test empty set
  CHECK( check_set_contents( mb, 0, set, flags ) );
  CHECK( check_set_contents( mb, 1, set, flags ) );
  CHECK( check_set_contents( mb, 2, set, flags ) );
  CHECK( check_set_contents( mb, 3, set, flags ) );
  
    // Add stuff to set
  MBRange range;
  range.insert( first_vert      , first_vert +  10 );
  range.insert( first_vert + 100, first_vert + 110 );
  range.insert( first_hex  + 200, first_hex  + 299 );
  range.insert( last_hex        , last_hex   -  99 );
  rval = mb.add_entities( set, range );
  CHECK_ERR(rval);
  
    // Test 
  CHECK( check_set_contents( mb, 0, set, flags ) );
  CHECK( check_set_contents( mb, 1, set, flags ) );
  CHECK( check_set_contents( mb, 2, set, flags ) );
  CHECK( check_set_contents( mb, 3, set, flags ) );
}

bool test_boolean( MBCore& mb, BoolOp op, 
                   unsigned flags1, const MBRange& set1_ents, 
                   unsigned flags2, const MBRange& set2_ents )
{
  MBErrorCode rval;
   
  // make sets
  MBEntityHandle set1, set2;
  rval = mb.create_meshset( flags1, set1 );
  CHECK_ERR(rval);
  rval = mb.create_meshset( flags2, set2 );
  CHECK_ERR(rval);
  rval = mb.add_entities( set1, set1_ents );
  CHECK_ERR(rval);
  rval = mb.add_entities( set2, set2_ents );
  CHECK_ERR(rval);
  
  MBRange tmp_range;
  std::vector<MBEntityHandle> expected;
  rval = MB_INDEX_OUT_OF_RANGE;
  switch (op) {
    case UNITE:
      if (flags1 & MESHSET_SET) {
        tmp_range = set1_ents;
        tmp_range.merge( set2_ents );
        expected.resize( tmp_range.size() );
        std::copy( tmp_range.begin(), tmp_range.end(), expected.begin() );
      }
      else {
        expected.clear();
        std::copy( set1_ents.begin(), set1_ents.end(), std::back_inserter(expected) );
        std::copy( set2_ents.begin(), set2_ents.end(), std::back_inserter(expected) );
      }
      rval = mb.unite_meshset( set1, set2 );
      break;
    case INTERSECT:
      tmp_range = intersect( set1_ents, set2_ents );
      expected.resize(tmp_range.size());
      std::copy( tmp_range.begin(), tmp_range.end(), expected.begin() );
      rval = mb.intersect_meshset( set1, set2 );
      break;
    case SUBTRACT:
      tmp_range = subtract( set1_ents, set2_ents );
      expected.resize(tmp_range.size());
      std::copy( tmp_range.begin(), tmp_range.end(), expected.begin() );
      rval = mb.subtract_meshset( set1, set2 );
      break;
  }
  CHECK_ERR(rval);
  
  return check_set_contents( mb, set1, expected );
}
  

void test_intersect( unsigned flags1, unsigned flags2 )
{
  MBRange empty, set1_ents, set2_ents;
  MBEntityHandle first_vert, last_vert, first_hex, last_hex;
  MBCore mb; make_mesh( mb, first_vert, last_vert, first_hex, last_hex );

    // define contents of first set
  set1_ents.insert( first_vert, first_vert + 10 );
  set1_ents.insert( first_vert+100, first_vert+199 );
  set1_ents.insert( first_hex+100, first_hex+200 );
  
    // define contents of second set
  set2_ents.insert( first_vert, first_vert );
  set2_ents.insert( first_vert+10, first_vert+10 );
  set2_ents.insert( first_vert+90, first_vert+209 );
  set2_ents.insert( first_hex+50, first_hex+300 );

    // check empty sets
  CHECK( test_boolean( mb, INTERSECT, flags1, empty, flags2, empty ) );
    // check intersection with 1 empty
  CHECK( test_boolean( mb, INTERSECT, flags1, empty, flags2, set2_ents ) );
  CHECK( test_boolean( mb, INTERSECT, flags1, set1_ents, flags2, empty ) );
    // check intersection of non-empty sets
  CHECK( test_boolean( mb, INTERSECT, flags1, set1_ents, flags2, set2_ents ) );
}

void test_unite( unsigned flags1, unsigned flags2 )
{
  MBRange empty, set1_ents, set2_ents;
  MBEntityHandle first_vert, last_vert, first_hex, last_hex;
  MBCore mb; make_mesh( mb, first_vert, last_vert, first_hex, last_hex );

    // define contents of first set
  set1_ents.insert( first_vert, first_vert + 10 );
  set1_ents.insert( first_vert+100, first_vert+199 );
  set1_ents.insert( first_hex+100, first_hex+200 );
  
    // define contents of second set
  set2_ents.insert( first_vert, first_vert );
  set2_ents.insert( first_vert+11, first_vert+99 );
  set2_ents.insert( first_vert+150, first_vert+209 );
  set2_ents.insert( first_vert+211, first_vert+211 );
  set2_ents.insert( first_hex+50, first_hex+99 );

    // check empty sets
  CHECK( test_boolean( mb, UNITE, flags1, empty, flags2, empty ) );
    // check union with 1 empty
  CHECK( test_boolean( mb, UNITE, flags1, empty, flags2, set2_ents ) );
  CHECK( test_boolean( mb, UNITE, flags1, set1_ents, flags2, empty ) );
    // check union of non-empty sets
  CHECK( test_boolean( mb, UNITE, flags1, set1_ents, flags2, set2_ents ) );
}

void test_subtract( unsigned flags1, unsigned flags2 )
{
  MBRange empty, set1_ents, set2_ents;
  MBEntityHandle first_vert, last_vert, first_hex, last_hex;
  MBCore mb; make_mesh( mb, first_vert, last_vert, first_hex, last_hex );

    // define contents of first set
  set1_ents.insert( first_vert, first_vert + 10 );
  set1_ents.insert( first_vert+100, first_vert+199 );
  set1_ents.insert( first_hex+100, first_hex+200 );
  
    // define contents of second set
  set2_ents.insert( first_vert, first_vert );
  set2_ents.insert( first_vert+9, first_vert+9 );
  set2_ents.insert( first_vert+11, first_vert+99 );
  set2_ents.insert( first_vert+150, first_vert+199 );
  set2_ents.insert( first_hex+50, first_hex+60 );
  set2_ents.insert( first_hex+90, first_hex+220 );

    // check empty sets
  CHECK( test_boolean( mb, SUBTRACT, flags1, empty, flags2, empty ) );
    // check union with 1 empty
  CHECK( test_boolean( mb, SUBTRACT, flags1, empty, flags2, set2_ents ) );
  CHECK( test_boolean( mb, SUBTRACT, flags1, set1_ents, flags2, empty ) );
    // check union of non-empty sets
  CHECK( test_boolean( mb, SUBTRACT, flags1, set1_ents, flags2, set2_ents ) );
}

void test_contains_entities( unsigned flags )
{
  CHECK( !(flags&MESHSET_TRACK_OWNER) );
  MBMeshSet set(flags);
  bool result;
  
  const MBEntityHandle entities[] = { 1,2,3,6,10,11,25,100 };
  const int num_ents = sizeof(entities)/sizeof(MBEntityHandle);
  MBErrorCode rval = set.add_entities( entities, num_ents, 0, 0 );
  CHECK_ERR(rval);
  
  result = set.contains_entities( entities, num_ents, MBInterface::UNION );
  CHECK(result);
  result = set.contains_entities( entities, num_ents, MBInterface::INTERSECT );
  CHECK(result);
  
  result = set.contains_entities( entities, 1, MBInterface::UNION );
  CHECK(result);
  result = set.contains_entities( entities, 1, MBInterface::INTERSECT );
  CHECK(result);
  
  const MBEntityHandle entities2[] = { 3,4,5 };
  const int num_ents2 = sizeof(entities2)/sizeof(MBEntityHandle);
  result = set.contains_entities( entities2, num_ents2, MBInterface::UNION );
  CHECK(result);
  result = set.contains_entities( entities2, num_ents2, MBInterface::INTERSECT );
  CHECK(!result);
}

void test_clear( unsigned flags )
{
  std::vector<MBEntityHandle> contents(4);
  MBCore mb; make_mesh( mb, contents[0], contents[1], contents[2], contents[3] );
  MBEntityHandle set;
  MBErrorCode rval = mb.create_meshset( flags, set );
  CHECK_ERR(rval);
  rval = mb.add_entities( set, &contents[0], contents.size() );
  CHECK_ERR(rval);
  CHECK( check_set_contents( mb, set, contents ) );
  rval = mb.clear_meshset( &set, 1 );
  CHECK_ERR(rval);
  contents.clear();
  CHECK( check_set_contents( mb, set, contents ) );
}

