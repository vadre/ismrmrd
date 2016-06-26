#include "icpServer.h"
#include <functional>
#include "icpServerCallbacks.h"

using namespace std::placeholders;

/*******************************************************************************
 ******************************************************************************/
void icpServer::processHandshake
(
  HANDSHAKE* msg
)
{
  // verify manifest here
  clientAccepted (msg, true);
}

/*******************************************************************************
 ******************************************************************************/
void icpServer::processCommand
(
  COMMAND* msg
)
{
  switch (msg->getCommandType())
  {
    case ISMRMRD::ISMRMRD_COMMAND_CONFIGURATION:

      configure (msg);
      break;

    case ISMRMRD::ISMRMRD_COMMAND_STOP_FROM_CLIENT:

      std::cout << "Received STOP command\n";
      _client_done = true;
      if (_task_done)
      {
        sendCommand (ISMRMRD::ISMRMRD_COMMAND_DONE_FROM_SERVER, 0);
      }
      break;

    case ISMRMRD::ISMRMRD_COMMAND_CLOSE_CONNECTION:

      std::cout << "Received CLOSE command\n";
      _session->shutdown();
      break;

    default:

      std::cout << "Received unexpected command\n";
      break;
  }
}

/*******************************************************************************
 ******************************************************************************/
void icpServer::processError
(
  ERRREPORT* msg
)
{
  std::cout << "Error Type: " << msg->getErrorType()
            << ", Error Command: " << msg->getErrorCommandType()
            << ", Error Command ID: " << msg->getErrorCommandId()
            << ", Error Entity: " << msg->getErrorEntityType()
            << ",\nError Description: " << msg->getErrorDescription() << "\n";
}

/*******************************************************************************
 ******************************************************************************/
void icpServer::taskDone
(
)
{
  _task_done = true;
  if (_client_done)
  {
    sendCommand (ISMRMRD::ISMRMRD_COMMAND_DONE_FROM_SERVER, 0);
  }
}

/*******************************************************************************
 ******************************************************************************/
void icpServer::configure
(
  COMMAND* msg
)
{
  if (msg->getConfigType() == ISMRMRD::CONFIGURATION_RECON_SHORT)
  {
    std::cout << "Received config request for image recon short\n";

    _recCB = new icpServerImageRecon<int16_t> (this);
    auto fp = std::bind (&icpCallback::receive, _recCB, _1, _2, _3, _4, _5, _6);
    _session->registerHandler ((ICP_CB) fp, ISMRMRD::ISMRMRD_HEADER, _recCB);
    _session->registerHandler ((ICP_CB) fp,
      ISMRMRD::ISMRMRD_MRACQUISITION, _recCB);
  }
  else if (msg->getConfigType() == ISMRMRD::CONFIGURATION_RECON_INT)
  {
    std::cout << "Received config request for image recon int\n";

    _recCB = new icpServerImageRecon<int32_t> (this);
    auto fp = std::bind (&icpCallback::receive, _recCB, _1, _2, _3, _4, _5, _6);
    _session->registerHandler ((ICP_CB) fp, ISMRMRD::ISMRMRD_HEADER, _recCB);
    _session->registerHandler ((ICP_CB) fp,
      ISMRMRD::ISMRMRD_MRACQUISITION, _recCB);
  }
  else if (msg->getConfigType() == ISMRMRD::CONFIGURATION_RECON_FLOAT)
  {
    std::cout << "Received config request for image recon float\n";

    _recCB = new icpServerImageRecon<float> (this);
    auto fp = std::bind (&icpCallback::receive, _recCB, _1, _2, _3, _4, _5, _6);
    _session->registerHandler ((ICP_CB) fp, ISMRMRD::ISMRMRD_HEADER, _recCB);
    _session->registerHandler ((ICP_CB) fp,
      ISMRMRD::ISMRMRD_MRACQUISITION, _recCB);
  }
  else if (msg->getConfigType() == ISMRMRD::CONFIGURATION_RECON_DOUBLE)
  {
    std::cout << "Received config request for image recon double\n";

    _recCB = new icpServerImageRecon<double> (this);
    auto fp = std::bind (&icpCallback::receive, _recCB, _1, _2, _3, _4, _5, _6);
    _session->registerHandler ((ICP_CB) fp, ISMRMRD::ISMRMRD_HEADER, _recCB);
    _session->registerHandler ((ICP_CB) fp,
      ISMRMRD::ISMRMRD_MRACQUISITION, _recCB);
  }
  else
  {
    std::cout <<"Warning: Requested configuration option not implemented\n";
  } 
}

