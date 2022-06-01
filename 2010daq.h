#include "EthernetInterface.h"
#include "mbed.h"

// device configuration
#define nOfChan 16
#define gnumber 25
#define bsize                                                                  \
  36 + gnumber * 3 * nOfChan // header=max_ch_size+12byte gpsBilgisi+4Counter=36
#define srate                                                                  \
  0x60 // 250->0x26 500->0x20 1000->0x40 2000->0x60 4000->0x80 8000->0xA0
       // 16000->0x00
#define default_pga_register 0x10
#define pgaDir "/local/pga.txt"
#define resetDir "/local/reset.txt"

// manage socket
UDPSocket manage_socket;
// Endpoint manage_client;
char comeData[128];

// adc spi-cs pin
SPI adc_spi(p11, p12, p13); // mosi, miso, sclk
DigitalOut adc_cs(p14);

// data configuration
char *bptr;              // buffer pointer
int sendready = 0;       // data send ready?
unsigned int pindex = 0; // udp packet index
int index = 0;           // index for buffers up to gumber;
char buffer0[bsize];
char buffer1[bsize];

// pga value
unsigned char pga_bits[nOfChan];
char gain_value[nOfChan];

// pga pin definitions
SPI pga_spi(p5, p6, p7);    // mosi,miso,sclk
DigitalOut pga_dataout(p9); // Shift Register data pin
DigitalOut pga_clk(p10);    // Shift Register clk pin

// bilgi pinleri
DigitalOut configLed(LED1);
DigitalOut readyLed(p16); // p21

// initialize for bone resetting
DigitalOut resetBoneLed(LED2);
DigitalOut resetBone(p30);
DigitalIn boneReady(p20);
int readyCount = 0;

// adc interrupt pin
InterruptIn button(p15);

// variables for reSync
// int g_validSize = 5;
// int g_reSyncAct = 0;
// int g_timeoutDuration = 30;

// readyThread
Thread *readyThread;

void do_synchronize(void);
void boneReset(void const *boneResetter);
void readyFunction(void const *argument);
void PGA_config(char komut);
void setPgaValues(char *pgaArray);
void readPgaValues(void);
void write_PgaValues_ToFile(char *str, size_t len);
void read_PgaValues_FromFile(char *str, size_t len);
void createResetFile(void);
int getResetStateFromFile(void);
void readADC(void);
void managerFunc(void const *argument);

void do_synchronize(void) {
  // button.fall(NULL);
  // Thread::wait(100);
  // pindex=0;
  // index=0;
  // ppsAct=0;
  // while(ppsValid && ppsAct!=1);

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

  // gps.rxBufferFlush();
  // gps.txBufferFlush();

  // GPS
  // while (!getGpsdata(g_validSize,g_timeoutDuration,g_reSyncAct));
  // copyGpsMsg(buffer0+4);
  //memcpy(buffer1, buffer0, bsize);
  // printf("copy msg: %s\r\n",buffer1+4);
  // ppsAct=0;
  // while(ppsValid && ppsAct!=1);

  button.fall(&readADC);
  readADC();
  // printf("readadc\r\n");
  // readyThread=new Thread(readyFunction);
  // g_validSize=2;
  // g_reSyncAct=1;
}

// reset controller for bone
void boneReset(void const *boneResetter) {
  while (1) {
    if (readyCount == 60) {
      readyCount = 0;
      if (boneReady != 0) {
        resetBoneLed = !resetBoneLed;
        resetBone = 1;
        // Thread::wait(3000);
        resetBone = 0;
        resetBoneLed = !resetBoneLed;
      }
    }
    readyCount++;
    // Thread::wait(1000);
  }
}

void readyFunction(void const *argument) {
  while (1) {
    /// if((!boneReady)&&(!timeoutState))
    {
      readyLed = 1;
      (*readyThread).terminate();
      delete readyThread;
    }
    // else if(boneReady)
    readyLed = 0;
    // else
    readyLed = !readyLed;

    // Thread::wait(1000);
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

// adc IRQ handler
void readADC(void) {
  adc_cs = 0;
  adc_spi.write(0xF0);
  for (int i = 0; i < 3 * nOfChan; i++)
    bptr[36 + 3 * nOfChan * index + i] = adc_spi.write(0x00);
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
  }
}

void managerFunc(void const *argument) {
  while (1) {
    memset(comeData, 0, sizeof(comeData));
    // int
    // come_len=manage_socket.receiveFrom(manage_client,comeData,sizeof(comeData));

    // if(come_len>0)
    {
      configLed = !configLed;

      if (comeData[0] == 0x01) {
        write_PgaValues_ToFile(comeData + 1, nOfChan); //**********yeni
        createResetFile();
        // mbed_reset();
        /*
        setLowFreq();
        setPgaValues(comeData+1);
        readPgaValues();
        memcpy(buffer0+16,gain_value,nOfChan);
        memcpy(buffer1+16,gain_value,nOfChan);
        setHighFreq();
        do_synchronize();
        */
      }
    }
  }
}
// write pga data
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
