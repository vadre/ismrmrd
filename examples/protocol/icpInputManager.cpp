#include "icpInputManager.h"

namespace ICPINPUTMANAGER
{
  /*****************************************************************************
   ****************************************************************************/
  icpInputManager::icpInputManager ()
  {
    memset (_client_name, 0, ISMRMRD::MAX_CLIENT_NAME_LENGTH);
    _session_timestamp = 0;
  }

  /*****************************************************************************
   ****************************************************************************/
  icpInputManager::icpInputManager
  (
    char*             name,
    uint64_t          timestamp
  )
  {
    icpInputManager::setClientName (name);
    _session_timestamp = timestamp;
  }

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
    char* name
  )
  {
    if (strlen (_client_name) <= 0)
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
  }

  /*****************************************************************************
   ****************************************************************************/
  char* icpInputManager::getClientName ()
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
  bool icpInputManager::readyForImageReconstruction ()
  {
    bool ret_val = false;
    if (...........................................................
    return ret_val;
  }

  /*****************************************************************************
   ****************************************************************************/
  bool icpInputManager::addIsmrmrdHeader
  (
    ISMRMRD::IsmrmrdHeader hdr
  )
  {
    _xml_hdr = hdr;
    return true;
  }

  /*****************************************************************************
   ****************************************************************************/
  bool icpInputManager::addMrAcquisition
  (
    ISMRMRD::Entity* acq
  )
  {
    _mr_acquisitions.push_back (acq);
    return true;
  }

  /*****************************************************************************
   ****************************************************************************/
  uint64_t icpInputManager::getSessionTimestamp ()
  {
    return _session_timestamp;
  }

  /*****************************************************************************
   ****************************************************************************/
  ISMRMRD::IsmrmrdHeader icpInputManager::getXmlHeader()
  {
    return _xml_hdr;
  }

  /*****************************************************************************
   ****************************************************************************/
  std::vector<ISMRMRD::Entity*> icpInputManager::getAcquisitions ()
  {
    
    return _mr_acquisitions;
  }

  /*****************************************************************************
   ****************************************************************************/
  void icpInputManager::cleanup ()
  {
    _mr_acquisitions.clear();
  }

} /* namespace ICPRECEIVEDDATA */
