////////////
// Base code for ESP8266 for Sean's Home Automation
// Does WifiManager, MQTT, and OTA Updates 
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// For wifimanager
#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager todo: replace with https://github.com/kentaylor/WiFiManager
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// for pubsub
#include <SPI.h>
#include <PubSubClient.h>
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// for OTA
#include <WiFiClient.h>
//#include <ESP8266WebServer.h> // <-- this is needed for both WifiManager and OTA; included above
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// drd conflicts with WifiManager which resets multiple times
// #include <DoubleResetDetector.h>  //https://github.com/datacute/DoubleResetDetector
//#define DRD_TIMEOUT 5
//#define DRD_ADDRESS 0
//DoubleResetDetector drd(DRD_TIMEOUT, DRD_ADDRESS);
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Functions and variables used in the Wifi 
bool readConfigFile();
bool writeConfigFile();
void loopBaseFunctions();
void setupBaseFunctions();
void reconnect();
void ISRbuttonStateChanged();
void ClearAllSettings();
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// #wifimanager - these are offered on WifiManager 
char mqtt_server[20] = "mqtt.muzik.ca";
int mqtt_port = 1883;
char mqtt_username[20] = "mqttEspUser";
char mqtt_password[20] = "Happy123";
char mqtt_topic[41] = "/home/esp/testdevice";  // allow 40 char max length
bool initialConfig = false;

//flag for saving data
bool shouldSaveConfig = false;
void MQTTcallback(char*, byte*, unsigned int);
WiFiClient espClient;
PubSubClient client(espClient);

// this will hold the hostname, like esp8266-123456 
char host[20] = "esp8266-test"; 
volatile unsigned long buttonEntry, buttonTime;
volatile bool buttonChanged = false;
int failedConnects = 0; 
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 
void setup() {
  // the very FIRST thing to do is turn the light on  
  setupBaseFunctions(); // <-- must be FIRST
  // at this point you're connected, MQTT is working serial is enabled 115200
  delay(1000);
  //Consider MQTT Publish here to notify HA that we're now online (and in our startup state)
}

void MQTTcallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  char message[length + 1];
  for (int i = 0; i < length; i++) {
    message[i] = (char)payload[i];
  }
  message[length] = '\0';
  Serial.println(message);

  // Add Code here to handle MQTT Messages
}

/* 
 // Consider an MQTT Response (Required by HomeAssistant)  
 
void sendState() {
  StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["state"] = (stateOn) ? on_cmd : off_cmd;
  JsonObject& color = root.createNestedObject("color");
  color["r"] = red;
  color["g"] = green;
  color["b"] = blue;
  root["brightness"] = brightness;
  char buffer[root.measureLength() + 1];
  root.printTo(buffer, sizeof(buffer));
  client.publish(mqtt_topic, buffer, true);
}
*/


void loop() {
  // Be careful no functions here block, or do not return to the loop in a timely fashion, otherwise the reset button will break.


  
  
  //run at the end of each cycle:
  loopBaseFunctions();
}



void reconnect() {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(host, mqtt_username, mqtt_password)) {
      failedConnects = 0;
      Serial.println("connected");
      client.publish(mqtt_topic,"Connected");
      char subbuf[44]; // 40 + \0 + the number of characters we're concatenating below
      snprintf(subbuf, 44, "%s/%s", mqtt_topic, "set"); 
      client.subscribe(subbuf);
    } else {
      failedConnects++;
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
    if (failedConnects == 25) {
      ClearAllSettings();
    }
}

ESP8266WebServer server(80);
const char* serverIndex = "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";


bool readConfigFile() {
  // this opens the config file in read-mode
  File f = SPIFFS.open("/config.json", "r");
  
  if (!f) {
    Serial.println("Configuration file not found");
    return false;
  } else {
    size_t size = f.size();
    std::unique_ptr<char[]> buf(new char[size]);
    f.readBytes(buf.get(), size);
    f.close();
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.parseObject(buf.get());
    if (!json.success()) {
      Serial.println("JSON parseObject() failed");
      return false;
    }
    json.printTo(Serial);
    if (json.containsKey("mqtt_server")) {
      strcpy(mqtt_server, json["mqtt_server"]);
    }
    if (json.containsKey("mqtt_port")) {
      mqtt_port = json["mqtt_port"];
    }
    if (json.containsKey("mqtt_topic")) {
      strcpy(mqtt_topic, json["mqtt_topic"]);
    }
    if (json.containsKey("mqtt_username")) {
      strcpy(mqtt_username,json["mqtt_username"]);
    }
    if (json.containsKey("mqtt_password")) {
      strcpy(mqtt_password,json["mqtt_password"]);
    }
  }
  Serial.println("\nConfig file was successfully parsed");
  return true;
}

bool writeConfigFile() {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
    json["mqtt_topic"] = mqtt_topic;
    json["mqtt_username"] = mqtt_username;
    json["mqtt_password"] = mqtt_password; 
    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
      return false;
    }
    json.printTo(Serial);
    json.printTo(configFile);
    configFile.close();
    Serial.println("\nConfig file was successfully saved");
    return true;
}

