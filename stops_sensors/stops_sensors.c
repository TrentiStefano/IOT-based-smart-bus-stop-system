#include <stdio.h>
#include "contiki.h"
#include "dev/dht22.h"
#include "dev/adc-zoul.h"
#include "dev/zoul-sensors.h"
#include "dev/leds.h"
#include "lib/sensors.h"
#include "dev/i2c.h"
#include "dev/leds.h"
#include "dev/tsl256x.h"
/*---------------------------------------------------------------------------*/
PROCESS(remote_dht22_process, "DHT22 test");
AUTOSTART_PROCESSES(&remote_dht22_process);
/*---------------------------------------------------------------------------*/
static struct etimer et;
static int32_t inital_pol_value;
static uint16_t light;
/*---------------------------------------------------------------------------*/
void light_interrupt_callback(uint8_t value)
{
  printf("* Light sensor interrupt!\n");
  //leds_toggle(LEDS_PURPLE);
}

/*---------------------------------------------------------------------------*/

PROCESS_THREAD(remote_dht22_process, ev, data)
{
  int16_t temperature, humidity;
  int32_t pollution;

  PROCESS_BEGIN();
  SENSORS_ACTIVATE(dht22);
  SENSORS_ACTIVATE(tsl256x);
  adc_zoul.configure(SENSORS_HW_INIT, ZOUL_SENSORS_ADC3);
  inital_pol_value = adc_zoul.value(ZOUL_SENSORS_ADC3);
  
  //TSL256X_REGISTER_INT(light_interrupt_callback);
  tsl256x.configure(TSL256X_INT_OVER, 0x15B8);

  while(1) {
    printf("start of while\n");

    etimer_set(&et, CLOCK_SECOND);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    printf("Finished timer\n");

    if(dht22_read_all(&temperature, &humidity) != DHT22_ERROR) {
      printf("Temperature %02d.%02d ºC, ", temperature / 10, temperature % 10);
      printf("Humidity %02d.%02d RH\n", humidity / 10, humidity % 10);
    } else {
      printf("Failed to read the sensor\n");
    }

    pollution = adc_zoul.value(ZOUL_SENSORS_ADC3);
    pollution -= inital_pol_value;
    if(pollution > 2000){
      printf("DANGEROUS pollution levels!\n"); // %ld mV\n", pollution);
    }
    else if(pollution > 600){
      printf("High pollution levels.\n"); // %ld mV\n", pollution);
    }
    else if(pollution > 100){
      printf("Low pollution levels.\n"); // %ld mV\n", pollution);
    }
    else{
      printf("Fresh air.\n");
    }
     
    
    light = tsl256x.value(TSL256X_VAL_READ);
    if(light != TSL256X_ERROR) {
      printf("Light = %u\n", (uint16_t)light);
    }
    printf("end of loop\n");
  }
  PROCESS_END();
}
