#include "icpClient.h"
#include "icpClientTestData.h"
#include "fftw3.h"

typedef ICPCLIENTTESTDATA::icpClientTestData MY_DATA;

/*******************************************************************************
 ******************************************************************************/

icpClient*  g_client_ptr;
std::string g_client_name;
std::string g_in_fname;
std::string g_out_fname;
std::string g_in_dset;
std::string g_out_dset;

bool checkInfo (USER_DATA info, MY_DATA** md);
void sendCommand (MY_DATA* md, ISMRMRD::CommandType cmd_type, uint32_t cmd_id);
void sendHandshake (MY_DATA* md);
void handleImage (ISMRMRD::Image<float>  image, USER_DATA info);
void handleCommand (ISMRMRD::Command  msg, USER_DATA info);
void handleErrorNotification (ISMRMRD::ErrorNotification msg, USER_DATA info);
void handleIsmrmrdHeader (ISMRMRD::IsmrmrdHeader msg, USER_DATA info);
void handleHandshake (ISMRMRD::Handshake  msg, USER_DATA info);
void sendError (MY_DATA* md, ISMRMRD::ErrorType type, std::string descr);
void beginInput (USER_DATA data);
bool allocateData (USER_DATA* data, icpClient* client_ptr);
