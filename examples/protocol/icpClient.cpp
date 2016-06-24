// client.cpp
#include "icpClient.h"
#include "icpClientCallbacks.h"

/*****************************************************************************
 ****************************************************************************/
void icpClient::processHandshake
(
  HANDSHAKE* msg
)
{
  std::cout << "Received handshake response for \"" << _client_name
            << "\": name = \"" << msg->getClientName()
            << "\", status = " << msg->getConnectionStatus() << "\n";
}

/*****************************************************************************
 ****************************************************************************/
void icpClient::processCommand
(
  COMMAND* msg
)
{
  switch (msg->getCommandType())
  {
    case ISMRMRD::ISMRMRD_COMMAND_DONE_FROM_SERVER:

      std::cout << "Received DONE from server\n";
      _server_done = true;
      if (_task_done)
      {
        std::cout << "Sending CLOSE_CONNECTION command\n";
        sendCommand (ISMRMRD::ISMRMRD_COMMAND_CLOSE_CONNECTION, 0);
        std::cout << _client_name << " shutting down\n";
        _session->shutdown();
      }
      break;

    default:
      std::cout << "Received unexpected command\n";
      break;
  }
}

/*****************************************************************************
 ****************************************************************************/
void icpClient::processError
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

/*****************************************************************************
 ****************************************************************************/
void icpClient::taskDone
(
)
{
  _task_done = true;
  if (_server_done)
  {
    std::cout << "Sending CLOSE_CONNECTION command\n";
    sendCommand (ISMRMRD::ISMRMRD_COMMAND_CLOSE_CONNECTION, 0);
    std::cout << _client_name << " shutting down\n";
    _session->shutdown();
  }
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
  _session->send (&msg, ISMRMRD_VERSION_MAJOR, ISMRMRD::ISMRMRD_COMMAND,
          ISMRMRD::ISMRMRD_STORAGE_NONE, ISMRMRD::ISMRMRD_STREAM_COMMAND);

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
  ISMRMRD::ErrorReport msg;
  msg.setErrorType (type);
  msg.setErrorDescription (descr);
  _session->send (&msg, ISMRMRD_VERSION_MAJOR, ISMRMRD::ISMRMRD_ERROR_REPORT,
         ISMRMRD::ISMRMRD_STORAGE_NONE, ISMRMRD::ISMRMRD_STREAM_ERRREPORT);

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
  // fill in the manifest here
  _session->send (&msg, ISMRMRD_VERSION_MAJOR, ISMRMRD::ISMRMRD_HANDSHAKE,
              ISMRMRD::ISMRMRD_STORAGE_NONE, ISMRMRD::ISMRMRD_STREAM_HANDSHAKE);
}

/*******************************************************************************
 ******************************************************************************/
