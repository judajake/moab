#include "TestUtil.hpp"
#include "moab/Core.hpp"

using namespace moab;

#ifdef MESHDIR
static const char example[] = STRINGIFY(MESHDIR) "/io/cam18x40x48.t2.nc";
#else
static const char example[] = "/io/cam18x40x48.nc";
#endif

void read_file( Interface& moab, const char* input_file );
void test_read_all();
void test_read_onevar();
void test_read_onetimestep();
void test_read_nomesh();

int main()
{
  int result = 0;
  
  result += RUN_TEST(test_read_all);
  result += RUN_TEST(test_read_onevar);
  result += RUN_TEST(test_read_onetimestep);
  result += RUN_TEST(test_read_nomesh);
  
  return result;
}


void test_read_all()
{
  Core moab;
  Interface& mb = moab;

  ErrorCode rval = mb.load_file( example );
  CHECK_ERR(rval);
  
    // check for proper tags
  Tag Ttag0, Ttag1;
  rval = mb.tag_get_handle("T0", 1, MB_TYPE_DOUBLE, Ttag0);
  CHECK_ERR(rval);
  
  rval = mb.tag_get_handle("T1", 1, MB_TYPE_DOUBLE, Ttag1);
  CHECK_ERR(rval);
}

void test_read_onevar() 
{
  Core moab;
  Interface& mb = moab;
  ErrorCode rval = mb.load_file( example, NULL, "VARIABLE=T" );
  CHECK_ERR(rval);
  
    // check for proper tags
  Tag Ttag0, Ttag1;
  rval = mb.tag_get_handle("T0", 1, MB_TYPE_DOUBLE, Ttag0);
  CHECK_ERR(rval);
  
  rval = mb.tag_get_handle("T1", 1, MB_TYPE_DOUBLE, Ttag1);
  CHECK_ERR(rval);
}

void test_read_onetimestep()
{
  Core moab;
  Interface& mb = moab;
  ErrorCode rval = mb.load_file( example, NULL, "TIMESTEP=1" );
  CHECK_ERR(rval);
  
    // check for proper tags
  Tag Ttag0, Ttag1;
  rval = mb.tag_get_handle("T0", 1, MB_TYPE_DOUBLE, Ttag0);
  CHECK_EQUAL(rval, MB_TAG_NOT_FOUND);
  
  rval = mb.tag_get_handle("T1", 1, MB_TYPE_DOUBLE, Ttag1);
  CHECK_ERR(rval);
}

void test_read_nomesh() 
{
  Core moab;
  Interface& mb = moab;

    // need a set for nomesh to work right
  EntityHandle set;
  ErrorCode rval = mb.create_meshset(MESHSET_SET, set);
  CHECK_ERR(rval);
  
  rval = mb.load_file( example, &set, "TIMESTEP=0" );
  CHECK_ERR(rval);
  
    // check for proper tag
  Tag Ttag0, Ttag1;
  rval = mb.tag_get_handle("T0", 1, MB_TYPE_DOUBLE, Ttag0);
  CHECK_ERR(rval);
  
  rval = mb.tag_get_handle("T1", 1, MB_TYPE_DOUBLE, Ttag1);
  CHECK_EQUAL(rval, MB_TAG_NOT_FOUND);

    // now read 2nd timestep with nomesh option
  rval = mb.load_file( example, &set, "TIMESTEP=1;NOMESH" );
  CHECK_ERR(rval);
  
    // check for proper tag
  rval = mb.tag_get_handle("T1", 1, MB_TYPE_DOUBLE, Ttag1);
  CHECK_ERR(rval);
}


