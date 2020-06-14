

 

void wifiManagerSetup(){
    //Local intialization. Once its business is done, there is no need to keep it around
//Set hostname
  WiFi.hostname("powermaxEsp"); 
  WiFiManager wifiManager;
  //reset saved settings
  //wifiManager.resetSettings();
  wifiManager.setDebugOutput(false);
  WiFiManagerParameter custom_mqtt_server("mqtt_server", "mqtt_server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("mqtt_port", "mqtt_port", mqtt_port, 6);
  WiFiManagerParameter custom_mqtt_user("mqtt_user", "mqtt_user", mqtt_user, 40); 
  WiFiManagerParameter custom_mqtt_pass("mqtt_pass", "mqtt_pass", mqtt_pass, 40);
  WiFiManagerParameter custom_mqtt_client("mqtt_client", "mqtt_client", mqtt_client, 40);  

  //WiFiManager


  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //set static ip
  //wifiManager.setSTAStaticIPConfig(IPAddress(10,1,1,99), IPAddress(10,1,1,1), IPAddress(255,255,255,0));
  
  //add all your parameters here
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_mqtt_user);
  wifiManager.addParameter(&custom_mqtt_pass);
  wifiManager.addParameter(&custom_mqtt_client);

 

  //set minimu quality of signal so it ignores AP's under that quality
  //defaults to 8%
  //wifiManager.setMinimumSignalQuality();
  
  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  //wifiManager.setTimeout(120);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  /*
  if (!wifiManager.autoConnect("AutoConnectAP", "password")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }
  */
wifiManager.autoConnect();
  WiFi.hostname("powermaxEsp"); 
  //if you get here you have connected to the WiFi
  //Serial.println("connected...yeey :)");
  server.reset(new ESP8266WebServer(WiFi.localIP(), 80));
  //read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(mqtt_user, custom_mqtt_user.getValue());
  strcpy(mqtt_pass, custom_mqtt_pass.getValue());
  strcpy(mqtt_client, custom_mqtt_client.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    //Serial.println("saving config");
    //DynamicJsonBuffer jsonBuffer;
    DynamicJsonDocument  json(1024);
    //JsonObject& json = jsonBuffer.createObject();
   
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
    json["mqtt_user"] = mqtt_user;
    json["mqtt_pass"] = mqtt_pass;
    json["mqtt_client"] = mqtt_client;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      //Serial.println("failed to open config file for writing");
      addLog("Failed to open config FS for writing");
    }

    //json.printTo(Serial);
    //json.printTo(configFile);
    char jsonLog[1024];
    serializeJson(json, jsonLog);
    addLog(jsonLog);
    serializeJson(json, configFile);
    configFile.close();
    //end save
  }
WiFi.mode(WIFI_STA); // Force to station mode because if device was switched off while in access point mode it will start up next time in access point mode.
    
  //Serial.println("local ip");
  //Serial.println(WiFi.localIP());
    
}

void createFS(){
    if (SPIFFS.begin()) {
        //Serial.println("mounted file system");
        if (SPIFFS.exists("/config.json")) {
          //file exists, reading and loading
          //Serial.println("reading config file");
          File configFile = SPIFFS.open("/config.json", "r");
          if (configFile) {
            //Serial.println("opened config file");
            size_t size = configFile.size();
            // Allocate a buffer to store contents of the file.
            std::unique_ptr<char[]> buf(new char[size]);
    
            configFile.readBytes(buf.get(), size);
            //DynamicJsonBuffer jsonBuffer;
            DynamicJsonDocument  json(1024);
            //JsonObject& json = jsonBuffer.parseObject(buf.get());
            auto error = deserializeJson(json, buf.get());
            //json.printTo(Serial);
            if (!error) {
              //Serial.println("\nparsed json");
    /*
     char mqtt_server[40];
     char mqtt_port[6] = "1883";
     char mqtt_user[40];
     char mqtt_pass[40];
     char mqtt_client = "EspPowerMax";
     */
              strcpy(mqtt_server, json["mqtt_server"]);
              strcpy(mqtt_port,   json["mqtt_port"]);
              strcpy(mqtt_user,   json["mqtt_user"]);
              strcpy(mqtt_pass,   json["mqtt_pass"]);
              strcpy(mqtt_client, json["mqtt_client"]);
              
              char jsonLog[1024];
              serializeJson(json, jsonLog);
              addLog(jsonLog);

            } else {
              addLog(1,"failed to load json config");
            }
            configFile.close();
          }
        }
      } else {
        //Serial.println("Fail to mount FS");        
        addLog("failed to mount FS");
      }
      //end read
}
