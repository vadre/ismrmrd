#include "icpServer.h"
#include <functional>
#include "icpServerCallbacks.h"

using namespace std::placeholders;
/*******************************************************************************
 ******************************************************************************/
void icpServer::configure
(
  ISMRMRD::CommandType  cmd
)
{
  if (cmd == ISMRMRD::ISMRMRD_COMMAND_CONFIG_IMREC_ONE)
  {
    std::cout << "Received request for configuration option 1\n";

    _imrec = new imageReconOne;

    //using namespace std::placeholders;

    std::unique_ptr<icpServerIsmrmrdHeaderWrapperHandler> headCB
      (new icpServerIsmrmrdHeaderWrapperHandler (this));
    auto fp1 = std::bind (&icpCallback::receive, *headCB, _1, _2, _3);
    _session->registerHandler
      ((ICP_CB) fp1, ISMRMRD::ISMRMRD_HEADER_WRAPPER);

    std::unique_ptr<icpServerAcquisitionHandler> acqCB
      (new icpServerAcquisitionHandler (this));
    auto fp2 = std::bind (&icpCallback::receive, *acqCB, _1, _2, _3);
    _session->registerHandler
      ((ICP_CB) fp2, ISMRMRD::ISMRMRD_MRACQUISITION);
  }
  else
  {
    std::cout <<"Requested configuration option not implemented\n";
  } 

  return;
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
  _session->send (&msg, ISMRMRD::ISMRMRD_COMMAND);

  return;
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

  _session->send (msg, ISMRMRD::ISMRMRD_HANDSHAKE);
    std::cout << __func__ << ": just sent handshake\n";

  if (!accepted)
  {
    std::cout << __func__ << ": sending DONE from server\n";
    sendCommand (ISMRMRD::ISMRMRD_COMMAND_DONE_FROM_SERVER, 0);
    std::cout << __func__ << ": just sent DONE from server\n";
  }

  return;
}

/*****************************************************************************
 ****************************************************************************/
void icpServer::sendError
(
  ISMRMRD::ErrorType type,
  std::string        descr
)
{
  ISMRMRD::ErrorNotification msg;
  msg.setErrorType (type);
  msg.setErrorDescription (descr);
  _session->send (&msg, ISMRMRD::ISMRMRD_ERROR_NOTIFICATION);
  return;
}

/*****************************************************************************
 ****************************************************************************/
ISMRMRD::Entity* icpServer::copyEntity
(
  ISMRMRD::Entity* ent,
  uint32_t         storage
)
{
  if (storage == ISMRMRD::ISMRMRD_SHORT)
  {
    ISMRMRD::Acquisition<int16_t>* tmp = static_cast<ISMRMRD::Acquisition<int16_t>*>(ent);
    ISMRMRD::Acquisition<int16_t>* acq
      (new ISMRMRD::Acquisition<int16_t> (tmp->getNumberOfSamples(),
                                          tmp->getActiveChannels(),
                                          tmp->getTrajectoryDimensions()));
    acq->setHead (tmp->getHead());
    acq->setTraj (tmp->getTraj());
    acq->setData (tmp->getData());

    return acq;
  }

  if (storage == ISMRMRD::ISMRMRD_INT)
  {
    ISMRMRD::Acquisition<int32_t>* tmp = static_cast<ISMRMRD::Acquisition<int32_t>*>(ent);

    ISMRMRD::Acquisition<int32_t>* acq
      (new ISMRMRD::Acquisition<int32_t>  (tmp->getNumberOfSamples(),
                                           tmp->getActiveChannels(),
                                           tmp->getTrajectoryDimensions()));
    acq->setHead (tmp->getHead());
    acq->setTraj (tmp->getTraj());
    acq->setData (tmp->getData());

    return acq;
  }

  if (storage == ISMRMRD::ISMRMRD_FLOAT)
  {
    ISMRMRD::Acquisition<float>* tmp = static_cast<ISMRMRD::Acquisition<float>*>(ent);
    ISMRMRD::Acquisition<float>* acq
      (new ISMRMRD::Acquisition<float> (tmp->getNumberOfSamples(),
                                        tmp->getActiveChannels(),
                                        tmp->getTrajectoryDimensions()));

    acq->setHead (tmp->getHead());
    acq->setTraj (tmp->getTraj());
    acq->setData (tmp->getData());

    return acq;
  }

  if (storage == ISMRMRD::ISMRMRD_DOUBLE)
  {
    ISMRMRD::Acquisition<double>* tmp = static_cast<ISMRMRD::Acquisition<double>*>(ent);
    ISMRMRD::Acquisition<double>* acq
      (new ISMRMRD::Acquisition<double> (tmp->getNumberOfSamples(),
                                         tmp->getActiveChannels(),
                                         tmp->getTrajectoryDimensions()));

    acq->setHead (tmp->getHead());
    acq->setTraj (tmp->getTraj());
    acq->setData (tmp->getData());

    return acq;
  }

  throw std::runtime_error ("MR Acquisition unexpected storage type");
  return NULL;
}

/*******************************************************************************
 ******************************************************************************/
void icpServer::run ()
{

  _session->beginReceiving ();
}

/*******************************************************************************
 ******************************************************************************/
icpServer::icpServer
(
  ICP_SESSION  session,
  uint32_t     id    // debug only
)
: _session     (session),
  _header_received (false),
  _acq_storage_set (false),
  _client_done (false),
  _imrec_done  (false)//,
  //_id          (id)
{

  //using namespace std::placeholders;

  std::unique_ptr<icpServerHandshakeHandler> handCB
    (new icpServerHandshakeHandler (this));
  auto fp1 = std::bind (&icpCallback::receive, *handCB, _1, _2, _3);
  _session->registerHandler ((ICP_CB) fp1, ISMRMRD::ISMRMRD_HANDSHAKE);

  std::unique_ptr<icpServerErrorNotificationHandler> errCB
    (new icpServerErrorNotificationHandler (this));
  auto fp2 = std::bind (&icpCallback::receive, *errCB, _1, _2, _3);
  _session->registerHandler ((ICP_CB) fp2, ISMRMRD::ISMRMRD_ERROR_NOTIFICATION);

  std::unique_ptr<icpServerCommandHandler> commCB
    (new icpServerCommandHandler (this));
  auto fp3 = std::bind (&icpCallback::receive, *commCB, _1, _2, _3);
  _session->registerHandler ((ICP_CB) fp3, ISMRMRD::ISMRMRD_COMMAND);

}

/*******************************************************************************
 ******************************************************************************/
icpServer::~icpServer ()
{
  delete (_imrec);
}
