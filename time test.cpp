#include "includesdefinesvariables.h"
#include "mbed.h"
#include "ntp_gettime.h"
uint32_t remsecs;
uint32_t tm_hour;
uint32_t tm_min;
uint32_t tm_sec;
time_t seconds;
int millis;
int i;
Timer t;
PwmOut pwm(p21);

int main(void) {
  while (true) {
    NTP_gettime();
    pwm.period(1);
    pwm = 0.2;
    while (i < 60) {
      t.start();
      i++;
      seconds = time(NULL);
      remsecs = seconds % 86400;
      tm_hour = remsecs / 3600;
      tm_min = remsecs / 60 % 60;
      tm_sec = remsecs % 60;
      char buffer2[32];
      millis =
          duration_cast<std::chrono::milliseconds>(t.elapsed_time()).count();
      //printf("millis %u \n", millis);
      strftime(buffer2, 32, "%Y-%m-%d %H:%M:%S:", localtime(&seconds));
      printf("%u:%u:%u:%u\n", tm_hour, tm_min, tm_sec, millis);
      t.stop();
      ThisThread::sleep_for(1s);
      t.reset();
    }
    i = 0;
    main();
  }
}