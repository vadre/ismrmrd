#include <iostream>
#include "icpServerCallbacks.h"

/*******************************************************************************
 ******************************************************************************/
icpServerEntityHandler::icpServerEntityHandler
(
  icpServer* server
)
: _server (server)
{
}
/******************************************************************************/

void icpServerEntityHandler::receive
(
  icpCallback* base,
  ENTITY*      entity,
  uint32_t     version,
  ETYPE        etype,
  STYPE        stype,
  uint32_t     stream
)
{
  icpServerEntityHandler* _this = static_cast<icpServerEntityHandler*>(base);
  switch (etype)
  {
    case ISMRMRD::ISMRMRD_HANDSHAKE:

      _this->_server->processHandshake (static_cast<HANDSHAKE*>(entity));
      break;

    case ISMRMRD::ISMRMRD_COMMAND:
      _this->_server->processCommand (static_cast<COMMAND*>(entity));
      break;

    case ISMRMRD::ISMRMRD_ERROR_REPORT:

      _this->_server->processError (static_cast<ERRREPORT*>(entity));
      break;

    default:

      std::cout << "Warning! Entity Handler received unexpected entity\n";
      break;
  }
}

/*******************************************************************************
 ******************************************************************************/
