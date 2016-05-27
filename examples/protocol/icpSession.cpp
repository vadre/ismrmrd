#include "icpSession.h"

/*******************************************************************************
 ******************************************************************************/
icpSession::icpSession
(
  SOCKET_PTR sock,
  uint32_t   id  // for debug only
)
: _sock (sock),
  _stop (false),
  _id (id),
  _writer ()
{
  std::ios_base::sync_with_stdio(false);
  std::cin.tie(static_cast<std::ostream*>(0));
  std::cerr.tie(static_cast<std::ostream*>(0));
}

/*******************************************************************************
 ******************************************************************************/
icpSession::~icpSession
(
)
{
  _callbacks.clear();
}

/*******************************************************************************
 ******************************************************************************/
void icpSession::beginReceiving
(
)
{
  _writer = std::thread (&icpSession::transmit, this);

 std::cout << __func__ <<": 1\n";
  while (!_stop)
  {
 std::cout << __func__ <<": 2\n";
    if (!getMessage())
    {
      std::cout << __func__ <<": getMessage Error!\n";
      _stop = true;

      return;
    }
  }

 std::cout << __func__ <<": 3\n";
  if (_writer.joinable())
  {
 std::cout << __func__ <<": 4\n";
    _writer.join();
  }
 std::cout << __func__ <<": 5\n";

  return;
}

/*******************************************************************************
 ******************************************************************************/
void icpSession::shutdown
(
)
{
  _stop = true;
}

/*******************************************************************************
 ******************************************************************************/
bool icpSession::getMessage
(
)
{
  IN_MSG                     in_msg;
  boost::system::error_code  error;

  uint32_t read_status = receiveFrameInfo (in_msg);
  if (read_status)
  {
    std::cout << "Error " << read_status << " from receiveFrameInfo\n";
    return false;
  }

  if (in_msg.data_size <= 0)
  {
    // TODO: Should this be an error or some special case? If special case
    //       then what to send to handler? Special handler too?
    std::cout << "Warning! No data for entity type "
              << in_msg.ehdr.entity_type << "\n";
    return true;
  }

  in_msg.data.resize (in_msg.data_size);
  int bytes_read = boost::asio::read (*_sock,
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
    return false;
  }
  else if (bytes_read != in_msg.data.size())
  {
    std::cout << "Error: Read " << bytes_read
              << ", expected " << in_msg.data_size << " bytes\n";
    return false;
  }

  deliver (in_msg);

  return true;
}

/*******************************************************************************
 ******************************************************************************/
