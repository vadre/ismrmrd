#include "icpServerTestData.h"

namespace ICPSERVERTESTDATA
{
  /*****************************************************************************
   ****************************************************************************/
  icpServerTestData::icpServerTestData ()
  :  _session_timestamp (0),
     //_send_msg_callback_registered (false),
     _xml_header_received (false),
     _client_done (false),
     _server_done (false)
  {
  }

  /*****************************************************************************
   ****************************************************************************/
  //bool icpServerTestData::setSendMessageCallback
  //(
    //bool (*func_ptr) (ISMRMRD::EntityType, ISMRMRD::Entity*)
    //SEND_MSG_CALLBACK func_ptr
  //)
  //{
    //if (func_ptr)
    //{
      //icpServerTestData::sendMessage = func_ptr;
      //_send_msg_callback_registered = true;
    //}
    //return _send_msg_callback_registered;
  //}

  /*****************************************************************************
   ****************************************************************************/
  void icpServerTestData::setClientName
  (
    std::string name
  )
  {
    _client_name = name;
  }

  /*****************************************************************************
   ****************************************************************************/
  std::string icpServerTestData::getClientName ()
  {
    return _client_name;
  }

  /*****************************************************************************
   ****************************************************************************/
  void icpServerTestData::setSessionTimestamp
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
  void icpServerTestData::setClientDone ()
  {
    _client_done = true;
  }

  /*****************************************************************************
   ****************************************************************************/
  bool icpServerTestData::isClientDone ()
  {
    return _client_done;
  }

  /*****************************************************************************
   ****************************************************************************/
  void icpServerTestData::setServerDone ()
  {
    _server_done = true;
  }

  /*****************************************************************************
   ****************************************************************************/
  bool icpServerTestData::isServerDone ()
  {
    return _server_done;
  }

  /*****************************************************************************
   ****************************************************************************/
  void icpServerTestData::setXmlHeaderReceived ()
  {
    _xml_header_received = true;
  }

  /*****************************************************************************
   ****************************************************************************/
  bool icpServerTestData::isXmlHeaderReceived ()
  {
    return _xml_header_received;
  }

  /*****************************************************************************
   ****************************************************************************/
  bool icpServerTestData::readyForImageReconstruction
  (
    uint32_t size,
    uint32_t encoding
  )
  {
    if (icpServerTestData::isXmlHeaderReceived())
    {
      return (size == _xml_hdr.encoding[encoding].encodedSpace.matrixSize.y);
    }
    return false;
  }

  /*****************************************************************************
   ****************************************************************************/
  bool icpServerTestData::addIsmrmrdHeader
  (
    ISMRMRD::IsmrmrdHeader hdr
  )
  {
    _xml_hdr = hdr;
    icpServerTestData::setXmlHeaderReceived();
    return _xml_header_received;
  }

  /*****************************************************************************
   ****************************************************************************/
  uint64_t icpServerTestData::getSessionTimestamp ()
  {
    return _session_timestamp;
  }

  /*****************************************************************************
   ****************************************************************************/
  ISMRMRD::IsmrmrdHeader icpServerTestData::getIsmrmrdHeader()
  {
    return _xml_hdr;
  }

  /*****************************************************************************
   ****************************************************************************/
  void icpServerTestData::cleanup ()
  {
    acqs_16.deleteData();
    acqs_32.deleteData();
    acqs_flt.deleteData();
    acqs_dbl.deleteData();
  }

} /* namespace ICPRECEIVEDDATA */
