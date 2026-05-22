extern bool mqtt_retain;

String macToStr(const uint8_t* mac){

    String result;
    
    for (int i = 0; i < 6; ++i) {
    result += String(mac[i], 16);
    
      if (i < 5){
        result += ':';
      }
    }
    
    return result;
}
/*
void callbackMqtt_old(char* topic, byte* payload, unsigned int length) {
   String srtPayload;
   
   for (int i=0;i<length;i++) {
        char receivedChar = (char)payload[i];
        srtPayload+=receivedChar;
   }
  addLog("MQTT: callbackMqtt: "+(String)topic+" "+srtPayload);  
     
  if (strcmp(topic,"powermax/command")==0){
    if(srtPayload=="ARMHOME")
      pm.sendCommand(Pmax_ARMHOME);
    if(srtPayload=="DISARM")
      pm.sendCommand(Pmax_DISARM);
    if(srtPayload=="ARMAWAY")
      pm.sendCommand(Pmax_ARMAWAY);
    if(srtPayload=="REBOOT")
      ESP.restart();
  }



}
*/
void callbackMqtt(char* topic, byte* payload, unsigned int length) {
  
        char message[51];
        int maxlength = 50;
        if (length < 50) {
            maxlength = length;
        }
        for (int i = 0; i < maxlength; i++) {
            message[i] = (char)payload[i];
            message[i + 1] = '\0';
        }
 
  addLog("MQTT: callbackMqtt: "+(String)topic+" "+message);  
     
  if (strcmp(topic,"powermax/command")==0){
    if((strcmp(message, "ARMHOME") == 0)) {
      pm.sendCommand(Pmax_ARMHOME);
      clientMqtt.publish("powermax/command_ack", "{\"command\":\"ARMHOME\",\"status\":\"received\"}", false);
    }
    else if((strcmp(message, "DISARM") == 0)) {
      pm.sendCommand(Pmax_DISARM);
      clientMqtt.publish("powermax/command_ack", "{\"command\":\"DISARM\",\"status\":\"received\"}", false);
    }
    else if((strcmp(message, "ARMAWAY") == 0)) {
      pm.sendCommand(Pmax_ARMAWAY);
      clientMqtt.publish("powermax/command_ack", "{\"command\":\"ARMAWAY\",\"status\":\"received\"}", false);
    }
    else if((strcmp(message, "REBOOT") == 0)) {
      clientMqtt.publish("powermax/command_ack", "{\"command\":\"REBOOT\",\"status\":\"received\"}", false);
      clientMqtt.publish(MQTT_AVAILABILITY_TOPIC, MQTT_LWT_OFFLINE, true);
      delay(200);
      ESP.restart();
    }
    else if((strcmp(message, "STATUS") == 0)) {
      // Force immediate status publish (zones via staggered sweep)
      publishAlarmStat();
      publishAlarmFlags();
      staggeredZoneIndex = 1;
      staggeredZoneActive = true;
      clientMqtt.publish("powermax/command_ack", "{\"command\":\"STATUS\",\"status\":\"received\"}", false);
    }
    else {
      addLog("MQTT: Unknown command: " + String(message));
    }
  }
}

bool isMqttConfigured() {
  return (strcmp(mqtt_server, "none") != 0 && strlen(mqtt_server) > 0);
}

bool isMqttConnected() {
  return isMqttConfigured() && clientMqtt.connected();
}

void setupMqtt() {
  if (!isMqttConfigured()) return;
  clientMqtt.setServer(mqtt_server, atoi(mqtt_port));
  clientMqtt.setBufferSize(mqttMaxPacketSize);
  clientMqtt.setKeepAlive(60); // 60s keepalive: discovery can publish many messages
  clientMqtt.setCallback(callbackMqtt);
}

