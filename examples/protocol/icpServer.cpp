#include "icpServer.h"

/*******************************************************************************
 ******************************************************************************/
bool  icpServer::_running  = false;

/*******************************************************************************
 ******************************************************************************/
void icpServer::sendMessage
(
  ICPOUTPUTMANAGER::icpOutputManager* om,
  uint32_t                            id,
  SOCKET_PTR                          sock
)
{
  std::cout << __func__ << ": Writer thread (" << id << ") started\n";

  struct timespec  ts;
  ts.tv_sec  =     0;
  ts.tv_nsec =     10000000;

  while (!om->isServerDone())
  {
    om->send (sock);
    nanosleep (&ts, NULL);
  }
  
  //std::cout << "Writer (" << id << "): Done!\n";
  return;
}

/*******************************************************************************
 ******************************************************************************/
int icpServer::receiveFrameInfo
(
  SOCKET_PTR sock,
  IN_MSG&    in_msg
)
{
  boost::system::error_code  error;

  int bytes_read =
    boost::asio::read (*sock,
                       boost::asio::buffer (&in_msg.size,
                                            DATAFRAME_SIZE_FIELD_SIZE),
                       error);

  if (error)
  {
    std::cout << "frame_size read status: " << error << std::endl;
  }

  if (bytes_read == 0)
  {
    std::cout << "Received EOF from client\n";
    return 100; // TODO define
  }
  else if (bytes_read != DATAFRAME_SIZE_FIELD_SIZE)
  {
    printf ("Read wrong num bytes for size: %d\n", bytes_read);
    return 1; // TODO: define
  }

  std::vector<unsigned char> buffer (ENTITY_HEADER_SIZE);
  bytes_read =
    boost::asio::read (*sock,
                       boost::asio::buffer (&buffer[0],
                                            ENTITY_HEADER_SIZE),
                       error);
  if (error)
  {
    std::cout << "EntityHeader read status: " << error << std::endl;
  }
  if (bytes_read != ENTITY_HEADER_SIZE)
  {
    // TODO: at this point communication is broken.
    printf ("Read wrong num bytes for entity header: %d\n", bytes_read);
    return 200;
  }

  in_msg.ehdr.deserialize (buffer);
  in_msg.data_size = in_msg.size - ENTITY_HEADER_SIZE;

  return 0;
}

/*******************************************************************************
 ******************************************************************************/
bool icpServer::receiveMessage
(
  SOCKET_PTR sock,
  IN_MSG&    in_msg
)
{
  boost::system::error_code  error;
  if (icpServer::receiveFrameInfo (sock, in_msg))
  {
    printf ("Error from receiveFrameInfo\n");
    return false;
  }

  if (in_msg.data_size <= 0)
  {
    std::cout << "Error! Data_size  = " << in_msg.data_size << std::endl;
    std::cout << "       EntityType = " << in_msg.ehdr.entity_type << std::endl;
    return false;
  }

  in_msg.data.resize (in_msg.data_size);
  int bytes_read = boost::asio::read (*sock,
                                      boost::asio::buffer (&in_msg.data[0],
                                      in_msg.data_size),
                                      error);
  if (error)
  {
    std::cout << "Error: message data read status: " << error << std::endl;
  }
  if (bytes_read != in_msg.data.size())
  {
    printf ("Read wrong num bytes for data: %d, instead of %lu\n",
            bytes_read, in_msg.data.size());
    return false;
  }

  return true;
}


/*******************************************************************************
 ******************************************************************************/
