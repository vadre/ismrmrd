#include "icpServer.h"
#include <functional>

using namespace std::placeholders;
/*******************************************************************************
 ******************************************************************************/
void icp::server handleCommand
(
  ISMRMRD::Command  msg,
)
{
  switch (msg.getCommandType())
  {
    case ISMRMRD::ISMRMRD_COMMAND_CONFIG_IMREC_ONE:

      _imrec = new imageReconstructionOne;

      auto fp1 = std::bind (&icpServerTest::handleIsmrmrdHeader, *this, _1, _2);
      _msg_get.registerHandler ((CB_XMLHEAD) fp1, ISMRMRD::ISMRMRD_HEADER);

      auto fp2 = std::bind (&icpServerTest::handleAcquisition, *this, _1, _2);
      _msg_get.registerHandler ((CB_MR_ACQ) fp2, ISMRMRD::ISMRMRD_MRACQUISITION);

      break;

    case ISMRMRD::ISMRMRD_COMMAND_CONFIG_IMREC_TWO:

      // Another preset option - hasn't been defined yet
      break;

    case ISMRMRD::ISMRMRD_COMMAND_STOP_FROM_CLIENT:

      std::cout << __func__ << ": Received STOP  from client\n";
      _client_done = true;
      if (_imrec_done)
      {
        sendCommand (ISMRMRD::ISMRMRD_COMMAND_DONE_FROM_SERVER, 0);
      }
      break;

    case ISMRMRD::ISMRMRD_COMMAND_CLOSE_CONNECTION:

      std::cout << __func__ << ": Received CLOSE from client\n";
      _session->shutdown();
      break;

    default:
      break;
  }
  return;
}

/*******************************************************************************
 ******************************************************************************/
void icp::server handleErrorNotification
(
  ISMRMRD::ErrorNotification msg,
)
{
  std::cout << __func__ <<":\nError Type: " << msg.getErrorType()
            << ", Error Command: " << msg.getErrorCommandType()
            << ", Error Command ID: " << msg.getErrorCommandId()
            << ", Error Entity: " << msg.getErrorEntityType()
            << ",\nError Description: " << msg.getErrorDescription() << "\n";
  // React to error...
  return;
}

/*******************************************************************************
 ******************************************************************************/
void icp::server handleIsmrmrdHeader
(
  ISMRMRD::IsmrmrdHeader  msg,
)
{
  std::cout << __func__ << ": Received IsmrmrdHeader\n";
  _imrec->addIsmrmrdHeader (msg);
  _session->forward (ISMRMRD::ISMRMRD_HEADER hdr); // Just for demo

  return;
}

/*****************************************************************************
 ****************************************************************************/
void icp::server sendCommand
(
  ISMRMRD::CommandType cmd_type,
  uint32_t             cmd_id
)
{
  ISMRMRD::Command msg;
  msg.setCommandType (cmd_type);
  msg.setCommandId (cmd_id);
  _session->forward (ISMRMRD::ISMRMRD_COMMAND, &msg);

  return;
}

/*****************************************************************************
 ****************************************************************************/
void icp::server clientAccepted
(
  ISMRMRD::Handshake  msg,
  bool                accepted
)
{
  msg.setConnectionStatus ((accepted) ?
    ISMRMRD::CONNECTION_ACCEPTED : ISMRMRD::CONNECTION_DENIED_UNKNOWN_USER);

  _session->forward (ISMRMRD::ISMRMRD_HANDSHAKE, &msg);

  if (!accepted)
  {
    sendCommand (ISMRMRD::ISMRMRD_COMMAND_DONE_FROM_SERVER, 0);
  }

  return;
}

/*******************************************************************************
 ******************************************************************************/
void icp::server handleHandshake
(
  ISMRMRD::Handshake  msg,
)
{
  // Could check if the client name is matching a list of expected clients
  std::cout << __func__ << ": client <"<< msg.getClientName() << "> accepted\n";
  clientAccepted (msg, true);

  return;
}

/*****************************************************************************
 ****************************************************************************/
void icp::server sendError
(
  ISMRMRD::ErrorType type,
  std::string        descr
)
{
  ISMRMRD::ErrorNotification msg;
  msg.setErrorType (type);
  msg.setErrorDescription (descr);
  _session->forward (ISMRMRD::ISMRMRD_ERROR_NOTIFICATION, &msg);
  return;
}

/*******************************************************************************
 This routine could be used to receive 
 ******************************************************************************/
void icp::server handleAcquisition
(
  ISMRMRD::Entity* ent,
  uint32_t         storage
)
{
  std::cout << __func__ << ": Received acquisition\n";
  _imrec->addAcquisition (ent, storage);
  if (_imrec->isImageDone())
  {
    _session->forward (_imrec->getImageEntityPointer());
    _imrec_done = true;
    if (_client_done)
    {
      sendCommand (ISMRMRD::ISMRMRD_COMMAND_DONE_FROM_SERVER, 0);
    }
  }
}

/*******************************************************************************
 ******************************************************************************/
void icpServer::run ()
{
  _session->beginReceiving();
}

/*******************************************************************************
 ******************************************************************************/
icpServer::icpServer
(
  ICP_SESSION  session,
  uint32_t     id    // debug only
)
: _session     (session),
  _client_done (false),
  _imrec_done  (false),
  _id          (id)
{
  auto fp1 = std::bind (&icpServerTest::handleHandshake, *this, _1, _2);
  _session->registerHandler ((CB_HNDSHK) fp, ISMRMRD::ISMRMRD_HANDSHAKE);

  auto fp2 = std::bind (&icpServerTest::handleErrorNotification, *this, _1, _2);
  _session->registerHandler ((CB_ERRNOTE) fp, ISMRMRD::ISMRMRD_ERROR_NOTIFICATION);

  auto fp3 = std::bind (&icpServerTest::handleCommand, *this, _1, _2);
  _session->registerHandler ((CB_COMMAND) fp, ISMRMRD::ISMRMRD_COMMAND);

}

/*******************************************************************************
 ******************************************************************************/
icpServer::~icpServer ()
{
  delete (_imrec);
}
