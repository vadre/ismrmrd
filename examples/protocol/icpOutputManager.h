/*******************************************************************************
 * Output data manager receives data and/or triggers from the user application
 * and prepares the data to be sent out to a network responder per ISMRMRD
 * Communication Protocol (version 2.x).
 *
 ******************************************************************************/

/**
 * @file icpOutputManager.h
 */
#ifndef ICPOUTPUTMANAGER_H
#define ICPOUTPUTMANAGER_H

#include "ismrmrd/xml.h"
#include <vector>
#include <queue>
#include "icp.h"



namespace ICPOUTPUTMANAGER
{

class icpOutputManager
{
public:

  icpOutputManager ();

  void clientAccepted (bool accepted);

  void sendImage (ISMRMRD::Image<float> img);
  void sendXmlHeader (ISMRMRD::IsmrmrdHeader hdr);
  void sendCommand (ISMRMRD::CommandType cmd_type);

  void setClientName (std::string name);
  void setSessionTimestamp (uint64_t timestamp);

  void setClientDone ();
  bool isClientDone ();

  void setServerDone ();
  bool isServerDone ();

  void setRequestCompleted ();
  bool isRequestCompleted ();

  void send (SOCKET_PTR sock);

private:

  void queueMessage (uint32_t                    size,
                     std::vector<unsigned char>& ent,
                     std::vector<unsigned char>& data);

  char              _client_name[ISMRMRD::MAX_CLIENT_NAME_LENGTH];
  uint64_t          _session_timestamp;
  USER_DATA         _udata;

  bool              _user_data_registered;
  bool              _client_done;
  bool              _server_done;
  bool              _request_completed;

  std::queue<OUT_MSG> _oq;

  
}; /* class icpOutputManager */

} /* namespace ICPOUTPUTMANAGER */

#endif /* ICPOUTPUTMANAGER_H */
