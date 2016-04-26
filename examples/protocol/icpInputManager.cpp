#include "icpInputManager.h"

namespace ICPINPUTMANAGER
{
  /*****************************************************************************
   ****************************************************************************/
  icpStream::icpStream ()
  {
    this->entity_header.version      = 0;
    this->entity_header.entity_type  = ISMRMRD::ISMRMRD_ERROR;
    this->entity_header.storage_type = ISMRMRD::ISMRMRD_CHAR;
    this->entity_header.stream       = 0;

    this->completed                  = false;
    this->processed                  = false;
  }

  /*****************************************************************************
   ****************************************************************************/
  icpStream::icpStream
  (
    ISMRMRD::EntityHeader& hdr
  )
  {
    this->entity_header.version      = hdr.version;
    this->entity_header.entity_type  = hdr.entity_type;
    this->entity_header.storage_type = hdr.storage_type;
    this->entity_header.stream       = hdr.stream;

    this->completed                  = false;
    this->processed                  = false;
  }

  /*****************************************************************************
   ****************************************************************************/
  icpInputManager::icpInputManager ()
  {
    memset (_client_name, 0, ISMRMRD::MAX_CLIENT_NAME_LENGTH);
    _session_timestamp = 0;
  }

  /*****************************************************************************
   ****************************************************************************/
  icpInputManager::icpInputManager
  (
    char*             name,
    uint64_t          timestamp
  )
  {
    icpInputManager::setClientName (name);
    _session_timestamp = timestamp;
  }

  /*****************************************************************************
   ****************************************************************************/
  void icpInputManager::setClientName
  (
    char* name
  )
  {
    if (strlen (_client_name) <= 0)
    {
      memset (_client_name, 0, ISMRMRD::MAX_CLIENT_NAME_LENGTH);
      if (name)
      {
        size_t size = strlen (name);
        size = (size > ISMRMRD::MAX_CLIENT_NAME_LENGTH) ?
                ISMRMRD::MAX_CLIENT_NAME_LENGTH : size;
        strncpy (_client_name, name, size);
      }
    }
  }

  /*****************************************************************************
   ****************************************************************************/
  char* icpInputManager::getClientName ()
  {
    return _client_name;
  }

  /*****************************************************************************
   ****************************************************************************/
  void icpInputManager::setSessionTimestamp
  (
    uint64_t timestamp
  )
  {
    if (_session_timestamp <= 0)
    {
      _session_timestamp = timestamp;
    }
  }

  /*****************************************************************************
   ****************************************************************************/
  uint64_t icpInputManager::getSessionTimestamp ()
  {
    return _session_timestamp;
  }

  /*****************************************************************************
   ****************************************************************************/
  void icpInputManager::ensureStreamExist
  (
    ISMRMRD::EntityHeader& hdr
  )
  {
    if (_streams.find (hdr.stream) == _streams.end())
    {
      icpStream tmp (hdr);
      _streams.insert (std::make_pair (hdr.stream, tmp));
    }
    else if (_streams.at (hdr.stream).entity_header.entity_type  !=
               hdr.entity_type ||
             _streams.at (hdr.stream).entity_header.storage_type !=
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
  bool icpInputManager::addToStream
  (
    ISMRMRD::EntityHeader      hdr,
    std::vector<unsigned char> data
  )
  {
    ensureStreamExist (hdr);
    _streams.at (hdr.stream).data.push (data);

    // Check if stream is ready to be processed
    if (_streams.at (hdr.stream).entity_header.entity_type ==
        ISMRMRD::ISMRMRD_MRACQUISITION)
    {
      if (_streams.at (hdr.stream).data.size() ==
            _xml_hdr.encoding[0].encodedSpace.matrixSize.y)
      {
        _streams.at (hdr.stream).completed = true;
      }
    }

    return _streams.at (hdr.stream).completed;
  }

  /*****************************************************************************
   ****************************************************************************/
  void icpInputManager::addXmlHeader (std::vector<unsigned char> data)
  {
    try
    {
      std::string xml (data.begin(), data.end());
      ISMRMRD::deserialize (xml.c_str(), _xml_hdr);
    }
    catch (std::runtime_error& e)
    {
      std::cout << "Unable to deserialize XML header: " << e.what() << std::endl;
      throw;
    }

  }

  /*****************************************************************************
   ****************************************************************************/
  ISMRMRD::IsmrmrdHeader icpInputManager::getXmlHeader()
  {
    return _xml_hdr;
  }

  /*****************************************************************************
   ****************************************************************************/
  std::vector<ISMRMRD::Acquisition<float> > icpInputManager::getAcquisitions
  (
    ISMRMRD::EntityHeader  hdr
  )
  {
    std::vector<ISMRMRD::Acquisition<float> > acqs;
    for (uint32_t ii = 0; ii < _streams.at (hdr.stream).data.size(); ii++)
    {
      ISMRMRD::Acquisition<float> acq;
      acq.deserialize (_streams.at (hdr.stream).data.front());
      acqs.push_back (acq);
    }

    return acqs;
  }

  /*****************************************************************************
   ****************************************************************************/
  void icpInputManager::deleteStream (uint32_t stream)
  {
    _streams.erase (stream);
  }

} /* namespace ICPRECEIVEDDATA */
