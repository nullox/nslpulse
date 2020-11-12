/*

ABOUT:

Drops useful summary data concerning an active live data server. May be
utilised by an autonomous client to sweep across a cluster of servers to
pinpoint those approaching load capacities and maintain an eye on running
database processes.

Clients may thereafter be configured to dispatch alerts to admin personnel
to address these key infrastructural concerns.

Nullox Standard Library Component
Copyright 2020 (c) Nullox Software

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
   and configure your client to point to the location of your server(s).
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

#include <stdlib.h>
#include <linux/kernel.h> /* for struct sysinfo */
#include <sys/sysinfo.h>
#include <sys/statvfs.h>
#include <dirent.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* system info struct */
struct sysinfo s_info;

/* iterate processes in /proc for ident */
pid_t process_id(const char *pname) {
  DIR* dir;
  struct dirent* ent;
  char* endptr;
  char buffert[512];
  if ( !(dir = opendir("/proc")) )
      return -1;
  while((ent = readdir(dir)) != NULL) {
      long lpid = strtol(ent->d_name, &endptr, 10);
      if (*endptr != '\0')
          continue;
      snprintf(buffert, sizeof(buffert), "/proc/%ld/cmdline", lpid);
      FILE* fp = fopen(buffert, "r");
      if (fp) {
          if (fgets(buffert, sizeof(buffert), fp) != NULL) {
              char *tokent = strtok(buffert, " ");
              char *splitt = strtok(tokent, "/");
              while ( splitt != NULL ) {
                if ( strcmp(splitt, pname) == 0 ) {
                  fclose(fp);
                  closedir(dir);
                  return (pid_t)lpid;
                }
                splitt = strtok(NULL, "/");
              }
          }
          fclose(fp);
      }
  }
  closedir(dir);
  return -1;
}

#endif

#if _POSIX_C_SOURCE >= 199309L
#include <time.h> // +nanosleep
#endif

/* daemon key for pulse */
static const char DAEMON_KEY[] = "NSLPULSE_PUBLIC_DEMO";

/* daemon port */
static const char DAEMON_PORT[] = "50110";

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

/* mount point for checking disk stats */
#ifdef _WIN32
static const char DISK_MOUNT_POINT[] = "C:";
#elif __linux__
static const char DISK_MOUNT_POINT[] = "/home";
#endif

/* sleep function in milliseconds */
void sleep_ms(uint32_t milliseconds) {
#ifdef WIN32
    Sleep(milliseconds);
#elif _POSIX_C_SOURCE >= 199309L
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
#else
    if (milliseconds >= 1000)
      sleep(milliseconds / 1000);
    usleep((milliseconds % 1000) * 1000);
#endif
}

/* available memory in kb */
uint32_t available_memory() {
#ifdef _WIN32
  MEMORYSTATUSEX mi;
  mi.dwLength = sizeof(mi);
  GlobalMemoryStatusEx(&mi);
  return mi.ullAvailPhys / BYTESPERKB;
#elif __linux__
  FILE *frd = fopen("/proc/meminfo", "r");
  if (frd == NULL)
    return 0;
  char line_t[256];
  while (fgets(line_t, sizeof(line_t), frd)) {
    uint32_t frv;
    if (sscanf(line_t, "MemAvailable: %d kB", &frv) == 1) {
      fclose(frd);
      return frv;
    }
  }
  fclose(frd);
  return 0;
#endif
}

/* available disk space in kb */
uint32_t available_space() {
#ifdef _WIN32
  ULARGE_INTEGER lpFBA, lpTNB, lpTNFB;
  if ( GetDiskFreeSpaceEx(DISK_MOUNT_POINT, &lpFBA, &lpTNB, &lpTNFB) )
    return lpFBA.QuadPart/BYTESPERKB;
  return 0;
#elif __linux__
  struct statvfs stat;
  if ( statvfs(DISK_MOUNT_POINT, &stat) == 0 )
    return (stat.f_bsize * stat.f_bfree)/BYTESPERKB;
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
#elif __linux__
  int error = sysinfo(&s_info);
  if ( error == 0 )
    return s_info.loads[0]/(1<<SI_LOAD_SHIFT);
  return 0;
#endif
}

/* signals that a live database is running */
uint8_t database_running() {
  uint32_t q = 0;
#ifdef _WIN32
  DWORD processIds[1024], bytesNeeded, procCount;
  HANDLE hProcess = NULL;
  TCHAR szProcessName[MAX_PATH] = TEXT("<unknown>");
  uint32_t p = 0;
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
#elif __linux__
  for ( q = 0; q < DBPROCESS_N; q++ ) {
    if ( process_id(DBPROCESSES[q]) != -1 )
      return 1;
  }
  return 0;
#endif
}

/* total disk space in kb */
uint32_t total_disk_space() {
#ifdef _WIN32
  ULARGE_INTEGER lpFBA, lpTNB, lpTNFB;
  if ( GetDiskFreeSpaceEx(DISK_MOUNT_POINT, &lpFBA, &lpTNB, &lpTNFB) )
    return lpTNB.QuadPart/BYTESPERKB;
  return 0;
#elif __linux__
  struct statvfs stat;
  if ( statvfs(DISK_MOUNT_POINT, &stat) == 0 )
    return (stat.f_blocks * stat.f_frsize)/BYTESPERKB;
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
#elif __linux__
  int error = sysinfo(&s_info);
  if ( error == 0 )
    return s_info.totalram/BYTESPERKB;
  return 0;
#endif
}

