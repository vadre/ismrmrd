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

    ICP_CONNECTION connection (new icpConnectionManager (sock));
    std::thread (runUserApp, connection, id).detach();
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
    std::out << "icpConnection is already running\n";
    return;
  }
  if (!_user_app_registered)
  {
    std::out << "User app not registered\n";
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
    SOCKET_PTR sock (new tcp::socket (io_service));
    tcp::endpoint endpoint
      (boost::asio::ip::address::from_string (_host), _port);

    boost::system::error_code error = boost::asio::error::host_not_found;
    //(*sock).connect(endpoint, error);
    sock->connect(endpoint, error);
    if (error)
    {
      throw boost::system::system_error(error);
    }

    _running = true;
    ICP_SESSION session (new icpSession (sock, id));
    std::thread (runUserApp, session, id).detach();
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
  _main_thread (),
  _user_app_registered (false)
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
  _user_app_registered (false)
{}