void setupBaseFunctions() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println();
  /* drd conflicts with WifiManager, which rests multiple times during the setup 
  if (drd.detectDoubleReset()) {
    Serial.println("Double Reset Detected");
    delay(DRD_TIMEOUT * 1002);  // clearAllSettings will cause a reset; wait past the timeout before doing this!
    ClearAllSettings();
  }
  */
  sprintf(host, "esp8266-%d", ESP.getChipId());
  
  pinMode(0, INPUT_PULLUP);  // GPIO0 as input for Config mode selection
  attachInterrupt(0, ISRbuttonStateChanged, CHANGE);

  Serial.println("mounting FS...");
    if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (!readConfigFile()) {
      Serial.println("Failed to read configuration file, using default values");
    }
  } else {
    Serial.println("failed to mount FS");
  }
  
  if (WiFi.SSID() == "") {
    Serial.println("We haven't got any access point credentials, so get them now");
    initialConfig = true;
  } else {
    WiFi.mode(WIFI_STA); // Force to station mode because if device was switched off while in access point mode it will start up next time in access point mode.
    unsigned long startedAt = millis();
    WiFi.begin();
    Serial.print("After waiting ");
    int connRes = WiFi.waitForConnectResult();
    float waited = (millis()- startedAt);
    Serial.print(waited/1000);
    Serial.print(" secs in setup() connection result is ");
    Serial.println(connRes);
  }

  if (initialConfig) {
    WiFiManagerParameter custom_mqtt_server("server", "MQTT Server", mqtt_server, 20);
    char convertedValue[6];
    sprintf(convertedValue, "%d", mqtt_port);
    WiFiManagerParameter custom_mqtt_port("port", "MQTT Port (1883)", convertedValue, 5);
    WiFiManagerParameter custom_mqtt_subscribe("subscribe", "Topic to subscribe to", mqtt_topic, 40);
    WiFiManagerParameter custom_mqtt_username("username", "MQTT Username", mqtt_username, 20);
    WiFiManagerParameter custom_mqtt_password("password", "MQTT Password", mqtt_password, 20);
    WiFiManager wifiManager;
    //add all your parameters here
    wifiManager.addParameter(&custom_mqtt_server);
    wifiManager.addParameter(&custom_mqtt_port);
    wifiManager.addParameter(&custom_mqtt_username);
    wifiManager.addParameter(&custom_mqtt_password);
    wifiManager.addParameter(&custom_mqtt_subscribe);
    if (!wifiManager.startConfigPortal()) {
        Serial.println("Not connected to WiFi but continuing anyway.");
      } else {
        // If you get here you have connected to the WiFi
        Serial.println("Connected...yeey :)");
      }
    //read updated parameters
    strcpy(mqtt_server, custom_mqtt_server.getValue());
    mqtt_port = atoi(custom_mqtt_port.getValue());
    strcpy(mqtt_topic, custom_mqtt_subscribe.getValue());
    strcpy(mqtt_username, custom_mqtt_username.getValue());
    strcpy(mqtt_password, custom_mqtt_password.getValue());
    writeConfigFile();

    ESP.reset(); 
  }
  if (WiFi.status()!=WL_CONNECTED){
    Serial.println("Failed to connect, finishing setup anyway");
  } else{
    Serial.print("Local ip: ");
    Serial.println(WiFi.localIP());
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(MQTTcallback);
    //MDNS.begin(host); //arduinoOTA will start mdns
    server.on("/", HTTP_GET, []() {
      server.sendHeader("Connection", "close");
      server.sendHeader("Access-Control-Allow-Origin", "*");
      server.send(200, "text/html", serverIndex);
    });
    server.on("/update", HTTP_POST, []() {
      server.sendHeader("Connection", "close");
      server.sendHeader("Access-Control-Allow-Origin", "*");
      server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
      ESP.restart();
    }, []() {
      HTTPUpload& upload = server.upload();
      if (upload.status == UPLOAD_FILE_START) {
        Serial.setDebugOutput(true);
        WiFiUDP::stopAll();
        Serial.printf("Update: %s\n", upload.filename.c_str());
        uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        if (!Update.begin(maxSketchSpace)) { //start with max available size
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) { //true to set the size to the current progress
          Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
        } else {
          Update.printError(Serial);
        }
        Serial.setDebugOutput(false);
      }
      yield();
    });
    server.begin();
    MDNS.addService("http", "tcp", 80);
    Serial.printf("OTA FW Updates Ready! Open http://%s.local in your browser to upload new FW\n", host);
    // send it through a loop of the normal run to connect and get MQTT working before proceeding
    loopBaseFunctions();
    loopBaseFunctions();
    Serial.printf("Base heap size: %u\n", ESP.getFreeHeap());
    
    ArduinoOTA.onStart([]() {
      Serial.println("Start");
    });
    ArduinoOTA.onEnd([]() {
      Serial.println("\nEnd");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
    ArduinoOTA.setHostname(host);
    ArduinoOTA.begin();
  }
}

void loopBaseFunctions() {
// From Andreas Spiess, https://github.com/SensorsIot
  if (buttonChanged && buttonTime > 4000) ClearAllSettings();  // long button press > 4sec
//  if (buttonChanged && buttonTime > 500 && buttonTime < 4000) IOTappStory(); // long button press > 1sec
  buttonChanged = false;
  // This is the PubSubClient
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  //drd.loop();
  ArduinoOTA.handle();
  server.handleClient();
}


void ClearAllSettings() {
  Serial.println("Clearing Everthing!");
  //WiFiManager wifiManager; // need to re-create this, only to reset it.
  //wifiManager.resetSettings();
  WiFi.disconnect();
  SPIFFS.format();
  ESP.restart();
}

void ISRbuttonStateChanged() {
  noInterrupts();
  if (digitalRead(0) == 0) buttonEntry = millis();
  else {
    buttonTime = millis() - buttonEntry;
    buttonChanged = true;
  }
  interrupts();
}


