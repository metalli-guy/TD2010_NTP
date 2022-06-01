#include "includesdefinesvariables.h"
#include "mbed.h"
Timer runtimer;
Timer offsettimer;
EthernetInterface net;
typedef struct {
  uint8_t li_vn_mode;
  uint8_t stratum;
  uint8_t poll;
  uint8_t precision;
  uint32_t rootDelay;
  uint32_t rootDispersion;
  uint32_t refId;
  uint32_t refTm_s;
  uint32_t refTm_f;
  uint32_t origTm_s;
  uint32_t origTm_f;
  uint32_t rxTm_s;
  uint32_t rxTm_f;
  uint32_t txTm_s;
  uint32_t txTm_f;
} ntp_packet;
int NTP_gettime() {
  //runtimer.start();
  // defining packet to be sent, imperative that we send time(NULL) for txTM_s,
  // lets us calculate offset and delay
  ntp_packet packet = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, time(NULL), 0};
  //
  memset(&packet, 0, sizeof(ntp_packet));
  //
  // li = 0, vn = 3, and mode = 3
  *((char *)&packet + 0) = 0x1b;
  //
  // opening sockets and connecting to static ip & NTP server
  SocketAddress sockAddr;
  int i = net.set_network(IP, MASK, GATEWAY);
  i = net.connect();
  //net.get_ip_address(&sockAddr);
  UDPSocket sock;
  sock.open(&net);
  net.gethostbyname("192.168.2.247", &sockAddr);
  // net.gethostbyname("0.pool.ntp.org", &sockAddr);
  sockAddr.set_port(123);
  //
  // sending & receiving data, defining timer for packet to depart and return
  offsettimer.start();
  if (0 > sock.sendto(sockAddr, (char *)&packet, sizeof(ntp_packet))) {
    printf("Error sending data");
    return -1;
  }
  sock.recvfrom(&sockAddr, (char *)&packet, sizeof(ntp_packet));
  offsettimer.stop();
  uint32_t timerduration = offsettimer.read_ms();
  offsettimer.reset();
  //
  // converting network byte order to host byte order
  packet.origTm_s = ntohl(packet.origTm_s);
  packet.origTm_f = ntohl(packet.origTm_f);
  packet.txTm_s = ntohl(packet.txTm_s);
  packet.txTm_f = ntohl(packet.txTm_f);
  packet.rxTm_s = ntohl(packet.rxTm_s);
  packet.rxTm_f = ntohl(packet.rxTm_f);
  packet.txTm_f = packet.txTm_f * MILLIS_CONVERSION_CONSTANT;
  ThisThread::sleep_for(1000 - packet.txTm_f);
  //int32_t fractionaltime = packet.txTm_f;
  uint32_t destTm_s = (time(NULL));
  uint32_t t1 = packet.origTm_s;
  uint32_t t2 = packet.rxTm_s;
  uint32_t t3 = packet.txTm_s;
  uint32_t t4 = destTm_s;
  uint32_t t5 = packet.origTm_f;
  uint32_t t6 = packet.rxTm_f;
  uint32_t t7 = packet.txTm_f;
  uint32_t t8 = timerduration;
  t8 = t8 / MILLIS_CONVERSION_CONSTANT;
  int32_t offset = (((t2 - t1) + (t3 - t4)) / 2) * MILLIS_CONVERSION_CONSTANT;
  int32_t delay = (((t4 - t1) - (t3 - t2))) * MILLIS_CONVERSION_CONSTANT;
  int32_t offset_second = (offset + delay);
  int32_t offset_f = (((t6 - t5) + (t7 - t8)) / 2) * MILLIS_CONVERSION_CONSTANT;
  int32_t delay_f = (((t8 - t5) - (t7 - t6))) * MILLIS_CONVERSION_CONSTANT;
  int32_t offset_fractional = offset_f + delay_f;
  double totaloffset = offset_second + offset_fractional;
  // printf("totaloffset %u \n", totaloffset);
  int32_t totaloffset_s = totaloffset / 1000;
  /*int32_t totaloffset_f = totaloffset / 1000;
  double intPart;
  double fractPart = modf(totaloffset,&intPart);
  printf("intpart %f \n", &intPart);
  if (totaloffset_f < 0) {
    totaloffset_f = 1 - abs(totaloffset_f);
  }
  */
  // printf("totaloffset_s %u \n", totaloffset_s- packet.txTm_f );
  //printf("totaloffset_f %u \n", totaloffset_f);

  //printf("txtm_f %u\n", packet.txTm_f);
  //runtimer.stop();
  //uint32_t time = runtimer.read_ms();
  //printf("run time = %u \n", time);
  //runtimer.reset();
  int32_t nextSecondTime = abs(1000 - totaloffset_s);
  uint32_t nextSecondTime_s = nextSecondTime / 1000;
  uint32_t nextSecondTime_f = nextSecondTime % 1000;
  //printf("nextSecondTime_s %u \n", nextSecondTime_s);
  //printf("nextSecondTime_f %u \n", nextSecondTime_f);
  // printf("sleeping for %u\n", nextSecondTime + totaloffset_f);
  ThisThread::sleep_for(nextSecondTime_f);
  time_t txTm = packet.txTm_s - NTP_TIMESTAMP_DELTA + totaloffset_s +
                nextSecondTime_s + 1;
  char buffer[32];
  strftime(buffer, 32, "%a,%d %B %Y %H:%M:%S", localtime(&txTm));
  
  uint32_t millisecond = 000;
  txTm = txTm +1;
  //printf("%s:%u\n", buffer, millisecond);
  set_time(txTm);
  nextSecondTime = 0;
  totaloffset = 0;
  totaloffset_s = 0;
  nextSecondTime = 0;
  nextSecondTime_s = 0;
  nextSecondTime_f = 0;
  //fractionaltime = 0;
  sock.close();
  net.disconnect();
  packet = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  return 0;

  // printf("t1 %u\n", t1);
  // printf("t2 %u\n", t2);
  // printf("t3 %u\n", t3);
  // printf("t4 %u\n", t4);
  // printf("t5 %u\n", t5);
  // printf("t6 %u\n", t6);
  // printf("t7 %u\n", t7);
  // printf("t8 %u\n", t8);
  // printf("t8 new %u\n", t8);

  // printf("packet.txTm_f %u\n", packet.txTm_f);
  // printf("offset_f %u\n", offset_f);
  // printf("delay_f %u\n", delay_f);

  // printf("totaltimecorrection_s %u \n", totaltimecorrection_s);
  // printf("totaltimecorrection_f %u \n", totaltimecorrection_f);
  //
  // defining the waiting time for PPS start, defining sleep
  // uint32_t waittimer = (packet.txTm_f + totaloffset_f);
  // printf("waittimer %u\n", waittimer);
  //
  // uint32_t waittimer_f = waittimer % 1000;
  // printf("waittimer_f %u\n", waittimer_f);
  // uint32_t waittimer_s = waittimer / 1000;
  // printf("waittimer_s %u\n", waittimer_s);

  // uint32_t nextSecondTime = 2000 - waittimer_f - runtime_f;
  // time_t txTm =
  // packet.txTm_s - NTP_TIMESTAMP_DELTA + waittimer_s + runtime_s + 1;
  // printf("%s:%u\n", buffer, waittimer_f);
  // printf("NTP Time = %s:%u\n", buffer, nextSecondTime);

  // totalOffset = (totalOffset * MILLIS_CONVERSION_CONSTANT);
  // printf("delay %u\n", delay);
  // printf("offset %u\n", offset);
  // printf("offset_s %u\n", offset_s);

  //
}