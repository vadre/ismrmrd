#ifndef ICP_SESSION_H
#define ICP_SESSION_H

#include <boost/asio.hpp>
#include <thread>
#include <functional>
#include <memory>
#include "icpMTQueue.h"
#include <utility>
#include <map>
#include <time.h>
#include "ismrmrd/version.h"
#include "ismrmrd/xml.h"
#include "icpCallback.h"

namespace ISMRMRD { namespace ICP
{
/*******************************************************************************
 ******************************************************************************/
using boost::asio::ip::tcp;
using SOCKET_PTR = std::shared_ptr<boost::asio::ip::tcp::socket>;

using STYPE      = StorageType;
using ETYPE      = EntityType;
using ENTITY     = Entity;
using HANDSHAKE  = Handshake;
using COMMAND    = Command;
using ERRREPORT  = ErrorReport;
using XMLHEAD    = IsmrmrdHeader;
//using WAVEFORM  = Waveform;
//using BLOB      = Blob;

/*******************************************************************************
 ******************************************************************************/
const size_t        DATAFRAME_SIZE_FIELD_SIZE     = sizeof (uint64_t);
const size_t        ENTITY_HEADER_SIZE            = sizeof (uint32_t) * 4 +
                                                    sizeof (ISMRMRD_DATA_ID);
const uint32_t      ERROR_SOCKET_EOF              = 100;
const uint32_t      ERROR_SOCKET_WRONG_LENGTH     = 200;
const uint32_t      ENTITY_WITH_NO_DATA           = 300;

/*******************************************************************************
 ******************************************************************************/
struct CbBase
{
  virtual ~CbBase() = default;
};

template <typename ...A>
struct CbStruct : public CbBase
{
  using CB = std::function <void (A...)>;
  CB callback;
  CbStruct (CB p_callback) : callback (p_callback) {}
};

using CB = CbStruct <Callback*, ENTITY*>;
using CB_MAP = std::map<uint32_t, std::unique_ptr<CbBase> >;
using OB_MAP = std::map<uint32_t, Callback*>;

/*******************************************************************************
 * Byte order utilities - DO NOT MODIFY!!!
 ******************************************************************************/
const int __one__ = 1;
const bool isCpuLittleEndian = 1 == *(char*)(&__one__);

/*******************************************************************************
 * Common Server and Client libraries structures
 ******************************************************************************/
struct InputMessageStruct
{
  uint64_t                   size;
  EntityHeader      ehdr;
  uint32_t                   data_size;
  std::vector<unsigned char> data;
};
typedef InputMessageStruct   IN_MSG;

struct OutputMessageStruct
{
  uint32_t                   size;
  std::vector<unsigned char> data;
};
typedef OutputMessageStruct  OUT_MSG;

/*******************************************************************************
 ******************************************************************************/
class Session
{
  public:

         Session      (SOCKET_PTR sock);
         ~Session     ();
  void   run             ();
  void   shutdown        ();
  bool   send            (ENTITY*);
  template <typename F, typename E>
  void   registerHandler (F func, E etype, Callback*);

  private:

  bool     getMessage       ();
  uint32_t getAcqStream     (ENTITY*, STYPE);
  uint32_t receiveFrameInfo (IN_MSG& in_msg);
  void     deliver          (IN_MSG  in_msg);
  void     transmit         ();
  void     queueMessage     (std::vector<unsigned char> ent,
                             std::vector<unsigned char> data);
  template<typename ...A>
  void     call             (uint32_t index, A&& ... args);
  

  SOCKET_PTR                _sock;
  bool                      _stop;
  MTQueue<OUT_MSG>       _oq;
  std::thread               _transmitter;
  CB_MAP                    _callbacks;
  OB_MAP                    _objects;
};

using SESSION = std::unique_ptr<Session>;
using START_USER_APP_FUNC = void (*) (SESSION);

/*******************************************************************************
 * Templates
 ******************************************************************************/
template <typename F, typename E>
void Session::registerHandler
(
  F            func,
  E            etype,
  Callback* obj
)
{
  std::unique_ptr<F> f_uptr (new F (func));
  _callbacks.insert (CB_MAP::value_type (etype, std::move (f_uptr)));
  _objects.insert (OB_MAP::value_type (etype, obj));
  return;
}

}} // end of namespace ISMRMRD::ICP
#endif // ICP_SESSION_H
