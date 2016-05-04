#include "icpOutputManager.h"

using boost::asio::ip::tcp;

namespace ICPOUTPUTMANAGER
{
  /*****************************************************************************
   ****************************************************************************/
  icpOutputManager::icpOutputManager ()
  : _session_timestamp (0),
    _udata (NULL),
    _user_data_registered (false),
    _client_done (false),
    _server_done (false),
    _request_completed (false)
  {
    memset (_client_name, 0, ISMRMRD::MAX_CLIENT_NAME_LENGTH);
  }

  /*****************************************************************************
   ****************************************************************************/
  void icpOutputManager::setClientName
  (
    std::string name
  )
  {
    if (strlen (_client_name) <= 0)
    {
      if (name.size())
      {
        size_t size = (ISMRMRD::MAX_CLIENT_NAME_LENGTH < name.size()) ?
                      ISMRMRD::MAX_CLIENT_NAME_LENGTH : name.size();
        strncpy (_client_name, name.c_str(), size);
      }
    }
  }

  /*****************************************************************************
   ****************************************************************************/
  void icpOutputManager::setSessionTimestamp
  (
    uint64_t timestamp
  )
  {
    if (_session_timestamp <= 0)
    {
      _session_timestamp = timestamp;
    }
  }

  /*****************************************************************************
   ****************************************************************************/
  void icpOutputManager::setClientDone()
  {
    _client_done = true;
  }

  /*****************************************************************************
   ****************************************************************************/
  bool icpOutputManager::isClientDone()
  {
    return _client_done;
  }

  /*****************************************************************************
   ****************************************************************************/
  void icpOutputManager::setServerDone()
  {
    _server_done = true;
  }

  /*****************************************************************************
   ****************************************************************************/
  bool icpOutputManager::isServerDone()
  {
    return _server_done;
  }

  /*****************************************************************************
   ****************************************************************************/
  void icpOutputManager::setRequestCompleted()
  {
    _request_completed = true;
  }

  /*****************************************************************************
   ****************************************************************************/
  bool icpOutputManager::isRequestCompleted()
  {
    return _request_completed;
  }

  /*****************************************************************************
   ****************************************************************************/
  void icpOutputManager::clientAccepted
  (
    bool accepted
  )
  {
    ISMRMRD::EntityHeader e_hdr;
    e_hdr.version = my_version;
    e_hdr.entity_type = ISMRMRD::ISMRMRD_HANDSHAKE;
    e_hdr.storage_type = ISMRMRD::ISMRMRD_CHAR;
    e_hdr.stream = 65536;
    std::vector<unsigned char> ent = e_hdr.serialize();

    ISMRMRD::Handshake handshake;
    handshake.timestamp = _session_timestamp;
    handshake.conn_status = (accepted) ? 
                             ISMRMRD::CONNECTION_ACCEPTED :
                             ISMRMRD::CONNECTION_DENIED_UNKNOWN_USER;
    strncpy (handshake.client_name,
             _client_name,
             ISMRMRD::MAX_CLIENT_NAME_LENGTH);
    std::vector<unsigned char> hand = handshake.serialize();

    queueMessage (ent.size() + hand.size(), ent, hand);

    if (!accepted)
    {
      sendCommand (ISMRMRD::ISMRMRD_COMMAND_DONE_FROM_SERVER);
    }

    return;
  }

  /*****************************************************************************
   ****************************************************************************/
  void icpOutputManager::sendCommand
  (
    ISMRMRD::CommandType cmd_type
  )
  {
    ISMRMRD::EntityHeader e_hdr;
    e_hdr.version = my_version;
    e_hdr.entity_type = ISMRMRD::ISMRMRD_COMMAND;
    e_hdr.storage_type = ISMRMRD::ISMRMRD_CHAR;
    e_hdr.stream = 65537; // Todo: define reserved stream number names
    std::vector<unsigned char> ent = e_hdr.serialize();

    ISMRMRD::Command   cmd;
    cmd.command_type = cmd_type;
    cmd.command_id   = 0;
    std::vector<unsigned char> command = cmd.serialize();

    queueMessage (ent.size() + command.size(), ent, command);

    return;
  }

  /*****************************************************************************
   ****************************************************************************/
  void icpOutputManager::sendXmlHeader
  (
    ISMRMRD::IsmrmrdHeader hdr
  )
  {
    std::stringstream str;
    ISMRMRD::serialize (hdr, str);
    std::string xml_header = str.str();
    std::vector<unsigned char> data (xml_header.begin(), xml_header.end());
    std::cout << __func__ << ": xml serialized size = " << data.size() << "\n";
    

    ISMRMRD::EntityHeader e_hdr;
    e_hdr.version = my_version;
    e_hdr.entity_type = ISMRMRD::ISMRMRD_XML_HEADER;
    e_hdr.storage_type = ISMRMRD::ISMRMRD_CHAR;
    e_hdr.stream = 1; // TODO: define reserved stream number names
    std::vector<unsigned char> ent = e_hdr.serialize();

    queueMessage (ent.size() + data.size(), ent, data);

    return;
  }

  /*****************************************************************************
   ****************************************************************************/
  void icpOutputManager::sendImage
  (
    ISMRMRD::Image<float> img
  )
  {
    std::vector<unsigned char> image;
    image = img.serialize();

    ISMRMRD::EntityHeader e_hdr;
    e_hdr.version = my_version;
    e_hdr.entity_type = ISMRMRD::ISMRMRD_IMAGE;
    e_hdr.storage_type = ISMRMRD::ISMRMRD_FLOAT;
    e_hdr.stream = 1; // TODO: define reserved stream number names
    std::vector<unsigned char> ent = e_hdr.serialize();


    queueMessage (ent.size() + image.size(), ent, image);
    return;
  }

  /*****************************************************************************
   ****************************************************************************/
  void icpOutputManager::queueMessage
  (
    uint32_t                    size,
    std::vector<unsigned char>& ent,
    std::vector<unsigned char>& data
  )
  {
    OUT_MSG msg;
    msg.size = size;
    msg.data.reserve (size);

    uint64_t s = size;
    size = (isCpuLittleEndian) ? size : __builtin_bswap64 (size);

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
   ****************************************************************************/
  void icpOutputManager::send (SOCKET_PTR sock)
  {
    if (isRequestCompleted())
    {
      setServerDone();
      sendCommand (ISMRMRD::ISMRMRD_COMMAND_DONE_FROM_SERVER);
      std::cout << __func__ << ": sending DONE_FROM_SERVER\n";
    }
    while (_oq.size() > 0)
    {
      OUT_MSG msg;
      msg = _oq.front();
      boost::asio::write (*sock, boost::asio::buffer (msg.data, msg.data.size()));
      _oq.pop();
    }

    return;
  }

} /* namespace ICPOUTPUTMANAGER */
