
#include "icpServer.h"
#include "fftw3.h"

typedef ICPINPUTMANAGER::icpInputManager INPUT_MANAGER;

/*******************************************************************************
 ******************************************************************************/
template <typename T>
void circshift(T *out, const T *in, int xdim, int ydim, int xshift, int yshift)
{
  for (int i =0; i < ydim; i++)
  {
    int ii = (i + yshift) % ydim;
    for (int j = 0; j < xdim; j++)
    {
      int jj = (j + xshift) % xdim;
      out[ii * xdim + jj] = in[i * xdim + j];
    }
  }
}

#define fftshift(out, in, x, y) circshift(out, in, x, y, (x/2), (y/2))
/*******************************************************************************
 ******************************************************************************/
void handleCommand
(
  ISMRMRD::Command  msg,
  USER_DATA         info
)
{
  switch (msg.cmd_type)
  {
    case ISMRMRD::ISMRMRD_COMMAND_STOP_FROM_CLIENT:

      std::cout << __func__ << ": Received STOP  from client\n";
      im->setClientDone();
      break;

    default:
      break;
  }
  return;
}

/*******************************************************************************
 ******************************************************************************/
void handleError
(
  ISMRMRD::Error    msg,
  USER_DATA         info
)
{
  switch (msg.error_type)
  {

    default:
      std::cout << ": Error from client: " << msg.description << "\n";
      break;
  }
  return;
}

/*******************************************************************************
 ******************************************************************************/
template <typename T>
bool handleMrAcquisition
(
  ISMRMRD::Acqusition<T> acq,
  USER_DATA              info,
)
{
  INPUT_MANAGER* im;
  bool ret_val = checkInfo (info, &im);

  if (!ret_val)
  {
    std::cout << __func__ << ": ERROR from checkInfo\n";
    return ret_val;
  }

  if (!im->addMrAcquisition (acq))
  {
    std::cout << __func__ << ": ERROR from INPUT_MANAGER->addAcquisition\n";
  }
  else if (im->readyForImageReconstruction())
  {
    imageReconstruction (im);
  }
  
  return ret_val;
}

/*******************************************************************************
 ******************************************************************************/
template <typename T>
void imageReconstruction
(
  INPUT_MANAGER*         im,
)
{
  std::cout << __func__ << " starting\n";

  ISMRMRD::IsmrmrdHeader hdr = im->getIsmrmrdHeader();
  std::vector<ISMRMRD::Acquisition<T> > acqs = im->getMrAcquisitions();

  std::cout << "Received " << acqs.size() << " acquisitions\n";

  im->sendMessage (hdr);

  ISMRMRD::EncodingSpace e_space = hdr.encoding[0].encodedSpace;
  ISMRMRD::EncodingSpace r_space = hdr.encoding[0].reconSpace;

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
  ISMRMRD::NDArray<std::complex<T> > buffer (dims);

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
              &acqs[ii].at (0, coil), sizeof (std::complex<T>) * nX);
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
    fftshift (reinterpret_cast<std::complex<T>*>(tmp),
              &buffer.at (0, 0, coil), nX, nY);

    //Execute the FFT
    fftwf_execute(p);

    //FFTSHIFT
    fftshift (&buffer.at (0, 0, coil),
              reinterpret_cast<std::complex<T>*>(tmp), nX, nY);

    //Clean up.
    fftwf_destroy_plan(p);
    fftwf_free(tmp);
  }

  ISMRMRD::Image<T> img_out (r_space.matrixSize.x, r_space.matrixSize.y, 1, 1);

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

  im->sendMessage (img_out);

  std::cout << __func__ << " done " << "\n";
  return;
}

/*******************************************************************************
 ******************************************************************************/
handleIsmrmrdHeader
(
  ISMRMRD::IsmrmrdHeader  msg,
  USER_DATA               info
)
{
  std::cout << __func__ << ": Received IsmrmrdHeader\n";

  INPUT_MANAGER* im;
  bool           ret_val = checkInfo (info, &im);

  if (!ret_val)
  {
    std::cout << __func__ << ": ERROR from checkInfo()\n";
    return ret_val;
  }

  im->addIsmrmrdHeader (hdr);

  return ret_val;
}

/*******************************************************************************
 ******************************************************************************/
