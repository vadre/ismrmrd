// clientTest.cpp
#include "icpClientTest.h"
#include "ismrmrd/ismrmrd.h"
#include "ismrmrd/dataset.h"
#include "ismrmrd/xml.h"
#include <boost/program_options.hpp>

namespace po = boost::program_options;

/*
icpClient*  g_client_ptr;
std::string g_client_name;
std::string g_in_fname;
std::string g_out_fname;
std::string g_in_dset;
std::string g_out_dset;
*/

/*******************************************************************************
 ******************************************************************************/
bool allocateData
(
  USER_DATA*        data,
  icpClient*        client
)
{
  if (!data)
  {
    throw std::runtime_error ("allocateData: Invalid USER_DATA pointer");
  }

  MY_DATA* md = new MY_DATA();
  md->setClientName (g_client_name);
  md->setInputFileName (g_in_fname);
  md->setInputDataset (g_in_dset);
  md->setOutputFileName (g_out_fname);
  md->setOutputDataset (g_out_dset);
  g_client_ptr    = client;
  *data = (USER_DATA) md;

  return true;
}

/*******************************************************************************
 ******************************************************************************/
bool checkInfo
(
  USER_DATA       info,
  MY_DATA** md
)
{
  bool ret_val = false;

  if (!md)
  {
    std::cout << __func__ << ": ERROR! InputManager double pointer invalid\n";
    return ret_val;
  }

  if (!info)
  {
    std::cout << __func__ << ": ERROR! USER_DATA pointer invalid\n";
    return ret_val;
  }

  MY_DATA* md_ptr = static_cast<MY_DATA*> (info);
  // TODO: boundary check
  *md = md_ptr;
  ret_val = true;

  return ret_val;
}

/*******************************************************************************
 ******************************************************************************/
