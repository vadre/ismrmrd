#include "icpServer.h"
#include "fftw3.h"

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
  uint32_t                            cmd_type,
  uint32_t                            cmd_id,
  ICPOUTPUTMANAGER::icpOutputManager* om
)
{
  switch (cmd_type)
  {
    case ISMRMRD::ISMRMRD_COMMAND_STOP_FROM_CLIENT:

      // Up to handler
      std::cout << __func__ << ": Received STOP  from client\n";
      break;

    default:
      break;
  }
  return;
}

/*******************************************************************************
 ******************************************************************************/
void handleImageReconstruction
(
  ISMRMRD::IsmrmrdHeader                     hdr,
  std::vector<ISMRMRD::Acquisition<float> >  acqs,
  ICPOUTPUTMANAGER::icpOutputManager*        om
)
{
  std::cout << __func__ << " received " << acqs.size() << " acquisitions\n";

  om->sendXmlHeader (hdr);

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

  om->sendImage (img_out);

  std::cout << __func__ << " done " << "\n";
  return;
}

/*******************************************************************************
 ******************************************************************************/
void handleAuthentication
(
  std::string  name,
  ICPOUTPUTMANAGER::icpOutputManager* om
)
{
  std::cout << __func__ << ": client <"<< name << "> accepted\n";
  om->clientAccepted (true);
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
  
  server.register_authentication_handler (&handleAuthentication);
  server.register_command_handler (&handleCommand);
  server.register_image_reconstruction_handler (&handleImageReconstruction);

  server.start();

  return 0;
}
