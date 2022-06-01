#include "EthernetInterface.h"
#include "LWIPStack.h"
#include <SocketAddress.h>
#include <chrono>
#define NTP_TIMESTAMP_DELTA 2208978000
#define MILLIS_CONVERSION_CONSTANT 0.00000023283064365386962890625
#define IP "192.168.2.240"
#define GATEWAY "192.168.2.1"
#define MASK "255.255.255.0"
#define LI(packet) (uint8_t)((packet.li_vn_mode & 0xC0) >> 6)
#define VN(packet) (uint8_t)((packet.li_vn_mode & 0x38) >> 3)
#define MODE(packet) (uint8_t)((packet.li_vn_mode & 0x07) >> 0)

