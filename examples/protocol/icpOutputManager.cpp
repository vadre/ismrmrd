#include "icpOutputManager.h"

using boost::asio::ip::tcp;

namespace ICPOUTPUTMANAGER
{
  /*****************************************************************************
   processEntity is the routine known to caller as the sendMessage callback
   ****************************************************************************/
  bool icpOutputManager::processEntity
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

      case ISMRMRD::ISMRMRD_XML_HEADER:

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

      case ISMRMRD::ISMRMRD_ERROR:
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

  /*****************************************************************************
   ****************************************************************************/
  void icpOutputManager::queueMessage
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
   ****************************************************************************/
  void icpOutputManager::send (SOCKET_PTR sock)
  {
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
