#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <time.h>
#include <TZ.h>
#include <FS.h>
#include <LittleFS.h>
#include <CertStoreBearSSL.h>
#include <DHT.h>

#define DHTPIN 0
#define DHTTYPE DHT11

#define humidity_topic "thaile/sensor/humidity"
#define temperature_celsius_topic "thaile/sensor/temperature_celsius"
#define sendDataTopic "thaile/esp/sendData"
#define readDelayTimeTopic "thaile/esp/readDelay"
#define ledTopic "thaile/esp/led"

// Update these with values suitable for your network.
const char *ssid = "Tuan-712";
const char *password = "letuan1973";
const char *host = "broker.hivemq.com";
const char *serverThingSpeak = "api.thingspeak.com";
String writeAPIKey = "V6DMB7QBC9R8YPGF";

long lastMsg = 0;
char msg[50];
int value = 0;
bool isSave = true;

DHT dht(DHTPIN, DHTTYPE);

long timedelay = 10000;
int isSendDatatoServer = true;

WiFiClient serverTP;
WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi()
{
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void setDateTime()
{
  // You can use your own timezone, but the exact time is not used at all.
  // Only the date is needed for validating the certificates.
  configTime(TZ_Europe_Berlin, "pool.ntp.org", "time.nist.gov");

  Serial.print("Waiting for NTP time sync: ");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2)
  {
    delay(100);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println();

  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  // Serial.printf("%s %s", tzname[0], asctime(&timeinfo));
}

void sendToServer(float temp, float humi)
{
  if (serverTP.connect(serverThingSpeak, 80))
  {
    // Construct API request body
    String body = "field1=" + String(temp, 1) + "&field2=" + String(humi, 1);

    serverTP.print("POST /update HTTP/1.1\n");
    serverTP.print("Host: api.thingspeak.com\n");
    serverTP.print("Connection: close\n");
    serverTP.print("X-THINGSPEAKAPIKEY: " + writeAPIKey + "\n");
    serverTP.print("Content-Type: application/x-www-form-urlencoded\n");
    serverTP.print("Content-Length: ");
    serverTP.print(body.length());
    serverTP.print("\n\n");
    serverTP.print(body);
    serverTP.print("\n\n");
    Serial.printf("Nhiet do %s - Do am %s\r\n", String(temp, 1).c_str(), String(humi, 1).c_str());
  }
  serverTP.stop();
}

void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  String msgTemp;
  for (int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
    msgTemp += (char)payload[i];
  }
  Serial.println();

  if (String(topic) == ledTopic)
  {
    Serial.print("Changing led to ");
    if (msgTemp == "on")

    {
      Serial.println("on");
      digitalWrite(LED_BUILTIN, LOW);
    }
    else if (msgTemp == "off")
    {
      Serial.println("off");
      digitalWrite(LED_BUILTIN, HIGH);
    }
  }

  if (String(topic) == readDelayTimeTopic)
  {
    Serial.print("Changing time delay per read");
    timedelay = atoi(msgTemp.c_str());
    Serial.printf("New time delay: %d", timedelay);
  }

  if (String(topic) == sendDataTopic)
  {
    if (msgTemp == "on")
    {
      isSave = true;
      Serial.println("Send data to server from Now!");
    }
    else
    {
      isSave = false;
      Serial.println("Not send data to server from Now!");
    }
  }
}

String macToStr(const uint8_t *mac)
{
  String result;
  for (int i = 0; i < 6; ++i)
  {
    result += String(mac[i], 16);
    if (i < 5)
      result += ':';
  }
  return result;
}

void reconnect()
{
  // Loop until we're reconnected
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");

    // Generate client name based on MAC address and last 8 bits of microsecond counter
    String clientName;
    clientName += "esp8266-";
    uint8_t mac[6];
    WiFi.macAddress(mac);
    clientName += macToStr(mac);
    clientName += "-";
    clientName += String(micros() & 0xff, 16);
    Serial.print("Connecting to ");
    Serial.print(host);
    Serial.print(" as ");
    Serial.println(clientName);

    // Attempt to connect

    if (client.connect((char *)clientName.c_str()))
    {

      Serial.println("connected");
      client.subscribe(ledTopic);
      client.subscribe(readDelayTimeTopic);
      client.subscribe(sendDataTopic);
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup()
{
  delay(500);
  // When opening the Serial Monitor, select 9600 Baud
  Serial.begin(9600);
  delay(500);
  pinMode(LED_BUILTIN, OUTPUT);
  setup_wifi();
  client.setServer(host, 1883);
  client.setCallback(callback);
}

void loop()
{
  if (!client.connected())
  {
    reconnect();
  }

  client.loop();

  long now = millis();
  if (now - lastMsg > timedelay)
  {
    lastMsg = now;
    delay(2000);
    float h = dht.readHumidity();
    // Read temperature as Celsius (the default)
    float t = dht.readTemperature();

    if (isnan(h) || isnan(t))
    {
      Serial.println("Failed to read from DHT sensor!");
      return;
    }

    Serial.print("Temperature in Celsius:");
    Serial.println(String(t).c_str());
    client.publish(temperature_celsius_topic, String(t).c_str(), true);

    Serial.print("Humidity:");
    Serial.println(String(h).c_str());
    client.publish(humidity_topic, String(h).c_str(), true);

    if (isSave)
    {
      sendToServer(t, h);
    }
  }
}