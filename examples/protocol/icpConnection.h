#ifndef ICP_CONNECTION_H
#define ICP_CONNECTION_H

#include <boost/asio.hpp>
#include "icp.h"
#include <thread>
#include "icpSession.h"

//using ICP_SESSION = std::unique_ptr<icpSession>;
using GET_USER_APP_INSTANCE = icpUserAppBase* (*) (uint32_t);

/*******************************************************************************
 ******************************************************************************/
class icpConnection
{
public:

       icpConnection     (uint16_t port);
       icpConnection     (std::string host, uint16_t port);
       ~icpConnection    ();
  void start             ();
  void connect           ();
  bool registerUserApp   (GET_USER_APP_INSTANCE func_ptr);

private:

  static bool            _running;
  std::string            _host;
  unsigned short         _port;
  bool                   _user_app_registered;
  std::thread            _main_thread;

  void acceptor          ();
  GET_USER_APP_INSTANCE  getUserApp;
};
#endif // ICP_CONNECTION_H */
