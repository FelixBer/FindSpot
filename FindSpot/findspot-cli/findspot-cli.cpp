#include <iostream>
#include <string>

#include "../socklib.h"
#include "../packetmanager.h"


void printusage()
{
  std::cerr << "findspot [port]\ndefault port is " << FS_PORT << std::endl;
}

int main(int argc, char** argv)
{
  int port = FS_PORT;
  if(argc == 2)
  {
    port = atoi(argv[1]);
  }

  if(argc > 2 || port == 0)
  {
    printusage();
    return 0;
  }

  std::cout << "using port " << port << std::endl;
  FindSpotPacketManager manager;
  manager.clientfd = manager.try_connect(port);
  std::cout << manager.recv_cmd_block() << std::endl;;

  while(1)
  {
    std::cout << "findspot>";
    std::string cmd;
    std::getline(std::cin, cmd);
    manager.send_cmd(cmd);
    std::cout << manager.recv_cmd_block() << std::endl;
  }

  return 0;
}
