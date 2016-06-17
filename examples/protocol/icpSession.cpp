#include "icpSession.h"

/*******************************************************************************
 ******************************************************************************/
icpSession::icpSession
(
  SOCKET_PTR sock
)
: _sock (sock),
  _stop (false),
  _transmitter ()
{
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
void icpSession::run
(
)
{
  _transmitter = std::thread (&icpSession::transmit, this);
  while (!_stop)
  {
    if (!getMessage())
    {
      std::cout << "Error from icpSession::getMessage\n";
      _stop = true;
    }
  }

  if (_transmitter.joinable())
  {
    _transmitter.join();
  }

  return;
}

/*******************************************************************************
 ******************************************************************************/
void icpSession::shutdown
(
)
{
  _stop = true;
  _oq.stop ();
}

/*******************************************************************************
 ******************************************************************************/
template<> void icpSession::registerHandler (ICP_CB, ETYPE);
template class icpMTQueue<OUTPUT_MESSAGE_STRUCTURE>;

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

  if (bytes_read != in_msg.data.size())
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
    std::cout << __func__ << "Received EOF\n";
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

  if (bytes_read != ENTITY_HEADER_SIZE)
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
  ISMRMRD::EntityType  etype = (ISMRMRD::EntityType) in_msg.ehdr.entity_type;
  ISMRMRD::StorageType stype = (ISMRMRD::StorageType) in_msg.ehdr.storage_type;

  if (_callbacks.find (etype) == _callbacks.end())
  {
    std::cout << "Warning! received unexpected entity type: " << etype << "\n";
    return;
  }

  if (etype == ISMRMRD::ISMRMRD_HANDSHAKE)
  {
    std::unique_ptr<HANDSHAKE> ent (new (HANDSHAKE));
    ent->deserialize (in_msg.data);
    call (etype, &(*ent), etype, stype);
  }
  else if (etype == ISMRMRD::ISMRMRD_COMMAND)
  {
    std::unique_ptr<COMMAND> ent (new (COMMAND));
    ent->deserialize (in_msg.data);
    call (etype, ent, etype, stype);
  }
  else if (etype == ISMRMRD::ISMRMRD_HEADER_WRAPPER)
  {
    std::unique_ptr<XMLWRAP> ent (new (XMLWRAP));
    ent->deserialize (in_msg.data);
    call (etype, &(*ent), etype, stype);
  }
  else if (etype == ISMRMRD::ISMRMRD_ERROR_REPORT)
  {
    std::unique_ptr<ERRREPORT> ent (new (ERRREPORT));
    ent->deserialize (in_msg.data);
    call (etype, &(*ent), etype, stype);
  }
  else if (etype == ISMRMRD::ISMRMRD_MRACQUISITION)
  {
    if (stype == ISMRMRD::ISMRMRD_SHORT)
    {
      std::unique_ptr<ISMRMRD::Acquisition<int16_t>> ent
        (new ISMRMRD::Acquisition<int16_t>);
      ent->deserialize (in_msg.data);
      call (etype, &(*ent), etype, stype);
    }
    else if (stype == ISMRMRD::ISMRMRD_INT)
    {
      std::unique_ptr<ISMRMRD::Acquisition<int32_t>> ent
        (new ISMRMRD::Acquisition<int32_t>);
      ent->deserialize (in_msg.data);
      call (etype, &(*ent), etype, stype);
    }
    if (stype == ISMRMRD::ISMRMRD_FLOAT)
    {
      std::unique_ptr<ISMRMRD::Acquisition<float>> ent
        (new ISMRMRD::Acquisition<float>);
      ent->deserialize (in_msg.data);
      call (etype, &(*ent), etype, stype);
    }
    else if (stype == ISMRMRD::ISMRMRD_DOUBLE)
    {
      std::unique_ptr<ISMRMRD::Acquisition<double>> ent
        (new ISMRMRD::Acquisition<double>);
      ent->deserialize (in_msg.data);
      call (etype, &(*ent), etype, stype);
    }
    else
    {
      std::cout << "Warning! Unexpected MR Acquisition Storage type "
                << stype << "\n";
    }
  }
  else if (etype == ISMRMRD::ISMRMRD_IMAGE)
  {
    if (stype == ISMRMRD::ISMRMRD_SHORT)
    {
      std::unique_ptr<ISMRMRD::Image<int16_t>> ent (new ISMRMRD::Image<int16_t>);
      ent->deserialize (in_msg.data);
      call (etype, &(*ent), etype, stype);
    }
    else if (stype == ISMRMRD::ISMRMRD_USHORT)
    {
      std::unique_ptr<ISMRMRD::Image<uint16_t>> ent (new ISMRMRD::Image<uint16_t>);
      ent->deserialize (in_msg.data);
      call (etype, &(*ent), etype, stype);
    }
    else if (stype == ISMRMRD::ISMRMRD_INT)
    {
      std::unique_ptr<ISMRMRD::Image<int32_t>> ent (new ISMRMRD::Image<int32_t>);
      ent->deserialize (in_msg.data);
      call (etype, &(*ent), etype, stype);
    }
    else if (stype == ISMRMRD::ISMRMRD_UINT)
    {
      std::unique_ptr<ISMRMRD::Image<uint32_t>> ent (new ISMRMRD::Image<uint32_t>);
      ent->deserialize (in_msg.data);
      call (etype, &(*ent), etype, stype);
    }
    else if (stype == ISMRMRD::ISMRMRD_FLOAT)
    {
      std::unique_ptr<ISMRMRD::Image<float>> ent (new ISMRMRD::Image<float>);
      ent->deserialize (in_msg.data);
      call (etype, &(*ent), etype, stype);
    }
    else if (stype == ISMRMRD::ISMRMRD_DOUBLE)
    {
      std::unique_ptr<ISMRMRD::Image<double>> ent (new ISMRMRD::Image<double>);
      ent->deserialize (in_msg.data);
      call (etype, &(*ent), etype, stype);
    }
    else if (stype == ISMRMRD::ISMRMRD_CXFLOAT)
    {
      std::unique_ptr <ISMRMRD::Image<std::complex<float>>> ent
        (new ISMRMRD::Image<std::complex<float>>);
      ent->deserialize (in_msg.data);
      call (etype, &(*ent), etype, stype);
    }
    else if (stype == ISMRMRD::ISMRMRD_CXDOUBLE)
    {
      std::unique_ptr <ISMRMRD::Image<std::complex<double>>> ent
        (new ISMRMRD::Image<std::complex<double>>);
      ent->deserialize (in_msg.data);
      call (etype, &(*ent), etype, stype);
    }
    else
    {
      std::cout << "Warning! Unexpected Image Storage type: " << stype << "\n";
    }
  }
  else
  {
    std::cout << "Warning! Dropping unknown entity type: " << etype << "\n\n";
  }
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
    using func_t = CB_STRUCT <A...>;
    using cb_t   = std::function <void (A...)>;

    const CB_BASE& base = *_callbacks[index];
    const cb_t& func = static_cast <const func_t&> (base).callback;

    func (std::forward <A> (args)...);
  }
}

