// client.cpp
#include "icpClient.h"

/*******************************************************************************
 ******************************************************************************/
void icpClient::handleCommand
(
  ISMRMRD::Command  msg,
  icpUserAppBase*   base
)
{
  icpClient* inst = static_cast<icpClient*>(base);

  switch (msg.getCommandType())
  {
    case ISMRMRD::ISMRMRD_COMMAND_DONE_FROM_SERVER:

      std::cout << __func__ << ": Received DONE from server\n";
      inst->_server_done = true;
      if (inst->_task_done)
      {
        std::cout << "Sending CLOSE_CONNECTION command\n";
        inst->sendCommand (ISMRMRD::ISMRMRD_COMMAND_CLOSE_CONNECTION, 0, inst);
        sleep (1);
        std::cout << "Client " << inst->_client_name << " shutting down\n";
        inst->_session->shutdown();
      }
      break;

    default:
      std::cout << __func__ << ": Received unexpected command "
                << msg.getCommandType() << " from server\n";
      break;
  }
  return;
}

/*******************************************************************************
 ******************************************************************************/
void icpClient::handleErrorNotification
(
  ISMRMRD::ErrorNotification msg,
  icpUserAppBase*   base
)
{
  std::cout << __func__ <<":\nType: " << msg.getErrorType()
            << ", Error Command: " << msg.getErrorCommandType()
            << ", Error Command ID: " << msg.getErrorCommandId()
            << ", Error Entity: " << msg.getErrorEntityType()
            << ",\nError Description: " << msg.getErrorDescription() << "\n";
  return;
}

/*******************************************************************************
 ******************************************************************************/
void icpClient::handleIsmrmrdHeader
(
  ISMRMRD::IsmrmrdHeader  msg,
  icpUserAppBase*   base
)
{
  icpClient* inst = static_cast<icpClient*>(base);

  std::cout << __func__ << ": Received IsmrmrdHeader\n";
  inst->_header_received = true;
  std::cout << __func__ <<": 1\n";
  inst->_header = msg;
  std::cout << __func__ <<": 2\n";

  return;
}

/*****************************************************************************
 ****************************************************************************/
void icpClient::sendCommand
(
  ISMRMRD::CommandType cmd_type,
  uint32_t             cmd_id,
  icpClient*           inst
)
{
  ISMRMRD::Command msg;
  msg.setCommandType (cmd_type);
  msg.setCommandId (cmd_id);
  inst->_session->forwardMessage (ISMRMRD::ISMRMRD_COMMAND, &msg);
  std::cout << __func__ << ": sent out a command\n";

  return;
}

/*****************************************************************************
 ****************************************************************************/
void icpClient::handleHandshake
(
  ISMRMRD::Handshake  msg,
  icpUserAppBase*   base
)
{
  icpClient* inst = static_cast<icpClient*>(base);

  std::cout << __func__ << " response for " << inst->_client_name << ":\n";
  std::cout << "Name  : " << msg.getClientName() << "\n";
  std::cout << "Status: " << msg.getConnectionStatus() << "\n";
  return;
}

/*****************************************************************************
 ****************************************************************************/
void icpClient::sendError
(
  ISMRMRD::ErrorType type,
  std::string        descr,
  icpClient*         inst
)
{
  ISMRMRD::ErrorNotification msg;
  msg.setErrorType (type);
  msg.setErrorDescription (descr);
  inst->_session->forwardMessage (ISMRMRD::ISMRMRD_ERROR_NOTIFICATION, &msg);

  return;
}

/*******************************************************************************
 ******************************************************************************/
