#include "MBCore.hpp"
#include "MBRange.hpp"
#include <iostream>

int main(int argc, char **argv) {
  if (1 == argc) {
    std::cout << "Usage: " << argv[0] << " <filename>" << std::endl;
    return 0;
  }

    // instantiate & load a mesh from a file
  MBCore *mb = new MBCore();
  MBErrorCode rval = mb->load_mesh(argv[1]);

  MBRange ents;

    // iterate over dimensions
  for (int d = 0; d <= 3; d++) {
    ents.clear();
    rval = mb->get_entities_by_dimension(0, d, ents);
    std::cout << "Found " << ents.size() << " " << d << "-dimensional entities:" << std::endl;
    for (MBRange::iterator it = ents.begin(); it != ents.end(); it++) {
      MBEntityHandle ent = *it;
      std::cout << "Found d=" << d << " entity " 
                << mb->id_from_handle(ent) << "." << std::endl;
    }
  }
  return 0;
}
