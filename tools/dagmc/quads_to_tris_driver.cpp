#include <iostream>
#include <assert.h>
#include "MBCore.hpp"
#include "quads_to_tris.hpp"

#define MBI mb_instance()
MBInterface* mb_instance();


// Read a DAGMC-style file of quads and convert it to tris
// Input argument is the input filename.
// Output file will be called input_filename_tris.h5m.
int main(int argc, char **argv) {

  clock_t start_time;
  start_time = clock();
  if( 2 > argc ) {
    std::cout << "Need name of input file with quads." << std::endl;
    return 0;
  }

  // load file from input argument
  MBErrorCode result;
  std::string filename = argv[1];
  MBEntityHandle input_meshset;
  result = MBI->load_file( filename.c_str(), input_meshset );
    assert( MB_SUCCESS == result );

  result = quads_to_tris( MBI, input_meshset );
    assert( MB_SUCCESS == result );


  // Write the file that has been converted from quads to tris.
  // Cut off the .h5m
  int len1 = filename.length();
  filename.erase(len1 - 4);
  std::string filename_new = filename + "_tris.h5m";
  result = MBI->write_mesh( filename_new.c_str());
    assert(MB_SUCCESS == result);

  return 0;
}
MBInterface* mb_instance() {
  static MBCore inst;
  return &inst;
}