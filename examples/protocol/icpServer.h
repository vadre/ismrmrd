#include "icpSession.h"
#include "imageReconOne.h"
#include "ismrmrd/ismrmrd.h"
#include "ismrmrd/xml.h"

using ICP_SESSION = std::shared_ptr<icpSession>;
/*******************************************************************************
 ******************************************************************************/
class icpServer : public icpUserAppBase
{
public:
       icpServer  (ICP_SESSION, uint32_t id); // id is for debug only
       ~icpServer ();
  void run        ();

  std::vector<ISMRMRD::Entity*>  _acqs;

private:

  ICP_SESSION             _session;
  imageReconBase*         _imrec;
  ISMRMRD::IsmrmrdHeader  _hdr;
  ISMRMRD::StorageType    _acq_storage;

  bool                    _header_received;
  bool                    _acq_storage_set;
  bool                    _client_done;
  bool                    _imrec_done;
  uint32_t                _id;           // Debug only



  void sendCommand             (ISMRMRD::CommandType cmd, uint32_t cmd_id,
                                icpServer* p);
  void handleAcquisition       (ISMRMRD::Entity* ent, uint32_t storage,
                                icpUserAppBase* p);
  ISMRMRD::Entity* copyEntity (ISMRMRD::Entity* ent,
                               uint32_t         storage,
                               icpServer*         inst);
  void handleCommand           (ISMRMRD::Command  msg, icpUserAppBase* p);
  void handleErrorNotification (ISMRMRD::ErrorNotification msg, icpUserAppBase* p);
  void handleIsmrmrdHeader     (ISMRMRD::IsmrmrdHeader msg, icpUserAppBase* p);
  void clientAccepted          (ISMRMRD::Handshake  msg, bool accepted,
                                icpServer* p);
  void handleHandshake         (ISMRMRD::Handshake  msg, icpUserAppBase* p);
  void sendError               (ISMRMRD::ErrorType type, std::string descr,
                                icpServer* p);
};
