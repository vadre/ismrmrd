#include "icpInputManager.h"
#include "icpOutputManager.h"
#include <boost/asio.hpp>
#include <thread>

const uint32_t ICP_ERROR_SOCKET_EOF          = 100;
const uint32_t ICP_ERROR_SOCKET_WRONG_LENGTH = 200;
const uint32_t ICP_ENTITY_WITH_NO_DATA       = 300;

typedef void (*GET_USER_DATA_FUNC)
        (
          USER_DATA*               user_data_ptr,
        );

typedef void (*SET_SEND_CALLBACK_FUNC)
        (
          USER_DATA                user_data_ptr,
          SEND_MSG_CALLBACK        callback
        );

typedef void (*HANDSHAKE_HANDLER_FUNC)
        (
          ISMRMRD::Handshake       msg,
          USER_DATA                user_data,
        );

typedef void (*COMMAND_HANDLER_FUNC)
        (
          ISMRMRD::Command         msg,
          USER_DATA                user_data,
        );

typedef void (*ERROR_HANDLER_FUNC)
        (
          ISMRMRD::Error           msg,
          USER_DATA                user_data,
        );

typedef void (*ISMRMRD_HEADER_HANDLER_FUNC)
        (
          ISMRMRD::IsmrmrdHeader   msg,
          USER_DATA                user_data,
        );

typedef template <typename T> void (*MR_ACQUISITION_HANDLER_FUNC)
        (
          ISMRMRD::Acquisition<T>  acqs,
          USER_DATA                user_data,
        );

/*******************************************************************************
 ******************************************************************************/
class icpServer
{
public:

  icpServer (uint16_t p);
  ~icpServer ();
  void start ();

  void registerUserDataAllocator      (GET_USER_DATA_FUNC           func_ptr);
  void registerCallbackSetter         (SET_SEND_CALLBACK_FUNC       func_ptr);
  void registerIsmrmrdHeaderHandler   (ISMRMRD_HEADER_HANDLER_FUNC  func_ptr);
  void registerHandshakeHandler       (HANDSHAKE_HANDLER_FUNC       func_ptr);
  void registerCommandHandler         (COMMAND_HANDLER_FUNC         func_ptr);
  void registerErrorHandler           (ERROR_HANDLER_FUNC           func_ptr);
  void registerMrAcquisitionHandler   (MR_ACQUISITION_HANDLER_FUNC  func_ptr);

private:

  static bool                  _running;
  unsigned short               _port;
  std::thread                  _main_thread;

  bool                         _user_data_allocator_registered;
  bool                         _send_callback_setter_registered;
  bool                         _handle_handshake_registered;
  bool                         _handle_command_registered;
  bool                         _handle_error_registered;
  bool                         _handle_mracquisition_registered;
  bool                         _handle_ismrmrd_header_registered;


  GET_USER_DATA_FUNC            getUserDataPointer;
  SET_SEND_CALLBACK_FUNC        setSendMessageCallback;
  HANDSHAKE_HANDLER_FUNC        handleHandshake;
  COMMAND_HANDLER_FUNC          handleCommand;
  ERROR_HANDLER_FUNC            handleError;
  MR_ACQUISITION_HANDLER_FUNC   handleMrAcquisition;
  ISMRMRD_HEADER_HANDLER_FUNC   handleIsmrmrdHeader;


  void readSocket       (SOCKET_PTR sock, uint32_t id);
  bool receiveMessage   (SOCKET_PTR sock, IN_MSG& in_msg);
  int  receiveFrameInfo (SOCKET_PTR sock, IN_MSG& in_msg);
  void serverMain       ();
  void sendMessage
  (
    ICPOUTPUTMANAGER::icpOutputManager* om,
    uint32_t id,
    SOCKET_PTR sock
  );
};
