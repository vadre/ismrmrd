#include "icpInputManager.h"
#include "icpOutputManager.h"
#include <boost/asio.hpp>
#include <thread>

typedef void (*AuthenticateHandlerPtr)
        (
          std::string                                client_name,
          ICPOUTPUTMANAGER::icpOutputManager*        om
        );

typedef void (*CommandHandlerPtr)
        (
          uint32_t                                   cmd,
          uint32_t                                   id,
          ICPOUTPUTMANAGER::icpOutputManager*        om
        );

typedef void (*ImageReconHandlerPtr)
        (
          ISMRMRD::IsmrmrdHeader                     hdr,
          std::vector<ISMRMRD::Acquisition<float> >  acqs,
          ICPOUTPUTMANAGER::icpOutputManager*        om
        );

/*******************************************************************************
 ******************************************************************************/
class icpServer
{
public:

  icpServer (uint16_t p);
  ~icpServer ();
  void start ();
  void register_authentication_handler
  (
    AuthenticateHandlerPtr handle_authentication
  );
  void register_command_handler
  (
    CommandHandlerPtr handle_command
  );
  void register_image_reconstruction_handler
  (
    ImageReconHandlerPtr  handle_image_reconstruction
  );

private:

  static bool            _running;
  unsigned short         _port;
  bool                   _handle_authentication_registered;
  bool                   _handle_command_registered;
  bool                   _handle_image_reconstruct_registered;
  std::thread            _main_thread;

  AuthenticateHandlerPtr handleAuthentication;
  CommandHandlerPtr      handleCommand;
  ImageReconHandlerPtr   handleImageReconstruction;


  void readSocket (SOCKET_PTR sock, uint32_t id);
  bool receiveMessage (SOCKET_PTR sock, IN_MSG& in_msg);
  int  receiveFrameInfo (SOCKET_PTR sock, IN_MSG& in_msg);
  void serverMain();
  void sendMessage
  (
    ICPOUTPUTMANAGER::icpOutputManager* om,
    uint32_t id,
    SOCKET_PTR sock
  );
};
