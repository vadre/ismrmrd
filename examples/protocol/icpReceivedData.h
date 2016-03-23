/*******************************************************************************
 * Heterogeneous collection of entities received from network connection during
 * communication session via ISMRMRD Communication Protocol (version 2.x).
 *
 ******************************************************************************/

/**
 * @file icpReceivedData.h
 */

#pragma once
#ifndef ICPRECEIVEDDATA_H
#define ICPRECEIVEDDATA_H

#include <complex>
#include <vector>
#include "ismrmrd/ismrmrd.h"

namespace ICPRECEIVEDDATA
{

struct icpStream
{
  icpStream (ISMRMRD::EntityHeader,
             bool complete);

  icpStream& operator = (const icpStream &other);

  ISMRMRD::EntityHeader                    entity_header;
  std::queue<std::vector<unsigned char> >  data;
  // Every new stream is created with the complete flag
  // initialized to false. Determination whether a parti-
  // cular stream is made later using the data-specific
  // criteria. In some cases a strem can be determined
  // complete when the STOP/DONE command is received.
  bool                                     complete;
};


class ReceivedData
{
public:

  RecievedData ();
  RecievedData (char* name, uint64_t timestamp = 0);

  void  setSenderName (char* name);
  char* getSenderName();

  void     setSessionTimestamp (uint64_t timestamp);
  uint64_t getSessionTimestamp ();

  void addCommand (ISMRMRD::EntityHeader  header,
                   ISMRMRD::Command       command);

  void addToStream (ISMRMRD::EntityHeader hdr);

  bool getCommand  (ISMRMRD::CommandType& command,
                    uint32_t&             stream);

protected:

  uint32_t checkStreamExist (uint32_t stream,
                             ISMRMRD::EntityType,
                             ISMRMRD::StorageType);

  char      sender_name[ISMRMRD::Max_Client_Name_Length];
  uint64_t  session_timestamp;

  std::map<uint32_t, icpStream>  streams;

  
}; /* class ReceivedData */

} /* namespace ICPRECEIVEDDATA */

#endif /* ICPRECEIVEDDATA_H */