bool connectMqtt() {
  if (!isMqttConfigured()) return false;

  // Connect with LWT (Last Will and Testament) for Home Assistant availability
  bool connected = false;
  if (strlen(mqtt_user) > 0) {
    connected = clientMqtt.connect(mqtt_client, mqtt_user, mqtt_pass,
                                    MQTT_AVAILABILITY_TOPIC, 1, true, MQTT_LWT_OFFLINE);
  } else {
    connected = clientMqtt.connect(mqtt_client,
                                    MQTT_AVAILABILITY_TOPIC, 1, true, MQTT_LWT_OFFLINE);
  }

  if (connected) {
    addLog("MQTT connected to " + String(mqtt_server));
    // Subscribe to command topic
    clientMqtt.subscribe("powermax/command");
    // Publish online status (retained)
    clientMqtt.publish(MQTT_AVAILABILITY_TOPIC, MQTT_LWT_ONLINE, true);
    // Reset backoff
    mqttReconnectInterval = 5000;
    mqttLastReconnectAttempt = 0;
    return true;
  } else {
    addLog("MQTT connection failed, rc=" + String(clientMqtt.state()));
    // Exponential backoff
    mqttReconnectInterval = min(mqttReconnectInterval * 2, (unsigned long)MQTT_RECONNECT_MAX_INTERVAL);
    return false;
  }
}

void publishAlarmStat(){
    if (!isMqttConnected()) return;

    StaticJsonDocument<mqttMaxPacketSize> doc;
    SystemStatus stat = pm.getStat();
    int panelType     = pm.getPanelType();
    int modelType     = pm.getModelType();
    int alarmState    = pm.getAlarmState();

    doc["client"]         = mqtt_client;
    doc["stat"]           = (String)stat;
    doc["stat_str"]       = pm.GetStrPmaxSystemStatus(stat);
    doc["lastCom"]        = (int)pm.getSecondsFromLastComm();
    doc["panelType"]      = (String)panelType;
    doc["panelTypeStr"]   = pm.GetStrPmaxPanelType(panelType);
    doc["panelModelType"] = (String)modelType;
    doc["alarmState"]     = (String)alarmState;
    doc["alarmStateStr"]  = pm.GetStrPmaxLogEvents(alarmState);    
    doc["uptime"]          = (unsigned long)(millis() / 1000);
    doc["freeHeap"]        = ESP.getFreeHeap();
    doc["wifiRSSI"]        = WiFi.RSSI();
    
    
    
    String output;
    serializeJson(doc, output);
    
    clientMqtt.publish("powermax/stat", output.c_str(), mqtt_retain); // You can activate the retain flag by setting the third parameter to true
    DEBUG(LOG_INFO,"powermax/stat");
    doc.clear();

    // Also publish the simple HA-compatible alarm state
    publishAlarmStateHA();
          
  
}

void publishAlarmFlags(){
    if (!isMqttConnected()) return;

    StaticJsonDocument<mqttMaxPacketSize> doc;
 
      //  outputStream->writeJsonTag("flags", flags);
    unsigned char flags = pm.getAlarmFlags();
        doc["client"]         = mqtt_client;
    doc["flags"]                = (String)flags;    
    doc["flags_ready"]          = pm.isFlagSet(0);
    doc["flags_alertInMemory"]  = pm.isFlagSet(1);
    doc["flags_trouble"]        = pm.isFlagSet(2);
    doc["flags_bypasOn"]        = pm.isFlagSet(3);
    doc["flags_last10sec"]      = pm.isFlagSet(4);
    doc["flags_zoneEvent"]      = pm.isFlagSet(5);
    doc["flags_armDisarmEvent"] = pm.isFlagSet(6);
    doc["flags_alarmEvent"]     = pm.isFlagSet(7);
    doc["FreeHeap"] = ESP.getFreeHeap();
    
    
    String output;
    serializeJson(doc, output);
    
    clientMqtt.publish("powermax/flags", output.c_str(), mqtt_retain); // You can activate the retain flag by setting the third parameter to true
    DEBUG(LOG_INFO,"powermax/flags");
    doc.clear();
  
}


void publishAlarmZones(){
            for(int ix=1; ix<MAX_ZONE_COUNT; ix++)
            {
                Zone z = pm.getAlarmZone(ix);
                if(z.enrolled)
                {
                    publishAlarmZone(ix);
                    clientMqtt.loop(); // keep MQTT alive
                    yield();           // feed WDT + allow serial ISR
                }
            }
  }  