void icpClient::beginInput
(
)
{
  std::cout << __func__ << "  starting\n";

  sendHandshake ();

  ISMRMRD::Dataset dset (_in_fname, _in_dset);
  std::string xml_head = dset.readHeader();

  ISMRMRD::IsmrmrdHeader xmlHeader;
  ISMRMRD::deserialize (xml_head.c_str(), xmlHeader);

  ETYPE etype = (ETYPE)xmlHeader.streams[0].entityType;
  STYPE stype = (STYPE)xmlHeader.streams[0].storageType;

  ISMRMRD::Command msg;
  msg.setCommandType (ISMRMRD::ISMRMRD_COMMAND_CONFIGURATION);
  msg.setCommandId (0);
  
  if (etype == ISMRMRD::ISMRMRD_MRACQUISITION)
  {
    if (stype == ISMRMRD::ISMRMRD_SHORT)
    {
      msg.setConfigType (ISMRMRD::CONFIGURATION_RECON_SHORT);
    }
    else if (stype == ISMRMRD::ISMRMRD_INT)
    {
      msg.setConfigType (ISMRMRD::CONFIGURATION_RECON_INT);
    }
    else if (stype == ISMRMRD::ISMRMRD_FLOAT ||
             stype == ISMRMRD::ISMRMRD_CXFLOAT)
    {
      msg.setConfigType (ISMRMRD::CONFIGURATION_RECON_FLOAT);
    }
    else if (stype == ISMRMRD::ISMRMRD_DOUBLE ||
             stype == ISMRMRD::ISMRMRD_CXDOUBLE)
    {
      msg.setConfigType (ISMRMRD::CONFIGURATION_RECON_DOUBLE);
    }
    else
    {
      std::cout << "Warning, unexpected acquisition storage type in file\n";
      msg.setConfigType (ISMRMRD::CONFIGURATION_NONE);
    }
  }
  else
  {
      std::cout << "Warning, unexpected entity type in file\n";
      msg.setConfigType (ISMRMRD::CONFIGURATION_NONE);
  }
  _session->send (&msg, ISMRMRD_VERSION_MAJOR, ISMRMRD::ISMRMRD_COMMAND,
      ISMRMRD::ISMRMRD_STORAGE_NONE, ISMRMRD::ISMRMRD_STREAM_COMMAND);


  ISMRMRD::IsmrmrdHeaderWrapper wrapper (xml_head);
  _session->send (&wrapper, ISMRMRD_VERSION_MAJOR, ISMRMRD::ISMRMRD_HEADER,
        ISMRMRD::ISMRMRD_STORAGE_NONE, ISMRMRD::ISMRMRD_STREAM_ISMRMRD_HEADER); 

  if (stype == ISMRMRD::ISMRMRD_SHORT)
  {
    sendAcquisitions <int16_t> (dset);
  }
  else if (stype == ISMRMRD::ISMRMRD_INT)
  {
    sendAcquisitions <int32_t> (dset);
  }
  // TODO: FLOAT and CXFLOAT are treated the same here
  else if (stype == ISMRMRD::ISMRMRD_FLOAT ||
           stype == ISMRMRD::ISMRMRD_CXFLOAT)
  {
    sendAcquisitions <float> (dset);
  }
  // TODO: DOUBLE and CXDOUBLE are treated the same here 
  else if (stype == ISMRMRD::ISMRMRD_DOUBLE ||
           stype == ISMRMRD::ISMRMRD_CXDOUBLE)
  {
    sendAcquisitions <double> (dset);
  }
  else
  {
    std::cout << "Acq storage type = " << stype << "\n";
    throw std::runtime_error ("Unexpected MR Acquisition storage type");
  }
  
  ISMRMRD::Command cmd;
  cmd.setCommandId (0);
  cmd.setCommandType (ISMRMRD::ISMRMRD_COMMAND_STOP_FROM_CLIENT);
  _session->send (&cmd, ISMRMRD_VERSION_MAJOR, ISMRMRD::ISMRMRD_COMMAND,
          ISMRMRD::ISMRMRD_STORAGE_NONE, ISMRMRD::ISMRMRD_STREAM_COMMAND);

  std::cout << "Input completed\n";
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
    usleep (500);
    ISMRMRD::Acquisition<S> acq = dset.readAcquisition<S> (ii, 0);
    _session->send (&acq, ISMRMRD_VERSION_MAJOR, ISMRMRD::ISMRMRD_MRACQUISITION,
                              ISMRMRD::get_storage_type<S>(), acq.getStream());
  }
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
: _session         (std::move (session)),
  _client_name     (client_name),
  _in_fname        (in_fname),
  _out_fname       (out_fname),
  _in_dset         (in_dataset),
  _out_dset        (out_dataset)//,
  //_server_done     (false),
  //_task_done       (false)//,
  //_header_received (false)
{

  std::cout << "\""<< _client_name << "\"  starting\n";

  using namespace std::placeholders;

  _entCB = new icpClientEntityHandler (this);
  auto fp = std::bind (&icpCallback::receive, _entCB, _1, _2, _3, _4, _5, _6);
  _session->registerHandler ((ICP_CB) fp, ISMRMRD::ISMRMRD_HANDSHAKE, _entCB);
  _session->registerHandler ((ICP_CB) fp, ISMRMRD::ISMRMRD_ERROR_REPORT, _entCB);
  _session->registerHandler ((ICP_CB) fp, ISMRMRD::ISMRMRD_COMMAND, _entCB);


  _imgCB = new icpClientImageProcessor (this, _out_fname, _out_dset);
  auto fp1 = std::bind (&icpCallback::receive, _imgCB, _1, _2, _3, _4, _5, _6);
  _session->registerHandler ((ICP_CB) fp1, ISMRMRD::ISMRMRD_IMAGE, _imgCB);
  _session->registerHandler ((ICP_CB) fp1, ISMRMRD::ISMRMRD_HEADER, _imgCB);


  std::thread it (&icpClient::beginInput, this);
  _session->run ();
  if (it.joinable())
  {
    it.join();
  }
}

/*******************************************************************************
 ******************************************************************************/
icpClient::~icpClient
(
)
{
  if (_entCB)
  {
    delete (_entCB);
  }

  if (_imgCB)
  {
    delete (_imgCB);
  }
}

/*******************************************************************************
 ******************************************************************************/
