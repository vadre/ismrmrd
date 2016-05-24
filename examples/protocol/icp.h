#ifndef ICP_H
#define ICP_H
// icp.h  Need to break this file into icp_public.h and icp_internal.h

//#include "icpServerTestData.h"
//#include "icpClientTestData.h"

#include <iostream>
#include <boost/asio.hpp>
#include "ismrmrd/ismrmrd.h"
#include "ismrmrd/xml.h"

using boost::asio::ip::tcp;
using SOCKET_PTR = std::shared_ptr<boost::asio::ip::tcp::socket>;

/*******************************************************************************
 * Constants - DO NOT MODIFY!!!
 ******************************************************************************/
//const unsigned char my_version                    =  2;
const size_t        DATAFRAME_SIZE_FIELD_SIZE     = sizeof (uint64_t);
const size_t        ENTITY_HEADER_SIZE            = sizeof (uint32_t) * 4;
const uint32_t      ICP_ERROR_SOCKET_EOF          = 100;
const uint32_t      ICP_ERROR_SOCKET_WRONG_LENGTH = 200;
const uint32_t      ICP_ENTITY_WITH_NO_DATA       = 300;

/*******************************************************************************
 * Entity handlers management - DO NOT MODIFY!!!
 ******************************************************************************/
struct CB_BASE
{
  virtual ~CB_BASE() = default;
};

template <typename ...A>
struct CB_STRUCT : public CB_BASE
{
  using CB = std::function <void (A...)>;
  CB callback;
  CB_STRUCT (CB p_callback) : callback (p_callback) {}
};
using CB_MAP = std::map<uint32_t, std::unique_ptr<CB_BASE> >;

/*******************************************************************************
 * Handlers callback signatures
 * These are handler callback signatures definitions
 ******************************************************************************/
using BEGIN_INPUT_CALLBACK_FUNC = bool (*)();

using CB_MR_ACQ   = CB_STRUCT <ISMRMRD::Entity*, uint32_t>;
using CB_IMAGE    = CB_STRUCT <ISMRMRD::Entity*, uint32_t>;
using CB_HANDSHK  = CB_STRUCT <ISMRMRD::Handshake>;
using CB_ERRNOTE  = CB_STRUCT <ISMRMRD::ErrorNotification>;
using CB_COMMAND  = CB_STRUCT <ISMRMRD::Command>;
using CB_XMLHEAD  = CB_STRUCT <ISMRMRD::IsmrmrdHeader>;
/*******************************************************************************
 * Byte order utilities - DO NOT MODIFY!!!
 ******************************************************************************/
const int __one__ = 1;
const bool isCpuLittleEndian = 1 == *(char*)(&__one__);

/*******************************************************************************
 * Common Server and Client libraries structures
 ******************************************************************************/
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