void handleCommand
(
  ISMRMRD::Command  msg,
  USER_DATA         info
)
{
  MY_DATA* md;
  if (!checkInfo (info, &md))
  {
    throw std::runtime_error ("handleCommand: User data pointer invalid");
  }

  switch (msg.getCommandType())
  {
    case ISMRMRD::ISMRMRD_COMMAND_DONE_FROM_SERVER:

      std::cout << __func__ << ": Received DONE from server\n";
      md->setServerDone();
      sendCommand (md, ISMRMRD::ISMRMRD_COMMAND_CLOSE_CONNECTION, 0);
      md->setSessionClosed();
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
void handleErrorNotification
(
  ISMRMRD::ErrorNotification msg,
  USER_DATA                  info
)
{
  MY_DATA* md;
  if (!checkInfo (info, &md))
  {
    throw std::runtime_error ("handleCommand: User data pointer invalid");
  }

  std::cout << __func__ <<":\nType: " << msg.getErrorType()
            << ", Error Command: " << msg.getErrorCommandType()
            << ", Error Command ID: " << msg.getErrorCommandId()
            << ", Error Entity: " << msg.getErrorEntityType()
            << ",\nError Description: " << msg.getErrorDescription() << "\n";
  return;
}

/*******************************************************************************
 ******************************************************************************/
void handleIsmrmrdHeader
(
  ISMRMRD::IsmrmrdHeader  msg,
  USER_DATA               info
)
{
  std::cout << __func__ << ": Received IsmrmrdHeader\n";

  MY_DATA* md;
  if (!checkInfo (info, &md))
  {
    throw std::runtime_error ("handleIsmrmrdHeader: User data pointer invalid");
  }

  md->addIsmrmrdHeader (msg);

  return;
}

/*****************************************************************************
 ****************************************************************************/
void sendCommand
(
  MY_DATA*       md,
  ISMRMRD::CommandType cmd_type,
  uint32_t             cmd_id
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
void handleHandshake
(
  ISMRMRD::Handshake  msg,
  USER_DATA           info
)
{
  MY_DATA* md;
  if (!checkInfo (info, &md))
  {
    throw std::runtime_error ("handleHandshake: User data pointer invalid");
  }
  // Could check if the client name is as expected
  return;
}

/*****************************************************************************
 ****************************************************************************/
void sendError
(
  MY_DATA*  md,
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
/*
bool setMessageSendCallback
(
  SEND_MSG_CALLBACK cb_func,
  USER_DATA         info
)
{
  std::cout << __func__ << "\n";

  MY_DATA* md;
  if (!checkInfo (info, &md))
  {
    throw std::runtime_error ("setMessageSendCallback: Invalid USER_DATA pointer");
  }

  if (!cb_func)
  {
    throw std::runtime_error ("setMessageSendCallback: NULL callback pointer");
  }

  md->sendMessage = cb_func;

  return true;
}
*/

/*******************************************************************************
 ******************************************************************************/

void handleImageFlt
(
  ISMRMRD::Image<float> image,
  USER_DATA             info
)
{
  std::cout << __func__ << "\n";

  MY_DATA* md;
  if (!checkInfo (info, &md))
  {
    std::cout << __func__ << ": ERROR from checkInfo()\n";
    throw std::runtime_error ("User data pointer invalid");
  }

  ISMRMRD::EncodingSpace r_space = md->getIsmrmrdHeader().encoding[0].reconSpace;
  ISMRMRD::Image<float> img (r_space.matrixSize.x,
                             r_space.matrixSize.y,
                             r_space.matrixSize.z,
                             1);

  ISMRMRD::Dataset dset (md->getOutputFileName().c_str(),
                         md->getOutputDataset().c_str());
  dset.writeHeader (md->getIsmrmrdHeaderSerialized());

  // TODO:  appendImage is not yet implemented for version 2.x
  dset.appendImage ("cpp", img);
  std::cout << "\nFinished processing the data - exiting" << std::endl;

  return;
}

/*******************************************************************************
 ******************************************************************************/
void sendHandshake (MY_DATA* md)
{
  ISMRMRD::Handshake msg;
  msg.setTimestamp ((uint64_t)std::time(nullptr));
  msg.setConnectionStatus (ISMRMRD::CONNECTION_REQUEST);
  msg.setClientName (md->getClientName());
  g_client_ptr->forwardMessage (ISMRMRD::ISMRMRD_HANDSHAKE, &msg);

  return;
}

/*******************************************************************************
 ******************************************************************************/
void beginInput
(
  USER_DATA   info
)
{
  MY_DATA* md;
  if (!checkInfo (info, &md))
  {
    throw std::runtime_error ("beginInput: User data pointer invalid");
  }

  sendHandshake (md);

  ISMRMRD::Dataset dset (md->getInputFileName().c_str(),
                         md->getInputDataset().c_str());
  std::string xml_head = dset.readHeader();

  ISMRMRD::IsmrmrdHeader xmlHeader;
  ISMRMRD::deserialize (xml_head.c_str(), xmlHeader);
  ISMRMRD::IsmrmrdHeaderWrapper wrapper (xmlHeader);
  g_client_ptr->forwardMessage (ISMRMRD::ISMRMRD_HEADER_WRAPPER, &wrapper); 

  int num_acq = dset.getNumberOfAcquisitions (0);
  //TODO: Per storage type: int16_t, int32_t, float, and stream
  for (int ii = 0; ii < num_acq; ii++)
  {
    ISMRMRD::Acquisition<float> acq = dset.readAcquisition<float> (ii, 0);
    g_client_ptr->forwardMessage (ISMRMRD::ISMRMRD_MRACQUISITION, &acq);
  }

  ISMRMRD::Command cmd;
  cmd.setCommandId (2);
  cmd.setCommandType (ISMRMRD::ISMRMRD_COMMAND_STOP_FROM_CLIENT);
  g_client_ptr->forwardMessage (ISMRMRD::ISMRMRD_COMMAND, &cmd);

  //std::cout << "Sent " << num_acq << " acquisitions\n";
  std::cout << "Finished queueing data and sent STOP_FROM_CLIENT command\n";
  return;
}

/*******************************************************************************
 ******************************************************************************/
int main
(
  int argc,
  char* argv[]
)
{
  std::string       client_name = "Client 1";
  std::string       host        = "127.0.0.1";
  std::string       in_fname    = "testdata.h5";
  std::string       in_dset     = "dataset";
  std::string       out_fname   = "FileOut.h5";
  std::string       out_dset    = "dataset";
  unsigned short    port        = 50050;

  struct timespec   ts;
  ts.tv_sec                     = 0;
  ts.tv_nsec                    = 10000000;


  po::options_description desc("Allowed options");
  desc.add_options()
    ("help,h",
     "produce help message")
    ("client name,c",
     po::value<std::string>(&client_name)->default_value(client_name),
     "Client Name")
    ("host,H",
     po::value<std::string>(&host)->default_value(host),
     "Server IP address")
    ("port,p",
     po::value<unsigned short>(&port)->default_value(port),
     "TCP port for server")
    ("fin,i",
     po::value<std::string>(&in_fname)->default_value(in_fname),
     "HDF5 Input file")
    ("in_group,I",
     po::value<std::string>(&in_dset)->default_value(in_dset),
     "Input group name")
    ("fout,o",
     po::value<std::string>(&out_fname)->default_value(out_fname),
     "Output file name")
    ("out_group,O",
     po::value<std::string>(&out_dset)->default_value(out_dset),
     "Output group name")
  ;

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help"))
  {
    std::cout << desc << "\n";
    return 0;
  }

  std::cout << "Using client name <" << client_name << ">, host IP address <"
            << host << ">, and port <" << port << ">" << std::endl;
  std::cout << "Re-start with: icpClient -h to see all options\n" << std::endl;

  g_client_name = client_name;
  g_in_fname = in_fname;
  g_in_dset = in_dset;
  g_out_fname = out_fname;
  g_out_dset = out_dset;

  icpClient client (host, port);

  client.registerUserDataAllocator ((icpClient::GET_USER_DATA_FUNC) &allocateData);
  //client.registerCallbackSetter  ((icpClient::SET_SEND_CALLBACK_FUNC)
                                   //&setMessageSendCallback);

  client.registerInputProvider   ((BEGIN_INPUT_CALLBACK_FUNC) &beginInput);

  client.registerHandler         ((CB_HANDSHK) handleHandshake,
                                  ISMRMRD::ISMRMRD_HANDSHAKE,
                                  ISMRMRD::ISMRMRD_CHAR);

  client.registerHandler         ((CB_ERRNOTE) handleErrorNotification,
                                  ISMRMRD::ISMRMRD_ERROR_NOTIFICATION,
                                  ISMRMRD::ISMRMRD_CHAR);

  client.registerHandler         ((CB_COMMAND) handleCommand,
                                  ISMRMRD::ISMRMRD_COMMAND,
                                  ISMRMRD::ISMRMRD_CHAR);

  client.registerHandler         ((CB_XMLHEAD) handleIsmrmrdHeader,
                                  ISMRMRD::ISMRMRD_HEADER,
                                  ISMRMRD::ISMRMRD_CHAR);

  client.registerHandler         ((CB_IMG_FLT) handleImageFlt,
                                  ISMRMRD::ISMRMRD_IMAGE,
                                  ISMRMRD::ISMRMRD_FLOAT);
  client.connect ();

  return 0;
}
