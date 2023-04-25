#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
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

const int SEND_TIMES = 3;
const int BUF_SIZE = 100;

/**
 * @brief Parse host:port like string
 *
 * @param str host:port like string
 * @param host host variable
 * @param port port variable
 */
void parse_ip_port(const char *str, char *host, char *port) {
  int len = strlen(str);

  const char *colon = strrchr(str, ':');
  if (colon != NULL) {
    int host_len = colon - str;
    strncpy(host, str, host_len);
    *(host + host_len) = '\0';
    strncpy(port, colon + 1, len - host_len - 1);
  } else {
    printf("Error: Invalid ip:port format\n");
  }
}

/**
 * @brief parse the host and the port into addrinfo struct
 *
 * @param host
 * @param port
 * @return
 */
addrinfo *host2addr(const char *host, const char *port) {
  struct addrinfo hints, *res;
  int status;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;    // IPv4 or IPv6
  hints.ai_socktype = SOCK_DGRAM; // UDP

  if ((status = getaddrinfo(host, port, &hints, &res)) != 0) {
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
    return nullptr;
  }

  return res;
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
  char port[BUF_SIZE];
  parse_ip_port(argv[1], host, port);
  printf("Host %s, and port %s.\n", host, port);

  // Get server address
  addrinfo *serv_addr = host2addr(host, port);
  if (!serv_addr) {
    return -1;
  }

  // Create UDP socket
  int sock = socket(serv_addr->ai_family, serv_addr->ai_socktype, serv_addr->ai_protocol);
  if (sock < 0) {
    printf("Socket creation error\n");
    return -1;
  }

  int tmp_sock;
  struct sockaddr_storage local_addr;
  socklen_t local_addrlen = sizeof(local_addr);

  if ((tmp_sock = socket(serv_addr->ai_family, serv_addr->ai_socktype, serv_addr->ai_protocol)) < 0) {
    printf("Socket creation error\n");
    return -1;
  }

  if (connect(tmp_sock, serv_addr->ai_addr, serv_addr->ai_addrlen) < 0) {
    puts("Connect error");
    close(tmp_sock);
    return -1;
  }

  if (getsockname(tmp_sock, (struct sockaddr *)&local_addr, &local_addrlen) < 0) {
    perror("getsockname error");
    exit(EXIT_FAILURE);
  }

  char local_ipstr[INET6_ADDRSTRLEN];
  int local_port;

  if (local_addr.ss_family == AF_INET) {
    struct sockaddr_in *s = (struct sockaddr_in *)&local_addr;
    local_port = ntohs(s->sin_port);
    inet_ntop(AF_INET, &s->sin_addr, local_ipstr, sizeof(local_ipstr));
  } else {
    struct sockaddr_in6 *s = (struct sockaddr_in6 *)&local_addr;
    local_port = ntohs(s->sin6_port);
    inet_ntop(AF_INET6, &s->sin6_addr, local_ipstr, sizeof(local_ipstr));
  }

#ifdef DEBUG
  printf("Connected to  %s:%s local %s:%d\n", host, port, local_ipstr, local_port);
#endif

  close(tmp_sock); // Release the trail socket

  // Bind local address
  if (bind(sock, (sockaddr *)&local_addr, local_addrlen) == -1) {
    perror("bind error");
    exit(EXIT_FAILURE);
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
    if (sendto(sock, &reqMsg, sizeof(reqMsg), 0, serv_addr->ai_addr, serv_addr->ai_addrlen) < 0) {
      puts("Send failed");
      return -1;
    }
    send_cnt++;

    int n = select(sock + 1, &readfds, NULL, NULL, &timeout);

    if (n < 0) {
      puts("Select error");
      return -1;
    } else if (n == 0) {
      if (send_cnt >= SEND_TIMES) {
        puts("Timeout error");
        return -1;
      }
      continue;
    } else {
      // Receive calculation protocol message
      memset(&calcProtocolMsg, 0, sizeof(calcProtocolMsg));
      ssize_t recv_size = 0;
      socklen_t len = serv_addr->ai_addrlen;
      if ((recv_size = recvfrom(sock, &calcProtocolMsg, sizeof(calcProtocolMsg), 0, serv_addr->ai_addr, &len)) < 0) {
        printf("Receive error\n");
        return -1;
      } else if (recv_size == sizeof(calcMessage) && ntohl(((calcMessage *)&calcProtocolMsg)->message) == 2) {
        puts("NOT OK");
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
    if (sendto(sock, &calcProtocolMsg, sizeof(calcProtocolMsg), 0, serv_addr->ai_addr, serv_addr->ai_addrlen) < 0) {
      printf("Send failed\n");
      return -1;
    }
    send_cnt++;

    int n = select(sock + 1, &readfds, NULL, NULL, &timeout);

    if (n < 0) {
      puts("Select error");
      return -1;
    } else if (n == 0) {
      if (send_cnt >= SEND_TIMES) {
        puts("Timeout error");
        return -1;
      }
      continue;
    } else {
      // Receive result message
      calcMessage resMsg;
      memset(&resMsg, 0, sizeof(resMsg));
      socklen_t len = serv_addr->ai_addrlen;
      if (recvfrom(sock, &resMsg, sizeof(resMsg), 0, serv_addr->ai_addr, &len) < 0 || ntohl(resMsg.message) != 1) {
        printf("NOT OK\n");
        return -1;
      } else if (ntohl(resMsg.message) == 1) {
        printf("OK\n");
      }

      break;
    }
  }

  // Release the resources
  close(sock);
  freeaddrinfo(serv_addr);
}
