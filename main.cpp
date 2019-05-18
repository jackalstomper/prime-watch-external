#include "common.h"
#include <sys/types.h>

#ifdef WIN32
#   include <winsock.h>
#   include <signal.h>
#include <windows.h>
#else

#   include <stdio.h>
#   include <sys/socket.h>
#   include <sys/unistd.h>
#   include <unistd.h>
#   include <netinet/in.h>
#   include <arpa/inet.h>
#   include <netdb.h>
#include <sys/stat.h>

#endif

#include <iostream>
#include <cstdio>
#include <prime1/CGameGlobalObjects.hpp>
#include <prime1/CGameState.hpp>
#include <prime1/CStateManager.hpp>
#include <unordered_map>
#include <fstream>
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
    socklen_t clientSize = sizeof(clientAddr);
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

void handleWorldLoads(ostream &pFile);

void handleClient(PW_SOCKET sock) {
  MemoryBuffer buffer(0x10000);

  #ifdef win32
  CreateDirectory("logs", nullptr);
  #else
  mkdir("logs", 0755);
  #endif

  auto time = chrono::system_clock::now().time_since_epoch().count();
  string filename = "logs/loads_" + to_string(time) + ".log";
  ofstream outFile(filename, ios_base::out | ios_base::binary);
  if (!outFile) {
    cout << "Failed to open log file" << endl;
    exit(3);
  }
  cout << "Logging loads to " << filename << endl;

  while (true) {
    handleWorldLoads(outFile);

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

    #ifdef __APPLE__
    ssize_t sent = send(sock, buffer.getBuff(), len + 4, 0);
    #else
    ssize_t sent = send(sock, buffer.getBuff(), len + 4, MSG_NOSIGNAL);
    #endif
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

int timeToFrames(double time) {
  return round(time * 60);
}

struct DolphinCAreaTracking {
  uint32_t mrea = -1;
  EChain chain = EChain::Deallocated;
  int loadStart = 0;
  int loadEnd = 0;
  EPhase phase = EPhase::LoadHeader;
  EOcclusionState occlusionState = EOcclusionState::Occluded;
};

static unordered_map<uint32_t, DolphinCAreaTracking> areas;

string phaseName(EPhase phase) {
  switch (phase) {
    case EPhase::LoadHeader:
      return "Load header";
    case EPhase::LoadSecSizes:
      return "Load section sizes";
    case EPhase::ReserveSections:
      return "Reserve Sections";
    case EPhase::LoadDataSections:
      return "Load data sections";
    case EPhase::WaitForFinish:
      return "Wait for finish";
  }
  return "Unknown";
}

string chainName(EChain chain) {
  switch (chain) {
    case EChain::Invalid:
      return "Invalid";
    case EChain::ToDeallocate:
      return "ToDeallocate";
    case EChain::Deallocated:
      return "Deallocated";
    case EChain::Loading:
      return "Loading";
    case EChain::Alive:
      return "Alive";
    case EChain::AliveJudgement:
      return "Alive Judgement";
  }
  return "Unknown";
}

string occlusionName(EOcclusionState state) {
  switch (state) {
    case EOcclusionState::Occluded:
      return "Occluded";
    case EOcclusionState::Visible:
      return "Visible";
  }
  return "Unknown";
}

void handleWorldLoads(ostream &outFile) {
  CGameGlobalObjects global(CGameGlobalObjects::LOCATION);
  CStateManager manager(CStateManager::LOCATION);
  CGameState gameState = global.gameState.deref();
  double time = gameState.playTime.read();
  int frame = timeToFrames(time);

  CWorld world = manager.world.deref();
  int areaCount = world.areas.size.read();
  for (int i = 0; i < areaCount; i++) {
    auto autoPtr = world.areas[i];
//    cout << hex << " autoptr offset: " << autoPtr.ptr();
//    cout << hex << " autoptr second value: " << autoPtr.dataPtr.read();
//    cout << endl;
    CGameArea area = autoPtr.dataPtr.deref();
    uint32_t mrea = area.mrea.read();

    DolphinCAreaTracking &areaTracking = areas[mrea];

    stringstream ss;
    EChain newChain = area.curChain.read();
    if (newChain != areaTracking.chain && newChain != EChain::AliveJudgement) {
      cout << dec << frame << ": Area " << hex << mrea
           << " chain " << chainName(areaTracking.chain)
           << " -> " << chainName(newChain) << endl;

      areaTracking.chain = newChain;
      if (newChain == EChain::Loading) {
        areaTracking.loadStart = frame;
      }
      if (newChain == EChain::Alive) {
        areaTracking.loadEnd = frame;
        int loadFrames = areaTracking.loadEnd - areaTracking.loadStart;
        ss << dec << frame << ": Area " << hex << mrea
           << " loaded in " << dec << loadFrames << " frames" << endl;
      }
    }

    EPhase newPhase = area.phase.read();
    if (newPhase != areaTracking.phase) {
      ss << dec << frame << ": Area " << hex << mrea
         << " phase " << phaseName(areaTracking.phase)
         << " -> " << phaseName(newPhase) << endl;
      areaTracking.phase = newPhase;
    }

    EOcclusionState newOcclusion = EOcclusionState::Occluded;
    if (area.postConstructed.read() != 0) {
      CPostConstructed pc = area.postConstructed.read();
      newOcclusion = pc.occlusionState.read();
    }

    if (newOcclusion != areaTracking.occlusionState) {
      ss << dec << frame << ": Area " << hex << mrea
         << " occlusion " << occlusionName(areaTracking.occlusionState)
         << " -> " << occlusionName(newOcclusion) << endl;
      areaTracking.occlusionState = newOcclusion;
    }

    string str = ss.str();
    if (!str.empty()) {
      cout << str;
      cout.flush();
      outFile << str;
      outFile.flush();
    }
  }

}
