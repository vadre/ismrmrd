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
  _session->send (ISMRMRD::ISMRMRD_COMMAND, &msg);

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
  _session->send (ISMRMRD::ISMRMRD_ERROR_NOTIFICATION, &msg);

  return;
}

/*******************************************************************************
 ******************************************************************************/
void icpClient::handleImage
(
  ENTITY* ent,
  STYPE   stype
)
{
  ISMRMRD::Dataset dset (_out_fname.c_str(), _out_dset.c_str());

  if (_header_received) // For debug only
  {
    std::stringstream sstr;
    ISMRMRD::serialize (_header, sstr);
    dset.writeHeader ((std::string) sstr.str ());
  }

  writeImage (dset, ent, stype);
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
  ENTITY*           ent,
  STYPE             stype
)
{
  if (stype == ISMRMRD::ISMRMRD_FLOAT)
  {
    ISMRMRD::Image<float>* img = static_cast<ISMRMRD::Image<float>*>(ent);
    dset.appendImage (*img, ISMRMRD::ISMRMRD_STREAM_IMAGE);
  }
  else if (stype == ISMRMRD::ISMRMRD_DOUBLE)
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
  HANDSHAKE msg;
  msg.setTimestamp ((uint64_t)std::time(nullptr));
  msg.setConnectionStatus (ISMRMRD::CONNECTION_REQUEST);
  msg.setClientName (_client_name);
  _session->send (ISMRMRD::ISMRMRD_HANDSHAKE, &msg);

  return;
}

/*******************************************************************************
 ******************************************************************************/
void icpClient::beginInput
(
)
{
  sleep (1);
  std::cout << __func__ << ": starting\n";
  sendHandshake ();
  sendCommand (ISMRMRD::ISMRMRD_COMMAND_CONFIG_IMREC_ONE, 0);
  std::cout << __func__ << "Sent out config command\n";

  ISMRMRD::Dataset dset (_in_fname, _in_dset);
  std::string xml_head = dset.readHeader();

  ISMRMRD::IsmrmrdHeader xmlHeader;
  ISMRMRD::deserialize (xml_head.c_str(), xmlHeader);
  ISMRMRD::IsmrmrdHeaderWrapper wrapper;
  wrapper.setHeader (xmlHeader);
  _session->send (ISMRMRD::ISMRMRD_HEADER_WRAPPER, &wrapper); 
  std::cout << __func__ << "Sent out IsmrmrdHeader\n";

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
  _session->send (ISMRMRD::ISMRMRD_COMMAND, &cmd);

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
  uint32_t num_acq = dset.getNumberOfAcquisitions (0);

  for (int ii = 0; ii < num_acq; ii++)
  {
    ISMRMRD::Acquisition<S> acq = dset.readAcquisition<S> (ii, 0);
    _session->send (ISMRMRD::ISMRMRD_MRACQUISITION, &acq,
                              ISMRMRD::get_storage_type<S>());
  }
  std::cout << __func__ << ": sent out " << num_acq << " acquisitions \n";
  return;
}

/*******************************************************************************
 ******************************************************************************/
void icpClient::saveSessionPointer
(
  icpSession* session
)
{
  _session = session;

  _session->subscribe (ISMRMRD::ISMRMRD_HEADER, true);
  _session->subscribe (ISMRMRD::ISMRMRD_ERROR_NOTIFICATION, true);
  _session->subscribe (ISMRMRD::ISMRMRD_IMAGE, true);

  std::thread it (&icpClient::beginInput, *this);
  if (it.joinable())
  {
    it.join();
  }
}

/*******************************************************************************
 ******************************************************************************/
icpClient::icpClient
(
  std::string      client_name,
  std::string      in_fname,
  std::string      out_fname,
  std::string      in_dataset,
  std::string      out_dataset
)
: _client_name     (client_name),
  _in_fname        (in_fname),
  _out_fname       (out_fname),
  _in_dset         (in_dataset),
  _out_dset        (out_dataset),
  _server_done     (false),
  _task_done       (false),
  _header_received (false)
{
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
