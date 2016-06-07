#include "icpSession.h"
#include "icpUserAppBase.h"
#include "ismrmrd/ismrmrd.h"
#include "ismrmrd/dataset.h"
#include "ismrmrd/xml.h"
#include "fftw3.h"

/*******************************************************************************
 ******************************************************************************/
class icpClient : public icpUserAppBase
{
public:

       icpClient          (std::string      client_name,
                           std::string      in_fname,
                           std::string      out_fname,
                           std::string      in_dataset,
                           std::string      out_dataset);
       //~icpClient         ();

  void saveSessionPointer      (icpSession* session);
  void handleImage             (ENTITY* ent, STYPE stype);
  void handleCommand           (COMMAND msg);
  void handleErrorNotification (ERRNOTE msg);
  void handleIsmrmrdHeader     (XMLHEAD msg);
  void handleHandshake         (HANDSHAKE msg);
  
private:


  void beginInput       ();
  void sendHandshake    ();
  void sendCommand      (ISMRMRD::CommandType cmd_type, uint32_t cmd_id);
  void sendError        (ISMRMRD::ErrorType type, std::string descr);
  void writeImage       (ISMRMRD::Dataset& dset, ENTITY* ent, STYPE stype);
  template <typename S>
  void sendAcquisitions (ISMRMRD::Dataset& dset);

  icpSession*            _session;
  std::string            _client_name;
  std::string            _in_fname;
  std::string            _out_fname;
  std::string            _in_dset;
  std::string            _out_dset;
  bool                   _server_done;
  bool                   _task_done;

  bool                   _header_received; // for debug only
  ISMRMRD::IsmrmrdHeader _header;          // For debug only
};
