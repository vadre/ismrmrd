/*******************************************************************************
 * Data class for the icpClientTest application
 * ISMRMRD Communication Protocol (version 2.x).
 *
 ******************************************************************************/

#pragma once
#ifndef ICPCLIENTTESTDATA_H
#define ICPCLIENTTESTDATA_H

#include "ismrmrd/ismrmrd.h"
#include "ismrmrd/xml.h"
#include <queue>
#include <vector>

namespace ICPCLIENTTESTDATA
{
  class icpClientTestData
  {
  public:
         icpClientTestData();
    void setServerDone();
    bool isServerDone();
    void setSessionClosed();
    bool isSessionClosed();
    bool addIsmrmrdHeader(ISMRMRD::IsmrmrdHeader hdr);
    ISMRMRD::IsmrmrdHeader getIsmrmrdHeader ();
    std::string getIsmrmrdHeaderSerialized ();
    bool isIsmrmrdHeaderSet();
    void setInputFileName (std::string fname);
    std::string getInputFileName ();
    void setOutputFileName (std::string fname);
    std::string getOutputFileName ();
    void setInputDataset (std::string dset_name);
    std::string getInputDataset ();
    void setOutputDataset (std::string dset_name);
    std::string getOutputDataset ();
    void setClientName (std::string name);
    std::string getClientName ();

    //SEND_MSG_CALLBACK sendMessage;
    //bool (*sendMessage)(ISMRMRD::EntityType, ISMRMRD::Entity*);

  private:
    std::string _client_name;
    std::string _input_file_name;
    std::string _output_file_name;
    std::string _input_dset_name;
    std::string _output_dset_name;
    bool        _server_done;
    bool        _session_closed;
    bool        _ismrmrd_header_received;
    ISMRMRD::IsmrmrdHeader _ismrmrd_header; 

  }; /* class icpClientTestData */
} /* namespace ICPCLIENTTESTDATA */
#endif /* ICPCLIENTTESTDATA_H */

