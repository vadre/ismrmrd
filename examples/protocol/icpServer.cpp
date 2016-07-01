#include "icpServer.h"
#include <functional>
#include "icpServerCallbacks.h"

using namespace ISMRMRD::ICP;
using namespace std::placeholders;

/*******************************************************************************
 ******************************************************************************/
void Server::processHandshake
(
  HANDSHAKE* msg
)
{
  // verify manifest here
  clientAccepted (msg, true);
}

/*******************************************************************************
 ******************************************************************************/
void Server::processCommand
(
  COMMAND* msg
)
{
  switch (msg->getCommandType())
  {
    case ISMRMRD_COMMAND_CONFIGURATION:

      configure (msg);
      break;

    case ISMRMRD_COMMAND_STOP_FROM_CLIENT:

      std::cout << "Received STOP command\n";
      _client_done = true;
      if (_task_done)
      {
        sendCommand (ISMRMRD_COMMAND_DONE_FROM_SERVER, 0);
      }
      break;

    case ISMRMRD_COMMAND_CLOSE_CONNECTION:

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
void Server::processError
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
void Server::taskDone
(
)
{
  _task_done = true;

  if (_client_done) sendCommand (ISMRMRD_COMMAND_DONE_FROM_SERVER, 0);
}

/*******************************************************************************
 ******************************************************************************/
void Server::configure
(
  COMMAND* msg
)
{
  if (msg->getConfigType() == CONFIGURATION_RECON_SHORT)
  {
    std::cout << "Received config request for image recon short\n";

    _callbacks.emplace_back (new ServerImageRecon<int16_t> (this));
    auto fp = std::bind (&Callback::receive, _callbacks.back(), _1, _2);
    _session->registerHandler ((CB)fp, ISMRMRD_HEADER, _callbacks.back());
    _session->registerHandler ((CB)fp, ISMRMRD_MRACQUISITION, _callbacks.back());
  }
  else if (msg->getConfigType() == CONFIGURATION_RECON_INT)
  {
    std::cout << "Received config request for image recon int\n";

    _callbacks.emplace_back (new ServerImageRecon<int32_t> (this));
    auto fp = std::bind (&Callback::receive, _callbacks.back(), _1, _2);
    _session->registerHandler ((CB)fp, ISMRMRD_HEADER, _callbacks.back());
    _session->registerHandler ((CB)fp, ISMRMRD_MRACQUISITION, _callbacks.back());
  }
  else if (msg->getConfigType() == CONFIGURATION_RECON_FLOAT)
  {
    std::cout << "Received config request for image recon float\n";

    _callbacks.emplace_back (new ServerImageRecon<float> (this));
    auto fp = std::bind (&Callback::receive, _callbacks.back(), _1, _2);
    _session->registerHandler ((CB)fp, ISMRMRD_HEADER, _callbacks.back());
    _session->registerHandler ((CB)fp, ISMRMRD_MRACQUISITION, _callbacks.back());
  }
  else if (msg->getConfigType() == CONFIGURATION_RECON_DOUBLE)
  {
    std::cout << "Received config request for image recon double\n";

    _callbacks.emplace_back (new ServerImageRecon<double> (this));
    auto fp = std::bind (&Callback::receive, _callbacks.back(), _1, _2);
    _session->registerHandler ((CB)fp, ISMRMRD_HEADER, _callbacks.back());
    _session->registerHandler ((CB)fp, ISMRMRD_MRACQUISITION, _callbacks.back());
  }
  else
  {
    std::cout <<"Warning: Requested configuration option not implemented\n";
  } 
}

/*****************************************************************************
 ****************************************************************************/
void Server::sendCommand
(
  CommandType cmd_type,
  uint32_t             cmd_id
)
{
  Command msg;
  msg.setCommandType (cmd_type);
  msg.setCommandId (cmd_id);
  _session->send (&msg);
}

/*****************************************************************************
 ****************************************************************************/
void Server::sendEntity
(
  ENTITY*  ent
)
{
  _session->send (ent);
}

/*****************************************************************************
 ****************************************************************************/
void Server::clientAccepted
(
  Handshake* msg,
  bool                accepted
)
{
  msg->setConnectionStatus ((accepted) ?
    CONNECTION_ACCEPTED : CONNECTION_DENIED_UNKNOWN_USER);

  _session->send (msg);

  if (!accepted)
  {
    sendCommand (ISMRMRD_COMMAND_DONE_FROM_SERVER, 0);
    std::cout << "Client not accepted, server DONE\n";
    _session->shutdown();
  }
}

/*****************************************************************************
 ****************************************************************************/
void Server::sendError
(
  ErrorType type,
  std::string        descr
)
{
  ErrorReport msg;
  msg.setErrorType (type);
  msg.setErrorDescription (descr);
  _session->send (&msg);
}

/*****************************************************************************
 ****************************************************************************/
Server::Server
(
  SESSION  session
)
: _session (std::move (session))
{

  _callbacks.emplace_back (new ServerEntityHandler (this));
  auto fp = std::bind (&Callback::receive, _callbacks.back(), _1, _2);

  _session->registerHandler ((CB) fp, ISMRMRD_HANDSHAKE, _callbacks.back());
  _session->registerHandler ((CB) fp, ISMRMRD_ERROR_REPORT, _callbacks.back());
  _session->registerHandler ((CB) fp, ISMRMRD_COMMAND, _callbacks.back());

  _session->run ();
}

/*******************************************************************************
 ******************************************************************************/
Server::~Server
(
)
{
  for (auto cb : _callbacks) if (cb) delete (cb);
}
