#include "DebugOutput.hpp"
#include "moab/Range.hpp"
#include "moab/CN.hpp"
#include "Internals.hpp"
#include <iostream>
#include <string.h>
#include <algorithm>

#ifdef USE_MPI
#  include "moab_mpi.h"
#endif

namespace moab {

DebugOutputStream::~DebugOutputStream() {}

class FILEDebugStream : public DebugOutputStream
{
private:
  FILE* filePtr;
public:
  FILEDebugStream( FILE* filep ) : filePtr(filep) {}
  void println( int rank, const char* pfx, const char* str );
  void println( const char* pfx, const char* str );
};
void FILEDebugStream::println( int rank, const char* pfx, const char* str )
  { fprintf(filePtr, "%3d  %s%s\n", rank, pfx, str); fflush(filePtr); }
void FILEDebugStream::println( const char* pfx, const char* str )
  { 
    fputs( pfx, filePtr ); 
    fputs( str, filePtr ); 
    fputc( '\n', filePtr ); 
    fflush(filePtr); 
  }


class CxxDebugStream : public DebugOutputStream
{
private:
  std::ostream& outStr;
public:
  CxxDebugStream( std::ostream& str ) : outStr(str) {}
  void println( int rank, const char* pfx, const char* str );
  void println( const char* pfx, const char* str );
};
void CxxDebugStream::println( int rank, const char* pfx, const char* str )
  { 
    outStr.width(3);
    outStr << rank << "  "  << pfx << str << std::endl; 
    outStr.flush(); 
  }
void CxxDebugStream::println( const char* pfx, const char* str )
  { outStr << pfx << str << std::endl; outStr.flush(); }


DebugOutput::DebugOutput( DebugOutputStream* impl, unsigned verbosity )
  : outputImpl(impl), deleteImpl(0), mpiRank(-1), verbosityLimit(verbosity) {}
DebugOutput::DebugOutput( DebugOutputStream* impl, int rank, unsigned verbosity )
  : outputImpl(impl), deleteImpl(0), mpiRank(rank), verbosityLimit(verbosity) {}
DebugOutput::DebugOutput( FILE* impl, unsigned verbosity )
  : outputImpl(new FILEDebugStream(impl)), deleteImpl(outputImpl),
    mpiRank(-1), verbosityLimit(verbosity) {}
DebugOutput::DebugOutput( FILE* impl, int rank, unsigned verbosity )
  : outputImpl(new FILEDebugStream(impl)), deleteImpl(outputImpl),
    mpiRank(rank), verbosityLimit(verbosity) {}
DebugOutput::DebugOutput( std::ostream& str, unsigned verbosity )
  : outputImpl(new CxxDebugStream(str)), deleteImpl(outputImpl),
    mpiRank(-1), verbosityLimit(verbosity) {}
DebugOutput::DebugOutput( std::ostream& str, int rank, unsigned verbosity )
  : outputImpl(new CxxDebugStream(str)), deleteImpl(outputImpl),
    mpiRank(rank), verbosityLimit(verbosity) {}
DebugOutput::DebugOutput( const char* pfx, DebugOutputStream* impl, unsigned verbosity )
  : linePfx(pfx), outputImpl(impl), deleteImpl(0), 
    mpiRank(-1), verbosityLimit(verbosity) {}
DebugOutput::DebugOutput( const char* pfx, DebugOutputStream* impl, int rank, unsigned verbosity )
  : linePfx(pfx), outputImpl(impl), deleteImpl(0), 
     mpiRank(rank), verbosityLimit(verbosity) {}
DebugOutput::DebugOutput( const char* pfx, FILE* impl, unsigned verbosity )
  : linePfx(pfx), outputImpl(new FILEDebugStream(impl)), deleteImpl(outputImpl),
    mpiRank(-1), verbosityLimit(verbosity) {}
DebugOutput::DebugOutput( const char* pfx, FILE* impl, int rank, unsigned verbosity )
  : linePfx(pfx), outputImpl(new FILEDebugStream(impl)), deleteImpl(outputImpl),
    mpiRank(rank), verbosityLimit(verbosity) {}
DebugOutput::DebugOutput( const char* pfx, std::ostream& str, unsigned verbosity )
  : linePfx(pfx), outputImpl(new CxxDebugStream(str)), deleteImpl(outputImpl),
    mpiRank(-1), verbosityLimit(verbosity) {}
DebugOutput::DebugOutput( const char* pfx, std::ostream& str, int rank, unsigned verbosity )
  : linePfx(pfx), outputImpl(new CxxDebugStream(str)), deleteImpl(outputImpl),
    mpiRank(rank), verbosityLimit(verbosity) {}

DebugOutput::~DebugOutput()
{ 
  if (!lineBuffer.empty()) {
    lineBuffer.push_back('\n');
    process_line_buffer();
  }
  delete deleteImpl; 
}
  
void DebugOutput::use_world_rank() 
{
  mpiRank = 0;
#ifdef USE_MPI
  MPI_Comm_rank( MPI_COMM_WORLD, &mpiRank );
#endif
}   

void DebugOutput::print_real( const char* buffer )
{
  lineBuffer.insert( lineBuffer.end(), buffer, buffer + strlen(buffer) );
  process_line_buffer();
}
  
void DebugOutput::print_real( const std::string& str )
{
  lineBuffer.insert( lineBuffer.end(), str.begin(), str.end() );
  process_line_buffer();
}

void DebugOutput::print_real( const char* fmt, va_list args )
{
  size_t idx = lineBuffer.size();
#ifdef HAVE_VSNPRINTF
    // try once with remaining space in buffer
  lineBuffer.resize( lineBuffer.capacity() );
  unsigned size = vsnprintf( &lineBuffer[idx], lineBuffer.size() - idx, fmt, args );
    // if necessary, increase buffer size and retry
  if (size > (lineBuffer.size() - idx)) {
    lineBuffer.resize( idx + size );
    size = vsnprintf( &lineBuffer[idx], lineBuffer.size() - idx, fmt, args );
  }
#else
    // Guess how much space might be required.
    // If every character is a format code then there are len/3 format codes.
    // Guess a random large value of 81 characters per formatted argument.
  unsigned exp_size = 27*strlen(fmt);
  lineBuffer.resize( idx + exp_size );
  unsigned size = vsprintf( &lineBuffer[idx], fmt, args );
    // check if we overflowed the buffer
  if (size > exp_size) {
    // crap!
   fprintf(stderr,"ERROR: Buffer overflow at %s:%d\n", __FILE__, __LINE__);
   lineBuffer.resize( idx + exp_size );
   size = vsprintf( &lineBuffer[idx], fmt, args );
  }
#endif

    // less one because we don't want the trailing '\0'
  lineBuffer.resize(size-1);
  process_line_buffer();
}

void DebugOutput::list_range_real( const char* pfx, const Range& range )
{
  if (range.empty()) {
    print_real("<empty>\n");
    return;
  }

  if (pfx) {
    lineBuffer.insert( lineBuffer.end(), pfx, pfx+strlen(pfx) );
    lineBuffer.push_back(' ');
  }

  char numbuf[48]; // unsigned 64 bit integer can't have more than 20 decimal digits
  Range::const_pair_iterator i;
  EntityType type = MBMAXTYPE;
  for (i = range.const_pair_begin(); i != range.const_pair_end(); ++i) {
    if (TYPE_FROM_HANDLE(i->first) != type) {
      type = TYPE_FROM_HANDLE(i->first);
      const char* name = CN::EntityTypeName(type);
      lineBuffer.insert( lineBuffer.end(), name, name+strlen(name) );
    }
    if (i->first == i->second) {
      sprintf(numbuf, " %lu,", (unsigned long)(ID_FROM_HANDLE(i->first)));
    }
    else {
      sprintf(numbuf, " %lu - %lu,", 
        (unsigned long)(ID_FROM_HANDLE(i->first)),
        (unsigned long)(ID_FROM_HANDLE(i->second)));
    }
    lineBuffer.insert( lineBuffer.end(), numbuf, numbuf+strlen(numbuf) );
  }

  lineBuffer.push_back('\n');
  process_line_buffer();
}
 
void DebugOutput::list_ints_real( const char* pfx, const Range& range )
{
  if (range.empty()) {
    print_real("<empty>\n");
    return;
  }

  if (pfx) {
    lineBuffer.insert( lineBuffer.end(), pfx, pfx+strlen(pfx) );
    lineBuffer.push_back(' ');
  }

  char numbuf[48]; // unsigned 64 bit integer can't have more than 20 decimal digits
  Range::const_pair_iterator i;
  for (i = range.const_pair_begin(); i != range.const_pair_end(); ++i) {
    if (i->first == i->second) 
      sprintf(numbuf, " %lu,", (unsigned long)(i->first));
    else
      sprintf(numbuf, " %lu - %lu,", (unsigned long)(i->first), (unsigned long)(i->second));
    lineBuffer.insert( lineBuffer.end(), numbuf, numbuf+strlen(numbuf) );
  }

  lineBuffer.push_back('\n');
  process_line_buffer();
}
   
void DebugOutput::process_line_buffer()
{
  size_t last_idx = 0;
  std::vector<char>::iterator i;
  for (i = std::find(lineBuffer.begin(), lineBuffer.end(), '\n');
       i != lineBuffer.end();  i = std::find(i, lineBuffer.end(), '\n')) {
    *i = '\0';
    if (have_rank()) 
      outputImpl->println( get_rank(), linePfx.c_str(), &lineBuffer[last_idx] );
    else
      outputImpl->println( linePfx.c_str(), &lineBuffer[last_idx] );
    ++i;
    last_idx = i - lineBuffer.end();
  }
  
  if (last_idx) {
    i = std::copy( lineBuffer.begin()+last_idx, lineBuffer.end(), lineBuffer.begin() );
    lineBuffer.erase( i, lineBuffer.end() );
  }
}

} // namespace moab