void publishAlarmZone(unsigned char ix){
  publishAlarmZone((int)ix);
}
void publishAlarmZone(int ix){
                  if (!isMqttConnected()) return;
                 // int alarmTrippedZone;
                  Zone alarmZone;
                  unsigned long  currentTime = os_getCurrentTimeSec();             
                 //  int enrolledZones = pm.getEnrolledZoneCnt();
                  alarmZone = pm.getAlarmZone(ix);
               
                  //addLog("ix: "+(String)ix+" SensorId: "+(String)alarmZone.sensorId+" tripped: "+(String)alarmTrippedZone+" Enroled: "+(String)alarmZone.enrolled);
                  if(alarmZone.enrolled){
                                
                      StaticJsonDocument<mqttMaxPacketSize> doc;
                      
                      doc["client"]              = mqtt_client;
                      doc["zoneId"]              = ix; 
                      doc["zoneName"]            = alarmZone.name;
                      doc["zoneType"]            = alarmZone.zoneType;
                      doc["zoneTypeStr"]         = alarmZone.zoneTypeStr;
                      doc["sensorId"]            = alarmZone.sensorId;
                      doc["sensorType"]          = alarmZone.sensorType;
                      doc["sensorMake"]          = alarmZone.sensorMake;
                      doc["device_class"]        = getHADeviceClass(alarmZone.zoneType);
                      doc["enrolled"]            = alarmZone.enrolled;
                      doc["signalStrength"]      = alarmZone.signalStrength;
                      doc["lastEvent"]           = (int)alarmZone.lastEvent;
                      doc["lastEventStr"]        = pm.GetStrPmaxZoneEventTypes(alarmZone.lastEvent);
                      doc["lastEventTime"]       = alarmZone.lastEventTime;
                      doc["lastEventTimeAgo"]    = currentTime-alarmZone.lastEventTime;
                      doc["stat_doorOpen"]       = alarmZone.stat.doorOpen;
                      doc["stat_bypased"]        = alarmZone.stat.bypased;
                      doc["stat_lowBattery"]     = alarmZone.stat.lowBattery;
                      doc["stat_active"]         = alarmZone.stat.active;
                      doc["stat_tamper"]         = alarmZone.stat.tamper;
                      String output;
                      serializeJson(doc, output);
                      
                      if(!clientMqtt.publish("powermax/zone", output.c_str(), mqtt_retain)){
                        addLog( "ERROR -> Publish powermax/zone " ); 
                        addLog(output);
                      }
                      // Publish to specific topic: powermax/zoneX (easier to manage in HomeAssistant)
                      String specificTopic = "powermax/zone" + String(ix);
                      clientMqtt.publish(specificTopic.c_str(), output.c_str(), mqtt_retain);
                      
                      doc.clear();
                      DEBUG(LOG_INFO,"powermax/zone");
             
                 }
}
  
void publishSytemDisarmed(unsigned char whoDisarmed, const char* whoDisarmedStr){
  if (!isMqttConnected()) return;
  String output;  
  StaticJsonDocument<mqttMaxPacketSize> doc;
      doc["client"]         = mqtt_client;
  doc["event"]               = "DISARMED";
  doc["whoDisarmed"]         = whoDisarmed;
  doc["whoDisarmedStr"]         = whoDisarmedStr;
  serializeJson(doc, output);
  addLog("DISARMED: "+output);
  if(!clientMqtt.publish("powermax/system", output.c_str(), mqtt_retain)){
    addLog( "ERROR -> Publish powermax/system " ); 
    addLog(output);
  }  
  doc.clear();
}
void publishSytemArmed( unsigned char whoArmed, const char* whoArmedStr,unsigned char armType, const char* armTypeStr){
  if (!isMqttConnected()) return;
  String output;  
  StaticJsonDocument<mqttMaxPacketSize> doc;
      doc["client"]         = mqtt_client;
  doc["event"]               = "ARMED";
  doc["whoArmed"]            = whoArmed;
  doc["whoArmedStr"]         = whoArmedStr;
  doc["armType"]             = armType;
  doc["armTypeStr"]          = armTypeStr;
  serializeJson(doc, output);
  addLog("ARMED: "+output);
  if(!clientMqtt.publish("powermax/system", output.c_str(), mqtt_retain)){
    addLog( "ERROR -> Publish powermax/system " ); 
    addLog(output);
  }
  doc.clear();
}

