#include "icpConnection.h"
#include "icpSession.h"

void runMe (ICP_SESSION session, uint32_t id)
{
  icpServer* server = new icpServer (session, id);
  std::cout << __func__ << " Running for id " << id << "\n";
  server->run();
  std::cout << __func__ << " Deleting for id " << id << "\n";
  delete (server);
  std::cout << __func__ << " Returning for id " << id << "\n";
  return;
}

/*******************************************************************************
 ******************************************************************************/
int main
(
  int argc,
  char* argv[]
)
{
  unsigned int port;

  if (argc != 2)
  {
    port = 50050;
  }
  else
  {
    port = std::atoi (argv[1]);
  }

  std::cout << "ISMRMRD Server app starts with port number " << port << '\n';
  std::cout << "To connect to a different port, restart: icpServer <port>\n\n";

  icpConnection server_conn (port);
  server_conn.registerUserApp ((icpConnection::START_USER_APP_FUNC) &runMe);
  server_conn.start();

  return 0;
}
