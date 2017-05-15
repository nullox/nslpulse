/*

ABOUT:

Drops useful summary data concerning an active live data server. May be
utilised by an autonomous client to sweep across a cluster of servers to
pinpoint those approaching load capacities and maintain an eye on running
database processes.

Clients may thereafter be configured to dispatch alerts to admin personnel
to address these key infrastructural concerns.

Nullox Standard Library Component
Copyright 2017 (c) Nullox Software
Written by William Johnson <johnson@nullox.com>

LICENSE:

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

1. Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

3. Neither the name of the project nor the names of its contributors may be
used to endorse or promote products derived from this software without specific
prior written permission.

THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
SUCH DAMAGE.

DEPLOYMENT NOTES:

1. Change the DAEMON_KEY value.

2. Change the port to prevent autonomous landers.

3. Keep a secure note of all keys you generate for daemon deployment
   and configure your client to point to the location of the server.
*/
#include <stdio.h>
#include <stdint.h>

#ifdef _WIN32

/*
compile on windows: gcc nslserva.c -o nslserva.exe -lpsapi -lws2_32
*/

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <tchar.h>
#include <psapi.h>

#elif __linux__

// todo

#endif

/* daemon key for pulse */
static const char DAEMON_KEY[] = "NSLPULSE_PUBLIC_DEMO";

/* daemon port */
static const char DAEMONPORT[] = "50110";

/* terminate server process command */
static const char DAEMON_CMD_KPROC[] = "kserv";

/* close connection command */
static const char DAEMON_CMD_CLOSE[] = "dc";

/* output delimitor */
static const char DELIMITOR[] = ":";

/* bytes in kb, base-2 interpretation as opposed to SI units */
static const uint16_t BYTESPERKB = 1024;

/* number of db processes to enquire */
static const uint8_t DBPROCESS_N = 3;

/* list of process names to expect */
static const char DBPROCESSES[3][11] = {"mysql", "mysqld.exe", "mysqld"};

/* internal buffer length to handle network-io */
static const uint16_t SZ_BUFFER_LEN = 512;

/* available memory in kb */
uint32_t available_memory() {
#ifdef _WIN32
  MEMORYSTATUSEX mi;
  mi.dwLength = sizeof(mi);
  GlobalMemoryStatusEx(&mi);
  return mi.ullAvailPhys / BYTESPERKB;
#endif
}

/* available disk space in kb */
uint32_t available_space() {
#ifdef _WIN32
  ULARGE_INTEGER lpFBA, lpTNB, lpTNFB;
  if ( GetDiskFreeSpaceEx("C:", &lpFBA, &lpTNB, &lpTNFB) )
    return lpFBA.QuadPart/BYTESPERKB;
  return 0;
#endif
}

/* current cpu load using prior tick computation */
double cpu_load() {
#ifdef _WIN32
  FILETIME it; /* idle time */
  FILETIME kt; /* kernel time */
  FILETIME ut; /* user time */
  if ( GetSystemTimes(&it, &kt, &ut) ) {
    static uint64_t previousTotalTicks = 0;
    static uint64_t previousIdleTicks = 0;
    uint64_t idleTicks = (((uint64_t)(it.dwHighDateTime))<<32)|((uint64_t)it.dwLowDateTime);
    uint64_t totalTicks = (((uint64_t)(kt.dwHighDateTime))<<32)|((uint64_t)kt.dwLowDateTime);
    totalTicks += (((uint64_t)(ut.dwHighDateTime))<<32)|((uint64_t)ut.dwLowDateTime);
    uint64_t ttd = totalTicks - previousTotalTicks;
    uint64_t itd = idleTicks - previousIdleTicks;
    double delta = (ttd > 0) ? ((double)itd)/ttd : 0;
    previousTotalTicks = totalTicks;
    previousIdleTicks = idleTicks;
    return 1.0 - delta;
  }
  return 0.0;
#endif
}

