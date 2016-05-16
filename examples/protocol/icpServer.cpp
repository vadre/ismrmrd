#include "icpServer.h"

/*******************************************************************************
 ******************************************************************************/
bool  icpServer::_running  = false;

/*******************************************************************************
 * Thread TODO: needs synchronization
 ******************************************************************************/
void icpServer::transmit
(
  ICP_SERVER_THREAD_DATA*  thread_data
)
{
  std::cout << __func__ << ": Writer thread (" << thread_data->id << ") started\n";

  struct timespec  ts;
  ts.tv_sec  =     0;
  ts.tv_nsec =     10000000;

  while (!thread_data->session_closed || thread_data->oq.size() > 0)
  {
    while (thread_data->oq.size() > 0)
    {
      OUT_MSG msg;
      msg = thread_data->oq.front();
      boost::asio::write (*thread_data->sock,
                          boost::asio::buffer (msg.data, msg.data.size()));
      thread_data->oq.pop();
    }
    nanosleep (&ts, NULL);
  }
  
  std::cout << "Writer (" << thread_data->id << "): Done!\n";
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
bool icpServer::forwardMessage
(
  ICP_SERVER_HANDLE   handle,
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
        static_cast<ISMRMRD::Acquisition<float>* >(entity)->getStorageType();
      head.stream       =
        static_cast<ISMRMRD::Acquisition<float>* >(entity)->getStream();
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
      std::cout << __func__ << ": xml serialized size = "
                << e_buffer.size() << "\n";
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
    queueMessage (handle, h_buffer, e_buffer);
  }

  return ret_val;
}

/*****************************************************************************
 ****************************************************************************/
void icpServer::queueMessage
(
  ICP_SERVER_THREAD_DATA*     thread_data,
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

  thread_data->oq.push (msg);
  std::cout << __func__ << ": num queued messages for id "
            << thread_data->id << " = " << thread_data->oq.size() << '\n';

  return;
}

/*******************************************************************************
 Thread for every connection
 ******************************************************************************/
void icpServer::closeConnection
(
  ICP_SERVER_HANDLE handle
)
{
  ICP_SERVER_THREAD_DATA* data = handle;
  data->session_closed = true;
}

/*******************************************************************************
 Thread for every connection
 ******************************************************************************/