void publishAlarmStarted(unsigned char alarmType, const char* alarmTypeStr, unsigned char zoneTripped, const char* zoneTrippedStr){
  if (!isMqttConnected()) return;
  String output;  
  StaticJsonDocument<mqttMaxPacketSize> doc;
      doc["client"]         = mqtt_client;
  doc["event"]              = "ALARMSTARTED";
  doc["alarmType"]          = alarmType;
  doc["alarmTypeStr"]       = alarmTypeStr;
  doc["zoneTripped"]        = zoneTripped;
  doc["zoneTrippedStr"]     = zoneTrippedStr;
  serializeJson(doc, output);
  addLog("ALARMSTARTED: "+output);
  if(!clientMqtt.publish("powermax/system", output.c_str(), mqtt_retain)){
    addLog( "ERROR -> Publish powermax/system " ); 
    addLog(output);
  } 
  doc.clear();

}

void publishAlarmCancelled(unsigned char whoDisarmed, const char* whoDisarmedStr){
  if (!isMqttConnected()) return;
  String output;  
  StaticJsonDocument<mqttMaxPacketSize> doc;
      doc["client"]         = mqtt_client;
  doc["event"]               = "ALARMCANCELLED";
  doc["whoDisarmed"]         = whoDisarmed;
  doc["whoDisarmedStr"]         = whoDisarmedStr;
  serializeJson(doc, output);
  addLog("ALARMCANCELLED: "+output);
  if(!clientMqtt.publish("powermax/system", output.c_str(), mqtt_retain)){
    addLog( "ERROR -> Publish powermax/system " ); 
    addLog(output);
  }
  doc.clear();
}

// ============================================================================
// Home Assistant MQTT Auto-Discovery
// ============================================================================

// Maps PowerMax SystemStatus to HA alarm_control_panel state string
const char* getHAAlarmState() {
  SystemStatus stat = pm.getStat();
  // If alarm event flag is set, system is triggered
  if (pm.isFlagSet(7)) return "triggered";

  switch (stat) {
    case SS_Disarm:       return "disarmed";
    case SS_Exit_Delay:   return "arming";
    case SS_Exit_Delay2:  return "arming";
    case SS_Entry_Delay:  return "pending";
    case SS_Armed_Home:   return "armed_home";
    case SS_Armed_Away:   return "armed_away";
    case SS_Home_Bypass:  return "armed_home";
    case SS_Away_Bypass:  return "armed_away";
    case SS_Ready:        return "disarmed";
    case SS_Not_Ready:    return "disarmed";
    case SS_User_Test:    return "disarmed";
    case SS_Downloading:  return "disarmed";
    case SS_Programming:  return "disarmed";
    case SS_Installer:    return "disarmed";
    default:              return "disarmed";
  }
}

// Publish the simple HA-compatible alarm state (does NOT replace existing powermax/stat)
void publishAlarmStateHA() {
  if (!isMqttConnected()) return;
  clientMqtt.publish(HA_ALARM_STATE_TOPIC, getHAAlarmState(), true);
}

// Returns HA device_class for a zone binary_sensor.
// All zones default to "door" — can be overridden manually in HA.
const char* getHADeviceClass(unsigned char zoneType) {
  return "door";
}

// Helper: add the common HA device block to a JSON doc
void addHADeviceBlock(JsonObject device) {
  JsonArray ids = device.createNestedArray("identifiers");
  ids.add(mqtt_client);
  device["name"] = "PowerMax Alarm";
  device["manufacturer"] = "Visonic";
  int ptype = pm.getPanelType();
  device["model"] = (ptype >= 0) ? pm.GetStrPmaxPanelType(ptype) : "PowerMax";
  device["sw_version"] = "PowerMaxEsp8266";
}

