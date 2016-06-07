#ifndef ICP_SESSION_H
#define ICP_SESSION_H

#include <boost/asio.hpp>
#include <thread>
#include <functional>
#include <memory>
#include <queue>
#include <utility>
#include <vector>
#include <time.h>
#include "ismrmrd/version.h"
#include "ismrmrd/xml.h"
#include "icp.h"

/*******************************************************************************
 ******************************************************************************/

class icpUserAppBase;

class icpSession
{
public:

  //icpUserAppBase* user;

           icpSession            (SOCKET_PTR sock, icpUserAppBase * user_obj,
                                  uint32_t id);
           ~icpSession           ();

  void     subscribe             (ETYPE etype, bool action);
  void     shutdown              ();
  bool     send                  (ETYPE etype, ENTITY* ent,
                                  STYPE stype = ISMRMRD::ISMRMRD_CHAR);

private:

  void     run                   ();
  bool     getMessage            ();
  void     transmit              ();
  bool     expected              (ETYPE etype);
  uint32_t receiveFrameInfo      (IN_MSG& in_msg);
  void     deliver               (IN_MSG& in_msg);
  void     queueMessage          (std::vector<unsigned char>& ent,
                                  std::vector<unsigned char>& data);
  
  std::thread                    _session_thread;
  std::thread                    _transmit_thread;
  SOCKET_PTR                     _sock;
  icpUserAppBase*                _user;
  bool                           _stop;
  std::vector<ETYPE>             _etypes;
  std::queue<OUT_MSG>            _oq;
  uint32_t                       _id;   // debug only
};

/*******************************************************************************
 ******************************************************************************/
using ICP_SESSION = std::shared_ptr<icpSession>;

#endif // ICP_SESSION_H
