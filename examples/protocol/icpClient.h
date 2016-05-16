#ifndef ICP_CLIENT_H
#define ICP_CLIENT_H

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
using BEGIN_INPUT_CALLBACK_FUNC = bool (*) (USER_DATA);

/*******************************************************************************
 * Entity handlers management
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

//using CB_ALLOCATE = CB_STRUCT <USER_DATA*>;
//using CB_SEND_CB  = CB_STRUCT <SEND_MSG_CALLBACK,             USER_DATA>;

using CB_IMG_16   = CB_STRUCT <ISMRMRD::Image<int16_t>,       USER_DATA>;
using CB_IMG_32   = CB_STRUCT <ISMRMRD::Image<int32_t>,       USER_DATA>;
using CB_IMG_FLT  = CB_STRUCT <ISMRMRD::Image<float>,         USER_DATA>;
using CB_HANDSHK  = CB_STRUCT <ISMRMRD::Handshake,            USER_DATA>;
using CB_ERRNOTE  = CB_STRUCT <ISMRMRD::ErrorNotification,    USER_DATA>;
using CB_COMMAND  = CB_STRUCT <ISMRMRD::Command,              USER_DATA>;
using CB_XMLHEAD  = CB_STRUCT <ISMRMRD::IsmrmrdHeader,        USER_DATA>;

/*******************************************************************************
 ******************************************************************************/
class icpClient
{
public:

  using GET_USER_DATA_FUNC = bool (*) (USER_DATA*, icpClient*);
  //using SET_SEND_CALLBACK_FUNC = bool (*) ( bool (icpClient::*callback)
  //using SET_SEND_CALLBACK_FUNC = bool (*) ( bool (*)
                               //(ISMRMRD::EntityType, ISMRMRD::Entity*), USER_DATA);

  icpClient (std::string host, uint16_t port);
  ~icpClient ();
  void connect ();

  void setSessionClosed();
  bool isSessionClosed ();
  bool registerUserDataAllocator (GET_USER_DATA_FUNC func_ptr);
  bool registerInputProvider (BEGIN_INPUT_CALLBACK_FUNC func_ptr);
  bool forwardMessage (ISMRMRD::EntityType type, ISMRMRD::Entity* entity);

  template <typename F, typename E, typename S>
  void registerHandler (F func, E etype, S stype);

private:

  static bool                  _running;
  std::string                  _host;
  uint16_t                     _port;

  bool                         _user_data_allocator_registered;
  bool                         _data_input_func_registered;
  bool                         _session_closed;
  USER_DATA*                   _user_data;
  CB_MAP                       _callbacks;
  std::queue<OUT_MSG>          _oq;
  std::thread                  _receive_thread;


  GET_USER_DATA_FUNC           getUserDataPointer;
  BEGIN_INPUT_CALLBACK_FUNC    beginDataInput;                   // Thread

  //SET_SEND_CALLBACK_FUNC       setSendMessageCallback;


  void      receive          (SOCKET_PTR sock);                  // Thread
  uint32_t  receiveMessage   (SOCKET_PTR sock, IN_MSG& in_msg);
  uint32_t  receiveFrameInfo (SOCKET_PTR sock, IN_MSG& in_msg);
  void      transmit         (SOCKET_PTR sock);                  // Thread
  void      queueMessage     (std::vector<unsigned char>& ent,
                              std::vector<unsigned char>& data);

  template<typename ...A>
  void call (uint32_t index, A&& ... args);
};

/*******************************************************************************
 ******************************************************************************/
template <typename F, typename E, typename S>
void icpClient::registerHandler
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

#endif // ICP_CLIENT_H */