/* signals that a live database is running */
uint8_t database_running() {
#ifdef _WIN32
  DWORD processIds[1024], bytesNeeded, procCount;
  HANDLE hProcess = NULL;
  TCHAR szProcessName[MAX_PATH] = TEXT("<unknown>");
  uint32_t p = 0, q = 0;
  if ( !EnumProcesses(processIds, sizeof(processIds), &bytesNeeded) )
    return 0;
  procCount = bytesNeeded / sizeof(DWORD);
  for ( ; p < procCount; p++ ) {
    if( processIds[p] != 0 ) {
      hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                             FALSE,
                             processIds[p]);
      if (hProcess != NULL) {
        HMODULE hMod;
        DWORD cbNeeded;
        if ( EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded) ) {
          GetModuleBaseName(hProcess,
                            hMod,
                            szProcessName,
                            sizeof(szProcessName)/sizeof(TCHAR));
          CloseHandle(hProcess);
          for ( q = 0; q < DBPROCESS_N; q++ ) {
            if ( strcmp(DBPROCESSES[q], szProcessName) == 0 )
              return 1;
  }}}}}
  return 0;
#endif
}

/* total disk space in kb */
uint32_t total_disk_space() {
#ifdef _WIN32
  ULARGE_INTEGER lpFBA, lpTNB, lpTNFB;
  if ( GetDiskFreeSpaceEx("C:", &lpFBA, &lpTNB, &lpTNFB) )
    return lpTNB.QuadPart/BYTESPERKB;
  return 0;
#endif
}

/* total physical memory on server box */
uint32_t total_physical_memory() {
#ifdef _WIN32
  MEMORYSTATUSEX mi;
  mi.dwLength = sizeof(mi);
  GlobalMemoryStatusEx(&mi);
  return mi.ullTotalPhys / BYTESPERKB;
#endif
}

/* current uptime */
uint32_t uptime_in_secs() {
#ifdef _WIN32
  return GetTickCount() / 1000;
#endif
}

/* compiles a nsl pulse string */
char* make_pulse_string()
{
  uint32_t total_ds = total_disk_space();
  uint32_t free_ds = available_space();
  uint32_t total_mem = total_physical_memory();
  uint32_t free_mem = available_memory();
  char *pulseString = (char*)malloc(SZ_BUFFER_LEN * sizeof(char));
  int psi = snprintf(pulseString, 100, "%.4f", cpu_load());
  psi += snprintf(pulseString+psi, SZ_BUFFER_LEN-psi, "%s", DELIMITOR);
  psi += snprintf(pulseString+psi, SZ_BUFFER_LEN-psi, "%d", database_running());
  psi += snprintf(pulseString+psi, SZ_BUFFER_LEN-psi, "%s", DELIMITOR);
  psi += snprintf(pulseString+psi, SZ_BUFFER_LEN-psi, "%d", uptime_in_secs());
  psi += snprintf(pulseString+psi, SZ_BUFFER_LEN-psi, "%s", DELIMITOR);
  psi += snprintf(pulseString+psi, SZ_BUFFER_LEN-psi, "%d", total_ds);
  psi += snprintf(pulseString+psi, SZ_BUFFER_LEN-psi, "%s", DELIMITOR);
  psi += snprintf(pulseString+psi, SZ_BUFFER_LEN-psi, "%d", free_ds);
  psi += snprintf(pulseString+psi, SZ_BUFFER_LEN-psi, "%s", DELIMITOR);
  psi += snprintf(pulseString+psi, SZ_BUFFER_LEN-psi, "%.4f", 1.0-(float)free_ds/total_ds);
  psi += snprintf(pulseString+psi, SZ_BUFFER_LEN-psi, "%s", DELIMITOR);
  psi += snprintf(pulseString+psi, SZ_BUFFER_LEN-psi, "%d", total_mem);
  psi += snprintf(pulseString+psi, SZ_BUFFER_LEN-psi, "%s", DELIMITOR);
  psi += snprintf(pulseString+psi, SZ_BUFFER_LEN-psi, "%d", free_mem);
  psi += snprintf(pulseString+psi, SZ_BUFFER_LEN-psi, "%s", DELIMITOR);
  psi += snprintf(pulseString+psi, SZ_BUFFER_LEN-psi, "%.4f", 1.0-(float)free_mem/total_mem);
  return pulseString;
}

