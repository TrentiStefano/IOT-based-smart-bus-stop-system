#define DEBUG 1 //DEBUG_NONE 0

#include "contiki-conf.h"
#include "rpl/rpl-private.h"
#include "mqtt.h"
#include "net/rpl/rpl.h"
#include "net/ip/uip.h"
#include "net/ipv6/uip-icmp6.h"
#include "net/ipv6/sicslowpan.h"

#if USE_TUNSLIP6
#include "net/netstack.h"
#include "dev/slip.h"
#include "net/ip/uip-debug.h"
#endif

#include "sys/etimer.h"
#include "sys/ctimer.h"
#include "lib/sensors.h"
#include "dev/leds.h"
#include "dev/button-sensor.h"

#include "dev/adc-zoul.h"
#include "dev/zoul-sensors.h"
#include "dev/dht22.h"
#include "dev/i2c.h"
#include "dev/tsl256x.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/*---------------------------------------------------------------------------*/
/*
 * MQTT broker address
 */
static const char *broker_ip = MQTT_DEMO_BROKER_IP_ADDR;
/*---------------------------------------------------------------------------*/
/*
 * A timeout used when waiting for something to happen (e.g. to connect or to
 * disconnect)
 */
#define STATE_MACHINE_PERIODIC     (CLOCK_SECOND >> 1)
/*---------------------------------------------------------------------------*/
/* Provide visible feedback via LEDS during various states */
/* When connecting to broker */
#define CONNECTING_LED_DURATION    (CLOCK_SECOND >> 2)

/* Each time we try to publish */
#define PUBLISH_LED_ON_DURATION    (CLOCK_SECOND)
/*---------------------------------------------------------------------------*/
/* Connections and reconnections */
#define RETRY_FOREVER              0xFF
#define RECONNECT_INTERVAL         (CLOCK_SECOND * 2)

/*
 * Number of times to try reconnecting to the broker.
 * Can be a limited number (e.g. 3, 10 etc) or can be set to RETRY_FOREVER
 */
#define RECONNECT_ATTEMPTS         RETRY_FOREVER
#define CONNECTION_STABLE_TIME     (CLOCK_SECOND * 5)
/*---------------------------------------------------------------------------*/
static struct timer connection_life;
static uint8_t connect_attempt;
/*---------------------------------------------------------------------------*/
/* Various states */
static uint8_t state;


#define STATE_INIT                    0
#define STATE_REGISTERED              1
#define STATE_CONNECTING              2
#define STATE_CONNECTED               3
#define STATE_PUBLISHING              4
#define STATE_DISCONNECTED            5
#define STATE_NEWCONFIG               6
#define STATE_CONFIG_ERROR         0xFE
#define STATE_ERROR                0xFF
/*---------------------------------------------------------------------------*/
#define CONFIG_EVENT_TYPE_ID_LEN     32
#define CONFIG_CMD_TYPE_LEN          24
#define CONFIG_IP_ADDR_STR_LEN       32
/*---------------------------------------------------------------------------*/
/* A timeout used when waiting to connect to a network */
#define NET_CONNECT_PERIODIC        (CLOCK_SECOND >> 2)
#define NO_NET_LED_DURATION         (NET_CONNECT_PERIODIC >> 1)
/*---------------------------------------------------------------------------*/
PROCESS_NAME(mqtt_publisher_process);
AUTOSTART_PROCESSES(&mqtt_publisher_process);
/*---------------------------------------------------------------------------*/
/**
 * \brief Data structure declaration for the MQTT client configuration
 */
typedef struct mqtt_client_config {
  char event_type_id[CONFIG_EVENT_TYPE_ID_LEN];
  char broker_ip[CONFIG_IP_ADDR_STR_LEN];
  char cmd_type[CONFIG_CMD_TYPE_LEN];
  clock_time_t pub_interval;
  uint16_t broker_port;
} mqtt_client_config_t;
/*---------------------------------------------------------------------------*/
/*
 * Buffers for Client ID and Topic.
 * Make sure they are large enough to hold the entire respective string
 */
