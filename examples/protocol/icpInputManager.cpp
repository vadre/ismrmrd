#include "icpInputManager.h"

namespace ICPINPUTMANAGER
{
  /*****************************************************************************
   ****************************************************************************/
  icpInputManager::icpInputManager ()
  :  _session_timestamp (0),
     _send_msg_callback_registered (false),
     _xml_header_received (false),
     _client_done (false),
     _server_done (false)
  {
    //std::memset (_client_name, 0, ISMRMRD::MAX_CLIENT_NAME_LENGTH);
   
  }

  /*****************************************************************************
   ****************************************************************************/
  /*
  icpInputManager::icpInputManager
  (
    char*             name,
    uint64_t          timestamp
  )
  {
    icpInputManager::setClientName (name);
    _session_timestamp = timestamp;
  }
  */
  /*****************************************************************************
   ****************************************************************************/
  bool icpInputManager::setSendMessageCallback
  (
    bool (*func_ptr) (ISMRMRD::EntityType, ISMRMRD::Entity*)
    //SEND_MSG_CALLBACK func_ptr
  )
  {
    if (func_ptr)
    {
      icpInputManager::sendMessage = func_ptr;
      _send_msg_callback_registered = true;
    }
    return _send_msg_callback_registered;
  }

  /*****************************************************************************
   ****************************************************************************/
  void icpInputManager::setClientName
  (
    std::string name
  )
  {
    /*if (strlen (_client_name) <= 0)
    {
      memset (_client_name, 0, ISMRMRD::MAX_CLIENT_NAME_LENGTH);
      if (name)
      {
        size_t size = strlen (name);
        size = (size > ISMRMRD::MAX_CLIENT_NAME_LENGTH) ?
                ISMRMRD::MAX_CLIENT_NAME_LENGTH : size;
        strncpy (_client_name, name, size);
      }
    }
    */
    _client_name = name;
  }

  /*****************************************************************************
   ****************************************************************************/
  std::string icpInputManager::getClientName ()
  {
    return _client_name;
  }

  /*****************************************************************************
   ****************************************************************************/
  void icpInputManager::setSessionTimestamp
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
  void icpInputManager::setClientDone ()
  {
    _client_done = true;
  }

  /*****************************************************************************
   ****************************************************************************/
  bool icpInputManager::isClientDone ()
  {
    return _client_done;
  }

  /*****************************************************************************
   ****************************************************************************/
  void icpInputManager::setServerDone ()
  {
    _server_done = true;
  }

  /*****************************************************************************
   ****************************************************************************/
  bool icpInputManager::isServerDone ()
  {
    return _server_done;
  }

  /*****************************************************************************
   ****************************************************************************/
  void icpInputManager::setXmlHeaderReceived ()
  {
    _xml_header_received = true;
  }

  /*****************************************************************************
   ****************************************************************************/
  bool icpInputManager::isXmlHeaderReceived ()
  {
    return _xml_header_received;
  }

  /*****************************************************************************
   ****************************************************************************/
  bool icpInputManager::readyForImageReconstruction
  (
    uint32_t size,
    uint32_t encoding
  )
  {
    if (icpInputManager::isXmlHeaderReceived())
    {
      return (size == _xml_hdr.encoding[encoding].encodedSpace.matrixSize.y);
    }
    return false;
  }

  /*****************************************************************************
   ****************************************************************************/
  bool icpInputManager::addIsmrmrdHeader
  (
    ISMRMRD::IsmrmrdHeader hdr
  )
  {
    _xml_hdr = hdr;
    icpInputManager::setXmlHeaderReceived();
    return _xml_header_received;
  }

  /*****************************************************************************
   ****************************************************************************/
  /*
  bool icpInputManager::addMrAcquisition
  (
    //ISMRMRD::StorageType storage,
    ISMRMRD::StorageType storage,
    ISMRMRD::Entity*     acq
  )
  {
    ISMRMRD::Acquisition<float>* acq_float;
    //ISMRMRD::Acquisition<double>* acq_double;
    //ICP_ISMRMRD_MR_ACQUISITION container;

    switch (storage)
    {
      case ISMRMRD::ISMRMRD_FLOAT:

        acq_float = new ISMRMRD::Acquisition<float>;
        //container = makeAcqContainer<float>(ent);
        break;

      case ISMRMRD::ISMRMRD_DOUBLE:

        //container = makeAcqContainer<double>(ent);
        break;

      default:

        std::cout << "Warning! Unexpected acquisition storage type: "
                  << storage << ", dropping...\n";
        break;
    }

    _mr_acquisitions.push_back (acq);
    return true;
  }
  */
  /*****************************************************************************
   ****************************************************************************/
  uint64_t icpInputManager::getSessionTimestamp ()
  {
    return _session_timestamp;
  }

  /*****************************************************************************
   ****************************************************************************/
  ISMRMRD::IsmrmrdHeader icpInputManager::getIsmrmrdHeader()
  {
    return _xml_hdr;
  }

  /*****************************************************************************
   ****************************************************************************/
  /*
  std::vector<ISMRMRD::Entity*> icpInputManager::getAcquisitions ()
  {
    
    return _mr_acquisitions;
  }
  */
  /*****************************************************************************
   ****************************************************************************/
  void icpInputManager::cleanup ()
  {
    acqs_16.deleteData();
    acqs_32.deleteData();
    acqs_flt.deleteData();
    acqs_dbl.deleteData();
  }

} /* namespace ICPRECEIVEDDATA */
