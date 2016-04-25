#include "icpServer.h"

/*******************************************************************************
 ******************************************************************************/
bool  icpServer::_running  = false;

/*******************************************************************************
 ******************************************************************************/
bool icpServer::isServerDone ()
{
  return _server_done;
}

/*******************************************************************************
 ******************************************************************************/
void icpServer::setServerDone ()
{
  _server_done = true;
}

/*******************************************************************************
 ******************************************************************************/
void icpServer::shutdown ()
{
  _shutdown = true;
}

/*******************************************************************************
 ******************************************************************************/
void icpServer::sendMessage
(
  ICPOUTPUTMANAGER::icpOutputManager& om,
  uint32_t                            id,
  SOCKET_PTR                          sock
)
{
  //int step = 0;
  std::cout << "Writer thread (" << id << ") started\n" << std::flush;

  struct timespec   ts;
  //ts.tv_sec  =    1;
  //ts.tv_nsec =    0;
  ts.tv_sec  =    0;
  ts.tv_nsec =    10000000;

  while (!om.isServerDone())
  {
    //std::cout << __func__ << step++ << '\n';
    om.send (sock);
    nanosleep (&ts, NULL);
    //std::cout << __func__ << step++ << '\n';
  }
  
  //std::cout << __func__ << step++ << '\n';
  std::cout << "Writer (" << id << "): Done!\n";
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
  int bytes_read = 0;
  boost::system::error_code  error;
  bytes_read =
    boost::asio::read (*sock,
                       boost::asio::buffer (&in_msg.size,
                                            DATAFRAME_SIZE_FIELD_SIZE),
                       error);

  if (bytes_read != DATAFRAME_SIZE_FIELD_SIZE)
  {
    printf ("Read wrong num bytes for size: %d\n", bytes_read);
  }
  if (error)
  {
    // TODO: Throw an error
    std::cout << "frame_size read status: " << error << std::endl;
    return 1;
  }

  //printf ("receiveFrameInfo: frame size: %llu\n", in_msg.size);

  std::vector<unsigned char> buffer (ENTITY_HEADER_SIZE);
  bytes_read =
    boost::asio::read (*sock,
                       boost::asio::buffer (&buffer[0],
                                            ENTITY_HEADER_SIZE),
                       error);
  if (error)
  {
    // TODO: Throw an error
    std::cout << "EntityHeader read status: " << error << std::endl;
    return 1;
  }
  if (bytes_read != ENTITY_HEADER_SIZE)
  {
    printf ("Read wrong num bytes for entity header: %d\n", bytes_read);
  }

  in_msg.ehdr.deserialize (buffer);

  in_msg.data_size = in_msg.size - ENTITY_HEADER_SIZE;

  //printf ("data size = %u\n\n", in_msg.data_size);

  return 0;
}

/*******************************************************************************
 ******************************************************************************/
void icpServer::receiveMessage
(
  SOCKET_PTR sock,
  IN_MSG&    in_msg
)
{
  int bytes_read = 0;
  boost::system::error_code  error;
  if (icpServer::receiveFrameInfo (sock, in_msg))
  {
    // An error
    printf ("Error from receiveFrameInfo\n");
    return;
  }

  if (in_msg.data_size <= 0)
  {
    // Could use this as End Of Stream
    // TODO: Throw an error (unless we can have an entity_header as a command ??)
    std::cout << "Error! Data_size  = " << in_msg.data_size << std::endl;
    std::cout << "       EntityType = " << in_msg.ehdr.entity_type << std::endl;
    return;
  }

/*
  printf ("%s\n%s = %u\n%s = %u\n%s = %u\n%s = %u\n%s = %u\n",
          "Entity header:",
          "version  ", in_msg.ehdr.version,
          "entity   ", in_msg.ehdr.entity_type,
          "storage  ", in_msg.ehdr.storage_type,
          "stream   ", in_msg.ehdr.stream,
          "data size", in_msg.data_size);
*/

  in_msg.data.resize (in_msg.data_size);
  bytes_read = boost::asio::read (*sock,
                                  boost::asio::buffer (&in_msg.data[0],
                                                       in_msg.data_size),
                                  error);
  if (error)
  {
    // TODO: Throw an error
    std::cout << "message data read status: " << error << std::endl;
  }

  if (bytes_read != in_msg.data.size())
  {
    printf ("Read wrong num bytes for data: %d, instead of %lu\n",
            bytes_read, in_msg.data.size());
  }

  //std::cout << __func__ << " - done\n";
  return;
}


/*******************************************************************************
 ******************************************************************************/
