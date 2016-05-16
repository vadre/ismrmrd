#include "icpServerTest.h"

/*******************************************************************************
 ******************************************************************************/
bool checkInfo
(
  USER_DATA       info,
  MY_DATA** md
)
{
  //std::cout << __func__ << ", USER_DATA* = " << info << "\n";
  if (!md)
  {
    throw std::runtime_error ("checkInfo ERROR! MY_DATA** is NULL\n");
  }
  if (!info)
  {
    throw std::runtime_error ("checkInfo ERROR! USER_DATA* is NULL\n");
  }

  MY_DATA* md_ptr = static_cast<MY_DATA*> (info);
  *md = md_ptr; // TODO: boundary check

  return true;
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
    case ISMRMRD::ISMRMRD_COMMAND_STOP_FROM_CLIENT:

      std::cout << __func__ << ": Received STOP  from client\n";
      md->setClientDone();
      break;

    case ISMRMRD::ISMRMRD_COMMAND_CLOSE_CONNECTION:

      std::cout << __func__ << ": Received CLOSE from client\n";
      md->server_ptr->closeConnection (md->server_handle);
      break;

    default:
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
void imageReconstruction
(
  std::vector<ISMRMRD::Acquisition<float> > acqs,
  uint32_t                                  encoding,
  MY_DATA*                                  md
)
{
  std::cout << __func__ << " starting\n";

  ISMRMRD::IsmrmrdHeader hdr = md->getIsmrmrdHeader();

  std::cout << "Received " << acqs.size() << " acquisitions\n";

  ISMRMRD::IsmrmrdHeaderWrapper wrapper (hdr);
  md->server_ptr->forwardMessage (md->server_handle,
                                  ISMRMRD::ISMRMRD_HEADER_WRAPPER,
                                  &wrapper);

  ISMRMRD::EncodingSpace e_space = hdr.encoding[encoding].encodedSpace;
  ISMRMRD::EncodingSpace r_space = hdr.encoding[encoding].reconSpace;

  if (e_space.matrixSize.z != 1)
  {
    std::cout <<
      "This reconstruction application only supports 2D encoding spaces" << '\n';
    // TODO: Report an error to client
    return;
  }

  uint16_t nX = e_space.matrixSize.x;
  uint16_t nY = e_space.matrixSize.y;


  uint32_t num_coils = acqs[0].getActiveChannels();

  std::vector<size_t> dims;
  dims.push_back (nX);
  dims.push_back (nY);
  dims.push_back (num_coils);
  ISMRMRD::NDArray<std::complex<float> > buffer (dims);

  //Now loop through and copy data
  unsigned int number_of_acquisitions = acqs.size();

  for (unsigned int ii = 0; ii < number_of_acquisitions; ii++)
  {
    //Read one acquisition at a time
    //ISMRMRD::Acquisition<float> acq = acqs [ii];

    //Copy data, we should probably be more careful here and do more tests....
    for (uint16_t coil = 0; coil < num_coils; coil++)
    {
      memcpy (&buffer.at (0, acqs[ii].getEncodingCounters().kspace_encode_step_1, coil),
              &acqs[ii].at (0, coil), sizeof (std::complex<float>) * nX);
    }
  }

  // Do the recon one slice at a time
  for (uint16_t coil = 0; coil < num_coils; coil++)
  {
    //Let's FFT the k-space to image (in-place)
    fftwf_complex* tmp =
      (fftwf_complex*) fftwf_malloc (sizeof (fftwf_complex) * (nX * nY));

    if (!tmp)
    {
      std::cout << "Error allocating temporary storage for FFTW" << std::endl;
      return;
    }

    //Create the FFTW plan
    fftwf_plan p = fftwf_plan_dft_2d (nY, nX, tmp ,tmp, FFTW_BACKWARD, FFTW_ESTIMATE);

    //FFTSHIFT
    fftshift (reinterpret_cast<std::complex<float>*>(tmp),
              &buffer.at (0, 0, coil), nX, nY);

    //Execute the FFT
    fftwf_execute(p);

    //FFTSHIFT
    fftshift (&buffer.at (0, 0, coil),
              reinterpret_cast<std::complex<float>*>(tmp), nX, nY);

    //Clean up.
    fftwf_destroy_plan(p);
    fftwf_free(tmp);
  }

  ISMRMRD::Image<float> img_out (r_space.matrixSize.x, r_space.matrixSize.y, 1, 1);

  //f there is oversampling in the readout direction remove it
  //Take the sqrt of the sum of squares
  uint16_t offset = ((e_space.matrixSize.x - r_space.matrixSize.x) / 2);
  for (uint16_t y = 0; y < r_space.matrixSize.y; y++)
  {
    for (uint16_t x = 0; x < r_space.matrixSize.x; x++)
    {
      for (uint16_t coil = 0; coil < num_coils; coil++)
      {
        img_out.at(x,y) += (std::abs (buffer.at (x + offset, y, coil))) *
                           (std::abs (buffer.at (x + offset, y, coil)));
      }

      img_out.at (x, y) = std::sqrt (img_out.at (x, y));
    }
  }

  // The following are extra guidance we can put in the image header
  img_out.setImageType (ISMRMRD::ISMRMRD_IMTYPE_MAGNITUDE);
  img_out.setSlice (0);
  img_out.setFieldOfView (r_space.fieldOfView_mm.x,
                          r_space.fieldOfView_mm.y,
                          r_space.fieldOfView_mm.z);

  md->server_ptr->forwardMessage (md->server_handle,
                                  ISMRMRD::ISMRMRD_IMAGE,
                                  &img_out);

  std::cout << __func__ << " done " << "\n";
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
  md->server_ptr->forwardMessage (md->server_handle,
                                  ISMRMRD::ISMRMRD_COMMAND,
                                  &msg);

  return;
}

/*****************************************************************************
 ****************************************************************************/
void clientAccepted
(
  MY_DATA* md,
  bool           accepted
)
{
  std::cout << __func__ << "\n";

  ISMRMRD::Handshake msg;
  msg.setTimestamp (md->getSessionTimestamp());
  msg.setConnectionStatus ((accepted) ?
                           ISMRMRD::CONNECTION_ACCEPTED :
  /* for now: */           ISMRMRD::CONNECTION_DENIED_UNKNOWN_USER);

  msg.setClientName (md->getClientName());
  md->server_ptr->forwardMessage (md->server_handle,
                                  ISMRMRD::ISMRMRD_HANDSHAKE,
                                  &msg);

  if (!accepted)
  {
    sendCommand (md, ISMRMRD::ISMRMRD_COMMAND_DONE_FROM_SERVER, 0);
  }

  return;
}

/*******************************************************************************
 ******************************************************************************/
void handleHandshake
(
  ISMRMRD::Handshake  msg,
  USER_DATA           info
)
{
  std::cout << __func__ << ": client <"<< msg.getClientName() << "> accepted\n";

  MY_DATA* md;
  if (!checkInfo (info, &md))
  {
    throw std::runtime_error ("handleIsmrmrdHeader: User data pointer invalid");
  }

  md->setClientName (msg.getClientName());
  md->setSessionTimestamp (msg.getTimestamp());

  // Could check if the client name is matching a list of expected clients
  clientAccepted (md, true);

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
  md->server_ptr->forwardMessage (md->server_handle,
                                  ISMRMRD::ISMRMRD_ERROR_NOTIFICATION,
                                  &msg);
  return;
}

/*******************************************************************************
 ******************************************************************************/
bool allocateData
(
  ICP_SERVER_HANDLE server_handle,
  USER_DATA*        data,
  icpServer*        server
)
{
  std::cout << __func__ << "\n";

  if (!data)
  {
    throw std::runtime_error ("allocateData: Invalid USER_DATA pointer");
  }

  std::cout <<__func__<<", UD: "<<data<< ", "<<*data<<", H: "<<server_handle<<"\n";
  MY_DATA* md = new MY_DATA();
  md->server_handle = server_handle;
  md->server_ptr    = server;
  *data = (USER_DATA) md;
  std::cout <<__func__<<", UD: "<<data<< ", "<<*data<<", H: "<<md->server_handle<<"\n";

  return true;
}

/*******************************************************************************
 ******************************************************************************/
/*bool setMessageSendCallback
(
  SEND_MSG_CALLBACK cb_func,
  ............
  ICPOUTPUTMANAGER::icpOutputManager& o,
  USER_DATA         info
)
{
  std::cout << __func__ << ", info " << info << ", func "
            << reinterpret_cast<void*>(cb_func) << "\n";

  MY_DATA* md;
  if (!checkInfo (info, &md))
  {
    throw std::runtime_error ("setMessageSendCallback: Invalid USER_DATA pointer");
  }

  if (!cb_func)
  {
    throw std::runtime_error ("setMessageSendCallback: NULL callback pointer");
  }

  md->setSendMessageCallback (cb_func);

  return true;
}
*/
/*******************************************************************************
 ******************************************************************************/
void handleAcquisition16
(
  ISMRMRD::Acquisition<int16_t> acq,
  USER_DATA info
)
{
  std::cout << __func__ << "\n";

  MY_DATA* md;

  if (!checkInfo (info, &md))
  {
    std::cout << __func__ << ": ERROR from checkInfo()\n";
    throw std::runtime_error ("User data pointer invalid");
    return;
  }

  uint32_t encoding = acq.getEncodingSpaceRef();
  std::vector<ISMRMRD::Acquisition<int16_t>> vec;

  md->acqs_16.addAcq (acq);
  if (md->readyForImageReconstruction (md->acqs_16.getSize(), encoding))
  {
    if (md->acqs_16.getAcqs (std::ref (vec)))
    {
      //imageReconstruction<int16_t> (vec, encoding, md);
      if (md->isClientDone())
      {
        sendCommand (md, ISMRMRD::ISMRMRD_COMMAND_DONE_FROM_SERVER, 0);
        std::cout << __func__ << ": sending DONE_FROM_SERVER\n";
      }
    }
    else
    {
      throw std::runtime_error ("Error: acq 16 vector empty");
    }
  }
}

/*******************************************************************************
 ******************************************************************************/
void handleAcquisition32
(
  ISMRMRD::Acquisition<int32_t> acq,
  USER_DATA info
)
{
  std::cout << __func__ << "\n";

  MY_DATA* md;

  if (!checkInfo (info, &md))
  {
    std::cout << __func__ << ": ERROR from checkInfo()\n";
    throw std::runtime_error ("User data pointer invalid");
    return;
  }

  uint32_t encoding = acq.getEncodingSpaceRef();
  std::vector<ISMRMRD::Acquisition<int32_t>> vec;

  md->acqs_32.addAcq (acq);
  if (md->readyForImageReconstruction (md->acqs_32.getSize(), encoding))
  {
    if (md->acqs_32.getAcqs (std::ref (vec)))
    {
      //TODO: Template not working - need to check the NDArray 
      //imageReconstruction<int32_t> (vec, encoding, md);
      if (md->isClientDone())
      {
        sendCommand (md, ISMRMRD::ISMRMRD_COMMAND_DONE_FROM_SERVER, 0);
        std::cout << __func__ << ": sending DONE_FROM_SERVER\n";
      }
    }
    else
    {
      throw std::runtime_error ("Error: acq 32 vector empty");
    }
  }
}

/*******************************************************************************
 ******************************************************************************/
void handleAcquisitionFlt
(
  ISMRMRD::Acquisition<float> acq,
  USER_DATA info
)
{
  //std::cout << __func__ << "\n";

  MY_DATA* md;

  if (!checkInfo (info, &md))
  {
    std::cout << __func__ << ": ERROR from checkInfo()\n";
    throw std::runtime_error ("User data pointer invalid");
    return;
  }

  uint32_t encoding = acq.getEncodingSpaceRef();
  std::vector<ISMRMRD::Acquisition<float>> vec;

  md->acqs_flt.addAcq (acq);
  if (md->readyForImageReconstruction (md->acqs_flt.getSize(), encoding))
  {
    if (md->acqs_flt.getAcqs (std::ref (vec)))
    {
      //TODO: Template not working - need to check the NDArray 
      imageReconstruction (vec, encoding, md);
      if (md->isClientDone())
      {
        sendCommand (md, ISMRMRD::ISMRMRD_COMMAND_DONE_FROM_SERVER, 0);
        std::cout << __func__ << ": sending DONE_FROM_SERVER\n";
      }
    }
    else
    {
      throw std::runtime_error ("Error: acq float vector empty");
    }
  }
}

/*******************************************************************************
 ******************************************************************************/
void handleAcquisitionDbl
(
  ISMRMRD::Acquisition<double> acq,
  USER_DATA info
)
{
  std::cout << __func__ << "\n";

  MY_DATA* md;
  if (!checkInfo (info, &md))
  {
    std::cout << __func__ << ": ERROR from checkInfo()\n";
    throw std::runtime_error ("User data pointer invalid");
    return;
  }

  uint32_t encoding = acq.getEncodingSpaceRef();
  std::vector<ISMRMRD::Acquisition<double>> vec;

  md->acqs_dbl.addAcq (acq);
  if (md->readyForImageReconstruction (md->acqs_dbl.getSize(), encoding))
  {
    if (md->acqs_dbl.getAcqs (std::ref (vec)))
    {
      //TODO: Template not working - need to check the NDArray 
      //imageReconstruction<double> (vec, encoding, md);//Template not linking 
      if (md->isClientDone()) //TODO: Check if more data for other tasks received
      {
        sendCommand (md, ISMRMRD::ISMRMRD_COMMAND_DONE_FROM_SERVER, 0);
        std::cout << __func__ << ": sending DONE_FROM_SERVER\n";
      }
    }
    else
    {
      throw std::runtime_error ("Error: acq double vector empty");
    }
  }
}

/*******************************************************************************
 ******************************************************************************/
int main
(
  int argc,
  char* argv[]
)
{
  unsigned int port;

  if (argc != 2)
  {
    port = 50050;
  }
  else
  {
    port = std::atoi (argv[1]);
  }

  std::cout << "ISMRMRD Processor app starts with port number " << port << '\n';
  std::cout << "To connect to a different port, restart: icpDisp <port>\n\n";

  icpServer server (port);
  
  server.registerUserDataAllocator    ((icpServer::GET_USER_DATA_FUNC) &allocateData);
//  server.registerCallbackSetter       ((SET_SEND_CALLBACK_FUNC) &setMessageSendCallback);

  server.registerHandler              ((CB_HANDSHK) handleHandshake,
                                       ISMRMRD::ISMRMRD_HANDSHAKE,
                                       ISMRMRD::ISMRMRD_CHAR);

  server.registerHandler              ((CB_ERRNOTE) handleErrorNotification,
                                       ISMRMRD::ISMRMRD_ERROR_NOTIFICATION,
                                       ISMRMRD::ISMRMRD_CHAR);

  server.registerHandler              ((CB_COMMAND) handleCommand,
                                       ISMRMRD::ISMRMRD_COMMAND,
                                       ISMRMRD::ISMRMRD_CHAR);

  server.registerHandler              ((CB_XMLHEAD) handleIsmrmrdHeader,
                                       ISMRMRD::ISMRMRD_HEADER,
                                       ISMRMRD::ISMRMRD_CHAR);

  server.registerHandler              ((CB_ACQ_16) handleAcquisition16,
                                       ISMRMRD::ISMRMRD_MRACQUISITION,
                                       ISMRMRD::ISMRMRD_SHORT);

  server.registerHandler              ((CB_ACQ_32) handleAcquisition32,
                                       ISMRMRD::ISMRMRD_MRACQUISITION,
                                       ISMRMRD::ISMRMRD_INT);

  server.registerHandler              ((CB_ACQ_FLT) handleAcquisitionFlt,
                                       ISMRMRD::ISMRMRD_MRACQUISITION,
                                       ISMRMRD::ISMRMRD_FLOAT);

  server.registerHandler              ((CB_ACQ_DBL) handleAcquisitionDbl,
                                       ISMRMRD::ISMRMRD_MRACQUISITION,
                                       ISMRMRD::ISMRMRD_DOUBLE);

  server.start();

  return 0;
}
