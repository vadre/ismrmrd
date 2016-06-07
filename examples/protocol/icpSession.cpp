#include "icpSession.h"
#include <algorithm>
#include "icpUserAppBase.h"

/*******************************************************************************
 ******************************************************************************/
icpSession::icpSession
(
  SOCKET_PTR      sock,
  icpUserAppBase* user_obj,
  uint32_t        id  // for debug only
)
: _session_thread (),
  _transmit_thread (),
  _sock (sock),
  _user (user_obj),
  _stop (false),
  _id (id)
{
  _etypes.push_back (ISMRMRD::ISMRMRD_HANDSHAKE);
  _etypes.push_back (ISMRMRD::ISMRMRD_COMMAND);

  _session_thread = std::thread (&icpSession::run, this);
}

/*******************************************************************************
 ******************************************************************************/
icpSession::~icpSession
(
)
{
  if (_session_thread.joinable())
  {
    _session_thread.join();
  }
  _etypes.clear();
  delete (_user);
}

/*******************************************************************************
 ******************************************************************************/
void icpSession::run
(
)
{
  _transmit_thread  = std::thread (&icpSession::transmit, this);
  _user->saveSessionPointer (this);
  while (!_stop)
  {
    if (!getMessage())
    {
      std::cout << __func__ <<": getMessage Error!\n";
      _stop = true;
    }
  }

  if (_transmit_thread.joinable())
  {
    _transmit_thread.join();
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
    std::cout << "Error " << read_status << " receiving message frame data\n";
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
    std::cout << __func__ << "EOF encountered\n";
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
void icpSession::subscribe
(
  ETYPE etype,
  bool  action
)
{
  if (expected (etype))
  {
    if (!action)
    {
      _etypes.erase
        (std::remove (_etypes.begin(), _etypes.end (), etype), _etypes.end ());
    }
  }
  else if (action)
  {
    _etypes.push_back (etype);
  }
}

/*******************************************************************************
 ******************************************************************************/
bool icpSession::expected
(
  ETYPE etype
)
{
  return (std::find (_etypes.begin(), _etypes.end(), etype) != _etypes.end());
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
  ISMRMRD::Acquisition<int16_t> a16;
  ISMRMRD::Acquisition<int32_t> a32;
  ISMRMRD::Acquisition<float> aflt;
  ISMRMRD::Acquisition<double> adbl;
  ISMRMRD::Image<int16_t> i16;
  ISMRMRD::Image<uint16_t> iu16;
  ISMRMRD::Image<int32_t> i32;
  ISMRMRD::Image<uint32_t> iu32;
  ISMRMRD::Image<float> iflt;
  ISMRMRD::Image<double> idbl;
  ISMRMRD::Image<std::complex<float> > icxflt;
  ISMRMRD::Image<std::complex<double> > icxdbl;

  ETYPE etype = (ETYPE)in_msg.ehdr.entity_type;
  etype = (etype == ISMRMRD::ISMRMRD_HEADER_WRAPPER) ?
          ISMRMRD::ISMRMRD_HEADER : etype;

  if (!expected (etype))
  {
    std::cout << "Warning: dropping unexpected message, entity type = "
              << etype << "\n";
    return;
  }

  STYPE stype = (STYPE)in_msg.ehdr.storage_type;

  switch (etype)
  {
    case ISMRMRD::ISMRMRD_HANDSHAKE:

      hand.deserialize (in_msg.data);
      _user->handleHandshake (hand);
      break;

    case ISMRMRD::ISMRMRD_COMMAND:

      cmd.deserialize (in_msg.data);
      _user->handleCommand (cmd);
      break;

    case ISMRMRD::ISMRMRD_HEADER:

      wrapper.deserialize (in_msg.data);
      _user->handleIsmrmrdHeader (wrapper.getHeader());
      break;

    case ISMRMRD::ISMRMRD_MRACQUISITION:

      if (in_msg.ehdr.storage_type == ISMRMRD::ISMRMRD_SHORT)
      {
        a16.deserialize (in_msg.data);
        _user->handleAcquisition (&a16, stype);
      }
      else if (in_msg.ehdr.storage_type == ISMRMRD::ISMRMRD_INT)
      {
        a32.deserialize (in_msg.data);
        _user->handleAcquisition (&a32, stype);
      }
      if (in_msg.ehdr.storage_type == ISMRMRD::ISMRMRD_FLOAT)
      {
        aflt.deserialize (in_msg.data);
        _user->handleAcquisition (&aflt, stype);
      }
      else if (in_msg.ehdr.storage_type == ISMRMRD::ISMRMRD_DOUBLE)
      {
        adbl.deserialize (in_msg.data);
        _user->handleAcquisition (&adbl, stype);
      }
      else
      {
        std::cout <<
          "Warning! Unexpected MR Acquisition Storage type " << stype << "\n";
      }
      break;

    case ISMRMRD::ISMRMRD_ERROR_NOTIFICATION:

      err.deserialize (in_msg.data);
      _user->handleErrorNotification (err);
      break;

    case ISMRMRD::ISMRMRD_IMAGE:

      if (stype == ISMRMRD::ISMRMRD_SHORT)
      {
        i16.deserialize (in_msg.data);
        _user->handleImage (&i16, stype);
      }
      else if (stype == ISMRMRD::ISMRMRD_USHORT)
      {
        iu16.deserialize (in_msg.data);
        _user->handleImage (&iu16, stype);
      }
      else if (stype == ISMRMRD::ISMRMRD_INT)
      {
        i32.deserialize (in_msg.data);
        _user->handleImage (&i32, stype);
      }
      else if (stype == ISMRMRD::ISMRMRD_UINT)
      {
        iu32.deserialize (in_msg.data);
        _user->handleImage (&iu32, stype);
      }
      else if (stype == ISMRMRD::ISMRMRD_FLOAT)
      {
        iflt.deserialize (in_msg.data);
        _user->handleImage (&iflt, stype);
      }
      else if (stype == ISMRMRD::ISMRMRD_DOUBLE)
      {
        idbl.deserialize (in_msg.data);
        _user->handleImage (&idbl, stype);
      }
      else if (stype == ISMRMRD::ISMRMRD_CXFLOAT)
      {
        icxflt.deserialize (in_msg.data);
        _user->handleImage (&icxflt, stype);
      }
      else if (stype == ISMRMRD::ISMRMRD_CXDOUBLE)
      {
        icxdbl.deserialize (in_msg.data);
        _user->handleImage (&icxdbl, stype);
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
bool icpSession::send
(
  ISMRMRD::EntityType  etype,
  ISMRMRD::Entity*     entity,
  ISMRMRD::StorageType stype
)
{
  bool ret_val = true;
  ISMRMRD::EntityHeader head;
  std::stringstream sstr;
  std::vector <unsigned char> h_buffer;
  std::vector <unsigned char> e_buffer;

  head.version     = ISMRMRD_VERSION_MAJOR;
  head.entity_type = etype;

  switch (etype)
  {
    case ISMRMRD::ISMRMRD_MRACQUISITION:

      head.storage_type = stype;
      head.stream       = ISMRMRD::ISMRMRD_STREAM_ACQUISITION_DATA;
      e_buffer          = entity->serialize();
      break;

    case ISMRMRD::ISMRMRD_IMAGE:

      head.storage_type = stype;
      head.stream       = ISMRMRD::ISMRMRD_STREAM_IMAGE;

      std::cout << "Image storage = " << static_cast<ISMRMRD::Image<float>*>(entity)->getStorageType() << "\n";

      e_buffer = static_cast<ISMRMRD::Image<float>*>(entity)->serialize();
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
  std::cout << "Transmitter (" << _id << ") started\n";

  struct timespec  ts;
  ts.tv_sec  =     0;
  ts.tv_nsec =     10000000;

  while (!_stop || _oq.size() > 0)
  {
    while (_oq.size() > 0)
    {
      OUT_MSG msg;
      msg = _oq.front();
      boost::asio::write (*_sock,
                          boost::asio::buffer (msg.data, msg.data.size()));
      _oq.pop();
    }
    nanosleep (&ts, NULL);
  }

  std::cout << "Transmitter (" << _id << "): Done!\n";
  return;
}
