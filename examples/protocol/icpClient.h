#include "icpSession.h"
#include "ismrmrd/ismrmrd.h"
#include "ismrmrd/dataset.h"
#include "ismrmrd/xml.h"
#include "fftw3.h"

/*******************************************************************************
 ******************************************************************************/
class icpClient
{
public:

       icpClient();
       //~icpClient();
  void run();
  
private:

  ICP_SESSION            _session;
  std::string            _client_name;
  std::string            _in_fname;
  std::string            _out_fname;
  std::string            _in_dset;
  std::string            _out_dset;
  bool                   _server_done;
  bool                   _task_done;

  bool                   _header_received; // for debug only
  ISMRMRD::IsmrmrdHeader _header;          // For debug only

  void sendCommand (ISMRMRD::CommandType cmd_type, uint32_t cmd_id);
  void sendHandshake ();
  void sendError (ISMRMRD::ErrorType type, std::string descr);
  void beginInput ();

  void handleImage (ISMRMRD::Image<float>  image, uint32_t index);
  void handleCommand (ISMRMRD::Command  msg, uint32_t index);
  void handleErrorNotification (ISMRMRD::ErrorNotification msg, uint32_t index);
  void handleIsmrmrdHeader (ISMRMRD::IsmrmrdHeader msg, uint32_t index);
  void handleHandshake (ISMRMRD::Handshake  msg, uint32_t index);
};