// Helper: add availability config
void addHAAvailability(JsonDocument& doc) {
  doc["availability_topic"] = MQTT_AVAILABILITY_TOPIC;
  doc["payload_available"] = MQTT_LWT_ONLINE;
  doc["payload_not_available"] = MQTT_LWT_OFFLINE;
}

// Publish HA discovery for the alarm control panel
void publishHADiscoveryAlarmPanel() {
  if (!isMqttConnected()) return;

  DynamicJsonDocument doc(700);
  doc["name"] = "PowerMax";
  doc["unique_id"] = String(mqtt_client) + "_alarm";
  doc["object_id"] = "powermax_alarm";
  doc["state_topic"] = HA_ALARM_STATE_TOPIC;
  doc["command_topic"] = "powermax/command";
  doc["payload_arm_home"] = "ARMHOME";
  doc["payload_arm_away"] = "ARMAWAY";
  doc["payload_disarm"] = "DISARM";
  if (strlen(ha_code) > 0) {
    doc["code"] = ha_code;
  }
  doc["code_arm_required"] = true;
  doc["code_disarm_required"] = true;
  addHAAvailability(doc);

  JsonObject device = doc.createNestedObject("device");
  addHADeviceBlock(device);

  String output;
  serializeJson(doc, output);
  String topic = String(HA_DISCOVERY_PREFIX) + "/alarm_control_panel/" + mqtt_client + "/alarm/config";
  if (!clientMqtt.publish(topic.c_str(), output.c_str(), true)) {
    addLog("HA discovery FAILED: alarm panel (buffer too small?)");
  }
  doc.clear();
}

