#include <iostream>
#include "icpClientCallbacks.h"

/*******************************************************************************
 ******************************************************************************/
icpClientEntityHandler::icpClientEntityHandler
(
  icpClient* client
)
: _client (client)
{
}
/******************************************************************************/

void icpClientEntityHandler::receive
(
  icpCallback*          base,
  ISMRMRD::Entity*      entity,
  uint32_t              version,
  ISMRMRD::EntityType   etype,
  ISMRMRD::StorageType  stype,
  uint32_t              stream
)
{
  icpClientEntityHandler* _this = static_cast<icpClientEntityHandler*>(base);
  switch (etype)
  {
    case ISMRMRD::ISMRMRD_HANDSHAKE:

      _this->_client->processHandshake (static_cast<HANDSHAKE*>(entity));
      break;

    case ISMRMRD::ISMRMRD_COMMAND:
      _this->_client->processCommand (static_cast<COMMAND*>(entity));
      break;

    case ISMRMRD::ISMRMRD_ERROR_REPORT:

      _this->_client->processError (static_cast<ERRREPORT*>(entity));
      break;

    default:

      std::cout << "Warning! Entity Handler received unexpected entity\n";
      break;
  }
}

/*******************************************************************************
 ******************************************************************************/
icpClientImageProcessor::icpClientImageProcessor
(
  icpClient* client,
  std::string fname,
  std::string dname
)
: _client (client),
  _dset (fname.c_str(), dname.c_str())
{
}
/******************************************************************************/

void icpClientImageProcessor::receive
(
  icpCallback*          base,
  ISMRMRD::Entity*      entity,
  uint32_t              version,
  ISMRMRD::EntityType   etype,
  ISMRMRD::StorageType  stype,
  uint32_t              stream
)
{
  icpClientImageProcessor* _this =
    static_cast<icpClientImageProcessor*>(base);

  if (etype == ISMRMRD::ISMRMRD_HEADER)
  {
    std::cout << "Writing Ismrmrd Header to file\n";
    ISMRMRD::IsmrmrdHeaderWrapper* wrp =
      static_cast<ISMRMRD::IsmrmrdHeaderWrapper*>(entity);
    _this->_dset.writeHeader (wrp->getString ());
  }
  else if (etype == ISMRMRD::ISMRMRD_IMAGE)
  {
    std::cout << "Writing image to file\n";

    if (stype == ISMRMRD::ISMRMRD_SHORT)
    {
      _this->writeImage<int16_t> (entity);
    }
    else if (stype == ISMRMRD::ISMRMRD_USHORT)
    {
      _this->writeImage<uint16_t> (entity);
    }
    else if (stype == ISMRMRD::ISMRMRD_INT)
    {
      _this->writeImage<int32_t> (entity);
    }
    else if (stype == ISMRMRD::ISMRMRD_UINT)
    {
      _this->writeImage<uint32_t> (entity);
    }
    else if (stype == ISMRMRD::ISMRMRD_FLOAT)
    {
      _this->writeImage<float> (entity);
    }
    else if (stype == ISMRMRD::ISMRMRD_DOUBLE)
    {
      _this->writeImage<double> (entity);
    }
    else if (stype == ISMRMRD::ISMRMRD_CXFLOAT)
    {
      _this->writeImage<std::complex<float>> (entity);
    }
    else if (stype == ISMRMRD::ISMRMRD_CXDOUBLE)
    {
      _this->writeImage<std::complex<double>> (entity);
    }
    else
    {
      std::cout << "Error! Received unexpected image storage type\n";
    }

    std::cout << "\nImage processing done" << std::endl;

    _this->_client->taskDone();
  }
  else
  {
    std::cout << "Error! Received unexpected entity type\n";
  }
}

/*******************************************************************************
 ******************************************************************************/
