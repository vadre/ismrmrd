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
       //~icpServer      ();

  void processHandshake (HANDSHAKE* msg);
  void processCommand   (COMMAND*   msg);
  void processError     (ERRREPORT* msg);
  void sendImage        (ENTITY*, uint32_t version, STYPE, uint32_t stream);
  void taskDone         ();

  private:

  void sendError       (ISMRMRD::ErrorType type, std::string descr);
  void clientAccepted  (ISMRMRD::Handshake* msg, bool accepted);
  void configure       (COMMAND* cmd);
  void sendCommand     (ISMRMRD::CommandType cmd, uint32_t cmd_id);

  ICP_SESSION  _session;
  bool         _header_received;
  bool         _client_done;
  bool         _task_done;

  std::string  _client_name; /* For multiple client testing */
};
#endif // ICP_SERVER_H
