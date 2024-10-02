/*---------------------------------------------------------------------------*/
#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_
/*---------------------------------------------------------------------------*/
/* User configuration */
#define DEFAULT_ORG_ID            "mqtt-demo"

#if USE_TUNSLIP6
#define MQTT_DEMO_BROKER_IP_ADDR  "fd00::1/64" 		// Use local Mosquitto Broker IPv6 running in the local virtual machine
#else
#define MQTT_DEMO_BROKER_IP_ADDR  "::ffff:c0a8:173"	// External MQTT Broker IPV6 Address
#endif

/*---------------------------------------------------------------------------*/
/* Default configuration values */
#define DEFAULT_STATUS_EVENT_TOPIC   "status/stop_1"
#define SUBSCRIBED_TOPIC       "timings/stop_1"
#define DEFAULT_BROKER_PORT          1883
#define DEFAULT_PUBLISH_INTERVAL     (30 * CLOCK_SECOND)
#define DEFAULT_KEEP_ALIVE_TIMER     60

#undef IEEE802154_CONF_PANID
#define IEEE802154_CONF_PANID        0xABCD

/* The following are Zoul (RE-Mote, etc) specific */
#undef CC2538_RF_CONF_CHANNEL
#define CC2538_RF_CONF_CHANNEL       26

/* Specific platform values */
#if CONTIKI_TARGET_ZOUL
#define BUFFER_SIZE                  36
#define APP_BUFFER_SIZE              512
#define BOARD_ID_STRING                 "Bus Stop 1"
#else /* Default is Z1 */
#define BUFFER_SIZE                  36
#define APP_BUFFER_SIZE              240
#define BOARD_ID_STRING                 "Bus Stop 1"
#undef NBR_TABLE_CONF_MAX_NEIGHBORS
#define NBR_TABLE_CONF_MAX_NEIGHBORS 3
#undef UIP_CONF_MAX_ROUTES
#define UIP_CONF_MAX_ROUTES          3
#endif
/*---------------------------------------------------------------------------*/
#undef NETSTACK_CONF_RDC
#define NETSTACK_CONF_RDC          nullrdc_driver

/* Maximum TCP segment size for outgoing segments of our socket */
#define MAX_TCP_SEGMENT_SIZE       32

#endif /* PROJECT_CONF_H_ */
/*---------------------------------------------------------------------------*/
/** @} */

//removeeee

#undef LPM_CONF_ENABLE
#define LPM_CONF_ENABLE 0