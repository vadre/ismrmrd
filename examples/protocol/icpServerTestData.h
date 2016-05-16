/*******************************************************************************
 * Data received by the icpServerTest application
 * ISMRMRD Communication Protocol (version 2.x).
 *
 ******************************************************************************/
/**
 * @file icpServerTestData.h
 */

#pragma once
#ifndef ICPSERVERTESTDATA_H
#define ICPSERVERTESTDATA_H

#include "icpServer.h"
#include "ismrmrd/xml.h"
#include <queue>
#include <vector>

namespace ICPSERVERTESTDATA
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

  class icpServerTestData
  {
  public:

    icpServerTestData ();

    void  setClientName (std::string name);
    std::string getClientName();

    void  setClientDone ();
    bool  isClientDone();

    void  setServerDone ();
    bool  isServerDone();

    void  setXmlHeaderReceived ();
    bool  isXmlHeaderReceived();

    void     setSessionTimestamp (uint64_t timestamp);
    uint64_t getSessionTimestamp ();

    //bool setSendMessageCallback (bool (*cb) (ISMRMRD::EntityType, ISMRMRD::Entity* ent));
    //bool (*sendMessage) (ISMRMRD::EntityType, ISMRMRD::Entity* ent);

    bool addIsmrmrdHeader (ISMRMRD::IsmrmrdHeader hdr);
    ISMRMRD::IsmrmrdHeader getIsmrmrdHeader();

    bool readyForImageReconstruction (uint32_t size, uint32_t encoding);

    void cleanup ();

    ACQ_DATA<int16_t>                    acqs_16;
    ACQ_DATA<int32_t>                    acqs_32;
    ACQ_DATA<float>                      acqs_flt;
    ACQ_DATA<double>                     acqs_dbl;

    ICP_SERVER_HANDLE                    server_handle;
    icpServer*                           server_ptr;

  private:

    std::string       _client_name;
    uint64_t          _session_timestamp;
    //bool              _send_msg_callback_registered;
    bool              _xml_header_received;
    bool              _client_done;
    bool              _server_done;

    ISMRMRD::IsmrmrdHeader               _xml_hdr;
  
  }; /* class icpServerTestData */
} /* namespace ICPSERVERTESTDATA */
#endif /* ICPSERVERTESTDATA_H */
