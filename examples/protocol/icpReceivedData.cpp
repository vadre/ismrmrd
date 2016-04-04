#include "ismrmrd/ismrmrd.h"
#include "icpReceivedData.h"

#include <sstream>
#include <stdexcept>
#include <iostream>
#include <algorithm>

#include <string.h>


namespace ICPRECEIVEDDATA
{
  /*****************************************************************************
   ****************************************************************************/
  icpStream::icpStream ()
  {
    this->entity_header.version      = 0;
    this->entity_header.entity_type  = ISMRMRD::ISMRMRD_ERROR;
    this->entity_header.storage_type = ISMRMRD::ISMRMRD_CHAR;
    this->entity_header.stream       = 0;

    this->processed                  = false;
  }

  /*****************************************************************************
   ****************************************************************************/
  icpStream::icpStream
  (
    ISMRMRD::EntityHeader& hdr,
    bool                   processed = false
  )
  {
    this->entity_header.version      = hdr.version;
    this->entity_header.entity_type  = hdr.entity_type;
    this->entity_header.storage_type = hdr.storage_type;
    this->entity_header.stream       = hdr.stream;

    this->processed                  = processed;
  }

  /*****************************************************************************
   ****************************************************************************/
  icpStream& icpStream::operator =
  (
    const icpStream& o
  )
  {
    if (this == &o) return *this;

    this->entity_header.version       = o.entity_header.version;
    this->entity_header.entity_type   = o.entity_header.entity_type;
    this->entity_header.storage_type  = o.entity_header.storage_type;
    this->entity_header.stream        = o.entity_header.stream;
    this->data                        = o.data;
    this->processed                   = o.processed;

    return *this;
  }

  /*****************************************************************************
   ****************************************************************************/
  icpCommand::icpCommand ()
  {
    this->cmd         = ISMRMRD::ISMRMRD_COMMAND_NO_COMMAND;
    this->in_progress = false;
  }

  /*****************************************************************************
   ****************************************************************************/
  icpCommand::icpCommand
  (
    ISMRMRD::CommandType       cmd,
    std::vector<uint32_t>      streams,
    std::vector<unsigned char> config,
    bool                       in_progress
  )
  {
    this->cmd         = cmd;
    this->streams     = streams;
    this->config      = config;
    this->in_progress = in_progress;
  }

  /*****************************************************************************
   ****************************************************************************/
  icpCommand& icpCommand::operator =
  (
    const icpCommand& o
  )
  {
    if (this == &o) return *this;

    this->cmd           = o.cmd;
    this->streams       = o.streams;
    this->config        = o.config;
    this->in_progress   = o.in_progress;

    return *this;
  }

  /*****************************************************************************
   ****************************************************************************/
  ReceivedData::ReceivedData ()
  {
    memset (sender_name, 0, ISMRMRD::Max_Client_Name_Length);
    session_timestamp = 0;
    respondent_done   = false;
  }

  /*****************************************************************************
   ****************************************************************************/
  ReceivedData::ReceivedData
  (
    char* name,
    uint64_t timestamp
   )
  {
    ReceivedData::setSessionTimestamp (timestamp);
    ReceivedData::setSenderName (name);
    respondent_done   = false;
  }

  /*****************************************************************************
   ****************************************************************************/
  void ReceivedData::setSenderName
  (
    char* name
  )
  {
    memset (sender_name, 0, ISMRMRD::Max_Client_Name_Length);
    if (name)
    {
      size_t size = strlen (name);
      size = (size > ISMRMRD::Max_Client_Name_Length) ?
              ISMRMRD::Max_Client_Name_Length : size;
      strncpy (sender_name, name, size);
    }
  }

  /*****************************************************************************
   ****************************************************************************/
  char* ReceivedData::getSenderName ()
  {
    return sender_name;
  }

  /*****************************************************************************
   ****************************************************************************/
  void ReceivedData::setSessionTimestamp
  (
    uint64_t timestamp
  )
  {
    session_timestamp = timestamp;
  }

  /*****************************************************************************
   ****************************************************************************/
  uint64_t ReceivedData::getSessionTimestamp ()
  {
    return session_timestamp;
  }

  /*****************************************************************************
   ****************************************************************************/
  void ReceivedData::ensureStreamExist
  (
    ISMRMRD::EntityHeader& hdr
  )
  {
    if (streams.find (hdr.stream) == streams.end())
    {
      icpStream tmp (hdr, false);
      //streams [hdr.stream] = tmp;
      streams.insert (std::make_pair (hdr.stream, tmp));
    }
    else if (streams.at (hdr.stream).entity_header.entity_type  !=
               hdr.entity_type ||
             streams.at (hdr.stream).entity_header.storage_type !=
               hdr.storage_type)
    {
      // TODO: Throw an error - all messages of a stream must have the same
      //       entity type and storage type
      std::cout << "Entity and/or Storage types for stream " << hdr.stream <<
                   " differ" << std::endl;
    }
  }

  /*****************************************************************************
   ****************************************************************************/
  void ReceivedData::setRespondentDone()
  {
    respondent_done = true;
  }

  /*****************************************************************************
   ****************************************************************************/
  bool ReceivedData::isRespondentDone()
  {
    return respondent_done;
  }

  /*****************************************************************************
   ****************************************************************************/
  void ReceivedData::addXMLHeader
  (
    ISMRMRD::EntityHeader      entity_header,
    std::vector<unsigned char> data
  )
  {
    // TODO: Needs synchronization
    ISMRMRD::deserialize ((const char*) &data[0], xml_header);
    return;
  }

  /*****************************************************************************
   ****************************************************************************/
  ISMRMRD::IsmrmrdHeader ReceivedData::getXMLHeader()
  {
    // TODO: Needs synchronization
    return xml_header;
  }

  /*****************************************************************************
   ****************************************************************************/
  void ReceivedData::addToStream
  (
    ISMRMRD::EntityHeader      hdr,
    std::vector<unsigned char> data
  )
  {
    // TODO: Needs synchronization
    ensureStreamExist (hdr);
    streams.at (hdr.stream).data.push (data);
  }

  /*****************************************************************************
   ****************************************************************************/
  bool ReceivedData::extractFromStream
  (
    uint32_t    stream_num,
    icpStream&  icp_stream
  )
  {
    // TODO: Needs synchronization

    bool success = false;

    if (streams.find (stream_num) != streams.end())
    {
      icpStream tmp = streams.at (stream_num);

      if (tmp.data.size() > 0)
      {
        icp_stream = tmp;
        while (streams.at (stream_num).data.size() > 0)
        {
          streams.at (stream_num).data.pop();
        }
        success = true;
      }
    }

    return success;
  }

  /*****************************************************************************
   ****************************************************************************/
  void ReceivedData::deleteStream (uint32_t stream)
  {
    streams.erase (stream);
  }

} /* namespace ICPRECEIVEDDATA */
