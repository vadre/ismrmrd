#ifndef ICP_H
#define ICP_H

#include <iostream>
#include <boost/asio.hpp>
#include "ismrmrd/ismrmrd.h"
#include "ismrmrd/xml.h"

using boost::asio::ip::tcp;

using SOCKET_PTR = std::shared_ptr<boost::asio::ip::tcp::socket>;
using STYPE      = ISMRMRD::StorageType;
using ETYPE      = ISMRMRD::EntityType;
using ENTITY     = ISMRMRD::Entity;
using HANDSHAKE  = ISMRMRD::Handshake;
using COMMAND    = ISMRMRD::Command;
using ERRNOTE    = ISMRMRD::ErrorNotification;
using XMLHEAD    = ISMRMRD::IsmrmrdHeader;
using XMLWRAP    = ISMRMRD::IsmrmrdHeaderWrapper;
//using WAVEFORM  = ISMRMRD::Waveform;
//using BLUB      = ISMRMRD::Blub;

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
