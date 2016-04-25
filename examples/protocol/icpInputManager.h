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

#include "icp.h"
#include "ismrmrd/xml.h"
#include <queue>
#include <vector>

namespace ICPINPUTMANAGER
{

struct icpStream
{
  icpStream ();
  icpStream (ISMRMRD::EntityHeader& hdr);
  icpStream& operator = (const icpStream &other);

  //mutable std::mutex                               stream_mutex;
  //mutable std::condition_variable                  updated_cv;

  ISMRMRD::EntityHeader                    entity_header;
  std::queue<std::vector<unsigned char>>   data;
  bool                                     completed;
  bool                                     processed;
};


class icpInputManager
{
public:

  icpInputManager ();
  icpInputManager (char*             name,
                   uint64_t          timestamp = 0);

  void  setClientName (char* name);
  char* getClientName();

  void     setSessionTimestamp (uint64_t timestamp);
  uint64_t getSessionTimestamp ();

  bool addToStream (ISMRMRD::EntityHeader      hdr,
                    std::vector<unsigned char> data);

  void setClientDone();
  bool isClientDone();

  void addXmlHeader (std::vector<unsigned char> data);
  ISMRMRD::IsmrmrdHeader getXmlHeader();

  std::vector<ISMRMRD::Acquisition<float> > getAcquisitions
  (
    ISMRMRD::EntityHeader      hdr
  );



private:

  void ensureStreamExist (ISMRMRD::EntityHeader& hdr);
  void deleteStream (uint32_t stream);

  char              _client_name[ISMRMRD::MAX_CLIENT_NAME_LENGTH];
  uint64_t          _session_timestamp;
  bool              _client_done;

  std::map<uint32_t, icpStream>   _streams;
  ISMRMRD::IsmrmrdHeader          _xml_hdr;
  
}; /* class icpInputManager */

} /* namespace ICPINPUTMANAGER */

#endif /* ICPINPUTMANAGER_H */