static char client_id[BUFFER_SIZE];
static char pub_topic[BUFFER_SIZE];
static char sub_topic[BUFFER_SIZE];
/*---------------------------------------------------------------------------*/
/*
 * The main MQTT buffers.
 * We will need to increase if we start publishing more data.
 */
static struct mqtt_connection conn;
static char app_buffer[APP_BUFFER_SIZE];
/*---------------------------------------------------------------------------*/
static struct mqtt_message *msg_ptr = 0;
static struct etimer publish_periodic_timer;
static struct ctimer ct;
static char *buf_ptr;
static uint16_t seq_nr_value = 0;
/*---------------------------------------------------------------------------*/
static mqtt_client_config_t conf;

#if USE_TUNSLIP6
static uip_ipaddr_t prefix;
static uint8_t prefix_set;
#endif
/*---------------------------------------------------------------------------*/


static int32_t inital_pol_value;
uint16_t  stop_rqst;

/*---------------------------------------------------------------------------*/
PROCESS(mqtt_publisher_process, "DEEC MQTT Publisher Demo");
/*---------------------------------------------------------------------------*/
int
ipaddr_sprintf(char *buf, uint8_t buf_len, const uip_ipaddr_t *addr)
{
  uint16_t a;
  uint8_t len = 0;
  int i, f;
  for(i = 0, f = 0; i < sizeof(uip_ipaddr_t); i += 2) {
    a = (addr->u8[i] << 8) + addr->u8[i + 1];
    if(a == 0 && f >= 0) {
      if(f++ == 0) {
        len += snprintf(&buf[len], buf_len - len, "::");
      }
    } else {
      if(f > 0) {
        f = -1;
      } else if(i > 0) {
        len += snprintf(&buf[len], buf_len - len, ":");
      }
      len += snprintf(&buf[len], buf_len - len, "%x", a);
    }
  }

  return len;
}
/*---------------------------------------------------------------------------*/
static void
publish_led_off(void *d)
{
  leds_off(LEDS_GREEN);
}
/*---------------------------------------------------------------------------*/
static void
pub_handler(const char *topic, uint16_t topic_len, const uint8_t *chunk,
            uint16_t chunk_len)
{
    int eta_34 = -1;
    int eta_24T = -1;

    // Print the received message
    //printf("Pub Handler: topic='%s' (len=%u), chunk_len=%u\n", topic, topic_len, chunk_len);

    // Convert chunk to a null-terminated string for easier parsing
    char json[chunk_len + 1];
    strncpy(json, (const char *)chunk, chunk_len);
    json[chunk_len] = '\0';
    
    // Manually parse the JSON formatted message
    char *bus_34_ptr = strstr(json, "\"bus_34\":\"");
    char *bus_24T_ptr = strstr(json, "\"bus_24T\":\"");

    if (bus_34_ptr) {
        bus_34_ptr += strlen("\"bus_34\":\"");
        eta_34 = atoi(bus_34_ptr);
    }
    if (bus_24T_ptr) {
        bus_24T_ptr += strlen("\"bus_24T\":\"");
        eta_24T = atoi(bus_24T_ptr);
    }

    // Check if either eta_34 or eta_24T is less than 1
    if (eta_34 < 1 || eta_24T < 1) {
        leds_off(LEDS_RED);
        stop_rqst = 0;
    }

    // Output the extracted values for debugging
    printf("\nETA: 34 = %d, 24T = %d \nStop request = %d\n", eta_34, eta_24T, stop_rqst);

    return;

}
/*---------------------------------------------------------------------------*/
static void
mqtt_event(struct mqtt_connection *m, mqtt_event_t event, void *data)
{
  switch(event) {
  case MQTT_EVENT_CONNECTED: {
    printf("APP - Application has a MQTT connection\n");
    timer_set(&connection_life, CONNECTION_STABLE_TIME);
    state = STATE_CONNECTED;
    break;
  }
  case MQTT_EVENT_DISCONNECTED: {
    printf("APP - MQTT Disconnect. Reason %u\n", *((mqtt_event_t *)data));

    state = STATE_DISCONNECTED;
    process_poll(&mqtt_publisher_process);
    break;
  }
  case MQTT_EVENT_PUBLISH: {
    msg_ptr = data;

    /* Implement first_flag in publish message? */
    if(msg_ptr->first_chunk) {
      msg_ptr->first_chunk = 0;
      /*
      printf("APP - Application received a publish on topic '%s'. Payload "
          "size is %i bytes. Content:\n\n",
          msg_ptr->topic, msg_ptr->payload_length);*/
    }

    pub_handler(msg_ptr->topic, strlen(msg_ptr->topic), msg_ptr->payload_chunk,
                msg_ptr->payload_length);
    break;
  }
  case MQTT_EVENT_SUBACK: {
    printf("APP - Application is subscribed to topic successfully\n");
    break;
  }
  case MQTT_EVENT_UNSUBACK: {
    printf("APP - Application is unsubscribed to topic successfully\n");
    break;
  }
  case MQTT_EVENT_PUBACK: {
    printf("APP - Publishing complete.\n");
    break;
  }
  default:
    printf("APP - Application got a unhandled MQTT event: %i\n", event);
    break;
  }
}
/*---------------------------------------------------------------------------*/
static int
construct_pub_topic(void)
{
  int len = snprintf(pub_topic, BUFFER_SIZE, "%s",
                     conf.event_type_id);
  if(len < 0 || len >= BUFFER_SIZE) {
    printf("Pub Topic too large: %d, Buffer %d\n", len, BUFFER_SIZE);
    return 0;
  }

  return 1;
}
/*---------------------------------------------------------------------------*/
static int
construct_sub_topic(void)
{
  int len = snprintf(sub_topic, BUFFER_SIZE, "%s",
                     conf.cmd_type);
  if(len < 0 || len >= BUFFER_SIZE) {
    printf("Sub Topic too large: %d, Buffer %d\n", len, BUFFER_SIZE);
    return 0;
  }

  printf("Subscription topic %s\n", sub_topic);

  return 1;
}
/*---------------------------------------------------------------------------*/
static int
construct_client_id(void)
{
  int len = snprintf(client_id, BUFFER_SIZE, "d:%02x%02x%02x%02x%02x%02x",
                     linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1],
                     linkaddr_node_addr.u8[2], linkaddr_node_addr.u8[5],
                     linkaddr_node_addr.u8[6], linkaddr_node_addr.u8[7]);

  /* len < 0: Error. Len >= BUFFER_SIZE: Buffer too small */
  if(len < 0 || len >= BUFFER_SIZE) {
    printf("Client ID: %d, Buffer %d\n", len, BUFFER_SIZE);
    return 0;
  }

  return 1;
}
/*---------------------------------------------------------------------------*/
static void
update_config(void)
{
  if(construct_client_id() == 0) {
    /* Fatal error. Client ID larger than the buffer */
    state = STATE_CONFIG_ERROR;
    return;
  }

  if(construct_sub_topic() == 0) {
    /* Fatal error. Topic larger than the buffer */
    state = STATE_CONFIG_ERROR;
    return;
  }

  if(construct_pub_topic() == 0) {
    /* Fatal error. Topic larger than the buffer */
    state = STATE_CONFIG_ERROR;
    return;
  }

  /* Reset the counter */
  seq_nr_value = 0;

  state = STATE_INIT;

  /*
   * Schedule next timer event ASAP
   * If we entered an error state then we won't do anything when it fires.
   * Since the error at this stage is a config error, we will only exit this
   * error state if we get a new config.
   */
  etimer_set(&publish_periodic_timer, 0);

  return;
}
/*---------------------------------------------------------------------------*/
static int
init_config()
{
  /* Populate configuration with default values */
  memset(&conf, 0, sizeof(mqtt_client_config_t));
  memcpy(conf.event_type_id, DEFAULT_STATUS_EVENT_TOPIC, strlen(DEFAULT_STATUS_EVENT_TOPIC));
  memcpy(conf.broker_ip, broker_ip, strlen(broker_ip));
  memcpy(conf.cmd_type, SUBSCRIBED_TOPIC, strlen(SUBSCRIBED_TOPIC));

  conf.broker_port = DEFAULT_BROKER_PORT;
  conf.pub_interval = DEFAULT_PUBLISH_INTERVAL;

  return 1;
}
/*---------------------------------------------------------------------------*/
static void
subscribe(void)
{
  /* Publish MQTT topic in IBM quickstart format */
  mqtt_status_t status;

  status = mqtt_subscribe(&conn, NULL, sub_topic, MQTT_QOS_LEVEL_0);

  printf("APP - Subscribing to %s\n", sub_topic);
  if(status == MQTT_STATUS_OUT_QUEUE_FULL) {
    printf("APP - Tried to subscribe but command queue was full!\n");
  }
}
/*---------------------------------------------------------------------------*/
static void
publish(void)
{
  int len;
  uint16_t pollution_level, light;
  int16_t temperature, humidity;
  int32_t pollution;
  int remaining = APP_BUFFER_SIZE;

  seq_nr_value++;
  buf_ptr = app_buffer;

  len = snprintf(buf_ptr, remaining,
                 "{"
                 "\"stop_id\":\"stop_1\","
                 "\"ts\":%lu", clock_seconds());

  if(len < 0 || len >= remaining) {
    printf("Buffer too short. Have %d, need %d + \\0\n", remaining, len);
    return;
  }

  remaining -= len;
  buf_ptr += len;

  /* Put our Default route's string representation in a buffer */
  char def_rt_str[64];
  memset(def_rt_str, 0, sizeof(def_rt_str));
  ipaddr_sprintf(def_rt_str, sizeof(def_rt_str), uip_ds6_defrt_choose());

  //len = snprintf(buf_ptr, remaining, ",\"Def Route\":\"%s\"", def_rt_str);

  if(len < 0 || len >= remaining) {
    printf("Buffer too short. Have %d, need %d + \\0\n", remaining, len);
    return;
  }

  //remaining -= len;
  //buf_ptr += len;
  dht22_read_all(&temperature, &humidity);

  pollution = adc_zoul.value(ZOUL_SENSORS_ADC3);
  //printf("pollution %ld mV\n", pollution);
  //printf("inital_pol_value %ld mV\n", inital_pol_value);
  pollution = pollution - inital_pol_value;
  pollution = pollution - 1000;
  printf("Pollution level: ");
  
  if(pollution > 2000){
    pollution_level = 3;
    printf("DANGEROUS pollution levels!\n");
  }
  else if(pollution > 600){
    pollution_level = 2;
    printf("High pollution levels.\n");
  }
  else if(pollution > 100){
    pollution_level = 1;
    printf("Low pollution levels.\n");
  }
  else{
    pollution_level = 0;
    printf("Fresh air.\n"); // %ld mV\n", pollution);
  }

  light = tsl256x.value(TSL256X_VAL_READ);

  printf("Light value: %u L\n",light);
  printf("Temperature:%02d.%02d °C, Humidity:%02d.%02d %%\n", temperature / 10, temperature % 10, humidity / 10, humidity % 10);
  
  len = snprintf(buf_ptr, remaining, ",\"Stop request\":\"%u\",\"Temperature\":\"%02d.%02d\",\"Humidity\":\"%02d.%02d\",\"Pollution\":\"%u\",\"Light\":\"%u\"", stop_rqst, temperature / 10, temperature % 10, humidity / 10, humidity % 10, pollution_level, light);

  remaining -= len;
  buf_ptr += len;

  len = snprintf(buf_ptr, remaining, "}");

  if(len < 0 || len >= remaining) {
    printf("Buffer too short. Have %d, need %d + \\0\n", remaining, len);
    return;
  }

  mqtt_publish(&conn, NULL, pub_topic, (uint8_t *)app_buffer,
               strlen(app_buffer), MQTT_QOS_LEVEL_0, MQTT_RETAIN_OFF);

  //printf("APP - Publish to %s: %s\n", pub_topic, app_buffer);
}
/*---------------------------------------------------------------------------*/
static void
connect_to_broker(void)
{
  /* Connect to MQTT server */
  mqtt_connect(&conn, conf.broker_ip, conf.broker_port,
               conf.pub_interval * 3);

  state = STATE_CONNECTING;
}
/*---------------------------------------------------------------------------*/
static void
state_machine(void)
{
  switch(state) {
  case STATE_INIT:
    /* If we have just been configured register MQTT connection */
    mqtt_register(&conn, &mqtt_publisher_process, client_id, mqtt_event,
                  MAX_TCP_SEGMENT_SIZE);

    conn.auto_reconnect = 0;
    connect_attempt = 1;

    state = STATE_REGISTERED;
    printf("Init\n");

    /* Notice there is no "break" here, it will continue to the
     * STATE_REGISTERED
     */
  case STATE_REGISTERED:
    if(uip_ds6_get_global(ADDR_PREFERRED) != NULL) {
      /* Registered and with a public IP. Connect */
      printf("Registered. Connect attempt %u\n", connect_attempt);
      connect_to_broker();

    } else {
      leds_on(LEDS_GREEN);
      ctimer_set(&ct, NO_NET_LED_DURATION, publish_led_off, NULL);
    }
    etimer_set(&publish_periodic_timer, NET_CONNECT_PERIODIC);
    return;
    break;

  case STATE_CONNECTING:
    leds_on(LEDS_GREEN);
    ctimer_set(&ct, CONNECTING_LED_DURATION, publish_led_off, NULL);
    /* Not connected yet. Wait */
    printf("Connecting (%u)\n", connect_attempt);
    break;

  case STATE_CONNECTED:
    /* Notice there's no "break" here, it will continue to subscribe */

  case STATE_PUBLISHING:
    /* If the timer expired, the connection is stable. */
    if(timer_expired(&connection_life)) {
      /*
       * Intentionally using 0 here instead of 1: We want RECONNECT_ATTEMPTS
       * attempts if we disconnect after a successful connect
       */
      connect_attempt = 0;
    }

    if(mqtt_ready(&conn) && conn.out_buffer_sent) {
      /* Connected. Publish */
      if(state == STATE_CONNECTED) {
        subscribe();
        state = STATE_PUBLISHING;

      } else {
        leds_on(LEDS_GREEN);
        printf("\nPublishing status.\n\n");
        ctimer_set(&ct, PUBLISH_LED_ON_DURATION, publish_led_off, NULL);
        publish();
      }
      etimer_set(&publish_periodic_timer, conf.pub_interval);

      /* Return here so we don't end up rescheduling the timer */
      return;

    } else {
      /*
       * Our publish timer fired, but some MQTT packet is already in flight
       * (either not sent at all, or sent but not fully ACKd).
       *
       * This can mean that we have lost connectivity to our broker or that
       * simply there is some network delay. In both cases, we refuse to
       * trigger a new message and we wait for TCP to either ACK the entire
       * packet after retries, or to timeout and notify us.
       */
      printf("Publishing... (MQTT state=%d, q=%u)\n", conn.state,
          conn.out_queue_full);
    }
    break;

  case STATE_DISCONNECTED:
    printf("Disconnected\n");
    if(connect_attempt < RECONNECT_ATTEMPTS ||
       RECONNECT_ATTEMPTS == RETRY_FOREVER) {
      /* Disconnect and backoff */
      clock_time_t interval;
      mqtt_disconnect(&conn);
      connect_attempt++;

      interval = connect_attempt < 3 ? RECONNECT_INTERVAL << connect_attempt :
        RECONNECT_INTERVAL << 3;

      printf("Disconnected. Attempt %u in %lu ticks\n", connect_attempt, interval);

      etimer_set(&publish_periodic_timer, interval);

      state = STATE_REGISTERED;
      return;

    } else {
      /* Max reconnect attempts reached. Enter error state */
      state = STATE_ERROR;
      printf("Aborting connection after %u attempts\n", connect_attempt - 1);
    }
    break;

  case STATE_CONFIG_ERROR:
    /* Idle away. The only way out is a new config */
    printf("Bad configuration.\n");
    return;

  case STATE_ERROR:
  default:
    leds_on(LEDS_GREEN);
    printf("Default case: State=0x%02x\n", state);
    return;
  }

  /* If we didn't return so far, reschedule ourselves */
  etimer_set(&publish_periodic_timer, STATE_MACHINE_PERIODIC);
}

