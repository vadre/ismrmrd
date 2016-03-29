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
#include <queue>
#include <map>
#include "ismrmrd/ismrmrd.h"
#include "ismrmrd/xml.h"

namespace ICPRECEIVEDDATA
{

struct icpStream
{
  icpStream (ISMRMRD::EntityHeader& hdr, bool complete);
  icpStream& operator = (const icpStream &other);

  // TODO: do we need to include the label field from the struct stream
  //       defined in the xml.h ?
  ISMRMRD::EntityHeader                    entity_header;
  std::queue<std::vector<unsigned char> >  data;
  bool                                     processed;
};

struct icpCommand
{
  icpCommand ();
  icpCommand (ISMRMRD::CommandType        cmd,
              std::vector<uint32_t>       streams,
              std::vector<unsigned char>  config,
              bool                        in_progress);
  icpCommand& operator = (const icpCommand& other);

  ISMRMRD::CommandType         cmd;
  std::vector<uint32_t>        streams;
  std::vector<unsigned char>   config;
  bool                         in_progress;
};


class ReceivedData
{
public:

  ReceivedData ();
  ReceivedData (char* name, uint64_t timestamp = 0);

  void  setSenderName (char* name);
  char* getSenderName();

  void     setSessionTimestamp (uint64_t timestamp);
  uint64_t getSessionTimestamp ();

  //void addCommand (ISMRMRD::EntityHeader  header,
                   //ISMRMRD::Command       command);

  void addXMLHeader (ISMRMRD::EntityHeader      entity_header,
                     std::vector<unsigned char> data);

  void addToStream (ISMRMRD::EntityHeader      hdr,
                    std::vector<unsigned char> data);

  //bool getCommand  (ICPRECEIVEDDATA::icpCommand& command,
                    //uint32_t&                    cmd_id);
  void setClientDone();

protected:

  void ensureStreamExist (ISMRMRD::EntityHeader& hdr);

  char      sender_name[ISMRMRD::Max_Client_Name_Length];
  uint64_t  session_timestamp;
  bool      client_done;

  ISMRMRD::IsmrmrdHeader         xml_header;
  std::map<uint32_t, icpStream>  streams;
  std::map<uint32_t, icpCommand> commands;

  
}; /* class ReceivedData */

} /* namespace ICPRECEIVEDDATA */

#endif /* ICPRECEIVEDDATA_H */
