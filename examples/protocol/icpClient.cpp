// client.cpp
#include "icpClient.h"
#include "icpClientCallbacks.h"

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
  std::cout << __func__ << ": sent out a command\n";

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
  _session->send (&msg, ISMRMRD::ISMRMRD_ERROR_NOTIFICATION);

  return;
}

/*******************************************************************************
 ******************************************************************************/
void icpClient::writeImage
(
  ISMRMRD::Dataset& dset,
  ISMRMRD::Entity*  ent,
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
  ISMRMRD::Handshake msg;
  msg.setTimestamp ((uint64_t)std::time(nullptr));
  msg.setConnectionStatus (ISMRMRD::CONNECTION_REQUEST);
  msg.setClientName (_client_name);
  _session->send (&msg, ISMRMRD::ISMRMRD_HANDSHAKE);

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
  sendHandshake ();
  sendCommand (ISMRMRD::ISMRMRD_COMMAND_CONFIG_IMREC_ONE, 0);
  std::cout << __func__ << "Sent out config command\n";

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
  _session->send (&cmd, ISMRMRD::ISMRMRD_COMMAND);

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
    _session->send (&acq, ISMRMRD::ISMRMRD_MRACQUISITION,
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
  _session->beginReceiving ();
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
  _out_dset        (out_dataset)//,
  //_server_done     (false),
  //_task_done       (false),
  //_header_received (false)//,
  //_input_thread    ()
{

  std::ios_base::sync_with_stdio(false);
  std::cin.tie(static_cast<std::ostream*>(0));
  std::cerr.tie(static_cast<std::ostream*>(0));

  using namespace std::placeholders;

  std::unique_ptr<icpClientHandshakeHandler> handCB
    (new icpClientHandshakeHandler (this));
  auto fp1 = std::bind (&icpCallback::receive, *handCB, _1, _2, _3);
  _session->registerHandler ((ICP_CB) fp1, ISMRMRD::ISMRMRD_HANDSHAKE);

  std::unique_ptr<icpClientErrorNotificationHandler> errCB
    (new icpClientErrorNotificationHandler (this));
  auto fp2 = std::bind (&icpCallback::receive, *errCB, _1, _2, _3);
  _session->registerHandler ((ICP_CB) fp2, ISMRMRD::ISMRMRD_ERROR_NOTIFICATION);

  std::unique_ptr<icpClientCommandHandler> commCB
    (new icpClientCommandHandler (this));
  auto fp3 = std::bind (&icpCallback::receive, *commCB, _1, _2, _3);
  _session->registerHandler ((ICP_CB) fp3, ISMRMRD::ISMRMRD_COMMAND);

  std::unique_ptr<icpClientIsmrmrdHeaderWrapperHandler> headCB
    (new icpClientIsmrmrdHeaderWrapperHandler (this));
  auto fp4 = std::bind (&icpCallback::receive, *headCB, _1, _2, _3);
  _session->registerHandler ((ICP_CB) fp4, ISMRMRD::ISMRMRD_HEADER_WRAPPER);

  std::unique_ptr<icpClientImageHandler> imCB
    (new icpClientImageHandler (this));
  auto fp5 = std::bind (&icpCallback::receive, *imCB, _1, _2, _3);
  _session->registerHandler ((ICP_CB) fp5, ISMRMRD::ISMRMRD_IMAGE);
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
