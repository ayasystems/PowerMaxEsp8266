 
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
    if((strcmp(message, "ARMHOME") == 0))
      pm.sendCommand(Pmax_ARMHOME);
    if((strcmp(message, "DISARM") == 0))
      pm.sendCommand(Pmax_DISARM);
    if((strcmp(message, "ARMAWAY") == 0))
      pm.sendCommand(Pmax_ARMAWAY);
    if((strcmp(message, "REBOOT") == 0))
      ESP.restart();
  }



}

void connectMqtt(){
  if (strcmp(mqtt_server,"none")!=0){
      clientMqtt.setServer(mqtt_server, atoi(mqtt_port));
      clientMqtt.setBufferSize(mqttMaxPacketSize);
      clientMqtt.connect(mqtt_client, mqtt_user, mqtt_pass);
      clientMqtt.subscribe("powermax/command"); 
      clientMqtt.setCallback(callbackMqtt);
  }  
}

void publishAlarmStat(){

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
    
    
    
    String output;
    serializeJson(doc, output);
    
    clientMqtt.publish("powermax/stat", output.c_str()); // You can activate the retain flag by setting the third parameter to true
    DEBUG(LOG_INFO,"powermax/stat");
    doc.clear();
          
  
}

void publishAlarmFlags(){

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
    
    clientMqtt.publish("powermax/flags", output.c_str()); // You can activate the retain flag by setting the third parameter to true
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
               
                      if(!clientMqtt.publish("powermax/zone", output.c_str())){
                        addLog( "ERROR -> Publish powermax/zone " ); 
                        addLog(output);
                      }
                      doc.clear();                         
                      DEBUG(LOG_INFO,"powermax/zone");
             
                 }  
}
  
void publishSytemDisarmed(unsigned char whoDisarmed, const char* whoDisarmedStr){
  String output;  
  StaticJsonDocument<mqttMaxPacketSize> doc;
      doc["client"]         = mqtt_client;
  doc["event"]               = "DISARMED";
  doc["whoDisarmed"]         = whoDisarmed;
  doc["whoDisarmedStr"]         = whoDisarmedStr;
  serializeJson(doc, output);
  addLog("DISARMED: "+output);
  if(!clientMqtt.publish("powermax/system", output.c_str())){
    addLog( "ERROR -> Publish powermax/system " ); 
    addLog(output);
  }  
  doc.clear();
}
void publishSytemArmed( unsigned char whoArmed, const char* whoArmedStr,unsigned char armType, const char* armTypeStr){
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
  if(!clientMqtt.publish("powermax/system", output.c_str())){
    addLog( "ERROR -> Publish powermax/system " ); 
    addLog(output);
  }
  doc.clear();
}

void publishAlarmStarted(unsigned char alarmType, const char* alarmTypeStr, unsigned char zoneTripped, const char* zoneTrippedStr){
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
  if(!clientMqtt.publish("powermax/system", output.c_str())){
    addLog( "ERROR -> Publish powermax/system " ); 
    addLog(output);
  } 
  doc.clear();

}

void publishAlarmCancelled(unsigned char whoDisarmed, const char* whoDisarmedStr){
  String output;  
  StaticJsonDocument<mqttMaxPacketSize> doc;
      doc["client"]         = mqtt_client;
  doc["event"]               = "ALARMCANCELLED";
  doc["whoDisarmed"]         = whoDisarmed;
  doc["whoDisarmedStr"]         = whoDisarmedStr;
  serializeJson(doc, output);
  addLog("ALARMCANCELLED: "+output);
  if(!clientMqtt.publish("powermax/system", output.c_str())){
    addLog( "ERROR -> Publish powermax/system " ); 
    addLog(output);
  }
  doc.clear();
}
