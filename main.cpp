#include "EthernetInterface.h"
#include "LWIPStack.h"
#include "mbed.h"
#include <SocketAddress.h>
#include <chrono>
extern "C" void mbed_reset();
//#include "rtos.h"
#define NTP_TIMESTAMP_DELTA 2208978000
#define MILLIS_CONVERSION_CONSTANT 0.00000023283064365386962890625
#define LI(packet) (uint8_t)((packet.li_vn_mode & 0xC0) >> 6)
#define VN(packet) (uint8_t)((packet.li_vn_mode & 0x38) >> 3)
#define MODE(packet) (uint8_t)((packet.li_vn_mode & 0x07) >> 0)
// device configuration
#define nOfChan 16
#define gnumber 25
#define bsize                                                                  \
  24 + gnumber * 3 * nOfChan // header=max_ch_size+12byte gpsBilgisi+4Counter=36
#define srate                                                                  \
  0x60 // 250->0x26 500->0x20 1000->0x40 2000->0x60 4000->0x80 8000->0xA0
       // 16000->0x00
#define default_pga_register 0x10
#define pgaDir "/local/pga.txt"
#define resetDir "/local/reset.txt"
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

void managerFunc();
void resetADC(void);
void readADC(void);
void PGA_config(char komut);
void setPgaValues(char *pgaArray);
void readPgaValues(void);
void write_PgaValues_ToFile(char *str, size_t len);
void read_PgaValues_FromFile(char *str, size_t len);
void createResetFile(void);
int getResetStateFromFile(void);
int NTP_gettime();
void readADC2(void);
void sendfunc();
// manage socket
UDPSocket manage_socket;
SocketAddress manage_client;
EthernetInterface eth;
SocketAddress host;
UDPSocket sock;

Timer offsettimer;

char comeData[128];
char *bptr; // buffer pointer
char buffer0[bsize];
int buffer = 0;
int sendready = 0;       // data send ready?
int index = 0;           // index for buffers up to gumber;
unsigned int pindex = 0; // udp packet index
// pga value
unsigned char pga_bits[nOfChan];
char gain_value[nOfChan];
// adc spi-cs pin
SPI adc_spi(p11, p12, p13); // mosi, miso, sclk
DigitalOut adc_cs(p14);
// pga pin definitions
SPI pga_spi(p5, p6, p7);    // mosi,miso,sclk
DigitalOut pga_dataout(p9); // Shift Register data pin
DigitalOut pga_clk(p10);    // Shift Register clk pin
// bilgi pinleri
DigitalOut readyLed(p16); // p21
// initialize for bone resetting
DigitalIn boneReady(p20);
// adc interrupt pin
InterruptIn button(p15);
LocalFileSystem local("local");

int main() {
  char mbedIp[16], maskIp[16], gatewayIp[16], boneIp[16];
  FILE *fp = fopen("/local/ip.txt", "r");
  fscanf(fp, "%s %s %s %s", mbedIp, maskIp, gatewayIp, boneIp);
  fclose(fp);

  // ip set and socket configuration
  int i = eth.set_network(mbedIp, maskIp, gatewayIp);
  i = eth.connect();
  sock.open(&eth);
  sock.set_blocking(false);
  host.set_ip_address(boneIp);
  host.set_port(33333);
  // manage socket binding
  manage_socket.open(&eth);
  manage_socket.set_blocking(false);
  manage_socket.bind(22222);
  printf("socket bound\n\r");
  // init for manage
  int come_len =
      manage_socket.recvfrom(&manage_client, comeData, sizeof(comeData));
  memset(gain_value, 0, sizeof(gain_value));
  // data buffer init

  bptr = buffer0;
  sendready = 0;

  // data buffer memset
  memset(buffer0, 0, sizeof(buffer0));

  // pga spi init
  pga_spi.format(8, 2);
  pga_spi.frequency(100000);

  // pga set and read
  char defaultPga[nOfChan];
  memset(defaultPga, default_pga_register, sizeof(defaultPga));
  read_PgaValues_FromFile(defaultPga, sizeof(defaultPga));
  setPgaValues(defaultPga);
  readPgaValues();

  // cpy pga data
  memcpy(buffer0 + 16, gain_value, nOfChan);
  // adc spi init
  adc_spi.format(8, 2);
  adc_spi.frequency(16000000);
  pindex = 0;
  index = 0;
  // reset adc
  adc_cs = 0;
  adc_spi.write(0x60);
  for (int k = 0; k < nOfChan / 4; k++)
    adc_spi.write(0xf0);
  adc_cs = 1;
  // disable crystal
  adc_cs = 0;
  adc_spi.write(0x60);
  adc_spi.write(0x20);
  for (int k = 0; k < nOfChan / 4 - 1; k++)
    adc_spi.write(0x20);
  adc_cs = 1;
  // configure sample rate
  adc_cs = 0;
  adc_spi.write(0x50);
  adc_spi.write(srate);
  adc_spi.write(0x00);
  adc_cs = 1;
  NTP_gettime();
  adc_cs = 0;
  adc_spi.write(0xF0);
  for (int i = 0; i < 3 * nOfChan; i++) {
    bptr[24 + 3 * nOfChan * index + i] = adc_spi.write(0x00);
  }
  adc_cs = 1;
  readyLed.write(1);
  while (1) {
    if (come_len > 0) {
      memset(comeData, 0, sizeof(comeData));
      if (comeData[0] == 0x01) {
        write_PgaValues_ToFile(comeData + 1, nOfChan);
        mbed_reset();
      }
    }
    if (button.read() == 0) {
      adc_cs = 0;
      adc_spi.write(0xF0);
      for (int i = 0; i < 3 * nOfChan; i++) {
        bptr[24 + 3 * nOfChan * index + i] = adc_spi.write(0x00);
      }
      adc_cs = 1;
      index++;
      if (index == gnumber) {
        index = 0;
        pindex++;
        bptr[0] = (pindex & 0xff000000) >> 24;
        bptr[1] = (pindex & 0xff0000) >> 16;
        bptr[2] = (pindex & 0xff00) >> 8;
        bptr[3] = (pindex & 0xff) >> 0;
        sendready = 1;
        if (sendready == 1) {
          sendready = 0;
          for (int k = 0; k < 2; k++) {
            sock.sendto(host, bptr, bsize);
          }
          if (pindex > 4294967290) {
            mbed_reset();
          }
        }
      }
    }
  }
}

