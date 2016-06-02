#include "icpServer.h"
#include <functional>

using namespace std::placeholders;
/*******************************************************************************
 ******************************************************************************/
void icpServer::handleCommand
(
  ISMRMRD::Command  msg,
  icpUserAppBase*   base
)
{
  icpServer* inst = static_cast<icpServer*>(base);

  auto fp1 = std::bind (&icpServer::handleIsmrmrdHeader, *this, _1, _2);
  auto fp2 = std::bind (&icpServer::handleAcquisition, *this, _1, _2, _3);

  switch (msg.getCommandType())
  {
    case ISMRMRD::ISMRMRD_COMMAND_CONFIG_IMREC_ONE:

      std::cout << __func__ << ": Received config1\n";
      inst->_imrec = new imageReconOne;

      inst->_session->registerHandler ((CB_XMLHEAD) fp1,
                                       ISMRMRD::ISMRMRD_HEADER);
      inst->_session->registerHandler ((CB_MR_ACQ) fp2,
                                       ISMRMRD::ISMRMRD_MRACQUISITION);
      break;

    case ISMRMRD::ISMRMRD_COMMAND_CONFIG_IMREC_TWO:

      // Another preset option - hasn't been defined yet
      std::cout << __func__ << ": Received config2\n";
      break;

    case ISMRMRD::ISMRMRD_COMMAND_STOP_FROM_CLIENT:

      std::cout << __func__ << ": Received STOP\n";
      inst->_client_done = true;
      if (inst->_imrec_done)
      {
        inst->sendCommand (ISMRMRD::ISMRMRD_COMMAND_DONE_FROM_SERVER, 0, inst);
      }
      break;

    case ISMRMRD::ISMRMRD_COMMAND_CLOSE_CONNECTION:

      std::cout << __func__ << ": Received CLOSE\n";
      inst->_session->shutdown();
      break;

    default:
      std::cout << __func__ << ": Received unexpected command\n";
      break;
  }
  return;
}

/*******************************************************************************
 ******************************************************************************/
void icpServer::handleErrorNotification
(
  ISMRMRD::ErrorNotification msg,
  icpUserAppBase*   base
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
void icpServer::handleIsmrmrdHeader
(
  ISMRMRD::IsmrmrdHeader  msg,
  icpUserAppBase*   base
)
{
  icpServer* inst = static_cast<icpServer*>(base);

  inst->_hdr = msg;
  inst->_header_received = true;
  ISMRMRD::IsmrmrdHeaderWrapper wrp;
  wrp.setHeader (msg);
  inst->_session->forwardMessage (ISMRMRD::ISMRMRD_HEADER_WRAPPER, &wrp); // For demo

  return;
}

/*****************************************************************************
 ****************************************************************************/
void icpServer::sendCommand
(
  ISMRMRD::CommandType cmd_type,
  uint32_t             cmd_id,
  icpServer*           inst
)
{
  ISMRMRD::Command msg;
  msg.setCommandType (cmd_type);
  msg.setCommandId (cmd_id);
  inst->_session->forwardMessage (ISMRMRD::ISMRMRD_COMMAND, &msg);

  return;
}

/*****************************************************************************
 ****************************************************************************/
void icpServer::clientAccepted
(
  ISMRMRD::Handshake  msg,
  bool                accepted,
  icpServer*          inst
)
{
  msg.setConnectionStatus ((accepted) ?
    ISMRMRD::CONNECTION_ACCEPTED : ISMRMRD::CONNECTION_DENIED_UNKNOWN_USER);

  inst->_session->forwardMessage (ISMRMRD::ISMRMRD_HANDSHAKE, &msg);

  if (!accepted)
  {
    inst->sendCommand (ISMRMRD::ISMRMRD_COMMAND_DONE_FROM_SERVER, 0, inst);
  }

  return;
}

/*******************************************************************************
 ******************************************************************************/
void icpServer::handleHandshake
(
  ISMRMRD::Handshake  msg,
  icpUserAppBase*   base
)
{
  icpServer* inst = static_cast<icpServer*>(base);

  // Could check if the client name is matching a list of expected clients
  std::cout << __func__ << ": client <"<< msg.getClientName() << "> accepted\n";
  inst->clientAccepted (msg, true, inst);

  return;
}

/*****************************************************************************
 ****************************************************************************/
void icpServer::sendError
(
  ISMRMRD::ErrorType type,
  std::string        descr,
  icpServer*         inst
)
{
  ISMRMRD::ErrorNotification msg;
  msg.setErrorType (type);
  msg.setErrorDescription (descr);
  _session->forwardMessage (ISMRMRD::ISMRMRD_ERROR_NOTIFICATION, &msg);
  return;
}

/*****************************************************************************
 ****************************************************************************/
ISMRMRD::Entity* icpServer::copyEntity
(
  ISMRMRD::Entity* ent,
  uint32_t         storage,
  icpServer*         inst
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
 This routine could be used to receive 
 ******************************************************************************/
void icpServer::handleAcquisition
(
  ISMRMRD::Entity* ent,
  uint32_t         storage,
  icpUserAppBase*   base
)
{
  icpServer* inst = static_cast<icpServer*>(base);

  if (!inst->_acq_storage_set)
  {
    inst->_acq_storage     = (ISMRMRD::StorageType)storage;
    inst->_acq_storage_set = true;
  }
  else if (inst->_acq_storage != storage)
  {
    throw std::runtime_error ("Inconsistent acquisition storage type");
  }

  inst->_acqs.push_back (inst->copyEntity (ent, storage, inst));

  if (inst->_acqs.size() == inst->_hdr.encoding[0].encodedSpace.matrixSize.y)
  {
    ISMRMRD::Entity* img_ent =
      inst->_imrec->runReconstruction (inst->_acqs,
                                       (ISMRMRD::StorageType)storage,
                                       inst->_hdr);

    inst->_session->forwardMessage (ISMRMRD::ISMRMRD_IMAGE, img_ent,
                                    (ISMRMRD::StorageType)storage);
    inst->_imrec_done = true;
    if (inst->_client_done)
    {
      inst->sendCommand (ISMRMRD::ISMRMRD_COMMAND_DONE_FROM_SERVER, 0, inst);
    }
  }
}

/*******************************************************************************
 ******************************************************************************/
void icpServer::run ()
{

  _session->beginReceiving (this);
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
  _imrec_done  (false),
  _id          (id)
{

  std::ios_base::sync_with_stdio(false);
  std::cin.tie(static_cast<std::ostream*>(0));
  std::cerr.tie(static_cast<std::ostream*>(0));

  auto fp1 = std::bind (&icpServer::handleHandshake, *this, _1, _2);
  _session->registerHandler ((CB_HANDSHK) fp1, ISMRMRD::ISMRMRD_HANDSHAKE);

  auto fp2 = std::bind (&icpServer::handleErrorNotification, *this, _1, _2);
  _session->registerHandler ((CB_ERRNOTE) fp2, ISMRMRD::ISMRMRD_ERROR_NOTIFICATION);

  auto fp3 = std::bind (&icpServer::handleCommand, *this, _1, _2);
  _session->registerHandler ((CB_COMMAND) fp3, ISMRMRD::ISMRMRD_COMMAND);

}

/*******************************************************************************
 ******************************************************************************/
icpServer::~icpServer ()
{
  delete (_imrec);
}