// Publish HA discovery for system diagnostic sensors
void publishHADiscoveryDiagnostics() {
  if (!isMqttConnected()) return;

  // WiFi RSSI sensor
  {
    DynamicJsonDocument doc(512);
    doc["name"] = "PowerMax WiFi Signal";
    doc["unique_id"] = String(mqtt_client) + "_wifi_rssi";
    doc["object_id"] = "powermax_wifi_rssi";
    doc["state_topic"] = "powermax/stat";
    doc["value_template"] = "{{ value_json.wifiRSSI }}";
    doc["unit_of_measurement"] = "dBm";
    doc["device_class"] = "signal_strength";
    doc["entity_category"] = "diagnostic";
    doc["enabled_by_default"] = false;
    addHAAvailability(doc);
    JsonObject device = doc.createNestedObject("device");
    addHADeviceBlock(device);

    String output;
    serializeJson(doc, output);
    String topic = String(HA_DISCOVERY_PREFIX) + "/sensor/" + mqtt_client + "/wifi_rssi/config";
    clientMqtt.publish(topic.c_str(), output.c_str(), true);
  }

  // Uptime sensor
  {
    DynamicJsonDocument doc(512);
    doc["name"] = "PowerMax Uptime";
    doc["unique_id"] = String(mqtt_client) + "_uptime";
    doc["object_id"] = "powermax_uptime";
    doc["state_topic"] = "powermax/stat";
    doc["value_template"] = "{{ value_json.uptime }}";
    doc["unit_of_measurement"] = "s";
    doc["device_class"] = "duration";
    doc["entity_category"] = "diagnostic";
    doc["enabled_by_default"] = false;
    addHAAvailability(doc);
    JsonObject device = doc.createNestedObject("device");
    addHADeviceBlock(device);

    String output;
    serializeJson(doc, output);
    String topic = String(HA_DISCOVERY_PREFIX) + "/sensor/" + mqtt_client + "/uptime/config";
    clientMqtt.publish(topic.c_str(), output.c_str(), true);
  }

  // Last communication sensor
  {
    DynamicJsonDocument doc(512);
    doc["name"] = "PowerMax Last Comm";
    doc["unique_id"] = String(mqtt_client) + "_lastcom";
    doc["object_id"] = "powermax_last_comm";
    doc["state_topic"] = "powermax/stat";
    doc["value_template"] = "{{ value_json.lastCom }}";
    doc["unit_of_measurement"] = "s";
    doc["entity_category"] = "diagnostic";
    doc["enabled_by_default"] = false;
    addHAAvailability(doc);
    JsonObject device = doc.createNestedObject("device");
    addHADeviceBlock(device);

    String output;
    serializeJson(doc, output);
    String topic = String(HA_DISCOVERY_PREFIX) + "/sensor/" + mqtt_client + "/lastcom/config";
    clientMqtt.publish(topic.c_str(), output.c_str(), true);
  }

  // Trouble binary sensor
  {
    DynamicJsonDocument doc(512);
    doc["name"] = "PowerMax Trouble";
    doc["unique_id"] = String(mqtt_client) + "_trouble";
    doc["object_id"] = "powermax_trouble";
    doc["state_topic"] = "powermax/flags";
    doc["value_template"] = "{{ 'ON' if value_json.flags_trouble else 'OFF' }}";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["device_class"] = "problem";
    doc["entity_category"] = "diagnostic";
    addHAAvailability(doc);
    JsonObject device = doc.createNestedObject("device");
    addHADeviceBlock(device);

    String output;
    serializeJson(doc, output);
    String topic = String(HA_DISCOVERY_PREFIX) + "/binary_sensor/" + mqtt_client + "/trouble/config";
    clientMqtt.publish(topic.c_str(), output.c_str(), true);
  }

  // Not Ready binary sensor — ON when panel is "Not Ready" (e.g. zone open while disarmed)
  {
    DynamicJsonDocument doc(512);
    doc["name"] = "PowerMax Not Ready";
    doc["unique_id"] = String(mqtt_client) + "_not_ready";
    doc["object_id"] = "powermax_not_ready";
    doc["state_topic"] = "powermax/stat";
    doc["value_template"] = "{{ 'ON' if value_json.stat_str == 'Not Ready' else 'OFF' }}";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["device_class"] = "problem";
    addHAAvailability(doc);
    JsonObject device = doc.createNestedObject("device");
    addHADeviceBlock(device);

    String output;
    serializeJson(doc, output);
    String topic = String(HA_DISCOVERY_PREFIX) + "/binary_sensor/" + mqtt_client + "/not_ready/config";
    clientMqtt.publish(topic.c_str(), output.c_str(), true);
  }

  // Panel status sensor — shows the raw status string (Ready, Not Ready, Armed Home, etc.)
  {
    DynamicJsonDocument doc(512);
    doc["name"] = "PowerMax Status";
    doc["unique_id"] = String(mqtt_client) + "_panel_status";
    doc["object_id"] = "powermax_panel_status";
    doc["state_topic"] = "powermax/stat";
    doc["value_template"] = "{{ value_json.stat_str }}";
    doc["icon"] = "mdi:shield-home";
    addHAAvailability(doc);
    JsonObject device = doc.createNestedObject("device");
    addHADeviceBlock(device);

    String output;
    serializeJson(doc, output);
    String topic = String(HA_DISCOVERY_PREFIX) + "/sensor/" + mqtt_client + "/panel_status/config";
    clientMqtt.publish(topic.c_str(), output.c_str(), true);
  }
}

