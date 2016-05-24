#include "icpConnection.h"
#include "icpSession.h"

void runMe (ICP_SESSION session, uint32_t id)
{
  icpClient* client = new IcpClient (session, id);
  std::cout << __func__ << " Running Client id " << id << "\n";
  client->run();
  std::cout << __func__ << " Deleting Client id " << id << "\n";
  delete (client);
  std::cout << __func__ << " Returning Client id " << id << "\n";
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

  std::cout << "ISMRMRD Processor app starts with port number " << port << '\n';
  std::cout << "To connect to a different port, restart: icpDisp <port>\n\n";

  icpConnection client_conn (host, port);
  client_conn.registerUserApp ((icpConnection::START_USER_APP_FUNC) &runMe);
  client_conn.connect();

  return 0;
}
