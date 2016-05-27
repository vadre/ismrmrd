#include "icpConnection.h"
#include "icpSession.h"

/*******************************************************************************
 ******************************************************************************/
bool  icpConnection::_running  = false;

/*******************************************************************************
 Thread
 ******************************************************************************/
void icpConnection::acceptor ()
{
  boost::asio::io_service io_service;
  tcp::acceptor a (io_service, tcp::endpoint (tcp::v4(), _port));

  for (uint32_t id = 1;; ++id)
  {
    SOCKET_PTR sock (new tcp::socket (io_service));
    a.accept (*sock);
    std::cout << __func__ << ": Connection #" << id << "\n\n";

    ICP_SESSION session (new icpSession (sock, id));
    std::thread (runUserApp, session, id).detach();
  }

  return;
}

/*******************************************************************************
 ******************************************************************************/
bool icpConnection::registerUserApp
(
  START_USER_APP_FUNC func_ptr
)
{
  if (func_ptr)
  {
    runUserApp           = func_ptr;
    _user_app_registered = true;
  }
  return _user_app_registered;
}

/*******************************************************************************
 ******************************************************************************/
void icpConnection::start ()
{
  if (_running)
  {
    std::cout << "icpConnection is already running\n";
    return;
  }
  if (!_user_app_registered)
  {
    std::cout << "User app not registered\n";
    return;
  }

  _main_thread = std::thread (&icpConnection::acceptor, this);
  _running = true;

  return;
}

/*******************************************************************************
 ******************************************************************************/
void icpConnection::connect ()
{
  if (!_running)
  {
    std::cout << "Connecting to <" << _host << "> on port " << _port << "\n\n";
    boost::asio::io_service io_service;
    std::cout << __func__ << ": 1\n";
    SOCKET_PTR sock (new tcp::socket (io_service));
    std::cout << __func__ << ": 1\n";
    tcp::endpoint endpoint
      (boost::asio::ip::address::from_string (_host), _port);
    std::cout << __func__ << ": 1\n";

    boost::system::error_code error = boost::asio::error::host_not_found;
    std::cout << __func__ << ": 1\n";
    //(*sock).connect(endpoint, error);
    sock->connect(endpoint, error);
    std::cout << __func__ << ": 1\n";
    if (error)
    {
    std::cout << __func__ << ": 1\n";
      throw boost::system::system_error(error);
    }
    std::cout << __func__ << ": 1\n";

    _running = true;
    std::cout << __func__ << ": 1\n";
    ICP_SESSION session (new icpSession (sock, 777));
    std::cout << __func__ << ": 1\n";
    std::thread t (runUserApp, session, 777);
    std::cout << __func__ << ": 1\n";
    t.join();
  }
  return;
}

/*******************************************************************************
 ******************************************************************************/
icpConnection::~icpConnection ()
{
  if (_main_thread.joinable ()) _main_thread.join();
}

/*******************************************************************************
 ******************************************************************************/
icpConnection::icpConnection
(
  uint16_t p
)
: _port (p),
  _user_app_registered (false),
  _main_thread ()
{}

/*******************************************************************************
 ******************************************************************************/
icpConnection::icpConnection
(
  std::string host,
  uint16_t    port
)
: _host (host),
  _port (port),
  _user_app_registered (false),
  _main_thread ()
{}

