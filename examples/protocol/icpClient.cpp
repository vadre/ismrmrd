// client.cpp
#include "icpClient.h"

/*******************************************************************************
 ******************************************************************************/
void icpClient::handleCommand
(
  ISMRMRD::Command  msg
)
{
  switch (msg.getCommandType())
  {
    case ISMRMRD::ISMRMRD_COMMAND_DONE_FROM_SERVER:

      std::cout << __func__ << ": Received DONE from server\n";
      _server_done = true;
      if (_task_done)
      {
        std::cout << "Sending CLOSE_CONNECTION command\n";
        sendCommand (ISMRMRD::ISMRMRD_COMMAND_CLOSE_CONNECTION, 0);
        sleep (1);
        std::cout << "Client " << _client_name << " shutting down\n";
        _session->shutdown();
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
  ISMRMRD::ErrorNotification msg
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
  ISMRMRD::IsmrmrdHeader  msg
)
{
  std::cout << __func__ << ": Received IsmrmrdHeader\n";
  _header_received = true;
  _header = msg;

  return;
}

/*****************************************************************************
 ****************************************************************************/
void icpClient::sendCommand
(
  ISMRMRD::CommandType cmd_type,
  uint32_t             cmd_id
)
{
  ISMRMRD::Command msg;
  msg.setCommandType (cmd_type);
  msg.setCommandId (cmd_id);
  _session->forwardMessage (ISMRMRD::ISMRMRD_COMMAND, &msg);

  return;
}

/*****************************************************************************
 ****************************************************************************/
void icpClient::handleHandshake
(
  ISMRMRD::Handshake  msg
)
{
  std::cout << __func__ << " response for " << _client_name << ":\n";
  std::cout << "Name  : " << msg.getClientName() << "\n";
  std::cout << "Status: " << msg.getConnectionStatus() << "\n";
  return;
}

/*****************************************************************************
 ****************************************************************************/
void icpClient::sendError
(
  ISMRMRD::ErrorType type,
  std::string        descr
)
{
  ISMRMRD::ErrorNotification msg;
  msg.setErrorType (type);
  msg.setErrorDescription (descr);
  _session->forwardMessage (ISMRMRD::ISMRMRD_ERROR_NOTIFICATION, &msg);

  return;
}

/*******************************************************************************
 ******************************************************************************/
void icpClient::handleImage
(
  ISMRMRD::Entity* ent,
  uint32_t         storage
)
{
  std::cout << __func__ << "\n";

  ISMRMRD::Dataset dset (_out_fname.c_str(), _out_dset.c_str());

  if (_header_received) // For debug only
  {
    std::stringstream sstr;
    ISMRMRD::serialize (_header, sstr);
    dset.writeHeader ((std::string) sstr.str ());
  }

  //uint32_t storage =
    //static_cast<ISMRMRD::Image<float>*>(ent)->getStorageType();
  writeImage (dset, ent, storage);
  std::cout << "\nFinished processing image" << std::endl;

  _task_done = true;
  if (_server_done)
  {
    std::cout << "Sending CLOSE_CONNECTION command\n";
    sendCommand (ISMRMRD::ISMRMRD_COMMAND_CLOSE_CONNECTION, 0);
    sleep (1);
    std::cout << "Client " << _client_name << " shutting down\n";
    _session->shutdown();
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
)
{
  ISMRMRD::Handshake msg;
  msg.setTimestamp ((uint64_t)std::time(nullptr));
  msg.setConnectionStatus (ISMRMRD::CONNECTION_REQUEST);
  msg.setClientName (_client_name);
  _session->forwardMessage (ISMRMRD::ISMRMRD_HANDSHAKE, &msg);

  return;
}

/*******************************************************************************
 ******************************************************************************/
void icpClient::beginInput
(
)
{
  
  std::cout << __func__ << ": 1\n";
  //sleep (1);
  sendHandshake ();
  sendCommand (ISMRMRD::ISMRMRD_COMMAND_CONFIG_IMREC_ONE, 0);

  ISMRMRD::Dataset dset (_in_fname, _in_dset);
  std::cout << __func__ << ": 5\n";
  std::string xml_head = dset.readHeader();
  std::cout << __func__ << ": 6\n";

  ISMRMRD::IsmrmrdHeader xmlHeader;
  std::cout << __func__ << ": 7\n";
  ISMRMRD::deserialize (xml_head.c_str(), xmlHeader);
  std::cout << __func__ << ": 8\n";
  ISMRMRD::IsmrmrdHeaderWrapper wrapper;
  std::cout << __func__ << ": 9\n";
  wrapper.setHeader (xmlHeader);
  std::cout << __func__ << ": 10\n";
  _session->forwardMessage (ISMRMRD::ISMRMRD_HEADER_WRAPPER, &wrapper); 
  std::cout << __func__ << ": 11\n";

  
  sendAcquisitions (dset, xmlHeader.streams[0].storageType);
  std::cout << __func__ << ": 12\n";

  ISMRMRD::Command cmd;
  std::cout << __func__ << ": 13\n";
  cmd.setCommandId (2);
  std::cout << __func__ << ": 14\n";
  cmd.setCommandType (ISMRMRD::ISMRMRD_COMMAND_STOP_FROM_CLIENT);
  std::cout << __func__ << ": 15\n";

  _session->forwardMessage (ISMRMRD::ISMRMRD_COMMAND, &cmd);

  std::cout << __func__ << ": 16\n";
  uint32_t num_acq = dset.getNumberOfAcquisitions (0);
  std::cout << "Finished input (" << num_acq << "), sent STOP_FROM_CLIENT\n";

  return;
}

/*******************************************************************************
 ******************************************************************************/
void icpClient::sendAcquisitions
(
  ISMRMRD::Dataset& dset,
  uint32_t          storage
)
{
  std::cout << __func__ << ": 1\n";
  uint32_t num_acq = dset.getNumberOfAcquisitions (0);
  std::cout << __func__ << ": 2\n";
  //uint32_t storage = dset.readAcquisition<int16_t> (0, 0).getStorageType();
  std::cout << __func__ << ": 3\n";

  //TODO: Per storage type: int16_t, int32_t, float, and stream
  if (storage == ISMRMRD::ISMRMRD_SHORT)
  {
  std::cout << __func__ << ": 4\n";
    for (int ii = 0; ii < num_acq; ii++)
    {
      ISMRMRD::Acquisition<int16_t> acq = dset.readAcquisition<int16_t> (ii, 0);
      _session->forwardMessage (ISMRMRD::ISMRMRD_MRACQUISITION, &acq);
    }
  }
  else if (storage == ISMRMRD::ISMRMRD_INT)
  {
  std::cout << __func__ << ": 5\n";
    for (int ii = 0; ii < num_acq; ii++)
    {
      ISMRMRD::Acquisition<int32_t> acq = dset.readAcquisition<int32_t> (ii, 0);
      _session->forwardMessage (ISMRMRD::ISMRMRD_MRACQUISITION, &acq);
    }
  }
  else if (storage == ISMRMRD::ISMRMRD_FLOAT)
  {
  std::cout << __func__ << ": 6\n";
    for (int ii = 0; ii < num_acq; ii++)
    {
      ISMRMRD::Acquisition<float> acq = dset.readAcquisition<float> (ii, 0);
      _session->forwardMessage (ISMRMRD::ISMRMRD_MRACQUISITION, &acq);
    }
  }
  else if (storage == ISMRMRD::ISMRMRD_DOUBLE)
  {
  std::cout << __func__ << ": 7\n";
    for (int ii = 0; ii < num_acq; ii++)
    {
      ISMRMRD::Acquisition<double> acq = dset.readAcquisition<double> (ii, 0);
      _session->forwardMessage (ISMRMRD::ISMRMRD_MRACQUISITION, &acq);
    }
  }
  else
  {
    std::cout << __func__ << ": Unexpected storage type " << storage << "\n";
  }

  std::cout << __func__ << ": 8\n";
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
  std::cout << __func__ << ": 2\n";
  _session->beginReceiving();
  std::cout << __func__ << ": 3\n";
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

  auto fp1 = std::bind (&icpClient::handleHandshake, *this, _1);
  _session->registerHandler ((CB_HANDSHK) fp1, ISMRMRD::ISMRMRD_HANDSHAKE);

  auto fp2 = std::bind (&icpClient::handleErrorNotification, *this, _1);
  _session->registerHandler ((CB_ERRNOTE) fp2, ISMRMRD::ISMRMRD_ERROR_NOTIFICATION);

  auto fp3 = std::bind (&icpClient::handleCommand, *this, _1);
  _session->registerHandler ((CB_COMMAND) fp3, ISMRMRD::ISMRMRD_COMMAND);

  auto fp4 = std::bind (&icpClient::handleIsmrmrdHeader, *this, _1);
  _session->registerHandler ((CB_XMLHEAD) fp4, ISMRMRD::ISMRMRD_HEADER);

  auto fp5 = std::bind (&icpClient::handleImage, *this, _1, _2);
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
