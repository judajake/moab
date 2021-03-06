/** \brief This test shows how to perform local point-in-element searches with MOAB's new tree searching functionality.  
 *
 * MOAB's SpatialLocator functionality performs point-in-element searches over a local or parallel mesh.
 * SpatialLocator is flexible as to what kind of tree is used and what kind of element basis functions are 
 * used to localize elements and interpolate local fields.
 */

#include <iostream>
#include <cstdlib>
#include <cstdio>

#include "moab/Core.hpp"
#include "moab/Interface.hpp"
#include "moab/Range.hpp"
#include "moab/AdaptiveKDTree.hpp"
#include "moab/ElemEvaluator.hpp"
#include "moab/LinearHex.hpp"
#include "moab/CN.hpp"
#include "moab/SpatialLocator.hpp"

using namespace moab;

#define ERR(s) if (MB_SUCCESS != rval) \
    {std::string str;mb.get_last_error(str); std::cerr << s << str << std::endl; return 1;}

int main(int argc, char **argv) {

  int num_queries = 1000000;
  
  if (argc == 1) {
    std::cout << "Usage: " << argv[0] << "<filename> [num_queries]" << std::endl;
    return 0;
  }
  else if (argc == 3) num_queries = atoi(argv[2]);

    // instantiate & load a file
  moab::Core mb;

    // load the file
  ErrorCode rval = mb.load_file(argv[argc-1]); ERR("Error loading file");
  
    // get all 3d elements in the file
  Range elems;
  rval = mb.get_entities_by_dimension(0, 3, elems); ERR("Error getting 3d elements");
  
    // create a tree to use for the location service
  AdaptiveKDTree tree(&mb);

    // specify an evaluator based on linear hexes
  ElemEvaluator el_eval(&mb);

    // build the SpatialLocator
  SpatialLocator sl(&mb, elems, &tree);
  
    // get the box extents
  BoundBox box;
  CartVect box_extents, pos;
  rval = sl.get_bounding_box(box); ERR("Problem getting tree bounding box");
  box_extents = box.bMax - box.bMin;
  
    // query at random places in the tree
  CartVect params;
  bool is_inside;
  int num_inside = 0;
  EntityHandle elem;
  for (int i = 0; i < num_queries; i++) {
    pos = box.bMin + 
        CartVect(box_extents[0]*.01*(rand()%100), box_extents[1]*.01*(rand()%100), box_extents[2]*.01*(rand()%100));
    ErrorCode tmp_rval = sl.locate_point(pos.array(), elem, params.array(), 0.0, 0.0, &is_inside);
    if (MB_SUCCESS != tmp_rval) rval = tmp_rval;
    if (is_inside) num_inside++;
  }
  
  std::cout << "Mesh contains " << elems.size() << " elements of type " 
            << CN::EntityTypeName(mb.type_from_handle(*elems.begin())) << std::endl;
  std::cout << "Bounding box min-max = (" << box.bMin[0] << "," << box.bMin[1] << "," << box.bMin[2] << ")-("
            << box.bMax[0] << "," << box.bMax[1] << "," << box.bMax[2] << ")" << std::endl;
  std::cout << "Queries inside box = " << num_inside << "/" << num_queries << " = " 
            << 100.0*((double)num_inside)/num_queries << "%" << std::endl;
}

    
  
  
  

