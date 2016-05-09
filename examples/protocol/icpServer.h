#ifndef ICP_SERVER_H
#define ICP_SERVER_H

#include "icpInputManager.h"
#include "icpOutputManager.h"
#include <boost/asio.hpp>
#include <thread>

const uint32_t ICP_ERROR_SOCKET_EOF          = 100;
const uint32_t ICP_ERROR_SOCKET_WRONG_LENGTH = 200;
const uint32_t ICP_ENTITY_WITH_NO_DATA       = 300;

using GET_USER_DATA_FUNC = bool (*) (USER_DATA*);

using SET_SEND_CALLBACK_FUNC = bool (*) (
        bool (ICPOUTPUTMANAGER::icpOutputManager::*callback)
          (ISMRMRD::EntityType, ISMRMRD::Entity*), USER_DATA);

using ENTITY_RECEIVER_FUNC = bool (*) (ISMRMRD::Entity*, ISMRMRD::EntityType,
                                       ISMRMRD::StorageType, USER_DATA);

/*******************************************************************************
 ******************************************************************************/
class icpServer
{
public:

  icpServer (uint16_t p);
  ~icpServer ();
  void start ();

  bool registerUserDataAllocator   (GET_USER_DATA_FUNC      func_ptr);
  bool registerCallbackSetter      (SET_SEND_CALLBACK_FUNC  func_ptr);
  bool registerEntityReceiver      (ENTITY_RECEIVER_FUNC    func_ptr);

private:

  static bool                  _running;
  unsigned short               _port;
  std::thread                  _main_thread;

  bool                         _user_data_allocator_registered;
  bool                         _send_callback_setter_registered;
  bool                         _entity_receiver_registered;


  GET_USER_DATA_FUNC            getUserDataPointer;
  SET_SEND_CALLBACK_FUNC        setSendMessageCallback;
  ENTITY_RECEIVER_FUNC          forwardEntity;


  void      readSocket       (SOCKET_PTR sock, uint32_t id);
  uint32_t  receiveMessage   (SOCKET_PTR sock, IN_MSG& in_msg);
  uint32_t  receiveFrameInfo (SOCKET_PTR sock, IN_MSG& in_msg);
  void      serverMain       ();
  void      sendMessage
  (
    ICPOUTPUTMANAGER::icpOutputManager* om,
    uint32_t id,
    SOCKET_PTR sock
  );
};

#endif // ICP_SERVER_H */
