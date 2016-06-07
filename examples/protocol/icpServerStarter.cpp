#include "icpServer.h"
#include "icpConnection.h"
#include "icpSession.h"

icpUserAppBase* getUserAppInstance (uint32_t id) // id for debug only
{
  icpUserAppBase* user_app (new icpServer (id));

  return user_app;
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
  server_conn.registerUserApp ((GET_USER_APP_INSTANCE) &getUserAppInstance);
  server_conn.start();

  return 0;
}
