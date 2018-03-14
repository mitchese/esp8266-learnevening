////////////
// LED For HomeAssistant
// 
// Define in configuration.yaml like this: 
// - platform: mqtt_json
//   name: mqtt_json_light_1
//   state_topic: "home/rgb1"
//   command_topic: "home/rgb1/set"
//   brightness: true
//   rgb: true
//   optimistic: false
//   qos: 0
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
void sendState();
bool processJson(char*);
int calculateStep(int, int);
int calculateVal(int, int, int);
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
void sendState();
bool processJson(char*);
int calculateStep(int, int);
int calculateVal(int, int, int);


#include <IRremoteESP8266.h> // https://github.com/markszabo/IRremoteESP8266.git
IRrecv irrecv(4); // D2
decode_results results;
#include "ircodes.h"

//added for motion
#define MOTIONPIN 16
bool motion=false; 

const int redPin = 14;//D5;
const int greenPin = 12;//D1;
const int bluePin = 5;//D6;
const char* on_cmd = "ON";
const char* off_cmd = "OFF";
const int BUFFER_SIZE = JSON_OBJECT_SIZE(10);
// Maintained state for reporting to HA
byte red = 255;
byte green = 255;
byte blue = 255;
byte brightness = 255;
// Real values to write to the LEDs (ex. including brightness and state)
byte realRed = 0;
byte realGreen = 0;
byte realBlue = 0;
bool stateOn = true;
// Globals for fade/transitions
bool startFade = false;
unsigned long lastLoop = 0;
int transitionTime = 0;
bool inFade = false;
int loopCount = 0;
int stepR, stepG, stepB;
int redVal, grnVal, bluVal;

  /*
  SAMPLE PAYLOAD:
    {
      "brightness": 120,
      "color": {
        "r": 255,
        "g": 100,
        "b": 100
      },
      "flash": 2,
      "transition": 5,
      "state": "ON"
    }
  */

  
void setup() {
  // the very FIRST thing to do is turn the light on
  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);
  // added for motion
  pinMode(MOTIONPIN, INPUT);
  
  analogWriteRange(255);
  digitalWrite(redPin, 200);
  digitalWrite(greenPin, 200);
  digitalWrite(bluePin, 170);
  
  setupBaseFunctions(); // <-- must be FIRST
  // at this point you're connected, MQTT is working serial is enabled 115200
  delay(1000);
  Serial.println("Updating MQTT after boot");
  //and now that we're connected, notify HomeAssistant that we're on and bright
  sendState();
  irrecv.enableIRIn();
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

  if (!processJson(message)) {
    return;
  }

  if (stateOn) {
    // Update lights
    realRed = map(red, 0, 255, 0, brightness);
    realGreen = map(green, 0, 255, 0, brightness);
    realBlue = map(blue, 0, 255, 0, brightness);
  }
  else {
    realRed = 0;
    realGreen = 0;
    realBlue = 0;
  }

  startFade = true;
  inFade = false; // Kill the current fade
  sendState();
}


bool processJson(char* message) {
  StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(message);
  if (!root.success()) {
    Serial.println("parseObject() failed");
    return false;
  }

  if (root.containsKey("state")) {
    if (strcmp(root["state"], on_cmd) == 0) {
      stateOn = true;
    }
    else if (strcmp(root["state"], off_cmd) == 0) {
      stateOn = false;
    }
  }
  if (root.containsKey("color")) {
    red = root["color"]["r"];
    green = root["color"]["g"];
    blue = root["color"]["b"];
  }
  if (root.containsKey("brightness")) {
    brightness = root["brightness"];
  }
  if (root.containsKey("transition")) {
    transitionTime = root["transition"];
  }
  else {
    transitionTime = 0;
  }
  return true;
}

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


void setColor(int inR, int inG, int inB) {
  analogWrite(redPin, 255 - inR);
  analogWrite(greenPin, 255 - inG);
  analogWrite(bluePin, 255 - inB);
}

