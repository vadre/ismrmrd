/*******************************************************************************
 * Output data manager serializes and queues messages from the user application
 * and sends them out via socket
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

         icpOutputManager();
    bool processEntity (ISMRMRD::EntityType type,
                        ISMRMRD::Entity*    entity);

    void send (SOCKET_PTR sock);
    void setSessionClosing();
    bool isSessionClosing();

  private:

    void queueMessage (std::vector<unsigned char>& ent,
                       std::vector<unsigned char>& data);

    std::queue<OUT_MSG> _oq;
    bool _session_closed;

  }; /* class icpOutputManager */

} /* namespace ICPOUTPUTMANAGER */

#endif /* ICPOUTPUTMANAGER_H */