#ifdef _WIN32
/* entry point for windows */
int winmain() {

  WSADATA wsaData;

  /* listener and responder */
  SOCKET listener = INVALID_SOCKET;
  SOCKET responder = INVALID_SOCKET;

  /* address info structs */
  struct addrinfo *result = NULL;
  struct addrinfo hints;

  /* socket work variables */
  char sockData[SZ_BUFFER_LEN];
  int sockDataLength = SZ_BUFFER_LEN;
  uint32_t byteCount_t = 0;

  uint32_t errorCode = WSAStartup(MAKEWORD(2,2), &wsaData);
  if ( errorCode != 0 ) {
    printf("winsock library failed with code: %d", errorCode);
    return 1;
  }

  ZeroMemory(&hints, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  hints.ai_flags = AI_PASSIVE;

  errorCode = getaddrinfo(NULL, DAEMONPORT, &hints, &result);
  if ( errorCode != 0 ) {
    printf("getaddrinfo failed with error: %d\n", errorCode);
    WSACleanup();
    return 1;
  }

  listener = socket(result->ai_family,
                    result->ai_socktype,
                    result->ai_protocol);
  if ( listener == INVALID_SOCKET ) {
    printf("socket failed with error: %d\n", WSAGetLastError());
    freeaddrinfo(result);
    WSACleanup();
  }

  errorCode = bind(listener, result->ai_addr, (int)result->ai_addrlen);
  if ( errorCode == SOCKET_ERROR ) {
    printf("binding failed with error: %d\n", WSAGetLastError());
    freeaddrinfo(result);
    closesocket(listener);
    WSACleanup();
    return 1;
  }

  freeaddrinfo(result);

  errorCode = listen(listener, SOMAXCONN);
  if ( errorCode == SOCKET_ERROR ) {
    printf("listener failed with error: %d\n", WSAGetLastError());
    closesocket(listener);
    WSACleanup();
    return 1;
  }

  uint8_t givenKey = 0;
  uint32_t uploadCode;
  while(1)
  {
    givenKey = 0;
    responder = accept(listener, NULL, NULL);
    if (responder == INVALID_SOCKET) {
      printf("accept failed with error: %d\n", WSAGetLastError());
      closesocket(listener);
      WSACleanup();
      return 1;
    }

    do
    {
      byteCount_t = recv(responder, sockData, sockDataLength, 0);
      if ( byteCount_t > 0 && byteCount_t < (SZ_BUFFER_LEN - 1) )
      {
        /* add terminator */
        sockData[byteCount_t] = '\0';

        if (strcmp(sockData, DAEMON_KEY) == 0) {
          char *ps = make_pulse_string();
          uploadCode = send(responder, ps, strlen(ps), 0);
          givenKey = 1;
        }
        else if (givenKey && strcmp(sockData, DAEMON_CMD_CLOSE) == 0) {
          closesocket(responder);
          shutdown(responder, SD_BOTH);
          byteCount_t = 0;
        }
        else if (givenKey && strcmp(sockData, DAEMON_CMD_KPROC) == 0) {
          byteCount_t = 0;
          closesocket(responder);
          shutdown(responder, SD_BOTH);
          closesocket(listener);
          WSACleanup();
          return 0;
        }
        else if ( givenKey == 0 ) {
          closesocket(responder);
          shutdown(responder, SD_BOTH);
          byteCount_t = 0;
        }

        if ( uploadCode == SOCKET_ERROR ) {
          shutdown(responder, SD_BOTH);
          closesocket(responder);
        }
      }
    }
    while(byteCount_t > 0);
    Sleep(1000);
  }

  closesocket(listener);
  WSACleanup();
  return 0;
}
#elif __linux__
/* entry point for linux */
int linmain() {
  return 0;
}
#endif

int main() {
#ifdef _WIN32
  return winmain();
#elif __linux__
  return linmain();
#endif
}
