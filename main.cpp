#include "mbed.h"
#include "rtos.h"
extern "C" void mbed_reset();
#include "2010daq.h"
#include "ntp_gettime.h"
#include "includesdefinesvariables.h"
//#include "gps.h"

LocalFileSystem local("local");

int main() {
  // run bonereset Thread
  // Thread boneResetThread(boneReset);

  if (getResetStateFromFile() == 1) {
    g_validSize = 2;
    g_reSyncAct = 1;
    g_timeoutDuration = 10;
  } else
    // wait(3);

    char mbedIp[16], maskIp[16], gatewayIp[16], boneIp[16];
  FILE *fp = fopen("/local/ip.txt", "r");
  // fscanf(fp,"%s %s %s %s",mbedIp,maskIp,gatewayIp,boneIp);
  // printf("%s:%s:%s:%s\r\n",mbedIp,maskIp,gatewayIp,boneIp);
  fclose(fp);

  // wait(35);
  // ip set and socket configuration
  EthernetInterface eth;
  // eth.init(mbedIp,maskIp,gatewayIp);
  eth.connect();

  UDPSocket sock;
  // sock.init();
  // Endpoint host;
  // host.set_address(boneIp,33333);
  // sock.set_blocking(false,0);

  // manage socket binding
  manage_socket.bind(22222);

  // init for manage
  memset(comeData, 0, sizeof(comeData));
  memset(gain_value, 0, sizeof(gain_value));
  // Thread managerThread(managerFunc);

  // data buffer init
  int buffer = 0; // which buffer 0 or 1
  char *sptr;     // send pointer
  buffer = 0;
  bptr = buffer0;
  sptr = NULL;
  sendready = 0;

  // data buffer memset
  memset(buffer0, 0, sizeof(buffer0));
  memset(buffer1, 0, sizeof(buffer1));

  // pga spi init
  pga_spi.format(8, 2);
  pga_spi.frequency(100000);

  // pga set and read
  char defaultPga[nOfChan];
  memset(defaultPga, default_pga_register, sizeof(defaultPga));
  read_PgaValues_FromFile(defaultPga, sizeof(defaultPga)); //**************yeni
  setPgaValues(defaultPga);
  readPgaValues();

  // cpy pga data
  memcpy(buffer0 + 16, gain_value, nOfChan);
  memcpy(buffer1 + 16, gain_value, nOfChan);

  // adc spi init
  adc_spi.format(8, 2);
  adc_spi.frequency(16000000);

  // gps init
  // ppsIn.rise(&ppsCallback);
  // gps.baud(38400);
  // wait(2);
  if (g_reSyncAct != 1)
    // setgps();

    do_synchronize();

  while (1) {

    if (sendready == 1) {
      sendready = 0;

      if (buffer == 0) {
        buffer = 1;
        bptr = buffer1;
        sptr = buffer0;
      } else {
        buffer = 0;
        bptr = buffer0;
        sptr = buffer1;
      }

      for (int k = 0; k < 10; k++) {
        // sock.sendTo(host,sptr,bsize);
        // wait(0.0002);
      }

      if (pindex > 4294967290)
        mbed_reset();
    }
  }
}