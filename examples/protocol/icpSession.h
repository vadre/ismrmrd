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

//TODO: enumerate errors
const uint32_t      ERROR_SOCKET_EOF              = 100;
const uint32_t      ERROR_SOCKET_WRONG_LENGTH     = 200;
const uint32_t      ENTITY_WITH_NO_DATA           = 300;
const uint32_t      ENTITY_HEAD_DESERIALIZE_ERROR = 400;
const uint32_t      NOT_ISMRMRD_DATA              = 500;

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
//using CB_MAP = std::map<uint32_t, std::unique_ptr<CbBase> >;
//using OB_MAP = std::map<uint32_t, Callback*>;
using CB_MAP = std::map<std::string, std::unique_ptr<CbBase> >;
using OB_MAP = std::map<std::string, Callback*>;
using CB_IT  = CB_MAP::const_iterator;

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

         Session         (SOCKET_PTR sock);
         ~Session        ();

  void   run             ();
  void   shutdown        ();
  bool   send            (ENTITY*);

  template <typename F>
  bool   deregister      (F func, Callback* obj, uint32_t index, uint32_t inst = 0);
  template <typename F>
  bool   registerHandler (F func, Callback* obj, uint32_t index, uint32_t inst = 0);

  private:

  bool     getMessage       ();
  uint32_t getAcqStream     (ENTITY*, STYPE);
  uint32_t receiveFrameInfo (IN_MSG& in_msg);
  CB_IT    getNextKey       (std::string prefix, CB_IT it);
  void     getSubscribers   (IN_MSG in_msg);
  void     deliver          (IN_MSG in_msg, std::string key);
  void     transmit         ();
  void     queueMessage     (std::vector<unsigned char> ent,
                             std::vector<unsigned char> data);
  template<typename ...A>
  void     call             (std::string key, A&& ... args);
  

  SOCKET_PTR             _sock;
  bool                   _stop;
  MTQueue<OUT_MSG>       _oq;
  std::thread            _transmitter;
  CB_MAP                 _callbacks;
  OB_MAP                 _objects;
};

using SESSION = std::unique_ptr<Session>;
using START_USER_APP_FUNC = void (*) (SESSION);

/*******************************************************************************
 * Templates
 ******************************************************************************/
template <typename F>
bool Session::registerHandler
(
  F         func,
  Callback* obj,
  uint32_t  index,
  uint32_t  instance
)
{
  std::string key =
    std::to_string (index) + std::string ("_") + std::to_string (instance);

  std::unique_ptr<F> f_uptr (new F (func));

  //using IT = OB_MMAP::const_iterator;
  //std::pair<IT, IT> range = _objects.equal_range (key);
  //for (IT it = range.first; it != range.second; ++it)
  //{
    //if (it->second == obj)
    //{
      //return false;
    //}
  //}
  
  if (_callbacks.find (key) != _callbacks.end() ||
      _objects.find (key)   != _objects.end())
  {
    return false;
  }

  _callbacks.insert (CB_MAP::value_type (key, std::move (f_uptr)));
  _objects.insert (OB_MAP::value_type (key, obj));
  return true;
}

/*******************************************************************************
 ******************************************************************************/
template <typename F>
bool Session::deregister
(
  F         func,
  Callback* obj,
  uint32_t  index,
  uint32_t  instance
)
{
  std::string key =
    std::to_string (index) + std::string ("_") + std::to_string (instance);

  if (_callbacks.find (key) == _callbacks.end() ||
      _objects.find (key)   == _objects.end())
  {
    return false;
  }

  _callbacks.erase (key);
  _objects.erase (key);

  return true;

  //bool ob_found = false;
  //bool cb_found = false;
  //using OB_IT = OB_MMAP::const_iterator;
  //std::pair<OB_IT, OB_IT> ob_range = _objects.equal_range (key);
  //for (OB_IT it = ob_range.first; it != ob_range.second; ++it)
  //{
    //if (it->second == obj)
    //{
      //_objects.erase (it);
      //ob_found = true;
    //}
  //}

  //using CB_IT = CB_MMAP::const_iterator;
  //std::pair<CB_IT, CB_IT> cb_range = __callbacks.equal_range (key);
  //for (CB_IT it = cb_range.first; it != cb_range.second; ++it)
  //{
    //if (*it->second == func)
    //{
      //__callbacks.erase (it);
      //cb_found = true;
    //}
  //}

  //return (ob_found && cb_found);
}

}} // end of namespace ISMRMRD::ICP
#endif // ICP_SESSION_H
