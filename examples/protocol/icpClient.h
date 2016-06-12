#ifndef ICP_CLIENT_H
#define ICP_CLIENT_H

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
  void sendCommand (ISMRMRD::CommandType cmd_type, uint32_t cmd_id);
  void writeImage (ISMRMRD::Dataset& dset, ENTITY* ent, STYPE stype);
  
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


private:

  void beginInput ();
  void sendHandshake ();
  void sendError (ISMRMRD::ErrorType type, std::string descr);
  template <typename S>
  void sendAcquisitions (ISMRMRD::Dataset& dset);



};
#endif // ICP_CLIENT_H */