// Publish HA discovery for a single zone (state + battery + tamper)
void publishHADiscoveryZone(int ix) {
  if (!isMqttConnected()) return;

  Zone alarmZone = pm.getAlarmZone(ix);
  if (!alarmZone.enrolled) return;

  String zoneIdStr = String(ix);
  String zoneTopic = "powermax/zone" + zoneIdStr;
  String zoneName = String(alarmZone.name);
  if (zoneName.length() == 0) zoneName = "Zone " + zoneIdStr;

  const char* devClass = getHADeviceClass(alarmZone.zoneType);

  // --- Zone state (door open / motion detected) ---
  {
    DynamicJsonDocument doc(600);
    doc["name"] = zoneName;
    doc["unique_id"] = String(mqtt_client) + "_zone" + zoneIdStr + "_state";
    doc["object_id"] = "powermax_zone" + zoneIdStr;
    doc["state_topic"] = zoneTopic;
    // Same logic as legacy YAML: door open OR (motion within 120s).
    // lastEvent==5 is ZE_Violated (motion). Doors use stat_doorOpen.
    doc["value_template"] = "{{ 'ON' if value_json.stat_doorOpen or (value_json.lastEvent == 5 and value_json.lastEventTimeAgo | int <= 120) else 'OFF' }}";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["device_class"] = devClass;
    addHAAvailability(doc);
    JsonObject device = doc.createNestedObject("device");
    addHADeviceBlock(device);

    String output;
    serializeJson(doc, output);
    String topic = String(HA_DISCOVERY_PREFIX) + "/binary_sensor/" + mqtt_client + "/zone" + zoneIdStr + "_state/config";
    clientMqtt.publish(topic.c_str(), output.c_str(), true);
  }

  // --- Zone battery (low battery) ---
  {
    DynamicJsonDocument doc(550);
    doc["name"] = zoneName + " Battery";
    doc["unique_id"] = String(mqtt_client) + "_zone" + zoneIdStr + "_battery";
    doc["object_id"] = "powermax_zone" + zoneIdStr + "_battery";
    doc["state_topic"] = zoneTopic;
    doc["value_template"] = "{{ 'ON' if value_json.stat_lowBattery else 'OFF' }}";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["device_class"] = "battery";
    doc["entity_category"] = "diagnostic";
    doc["enabled_by_default"] = true;
    addHAAvailability(doc);
    JsonObject device = doc.createNestedObject("device");
    addHADeviceBlock(device);

    String output;
    serializeJson(doc, output);
    String topic = String(HA_DISCOVERY_PREFIX) + "/binary_sensor/" + mqtt_client + "/zone" + zoneIdStr + "_battery/config";
    clientMqtt.publish(topic.c_str(), output.c_str(), true);
  }

  // --- Zone tamper ---
  {
    DynamicJsonDocument doc(550);
    doc["name"] = zoneName + " Tamper";
    doc["unique_id"] = String(mqtt_client) + "_zone" + zoneIdStr + "_tamper";
    doc["object_id"] = "powermax_zone" + zoneIdStr + "_tamper";
    doc["state_topic"] = zoneTopic;
    doc["value_template"] = "{{ 'ON' if value_json.stat_tamper else 'OFF' }}";
    doc["payload_on"] = "ON";
    doc["payload_off"] = "OFF";
    doc["device_class"] = "tamper";
    doc["entity_category"] = "diagnostic";
    doc["enabled_by_default"] = true;
    addHAAvailability(doc);
    JsonObject device = doc.createNestedObject("device");
    addHADeviceBlock(device);

    String output;
    serializeJson(doc, output);
    String topic = String(HA_DISCOVERY_PREFIX) + "/binary_sensor/" + mqtt_client + "/zone" + zoneIdStr + "_tamper/config";
    clientMqtt.publish(topic.c_str(), output.c_str(), true);
  }

}

// Publish all HA discovery messages (call on connect and periodically)
void publishHADiscovery() {
  if (!isMqttConnected()) return;

  addLog("Publishing HA Auto-Discovery...");

  // Alarm panel
  publishHADiscoveryAlarmPanel();
  clientMqtt.loop(); // keep MQTT alive during long publish sequence

  // System diagnostics
  publishHADiscoveryDiagnostics();
  clientMqtt.loop();

  // All enrolled zones — call loop() between each to prevent keepalive timeout
  for (int ix = 1; ix < MAX_ZONE_COUNT; ix++) {
    Zone z = pm.getAlarmZone(ix);
    if (z.enrolled) {
      publishHADiscoveryZone(ix);
      clientMqtt.loop();
      yield(); // also feed WDT
    }
  }

  haDiscoveryPublished = true;
  lastHADiscovery = millis();
  addLog("HA Auto-Discovery published");
}
