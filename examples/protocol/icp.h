#ifndef ICP_H
#define ICP_H
// icp.h 

#include <iostream>
#include <boost/asio.hpp>
#include "ismrmrd/ismrmrd.h"

using boost::asio::ip::tcp;

const unsigned char my_version   =  2;

// Dataframe size is of type uint64_t
const size_t   DATAFRAME_SIZE_FIELD_SIZE  =  sizeof (uint64_t);

// Entity Header size with 0 padding is sizeof (uint32_t) times 4
const size_t   ENTITY_HEADER_SIZE         = sizeof (uint32_t) * 4;

const int __one__ = 1;
const bool isCpuLittleEndian = 1 == *(char*)(&__one__);


typedef void* USER_DATA;
typedef template<typaname T> bool (*f) (T)  SEND_MSG_CALLBACK;
typedef std::shared_ptr<boost::asio::ip::tcp::socket> SOCKET_PTR;

struct INPUT_MESSAGE_STRUCTURE
{
  uint64_t                   size;
  ISMRMRD::EntityHeader      ehdr;
  uint32_t                   data_size;
  std::vector<unsigned char> data;
};

typedef INPUT_MESSAGE_STRUCTURE   IN_MSG;


struct OUTPUT_MESSAGE_STRUCTURE
{
  uint32_t                   size;
  std::vector<unsigned char> data;
};

typedef OUTPUT_MESSAGE_STRUCTURE  OUT_MSG;

#endif // ICP_H */