bool handleHandshake
(
  ISMRMRD::Handshake  msg,
  USER_DATA           info
)
{
  std::cout << __func__ << ": client <"<< msg.name << "> accepted\n";

  INPUT_MANAGER* im;
  bool           ret_val = checkInfo (info, &im);

  if (!ret_val)
  {
    std::cout << __func__ << ": ERROR from checkInfo()\n";
    return ret_val;
  }

  im->setClientName (msg.name);
  im->setSessionTimestamp (msg.timestamp);

  // Could check if the client name is matching a list of expected clients
  return clientAccepted (im, true);
}

/*****************************************************************************
 ****************************************************************************/
bool clientAccepted
(
  INPUT_MANAGER* im,
  bool           accepted
)
{
  std::cout << __func__ << "\n";

  ISMRMRD::Handshake msg;
  msg.timestamp = _session_timestamp;
  msg.conn_status = (accepted) ?
                    ISMRMRD::CONNECTION_ACCEPTED :
                    ISMRMRD::CONNECTION_DENIED_UNKNOWN_USER;

  strncpy (handshake.client_name,
           _client_name,
           ISMRMRD::MAX_CLIENT_NAME_LENGTH);

  im->sendMessage (msg);

  if (!accepted)
  {
    sendCommand (im, ISMRMRD::ISMRMRD_COMMAND_DONE_FROM_SERVER);
  }

  return;
}

/*****************************************************************************
 ****************************************************************************/
void sendCommand
(
  INPUT_MANAGER*       im,
  ISMRMRD::CommandType cmd_type
)
{
  ISMRMRD::Command   msg;
  msg.command_type = cmd_type;
  msg.command_id   = 0;

  im->sendMessage (msg);

  return;
}

/*****************************************************************************
 ****************************************************************************/
void sendError
(
  INPUT_MANAGER*  im,
  ISMRMRD::ErrorType type,
  std::string        descr
)
{
  ISMRMRD::Error     msg;
  msg.error_type        = type;
  msg.error_description = descr;

  im->sendMessage (msg);

  return;
}

/*******************************************************************************
 ******************************************************************************/
bool allocateData
(
  USER_DATA*  data,
)
{
  std::cout << __func__ << "\n";
  bool ret_val = false;

  if (data)
  {
    INPUT_MANAGER* im = new INPUT_MANAGER();

    *data = (void*) im;
    ret_val = true;
  }
  else
  {
    std::cout << __func__ << ": ERROR! Invalid USER_DATA pointer\n";
  }

  return ret_val;
}

/*******************************************************************************
 ******************************************************************************/

bool checkInfo
(
  USED_DATA       info,
  INPUT_MANAGER** im
)
{
  bool ret_val = false;

  if (!im_ptr)
  {
    std::cout << __func__ << ": ERROR! Info pointer invalid\n";
    return ret_val;
  }

  INPUT_MANAGER* im_ptr = static_cast<INPUT_MANAGER> (info);
  if (im_ptr->checkBounds ("ICP_UPPER_BOUND", "ICP_LOWER_BOUND"))
  {
    std::cout << __func__ << ": ERROR! Info data invalid\n";
  }
  else
  {
    *im = im_ptr;
    ret_val = true;
  }

  return ret_val;
}

/*******************************************************************************
 ******************************************************************************/
bool setMessageSendCallback
(
  SEND_MSG_CALLBACK cb_func,
  USER_DATA         info
)
{
  std::cout << __func__ << "\n";

  INPUT_MANAGER* im;
  bool           ret_val = checkInfo (info, &im);

  if (!retval)
  {
    std::cout << __func__ << ": ERROR from checkInfo()\n";
  }
  else if (!cb_func)
  {
    std::cout << __func__ << ": Callback pointer invalid\n";
    ret_val = false;
  }
  else
  }
    im->setSendMessageCallback (cb_func);
  }

  return ret_val;
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
  
  server.registerUserDataAllocator    (&allocateData);
  server.registerCallbackSetter       (&setMessageSendCallback);
  server.registerHandshakeHandler     (&handleHandshake);
  server.registerCommandHandler       (&handleCommand);
  server.registerErrorHandler         (&handleError);
  server.registerIsmrmrdHeaderHandler (&handleIsmrmrdHeader);
  server.registerMrAcquisitionHandler (&handleMrAcquisition);

  server.start();

  return 0;
}
