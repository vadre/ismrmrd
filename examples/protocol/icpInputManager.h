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

  class icpInputManager
  {
  public:

    icpInputManager ();
    icpInputManager (char*             name,
                     uint64_t          timestamp = 0);

    void  setClientName (char* name);
    char* getClientName();

    void  setClientDone ();
    bool  isClientDone();

    void     setSessionTimestamp (uint64_t timestamp);
    uint64_t getSessionTimestamp ();

    bool setSendMessageCallback (bool (*cb) (ISMRMRD::EntityType, ISMRMRD::Entity* ent));
    bool (*sendMessage) (ISMRMRD::EntityType, ISMRMRD::Entity* ent);
    //bool setSendMessageCallback (SEND_MSG_CALLBACK func_ptr);
    //SEND_MSG_CALLBACK sendMessage;
  

    bool addIsmrmrdHeader (ISMRMRD::IsmrmrdHeader hdr);
    ISMRMRD::IsmrmrdHeader getXmlHeader();

    bool addMrAcquisition (ISMRMRD::Entity* acq);
    std::vector<ISMRMRD::Entity*> getAcquisitions ();
    bool readyForImageReconstruction ();

    void cleanup ();

  private:

    char              _client_name[ISMRMRD::MAX_CLIENT_NAME_LENGTH];
    uint64_t          _session_timestamp;
    bool              _send_msg_callback_registered;
    bool              _client_done;

    ISMRMRD::IsmrmrdHeader          _xml_hdr;
    std::vector<ISMRMRD::Entity*> _mr_acquisitions;
  
  }; /* class icpInputManager */

} /* namespace ICPINPUTMANAGER */

#endif /* ICPINPUTMANAGER_H */
