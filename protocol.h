
#ifdef __GCC_IEC_559 
#pragma message("GCC ICE 559 defined...")

#else

#error *** do not use this platform

#endif


#include <stdint.h>

/* 
   Used in both directions; if 
   server->client,type should be set to 1, 
   client->server type = 2. 
 */
struct calcProtocol{
  uint16_t type;  // What message is this, 1 = server to client, 2 client to server, 3... reserved 
  uint16_t major_version; // 1
  uint16_t minor_version; // 0 
  uint32_t arith; // What operation to perform, see mapping below. 
  int32_t inValue1; // integer value 1
  int32_t inValue2; // integer value 2
  int32_t inResult; // integer result
  double flValue1;  // float value 1
  double flValue2;  // float value 2
  double flResult;  // float result
};

  
struct calcMessage {
  //  1 - Server-to-client, Text,  2 - Server-to-client, Binary,
  // 21 - Client-to-server, Test, 22 - Client-to-server, Binary.
  uint16_t type;
  uint32_t message; // See below
  
  // Protocol, UDP = 17, TCP = 6, other values are reserved. 
  uint16_t protocol;
  uint16_t major_version; // 1
  uint16_t minor_version; // 0 

};


/* arith mapping in calcProtocol
1 - add
2 - sub
3 - mul
4 - div
5 - fadd
6 - fsub
7 - fmul
8 - fdiv

other numbers are reserved

*/


/* helloMessage.message 

   1 = OK   // Accept 
   2 = NOT OK  // Reject 

*/