/*******************************************************************************
 ******************************************************************************/
uint32_t icpSession::getAcqStream
(
  ENTITY* entity,
  STYPE   stype
)
{
  if (stype == ISMRMRD::ISMRMRD_SHORT)
  {
    return static_cast<ISMRMRD::Acquisition<int16_t>*>(entity)->getStream();
  }
  else if (stype == ISMRMRD::ISMRMRD_INT)
  {
    return static_cast<ISMRMRD::Acquisition<int32_t>*>(entity)->getStream();
  }
  else if (stype == ISMRMRD::ISMRMRD_FLOAT)
  {
    return static_cast<ISMRMRD::Acquisition<float>*>(entity)->getStream();
  }
  else if (stype == ISMRMRD::ISMRMRD_DOUBLE)
  {
    return static_cast<ISMRMRD::Acquisition<double>*>(entity)->getStream();
  }

  throw std::runtime_error ("Unexpected Acquisition Storage Type");
}

/*******************************************************************************
 ******************************************************************************/
bool icpSession::send
(
  ENTITY* entity,
  ETYPE   etype,
  STYPE   stype
)
{
  bool ret_val = true;
  ISMRMRD::EntityHeader head;
  std::vector <unsigned char> h_buffer;
  std::vector <unsigned char> e_buffer;

  head.version     = ISMRMRD_VERSION_MAJOR;
  head.entity_type = etype;

  switch (etype)
  {
    case ISMRMRD::ISMRMRD_MRACQUISITION:

      head.storage_type = stype;
      head.stream       = getAcqStream (entity, stype);
      e_buffer          = entity->serialize();
      break;

    case ISMRMRD::ISMRMRD_IMAGE:

      head.storage_type = stype;
      head.stream       = ISMRMRD::ISMRMRD_STREAM_IMAGE;
      e_buffer          = entity->serialize();
      break;

    case ISMRMRD::ISMRMRD_HEADER_WRAPPER:

      head.storage_type = ISMRMRD::ISMRMRD_CHAR;
      head.stream       = ISMRMRD::ISMRMRD_STREAM_ISMRMRD_HEADER;
      e_buffer          = entity->serialize();
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

    case ISMRMRD::ISMRMRD_ERROR_REPORT:
      head.storage_type = ISMRMRD::ISMRMRD_CHAR;
      head.stream       = ISMRMRD::ISMRMRD_STREAM_ERROR;
      e_buffer = entity->serialize();
      break;

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

  return;
}

/*******************************************************************************
 * Thread TODO: needs synchronization
 ******************************************************************************/
void icpSession::transmit
(
)
{
  std::cout << "Transmitter starting\n";

  while (!_stop)
  {
    OUT_MSG msg;
    if (_oq.pop (msg))
    {
      boost::asio::write (*_sock, boost::asio::buffer (msg.data, msg.data.size()));
    }
  }

  std::cout << "Transmitter done\n";
  return;
}
