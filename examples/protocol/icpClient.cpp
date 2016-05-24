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
  ISMRMRD::ErrorNotification msg,
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
)
{
  ISMRMRD::Command msg;
  msg.setCommandType (cmd_type);
  msg.setCommandId (cmd_id);
  g_client_ptr->forwardMessage (ISMRMRD::ISMRMRD_COMMAND, &msg);

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
  g_client_ptr->forwardMessage (ISMRMRD::ISMRMRD_ERROR_NOTIFICATION, &msg);

  return;
}

/*******************************************************************************
 ******************************************************************************/
void icpClient::handleImage
(
  ISMRMRD::Entity* ent,
)
{
  std::cout << __func__ << "\n";

  ISMRMRD::Dataset dset (_out_fname.c_str(), _out_dset.c_str());

  if (_header-received) // For debug only
  {
    std::stringstream sstr;
    ISMRMRD::serialize (_header, sstr);
    dset.writeHeader ((std::string) sstr.str ());
  }

  uint32_t storage =
    static_cast<ISMRMRD::Image<int16_t>*>(ent).getHead().getStorageType();
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
  if (storage == )
  {
    ISMRMRD::Image<int16_t>* img = static_cast<ISMRMRD::Image<int16_t>*>(ent);
    dset.appendImage ("cpp", *img);
  }
  else if (storage == )
  {
    ISMRMRD::Image<uint16_t>* img = static_cast<ISMRMRD::Image<uint16_t>*>(ent);
    dset.appendImage ("cpp", *img);
  }
  else if (storage == )
  {
    ISMRMRD::Image<int32_t>* img = static_cast<ISMRMRD::Image<int32_t>*>(ent);
    dset.appendImage ("cpp", *img);
  }
  else if (storage == )
  {
    ISMRMRD::Image<uint32_t>* img = static_cast<ISMRMRD::Image<uint32_t>*>(ent);
    dset.appendImage ("cpp", *img);
  }
  else if (storage == )
  {
    ISMRMRD::Image<float>* img = static_cast<ISMRMRD::Image<float>*>(ent);
    dset.appendImage ("cpp", *img);
  }
  else if (storage == )
  {
    ISMRMRD::Image<double>* img = static_cast<ISMRMRD::Image<double>*>(ent);
    dset.appendImage ("cpp", *img);
  }
  else if (storage == )
  {
    ISMRMRD::Image<std::complex<float>>* img =
      static_cast<ISMRMRD::Image<std::complex<float>>*>(ent);
    dset.appendImage ("cpp", *img);
  }
  else if (storage == )
  {
    ISMRMRD::Image<std::complex<double>>* img =
      static_cast<ISMRMRD::Image<std::complex<float>>*>(ent);
    dset.appendImage ("cpp", *img);
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
  _session->forward (ISMRMRD::ISMRMRD_HANDSHAKE, &msg);

  return;
}

/*******************************************************************************
 ******************************************************************************/
void icpClient::beginInput
(
)
{
  sleep (1);
  sendHandshake ();
  sendCommand (ISMRMRD::ISMRMRD_COMMAND_CONFIG_IMREC_ONE, 0);

  ISMRMRD::Dataset dset (_in_fname.c_str(), _in_dset.c_str());
  std::string xml_head = dset.readHeader();

  ISMRMRD::IsmrmrdHeader xmlHeader;
  ISMRMRD::deserialize (xml_head.c_str(), xmlHeader);
  ISMRMRD::IsmrmrdHeaderWrapper wrapper (xmlHeader);
  _session->forward (ISMRMRD::ISMRMRD_HEADER_WRAPPER, &wrapper); 

  sendAcquisitions (dset);

  ISMRMRD::Command cmd;
  cmd.setCommandId (2);
  cmd.setCommandType (ISMRMRD::ISMRMRD_COMMAND_STOP_FROM_CLIENT);
  _session->forward (ISMRMRD::ISMRMRD_COMMAND, &cmd);

  std::cout << "Finished input (" << num_acq << "), sent STOP_FROM_CLIENT\n";

  return;
}

/*******************************************************************************
 ******************************************************************************/
void sendAcquisitions
(
  ISMRMRD::Dataset& dset
)
{
  uint32_t num_acq = dset.getNumberOfAcquisitions (0);
  uint32_t storage =
    dset.readAcquisition<int16_t> (0, 0).getHead().getStorageType();

  //TODO: Per storage type: int16_t, int32_t, float, and stream
  if (storage == ISMRMRD::ISMRMRD_SHORT)
  {
    for (int ii = 0; ii < num_acq; ii++)
    {
      ISMRMRD::Acquisition<int16_t> acq = dset.readAcquisition<int16_t> (ii, 0);
      _session->forward (ISMRMRD::ISMRMRD_MRACQUISITION, &acq);
    }
  }
  else if (storage == ISMRMRD::ISMRMRD_INT)
  {
    for (int ii = 0; ii < num_acq; ii++)
    {
      ISMRMRD::Acquisition<int32_t> acq = dset.readAcquisition<int32_t> (ii, 0);
      _session->forward (ISMRMRD::ISMRMRD_MRACQUISITION, &acq);
    }
  }
  else if (storage == ISMRMRD::ISMRMRD_SHORT)
  {
    for (int ii = 0; ii < num_acq; ii++)
    {
      ISMRMRD::Acquisition<float> acq = dset.readAcquisition<float> (ii, 0);
      _session->forward (ISMRMRD::ISMRMRD_MRACQUISITION, &acq);
    }
  }
  else
  {
    std::cout << __func__ << ": Unexpected storage type " << storage << "\n";
  }

  return;
}

/*******************************************************************************
 ******************************************************************************/
void icpClient::run
(
)
{
  std::thread (&icpClient::beginInput, this).detach();
  _session->beginReceiving();
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
  _header_received (false)
{
  auto fp1 = std::bind (&icpClient::handleHandshake, *this, _1, _2);
  _session->registerHandler ((CB_HANDSHK) fp1, ISMRMRD::ISMRMRD_HANDSHAKE);

  auto fp2 = std::bind (&icpClient::handleErrorNotification, *this, _1, _2);
  _session->registerHandler ((CB_ERRNOTE) fp2, ISMRMRD::ISMRMRD_ERROR_NOTIFICATION);

  auto fp3 = std::bind (&icpClient::handleCommand, *this, _1, _2);
  _session->registerHandler ((CB_COMMAND) fp3, ISMRMRD::ISMRMRD_COMMAND);

  auto fp4 = std::bind (&icpClient::handleIsmrmrdHeader, *this, _1, _2);
  _session->registerHandler ((CB_XMLHEAD) fp4, ISMRMRD::ISMRMRD_HEADER);

  auto fp5 = std::bind (&icpClient::handleImage, *this, _1, _2);
  _session->registerHandler ((CB_IMG_FLT) fp5, ISMRMRD::ISMRMRD_IMAGE);
}
