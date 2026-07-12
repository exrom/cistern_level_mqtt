
#include <stdio.h>

#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <uptime.h>


/************************* globals ************************************/
byte my_mac[]    = {  0xDE, 0xED, 0xBA, 0xFE, 0xFE, 0xED };
IPAddress my_ip(192, 168, 6, 44);
char fw_version_string[64];

/************************* MQTT setup ************************************/
#define MQTT_SERVER      "homeassistant.fritz.box"
IPAddress mqtt_server_IP(192, 168, 6, 13);
#define MQTT_SERVERPORT  1883
#define MQTT_USERNAME    "mqtt"
#define MQTT_PASSWORD    "*******"

/************************* Home Assistant setup ************************************/
/*
/homeassistant/configuration.yaml

mqtt:
  sensor:
    - name: "Zisterne 1 Füllstand"
      unique_id: "zisterne_1_fuellstand"
      icon: "mdi:water-percent"
      device_class: "water"
      state_topic: "cistern/0"
      unit_of_measurement: "%"
*/



/************************* ADC + currrent loop config ******************************/
/* Calculation of filled volume in the cistern from tha analog input read

  level sensor      0...3 meter fill level
  current loop      4..20mA
  analog voltage    0.8V....4.4V  (shunt resistor 220Ohm)
  analog reading    180.. 901    (1024 =^ 5V)
  
  arduino ADC range is 0..1024
 */
#define ADC_VALUE_4MA       180     /* ADC value for 4mA */
#define ADC_VALUE_20MA      901     /* ADC value for 20mA */

/************************* cistern config ******************************/
#define SENSOR_UPPER_LIMIT        3.0f        /* for ALS-MPM-2F 3m */
#define CISTERN_UPPER_LIMIT       2.65f       /* for BZB12500 */
#define CISTERN_CAPACITY          12500       /* for BZB12500 */
#define CISTERN_COUNT             4


EthernetClient  ethClient;
PubSubClient    mqttClient(ethClient);
EthernetServer  http_server(80);

/**
 * Calculate the full scale value of sensor by raw ADC reading
 * 
 * @param[in]     u32ADC    raw analog to digital measure ADC_VALUE_4MA..ADC_VALUE_20MA
 * @param[out]    float     fraction of sensor measure 0.0 ... 1.0
 */
float adc2sensorFS(const uint32_t u32ADC)
{
  float fPercent;

  fPercent = ((float)u32ADC-ADC_VALUE_4MA)/(ADC_VALUE_20MA-ADC_VALUE_4MA);
  
  return fPercent;
}

/**
 * Calculate the percentage fill level from the sensor measure.
 * Example:
 *    sensor range is 0..5m  but the cistern top level is 3.2m
 *    and: the geometry of cistern is considered in this formula
 * 
 * 
 * @param[in]     fSensor     sensor value 0.0 ... 1.0
 * @param[out]    fFillLevel  sensor value 0.0 ... 1.0
 */
float sensorFS2fillLevel(const float fSensor)
{
  float fFillLevel;

  fFillLevel = fSensor * ( SENSOR_UPPER_LIMIT / CISTERN_UPPER_LIMIT);
  
  return fFillLevel;
}


uint32_t fillLevel2liters(const float fFillLevel)
{
  return (uint32_t) (fFillLevel * CISTERN_CAPACITY);
}