/*****************************************************************************
 ****************************************************************************/
void icpServer::sendCommand
(
  ISMRMRD::CommandType cmd_type,
  uint32_t             cmd_id
)
{
  ISMRMRD::Command msg;
  msg.setCommandType (cmd_type);
  msg.setCommandId (cmd_id);
  _session->send (&msg, ISMRMRD_VERSION_MAJOR, ISMRMRD::ISMRMRD_COMMAND,
        ISMRMRD::ISMRMRD_STORAGE_NONE, ISMRMRD::ISMRMRD_STREAM_COMMAND);
}

/*****************************************************************************
 ****************************************************************************/
void icpServer::sendImage
(
  ENTITY*  ent,
  uint32_t version,
  STYPE    stype,
  uint32_t stream
)
{
  _session->send (ent, version, ISMRMRD::ISMRMRD_IMAGE, stype, stream);
}

/*****************************************************************************
 ****************************************************************************/
void icpServer::clientAccepted
(
  ISMRMRD::Handshake* msg,
  bool                accepted
)
{
  msg->setConnectionStatus ((accepted) ?
    ISMRMRD::CONNECTION_ACCEPTED : ISMRMRD::CONNECTION_DENIED_UNKNOWN_USER);

  _session->send (msg, ISMRMRD_VERSION_MAJOR, ISMRMRD::ISMRMRD_HANDSHAKE,
       ISMRMRD::ISMRMRD_STORAGE_NONE, ISMRMRD::ISMRMRD_STREAM_HANDSHAKE);

  if (!accepted)
  {
    sendCommand (ISMRMRD::ISMRMRD_COMMAND_DONE_FROM_SERVER, 0);
    std::cout << "Client not accepted, server DONE\n";
    _session->shutdown();
  }
}

/*****************************************************************************
 ****************************************************************************/
void icpServer::sendError
(
  ISMRMRD::ErrorType type,
  std::string        descr
)
{
  ISMRMRD::ErrorReport msg;
  msg.setErrorType (type);
  msg.setErrorDescription (descr);
  _session->send (&msg, ISMRMRD_VERSION_MAJOR, ISMRMRD::ISMRMRD_ERROR_REPORT,
      ISMRMRD::ISMRMRD_STORAGE_NONE, ISMRMRD::ISMRMRD_STREAM_ERRREPORT);
}

/*****************************************************************************
 ****************************************************************************/
void icpServer::sendHeader
(
  ISMRMRD::IsmrmrdHeaderWrapper* wrp,
  ETYPE                          etype
)
{
  _session->send (wrp, wrp->getVersion(), etype,
                  wrp->getStorageType(), wrp->getStream());
}

/*****************************************************************************
 ****************************************************************************/
icpServer::icpServer
(
  ICP_SESSION  session
)
: _session (std::move (session))
{

  _entCB = new icpServerEntityHandler (this);
  auto fp = std::bind (&icpCallback::receive, _entCB, _1, _2, _3, _4, _5, _6);

  _session->registerHandler ((ICP_CB) fp, ISMRMRD::ISMRMRD_HANDSHAKE, _entCB);
  _session->registerHandler ((ICP_CB) fp, ISMRMRD::ISMRMRD_ERROR_REPORT, _entCB);
  _session->registerHandler ((ICP_CB) fp, ISMRMRD::ISMRMRD_COMMAND, _entCB);

  _session->run ();
}

/*******************************************************************************
 ******************************************************************************/
icpServer::~icpServer
(
)
{
  if (_entCB)
  {
    delete (_entCB);
  }

  if (_recCB)
  {
    delete (_recCB);
  }
}
