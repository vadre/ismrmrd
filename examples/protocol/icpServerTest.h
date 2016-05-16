#include "icpServerTestData.h"
#include "icpServer.h"
#include "fftw3.h"

typedef ICPSERVERTESTDATA::icpServerTestData MY_DATA;

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
bool checkInfo (USER_DATA info, MY_DATA** im);
void sendCommand (MY_DATA* im, ISMRMRD::CommandType cmd_type, uint32_t cmd_id);
void handleAcquisition16  (ISMRMRD::Acquisition<int16_t> acq, USER_DATA info);
void handleAcquisition32  (ISMRMRD::Acquisition<int32_t> acq, USER_DATA info);
void handleAcquisitionFlt (ISMRMRD::Acquisition<float>   acq, USER_DATA info);
void handleAcquisitionDbl (ISMRMRD::Acquisition<double>  acq, USER_DATA info);
void handleCommand (ISMRMRD::Command  msg, USER_DATA info);
void handleErrorNotification (ISMRMRD::ErrorNotification msg, USER_DATA info);
void handleIsmrmrdHeader (ISMRMRD::IsmrmrdHeader msg, USER_DATA info);
void clientAccepted (MY_DATA* im, bool accepted);
void handleHandshake (ISMRMRD::Handshake  msg, USER_DATA info);
void sendError (MY_DATA* im, ISMRMRD::ErrorType type, std::string descr);
bool allocateData (ICP_SERVER_HANDLE handle, USER_DATA* ud, icpServer* server);