void icpServer::readSocket
(
  SOCKET_PTR sock,
  uint32_t   id
)
{
  std::cout << __func__ << " : Reader thread (" << id << ") started\n";

  ISMRMRD::Handshake     hand;
  ISMRMRD::Command       cmd;

  ICPOUTPUTMANAGER::icpOutputManager* om =
    new ICPOUTPUTMANAGER::icpOutputManager ();
  ICPINPUTMANAGER::icpInputManager*   im =
    new ICPINPUTMANAGER::icpInputManager ();;

  std::thread writer (&icpServer::sendMessage, this, om, id, sock);

  bool done = false;
  while (!done)
  {
    IN_MSG in_msg;
    if (!receiveMessage (sock, in_msg))
    {
      // TODO: receive detailed status and report/throw
      break;
    }

    switch (in_msg.ehdr.entity_type)
    {
      case ISMRMRD::ISMRMRD_HANDSHAKE:

        hand.deserialize (in_msg.data);
        om->setClientName (std::string (hand.client_name));
        om->setSessionTimestamp (hand.timestamp);
        if (_handle_authentication_registered)
        {
          handleAuthentication (std::string (hand.client_name), om);
        }
        break;

      case ISMRMRD::ISMRMRD_COMMAND:

        cmd.deserialize (in_msg.data);
        if (cmd.command_type == ISMRMRD::ISMRMRD_COMMAND_STOP_FROM_CLIENT)
        {
          done = true;
          om->setClientDone();
          std::cout << "Received STOP from client\n";
        }
        if (_handle_command_registered)
        {
          handleCommand (cmd.command_type, cmd.command_id, om);
        }
        break;

      case ISMRMRD::ISMRMRD_XML_HEADER:

        std::cout << "Received XML Header " << in_msg.data.size() << " bytes\n";
        im->addXmlHeader (in_msg.data);
        break;

      case ISMRMRD::ISMRMRD_MRACQUISITION:

        if (im->addToStream (in_msg.ehdr, in_msg.data))
        {
          std::cout << "Received number of acqs matching XML Header, ";
          std::vector<ISMRMRD::Acquisition<float> > acqs =
            im->getAcquisitions (in_msg.ehdr);

          if (_handle_image_reconstruct_registered)
          {
            std::cout << "Calling handleImageReconstruction\n";
            handleImageReconstruction (im->getXmlHeader(), acqs, om);
          }
          else
          {
            std::cout << "but no ImageReconstruction handler registered\n";
          }
          om->setRequestCompleted();
        }
        break;

      // TODO:
      //case ISMRMRD::ISMRMRD_ERROR:
        //break;
      //case ISMRMRD::ISMRMRD_WAVEFORM:
        //break;
      //case ISMRMRD::ISMRMRD_IMAGE:
        //break;
      //case ISMRMRD::ISMRMRD_BLOB:
        //break;

      default:

        // TODO: Throw Unexpected entity type error.
        printf ("Warning! Dropping unknown message type: %u\n\n",
                in_msg.ehdr.entity_type);
        break;

    } // switch ((*in_msg).ehdr.entity_type)
  } // while (!in_data.isRespondentDone())

  std::cout << __func__ << ": Waiting for Writer (" << id << ") to join\n";
  writer.join();
  delete (om);
  delete (im);
  std::cout << __func__ << ": Writer (" << id << ") joined, exiting\n\n\n";

  return;
}

/*******************************************************************************
 ******************************************************************************/
void icpServer::serverMain ()
{
  std::cout << __func__ << ": Running with port = " << _port << "\n\n";

  boost::asio::io_service io_service;
  tcp::acceptor a (io_service, tcp::endpoint (tcp::v4(), _port));

  for (uint32_t id = 1;; ++id)
  {
    SOCKET_PTR sock (new tcp::socket (io_service));
    a.accept (*sock);
    std::cout << __func__ << ": Connection #" << id << '\n';
    std::thread (&icpServer::readSocket, this, sock, id).detach();
  }

  return;
}

/*******************************************************************************
 ******************************************************************************/
void icpServer::register_authentication_handler
(
  AuthenticateHandlerPtr handle_authentication
)
{
  handleAuthentication = handle_authentication;
  if (handleAuthentication)
  {
    _handle_authentication_registered = true;
  }
}

/*******************************************************************************
 ******************************************************************************/
void icpServer::register_command_handler
(
  CommandHandlerPtr handle_command
)
{
  handleCommand = handle_command;
  if (handleCommand)
  {
    _handle_command_registered = true;
  }
}

/*******************************************************************************
 ******************************************************************************/
void icpServer::register_image_reconstruction_handler
(
  ImageReconHandlerPtr  handle_image_reconstruction
)
{
  handleImageReconstruction = handle_image_reconstruction;
  if (handleImageReconstruction)
  {
    _handle_image_reconstruct_registered = true;
  }
}

/*******************************************************************************
 ******************************************************************************/
void icpServer::start ()
{
  if (!_running)
  {
    _main_thread = std::thread (&icpServer::serverMain, this);
    _running = true;
  }
}


/*******************************************************************************
 ******************************************************************************/
icpServer::~icpServer ()
{
  //_shutdown = true;
  if(_main_thread.joinable())
  {
    _main_thread.join();
  }
}

/*******************************************************************************
 ******************************************************************************/
icpServer::icpServer
(
  uint16_t p
)
: _port (p),
  //_shutdown (false),
  _handle_authentication_registered (false),
  _handle_command_registered (false),
  _handle_image_reconstruct_registered (false),
  _main_thread (),
  handleAuthentication (NULL),
  handleCommand (NULL),
  handleImageReconstruction (NULL)
{}

