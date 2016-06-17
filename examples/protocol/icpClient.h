#ifndef ICP_CLIENT_H
#define ICP_CLIENT_H

#include "icpSession.h"
#include "ismrmrd/ismrmrd.h"
#include "ismrmrd/dataset.h"
#include "ismrmrd/xml.h"

/*******************************************************************************
 ******************************************************************************/
class icpClient
{
  public:

       icpClient        (ICP_SESSION session,
                         std::string client_name,
                         std::string in_fname,
                         std::string out_fname,
                         std::string in_dataset,
                         std::string out_dataset);

  void processHandshake (HANDSHAKE* msg);
  void processCommand   (COMMAND*   msg);
  void processError     (ERRREPORT* msg);
  void taskDone         ();

  private:

  void beginInput ();
  void sendHandshake ();
  void sendCommand (ISMRMRD::CommandType cmd_type, uint32_t cmd_id);
  void sendError (ISMRMRD::ErrorType type, std::string descr);
  template <typename S>
  void sendAcquisitions (ISMRMRD::Dataset& dset);

  ICP_SESSION  _session;
  std::string  _client_name;
  std::string  _in_fname;
  std::string  _out_fname;
  std::string  _in_dset;
  std::string  _out_dset;
  bool         _server_done;
  bool         _task_done;

};
#endif // ICP_CLIENT_H */
