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

  while (!om->isSessionClosing())
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
 Thread for every connection
 ******************************************************************************/
void icpServer::readSocket
(
  SOCKET_PTR sock,
  uint32_t   id
)
{
  std::cout << __func__ << " : Reader thread (" << id << ") started\n";

  ISMRMRD::Handshake           hand;
  ISMRMRD::Command             cmd;
  ISMRMRD::ErrorNotification   err;
  ISMRMRD::IsmrmrdHeader       hdr;
  ISMRMRD::IsmrmrdHeaderWrapper wrapper (hdr);
  USER_DATA                    user_data;

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

  if (!getUserDataPointer (&user_data))
  {
    std::cout << __func__ << "getUserDataPointer ERROR, thread exits" << "\n";
    return;
  }

  ICPOUTPUTMANAGER::icpOutputManager* om =
    new ICPOUTPUTMANAGER::icpOutputManager ();

  if (!setSendMessageCallback
        (&ICPOUTPUTMANAGER::icpOutputManager::processEntity, user_data))
  {
    std::cout << __func__ 
              << "setSendMessageCallback ERROR, thread exits" << "\n";
    delete (om);
    return;
  }

  if (!callbacks.size())
  {
    std::cout << __func__
              << "No handlers registered, thread exits" << "\n";
    delete (om);
    return;
  }

  std::thread writer (&icpServer::sendMessage, this, om, id, sock);

  while (!om->isSessionClosing())
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
          om->setSessionClosing();
        }
        break;

      case ISMRMRD::ISMRMRD_HEADER:

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

  std::cout << __func__ << ": Waiting for Writer (" << id << ") to join\n";
  writer.join();
  delete (om);
  std::cout << __func__ << ": Writer (" << id << ") joined, exiting\n\n\n";

  return;
} // readSocket

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
/*
template <typename F, typename E, typename S>
void icpServer::registerHandler
(
  F func,
  E etype,
  S stype
)
{
  if (!func)
  {
    throw std::runtime_error ("registerHandler: NULL function pointer");
  }

  std::unique_ptr<F> f1(new F(func));
  uint32_t index = etype * 100 + stype; //TODO
  callbacks.insert (CB_MAP::value_type(index, std::move(func)));

  return;
}
*/
template<> void icpServer::registerHandler (CB_ACQ_16, uint32_t, uint32_t);

template<> void icpServer::registerHandler (CB_ACQ_32, uint32_t, uint32_t);

template<> void icpServer::registerHandler (CB_ACQ_FLT, uint32_t, uint32_t);

template<> void icpServer::registerHandler (CB_ACQ_DBL, uint32_t, uint32_t);

/*
template<> void icpServer::registerHandler (ISMRMRD::Acquisition<int16_t>,
                                            uint32_t, uint32_t);

template<> void icpServer::registerHandler (ISMRMRD::Acquisition<int32_t>,
                                            uint32_t, uint32_t);

template<> void icpServer::registerHandler (ISMRMRD::Acquisition<float>,
                                            uint32_t, uint32_t);

template<> void icpServer::registerHandler (ISMRMRD::Acquisition<double>,
                                            uint32_t, uint32_t);
*/

/*******************************************************************************
 ******************************************************************************/
template<typename ...A>
void icpServer::call (uint32_t index, A&& ... args)
{
  //TODO needs synchronization
  using func_t = CB_STRUCT <A...>;
  using cb_t   = std::function <void (A...)>;

  const CB_BASE& base = *callbacks[index];
  const cb_t& func = static_cast <const func_t&> (base).callback;

  func (std::forward <A> (args)...);
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
  _send_callback_setter_registered (false),
  getUserDataPointer (NULL),
  setSendMessageCallback (NULL)
{}

