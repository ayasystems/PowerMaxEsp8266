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
      // Force immediate status publish
      publishAlarmStat();
      publishAlarmFlags();
      publishAlarmZones();
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
  


            for(int ix=0; ix<MAX_ZONE_COUNT; ix++)
            {
 
                if(ix >0 and ix < 6)
                {
                    publishAlarmZone(ix);                          
                }
                
                //char zoneId[10] = "";
                //sprintf(zoneId, "%d", ix);
                //outputStream->write(zoneId);
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