#if USE_TUNSLIP6
/*---------------------------------------------------------------------------*/
static void print_local_addresses(void)
{
  int i;
  uint8_t state;

  printf("Server IPv6 addresses:\n");
  for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
    state = uip_ds6_if.addr_list[i].state;
    if(uip_ds6_if.addr_list[i].isused &&
       (state == ADDR_TENTATIVE || state == ADDR_PREFERRED)) {
      printf(" ");
      uip_debug_ipaddr_print(&uip_ds6_if.addr_list[i].ipaddr);
      printf("\n");
    }
  }
}
/*---------------------------------------------------------------------------*/
void request_prefix(void)
{
  /* mess up uip_buf with a dirty request... */
  uip_buf[0] = '?';
  uip_buf[1] = 'P';
  uip_len = 2;
  slip_send();
  uip_clear_buf();
}
/*---------------------------------------------------------------------------*/
void set_prefix_64(uip_ipaddr_t *prefix_64)
{
  rpl_dag_t *dag;
  uip_ipaddr_t ipaddr;
  memcpy(&prefix, prefix_64, 16);
  memcpy(&ipaddr, prefix_64, 16);
  prefix_set = 1;
  uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
  uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);

  dag = rpl_set_root(RPL_DEFAULT_INSTANCE, &ipaddr);
  if(dag != NULL) {
    rpl_set_prefix(dag, &prefix, 64);
    PRINTF("created a new RPL dag\n");
  }
}
#endif

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(mqtt_publisher_process, ev, data)
{
#if USE_TUNSLIP6
  static struct etimer et;
#endif

  PROCESS_BEGIN();
  /*-------------------SENSORS---------------------*/
  SENSORS_ACTIVATE(dht22);
  SENSORS_ACTIVATE(tsl256x);
  SENSORS_ACTIVATE(button_sensor);
  adc_zoul.configure(SENSORS_HW_INIT, ZOUL_SENSORS_ADC3);
  tsl256x.configure(TSL256X_INT_OVER, 0x15B8);
  etimer_set(&et, CLOCK_SECOND);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
  inital_pol_value = adc_zoul.value(ZOUL_SENSORS_ADC3);

  stop_rqst = 0;
  /*--------------------------------------------------------*/

#if USE_TUNSLIP6
/* While waiting for the prefix to be sent through the SLIP connection, the future
 * border router can join an existing DAG as a parent or child, or acquire a default
 * router that will later take precedence over the SLIP fallback interface.
 * Prevent that by turning the radio off until we are initialized as a DAG root.
 */
  prefix_set = 0;
  NETSTACK_MAC.off(0);
#endif

  printf("DEEC MQTT Publisher Demo Process\n");

#if USE_TUNSLIP6


#if WEBSERVER
  httpd_init();
#endif

  /* Request prefix until it has been received */
  while(!prefix_set) {
    etimer_set(&et, CLOCK_SECOND);
    request_prefix();
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
  }

  /* Now turn the radio on, but disable radio duty cycling.
   * Since we are the DAG root, reception delays would constrain mesh throughput.
   */
  NETSTACK_MAC.off(1);

  print_local_addresses();
#endif


  if(init_config() != 1) {
    PROCESS_EXIT();
  }

  //adc_zoul.configure(SENSORS_HW_INIT, ZOUL_SENSORS_ADC_ALL);

  update_config();

  while(1) {
    PROCESS_YIELD();


  if(ev == sensors_event && data == &button_sensor){
    leds_on(LEDS_RED);
    printf("Button pressed!");
    stop_rqst = 1;
  }
  

  if((ev == PROCESS_EVENT_TIMER && data == &publish_periodic_timer) ||
     ev == PROCESS_EVENT_POLL) {
    state_machine();
  }
}

  PROCESS_END();

}
/*---------------------------------------------------------------------------*/
