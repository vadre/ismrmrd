#include <signal.h>
#include "icpConnection.h"
#include "icpSession.h"

/*******************************************************************************
 ******************************************************************************/
bool           icpConnection::_running  = false;
icpConnection* icpConnection::_this     = NULL;

/*******************************************************************************
 ******************************************************************************/
void icpConnection::signalHandler
(
  int sig_id
)
{
  _this->_acceptor->cancel();
  _this->_io_service.stop();
  std::cout << "\nConnection module exiting\n";
}

/*******************************************************************************
 ******************************************************************************/
void icpConnection::catchSignal ()
{
  struct sigaction action;
  action.sa_handler = icpConnection::signalHandler;
  action.sa_flags = 0;
  sigemptyset (&action.sa_mask);
  sigaction (SIGINT, &action, NULL);
  sigaction (SIGTERM, &action, NULL);
}

/*******************************************************************************
 ******************************************************************************/
void icpConnection::asyncAccept ()
{

  SOCKET_PTR sock (new tcp::socket (_io_service));
  _acceptor->async_accept
    (*sock, bind (&icpConnection::startUserApp, this, sock));
  _io_service.run_one();
}

/*******************************************************************************
 ******************************************************************************/
void icpConnection::startUserApp
(
  SOCKET_PTR sock
)
{
  ICP_SESSION session (new icpSession (sock));
  std::thread (runUserApp, std::move (session)).detach();

  asyncAccept ();
}

/*******************************************************************************
 ******************************************************************************/
void icpConnection::registerUserApp
(
  START_USER_APP_FUNC func_ptr
)
{
  if (!func_ptr)
  {
    throw std::runtime_error ("Invalid User App pointer");
  }

  runUserApp           = func_ptr;
  _user_app_registered = true;
}

/*******************************************************************************
 For server
 ******************************************************************************/
void icpConnection::start ()
{
  if (_running)
  {
    throw std::runtime_error ("icpConnection is already running\n");
  }
  if (!_user_app_registered)
  {
    throw std::runtime_error ("User app not registered\n");
  }

  icpConnection::_this = this;
  _acceptor = std::shared_ptr<tcp::acceptor>
    (new tcp::acceptor (_io_service, tcp::endpoint(tcp::v4(), _port)));

  catchSignal();
  asyncAccept ();
  _running = true;

  return;
}

/*******************************************************************************
 For client
 ******************************************************************************/
void icpConnection::connect ()
{
  if (_running)
  {
    throw std::runtime_error ("icpConnection is already running\n");
  }
  if (!_user_app_registered)
  {
    throw std::runtime_error ("User app not registered\n");
  }

  std::cout << "Connecting to <" << _host << "> on port " << _port << "\n\n";
  boost::asio::io_service io_service;
  SOCKET_PTR sock (new tcp::socket (io_service));
  tcp::endpoint endpoint
    (boost::asio::ip::address::from_string (_host), _port);

  boost::system::error_code error = boost::asio::error::host_not_found;
  sock->connect(endpoint, error);
  if (error)
  {
    throw boost::system::system_error(error);
  }

  _running = true;
  ICP_SESSION session (new icpSession (sock));
  std::thread t (runUserApp, std::move (session));
  t.join();
}

/*******************************************************************************
 Constructor for server application
 ******************************************************************************/
icpConnection::icpConnection
(
  uint16_t p
)
: _port (p),
  _user_app_registered (false)
{}

/*******************************************************************************
 Constructor for client application
 ******************************************************************************/
icpConnection::icpConnection
(
  std::string host,
  uint16_t    port
)
: _host (host),
  _port (port),
  _user_app_registered (false)
{
}
