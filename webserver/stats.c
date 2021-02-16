#include "stats.h"

static web_stats stat;

int init_stats(void){
  stat.served_connections=0;
  stat.served_requests=0;
  stat.ok_200=0;
  stat.ko_400=0;
  stat.ko_403=0;
  stat.ko_404=0;
  return 0;
}

web_stats *get_stats(void){
  return &stat;
}
