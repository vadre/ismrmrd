#include "icpServer.h"

/*******************************************************************************
 ******************************************************************************/
bool  icpServer::_running  = false;

/*******************************************************************************
 queueResponse is the routine known to caller as the sendMessage callback
 ******************************************************************************/
template<typename T>
bool queueResponse
(
  T entity
)
{
  bool ret_val = false;
  ISMRMRD::EntityHeader e_head;
  std::vector <unsigned char> buffer;

  e_head.version     = my_version;
  e_head.entity_type = ISMRMRD::get_entity_type<T> ();

  switch (e_head.entity_type)
  {
    case ISMRMRD_MRACQUISITION:

      e_head.storage_type = entity.getStorageType();
      e_head.stream       = entity.getStream();
      break;

    case ISMRMRD_IMAGE:

      e_head.storage_type = entity.getStorageType();
      e_head.stream       = entity.getStream();
      break;

    case ISMRMRD_XML_HEADER:

      e_head.storage_type = ISMRMRD::ISMRMRD_CHAR;
      e_head.stream       = ISM................................................
      break;

    case ISMRMRD_HANDSHAKE:

      e_head.storage_type = ISMRMRD::ISMRMRD_CHAR;
      break;

    case ISMRMRD_COMMAND:

      e_head.storage_type = ISMRMRD::ISMRMRD_CHAR;
      break;

    /*case ISMRMRD_ERROR:
      break;
    case ISMRMRD_WAVEFORM:
      break;
    case ISMRMRD_BLOB:
      break;*/

    default:

      break;
  }

  if (decltype (T) != decltype (ISMRMRD::Image<float>)  &&
      decltype (T) != decltype (ISMRMRD::Image<double>) &&
      decltype (T) != decltype (ISMRMRD::IsmrmrdHeader) &&
      decltype (T) != decltype (ISMRMRD::Command)       &&
      decltype (T) != decltype (ISMRMRD::Handshake)     &&
      decltype (T) != decltype (ISMRMRD::Error))
  {
    std::cout << __func__ << "Warning: No processing exists for the Type <"
              << decltype (T) << ">\n"; 
    return retval;
  }

  queueResponseType


  return ret_val;
}

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
uint32_t icpServer::receiveFrameInfo
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
    std::cout << "Frame size read status: " << error << "\n";
  }

  if (bytes_read == 0)
  {
    std::cout << __func__ << "Received EOF from client\n";
    return ICP_ERROR_SOCKET_EOF;
  }
  else if (bytes_read != DATAFRAME_SIZE_FIELD_SIZE)
  {
    std::cout << __func__ << "Error: Read " << bytes_read << ", expected "
              << DATAFRAME_SIZE_FIELD_SIZE << " bytes\n";
    return ICP_ERROR_SOCKET_WRONG_LENGTH;
  }



  std::vector<unsigned char> buffer (ENTITY_HEADER_SIZE);
  bytes_read =
    boost::asio::read (*sock,
                       boost::asio::buffer (&buffer[0], ENTITY_HEADER_SIZE),
                       error);
  if (error)
  {
    std::cout << "Entity Header read status: " << error << "\n";
  }

  if (bytes_read == 0)
  {
    std::cout << __func__ << "Received EOF from client\n";
    return ICP_ERROR_SOCKET_EOF;
  }
  else if (bytes_read != ENTITY_HEADER_SIZE)
  {
    std::cout << "Error: Read " << bytes_read << ", expected "
              << ENTITY_HEADER_SIZE << " bytes\n";
    return ICP_ERROR_SOCKET_WRONG_LENGTH;
  }

  in_msg.ehdr.deserialize (buffer);
  in_msg.data_size = in_msg.size - ENTITY_HEADER_SIZE;

  return 0;
}

/*******************************************************************************
 ******************************************************************************/
