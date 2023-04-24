#include <arpa/inet.h>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "calcLib.h"

#include "protocol.h"

#define DEBUG

const int gMaxSendTimes = 3;
const int BUF_SIZE = 100;
const int LOCAL_PORT = 12345; // Local listening port

void parse_ip_port(const char *str, char *host, int *port) {
  const char *colon = strrchr(str, ':');
  if (colon != NULL) {
    int len = colon - str;
    strncpy(host, str, len);
    *(host + len) = '\0';
    *port = atoi(colon + 1);
  } else {
    printf("Error: Invalid ip:port format\n");
  }
}

/**
 * @brief Translate host to IP
 *
 */
sockaddr_in *host2addr(const char *host, const int port) {
  hostent *hostinfo;
  in_addr **addr_list;
  sockaddr_in *serv_addr;

  serv_addr = (sockaddr_in *)malloc(sizeof(sockaddr_in));
  memset(serv_addr, 0, sizeof(sockaddr_in));

  hostinfo = gethostbyname(host);
  if (!hostinfo) {
    printf("Invalid address/ Address not supported\n");
    return NULL;
  }

  addr_list = (struct in_addr **)hostinfo->h_addr_list;

  // Get IP address
  char *ip = inet_ntoa(*addr_list[0]);

  if (inet_pton(AF_INET, ip, &(serv_addr->sin_addr)) <= 0) {
    printf("Invalid address/ Address not supported\n");
    return NULL;
  }
  serv_addr->sin_family = AF_INET;
  serv_addr->sin_port = htons(port);

  return serv_addr;
}

/**
 * @brief Calculation the result
 *
 * @param calcProtocolMsg
 */
void calculate(calcProtocol *calcProtocolMsg) {
  uint32_t arith = ntohl(calcProtocolMsg->arith);
  if (arith == 1) {
    calcProtocolMsg->inResult = htonl(ntohl(calcProtocolMsg->inValue1) + ntohl(calcProtocolMsg->inValue2));
  } else if (arith == 2) {
    calcProtocolMsg->inResult = htonl(ntohl(calcProtocolMsg->inValue1) - ntohl(calcProtocolMsg->inValue2));
  } else if (arith == 3) {
    calcProtocolMsg->inResult = htonl(ntohl(calcProtocolMsg->inValue1) * ntohl(calcProtocolMsg->inValue2));
  } else if (arith == 4) {
    calcProtocolMsg->inResult = htonl(ntohl(calcProtocolMsg->inValue1) / ntohl(calcProtocolMsg->inValue2));
  } else if (arith == 5) {
    calcProtocolMsg->flResult = calcProtocolMsg->flValue1 + calcProtocolMsg->flValue2;
  } else if (arith == 6) {
    calcProtocolMsg->flResult = calcProtocolMsg->flValue1 - calcProtocolMsg->flValue2;
  } else if (arith == 7) {
    calcProtocolMsg->flResult = calcProtocolMsg->flValue1 * calcProtocolMsg->flValue2;
  } else if (arith == 8) {
    calcProtocolMsg->flResult = calcProtocolMsg->flValue1 / calcProtocolMsg->flValue2;
  }
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("Usage: %s <host>:<port>", argv[0]);
    return -1;
  }

  // Parse host and port
  char host[BUF_SIZE];
  int port;
  parse_ip_port(argv[1], host, &port);

  printf("Host %s, and port %d.\n", host, port);

  // Create UDP socket
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    printf("Socket creation error\n");
    return -1;
  }

  // Get server address
  sockaddr_in *serv_addr = host2addr(host, port);
  if (!serv_addr) {
    return -1;
  }

#ifdef DEBUG
  // Get the local IP and port
  int trial_sock = socket(AF_INET, SOCK_DGRAM, 0);

  if (connect(trial_sock, (struct sockaddr *)serv_addr, sizeof(*serv_addr)) < 0) {
    printf("Connection Failed\n");
    return -1;
  }

  sockaddr_in addr;
  socklen_t len = sizeof(addr);

  getsockname(trial_sock, (struct sockaddr *)&addr, &len);

  char *local_host = inet_ntoa(addr.sin_addr);
  uint16_t local_port = ntohs(addr.sin_port);

  printf("Connected to  %s:%d local %s:%d\n", host, port, local_host, local_port);
  close(trial_sock);
#endif

  // Bind the local address
  struct sockaddr_in local_addr;
  local_addr.sin_family = AF_INET;
  local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  local_addr.sin_port = htons(LOCAL_PORT);

  if (bind(sock, (struct sockaddr *)&local_addr, sizeof(local_addr)) == -1) { // 绑定本地地址和端口号
    puts("Bind error");
    return -1;
  }

  // Set up fd sets
  fd_set readfds;
  FD_ZERO(&readfds);
  FD_SET(sock, &readfds);

  // Set up timeout struct
  timeval timeout;
  timeout.tv_sec = 2;
  timeout.tv_usec = 0;

  int send_cnt = 0;
  calcProtocol calcProtocolMsg;
  while (true) {
    // send request message
    calcMessage reqMsg = {htons(22), htons(0), htons(17), htons(1), htons(0)};
    if (sendto(sock, &reqMsg, sizeof(reqMsg), 0, (sockaddr *)serv_addr, sizeof(*serv_addr)) < 0) {
      printf("Send failed\n");
      return -1;
    }
    send_cnt++;

    int n = select(sock + 1, &readfds, NULL, NULL, &timeout);

    if (n < 0) {
      puts("Select error");
      return -1;
    } else if (n == 0) {
      if (send_cnt >= gMaxSendTimes) {
        puts("Timeout error");
        return -1;
      }
      continue;
    } else {
      // Receive calculation protocol message
      memset(&calcProtocolMsg, 0, sizeof(calcProtocolMsg));
      socklen_t len = sizeof(*serv_addr);
      if (recvfrom(sock, &calcProtocolMsg, sizeof(calcProtocolMsg), 0, (sockaddr *)serv_addr, &len) < 0) {
        // Version check
        printf("NOT OK\n");
        return -1;
      }

      break;
    }
  }

  // Calculate the result
  calculate(&calcProtocolMsg);

  send_cnt = 0;
  while (true) {
    // Send result
    if (sendto(sock, &calcProtocolMsg, sizeof(calcProtocolMsg), 0, (sockaddr *)serv_addr, sizeof(*serv_addr)) < 0) {
      printf("Send failed\n");
      return -1;
    }
    send_cnt++;

    int n = select(sock + 1, &readfds, NULL, NULL, &timeout);

    if (n < 0) {
      puts("Select error");
      return -1;
    } else if (n == 0) {
      if (send_cnt >= gMaxSendTimes) {
        puts("Timeout error");
        return -1;
      }
      continue;
    } else {
      // Receive result message
      calcMessage resMsg;
      memset(&resMsg, 0, sizeof(resMsg));
      socklen_t len = sizeof(*serv_addr);
      if (recvfrom(sock, &resMsg, sizeof(resMsg), 0, (sockaddr *)serv_addr, &len) < 0 || ntohl(resMsg.message) != 1) {
        printf("NOT OK\n");
        return -1;
      } else if (ntohl(resMsg.message) == 1) {
        printf("OK\n");
      }

      break;
    }
  }

  close(sock);
}