void icpClient::handleImage
(
  ISMRMRD::Entity* ent,
  uint32_t         storage,
  icpUserAppBase*   base
)
{
  std::cout << __func__ << "\n";
  icpClient* inst = static_cast<icpClient*>(base);

  ISMRMRD::Dataset dset (inst->_out_fname.c_str(), inst->_out_dset.c_str());

  if (_header_received) // For debug only
  {
    std::stringstream sstr;
    ISMRMRD::serialize (inst->_header, sstr);
    dset.writeHeader ((std::string) sstr.str ());
  }

  //uint32_t storage =
    //static_cast<ISMRMRD::Image<float>*>(ent)->getStorageType();
  inst->writeImage (dset, ent, storage);
  std::cout << "\nFinished processing image" << std::endl;

  inst->_task_done = true;
  if (inst->_server_done)
  {
    std::cout << "Sending CLOSE_CONNECTION command\n";
    inst->sendCommand (ISMRMRD::ISMRMRD_COMMAND_CLOSE_CONNECTION, 0, inst);
    sleep (1);
    std::cout << "Client " << inst->_client_name << " shutting down\n";
    inst->_session->shutdown();
  }

  return;
}
/*******************************************************************************
 ******************************************************************************/
void icpClient::writeImage
(
  ISMRMRD::Dataset& dset,
  ISMRMRD::Entity*  ent,
  uint32_t          storage
)
{
  if (storage == ISMRMRD::ISMRMRD_FLOAT)
  {
    ISMRMRD::Image<float>* img = static_cast<ISMRMRD::Image<float>*>(ent);
    dset.appendImage (*img, ISMRMRD::ISMRMRD_STREAM_IMAGE);
  }
  else if (storage == ISMRMRD::ISMRMRD_DOUBLE)
  {
    ISMRMRD::Image<double>* img = static_cast<ISMRMRD::Image<double>*>(ent);
    dset.appendImage (*img, ISMRMRD::ISMRMRD_STREAM_IMAGE);
  }
  else
  {
    std::cout << __func__ << ": Error! Unexpected image storage type\n";
  }

  return;
}

/*******************************************************************************
 ******************************************************************************/
void icpClient::sendHandshake
(
  icpClient* inst
)
{
  ISMRMRD::Handshake msg;
  msg.setTimestamp ((uint64_t)std::time(nullptr));
  msg.setConnectionStatus (ISMRMRD::CONNECTION_REQUEST);
  msg.setClientName (_client_name);
  inst->_session->forwardMessage (ISMRMRD::ISMRMRD_HANDSHAKE, &msg);

  return;
}

/*******************************************************************************
 ******************************************************************************/
void icpClient::beginInput
(
)
{
  
  std::cout << __func__ << ": starting\n";
  //sleep (1);
  sendHandshake (this);
  sendCommand (ISMRMRD::ISMRMRD_COMMAND_CONFIG_IMREC_ONE, 0, this);
  std::cout << __func__ << "Sent out config command\n";

  ISMRMRD::Dataset dset (_in_fname, _in_dset);
  std::string xml_head = dset.readHeader();

  ISMRMRD::IsmrmrdHeader xmlHeader;
  ISMRMRD::deserialize (xml_head.c_str(), xmlHeader);
  ISMRMRD::IsmrmrdHeaderWrapper wrapper;
  wrapper.setHeader (xmlHeader);
  _session->forwardMessage (ISMRMRD::ISMRMRD_HEADER_WRAPPER, &wrapper); 

  if (xmlHeader.streams[0].storageType == ISMRMRD::ISMRMRD_SHORT)
  {
    sendAcquisitions <int16_t> (dset);
  }
  else if (xmlHeader.streams[0].storageType == ISMRMRD::ISMRMRD_INT)
  {
    sendAcquisitions <int32_t> (dset);
  }
  // Both FLOAT and CXFLOAT should probably be treated the same here
  else if (xmlHeader.streams[0].storageType == ISMRMRD::ISMRMRD_FLOAT ||
           xmlHeader.streams[0].storageType == ISMRMRD::ISMRMRD_CXFLOAT)
  {
    sendAcquisitions <float> (dset);
  }
  // Both DOUBLE and CXDOUBLE should probably be treated the same here
  else if (xmlHeader.streams[0].storageType == ISMRMRD::ISMRMRD_DOUBLE ||
           xmlHeader.streams[0].storageType == ISMRMRD::ISMRMRD_CXDOUBLE)
  {
    sendAcquisitions <double> (dset);
  }
  else
  {
    std::cout << "Acq storage type = " << xmlHeader.streams[0].storageType << "\n";
    throw std::runtime_error ("Unexpected MR Acquisition storage type");
  }
  
  ISMRMRD::Command cmd;
  cmd.setCommandId (2);
  cmd.setCommandType (ISMRMRD::ISMRMRD_COMMAND_STOP_FROM_CLIENT);
  _session->forwardMessage (ISMRMRD::ISMRMRD_COMMAND, &cmd);

  uint32_t num_acq = dset.getNumberOfAcquisitions (0);
  std::cout << "Finished input, " << num_acq << " acqs, sent STOP_FROM_CLIENT\n";

  return;
}

