/** \file   ReadHDF5Dataset.hpp
 *  \author Jason Kraftcheck 
 *  \date   2010-07-09
 */

#ifndef moab_READ_HDF5DATASET_HPP
#define moab_READ_HDF5DATASET_HPP

#ifdef USE_MPI
# include <moab_mpi.h>
#endif

#include <H5Spublic.h>
#include <H5Ppublic.h>
#include <stdlib.h> // for size_t

#include "moab/Range.hpp"
#include <vector>

namespace moab {

/**\brief Utility used for reading portions of an HDF5 dataset
 *
 * Implement iterative read of table where:
 * - subset of rows to be read can be specified usign an Range of offsets
 * - each read fills as much as possible of a passed buffer 
 * - each read call reads a subsequent set of rows of the data set in an
 *   iterator-like fashion.
 *
 * NOTE: This class also implements an RAII pattern for the data set handle:
 *       It will close the data set in its destructor unless it is specified
 *       to the constructor that only a single column should be read.
 */
class ReadHDF5Dataset 
{
public:

#ifdef USE_MPI
  typedef MPI_Comm Comm;
#else
  typedef int Comm;
#endif

  class Exception { public: int line_no; Exception(int l) : line_no(l) {} };
  
  /**\brief Setup to read entire table
   *\param data_set_handle The HDF5 DataSet to read.
   *\param param     The data type of the buffer into which table values
   *                 are to be read.
   *\param close_data_set_on_destruct Call \c H5Dclose on passed
   *                 \c data_set_handle in desturctor.
   *
   *\NOTE Class instance will not be usable until one of either
   *      \c set_file_ids or \c set_all_file_ids is called.
   */
  ReadHDF5Dataset( hid_t data_set_handle ,
                   hid_t data_type,
                   bool close_data_set_on_destruct = true );
  
  bool will_close_data_set() const { return closeDataSet; }
  void close_data_set_on_destruct( bool val ) { closeDataSet = true; }
  
  ~ReadHDF5Dataset();
  
  
  /**\brief Change file ids to read from. 
   *
   *\param io_prop     Read property taht will be passed to read calls.
   *\param communictor If \c io_prop is \c H5FD_MPIO_COLLECTIVE, then this
   *                   must be a pointer to the MPI_Communicator value.
   */
  void set_file_ids( const Range& file_ids, 
                     EntityHandle start_id,
                     hid_t io_prop = H5P_DEFAULT,
                     const Comm* communicator = 0 );
  
  void set_all_file_ids( hid_t io_prop = H5P_DEFAULT, const Comm* communicator = 0 );
  
  /**\brief Return false if more data to read, true otherwise
   *
   * Test if the iterative read has reached the end.
   */
  bool done() const { return (currOffset == rangeEnd); }
  
  /**\brief Read rows of table
   *
   * Read up to max_num_rows from data set.
   *\param buffer    Memory in which to store values read from data set
   *\param type      HDF5 data type in which to store values in memory
   *                 (data type of values in memory pointed to by \c buffer)
   *\param max_rows  The maximum number of rows that will fit in \c buffer
   *\param rows_read The actual number of rows read from the table.  Will
   *                 never exceed \c max_rows .
   *\param io_prop   Used to request collective IO or other special read
   *                 options.
   */
  void read( void* buffer, 
             size_t max_rows,
             size_t& rows_read );
  
  /**\brief Return position in \c Range of file IDs at which next read will start
   */
  Range::const_iterator next_file_id() const { return currOffset; }
  
  /**\brief Do null read operation
   *
   * Do a read call requesting no data.  This functionality is provided
   * so as to allow collective IO when not all processes need to make the
   * same number of read calls.  To prevent deadlock in this case, processes
   * that have finished their necessary read calls can call this function
   * so that all processes are calling the read method collectively.
   */
  void null_read();
  
  void set_data_type( hid_t type ) { dataType = type; }
  
  unsigned columns() const;
  void set_column( unsigned c );

  enum Mode {
    HYPERSLAB = 0, //!< Use H5Sselect_hyperslab for reading subsets
    POINT = 1,     //!< Use H5Sselect_elements for reading subsets
    CONTIGUOUS = 2 //!< Read contiguous blocks of data and eliminate unwanted values
  };
  
  Mode get_mode() const { return ioMode; }
  
  const char* get_mode_str() const;

private:
  
  Mode ioMode;

  void init( int column = -1 );
  
  void read_hyperslab( void* buffer, 
                       size_t max_rows,
                       size_t& rows_read );
  
  void read_point( void* buffer, 
                   size_t max_rows,
                   size_t& rows_read );
  
  void read_contig( void* buffer, 
                    size_t max_rows,
                    size_t& rows_read );

  Range internalRange; //!< used when reading entire dataset
  std::vector<hsize_t> selectData; //!< array used for H5Sselect_elements

  bool closeDataSet; //!< close dataset in destructor
  hsize_t dataSetOffset[H5S_MAX_RANK], dataSetCount[H5S_MAX_RANK];
  hid_t dataSet, dataSpace, dataType, ioProp;
  int dataSpaceRank;
  hsize_t rowsInTable;
  
  Range::const_iterator currOffset, rangeEnd;
  EntityHandle startID;
}; 



} // namespace moab

#endif // moab_READ_HDF5DATASET_HPP