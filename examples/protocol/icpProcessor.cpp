
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
  INPUT_MANAGER*    im
)
{
  switch (msg.getCommandType())
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
void handleErrorNotification
(
  ISMRMRD::ErrorNotification msg,
  INPUT_MANAGER*             im
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
template <typename T>
bool handleMrAcquisition
(
  ISMRMRD::Acquisition<T> acq,
  INPUT_MANAGER*         im
)
{
  if (!im->addMrAcquisition (acq))
  {
    std::cout << __func__ << ": ERROR from INPUT_MANAGER->addAcquisition\n";
  }
  else if (im->readyForImageReconstruction())
  {
    imageReconstruction (im);
    if (im->isClientDone())
    {
      sendCommand (ISMRMRD::ISMRMRD_COMMAND_DONE_FROM_SERVER);
      std::cout << __func__ << ": sending DONE_FROM_SERVER\n";
    }
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

  if (ret_val)
  {
    im->setClientName (msg.name);
    im->setSessionTimestamp (msg.timestamp);

    // Could check if the client name is matching a list of expected clients
    clientAccepted (im, true);
  }
  else
  {
    std::cout << __func__ << ": ERROR from checkInfo()\n";
  }

  return ret_val;
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
  msg.setTimestamp (im->getSessionTimestamp());
  msg.stConnectionStatus ((accepted) ?
                          ISMRMRD::CONNECTION_ACCEPTED :
  /* for now: */          ISMRMRD::CONNECTION_DENIED_UNKNOWN_USER);

  msg.setClientName (im->getClientName);
  im->sendMessage (msg);

  if (!accepted)
  {
    sendCommand (im, ISMRMRD::ISMRMRD_COMMAND_DONE_FROM_SERVER, 0);
  }

  return;
}

/*****************************************************************************
 ****************************************************************************/
void sendCommand
(
  INPUT_MANAGER*       im,
  ISMRMRD::CommandType cmd_type,
  uint32_t             cmd_id
)
{
  ISMRMRD::Command msg;
  msg.setCommandType (cmd_type);
  msg.setCommandId (cmd_id);
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
  ISMRMRD::ErrorNotification msg;
  msg.setErrorType (type);
  msg.setDescription (descr);
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

    *data = (USER_DATA) im;
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
  {
    im->setSendMessageCallback (cb_func);
  }

  return ret_val;
}

/*******************************************************************************
 ******************************************************************************/
bool entityReceive
(
  ISMRMRD::Entity*     ent,
  ISMRMRD::EntityType  type,
  ISMRMRD::StorageType storage,
  USER_DATA            info
)
{
  std::cout << __func__ << "\n";

  INPUT_MANAGER* im;
  bool           ret_val = checkInfo (info, &im);

  if (!retval)
  {
    std::cout << __func__ << ": ERROR from checkInfo()\n";
  }
  else if (!ent)
  {
    std::cout << __func__ << ": Entity pointer NULL\n";
    ret_val = false;
  }
  else
  {
    switch (type)
    {
      case ISMRMRD::ISMRMRD_MRACQUISITION:
        switch (storage)
        {
          case ISMRMRD::ISMRMRD_FLOAT:

            handleMrAcquisition<float>
              (static_cast<ISMRMRD::Acquisition<float>* >(ent), im);
            break;

          case ISMRMRD::ISMRMRD_DOUBLE:

            handleMrAcquisition<double>
              (static_cast<ISMRMRD::Acquisition<double>* >(ent), im);
            break;

          default:

            std::cout << "Warning! Unexpected storage type: " << storage 
                      << ", dropping...\n";
            break;
        }
        break;

      case ISMRMRD::ISMRMRD_COMMAND:

        handleCommand (static_cast<ISMRMRD::Command*>(ent), im);
        break;

      case ISMRMRD::ISMRMRD_HANDSHAKE:

        handleHandshake (static_cast<ISMRMRD::Handshake*>(ent), im);
        break;

      case ISMRMRD::ISMRMRD_ERRORNOTIFICATION:

        handleErrorNotification
          (static_cast<ISMRMRD::ErrorNotification*>(ent), im);
        break;

      case ISMRMRD::ISMRMRD_ISMRMRDHEADERWRAPPER:

        handleIsmrmrdHeader
          (static_cast<ISMRMRD::IsmrmrdHeaderWrapper*>(ent)->getHeader(), im);
        break;

      default:

        std::cout << "Unexpected entity type: " << type << ", ignoring...\n"
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
  
  server.registerUserDataAllocator    (&allocateData);
  server.registerCallbackSetter       (&setMessageSendCallback);
  server.registerMessageReceiver      (&entityReceive);

  server.start();

  return 0;
}
