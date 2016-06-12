#ifndef ICP_SESSION_H
#define ICP_SESSION_H

#include <boost/asio.hpp>
#include <thread>
#include <functional>
#include <memory>
#include <queue>
#include <utility>
#include <map>
#include <time.h>
#include "ismrmrd/version.h"
#include "ismrmrd/xml.h"
#include "icpCallback.h"

/*******************************************************************************
 ******************************************************************************/
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

//using BEGIN_INPUT_CALLBACK_FUNC = bool (*)();

/*******************************************************************************
 ******************************************************************************/
const size_t        DATAFRAME_SIZE_FIELD_SIZE     = sizeof (uint64_t);
const size_t        ENTITY_HEADER_SIZE            = sizeof (uint32_t) * 4;
const uint32_t      ICP_ERROR_SOCKET_EOF          = 100;
const uint32_t      ICP_ERROR_SOCKET_WRONG_LENGTH = 200;
const uint32_t      ICP_ENTITY_WITH_NO_DATA       = 300;

/*******************************************************************************
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

using ICP_CB = CB_STRUCT <ENTITY*, ETYPE, STYPE>;
using CB_MAP = std::map<uint32_t, std::unique_ptr<CB_BASE> >;
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

/*******************************************************************************
 ******************************************************************************/
class icpSession
{
public:

         icpSession      (SOCKET_PTR sock);
         ~icpSession     ();
  void   beginReceiving  ();
  void   shutdown        ();
  bool   send            (ENTITY*, ETYPE, STYPE stype = ISMRMRD::ISMRMRD_CHAR);
  template <typename F, typename E>
  void   registerHandler (F func, E etype);

private:
  SOCKET_PTR                _sock;
  bool                      _stop;
  std::queue<OUT_MSG>       _oq;
  std::thread               _writer;
  CB_MAP                    _callbacks;

  bool     getMessage       ();
  uint32_t receiveFrameInfo (IN_MSG& in_msg);
  void     deliver          (IN_MSG& in_msg);
  void     transmit         ();
  void     queueMessage     (std::vector<unsigned char>& ent,
                             std::vector<unsigned char>& data);
  template<typename ...A>
  void     call             (uint32_t index, A&& ... args);
  
};

/*******************************************************************************
 * Template
 ******************************************************************************/
template <typename F, typename E>
void icpSession::registerHandler
(
  F            func,
  E            etype
)
{
  std::unique_ptr<F> f_uptr (new F (func));
  _callbacks.insert (CB_MAP::value_type (etype, std::move (f_uptr)));
  return;
}

#endif // ICP_SESSION_H
