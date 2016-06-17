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
        sleep (1);
        std::cout << "<" << _client_name << "> shutting down\n";
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
    std::cout << "Client " << _client_name << " shutting down\n";
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
  _session->send (&msg, ISMRMRD::ISMRMRD_COMMAND);

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
  _session->send (&msg, ISMRMRD::ISMRMRD_ERROR_REPORT);

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
  _session->send (&msg, ISMRMRD::ISMRMRD_HANDSHAKE);

  return;
}

/*******************************************************************************
 ******************************************************************************/
void icpClient::beginInput
(
)
{
  std::cout << __func__ << "  starting\n";

  sendHandshake ();

  ISMRMRD::Command msg;
  msg.setCommandType (ISMRMRD::ISMRMRD_COMMAND_CONFIGURATION);
  msg.setCommandId (0);
  msg.setConfigType (ISMRMRD::CONFIGURATION_BUILT_IN_1);
  _session->send (&msg, ISMRMRD::ISMRMRD_COMMAND);

  ISMRMRD::Dataset dset (_in_fname, _in_dset);
  std::string xml_head = dset.readHeader();

  ISMRMRD::IsmrmrdHeader xmlHeader;
  ISMRMRD::deserialize (xml_head.c_str(), xmlHeader);
  ISMRMRD::IsmrmrdHeaderWrapper wrapper;
  wrapper.setHeader (xmlHeader);
  _session->send (&wrapper, ISMRMRD::ISMRMRD_HEADER_WRAPPER); 

  if (xmlHeader.streams[0].storageType == ISMRMRD::ISMRMRD_SHORT)
  {
    sendAcquisitions <int16_t> (dset);
  }
  else if (xmlHeader.streams[0].storageType == ISMRMRD::ISMRMRD_INT)
  {
    sendAcquisitions <int32_t> (dset);
  }
  // TODO: FLOAT and CXFLOAT are treated the same here
  else if (xmlHeader.streams[0].storageType == ISMRMRD::ISMRMRD_FLOAT ||
           xmlHeader.streams[0].storageType == ISMRMRD::ISMRMRD_CXFLOAT)
  {
    sendAcquisitions <float> (dset);
  }
  // TODO: DOUBLE and CXDOUBLE are treated the same here 
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
  cmd.setCommandId (0);
  cmd.setCommandType (ISMRMRD::ISMRMRD_COMMAND_STOP_FROM_CLIENT);
  _session->send (&cmd, ISMRMRD::ISMRMRD_COMMAND);

  std::cout << "Input completed\n";

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
    _session->send (&acq, ISMRMRD::ISMRMRD_MRACQUISITION,
                              ISMRMRD::get_storage_type<S>());
  }

  return;
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
  _out_dset        (out_dataset),
  _server_done     (false),
  _task_done       (false)//,
  //_header_received (false)
{

  std::cout << "\""<< _client_name << "\"  starting\n";

  using namespace std::placeholders;

  std::unique_ptr<icpClientHandshakeHandler> handCB
    (new icpClientHandshakeHandler (this));
  auto fp1 = std::bind (&icpCallback::receive, *handCB, _1, _2, _3);
  _session->registerHandler ((ICP_CB) fp1, ISMRMRD::ISMRMRD_HANDSHAKE);

  std::unique_ptr<icpClientErrorReportHandler> errCB
    (new icpClientErrorReportHandler (this));
  auto fp2 = std::bind (&icpCallback::receive, *errCB, _1, _2, _3);
  _session->registerHandler ((ICP_CB) fp2, ISMRMRD::ISMRMRD_ERROR_REPORT);

  std::unique_ptr<icpClientCommandHandler> commCB
    (new icpClientCommandHandler (this));
  auto fp3 = std::bind (&icpCallback::receive, *commCB, _1, _2, _3);
  _session->registerHandler ((ICP_CB) fp3, ISMRMRD::ISMRMRD_COMMAND);

  //std::unique_ptr<icpClientIsmrmrdHeaderWrapperHandler> headCB
    //(new icpClientIsmrmrdHeaderWrapperHandler (this));
  //auto fp4 = std::bind (&icpCallback::receive, *headCB, _1, _2, _3);
  //_session->registerHandler ((ICP_CB) fp4, ISMRMRD::ISMRMRD_HEADER_WRAPPER);

  //std::unique_ptr<icpClientImageHandler> imCB
    //(new icpClientImageHandler (this));
  //auto fp5 = std::bind (&icpCallback::receive, *imCB, _1, _2, _3);
  //_session->registerHandler ((ICP_CB) fp5, ISMRMRD::ISMRMRD_IMAGE);

  std::unique_ptr<icpClientImageProcessor> imCB
    (new icpClientImageProcessor (this, _out_fname, _out_dset));

  auto fp5 = std::bind (&icpCallback::receive, *imCB, _1, _2, _3);
  _session->registerHandler ((ICP_CB) fp5, ISMRMRD::ISMRMRD_IMAGE);

  auto fp4 = std::bind (&icpCallback::receive, *imCB, _1, _2, _3);
  _session->registerHandler ((ICP_CB) fp4, ISMRMRD::ISMRMRD_HEADER_WRAPPER);


  std::thread it (&icpClient::beginInput, this);
  _session->run ();
  if (it.joinable())
  {
    it.join();
  }
}

/*******************************************************************************
 ******************************************************************************/