uint32_t icpSession::receiveFrameInfo
(
  IN_MSG& in_msg
)
{
  boost::system::error_code  error;

  int bytes_read = boost::asio::read (*_sock, boost::asio::buffer (&in_msg.size,
                                      DATAFRAME_SIZE_FIELD_SIZE), error);
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
  bytes_read = boost::asio::read (*_sock,
               boost::asio::buffer (&buffer[0], ENTITY_HEADER_SIZE), error);
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
void icpSession::deliver
(
  IN_MSG& in_msg
)
{
  ISMRMRD::Handshake hand;
  ISMRMRD::Command cmd;
  ISMRMRD::IsmrmrdHeaderWrapper wrapper;
  ISMRMRD::ErrorNotification err;
  ISMRMRD::Acquisition<float> aflt;
  ISMRMRD::Acquisition<double> adbl;
  ISMRMRD::Image<float> iflt;
  ISMRMRD::Image<double> idbl;

  switch (in_msg.ehdr.entity_type)
  {
    case ISMRMRD::ISMRMRD_HANDSHAKE:

      hand.deserialize (in_msg.data);
      call (in_msg.ehdr.entity_type, hand);
      break;

    case ISMRMRD::ISMRMRD_COMMAND:

      cmd.deserialize (in_msg.data);
      call (in_msg.ehdr.entity_type, cmd);
      break;

    case ISMRMRD::ISMRMRD_HEADER_WRAPPER:

      wrapper.deserialize (in_msg.data);
      call (ISMRMRD::ISMRMRD_HEADER, wrapper.getHeader());
      break;

    case ISMRMRD::ISMRMRD_MRACQUISITION:

      if (in_msg.ehdr.storage_type == ISMRMRD::ISMRMRD_FLOAT)
      {
        aflt.deserialize (in_msg.data);
        call (in_msg.ehdr.entity_type, aflt, in_msg.ehdr.storage_type);
      }
      else if (in_msg.ehdr.storage_type == ISMRMRD::ISMRMRD_DOUBLE)
      {
        adbl.deserialize (in_msg.data);
        call (in_msg.ehdr.entity_type, adbl, in_msg.ehdr.storage_type);
      }
      else
      {
        std::cout << "Warning! Unexpected MR Acquisition Storage type "
                  << in_msg.ehdr.storage_type << "\n";
      }
      break;

    case ISMRMRD::ISMRMRD_ERROR_NOTIFICATION:

      err.deserialize (in_msg.data);
      call (in_msg.ehdr.entity_type, err);
      break;

    case ISMRMRD::ISMRMRD_IMAGE:

      if (in_msg.ehdr.storage_type == ISMRMRD::ISMRMRD_FLOAT)
      {
        iflt.deserialize (in_msg.data);
        call (in_msg.ehdr.entity_type, iflt, in_msg.ehdr.storage_type);
      }
      else if (in_msg.ehdr.storage_type == ISMRMRD::ISMRMRD_DOUBLE)
      {
        idbl.deserialize (in_msg.data);
        call (in_msg.ehdr.entity_type, idbl, in_msg.ehdr.storage_type);
      }
      else
      {
        std::cout << "Warning! Unexpected MR Image Storage type: "
                  << in_msg.ehdr.storage_type << "\n";
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
}

/*******************************************************************************
 ******************************************************************************/
template<typename ...A>
void icpSession::call
(
  uint32_t index,
  A&&      ... args
)
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
template<> void icpSession::registerHandler (CB_HANDSHK, uint32_t);
template<> void icpSession::registerHandler (CB_ERRNOTE, uint32_t);
template<> void icpSession::registerHandler (CB_COMMAND, uint32_t);
template<> void icpSession::registerHandler (CB_XMLHEAD, uint32_t);
template<> void icpSession::registerHandler (CB_MR_ACQ,  uint32_t);
template<> void icpSession::registerHandler (CB_IMAGE,   uint32_t);

/*******************************************************************************
 ******************************************************************************/
bool icpSession::forwardMessage
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

  head.version     = ISMRMRD_VERSION_MAJOR;
  head.entity_type = type;

  switch (type)
  {
    case ISMRMRD::ISMRMRD_MRACQUISITION:

      head.storage_type =
        static_cast<ISMRMRD::Acquisition<int16_t>* >(entity)->getStorageType();
      head.stream       =
        static_cast<ISMRMRD::Acquisition<int16_t>* >(entity)->getStream();
      e_buffer          = entity->serialize();
      break;

    case ISMRMRD::ISMRMRD_IMAGE:

      head.storage_type =
        static_cast<ISMRMRD::Image<int16_t>* >(entity)->getStorageType();
      head.stream       =
        static_cast<ISMRMRD::Image<int16_t>* >(entity)->getStream();
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

/*****************************************************************************
 ****************************************************************************/
void icpSession::queueMessage
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
  //std::cout << __func__ << ": num queued messages for id "
            //<< thread_data->id << " = " << thread_data->oq.size() << '\n';

  return;
}

/*******************************************************************************
 * Thread TODO: needs synchronization
 ******************************************************************************/
void icpSession::transmit
(
)
{
  std::cout << __func__ << ": (" << _id << ") started\n";

  struct timespec  ts = {0};
  ts.tv_sec  =     0;
  ts.tv_nsec =     1000000;
  //ts.tv_nsec =     10000000;
  int print = 0;

  if (200 > print++) std::cout << __func__ << " 1\n";
  while (!_stop || _oq.size() > 0)
  {
    if (200 > print++) std::cout << __func__ << " 2\n";
    while (_oq.size() > 0)
    {
      if (200 > print++) std::cout << __func__ << " 3\n";
      OUT_MSG msg;
      msg = _oq.front();
      if (200 > print++) std::cout << __func__ << " 4\n";
      boost::asio::write (*_sock,
                          boost::asio::buffer (msg.data, msg.data.size()));
      if (200 > print++) std::cout << __func__ << " 5\n";
      _oq.pop();
      if (200 > print++) std::cout << __func__ << " 6\n";
    }
      if (200 > print++) std::cout << __func__ << " 7\n";
    nanosleep (&ts, (struct timespec*)NULL);
      if (200 > print++) std::cout << __func__ << " 8\n";
  }

  std::cout << "Writer (" << _id << "): Done!\n";
  return;
}
