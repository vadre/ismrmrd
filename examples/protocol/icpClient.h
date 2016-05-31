#include "icpSession.h"
#include "ismrmrd/ismrmrd.h"
#include "ismrmrd/dataset.h"
#include "ismrmrd/xml.h"
#include "fftw3.h"

using ICP_SESSION = std::shared_ptr<icpSession>;
/*******************************************************************************
 ******************************************************************************/
class icpClient
{
public:

       icpClient  (ICP_SESSION      session,
                   std::string      client_name,
                   std::string      in_fname,
                   std::string      out_fname,
                   std::string      in_dataset,
                   std::string      out_dataset);
       //~icpClient ();
  void run        ();
  
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
  //std::thread            _input_thread;
  ISMRMRD::IsmrmrdHeader _header;          // For debug only

  void sendCommand (ISMRMRD::CommandType cmd_type, uint32_t cmd_id);
  void sendHandshake ();
  void sendError (ISMRMRD::ErrorType type, std::string descr);
  void writeImage (ISMRMRD::Dataset& dset, ISMRMRD::Entity* ent, uint32_t strg);
  //void sendAcquisitions (ISMRMRD::Dataset& dset, ISMRMRD::StorageType storage);
  void sendAcquisitions (ISMRMRD::Dataset& dset, uint32_t storage);

  void beginInput ();

  void handleImage             (ISMRMRD::Entity* ent, uint32_t storage);
  void handleCommand           (ISMRMRD::Command msg);
  void handleErrorNotification (ISMRMRD::ErrorNotification msg);
  void handleIsmrmrdHeader     (ISMRMRD::IsmrmrdHeader msg);
  void handleHandshake         (ISMRMRD::Handshake msg);
};
