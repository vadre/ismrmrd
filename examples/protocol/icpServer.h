#ifndef ICP_SERVER_H
#define ICP_SERVER_H

#include "icpSession.h"
#include "imageReconOne.h"
#include "ismrmrd/ismrmrd.h"
#include "ismrmrd/xml.h"

using ICP_SESSION = std::shared_ptr<icpSession>;
/*******************************************************************************
 ******************************************************************************/
class icpServer
{
public:
       icpServer       (ICP_SESSION, uint32_t id); // id is for debug only
       ~icpServer      ();
  void run             ();
  void clientAccepted  (ISMRMRD::Handshake* msg, bool accepted);
  void configure       (ISMRMRD::CommandType cmd);
  void sendCommand     (ISMRMRD::CommandType cmd, uint32_t cmd_id);
  ISMRMRD::Entity* copyEntity (ISMRMRD::Entity* ent, uint32_t storage);

  std::vector<ISMRMRD::Entity*>  _acqs;
  ICP_SESSION                    _session;
  bool                           _header_received;
  bool                           _acq_storage_set;
  bool                           _client_done;
  bool                           _imrec_done;
  imageReconBase*                _imrec;
  ISMRMRD::IsmrmrdHeader         _hdr;
  ISMRMRD::StorageType           _acq_storage;

private:

  void sendError       (ISMRMRD::ErrorType type, std::string descr);

  //uint32_t                _id;           // Debug only
};
#endif // ICP_SERVER_H