void PGA_config(char komut) {
  pga_spi.write(0x41);
  pga_spi.write(0x01);
  pga_spi.write(0x42);
  pga_spi.write(0x01);
  pga_spi.write(0x00); // ****
  pga_spi.write(0x48);
  pga_spi.write(0xFF);
  pga_spi.write(0x00); // ****
  pga_spi.write(0x40);
  pga_spi.write(komut);
  pga_spi.write(0x00); // ****
}

// write pga for all channels

void setPgaValues(char *pgaArray) {
  for (int j = 0; j < nOfChan; j++) {
    for (int i = 0; i < nOfChan; i++) {
      pga_bits[i] = (i == j) ? 0 : 1;
    }
    for (int i = nOfChan - 1; i >= 0; i--) {
      pga_clk = 0;
      pga_dataout = pga_bits[i];
      pga_clk = 1;
    }
    PGA_config(pgaArray[j]);
  }
}

// read pga for all channels

void readPgaValues(void) {
  for (int j = 0; j < nOfChan; j++) {
    for (int i = 0; i < nOfChan; i++) {
      pga_bits[i] = (i == j) ? 0 : 1;
    }
    for (int i = nOfChan - 1; i >= 0; i--) {
      pga_clk = 0;
      pga_dataout = pga_bits[i];
      pga_clk = 1;
    }
    pga_spi.write(0x80);
    gain_value[j] = pga_spi.write(0x00);
  }
}

void write_PgaValues_ToFile(char *str, size_t len) {
  FILE *fp = fopen(pgaDir, "w");
  fwrite(str, sizeof(char), len, fp);
  fclose(fp);
}

void read_PgaValues_FromFile(char *str, size_t len) {
  FILE *fp = fopen(pgaDir, "r");
  if (fp != NULL) {
    fread(str, sizeof(char), len, fp);
    fclose(fp);
  }
}

void createResetFile(void) {
  FILE *fp = fopen(resetDir, "w");
  fclose(fp);
}

int getResetStateFromFile(void) {
  FILE *fp = fopen(resetDir, "r");
  if (fp != NULL) {
    fclose(fp);
    remove(resetDir);
    return 1;
  }
  return 0;
}

int NTP_gettime() {
  printf("gettime init \n\r");
  ntp_packet packet = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, time(NULL), 0};
  //
  memset(&packet, 0, sizeof(ntp_packet));
  //
  // li = 0, vn = 3, and mode = 3
  *((char *)&packet + 0) = 0x1b;
  //
  // opening sockets and connecting to static ip & NTP server
  SocketAddress sockAddr;
  UDPSocket socket;
  socket.open(&eth);
  eth.gethostbyname("192.168.2.247", &sockAddr);
  sockAddr.set_port(123);
  // sending & receiving data, defining timer for packet to depart and return
  offsettimer.start();
  if (0 > socket.sendto(sockAddr, (char *)&packet, sizeof(ntp_packet))) {
    printf("Error sending data");
    return -1;
  }
  socket.recvfrom(&sockAddr, (char *)&packet, sizeof(ntp_packet));
  offsettimer.stop();
  printf("gettime data arrived\n\r");
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
  int32_t totaloffset_s = totaloffset / 1000;
  int32_t nextSecondTime = abs(1000 - totaloffset_s);
  uint32_t nextSecondTime_s = nextSecondTime / 1000;
  uint32_t nextSecondTime_f = nextSecondTime % 1000;

  ThisThread::sleep_for(nextSecondTime_f);
  time_t txTm = packet.txTm_s - NTP_TIMESTAMP_DELTA + totaloffset_s +
                nextSecondTime_s + 1;
  char buffertime[32];
  strftime(buffertime, 32, "%a,%d %B %Y %H:%M:%S", localtime(&txTm));
  printf("gettime time string\n\r");
  uint32_t millisecond = 000;
  txTm = txTm + 1;
  printf("%s:%u\n", buffertime, millisecond);
  set_time(txTm);
  printf("gettime time set to local\n\r");
  nextSecondTime = 0;
  totaloffset = 0;
  totaloffset_s = 0;
  nextSecondTime = 0;
  nextSecondTime_s = 0;
  nextSecondTime_f = 0;
  socket.close();
  // net.disconnect();
  packet = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  printf("return 0\n\r");
  return 0;
}
