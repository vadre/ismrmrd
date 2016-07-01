// client.cpp
#include "icpClient.h"
#include "icpClientCallbacks.h"

using namespace ISMRMRD::ICP;

static std::mutex gmtx;

/*****************************************************************************
 ****************************************************************************/
void Client::processHandshake
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
void Client::processCommand
(
  COMMAND* msg
)
{
  switch (msg->getCommandType())
  {
    case ISMRMRD_COMMAND_DONE_FROM_SERVER:

      std::cout << "Received DONE from server\n";
      _server_done = true;
      if (_task_done)
      {
        std::cout << "Sending CLOSE_CONNECTION command\n";
        sendCommand (ISMRMRD_COMMAND_CLOSE_CONNECTION, 0);
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
void Client::processError
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
void Client::taskDone
(
)
{
  _task_done = true;
  if (_server_done)
  {
    std::cout << "Sending CLOSE_CONNECTION command\n";
    sendCommand (ISMRMRD_COMMAND_CLOSE_CONNECTION, 0);
    std::cout << _client_name << " shutting down\n";
    _session->shutdown();
  }
}

/*****************************************************************************
 ****************************************************************************/
void Client::sendCommand
(
  CommandType cmd_type,
  uint32_t    cmd_id
)
{
  Command msg;
  msg.setCommandType (cmd_type);
  msg.setCommandId (cmd_id);
  _session->send (&msg);

  return;
}

/*****************************************************************************
 ****************************************************************************/
void Client::sendError
(
  ErrorType    type,
  std::string  descr
)
{
  ErrorReport msg;
  msg.setErrorType (type);
  msg.setErrorDescription (descr);
  _session->send (&msg);

  return;
}

/*******************************************************************************
 ******************************************************************************/
void Client::sendHandshake
(
)
{
  Handshake msg;
  msg.setTimestamp ((uint64_t)std::time(nullptr));
  msg.setConnectionStatus (CONNECTION_REQUEST);
  msg.setClientName (_client_name);
  // fill in the manifest here
  _session->send (&msg);
}

/*******************************************************************************
 ******************************************************************************/
void Client::beginInput
(
  std::mutex& mtx
)
{
  std::cout << __func__ << "  starting\n";

  sendHandshake ();

  std::string xml_head;
  Dataset dset (_in_fname, _in_dset);

  {
    std::lock_guard<std::mutex> guard (mtx);
    xml_head = dset.readHeader();
  }

  IsmrmrdHeader xmlHeader;
  deserialize (xml_head.c_str(), xmlHeader);

  ETYPE etype = (ETYPE)xmlHeader.streams[0].entityType;
  STYPE stype = (STYPE)xmlHeader.streams[0].storageType;

  Command msg;
  msg.setCommandType (ISMRMRD_COMMAND_CONFIGURATION);
  msg.setCommandId (0);
  
  if (etype == ISMRMRD_MRACQUISITION)
  {
    if (stype == ISMRMRD_SHORT)
    {
      msg.setConfigType (CONFIGURATION_RECON_SHORT);
    }
    else if (stype == ISMRMRD_INT)
    {
      msg.setConfigType (CONFIGURATION_RECON_INT);
    }
    else if (stype == ISMRMRD_FLOAT ||
             stype == ISMRMRD_CXFLOAT)
    {
      msg.setConfigType (CONFIGURATION_RECON_FLOAT);
    }
    else if (stype == ISMRMRD_DOUBLE ||
             stype == ISMRMRD_CXDOUBLE)
    {
      msg.setConfigType (CONFIGURATION_RECON_DOUBLE);
    }
    else
    {
      std::cout << "Warning, unexpected acquisition storage type in file\n";
      msg.setConfigType (CONFIGURATION_NONE);
    }
  }
  else
  {
      std::cout << "Warning, unexpected entity type in file\n";
      msg.setConfigType (CONFIGURATION_NONE);
  }

  _session->send (&msg);
  _session->send (&xmlHeader);

  if (stype == ISMRMRD_SHORT)
  {
    sendAcquisitions <int16_t> (dset, mtx);
  }
  else if (stype == ISMRMRD_INT)
  {
    sendAcquisitions <int32_t> (dset, mtx);
  }
  // TODO: FLOAT and CXFLOAT are treated the same here
  else if (stype == ISMRMRD_FLOAT ||
           stype == ISMRMRD_CXFLOAT)
  {
    sendAcquisitions <float> (dset, mtx);
  }
  // TODO: DOUBLE and CXDOUBLE are treated the same here 
  else if (stype == ISMRMRD_DOUBLE ||
           stype == ISMRMRD_CXDOUBLE)
  {
    sendAcquisitions <double> (dset, mtx);
  }
  else
  {
    std::cout << "Acq storage type = " << stype << "\n";
    throw std::runtime_error ("Unexpected MR Acquisition storage type");
  }
  
  Command cmd;
  cmd.setCommandId (0);
  cmd.setCommandType (ISMRMRD_COMMAND_STOP_FROM_CLIENT);
  _session->send (&cmd);

  std::cout << "Input completed\n";
}

/*******************************************************************************
 ******************************************************************************/
template <typename S>
void Client::sendAcquisitions
(
  Dataset&    dset,
  std::mutex& mtx
)
{
  uint32_t num_acq = dset.getNumberOfAcquisitions (0);
  for (int ii = 0; ii < num_acq; ii++)
  {
    usleep (500); // For testing only
    Acquisition<S> acq;
    {
      std::lock_guard<std::mutex> guard (mtx);
      acq = dset.readAcquisition<S> (ii, 0);
    }
    _session->send (&acq);
  }
}

/*******************************************************************************
 ******************************************************************************/
Client::Client
(
  SESSION          session,
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
  _out_dset        (out_dataset)
{

  std::cout << "\""<< _client_name << "\"  starting\n";

  using namespace std::placeholders;

  _callbacks.emplace_back (new ClientEntityHandler (this));
  auto fp = std::bind (&Callback::receive, _callbacks.back(), _1, _2);
  _session->registerHandler ((CB) fp, ISMRMRD_HANDSHAKE,    _callbacks.back());
  _session->registerHandler ((CB) fp, ISMRMRD_ERROR_REPORT, _callbacks.back());
  _session->registerHandler ((CB) fp, ISMRMRD_COMMAND,      _callbacks.back());


  _callbacks.emplace_back
    (new ClientImageProcessor (this, _out_fname, _out_dset, gmtx));
  auto fp1 = std::bind (&Callback::receive, _callbacks.back(), _1, _2);
  _session->registerHandler ((CB) fp1, ISMRMRD_IMAGE,  _callbacks.back());
  _session->registerHandler ((CB) fp1, ISMRMRD_HEADER, _callbacks.back());


  std::thread it (&Client::beginInput, this, std::ref (gmtx));
  _session->run ();

  if (it.joinable()) it.join();
}

/*******************************************************************************
 ******************************************************************************/
Client::~Client
(
)
{
  for (auto cb : _callbacks) if (cb) delete (cb);
}

/*******************************************************************************
 ******************************************************************************/
