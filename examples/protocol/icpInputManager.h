/*******************************************************************************
 * Data received from network connection during communication session via
 * ISMRMRD Communication Protocol (version 2.x).
 *
 ******************************************************************************/

/**
 * @file icpInputManager.h
 */

#pragma once
#ifndef ICPINPUTMANAGER_H
#define ICPINPUTMANAGER_H

#include "icpServer.h"
#include "ismrmrd/xml.h"
#include <queue>
#include <vector>

namespace ICPINPUTMANAGER
{

  template <typename T>
  struct ACQ_DATA
  {
    void addAcq (ISMRMRD::Acquisition<T>& acq)
    {
      _vec.push_back (acq);
    }
    bool getAcqs (std::vector<ISMRMRD::Acquisition<T> >& vec)
    {
      vec = _vec;
      return (_vec.size() > 0);
    }
    uint32_t getSize ()
    {
      return _vec.size();
    }
    void deleteData ()
    {
      _vec.clear ();
    }

  private:
    std::vector<ISMRMRD::Acquisition<T> > _vec;
  };

  //template struct ACQ_DATA<int16_t>;
  //template struct ACQ_DATA<int32_t>;
  //template struct ACQ_DATA<float>;
  //template struct ACQ_DATA<double>;

  class icpInputManager
  {
  public:

    icpInputManager ();
    //icpInputManager (char*             name,
                     //uint64_t          timestamp = 0);

    void  setClientName (std::string name);
    std::string getClientName();
    //char* getClientName();

    void  setClientDone ();
    bool  isClientDone();

    void  setServerDone ();
    bool  isServerDone();

    void  setXmlHeaderReceived ();
    bool  isXmlHeaderReceived();

    void     setSessionTimestamp (uint64_t timestamp);
    uint64_t getSessionTimestamp ();

    bool setSendMessageCallback (bool (*cb) (ISMRMRD::EntityType, ISMRMRD::Entity* ent));
    bool (*sendMessage) (ISMRMRD::EntityType, ISMRMRD::Entity* ent);
    //bool setSendMessageCallback (SEND_MSG_CALLBACK func_ptr);
    //SEND_MSG_CALLBACK sendMessage;
  

    bool addIsmrmrdHeader (ISMRMRD::IsmrmrdHeader hdr);
    ISMRMRD::IsmrmrdHeader getIsmrmrdHeader();

    //bool addMrAcquisition (ISMRMRD::StorageType storage, ISMRMRD::Entity* acq);
    //std::vector<ISMRMRD::Entity*> getAcquisitions ();
    bool readyForImageReconstruction (uint32_t size, uint32_t encoding);

    void cleanup ();

    ACQ_DATA<int16_t>                    acqs_16;
    ACQ_DATA<int32_t>                    acqs_32;
    ACQ_DATA<float>                      acqs_flt;
    ACQ_DATA<double>                     acqs_dbl;

  private:

    std::string       _client_name;
    //char              _client_name[ISMRMRD::MAX_CLIENT_NAME_LENGTH];
    uint64_t          _session_timestamp;
    bool              _send_msg_callback_registered;
    bool              _xml_header_received;
    bool              _client_done;
    bool              _server_done;

    ISMRMRD::IsmrmrdHeader               _xml_hdr;
  
  }; /* class icpInputManager */

} /* namespace ICPINPUTMANAGER */

#endif /* ICPINPUTMANAGER_H */