void icpServer::receive
(
  SOCKET_PTR sock,
  uint32_t   id
)
{
  std::cout << __func__ << " : Receiver thread (" << id << ") started\n";

  ISMRMRD::Handshake           hand;
  ISMRMRD::Command             cmd;
  ISMRMRD::ErrorNotification   err;
  ISMRMRD::IsmrmrdHeader       hdr;
  ISMRMRD::IsmrmrdHeaderWrapper wrapper (hdr);
  USER_DATA*                   ud_ptr_ptr = new USER_DATA;

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


  ICP_SERVER_THREAD_DATA *thread_data = new ICP_SERVER_THREAD_DATA;

  if (getUserDataPointer ((ICP_SERVER_HANDLE)thread_data, ud_ptr_ptr, this))
  {
    thread_data->id = id;
    thread_data->sock = sock;
    thread_data->session_closed = false;
    thread_data->user_data = *ud_ptr_ptr;
  }
  else
  {
    std::cout << __func__ << "getUserDataPointer ERROR, thread exits" << "\n";
    return;
  }
  
  USER_DATA user_data = *ud_ptr_ptr;

  if (!_callbacks.size())
  {
    std::cout << __func__
              << "No handlers registered, thread exits" << "\n";
    delete (thread_data);
    return;
  }

  std::thread writer (&icpServer::transmit, this, thread_data);

  while (!thread_data->session_closed)
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
        call (cb_index, hand, user_data);
        break;

      case ISMRMRD::ISMRMRD_COMMAND:

        cmd.deserialize (in_msg.data);
        call (cb_index, cmd, user_data);
        if (cmd.getCommandType() == ISMRMRD::ISMRMRD_COMMAND_CLOSE_CONNECTION)
        {
          thread_data->session_closed = true;
        }
        break;

      case ISMRMRD::ISMRMRD_HEADER_WRAPPER:

        wrapper.deserialize (in_msg.data);
        cb_index = ISMRMRD::ISMRMRD_HEADER * 100 + in_msg.ehdr.storage_type;
        call (cb_index, wrapper.getHeader(), user_data);
        break;

      case ISMRMRD::ISMRMRD_MRACQUISITION:

        if (in_msg.ehdr.storage_type == ISMRMRD::ISMRMRD_SHORT)
        {
          a16.deserialize (in_msg.data);
          call (cb_index, a16, user_data);
        }
        else if (in_msg.ehdr.storage_type == ISMRMRD::ISMRMRD_INT)
        {
          a32.deserialize (in_msg.data);
          call (cb_index, a32, user_data);
        }
        else if (in_msg.ehdr.storage_type == ISMRMRD::ISMRMRD_FLOAT)
        {
          aflt.deserialize (in_msg.data);
          call (cb_index, aflt, user_data);
        }
        else if (in_msg.ehdr.storage_type == ISMRMRD::ISMRMRD_DOUBLE)
        {
          adbl.deserialize (in_msg.data);
          call (cb_index, adbl, user_data);
        }
        else
        {
          throw std::runtime_error ("Unexpected MR Acquisition Storage type");
        }
        break;

      case ISMRMRD::ISMRMRD_ERROR_NOTIFICATION:

        err.deserialize (in_msg.data);
        call (cb_index, err, user_data);
        break;

      case ISMRMRD::ISMRMRD_IMAGE:

        if (in_msg.ehdr.storage_type == ISMRMRD::ISMRMRD_USHORT)
        {
          ui16.deserialize (in_msg.data);
          call (cb_index, ui16, user_data);
        }
        else if (in_msg.ehdr.storage_type == ISMRMRD::ISMRMRD_SHORT)
        {
          i16.deserialize (in_msg.data);
          call (cb_index, i16, user_data);
        }
        else if (in_msg.ehdr.storage_type == ISMRMRD::ISMRMRD_UINT)
        {
          ui32.deserialize (in_msg.data);
          call (cb_index, ui32, user_data);
        }
        else if (in_msg.ehdr.storage_type == ISMRMRD::ISMRMRD_INT)
        {
          i32.deserialize (in_msg.data);
          call (cb_index, i32, user_data);
        }
        else if (in_msg.ehdr.storage_type == ISMRMRD::ISMRMRD_FLOAT)
        {
          iflt.deserialize (in_msg.data);
          call (cb_index, iflt, user_data);
        }
        else if (in_msg.ehdr.storage_type == ISMRMRD::ISMRMRD_DOUBLE)
        {
          idbl.deserialize (in_msg.data);
          call (cb_index, idbl, user_data);
        }
        else if (in_msg.ehdr.storage_type == ISMRMRD::ISMRMRD_CXFLOAT)
        {
          icflt.deserialize (in_msg.data);
          call (cb_index, icflt, user_data);
        }
        else if (in_msg.ehdr.storage_type == ISMRMRD::ISMRMRD_CXDOUBLE)
        {
          icdbl.deserialize (in_msg.data);
          call (cb_index, icdbl, user_data);
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

  std::cout << __func__ << ": Waiting for Transmit (" << id << ") to join\n";
  writer.join();
  delete (thread_data);
  std::cout << __func__ << ": Transmit (" << id << ") joined, exiting\n\n\n";

  return;
} // receive

/*******************************************************************************
 Thread
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
    std::thread (&icpServer::receive, this, sock, id).detach();
  }

  return;
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
template<> void icpServer::registerHandler (CB_HANDSHK, uint32_t, uint32_t);
template<> void icpServer::registerHandler (CB_ERRNOTE, uint32_t, uint32_t);
template<> void icpServer::registerHandler (CB_COMMAND, uint32_t, uint32_t);
template<> void icpServer::registerHandler (CB_XMLHEAD, uint32_t, uint32_t);
template<> void icpServer::registerHandler (CB_ACQ_16, uint32_t, uint32_t);
template<> void icpServer::registerHandler (CB_ACQ_32, uint32_t, uint32_t);
template<> void icpServer::registerHandler (CB_ACQ_FLT, uint32_t, uint32_t);
template<> void icpServer::registerHandler (CB_ACQ_DBL, uint32_t, uint32_t);

/*******************************************************************************
 ******************************************************************************/
template<typename ...A>
void icpServer::call (uint32_t index, A&& ... args)
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
void icpServer::start ()
{
  if (!_running)
  {
    _main_thread = std::thread (&icpServer::serverMain, this);
    _running = true;
  }
  return;
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
  _user_data_allocator_registered (false),
  getUserDataPointer (NULL)
{}

