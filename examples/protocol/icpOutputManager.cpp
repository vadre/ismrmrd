#include "icpOutputManager.h"

using boost::asio::ip::tcp;

namespace ICPOUTPUTMANAGER
{
  /*****************************************************************************
   ****************************************************************************/
  icpOutputManager::icpOutputManager ()
  {
    memset (_client_name, 0, ISMRMRD::MAX_CLIENT_NAME_LENGTH);

    _session_timestamp  = 0;
    _client_done        = false;
    _client_accepted    = false;
    _handshake_received = false;
  }

  /*****************************************************************************
   ****************************************************************************/
  icpOutputManager::icpOutputManager
  (
    uint32_t id
  )
  {
    memset (_client_name, 0, ISMRMRD::MAX_CLIENT_NAME_LENGTH);

    _session_timestamp  = 0;
    _client_done        = false;
    _client_accepted    = false;
    _server_done        = false;
    _handshake_received = false;
    _id                 = id;
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
  void icpOutputManager::setHandshakeReceived ()
  {
    _handshake_received = true;
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
    //std::cout << __func__ << ": " << _server_done << '\n';
    return _server_done;
  }

  /*****************************************************************************
   ****************************************************************************/
  void icpOutputManager::clientAccepted
  (
    bool accepted
  )
  {
    _client_accepted = accepted;

    ISMRMRD::EntityHeader e_hdr;
    e_hdr.version = my_version;
    e_hdr.entity_type = ISMRMRD::ISMRMRD_HANDSHAKE;
    e_hdr.storage_type = ISMRMRD::ISMRMRD_CHAR;
    e_hdr.stream = 65536;
    std::vector<unsigned char> ent = e_hdr.serialize();

    ISMRMRD::Handshake handshake;
    handshake.timestamp = _session_timestamp;
    handshake.conn_status = (_client_accepted) ? 
                             ISMRMRD::CONNECTION_ACCEPTED :
                             ISMRMRD::CONNECTION_DENIED_UNKNOWN_USER;
    strncpy (handshake.client_name,
             _client_name,
             ISMRMRD::MAX_CLIENT_NAME_LENGTH);
    std::vector<unsigned char> hand = handshake.serialize();

    queueMessage (ent.size() + hand.size(), ent, hand);

    int step = 0;
    std::cout << __func__ << step++ << '\n';

    if (!_client_accepted)
    {
      std::cout << __func__ << step++ << '\n';
      sendCommand (ISMRMRD::ISMRMRD_COMMAND_DONE_FROM_SERVER);
      std::cout << __func__ << step++ << '\n';
    }
    std::cout << __func__ << ": done \n";

    return;
  }

  /*****************************************************************************
   ****************************************************************************/
  void icpOutputManager::sendCommand
  (
    ISMRMRD::CommandType cmd_type
  )
  {
    int step = 0;
    std::cout << __func__ << step++ << '\n';

    ISMRMRD::EntityHeader e_hdr;
    e_hdr.version = my_version;
    e_hdr.entity_type = ISMRMRD::ISMRMRD_COMMAND;
    e_hdr.storage_type = ISMRMRD::ISMRMRD_CHAR;
    e_hdr.stream = 65537; // Todo: define reserved stream number names
    std::vector<unsigned char> ent = e_hdr.serialize();
    printf ("ent size = %ld\n", ent.size());
    std::cout << __func__ << step++ << '\n';

    ISMRMRD::Command   cmd;
    cmd.command_type = cmd_type;
    cmd.command_id   = 0;
    std::vector<unsigned char> command = cmd.serialize();
    printf ("command size = %ld\n", command.size());
    std::cout << __func__ << step++ << '\n';

    queueMessage (ent.size() + command.size(), ent, command);
    std::cout << __func__ << step++ << '\n';

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
    std::vector<unsigned char> data;
    std::copy (str.str().begin(), str.str().end(), std::back_inserter (data));

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

    std::cout << "Queueing message of size = " << size << std::endl;
    printf ("ent size = %ld, data size = %ld\n", ent.size(), data.size());

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
    std::cout << __func__ << ": num messages = " << _oq.size() << '\n';

    return;
  }

  /*****************************************************************************
   ****************************************************************************/
  void icpOutputManager::send (SOCKET_PTR sock)
  {
    //std::cout << __func__ << " starting\n";

    while (_oq.size() > 0)
    {
      OUT_MSG msg;
      //int step = 0;
      //printf ("About to send a message\n");
      msg = _oq.front();
      //std::cout << __func__ << step++ << '\n';
      boost::asio::write (*sock, boost::asio::buffer (msg.data, msg.data.size()));
      _oq.pop();
      printf ("Sent message size = %u, %lu more messages to send\n",
              msg.size, _oq.size());
      //std::cout << __func__ << step++ << '\n';
    }
    //std::cout << __func__ << " done\n";

    return;
  }

} /* namespace ICPOUTPUTMANAGER */