void setLightColour(long inColour) {
  /* setLightColour - change the colour of the light strip
      Input: long inColour - should be a hex string of format FFFFFF
             where colours are RGB.
      Return: Return 0 if successful completion of setting the lights.
       Required constants: REDPIN, GREENPIN, BLUEPIN to be set
  */

  // Split the colour number into the individual channels
  blue = inColour % 0x100;
  green = (inColour / 0x100) % 0x100;
  red = (inColour / 0x100 / 0x100) % 0x100;
  // Write each colour to the correct pin, using analogwrite
  // to allow for dimmable PWMing
  if (inColour == 0) 
    stateOn=0;
  else
    stateOn=1;
  realRed = map(red, 0, 255, 0, brightness);
  realGreen = map(green, 0, 255, 0, brightness);
  realBlue = map(blue, 0, 255, 0, brightness);
  startFade = true;
  inFade = false; // Kill the current fade
  sendState();
}
void loop() {
  // Be careful no functions here block, or do not return to the loop in a timely fashion, otherwise the reset button will break.
  if (motion != digitalRead(MOTIONPIN)) {
    Serial.println("Motion was change state"); 
    motion = digitalRead(MOTIONPIN); 
    char subbuf[40];
    snprintf(subbuf, 40, "%s/%s", mqtt_topic, "pir"); 
    client.publish(subbuf, motion?"{\"motion\":1}":"{\"motion\":0}", true);
  }
  
  if (startFade) {
    // If we don't want to fade, skip it.
    if (transitionTime == 0) {
      setColor(realRed, realGreen, realBlue);
      redVal = realRed;
      grnVal = realGreen;
      bluVal = realBlue;
      startFade = false;
    }
    else {
      loopCount = 0;
      stepR = calculateStep(redVal, realRed);
      stepG = calculateStep(grnVal, realGreen);
      stepB = calculateStep(bluVal, realBlue);
      inFade = true;
    }
  }
  if (inFade) {
    startFade = false;
    unsigned long now = millis();
    if (now - lastLoop > transitionTime) {
      if (loopCount <= 1020) {
        lastLoop = now;
        
        redVal = calculateVal(stepR, redVal, loopCount);
        grnVal = calculateVal(stepG, grnVal, loopCount);
        bluVal = calculateVal(stepB, bluVal, loopCount);
        
        setColor(redVal, grnVal, bluVal); // Write current values to LED pins

        Serial.print("Loop count: ");
        Serial.println(loopCount);
        loopCount++;
      }
      else {
        inFade = false;
      }
    }
  }
  if (irrecv.decode(&results)) {
    Serial.println(results.value, HEX);
    irrecv.resume(); // Receive the next value
    switch (results.value) {
      case KEY44_BRIGHTER:
        break;
      case KEY44_DIMMER:
        break;
      case KEY44_PLAY:
        setLightColour(0xdddddd);
        break;
      case KEY44_POWER:
        setLightColour(0x000000); //off
        break;
      case KEY44_RED:
        setLightColour(0xcc0000);
        break;
      case KEY44_GREEN:
        setLightColour(0x00cc00);
        break;
      case KEY44_BLUE:
        setLightColour(0x0000cc);
        break;
      case KEY44_WHITE:
        setLightColour(0xffffff); //on full
        break;
      case KEY44_C1:
        setLightColour(0xcc2200); //dark orange
        break;
      case KEY44_C2:
        setLightColour(0x02cc22); //greeny
        break;
      case KEY44_C3:
        setLightColour(0x0222cc); //bluey
        break;
      case KEY44_C4:
        setLightColour(0xFBEC5D); // pale yellow
        break;
      case KEY44_C5:
        setLightColour(0xcc4411); //orangy
        break;
      case KEY44_C6:
        setLightColour(0x22cc88); //greeny blue
        break;
      case KEY44_C7:
        setLightColour(0x5522cc); //purply blue
        break;
      case KEY44_C8:
        setLightColour(0xFC99DD); //pink
        break;
      case KEY44_C9:
        setLightColour(0xcc7700); //orange
        break;
      case KEY44_C10:
        setLightColour(0x55ddaa); // bluer green
        break;
      case KEY44_C11:
        setLightColour(0x6600cc); //purple
        break;
      case KEY44_C12:
        setLightColour(0x7BAC9D); //some blue green colour
        break;
      case KEY44_C13:
        setLightColour(0xcccc00); // yellow
        break;
      case KEY44_C14:
        setLightColour(0x23bbaa); //blueish
        break;
      case KEY44_C15:
        setLightColour(0xcc00cc); //magenta
        break;
      case KEY44_C16:
        setLightColour(0xddffdd); //whitiesh with green
        break;
      case KEY44_REDUP:
        break;
      case KEY44_GREENUP:
        break;
      case KEY44_BLUEUP:
        break;
      case KEY44_QUICK: //speeds up rainbow function by reducing number of colours and delay
        break;
      case KEY44_REDDOWN:
        break;
      case KEY44_GREENDOWN:
        break;
      case KEY44_BLUEDOWN:
        break;
      case KEY44_SLOW: //slows down rainbow function by increasing delay and number of colours
        break;
      case KEY44_DIY1:
        break;
      case KEY44_DIY2:
        break;
      case KEY44_DIY3:
        break;
      case KEY44_AUTO:
        break;
      case KEY44_DIY4:
        break;
      case KEY44_DIY5:
        break;
      case KEY44_DIY6:
        break;
      case KEY44_FLASH:
        break;
      case KEY44_JUMP3:
        break;
      case KEY44_JUMP7:
        break;
      case KEY44_FADE3:
        break;
      case KEY44_FADE7:
        break;
      case 0xFFFFFF:
        break;
    }
  }

  //run at the end of each cycle:
  loopBaseFunctions();
}


int calculateStep(int prevValue, int endValue) {
    int step = endValue - prevValue; // What's the overall gap?
    if (step) {                      // If its non-zero, 
        step = 1020/step;            //   divide by 1020
    }
    
    return step;
}

/* The next function is calculateVal. When the loop value, i,
*  reaches the step size appropriate for one of the
*  colors, it increases or decreases the value of that color by 1. 
*  (R, G, and B are each calculated separately.)
*/
int calculateVal(int step, int val, int i) {
    if ((step) && i % step == 0) { // If step is non-zero and its time to change a value,
        if (step > 0) {              //   increment the value if step is positive...
            val += 1;
        }
        else if (step < 0) {         //   ...or decrement it if step is negative
            val -= 1;
        }
    }
    
    // Defensive driving: make sure val stays in the range 0-255
    if (val > 255) {
        val = 255;
    }
    else if (val < 0) {
        val = 0;
    }
    
    return val;
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
  
  pinMode(0, INPUT);  // GPIO0 as input for Config mode selection
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


