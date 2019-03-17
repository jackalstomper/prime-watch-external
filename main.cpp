#include "common.h"
#include <sys/types.h>

#ifdef WIN32
#   include <winsock.h>
#   include <signal.h>
#else
#   include <sys/socket.h>
#   include <sys/unistd.h>
#   include <netinet/in.h>
#   include <netdb.h>
#endif

#include <iostream>
#include <cstdio>
#include "GameMemory.h"
#include "prime1/Prime1JsonDumper.hpp"
#include "MemoryBuffer.hpp"

#ifdef WIN32
#   ifndef PW_SOCKET
#       define PW_SOCKET SOCKET
#   endif
#   ifndef ssize_t
#       define ssize_t int64_t
#   endif
#   ifndef MSG_NOSIGNAL
#       define MSG_NOSIGNAL 0
#   endif
#else
#   ifndef PW_SOCKET
#       define PW_SOCKET int
#   endif
#endif

using namespace std;
using namespace nlohmann;

bool run = true;

void sighup(int sig) {
  cout << "SIGNAL " << sig << endl;
  run = false;
}

void handleClient(PW_SOCKET sock);

int main() {
  findAndAttachProcess();

  //Not sure what I'm doing wrong here
#ifndef WIN32 // Windows doesn't support SIGHUP
  signal(SIGHUP, sighup);
#endif
  signal(SIGINT, sighup);
  signal(SIGABRT, sighup);

#ifdef WIN32
  // initialize winsock for use
  WSADATA wsaData;
  int error = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (error != 0) {
    cerr << "Failed to initialize winsock: " << error << endl;
    return 1;
  }

  if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
    cerr << "Could not find a usable version of winsock" << endl;
    WSACleanup();
    return 1;
  }
#endif

  constexpr uint16_t port = 43673;
  PW_SOCKET serverSock = socket(AF_INET, SOCK_STREAM, 0);

  if (serverSock < 0) {
    cerr << "Failed to create server sock" << endl;
#ifdef WIN32
    int error = WSAGetLastError();
    cerr << "Error: " << error << endl;
    WSACleanup();
#endif
    return 1;
  }

  sockaddr_in serverAddr;
  memset(&serverAddr, 0, sizeof(serverAddr));
  serverAddr.sin_family = AF_INET;

#ifdef PW_USE_LOOPBACK
  serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
#else
  serverAddr.sin_addr.s_addr = INADDR_ANY;
#endif

  serverAddr.sin_port = htons(port);


#ifdef WIN32
  char value = 1;
  setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));
#else
  int value = 1;
  setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));
#endif

  if (::bind(serverSock, reinterpret_cast<sockaddr *>(&serverAddr), sizeof(serverAddr)) < 0) {
    cerr << "Failed to bind to port" << endl;
#ifdef WIN32
    int error = WSAGetLastError();
    cerr << "Error: " << error << endl;
    WSACleanup();
#endif
    return 1;
  }

  listen(serverSock, 5);

  cout << "Listening on port " << port << endl;

  while (run) {
    cout << "Waiting for client..." << endl;
    sockaddr_in clientAddr;
    int clientSize = sizeof(clientAddr);
    memset(&clientAddr, 0, sizeof(clientAddr));
    PW_SOCKET clientSock = accept(serverSock, reinterpret_cast<sockaddr *>(&clientAddr), &clientSize);
    cout << "Client connected" << endl;

    handleClient(clientSock);

#ifdef WIN32
    closesocket(clientSock);
#else
    close(clientSock);
#endif
  }

  cout << "Closing server" << endl;

#ifdef WIN32
  closesocket(serverSock);
  WSACleanup();
#else
  close(serverSock);
#endif

  return 0;
}

void handleClient(PW_SOCKET sock) {
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
    uint32_t belen = hostToBe32(len);
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

#ifdef WIN32
    Sleep(33);
#else
    usleep(33);
#endif
  }
}

