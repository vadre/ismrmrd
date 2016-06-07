#include "icpSession.h"
#include "icpUserAppBase.h"
#include "imageReconOne.h"
#include "ismrmrd/ismrmrd.h"
#include "ismrmrd/xml.h"

/*******************************************************************************
 ******************************************************************************/
class icpServer : public icpUserAppBase
{
public:
       icpServer  (uint32_t id); // id is for debug only
       ~icpServer ();

  void saveSessionPointer      (icpSession* session);
  void handleAcquisition       (ENTITY*   ent, STYPE stype);
  void handleCommand           (COMMAND   msg);
  void handleErrorNotification (ERRNOTE   msg);
  void handleIsmrmrdHeader     (XMLHEAD   msg);
  void handleHandshake         (HANDSHAKE msg);

private:

  void clientAccepted          (HANDSHAKE msg, bool accepted);
  void sendCommand             (ISMRMRD::CommandType cmd, uint32_t cmd_id);
  void sendError               (ISMRMRD::ErrorType type, std::string descr);
  ENTITY* copyEntity           (ENTITY* ent, STYPE stype);

  icpSession*                    _session;
  imageReconBase*                _imrec;
  ISMRMRD::IsmrmrdHeader         _hdr;
  ISMRMRD::StorageType           _acq_storage;
  std::vector<ISMRMRD::Entity*>  _acqs;

  bool                           _header_received;
  bool                           _acq_storage_set;
  bool                           _client_done;
  bool                           _imrec_done;
  uint32_t                       _id;           // Debug only
};
