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
  icpStream::icpStream
  (
    ISMRMRD::EntityType  ent = ISMRMRD::ISMRMRD_ERROR,
    ISMRMRD::StorageType sto = ISMRMRD::ISMRMRD_CHAR,
    ISMRMRD::CommandType cmd = ISMRMRD::ISMRMRD_COMMAND_NO_COMMAND,
    ISMRMRD::ErrorType   err = ISMRMRD::ISMRMRD_ERROR_NO_ERROR,
    bool                 complete = false) :
    entity_type  (ent),
    storage_type (sto),
    this->complete (complete)
  {
    if (cmd != ISMRMRD::ISMRMRD_COMMAND_NO_COMMAND)
    {
      commands.push (cmd);
    }

    if (err != ISMRMRD::ISMRMRD_ERROR_NO_ERROR)
    {
      errors.push (err);
    }
  }

  /*****************************************************************************
   ****************************************************************************/
  icpStream& icpStream::operator= (const icpStream& o)
  {
    if (this == &o) return *this;

    this->commands      = o.commands;
    this->errors        = o.errors;
    this->entity_type   = o.entity_type;
    this->strorage_type = o.storage_type;
    this->description   = o.description;
    this->data          = o.data;
    this->complete      = o.complete;

    return *this;
  }

  /*****************************************************************************
   ****************************************************************************/
  ReceivedData::RecievedData ()
  {
    memset (sender_name, 0, ISMRMRD::Max_Client_Name_Length);
    session_timestamp = 0;
  }

  /*****************************************************************************
   ****************************************************************************/
  ReceivedData::RecievedData (char* name, uint64_t timestamp)
  {
    ReceivedData::setSessionTimestamp (timestamp);
    ReceivedData::setSenderName (name);
  }

  /*****************************************************************************
   ****************************************************************************/
  void ReceivedData::setSenderName (char* name)
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
  void ReceivedData::setSessionTimestamp (uint64_t timestamp)
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
  void ReceivedData::checkStreamExist
  (
    uint32_t                stream,
    ISMRMRD::EntityType     ent,
    ISMRMRD::StorageType    stor
  )
  {
    if (streams.find (stream) == streams.end())
    {
      streams (stream) = icpStream tmp (ent, stor);
    }
    else
    {
      if ( streams.at (stream).entity_type != ent ||
           streams.at (stream).storage_type != stor )
      {
        // Throw an error - all messages of a stream must have the same
        // entity type and storage type
        std::cout << "Entity and/or Storage types for stream " << stream <<
                     " differ" << std::endl;
      }
    }
  }

  /*****************************************************************************
   ****************************************************************************/
  void ReceivedData::addCommand (ISMRMRD::EntityHeader  hdr,
                                 ISMRMRD::Command       cmd)
  {
    checkStreamExist (hdr.stream, hdr.entity_type, hdr.storage_type);

    streams.at (hdr.stream).commands.push (cmd.command_type);

    if (cmd.error_type != ISMRMRD::ISMRMRD_ERROR_NO_ERROR)
    {
      streams.at (hdr.stream).errors.push (cmd.error_type);
    }

    // Evaluate the command/error to see if an action is immediately needed.
  }

  /*****************************************************************************
   ****************************************************************************/
  void ReceivedData::addToStream
  (
    ISMRMRD::EntityType        hdr,
    std::vector<unsigned char> data
  )
  {
    checkStreamExist (hdr.stream, hdr.entity_type, hdr.storage_type);
    streams.at (hdr.stream).data.push (data);
    // TODO: Check against the stream description in the XML Header if this
    //       stream is complete and ready for processing. If so - process and
    //       then delete the stream, or possibly mark the stream as processed.
  }

  /*****************************************************************************
   ****************************************************************************/
  bool ReceivedData::getCommand
  (
    ISMRMRD::CommandType& cmd,
    uint32_t&             stream
  )
  {
    bool more_to_process = true;

    

    return more_to_process;
  }



} /* namespace ICPRECEIVEDDATA */
