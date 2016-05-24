#include "icpSession.h"
#include "icpImageReconOne.h"
#include "icpImageReconTwo.h"
#include "ismrmrd/ismrmrd.h"
#include "ismrmrd/xml.h"

/*******************************************************************************
 ******************************************************************************/
Class icpServer
{
public:
       icpServer  (SOCKET_PTR sock, uint32_t id); // id is for debug only
       ~icpServer ();
  void run        ();


private:

  ICP_SESSION       _session;
  imageReconBase*   _imrec;

  bool              _client_done;
  bool              _imrec_done;

  uint32_t          _id,           // Debug only

  void sendCommand             (ISMRMRD::CommandType cmd, uint32_t cmd_id);
  void handleAcquisition       (ISMRMRD::Entity  ent);
  void handleCommand           (ISMRMRD::Command  msg);
  void handleErrorNotification (ISMRMRD::ErrorNotification msg);
  void handleIsmrmrdHeader     (ISMRMRD::IsmrmrdHeader msg);
  void clientAccepted          (bool accepted);
  void handleHandshake         (ISMRMRD::Handshake  msg);
  void sendError               (ISMRMRD::ErrorType type, std::string descr);
};
