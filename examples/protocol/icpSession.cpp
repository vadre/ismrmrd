#include "icpSession.h"

namespace ISMRMRD { namespace ICP
{
/*******************************************************************************
 ******************************************************************************/
Session::Session
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
Session::~Session
(
)
{
  _callbacks.clear();
  _objects.clear();
}

/*******************************************************************************
 ******************************************************************************/
void Session::run
(
)
{
  _transmitter = std::thread (&Session::transmit, this);
  while (!_stop)
  {
    if (!getMessage())
    {
      std::cerr << "Error from Session::getMessage\n";
      std::cerr << "Session shutting down\n";
      shutdown();
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
void Session::shutdown
(
)
{
  _stop = true;
  _oq.stop ();
}

/*******************************************************************************
 ******************************************************************************/
template<> void Session::registerHandler (CB, ETYPE, Callback*);
template class MTQueue<OUT_MSG>;

/*******************************************************************************
 ******************************************************************************/
bool Session::getMessage
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
uint32_t Session::receiveFrameInfo
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
    return ERROR_SOCKET_EOF;
  }
  else if (bytes_read != DATAFRAME_SIZE_FIELD_SIZE)
  {
    std::cout << "Error: Read " << bytes_read 
              << ", expected " << DATAFRAME_SIZE_FIELD_SIZE << "\n";
    return ERROR_SOCKET_WRONG_LENGTH;
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
    return ERROR_SOCKET_WRONG_LENGTH;
  }

  in_msg.ehdr.deserialize (buffer);
  in_msg.data_size = in_msg.size - ENTITY_HEADER_SIZE;

  return 0;
}

/*******************************************************************************
 ******************************************************************************/
void Session::deliver
(
  IN_MSG in_msg
)
{
  ETYPE  etype  = (EntityType)  in_msg.ehdr.entity_type;
  STYPE stype   = (StorageType) in_msg.ehdr.storage_type;

  if (_callbacks.find (etype) == _callbacks.end())
  {
    std::cout << "Warning! received unexpected entity type: " << etype << "\n";
    return;
  }

  Callback* obj = _objects [etype];

  if (etype == ISMRMRD_HANDSHAKE)
  {
    HANDSHAKE ent;
    ent.deserialize (in_msg.data);
    call (etype, obj, &ent);
  }
  else if (etype == ISMRMRD_COMMAND)
  {
    COMMAND ent;
    ent.deserialize (in_msg.data);
    call (etype, obj, &ent);
  }
  else if (etype == ISMRMRD_HEADER)
  {
    XMLHEAD ent;
    ent.deserialize (in_msg.data);
    call (etype, obj, &ent);
  }
  else if (etype == ISMRMRD_ERROR_REPORT)
  {
    ERRREPORT ent;
    ent.deserialize (in_msg.data);
    call (etype, obj, &ent);
  }
  else if (etype == ISMRMRD_MRACQUISITION)
  {
    if (stype == ISMRMRD_SHORT)
    {
      Acquisition<int16_t> ent;
      ent.deserialize (in_msg.data);
      call (etype, obj, &ent);
    }
    else if (stype == ISMRMRD_INT)
    {
      Acquisition<int32_t> ent;
      ent.deserialize (in_msg.data);
      call (etype, obj, &ent);
    }
    if (stype == ISMRMRD_FLOAT)
    {
      Acquisition<float> ent;
      ent.deserialize (in_msg.data);
      call (etype, obj, &ent);
    }
    else if (stype == ISMRMRD_DOUBLE)
    {
      Acquisition<double> ent;
      ent.deserialize (in_msg.data);
      call (etype, obj, &ent);
    }
    else
    {
      std::cout << "Warning! Unexpected MR Acquisition Storage type "
                << stype << "\n";
    }
  }
  else if (etype == ISMRMRD_IMAGE)
  {
    if (stype == ISMRMRD_SHORT)
    {
      Image<int16_t> ent;
      ent.deserialize (in_msg.data);
      call (etype, obj, &ent);
    }
    else if (stype == ISMRMRD_USHORT)
    {
      Image<uint16_t> ent;
      ent.deserialize (in_msg.data);
      call (etype, obj, &ent);
    }
    else if (stype == ISMRMRD_INT)
    {
      Image<int32_t> ent;
      ent.deserialize (in_msg.data);
      call (etype, obj, &ent);
    }
    else if (stype == ISMRMRD_UINT)
    {
      Image<uint32_t> ent;
      ent.deserialize (in_msg.data);
      call (etype, obj, &ent);
    }
    else if (stype == ISMRMRD_FLOAT)
    {
      Image<float> ent;
      ent.deserialize (in_msg.data);
      call (etype, obj, &ent);
    }
    else if (stype == ISMRMRD_DOUBLE)
    {
      Image<double> ent;
      ent.deserialize (in_msg.data);
      call (etype, obj, &ent);
    }
    else if (stype == ISMRMRD_CXFLOAT)
    {
      Image<std::complex<float>> ent;
      ent.deserialize (in_msg.data);
      call (etype, obj, &ent);
    }
    else if (stype == ISMRMRD_CXDOUBLE)
    {
      Image<std::complex<double>> ent;
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
void Session::call
(
  uint32_t index,
  A&&      ... args
)
{
  if (_callbacks.find (index) != _callbacks.end())
  {
    using func_t = CbStruct <A...>;
    using cb_t   = std::function <void (A...)>;

    const CbBase& base = *_callbacks[index];
    const cb_t& func = static_cast <const func_t&> (base).callback;

    func (std::forward <A> (args)...);
  }
}

/*******************************************************************************
 ******************************************************************************/
bool Session::send
(
  ENTITY*  entity
)
{
  EntityHeader head;

  head.version      = entity->getVersion();
  head.entity_type  = entity->getEntityType();
  head.storage_type = entity->getStorageType();
  head.stream       = entity->getStream();

  std::vector <unsigned char> h_buffer = head.serialize();
  std::vector <unsigned char> e_buffer = entity->serialize();

  queueMessage (h_buffer, e_buffer);

  return true;
}

/*****************************************************************************
 ****************************************************************************/
void Session::queueMessage
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
void Session::transmit
(
)
{
  std::cout << "Transmitter starting\n";

  while (!_stop)
  {
    OUT_MSG msg;
    if (_oq.pop (std::ref(msg)))
    {
      try
      {
        boost::asio::write (*_sock,
                            boost::asio::buffer (msg.data, msg.data.size()));
      }
      catch (const boost::system::system_error& e)
      {
        std::cerr << e.what() << "\n";
        std::cerr << "Session shutting down\n";
        shutdown();
      }
      catch (...)
      {
        std::cerr << "Unknown exception from boost::asio::write\n";
        std::cerr << "Session shutting down\n";
        shutdown ();
      }
    }
  }

  std::cout << "Transmitter done\n";
}
}} // end of namespace ISMRMRD::ICP
