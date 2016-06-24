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
  _objects.clear();
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
template<> void icpSession::registerHandler (ICP_CB, ETYPE, icpCallback*);
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
              << ", expected " << in_msg.data_size << "\n";
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
    std::cout << "Received EOF\n";
    return ICP_ERROR_SOCKET_EOF;
  }
  else if (bytes_read != DATAFRAME_SIZE_FIELD_SIZE)
  {
    std::cout << "Error: Read " << bytes_read 
              << ", expected " << DATAFRAME_SIZE_FIELD_SIZE << "\n";
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
    std::cout << "Error: Read " << bytes_read 
              << ", expected " << ENTITY_HEADER_SIZE << "\n";
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
  IN_MSG in_msg
)
{
  uint32_t             version = in_msg.ehdr.version;
  ISMRMRD::EntityType  etype   = (ISMRMRD::EntityType)  in_msg.ehdr.entity_type;
  ISMRMRD::StorageType stype   = (ISMRMRD::StorageType) in_msg.ehdr.storage_type;
  uint32_t             stream  = in_msg.ehdr.stream;

  if (_callbacks.find (etype) == _callbacks.end())
  {
    std::cout << "Warning! received unexpected entity type: " << etype << "\n";
    return;
  }

  icpCallback* obj = _objects [etype];

  if (etype == ISMRMRD::ISMRMRD_HANDSHAKE)
  {
    HANDSHAKE ent;
    ent.deserialize (in_msg.data);
    call (etype, obj, &ent, version, etype, stype, stream);
  }
  else if (etype == ISMRMRD::ISMRMRD_COMMAND)
  {
    COMMAND ent;
    ent.deserialize (in_msg.data);
    call (etype, obj, &ent, version, etype, stype, stream);
  }
  else if (etype == ISMRMRD::ISMRMRD_HEADER)
  {
    XMLWRAP ent (in_msg.data);
    call (etype, obj, &ent, version, etype, stype, stream);
  }
  else if (etype == ISMRMRD::ISMRMRD_ERROR_REPORT)
  {
    ERRREPORT ent;
    ent.deserialize (in_msg.data);
    call (etype, obj, &ent, version, etype, stype, stream);
  }
  else if (etype == ISMRMRD::ISMRMRD_MRACQUISITION)
  {
    if (stype == ISMRMRD::ISMRMRD_SHORT)
    {
      ISMRMRD::Acquisition<int16_t> ent;
      ent.deserialize (in_msg.data);
      call (etype, obj, &ent, version, etype, stype, stream);
    }
    else if (stype == ISMRMRD::ISMRMRD_INT)
    {
      ISMRMRD::Acquisition<int32_t> ent;
      ent.deserialize (in_msg.data);
      call (etype, obj, &ent, version, etype, stype, stream);
    }
    if (stype == ISMRMRD::ISMRMRD_FLOAT)
    {
      ISMRMRD::Acquisition<float> ent;
      ent.deserialize (in_msg.data);
      call (etype, obj, &ent, version, etype, stype, stream);
    }
    else if (stype == ISMRMRD::ISMRMRD_DOUBLE)
    {
      ISMRMRD::Acquisition<double> ent;
      ent.deserialize (in_msg.data);
      call (etype, obj, &ent, version, etype, stype, stream);
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
      ISMRMRD::Image<int16_t> ent;
      ent.deserialize (in_msg.data);
      call (etype, obj, &ent, version, etype, stype, stream);
    }
    else if (stype == ISMRMRD::ISMRMRD_USHORT)
    {
      ISMRMRD::Image<uint16_t> ent;
      ent.deserialize (in_msg.data);
      call (etype, obj, &ent, version, etype, stype, stream);
    }
    else if (stype == ISMRMRD::ISMRMRD_INT)
    {
      ISMRMRD::Image<int32_t> ent;
      ent.deserialize (in_msg.data);
      call (etype, obj, &ent, version, etype, stype, stream);
    }
    else if (stype == ISMRMRD::ISMRMRD_UINT)
    {
      ISMRMRD::Image<uint32_t> ent;
      ent.deserialize (in_msg.data);
      call (etype, obj, &ent, version, etype, stype, stream);
    }
    else if (stype == ISMRMRD::ISMRMRD_FLOAT)
    {
      ISMRMRD::Image<float> ent;
      ent.deserialize (in_msg.data);
      call (etype, obj, &ent, version, etype, stype, stream);
    }
    else if (stype == ISMRMRD::ISMRMRD_DOUBLE)
    {
      ISMRMRD::Image<double> ent;
      ent.deserialize (in_msg.data);
      call (etype, obj, &ent, version, etype, stype, stream);
    }
    else if (stype == ISMRMRD::ISMRMRD_CXFLOAT)
    {
      ISMRMRD::Image<std::complex<float>> ent;
      ent.deserialize (in_msg.data);
      call (etype, obj, &ent, version, etype, stype, stream);
    }
    else if (stype == ISMRMRD::ISMRMRD_CXDOUBLE)
    {
      ISMRMRD::Image<std::complex<double>> ent;
      ent.deserialize (in_msg.data);
      call (etype, obj, &ent, etype, stype);
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
bool icpSession::send
(
  ENTITY*  entity,
  uint32_t version,
  ETYPE    etype,
  STYPE    stype,
  uint32_t stream
)
{
  ISMRMRD::EntityHeader head;

  head.version      = version;
  head.entity_type  = etype;
  head.storage_type = stype;
  head.stream       = stream;

  std::vector <unsigned char> h_buffer = head.serialize();
  std::vector <unsigned char> e_buffer = entity->serialize();

  queueMessage (h_buffer, e_buffer);

  return true;
}

/*****************************************************************************
 ****************************************************************************/
void icpSession::queueMessage
(
  std::vector<unsigned char> ent,
  std::vector<unsigned char> data
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
    if (_oq.pop (std::ref(msg)))
    {
      boost::asio::write (*_sock,
                          boost::asio::buffer (msg.data, msg.data.size()));
    }
  }

  std::cout << "Transmitter done\n";
}
