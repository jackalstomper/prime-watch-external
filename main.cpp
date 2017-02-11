#include "common.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/unistd.h>
#include <netinet/in.h>
#include <iostream>
#include <cstdio>
#include <netdb.h>
#include "GameMemory.h"
#include "prime1/Prime1JsonDumper.hpp"
#include "MemoryBuffer.hpp"

using namespace std;
using namespace nlohmann;

bool run = true;

void sighup(int sig) {
  cout << "SIGNAL " << sig << endl;
  run = false;
}

int main() {
  wootWootGetRoot();
  findAndAttachProcess();

  //Not sure what I'm doing wrong here
  signal(SIGHUP, sighup);
  signal(SIGINT, sighup);
  signal(SIGABRT, sighup);

  constexpr uint16_t port = 43673;
  int serverSock = socket(AF_INET, SOCK_STREAM, 0);

  if (serverSock < 0) {
    cerr << "Failed to create server sock" << endl;
    return 1;
  }

  sockaddr_in serverAddr;
  memset(&serverAddr, 0, sizeof(serverAddr));
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_addr.s_addr = INADDR_ANY;
  serverAddr.sin_port = htons(port);


  int value = 1;
  setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));

  if (bind(serverSock, reinterpret_cast<sockaddr *>(&serverAddr), sizeof(serverAddr)) < 0) {
    cerr << "Failed to bind to port" << endl;
    return 1;
  }

  listen(serverSock, 5);

  cout << "Listening on port " << port << endl;

  while (run) {
    cout << "Waiting for client..." << endl;
    sockaddr_in clientAddr;
    int clientSize = sizeof(clientAddr);
    memset(&clientAddr, 0, sizeof(clientAddr));
    int clientSock = accept(serverSock, reinterpret_cast<sockaddr *>(&clientAddr), &clientSize);
    cout << "Client connected" << endl;

    handleClient(clientSock);

    close(clientSock);
  }

  cout << "Closing server" << endl;
  close(serverSock);

  return 0;
}

void handleClient(int sock) {
  MemoryBuffer buffer(0x10000);

  while (true) {
    buffer.clear();

    json json_message;

    uint32_t gameID = GameMemory::read_u32(0x80000000);
    uint32_t makerID = GameMemory::read_u32(0x80000004) >> 16;

    if (gameID == 0x4147424A || (gameID == 0x474D3845 && makerID == 0x3031)) {
//        json_message["heap"] = Prime1JsonDumper::parseHeap();
      json_message["camera"] = Prime1JsonDumper::parseCamera();
//        json_message["player"] = Prime1JsonDumper::parsePlayer();
      json_message["player_raw"] = Prime1JsonDumper::parsePlayerRaw();
      json_message["world"] = Prime1JsonDumper::parseWorld();
      json_message["pool_summary"] = Prime1JsonDumper::parsePoolSummary();
      json_message["heap_stats"] = Prime1JsonDumper::parseHeapStats();
    }

    string message = json_message.dump();
    if (message.length() == 0 || message == "null") {
      message = "{}";
    }
    uint32_t len = static_cast<uint32_t>(message.length());
    uint32_t belen = htobe32(len);
    buffer.write(&belen, 4);
    buffer << message;

    ssize_t sent = send(sock, buffer.getBuff(), len + 4, MSG_NOSIGNAL);
    if (sent < 0) {
      cerr << "Failed to write" << endl;
      return;
    }

    if (sent != len + 4) {
      cerr << "Didn't send everything?!? " << sent << "/" << (len + 4) << endl;
    }

    usleep(1000);
  }
}

