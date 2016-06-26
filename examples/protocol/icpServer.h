#ifndef ICP_SERVER_H
#define ICP_SERVER_H

#include "icpSession.h"
#include "ismrmrd/ismrmrd.h"
#include "ismrmrd/xml.h"

/*******************************************************************************
 ******************************************************************************/
class icpServer
{
  public:

       icpServer       (ICP_SESSION);
       ~icpServer      ();

  void processHandshake (HANDSHAKE* msg);
  void processCommand   (COMMAND*   msg);
  void processError     (ERRREPORT* msg);
  void sendImage        (ENTITY*, uint32_t version, STYPE, uint32_t stream);
  void sendHeader       (ISMRMRD::IsmrmrdHeaderWrapper*, ETYPE);
  void sendError        (ISMRMRD::ErrorType type, std::string descr);
  void taskDone         ();

  private:

  void clientAccepted  (ISMRMRD::Handshake* msg, bool accepted);
  void configure       (COMMAND* cmd);
  void sendCommand     (ISMRMRD::CommandType cmd, uint32_t cmd_id);

  ICP_SESSION  _session;
  icpCallback* _entCB;
  icpCallback* _recCB;
  bool         _client_done;
  bool         _task_done;
};
#endif // ICP_SERVER_H
