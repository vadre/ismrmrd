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

typedef  void (*DispatcherFuncPtr)(ReceivedData*, OUTPUT_DUEUE);

struct icpStream
{
  icpStream ();
  icpStream (ISMRMRD::EntityHeader& hdr);
  icpStream& operator = (const icpStream &other);

  // TODO: do we need to include the label field from the struct stream
  //       defined in the xml.h ?
  ISMRMRD::EntityHeader                    entity_header;
  std::queue<std::vector<unsigned char>*>  data;
  std::atomic<bool>                        updated;
  std::atomic<bool>                        completed;
  std::atomic<bool>                        processed;

  std::mutex                               stream_mutex;
  std::condition_variable                  updated_cv;
};

//struct icpCommand
//{
  //icpCommand ();
  //icpCommand (ISMRMRD::CommandType        cmd,
              //std::vector<uint32_t>       streams,
              //std::vector<unsigned char>  config,
              //bool                        in_progress);
  //icpCommand& operator = (const icpCommand& other);

  //ISMRMRD::CommandType         cmd;
  //std::vector<uint32_t>        streams;
  //std::vector<unsigned char>   config;
  //bool                         in_progress;
//};


class ReceivedData
{
public:

  ReceivedData ();
  ReceivedData (char* name, uint64_t timestamp = 0);

  void  setSenderName (char* name);
  char* getSenderName();

  void     setSessionTimestamp (uint64_t timestamp);
  uint64_t getSessionTimestamp ();

  void addToStream (ISMRMRD::EntityHeader      hdr,
                    std::vector<unsigned char> data);

  bool extractFromStream (uint32_t   stream_num,
                          icpStream& icp_stream);

  void setRespondentDone();
  bool isRespondentDone();


private:

  void ensureStreamExist (ISMRMRD::EntityHeader& hdr);
  void deleteStream (uint32_t stream);

  char              sender_name[ISMRMRD::Max_Client_Name_Length];
  uint64_t          session_timestamp;
  std::atomic<bool> respondent_done;

  DispatcherFuncPtr               dispatcher;
  std::map<uint32_t, icpStream>   streams;
  
}; /* class ReceivedData */

} /* namespace ICPRECEIVEDDATA */

#endif /* ICPRECEIVEDDATA_H */