/* current uptime */
uint32_t uptime_in_secs() {
#ifdef _WIN32
  return GetTickCount() / 1000;
#elif __linux__
  int error = sysinfo(&s_info);
  if ( error == 0 )
    return s_info.uptime;
  return 0;
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
  char sockdata[SZ_BUFFER_LEN];
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

  errorCode = getaddrinfo(NULL, DAEMON_PORT, &hints, &result);
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

  uint32_t n;
  while(1)
  {
    struct sockaddr_in sa = {0};
    socklen_t socklen = sizeof(sa);
    responder = accept(listener, (struct sockaddr *) &sa, &socklen);
    if (responder == INVALID_SOCKET) {
      printf("accept failed with error: %d\n", WSAGetLastError());
      closesocket(listener);
      WSACleanup();
      return 1;
    }

    do
    {
      byteCount_t = recv(responder, sockdata, sockDataLength, 0);
      if ( byteCount_t > 0 && byteCount_t <= (SZ_BUFFER_LEN - 1) )
      {
        if (strcmp(sockdata, DAEMON_KEY) == 0) {
          char *c_ipaddr = inet_ntoa(sa.sin_addr);
          printf("valid authority key provided by client: %s\n", c_ipaddr);
          char *ps = make_pulse_string();
          n = send(responder, ps, strlen(ps), 0);
          if ( n != 0 )
            printf("%s pulse sent to client %s\n", ps, c_ipaddr);
        }
        closesocket(responder);
        shutdown(responder, SD_BOTH);
        byteCount_t = 0;
      }
    }
    while(byteCount_t > 0);
    sleep_ms(200);
  }

  closesocket(listener);
  WSACleanup();
  return 0;
}
#elif __linux__
char *get_client_ip(const struct sockaddr *sa, char *ipstr, uint16_t mlen)
{
	switch(sa->sa_family){
		case AF_INET:
		inet_ntop(AF_INET, &(((struct sockaddr_in*)sa)->sin_addr), ipstr, mlen);
		break;
		case AF_INET6:
		inet_ntop(AF_INET6, &(((struct sockaddr_in6*)sa)->sin6_addr), ipstr, mlen);
		break;
		default:
		strncpy(ipstr, "Unknown AF", mlen);
		return NULL;
	}
	return ipstr;
}
int linmain() {

  int32_t sockfd, newsockfd;
  socklen_t clilen;
  char sockdata[SZ_BUFFER_LEN];
  struct sockaddr_in serv_addr, cli_addr;
  int32_t n;
  char c_ipaddr[64];

  while(1) {
  	sockfd = socket(AF_INET, SOCK_STREAM, 0);
  	if ( sockfd < 0 ) {
  		perror("socket opening ERROR");
  		continue;
  	}
  	bzero((char*)&serv_addr, sizeof(serv_addr));
  	serv_addr.sin_family = AF_INET;
  	serv_addr.sin_addr.s_addr = INADDR_ANY;
  	serv_addr.sin_port = htons(atoi(DAEMON_PORT));
  	if ( bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0 ) {
  		perror("socket binding ERROR");
  		close(sockfd);
  		continue;
  	}
  	printf("awaiting connection(s)...\n");
  	listen(sockfd, 10);
  	clilen = sizeof(cli_addr);
  	newsockfd = accept(sockfd, (struct sockaddr*)&cli_addr, &clilen);
  	if ( newsockfd < 0 ) {
  		perror("socket accept ERROR");
  		close(sockfd);
  		continue;
  	}
  	bzero(sockdata, SZ_BUFFER_LEN);
  	n = read(newsockfd, sockdata, SZ_BUFFER_LEN-1);
  	if ( n < 0 ) {
  		perror("socket read ERROR");
  		close(newsockfd);
  		close(sockfd);
  		continue;
  	}  	
  	bzero(c_ipaddr, 64);
  	if ( strcmp(sockdata, DAEMON_KEY) == 0 ) {
  		/* ipv6 addresses consist of a max 39 characters */
  		get_client_ip((struct sockaddr*)&cli_addr, c_ipaddr, 64);
  		printf("valid authority key provided by client: %s\n", c_ipaddr);
  		char* pulsestr = make_pulse_string();
  		n = write(newsockfd, pulsestr, strlen(pulsestr));
  		if ( n < 0 )
  			perror("socket write ERROR");
  		else
  			printf("%s pulse sent to client %s\n", pulsestr, c_ipaddr);
  	}
  	else
  		printf("invalid authority key provided by client: %s\n", c_ipaddr);
  	close(newsockfd);
  	close(sockfd);
  	sleep_ms(200);
  }
  return 0;
}
#endif

int main() {
  printf("NSL Pulse Server (refer to https://www.nullox.com/docs/pulse/ for documentation)\n\n");
  printf("BTC Donation: 3NQBVhxMrJpVeCViZNiHLouLgrCUXbL18C\n");
  printf("ETH Donation: 0x4C11E15Df5483Fd94Ae474311C9741041eB451ed\n");
  printf("VRSC Donation: RMm4wJ74eHBzuXhJ9MLsAvUQP925YmDUnp\n\n");
#ifdef _WIN32
  return winmain();
#elif __linux__
  return linmain();
#endif
}
