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
  char *host = strtok(argv[1], ":");
  int port = atoi(strtok(NULL, ":"));

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

  // Connect to server
  if (connect(sock, (struct sockaddr *)serv_addr, sizeof(*serv_addr)) < 0) {
    printf("Connection Failed\n");
    return -1;
  }

  // Send request
  calcMessage reqMsg = {htons(22), htons(0), htons(17), htons(1), htons(0)};
  if (send(sock, &reqMsg, sizeof(reqMsg), 0) < 0) {
    printf("Send failed\n");
    return -1;
  }

  // Receive calculation protocol message
  calcProtocol calcProtocolMsg;
  memset(&calcProtocolMsg, 0, sizeof(calcProtocolMsg));
  if (recv(sock, &calcProtocolMsg, sizeof(calcProtocolMsg), 0) < 0) {
    printf("NOT OK\n");
    return -1;
  }

  // Calculate the result
  calculate(&calcProtocolMsg);

  // Send result
  if (send(sock, &calcProtocolMsg, sizeof(calcProtocolMsg), 0) < 0) {
    printf("Send failed\n");
    return -1;
  }

  std::cout << ntohl(calcProtocolMsg.arith) << std::endl;
  std::cout << ntohl(calcProtocolMsg.inValue1) << " " << ntohl(calcProtocolMsg.inValue2) << " " << ntohl(calcProtocolMsg.inResult) << std::endl;
  std::cout << calcProtocolMsg.flValue1 << " " << calcProtocolMsg.flValue2 << " " << calcProtocolMsg.flResult << std::endl;

  // Receive result message
  calcMessage resMsg;
  memset(&resMsg, 0, sizeof(resMsg));
  if (recv(sock, &resMsg, sizeof(resMsg), 0) < 0 || ntohl(resMsg.message) != 1) {
    printf("NOT OK\n");
    return -1;
  } else if (ntohl(resMsg.message) == 1) {
    printf("OK\n");
  }
}
