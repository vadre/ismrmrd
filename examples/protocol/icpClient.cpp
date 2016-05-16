#include "icpClient.h"

/*******************************************************************************
 ******************************************************************************/
bool  icpClient::_running  = false;

/*******************************************************************************
 ******************************************************************************/
void icpClient::setSessionClosed ()
{
  _session_closed = true;
}

/*****************************************************************************
 ****************************************************************************/
bool icpClient::isSessionClosed ()
{
  return _session_closed;
}

/*******************************************************************************
 ******************************************************************************/
void icpClient::transmit
(
  SOCKET_PTR sock
)
{
  std::cout << __func__ << ": Writer thread started\n";

  struct timespec  ts;
  ts.tv_sec  =     0;
  ts.tv_nsec =     10000000;

  while (!isSessionClosed())
  {
    while (_oq.size() > 0)
    {
      OUT_MSG msg;
      msg = _oq.front();
      boost::asio::write (*sock, boost::asio::buffer (msg.data, msg.data.size()));
      _oq.pop();
    }
    nanosleep (&ts, NULL);
  }
  
  //std::cout << "Writer (" << id << "): Done!\n";
  return;
}

/*******************************************************************************
 ******************************************************************************/
uint32_t icpClient::receiveFrameInfo
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
uint32_t icpClient::receiveMessage
(
  SOCKET_PTR sock,
  IN_MSG&    in_msg
)
{
  boost::system::error_code  error;
  uint32_t ret_val = icpClient::receiveFrameInfo (sock, in_msg);

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
bool icpClient::registerUserDataAllocator
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

/*****************************************************************************
 ****************************************************************************/
template<> void icpClient::registerHandler (CB_HANDSHK, uint32_t, uint32_t);
template<> void icpClient::registerHandler (CB_ERRNOTE, uint32_t, uint32_t);
template<> void icpClient::registerHandler (CB_COMMAND, uint32_t, uint32_t);
template<> void icpClient::registerHandler (CB_XMLHEAD, uint32_t, uint32_t);
template<> void icpClient::registerHandler (CB_IMG_FLT, uint32_t, uint32_t);

/*****************************************************************************
 ****************************************************************************/
void icpClient::queueMessage
(
  std::vector<unsigned char>& ent,
  std::vector<unsigned char>& data
)
{
  OUT_MSG msg;
  msg.size = ent.size() + data.size();
  msg.data.reserve (msg.size);

  uint64_t s = msg.size;
  s = (isCpuLittleEndian) ? s : __builtin_bswap64 (s);

  std::copy ((unsigned char*) &s,
             (unsigned char*) &s + sizeof (s),
             std::back_inserter (msg.data));

  std::copy ((unsigned char*) &ent[0],
             (unsigned char*) &ent[0] + ent.size(),
             std::back_inserter (msg.data));

  std::copy ((unsigned char*) &data[0],
             (unsigned char*) &data[0] + data.size(),
             std::back_inserter (msg.data));

  _oq.push (msg);
  //std::cout << __func__ << ": num queued messages = " << _oq.size() << '\n';

  return;
}

/*****************************************************************************
 forwardMessage is the routine known to caller as the sendMessage callback
 ****************************************************************************/
bool icpClient::forwardMessage
(
  ISMRMRD::EntityType type,
  ISMRMRD::Entity*    entity
)
{
  bool ret_val = true;
  ISMRMRD::EntityHeader head;
  std::stringstream sstr;
  std::vector <unsigned char> h_buffer;
  std::vector <unsigned char> e_buffer;

  head.version     = my_version;
  head.entity_type = type;

  switch (type)
  {
    case ISMRMRD::ISMRMRD_MRACQUISITION:

      head.storage_type =
        static_cast<ISMRMRD::Acquisition<float>* > (entity)->getStorageType();
      head.stream       =
        static_cast<ISMRMRD::Acquisition<float>* > (entity)->getStream();
      e_buffer          = entity->serialize();
      break;

    case ISMRMRD::ISMRMRD_IMAGE:

      head.storage_type =
        static_cast<ISMRMRD::Image<float>* >(entity)->getStorageType();
      head.stream       =
        static_cast<ISMRMRD::Image<float>* >(entity)->getStream();
      e_buffer = entity->serialize();
      break;

    case ISMRMRD::ISMRMRD_HEADER_WRAPPER:

      head.storage_type = ISMRMRD::ISMRMRD_CHAR;
      head.stream       = ISMRMRD::ISMRMRD_STREAM_ISMRMRD_HEADER;

      e_buffer =
        static_cast<ISMRMRD::IsmrmrdHeaderWrapper*>(entity)->serialize();
      break;

    case ISMRMRD::ISMRMRD_HANDSHAKE:

      head.storage_type = ISMRMRD::ISMRMRD_CHAR;
      head.stream       = ISMRMRD::ISMRMRD_STREAM_HANDSHAKE;
      e_buffer = entity->serialize();
      break;

    case ISMRMRD::ISMRMRD_COMMAND:

      head.storage_type = ISMRMRD::ISMRMRD_CHAR;
      head.stream       = ISMRMRD::ISMRMRD_STREAM_COMMAND;
      e_buffer = entity->serialize();
      break;

    case ISMRMRD::ISMRMRD_ERROR_NOTIFICATION:
      head.storage_type = ISMRMRD::ISMRMRD_CHAR;
      head.stream       = ISMRMRD::ISMRMRD_STREAM_ERROR;
      e_buffer = entity->serialize();
      break;

    /*case ISMRMRD_WAVEFORM:
    case ISMRMRD_BLOB:*/
    default:

      // Still send the data to the user maybe?
      std::cout << __func__ << "Warning: Entity " << head.entity_type
                << " not processed in this version of icpSession\n";
      ret_val = false;
      break;
  }

  if (ret_val)
  {
    h_buffer = head.serialize();
    queueMessage (h_buffer, e_buffer);
  }

  return ret_val;
}

/*******************************************************************************
 Thread
 ******************************************************************************/
void icpClient::receive
(
  SOCKET_PTR sock
)
{
  std::cout << __func__ << " : Reader thread started\n";

  ISMRMRD::Handshake           hand;
  ISMRMRD::Command             cmd;
  ISMRMRD::ErrorNotification   err;
  ISMRMRD::IsmrmrdHeader       hdr;
  ISMRMRD::IsmrmrdHeaderWrapper wrapper (hdr);

  ISMRMRD::Acquisition<int16_t> a16;
  ISMRMRD::Acquisition<int32_t> a32;
  ISMRMRD::Acquisition<float>   aflt;
  ISMRMRD::Acquisition<double>  adbl;

  ISMRMRD::Image<uint16_t>              ui16;
  ISMRMRD::Image<int16_t>               i16;
  ISMRMRD::Image<uint32_t>              ui32;
  ISMRMRD::Image<int32_t>               i32;
  ISMRMRD::Image<float>                 iflt;
  ISMRMRD::Image<double>                idbl;
  ISMRMRD::Image<std::complex<float> >  icflt;
  ISMRMRD::Image<std::complex<double> > icdbl;

  _user_data = new USER_DATA;
  if (!getUserDataPointer (_user_data, this))
  {
    std::cout << __func__ << "getUserDataPointer ERROR, thread exits" << "\n";
    delete _user_data;
    return;
  }
 
  if (!_callbacks.size())
  {
    std::cout << "No handlers registered, thread exits" << "\n";
    return;
  }

  std::thread writer (&icpClient::transmit, this, sock);

  std::thread input (beginDataInput, *_user_data);
  //if (!beginDataInput(*_user_data))
  //{
    //std::cout << "beginDataInput ERROR, thread exits" << "\n";
    //return;
  //}

  while (!isSessionClosed())
  {
    IN_MSG in_msg;
    uint32_t read_status = receiveMessage (sock, in_msg);
    if (read_status)
    {
      std::cout << __func__ << "receiveMessage ERROR <" << read_status << ">\n";
      break;
    }

    uint32_t cb_index = in_msg.ehdr.entity_type * 100 +
                     in_msg.ehdr.storage_type; //TODO

    switch (in_msg.ehdr.entity_type)
    {
      case ISMRMRD::ISMRMRD_HANDSHAKE:

        hand.deserialize (in_msg.data);
        call (cb_index, hand, *_user_data);
        break;

      case ISMRMRD::ISMRMRD_COMMAND:

        cmd.deserialize (in_msg.data);
        call (cb_index, cmd, *_user_data);
        //if (cmd.getCommandType() == ISMRMRD::ISMRMRD_COMMAND_DONE_FROM_SERVER)
        //{
          //setServerDone();
        //}
        break;

      case ISMRMRD::ISMRMRD_HEADER_WRAPPER:

        wrapper.deserialize (in_msg.data);
        cb_index = ISMRMRD::ISMRMRD_HEADER * 100 + in_msg.ehdr.storage_type;
        call (cb_index, wrapper.getHeader(), *_user_data);
        break;

      case ISMRMRD::ISMRMRD_MRACQUISITION:

        if (in_msg.ehdr.storage_type == ISMRMRD::ISMRMRD_SHORT)
        {
          a16.deserialize (in_msg.data);
          call (cb_index, a16, *_user_data);
        }
        else if (in_msg.ehdr.storage_type == ISMRMRD::ISMRMRD_INT)
        {
          a32.deserialize (in_msg.data);
          call (cb_index, a32, *_user_data);
        }
        else if (in_msg.ehdr.storage_type == ISMRMRD::ISMRMRD_FLOAT)
        {
          aflt.deserialize (in_msg.data);
          call (cb_index, aflt, *_user_data);
        }
        else if (in_msg.ehdr.storage_type == ISMRMRD::ISMRMRD_DOUBLE)
        {
          adbl.deserialize (in_msg.data);
          call (cb_index, adbl, *_user_data);
        }
        else
        {
          throw std::runtime_error ("Unexpected MR Acquisition Storage type");
        }
        break;

      case ISMRMRD::ISMRMRD_ERROR_NOTIFICATION:

        err.deserialize (in_msg.data);
        call (cb_index, err, *_user_data);
        break;

      case ISMRMRD::ISMRMRD_IMAGE:

        if (in_msg.ehdr.storage_type == ISMRMRD::ISMRMRD_USHORT)
        {
          ui16.deserialize (in_msg.data);
          call (cb_index, ui16, *_user_data);
        }
        else if (in_msg.ehdr.storage_type == ISMRMRD::ISMRMRD_SHORT)
        {
          i16.deserialize (in_msg.data);
          call (cb_index, i16, *_user_data);
        }
        else if (in_msg.ehdr.storage_type == ISMRMRD::ISMRMRD_UINT)
        {
          ui32.deserialize (in_msg.data);
          call (cb_index, ui32, *_user_data);
        }
        else if (in_msg.ehdr.storage_type == ISMRMRD::ISMRMRD_INT)
        {
          i32.deserialize (in_msg.data);
          call (cb_index, i32, *_user_data);
        }
        else if (in_msg.ehdr.storage_type == ISMRMRD::ISMRMRD_FLOAT)
        {
          iflt.deserialize (in_msg.data);
          call (cb_index, iflt, *_user_data);
        }
        else if (in_msg.ehdr.storage_type == ISMRMRD::ISMRMRD_DOUBLE)
        {
          idbl.deserialize (in_msg.data);
          call (cb_index, idbl, *_user_data);
        }
        else if (in_msg.ehdr.storage_type == ISMRMRD::ISMRMRD_CXFLOAT)
        {
          icflt.deserialize (in_msg.data);
          call (cb_index, icflt, *_user_data);
        }
        else if (in_msg.ehdr.storage_type == ISMRMRD::ISMRMRD_CXDOUBLE)
        {
          icdbl.deserialize (in_msg.data);
          call (cb_index, icdbl, *_user_data);
        }
        else
        {
          throw std::runtime_error ("Unexpected MR Acquisition Storage type");
        }
        break;

      //case ISMRMRD::ISMRMRD_WAVEFORM:
        //break;
      //case ISMRMRD::ISMRMRD_BLOB:
        //break;

      default:

        std::cout << "Warning! Dropping unknown entity type: "
                  << in_msg.ehdr.entity_type << "\n\n";
        break;

    } // switch ((*in_msg).ehdr.entity_type)
  } // while (!in_data.isRespondentDone())

  std::cout << __func__ << ": Waiting for Input to join\n";
  input.join();
  std::cout << __func__ << ": Waiting for Writer to join\n";
  writer.join();
  std::cout << __func__ << ": Writer joined, exiting\n\n\n";

  return;
} // receive

/*******************************************************************************
 ******************************************************************************/
bool icpClient::registerInputProvider
(
  BEGIN_INPUT_CALLBACK_FUNC func_ptr
)
{
  if (func_ptr)
  {
    beginDataInput              = func_ptr;
    _data_input_func_registered = true;
  }
  return _data_input_func_registered;
}

/*******************************************************************************
 ******************************************************************************/
template<typename ...A>
void icpClient::call (uint32_t index, A&& ... args)
{
  if (_callbacks.find (index) != _callbacks.end())
  {
    //TODO needs synchronization
    using func_t = CB_STRUCT <A...>;
    using cb_t   = std::function <void (A...)>;

    const CB_BASE& base = *_callbacks[index];
    const cb_t& func = static_cast <const func_t&> (base).callback;

    func (std::forward <A> (args)...);
  }
  else
  {
    std::cout << "Warning: Entity handler for entity "
              << (uint32_t)(index / 100) << ", storage " << index % 100
              << ", not registered\n";
  }
}

/*******************************************************************************
 ******************************************************************************/
void icpClient::connect ()
{
  if (!_running)
  {
    std::cout << "Connecting to <" << _host << "> on port " << _port << "\n\n";
    boost::asio::io_service io_service;
    SOCKET_PTR sock (new tcp::socket (io_service));
    tcp::endpoint endpoint 
      (boost::asio::ip::address::from_string (_host), _port);

    boost::system::error_code error = boost::asio::error::host_not_found;
    (*sock).connect(endpoint, error);
    if (error)
    {
      throw boost::system::system_error(error);
    }
   
    _receive_thread = std::thread (&icpClient::receive, this, sock);
    _running = true;
  }
  return;
}

/*******************************************************************************
 ******************************************************************************/
icpClient::~icpClient ()
{
  if (_receive_thread.joinable())
  {
    _receive_thread.join();
  }
  _oq = std::queue<OUT_MSG>(); 
  _callbacks.clear();
}

/*******************************************************************************
 ******************************************************************************/
icpClient::icpClient
(
  std::string host,
  uint16_t    port
)
: _host (host),
  _port (port),
  _user_data_allocator_registered (false),
  _data_input_func_registered (false),
  _session_closed (false),
  getUserDataPointer (NULL),
  beginDataInput (NULL)
{}

