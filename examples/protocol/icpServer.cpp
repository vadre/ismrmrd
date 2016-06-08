#include "icpServer.h"
#include <functional>

using namespace std::placeholders;
/*******************************************************************************
 ******************************************************************************/
void icpServer::handleCommand
(
  COMMAND  msg
)
{
  switch (msg.getCommandType())
  {
    case ISMRMRD::ISMRMRD_COMMAND_CONFIG_IMREC_ONE:

      std::cout << __func__ << ": Received config1\n";
      _imrec = new imageReconOne;
      _session->subscribe (ISMRMRD::ISMRMRD_HEADER, true);
      _session->subscribe (ISMRMRD::ISMRMRD_MRACQUISITION, true);
      _session->subscribe (ISMRMRD::ISMRMRD_ERROR_NOTIFICATION, true);
      break;

    case ISMRMRD::ISMRMRD_COMMAND_CONFIG_IMREC_TWO:

      // Another preset option - hasn't been defined yet
      std::cout << __func__ << ": Received config2 - not supported\n";
      break;

    case ISMRMRD::ISMRMRD_COMMAND_STOP_FROM_CLIENT:

      std::cout << __func__ << ": Received STOP\n";
      _client_done = true;
      if (_imrec_done)
      {
        sendCommand (ISMRMRD::ISMRMRD_COMMAND_DONE_FROM_SERVER, 0);
      }
      break;

    case ISMRMRD::ISMRMRD_COMMAND_CLOSE_CONNECTION:

      std::cout << __func__ << ": Received CLOSE\n";
      _session->shutdown();
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
  ISMRMRD::ErrorNotification msg
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
  XMLHEAD  msg
)
{
  std::cout << __func__ << ": received IsmrmrdHeader\n";
  _hdr = msg;
  _header_received = true;
  XMLWRAP wrp;
  wrp.setHeader (msg);
  _session->send (ISMRMRD::ISMRMRD_HEADER_WRAPPER, &wrp); // For demo

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
  COMMAND msg;
  msg.setCommandType (cmd_type);
  msg.setCommandId (cmd_id);
  _session->send (ISMRMRD::ISMRMRD_COMMAND, &msg);

  return;
}

/*****************************************************************************
 ****************************************************************************/
void icpServer::clientAccepted
(
  HANDSHAKE  msg,
  bool       accepted
)
{
  std::cout << __func__ << ": 1" << "\n";
  msg.setConnectionStatus ((accepted) ?
    ISMRMRD::CONNECTION_ACCEPTED : ISMRMRD::CONNECTION_DENIED_UNKNOWN_USER);

  std::cout << __func__ << ": 2" << "\n";
  _session->send (ISMRMRD::ISMRMRD_HANDSHAKE, &msg);

  std::cout << __func__ << ": 3" << "\n";
  if (!accepted)
  {
  std::cout << __func__ << ": 4" << "\n";
    sendCommand (ISMRMRD::ISMRMRD_COMMAND_DONE_FROM_SERVER, 0);
  std::cout << __func__ << ": 5" << "\n";
  }

  std::cout << __func__ << ": 6" << "\n";
  return;
}

/*******************************************************************************
 ******************************************************************************/
void icpServer::handleHandshake
(
  HANDSHAKE  msg
)
{
  // Could check if the client name is matching a list of expected clients
  std::cout << __func__ << ": client_name = <"<< msg.getClientName() << ">\n";

  bool status = true;
  
  std::cout << __func__ << ": 1" << "\n";
  status = msg.verifyManifestEntry (ISMRMRD::ISMRMRD_HANDSHAKE,
                                    ISMRMRD::ISMRMRD_CHAR, "ISHANDSHAKE");
  std::cout << __func__ << ": 2" << "\n";
  if (!status) clientAccepted (msg, false);
  status = msg.verifyManifestEntry (ISMRMRD::ISMRMRD_COMMAND,
                                    ISMRMRD::ISMRMRD_CHAR, "ISCOMMAND");
  std::cout << __func__ << ": 3" << "\n";
  if (!status) clientAccepted (msg, false);
  status = msg.verifyManifestEntry (ISMRMRD::ISMRMRD_MRACQUISITION,
                                    ISMRMRD::ISMRMRD_SHORT, "ISACQSHORT");
  std::cout << __func__ << ": 4" << "\n";
  if (!status) clientAccepted (msg, false);
  std::cout << __func__ << ": 4.1" << "\n";
  status = msg.verifyManifestEntry (ISMRMRD::ISMRMRD_MRACQUISITION,
                                    ISMRMRD::ISMRMRD_INT, "ISACQINT");
  std::cout << __func__ << ": 5" << "\n";
  if (!status) clientAccepted (msg, false);
  status = msg.verifyManifestEntry (ISMRMRD::ISMRMRD_MRACQUISITION,
                                    ISMRMRD::ISMRMRD_FLOAT, "ISACQFLT");
  std::cout << __func__ << ": 6" << "\n";
  if (!status) clientAccepted (msg, false);
  status = msg.verifyManifestEntry (ISMRMRD::ISMRMRD_MRACQUISITION,
                                    ISMRMRD::ISMRMRD_DOUBLE, "ISACQDBL");
  std::cout << __func__ << ": 7" << "\n";
  if (!status) clientAccepted (msg, false);
  status = msg.verifyManifestEntry (ISMRMRD::ISMRMRD_IMAGE,
                                    ISMRMRD::ISMRMRD_USHORT, "ISIMUSHORT");
  std::cout << __func__ << ": 8" << "\n";
  if (!status) clientAccepted (msg, false);
  status = msg.verifyManifestEntry (ISMRMRD::ISMRMRD_IMAGE,
                                    ISMRMRD::ISMRMRD_SHORT, "ISIMSHORT");
  std::cout << __func__ << ": 9" << "\n";
  if (!status) clientAccepted (msg, false);
  status = msg.verifyManifestEntry (ISMRMRD::ISMRMRD_IMAGE,
                                    ISMRMRD::ISMRMRD_UINT, "ISIMUINT");
  std::cout << __func__ << ": 10" << "\n";
  if (!status) clientAccepted (msg, false);
  status = msg.verifyManifestEntry (ISMRMRD::ISMRMRD_IMAGE,
                                    ISMRMRD::ISMRMRD_INT, "ISIMINT");
  std::cout << __func__ << ": 11" << "\n";
  if (!status) clientAccepted (msg, false);
  status = msg.verifyManifestEntry (ISMRMRD::ISMRMRD_IMAGE,
                                    ISMRMRD::ISMRMRD_FLOAT, "ISIMFLT");
  std::cout << __func__ << ": 12" << "\n";
  if (!status) clientAccepted (msg, false);
  status = msg.verifyManifestEntry (ISMRMRD::ISMRMRD_IMAGE,
                                    ISMRMRD::ISMRMRD_DOUBLE, "ISIMDBL");
  std::cout << __func__ << ": 13" << "\n";
  if (!status) clientAccepted (msg, false);
  status = msg.verifyManifestEntry (ISMRMRD::ISMRMRD_IMAGE,
                                    ISMRMRD::ISMRMRD_CXFLOAT, "ISIMCXFLT");
  std::cout << __func__ << ": 14" << "\n";
  if (!status) clientAccepted (msg, false);
  status = msg.verifyManifestEntry (ISMRMRD::ISMRMRD_IMAGE,
                                    ISMRMRD::ISMRMRD_CXDOUBLE, "ISIMCXDBL");
  std::cout << __func__ << ": 15" << "\n";
  if (!status) clientAccepted (msg, false);
  status = msg.verifyManifestEntry (ISMRMRD::ISMRMRD_HEADER,
                                    ISMRMRD::ISMRMRD_CHAR, "ISHEADER");
  std::cout << __func__ << ": 16" << "\n";
  if (!status) clientAccepted (msg, false);
  status = msg.verifyManifestEntry (ISMRMRD::ISMRMRD_HEADER_WRAPPER,
                                    ISMRMRD::ISMRMRD_CHAR, "ISHEADWRAP");
  std::cout << __func__ << ": 17" << "\n";
  if (!status) clientAccepted (msg, false);
  status = msg.verifyManifestEntry (ISMRMRD::ISMRMRD_ERROR_NOTIFICATION,
                                    ISMRMRD::ISMRMRD_CHAR, "ISERRNOTE");
  std::cout << __func__ << ": 18" << "\n";


  /*if (msg.verifyManifestEntry (ISMRMRD::ISMRMRD_HANDSHAKE,
                               ISMRMRD::ISMRMRD_CHAR, "ISHANDSHAKE") &&
      msg.verifyManifestEntry (ISMRMRD::ISMRMRD_COMMAND,
                               ISMRMRD::ISMRMRD_CHAR, "ISCOMMAND") &&
      msg.verifyManifestEntry (ISMRMRD::ISMRMRD_MRACQUISITION,
                               ISMRMRD::ISMRMRD_SHORT, "ISACQSHORT") &&
      msg.verifyManifestEntry (ISMRMRD::ISMRMRD_MRACQUISITION,
                               ISMRMRD::ISMRMRD_INT, "ISACQINT") &&
      msg.verifyManifestEntry (ISMRMRD::ISMRMRD_MRACQUISITION,
                               ISMRMRD::ISMRMRD_FLOAT, "ISACQFLT") &&
      msg.verifyManifestEntry (ISMRMRD::ISMRMRD_MRACQUISITION,
                               ISMRMRD::ISMRMRD_DOUBLE, "ISACQDBL") &&
      msg.verifyManifestEntry (ISMRMRD::ISMRMRD_IMAGE,
                               ISMRMRD::ISMRMRD_USHORT, "ISIMUSHORT") &&
      msg.verifyManifestEntry (ISMRMRD::ISMRMRD_IMAGE,
                               ISMRMRD::ISMRMRD_SHORT, "ISIMSHORT") &&
      msg.verifyManifestEntry (ISMRMRD::ISMRMRD_IMAGE,
                               ISMRMRD::ISMRMRD_UINT, "ISIMUINT") &&
      msg.verifyManifestEntry (ISMRMRD::ISMRMRD_IMAGE,
                               ISMRMRD::ISMRMRD_INT, "ISIMINT") &&
      msg.verifyManifestEntry (ISMRMRD::ISMRMRD_IMAGE,
                               ISMRMRD::ISMRMRD_FLOAT, "ISIMFLT") &&
      msg.verifyManifestEntry (ISMRMRD::ISMRMRD_IMAGE,
                               ISMRMRD::ISMRMRD_DOUBLE, "ISIMDBL") &&
      msg.verifyManifestEntry (ISMRMRD::ISMRMRD_IMAGE,
                               ISMRMRD::ISMRMRD_CXFLOAT, "ISIMCXFLT") &&
      msg.verifyManifestEntry (ISMRMRD::ISMRMRD_IMAGE,
                               ISMRMRD::ISMRMRD_CXDOUBLE, "ISIMCXDBL") &&
      msg.verifyManifestEntry (ISMRMRD::ISMRMRD_HEADER,
                               ISMRMRD::ISMRMRD_CHAR, "ISHEADER") &&
      msg.verifyManifestEntry (ISMRMRD::ISMRMRD_HEADER_WRAPPER,
                               ISMRMRD::ISMRMRD_CHAR, "ISHEADWRAP") &&
      msg.verifyManifestEntry (ISMRMRD::ISMRMRD_ERROR_NOTIFICATION,
                               ISMRMRD::ISMRMRD_CHAR, "ISERRNOTE"))
  {
    status = true;
  } 
  */

  clientAccepted (msg, status);

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
  _session->send (ISMRMRD::ISMRMRD_ERROR_NOTIFICATION, &msg);
  return;
}

/*****************************************************************************
 ****************************************************************************/
ENTITY* icpServer::copyEntity
(
  ENTITY* ent,
  STYPE   stype
)
{
  if (stype == ISMRMRD::ISMRMRD_SHORT)
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

  if (stype == ISMRMRD::ISMRMRD_INT)
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

  if (stype == ISMRMRD::ISMRMRD_FLOAT)
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

  if (stype == ISMRMRD::ISMRMRD_DOUBLE)
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
  ENTITY* ent,
  STYPE   stype
)
{
  if (!_acq_storage_set)
  {
    _acq_storage     = stype;
    _acq_storage_set = true;
  }
  else if (_acq_storage != stype)
  {
    throw std::runtime_error ("Inconsistent acquisition storage type");
  }

  _acqs.push_back (copyEntity (ent, stype));

  if (_header_received &&
      _acqs.size() == _hdr.encoding[0].encodedSpace.matrixSize.y)
  {
    ENTITY* img_ent = _imrec->runReconstruction (_acqs, stype, _hdr);
    _session->send (ISMRMRD::ISMRMRD_IMAGE, img_ent, stype);
    _imrec_done = true;

    if (_client_done)
    {
      sendCommand (ISMRMRD::ISMRMRD_COMMAND_DONE_FROM_SERVER, 0);
    }
  }
}

/*******************************************************************************
 ******************************************************************************/
void icpServer::saveSessionPointer
(
  icpSession* session
)
{
  _session = session;
}

/*******************************************************************************
 ******************************************************************************/
icpServer::icpServer
(
  uint32_t     id   // debug only
)
: _header_received (false),
  _acq_storage_set (false),
  _client_done (false),
  _imrec_done  (false),
  _id          (id)
{
  std::cout << __func__ << " (" << _id << ") - starting\n";
}

/*******************************************************************************
 ******************************************************************************/
icpServer::~icpServer ()
{
  std::cout << __func__ << " (" << _id << ") - completed\n";
  delete (_imrec);
}
