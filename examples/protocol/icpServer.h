#ifndef ICP_SERVER_H
#define ICP_SERVER_H

#include <boost/asio.hpp>
#include "icp.h"
#include "ismrmrd/xml.h"
#include <thread>
#include <map>
#include <queue>
#include <functional>
#include <memory>
#include <utility>

const uint32_t ICP_ERROR_SOCKET_EOF          = 100;
const uint32_t ICP_ERROR_SOCKET_WRONG_LENGTH = 200;
const uint32_t ICP_ENTITY_WITH_NO_DATA       = 300;

//using SEND_MSG_CALLBACK  = bool (*)(ISMRMRD::EntityType, ISMRMRD::Entity*);

/*******************************************************************************
 * Entity handlers management
 ******************************************************************************/
//using SET_SEND_CALLBACK_FUNC =
   //bool (*) (bool (ICPOUTPUTMANAGER::icpOutputManager::*callback)
             //(ISMRMRD::EntityType, ISMRMRD::Entity*), USER_DATA);

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

using CB_ACQ_16   = CB_STRUCT <ISMRMRD::Acquisition<int16_t>, USER_DATA>;
using CB_ACQ_32   = CB_STRUCT <ISMRMRD::Acquisition<int32_t>, USER_DATA>;
using CB_ACQ_FLT  = CB_STRUCT <ISMRMRD::Acquisition<float>,   USER_DATA>;
using CB_ACQ_DBL  = CB_STRUCT <ISMRMRD::Acquisition<double>,  USER_DATA>;
using CB_HANDSHK  = CB_STRUCT <ISMRMRD::Handshake,            USER_DATA>;
using CB_ERRNOTE  = CB_STRUCT <ISMRMRD::ErrorNotification,    USER_DATA>;
using CB_COMMAND  = CB_STRUCT <ISMRMRD::Command,              USER_DATA>;
using CB_XMLHEAD  = CB_STRUCT <ISMRMRD::IsmrmrdHeader,        USER_DATA>;

//using CB_ALLOCATE = CB_STRUCT <USER_DATA*>;
//using CB_SEND_CB  = CB_STRUCT <SEND_MSG_CALLBACK,             USER_DATA>;

/*******************************************************************************
 * Thread data management
 ******************************************************************************/
struct ICP_SERVER_THREAD_DATA
{
  std::string         client_name; //For debug only
  uint64_t            timestamp;   //For debug only
  uint32_t            id;          //For debug only
  SOCKET_PTR          sock;
  std::queue<OUT_MSG> oq;
  bool                session_closed;
  USER_DATA           user_data;
};
//using ICP_SERVER_HANDLE = static_cast<void*>ICP_SERVER_THREAD_DATA*;
using ICP_SERVER_HANDLE =  ICP_SERVER_THREAD_DATA*;

/*******************************************************************************
 ******************************************************************************/
class icpServer
{
public:

  using GET_USER_DATA_FUNC = bool (*) (ICP_SERVER_HANDLE, USER_DATA*, icpServer*);

  icpServer (uint16_t p);
  ~icpServer ();
  void start ();

  bool forwardMessage (ICP_SERVER_HANDLE, ISMRMRD::EntityType, ISMRMRD::Entity*);
  bool registerUserDataAllocator (GET_USER_DATA_FUNC func_ptr);
  void closeConnection (ICP_SERVER_HANDLE);
  //bool registerCallbackSetter      (SET_SEND_CALLBACK_FUNC  func_ptr);
  template <typename F, typename E, typename S>
  void registerHandler            (F func, E etype, S stype);

private:

  static bool                  _running;
  unsigned short               _port;
  std::thread                  _main_thread;

  bool                         _user_data_allocator_registered;
  //bool                         _send_callback_setter_registered;
  CB_MAP                       _callbacks;

  GET_USER_DATA_FUNC            getUserDataPointer;
  //SET_SEND_CALLBACK_FUNC        setSendMessageCallback;
  void      serverMain       ();                                    //thread
  void      receive          (SOCKET_PTR sock, uint32_t id);        //threads
  void      transmit         (ICP_SERVER_THREAD_DATA* thread_data); //threads

  uint32_t  receiveMessage   (SOCKET_PTR sock, IN_MSG& in_msg);
  uint32_t  receiveFrameInfo (SOCKET_PTR sock, IN_MSG& in_msg);

  void      queueMessage     (ICP_SERVER_THREAD_DATA* thread_data,
                              std::vector<unsigned char>& ent,
                              std::vector<unsigned char>& data);
  template<typename ...A>
  void      call             (uint32_t index, A&& ... args);
};

/*******************************************************************************
 * Template
 ******************************************************************************/
template <typename F, typename E, typename S>
void icpServer::registerHandler
(
  F func,
  E etype,
  S stype
)
{
  std::unique_ptr<F> f_uptr(new F(func));
  uint32_t index = etype * 100 + stype; //TODO
  _callbacks.insert (CB_MAP::value_type(index, std::move(f_uptr)));

  return;
}

#endif // ICP_SERVER_H */