void mqttConnect() {
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttClient.connect("arduinoClient", MQTT_USERNAME, MQTT_PASSWORD)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup()
{
  Serial.begin(9600);
  Ethernet.begin(my_mac, my_ip);

  http_server.begin();

  mqttClient.setServer(mqtt_server_IP, MQTT_SERVERPORT);

  snprintf(fw_version_string, sizeof(fw_version_string), "%s %s", __DATE__, __TIME__);

  // Allow the hardware to sort itself out
  delay(1500);
}

void loop()
{
  char mqtt_topic[32];
  char mqtt_data[64];
  char uptime_string[64];

  uint32_t    u32ADC[CISTERN_COUNT];
  float       fFillLevel[CISTERN_COUNT];
  uint32_t    u32Liters[CISTERN_COUNT];


  EthernetClient http_client = http_server.available();


  /************ get data and do calculations ************/
  uptime::calculateUptime();
  snprintf(uptime_string, sizeof(uptime_string), "%ldT%02ld:%02ld:%02ld", uptime::getDays(), uptime::getHours(), uptime::getMinutes(), uptime::getSeconds());
  for( uint8_t u8Cistern=0; u8Cistern<CISTERN_COUNT; u8Cistern++)
  {
      u32ADC[u8Cistern] = analogRead(u8Cistern);    /* cistern 0..3 go to A0..A3 */

      /* for debugging */
      u32ADC[0] = 499;   /* 50% */
      u32ADC[1] = 817;   /* 100% */
      u32ADC[2] = 180;   /* 0%  */
      
      fFillLevel[u8Cistern] = sensorFS2fillLevel(adc2sensorFS(u32ADC[u8Cistern]));
      u32Liters[u8Cistern]  = fillLevel2liters(fFillLevel[u8Cistern]);
  }

  /************ MQTT handling ************/
  mqttConnect();    /* re-connect if closed */
  mqttClient.publish("cistern/uptime", uptime_string);
  mqttClient.publish("cistern/fw-version", fw_version_string);
  for( uint8_t u8Cistern=0; u8Cistern<CISTERN_COUNT; u8Cistern++)
  {
      snprintf(mqtt_topic, sizeof(mqtt_topic), "cistern/chn%d/adc_raw", u8Cistern);
      snprintf(mqtt_data, sizeof(mqtt_data), "%ld", u32ADC[u8Cistern]);
      mqttClient.publish(mqtt_topic, mqtt_data);
      snprintf(mqtt_topic, sizeof(mqtt_topic), "cistern/chn%d/volume_percent", u8Cistern);
      dtostrf(fFillLevel[u8Cistern]*100, 4, 2, mqtt_data);
      mqttClient.publish(mqtt_topic, mqtt_data);
      snprintf(mqtt_topic, sizeof(mqtt_topic), "cistern/chn%d/liters", u8Cistern);
      snprintf(mqtt_data, sizeof(mqtt_data), "%ld", u32Liters[u8Cistern]);
      mqttClient.publish(mqtt_topic, mqtt_data);
  }


  /************ serial monitor handling ************/
  for( uint8_t u8Cistern=0; u8Cistern<CISTERN_COUNT; u8Cistern++)
  {
      String sPercent = String(fFillLevel[u8Cistern]);
      Serial.print(u8Cistern);
      Serial.print(": ");
      Serial.print(sPercent);
      Serial.print("    ");
  }
  Serial.print(uptime_string);
  Serial.println();

  /************ http handling ************/
  if (http_client) {
    //Serial.println("new http client");
    boolean currentLineIsBlank = true;    // an http request ends with a blank line
    while (http_client.connected()) {
      if (http_client.available()) {
        char c = http_client.read();
        if (c == '\n' && currentLineIsBlank) {
          // send a standard http response header
          http_client.println("HTTP/1.1 200 OK");
          http_client.println("Content-Type: text/html");
          http_client.println("Connection: close");
          http_client.println();
          http_client.println("<!DOCTYPE HTML>");
          http_client.println("<html>");

          http_client.println("<meta http-equiv=\"refresh\" content=\"10\">");
          http_client.print("<h1>Arduino cistern level measurement</h1><br>");
          http_client.print("<style>body {font-family: 'Courier New', monospace;}</style>");

          for( uint8_t u8Cistern=0; u8Cistern<CISTERN_COUNT; u8Cistern++)
          {
              String sPercent = String(fFillLevel[u8Cistern]);
              http_client.print("cistern:");
              http_client.print(u8Cistern);
              http_client.print(",     volume_percent: ");
              http_client.print(fFillLevel[u8Cistern]*100);
              http_client.print(",     liters: ");
              http_client.print(u32Liters[u8Cistern]);
              http_client.print(",     adc_raw: ");
              http_client.print(u32ADC[u8Cistern]);
              http_client.print("<br>");
          }
         
          http_client.print("<br><br>version: ");
          http_client.println(fw_version_string);
          http_client.print("<br>uptime: ");
          http_client.println(uptime_string);
          http_client.println("</html>");
          break;
        }
       
        if (c == '\n') {
          currentLineIsBlank = true;
        } else if (c != '\r') {
          currentLineIsBlank = false;
        }
      }
    }
   
    delay(5);   // allow browwser to receive data

    http_client.stop();
    //Serial.println("http connection closed.");
  }


  delay(1000);    // main lop delay
}