uint32_t icpServer::receiveMessage
(
  SOCKET_PTR sock,
  IN_MSG&    in_msg
)
{
  boost::system::error_code  error;
  uint32_t ret_val = icpServer::receiveFrameInfo (sock, in_msg);

  if (ret_val)
  {
    return ret_val;
  }

  if (in_msg.data_size <= 0)
  {
    // Special case?
    std::cout << "Error! Data_size  = " << in_msg.data_size << "\n";
    std::cout << "       EntityType = " << in_msg.ehdr.entity_type << "\n";
    return ICP_ENTITY_WITH_NO_DATA;
  }

  in_msg.data.resize (in_msg.data_size);
  int bytes_read = boost::asio::read (*sock,
                                      boost::asio::buffer (&in_msg.data[0],
                                      in_msg.data_size),
                                      error);
  if (error)
  {
    std::cout << "Error: message data read status: " << error << "\n";
  }

  if (bytes_read == 0)
  {
    std::cout << __func__ << "Received EOF from client\n";
    return ICP_ERROR_SOCKET_EOF;
  }
  else if (bytes_read != in_msg.data.size())
  {
    std::cout << "Error: Read " << bytes_read << ", expected "
              << in_msg.data_size << " bytes\n";
    return ICP_ERROR_SOCKET_WRONG_LENGTH;
  }

  return 0;
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

  uint32_t                     read_status = 0;
  ISMRMRD::Handshake           hand;
  ISMRMRD::Command             cmd;
  ISMRMRD::Error               err;
  ISMRMRD::IsmrmrdHeader       hdr;
  ISMRMRD::Acquisition<float>  acq_float;
  ISMRMRD::Acquisition<double> acq_double;

  ICPOUTPUTMANAGER::icpOutputManager* om =
    new ICPOUTPUTMANAGER::icpOutputManager ();

  std::thread writer (&icpServer::sendMessage, this, om, id, sock);

  while (!om->session_done)
  {
    IN_MSG in_msg;
    uint32_t read_status = receiveMessage (sock, in_msg);
    if (read_status)
    {
      std::cout << __func__ << "receiveMessage ERROR <" << read_status << ">\n";
      break;
    }

    switch (in_msg.ehdr.entity_type)
    {
      case ISMRMRD::ISMRMRD_HANDSHAKE:

        if (_handle_handshake_registered)
        {
          hand.deserialize (in_msg.data);
          handleHandshake (hand);
        }
        break;

      case ISMRMRD::ISMRMRD_COMMAND:

        cmd.deserialize (in_msg.data);
        switch (cmd.command_type)
        {
          //case SOME_SERVER_INTENDED_COMMAND_TYPE:

            //do_something (cmd);
            //break;

          default:

            if (_handle_command_registered)
            {
              handleCommand (cmd);
            }
            break;
        }

      case ISMRMRD::ISMRMRD_XML_HEADER:

        if (_handle_ismrmrd_header_registered)
        {
          try
          {
            std::string xml (in_msg.data.begin(), in_msg.data.end());
            ISMRMRD::deserialize (xml.c_str(), hdr);
            handleIsmrmrdHeader (hdr);
          }
          catch (std::runtime_error& e)
          {
            std::cout << "Unable to deserialize XML header: " << e.what() << "\n";
            throw;
          }
        }
        break;

      case ISMRMRD::ISMRMRD_MRACQUISITION:

        if (_handle_mracquisition_registered)
        {
          if (in_msg.ehdr.entity_type == ISMRMRD_FLOAT)
          {
            acq_float.deserialize (in_msg.data);
            handleMrAcquisition (acq_float);
          }
          else if (in_msg.ehdr.entity_type == ISMRMRD_DOUBLE)
          {
            acq_double.deserialize (in_msg.data);
            handleMrAcquisition (acq_double);
          }
        }
        break;

      case ISMRMRD::ISMRMRD_ERROR:

        err.deserialize (in_msg.data);
        switch (err.error_type)
        {
          //case SOME_SERVER_INTENDED_ERROR_TYPE:

            //do_something (err);
            //break;

          default:

            if (_handle_error_registered)
            {
              handleError (err);
            }
            break;
        }
        break;

      // TODO:
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
bool icpServer::registerCallbackSetter
(
  SET_SEND_CALLBACK_FUNC func_ptr
)
{
  if (func_ptr)
  {
    setSendMessageCallback           = func_ptr;
    _send_callback_setter_registered = true;
  }
  return _send_callback_setter_registered;
}

/*******************************************************************************
 ******************************************************************************/
bool icpServer::registerUserDataAllocator
(
  GET_USER_DATA_FUNC func_ptr
)
{
  if (func_ptr)
  {
    getUserDataPointer              = func_ptr;
    _user_data_allocator_registered = true;
  }
  return _user_data_allocator_registered;
}

/*******************************************************************************
 ******************************************************************************/
bool icpServer::registerHandshakeHandler
(
  HANDSHAKE_HANDLER_PTR func_ptr
)
{
  if (func_ptr)
  {
    handleHandshake              = func_ptr;
    _handle_handshake_registered = true;
  }
  return _handle_handshake_registered;
}

/*******************************************************************************
 ******************************************************************************/
bool icpServer::registerIsmrmrdHeaderHandler
(
  ISMRMRD_HEADER_HANDLER_PTR  func_ptr
)
{
  if (func_ptr)
  {
    handleIsmrmrdHeader               = func_ptr;
    _handle_ismrmrd_header_registered = true;
  }
  return _handle_ismrmrd_header_registered;
}

/*******************************************************************************
 ******************************************************************************/
bool icpServer::registerCommandHandler
(
  COMMAND_HANDLER_PTR  func_ptr
)
{
  if (func_ptr)
  {
    handleCommand              = func_ptr;
    _handle_command_registered = true;
  }
  return _handle_command_registered;
}

/*******************************************************************************
 ******************************************************************************/
bool icpServer::registerErrorHandler
(
  ERROR_HANDLER_PTR  func_ptr
)
{
  if (func_ptr)
  {
    handleError              = func_ptr;
    _handle_error_registered = true;
  }
  return _handle_error_registered;
}

/*******************************************************************************
 ******************************************************************************/
bool icpServer::registerMrAcquisitionHandler
(
  MR_ACQUISITION_HANDLER_PTR  func_ptr
)
{
  if (func_ptr)
  {
    handleMrAcquisition              = func_ptr;
    _handle_mracquisition_registered = true;
  }
  return _handle_mracquisition_registered;
}

/*******************************************************************************
 ******************************************************************************/
bool icpServer::start ()
{
  if (!_running)
  {
    _main_thread = std::thread (&icpServer::serverMain, this);
    _running = true;
  }
  return _running;
}

/*******************************************************************************
 ******************************************************************************/
icpServer::~icpServer ()
{
  if (_main_thread.joinable ()) _main_thread.join();
}

/*******************************************************************************
 ******************************************************************************/
icpServer::icpServer
(
  uint16_t p
)
: _port (p),
  _main_thread (),
  _user_data_registered (false),
  _send_message_callback_registered (false),
  _handle_handshake_registered (false),
  _handle_command_registered (false),
  _handle_error_registered (false),
  _handle_mracquisition_registered (false),
  _handle_ismrmrd_header_registered (false),
  getUserDataPointer (NULL),
  setSendMessageCallback (NULL),
  handleHandshake (NULL),
  handleCommand (NULL),
  handleError (NULL),
  handleMrAcquisition (NULL),
  handleIsmrmrdHeader (NULL)
{}