void icpServer::readSocket
(
  SOCKET_PTR sock,
  uint32_t   id
)
{
  std::cout << __func__ << "(" << id << "): Reader started\n";

  ISMRMRD::Handshake hand;
  ISMRMRD::Command   cmd;

  ICPOUTPUTMANAGER::icpOutputManager om (id);
  ICPINPUTMANAGER::icpInputManager im;


  std::thread writer (&icpServer::sendMessage, this, std::ref (om), id, sock);

  // Keep reading messages until respondent is done
  //int msg_count = 0;
  while (!im.isClientDone())
  {
    IN_MSG in_msg;
    receiveMessage (sock, in_msg);
    //printf ("%s(%d): Received msg %d\n", __func__, id, msg_count++);

    switch (in_msg.ehdr.entity_type)
    {
      case ISMRMRD::ISMRMRD_HANDSHAKE:

        hand.deserialize (in_msg.data);
        om.setClientName (std::string (hand.client_name));
        om.setSessionTimestamp (hand.timestamp);
        om.setHandshakeReceived();
        if (_handle_authentication_registered)
        {
          handleAuthentication (std::string (hand.client_name), om);
        }
        break;

      case ISMRMRD::ISMRMRD_COMMAND:

        cmd.deserialize (in_msg.data);
        switch (cmd.command_type)
        {
          // TODO:
          //case ISMRMRD::ISMRMRD_INTERNAL_1:
            // process ISMRMRD protocol specific command
            //break;
          case ISMRMRD::ISMRMRD_COMMAND_STOP_FROM_CLIENT:

            om.setClientDone ();
            break;

          case ISMRMRD::ISMRMRD_COMMAND_USER_DEFINED_1:
          case ISMRMRD::ISMRMRD_COMMAND_USER_DEFINED_2:
          case ISMRMRD::ISMRMRD_COMMAND_USER_DEFINED_3:
          case ISMRMRD::ISMRMRD_COMMAND_USER_DEFINED_4:
                       
            if (_handle_command_registered)
            {
              handleCommand (cmd.command_type, cmd.command_id, om);
            }
            break;

          default:

            std::cout << __func__ << "(" << id <<
              "): Received unexpected command " << cmd.command_type << '\n';
            break;
        }
        break;

      case ISMRMRD::ISMRMRD_XML_HEADER:

        //ISMRMRD::deserialize ((const char*) &in_msg.data[0], _xml_header);
        ISMRMRD::deserialize ((const char*) &in_msg.data[0], _xml_header);
        im.addXmlHeader (in_msg.data);
        std::cout << __func__ << ": received XML Header\n";
        std::cout << "in_msg.data.size() = " << in_msg.data.size() << "\n";
        std::cout << "_xml_header.version = " << _xml_header.version << "\n";
        std::cout << "_xml_header.encoding[0].encodedSpace.matrixSize.y = " <<
                 _xml_header.encoding[0].encodedSpace.matrixSize.y << '\n';
        break;

      case ISMRMRD::ISMRMRD_MRACQUISITION:

        //std::cout << __func__ << ": received acq\n";
        if (im.addToStream (in_msg.ehdr, in_msg.data))
        {
          std::cout << __func__ << ": got all acqs\n";
          std::vector<ISMRMRD::Acquisition<float> > acqs =
            im.getAcquisitions (in_msg.ehdr);
          std::cout << __func__ << ": copied acqs\n";

          if (_handle_image_reconstruct_registered)
          {
            std::cout << __func__ << ": passing acqs to handler\n";
            handleImageReconstruction (_xml_header, acqs, om);
            std::cout << __func__ << ": returned from handler\n";
          }
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

  printf ("%s %d: waiting for writer to join\n", __func__, id);
  writer.join();
  printf ("%s %d: writer joined, reader exiting\n", __func__, id);

  return;
}

/*******************************************************************************
 ******************************************************************************/
void icpServer::serverMain ()
{
  std::cout << __func__ << ": Running with port = " << _port << '\n';

  boost::asio::io_service io_service;
  tcp::acceptor a (io_service, tcp::endpoint (tcp::v4(), _port));

  for (uint32_t id = 1;; ++id)
  {
    SOCKET_PTR sock (new tcp::socket (io_service));
    a.accept (*sock);
    std::cout << __func__ << ": Accepted client " << id << '\n';
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
  _shutdown = true;
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
  _shutdown (false),
  _handle_authentication_registered (false),
  _handle_command_registered (false),
  _handle_image_reconstruct_registered (false),
  _main_thread (),
  handleAuthentication (NULL),
  handleCommand (NULL),
  handleImageReconstruction (NULL)
{}