/*******************************************************************************
 ******************************************************************************/
template <typename S>
void icpClient::sendAcquisitions
(
  ISMRMRD::Dataset& dset
)
{
  std::cout << __func__ << ": starting\n";
  uint32_t num_acq = dset.getNumberOfAcquisitions (0);

  for (int ii = 0; ii < num_acq; ii++)
  {
    ISMRMRD::Acquisition<S> acq = dset.readAcquisition<S> (ii, 0);
    _session->forwardMessage (ISMRMRD::ISMRMRD_MRACQUISITION, &acq,
                              ISMRMRD::get_storage_type<S>());
  }

  std::cout << __func__ << ": sent out " << num_acq << " acquisitions \n";
  return;
}

/*******************************************************************************
 ******************************************************************************/
void icpClient::icpClient::run
(
)
{
  std::cout << __func__ << ": 1\n";
  std::thread it (&icpClient::beginInput, this);
  if (it.joinable())
  {
    it.join();
  }
  _session->beginReceiving (this);
  //if (it.joinable())
  //{
    //it.join();
  //}
}

/*******************************************************************************
 ******************************************************************************/
icpClient::icpClient
(
  ICP_SESSION      session,
  std::string      client_name,
  std::string      in_fname,
  std::string      out_fname,
  std::string      in_dataset,
  std::string      out_dataset
)
: _session         (session),
  _client_name     (client_name),
  _in_fname        (in_fname),
  _out_fname       (out_fname),
  _in_dset         (in_dataset),
  _out_dset        (out_dataset),
  _server_done     (false),
  _task_done       (false),
  _header_received (false)//,
  //_input_thread    ()
{

  std::ios_base::sync_with_stdio(false);
  std::cin.tie(static_cast<std::ostream*>(0));
  std::cerr.tie(static_cast<std::ostream*>(0));

  using namespace std::placeholders;

  auto fp1 = std::bind (&icpClient::handleHandshake, *this, _1, _2);
  _session->registerHandler ((CB_HANDSHK) fp1, ISMRMRD::ISMRMRD_HANDSHAKE);

  auto fp2 = std::bind (&icpClient::handleErrorNotification, *this, _1, _2);
  _session->registerHandler ((CB_ERRNOTE) fp2, ISMRMRD::ISMRMRD_ERROR_NOTIFICATION);

  auto fp3 = std::bind (&icpClient::handleCommand, *this, _1, _2);
  _session->registerHandler ((CB_COMMAND) fp3, ISMRMRD::ISMRMRD_COMMAND);

  auto fp4 = std::bind (&icpClient::handleIsmrmrdHeader, *this, _1, _2);
  _session->registerHandler ((CB_XMLHEAD) fp4, ISMRMRD::ISMRMRD_HEADER);

  auto fp5 = std::bind (&icpClient::handleImage, *this, _1, _2, _3);
  _session->registerHandler ((CB_IMAGE) fp5, ISMRMRD::ISMRMRD_IMAGE);
}

/*******************************************************************************
 ******************************************************************************/
/*
icpClient::icpClient
(
)
{
}
*/
