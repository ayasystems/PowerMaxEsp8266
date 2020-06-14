#include <FS.h>   
#include <DNSServer.h>
#include <pmax.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
#include <ArduinoOTA.h>
 #include <WiFiManager.h> 
//#include <SoftwareSerial.h>
//#define MQTT_MAX_PACKET_SIZE 512 //need to change in PubSubClient.h library
#include <PubSubClient.h>
#include <ArduinoJson.h>//v6
//////////////////// IMPORTANT DEFINES, ADJUST TO YOUR NEEDS //////////////////////
//ESP8266WebServer server(80);
std::unique_ptr<ESP8266WebServer> server;

//Telnet allows to see debug logs via network, it allso allows CONTROL of the system if PM_ALLOW_CONTROL is defined
#define PM_ENABLE_TELNET_ACCESS

//This enables control over your system, when commented out, alarm is 'read only' (esp will read the state, but will never ardm/disarm)
#define PM_ALLOW_CONTROL

//This enables flashing of ESP via OTA (WiFI)
#define PM_ENABLE_OTA_UPDATES

//This enables ESP to send a multicast UDP packet on LAN with notifications
//Any device on your LAN can register to listen for those messages, and will get a status of the alarm
//#define PM_ENABLE_LAN_BROADCAST

//Those settings control where to send lan broadcast, clients need to use the same value. Note IP specified here has nothing to do with ESP IP, it's only a destination for multicast
//Read more about milticast here: http://en.wikipedia.org/wiki/IP_multicast
IPAddress PM_LAN_BROADCAST_IP(224, 192, 32, 12);
#define   PM_LAN_BROADCAST_PORT  23127

//Specify your WIFI settings:
 
long prevMqtt = 0;        // will store last time LED was updated
long intervalMqtt = 60000;           // send data each xxx ms

WiFiClient wifiClientMQTT;
PubSubClient clientMqtt(wifiClientMQTT);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE  (50)
char msg[MSG_BUFFER_SIZE];
int value = 0;

int logcount = -1;
//flag for saving data
bool shouldSaveConfig = false;

char mqtt_server[40];
char mqtt_port[6] = "1883";
char mqtt_user[40];
char mqtt_pass[40];
char mqtt_client[40] = "EspPowerMax";

struct LogStruct
{
  unsigned long timeStamp;
  String Message;
} Logging[21];
//#define DEBUG_SERIAL true
 
void addLog(byte loglevel, String& string)
{
  addLog(loglevel, string.c_str());
}

void addLog(String& string)
{
  addLog( string.c_str());
}

void addLog(const char *line)
{
 
    logcount++;
    if (logcount > 20)
      logcount = 0;
    Logging[logcount].timeStamp = millis();
    Logging[logcount].Message = line;
 
}

void addLog(byte loglevel, const char *line)
{
 
    logcount++;
    if (logcount > 20)
      logcount = 0;
    Logging[logcount].timeStamp = millis();
    Logging[logcount].Message = line;
 
}
//callback notifying us of the need to save config
void saveConfigCallback () {
  addLog("Should save config");
  shouldSaveConfig = true;
}
//SoftwareSerial swSer;
//////////////////////////////////////////////////////////////////////////////////
//NOTE: PowerMaxAlarm class should contain ONLY functionality of Powerlink
//If you want to (for example) send an SMS on arm/disarm event, don't add it to PowerMaxAlarm
//Instead create a new class that inherits from PowerMaxAlarm, and override required function
class MyPowerMax : public PowerMaxAlarm
{
public:
    virtual void OnStatusChange(const PlinkBuffer  * Buff)
    {
        //call base class implementation first, this will send ACK back and upate internal state.
        PowerMaxAlarm::OnStatusChange(Buff);
        //addLog((char*)Buff->buffer[4]);
        //now our customization:
        switch(Buff->buffer[4])
        {
        case 0x51: //"Arm Home" 
            addLog("Event Alarm: Arm Home");
            break;
        case 0x53: //"Quick Arm Home"
            addLog("Event Alarm: Quick Arm Home");
            break;

        case 0x52: //"Arm Away"
            addLog("Event Alarm: Arm Away");
            break;
        case 0x54: //"Quick Arm Away"
            addLog("Event Alarm: Quick Arm Away");
            break;

        case 0x55: //"Disarm"
            addLog("Event Alarm: Disarm");
            break;
        default: //"Disarm"
            addLog("Default case");
            break;
        }        
    }


    virtual void OnSytemArmed(unsigned char armType, const char* armTypeStr, unsigned char whoArmed, const char* whoArmedStr){
      addLog("Armed by : "+(String)whoArmedStr); 
      publishSytemArmed(whoArmed, whoArmedStr,armType,armTypeStr); 
    }; 
    
    virtual void OnSytemDisarmed(unsigned char whoDisarmed, const char* whoDisarmedStr){ 
      addLog("Disarmed by : "+(String)whoDisarmedStr); 
      publishSytemDisarmed(whoDisarmed, whoDisarmedStr);
    };
    
    virtual void OnAlarmStarted(unsigned char alarmType, const char* alarmTypeStr, unsigned char zoneTripped, const char* zoneTrippedStr){
      addLog("System ALARM raised by : "+(String)zoneTrippedStr); 
      publishAlarmStarted(alarmType, alarmTypeStr, zoneTripped, zoneTrippedStr);
    };

    virtual void OnAlarmCancelled(unsigned char whoDisarmed, const char* whoDisarmedStr){
      addLog("Alarm called by : "+(String)whoDisarmedStr); 
      publishAlarmCancelled(whoDisarmed, whoDisarmedStr);
    };
    
    virtual void OnStatusUpdatePanel(const PlinkBuffer  * Buff)    
    {
        //call base class implementation first, this will send ACK back and upate internal state.
        PowerMaxAlarm::OnStatusUpdatePanel(Buff);
              
      if (this->isZoneEvent()) {
           const unsigned char zoneId = Buff->buffer[5];
           ZoneEvent eventType = (ZoneEvent)Buff->buffer[6];
           addLog("Zone: "+(String)zoneId+" Name:"+this->getZoneName(zoneId));
           publishAlarmZone(zoneId);
           
           
      }
    }
 
    
    SystemStatus getStat(){
     return PowerMaxAlarm::stat;  
    }
    int getPanelType(){
     return PowerMaxAlarm::m_iPanelType;  
    }
    int getModelType(){
     return PowerMaxAlarm::m_iModelType;  
    }    
    int getAlarmState(){
     return PowerMaxAlarm::alarmState;  
    }       
    unsigned char getAlarmFlags(){
     return PowerMaxAlarm::flags;  
    }

    int getAlarmTrippedZones(int i){
      return PowerMaxAlarm::alarmTrippedZones[i];  
    }

  
    bool isFlagSet(unsigned char id) const { return (flags & 1<<id) != 0; }

    Zone getAlarmZone(int i){
      return PowerMaxAlarm::zone[i];
    }
};
//////////////////////////////////////////////////////////////////////////////////

MyPowerMax pm;

int telnetDbgLevel = LOG_NO_FILTER; //by default only NO FILTER messages are logged to telnet clients 

#ifdef PM_ENABLE_LAN_BROADCAST
WiFiUDP udp;
unsigned int packetCnt = 0;
#endif

#ifdef PM_ENABLE_TELNET_ACCESS
WiFiServer telnetServer(23); //telnet server
WiFiClient telnetClient;
#endif

#define PRINTF_BUF 512 // define the tmp buffer size (change if desired)
void LOG(const char *format, ...)
{
  char buf[PRINTF_BUF];
  va_list ap;
  
  va_start(ap, format);
  vsnprintf(buf, sizeof(buf), format, ap);
#ifdef PM_ENABLE_TELNET_ACCESS  
  if(telnetClient.connected())
  {
    telnetClient.write((const uint8_t *)buf, strlen(buf));
  }
#endif  
  va_end(ap);
}

void handleDisarm() {
  String reply = "";
  addHeader(true, reply);
  pm.sendCommand(Pmax_DISARM);
  
  server->send(200, "text/html", reply);
}
void handleArmaway() {
  String reply = "";
  addHeader(true, reply);
  pm.sendCommand(Pmax_ARMAWAY);
  
  
  server->send(200, "text/html", reply);
}
void handleArmHome() {
  String reply = "";
  addHeader(true, reply);
  pm.sendCommand(Pmax_ARMHOME);
  
  
  server->send(200, "text/html", reply);
}
void handleRoot() {
  unsigned long days = 0, hours = 0, minutes = 0;
  unsigned long val = os_getCurrentTimeSec();
  
  days = val / (3600*24);
  val -= days * (3600*24);
  
  hours = val / 3600;
  val -= hours * 3600;
  
  minutes = val / 60;
  val -= minutes*60;
    String reply = "";
  addHeader(true, reply);
  char szTmp[PRINTF_BUF*2];
  /*
   * 
      json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
    json["mqtt_user"] = mqtt_user;
    json["mqtt_pass"] = mqtt_pass;
    json["mqtt_client"] = mqtt_client;
   * 
   * 
   */
  int enrolledZones = pm.getEnrolledZoneCnt();
  sprintf(szTmp, "Hello from esp powermax gateway.<br>Uptime: %02d:%02d:%02d.%02d<br>free heap: %u<br>Enrolled Zones: %03d<br>Mqtt Server: %s<br>Mqtt Port: %s<br>Mqtt User: %s<br>Mqtt Pass: *********<br>Mqtt ClientId: %s ",
                  (int)days, (int)hours, (int)minutes, (int)val, ESP.getFreeHeap(),enrolledZones,mqtt_server,mqtt_port,mqtt_user,mqtt_client);  
  reply+=szTmp;
  sprintf(szTmp,"<br>MQTT_MAX_PACKET_SIZE: %02d (Need more of 256)",(int)clientMqtt.getBufferSize());
  reply+=szTmp;
  reply+="</html>";
  
  //MQTT_MAX_PACKET_SIZE
  server->send(200, "text/html", reply);
}
void handleLog() {
 // if (!isLoggedIn()) return;

  char *TempString = (char*)malloc(80);

  String reply = "";
  addHeader(true, reply);
  reply += F("<script language='JavaScript'>function RefreshMe(){window.location = window.location}setTimeout('RefreshMe()', 5000);</script>");
  reply += F("<div class=\"table-responsive\"><table id=\"log\" class=\"table curTx table-condensed table-striped table-hover\">");
  reply += F("<thead><tr><th>Time</th><th>entrie</th></tr></thead><tbody>");

  if (logcount != -1)
  {
    byte counter = logcount;
    do
    {
      counter++;
      if (counter > 20)
        counter = 0;
      if (Logging[counter].timeStamp > 0)
      {
        reply += "<tr><td>";
        reply += Logging[counter].timeStamp;
        reply += "</td><td>";
        reply += Logging[counter].Message;
        reply += F("</td></tr>");
      }
    }  while (counter != logcount);
  }
  reply += F(" </tbody></table></div>");
  
  server->send(200, "text/html", reply);
  free(TempString);
}  
void addHeader(boolean showMenu, String& str)
{
  str += F("<script language=\"javascript\"><!--\n");
  str += F("function dept_onchange(frmselect) {frmselect.submit();}\n");
  str += F("//--></script>");
  str += F("<head><title>ESPPowerMax mqtt Gateway</title>");
    str += F("<style>");
    str += F("* {font-family:sans-serif; font-size:12pt;}");
    str += F("h1 {font-size:16pt; color:black;}");
    str += F("h6 {font-size:10pt; color:black;}");
    str += F(".button-menu {background-color:#ffffff; color:blue; margin: 10px; text-decoration:none}");
    str += F(".button-link {padding:5px 15px; background-color:#0077dd; color:#fff; border:solid 1px #fff; text-decoration:none}");
    str += F(".button-menu:hover {background:#ddddff;}");
    str += F(".button-link:hover {background:#369;}");
    str += F("th {padding:10px; background-color:black; color:#ffffff;}");
    str += F("td {padding:7px;}");
    str += F("table {color:black;}");
    str += F(".div_l {float: left;}");
    str += F(".div_r {float: right; margin: 2px; padding: 1px 10px; border-radius: 7px; background-color:#080; color:white;}");
    str += F(".div_br {clear: both;}");
    str += F("</style>");
   str += F("<meta charset=\"utf-8\">");
   str += F("<script src=\"https://ajax.googleapis.com/ajax/libs/jquery/1.12.3/jquery.min.js\"></script>");
   str += F("<link rel=\"stylesheet\" href=\"https://maxcdn.bootstrapcdn.com/bootstrap/latest/css/bootstrap.min.css\">");
   str += F("<link rel=\"stylesheet\" href=\"https://maxcdn.bootstrapcdn.com/bootstrap/latest/css/bootstrap-theme.min.css\">");
   str += F("<script src=\"https://maxcdn.bootstrapcdn.com/bootstrap/latest/js/bootstrap.min.js\"></script>");
   str += F("<link rel=\"stylesheet\" href=\"https://cdn.datatables.net/1.10.12/css/jquery.dataTables.min.css\">");
   str += F("<script type=\"text/javascript\" src=\"https://cdn.datatables.net/1.10.12/js/jquery.dataTables.min.js\"></script>");
        
    str += F("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
  str += F("</head>");
  if (showMenu)
  {
    //str += F("<BR><a class=\"button-menu\" href=\".\">Main</a>");
    //str += F("<a class=\"button-menu\" href=\"config\">Config</a>");
    //str += F("<a class=\"button-menu\" href=\"hardware\">Hardware</a>");
    //str += F("<a class=\"button-menu\" href=\"devices\">Devices</a>");
    //str += F("<a class=\"button-menu\" href=\"log\">Log</a>");
    //str += F("<a class=\"button-menu\" href=\"reboot\">Reboot</a><BR><BR>");
    str += F("<button onclick=\"window.location.href='./'\"  type=\"button\" class=\"btn btn-default navbar-btn\"><span class=\"glyphicon glyphicon-folder-open\" aria-hidden=\"true\"></span>&nbsp;Main</button>");
    str += F("<button onclick=\"window.location.href='./status'\"  type=\"button\" class=\"btn btn-default navbar-btn\"><span class=\"glyphicon glyphicon-refresh\" aria-hidden=\"true\"></span>&nbsp;Status</button>");
    str += F("<button onclick=\"window.location.href='./log'\"  type=\"button\" class=\"btn btn-default navbar-btn\"><span class=\"glyphicon glyphicon-refresh\" aria-hidden=\"true\"></span>&nbsp;Log</button>");



    str += F("<button onclick=\"window.location.href='./armaway'\"  type=\"button\" class=\"btn btn-danger navbar-btn\"><span class=\"glyphicon glyphicon-refresh\" aria-hidden=\"true\"></span>&nbsp;ARM Away</button>");
    str += F("<button onclick=\"window.location.href='./armhome'\"  type=\"button\" class=\"btn btn-warning navbar-btn\"><span class=\"glyphicon glyphicon-refresh\" aria-hidden=\"true\"></span>&nbsp;ARM Home</button>");
    str += F("<button onclick=\"window.location.href='./disarm'\"  type=\"button\" class=\"btn btn-success navbar-btn\"><span class=\"glyphicon glyphicon-refresh\" aria-hidden=\"true\"></span>&nbsp;DISARM</button>");

    
    str += F("<button onclick=\"window.location.href='./reboot'\"  type=\"button\" class=\"btn btn-default navbar-btn\"><span class=\"glyphicon glyphicon-repeat\" aria-hidden=\"true\"></span>&nbsp;Reboot</button><BR><BR>");  
  }  
  
}
//writes to webpage without storing large buffers, only small buffer is used to improve performance
class WebOutput : public IOutput
{
  WiFiClient* c;
  char buffer[PRINTF_BUF+1];
  int bufferLen;
      
    public:
      WebOutput(WiFiClient* a_c)
      {
        c = a_c;
        bufferLen = 0;
        memset(buffer, 0, sizeof(buffer));
      }
      
      void write(const char* str)
      {
        int len = strlen(str);
        if(len < (PRINTF_BUF/2))
        {
          if(bufferLen+len < PRINTF_BUF)
          {
            strcat(buffer, str);
            bufferLen += len;
            return;
          }
        }
        
        flush();
        c->write(str, len);
            
        yield();
      }
    
      void flush()
      {
        if(bufferLen)
        {
          c->write((const uint8_t *)buffer, bufferLen);
          buffer[0] = 0;
          bufferLen = 0;
        }   
      }
};

 

void handleStatus() {
  
  WiFiClient client = server->client();
  
  client.print("HTTP/1.1 200 OK\r\n");
  client.print("Content-Type: text/plain\r\n");
  client.print("Connection: close\r\n");
  client.print("\r\n");

  WebOutput out(&client);
  pm.dumpToJson(&out);
  out.flush();

  client.stop();
}

 
void handleReboot() {
  ESP.restart(); 
}




void handleNotFound(){
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server->uri();
  message += "\nMethod: ";
  message += (server->method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server->args();
  message += "\n";
  for (uint8_t i=0; i<server->args(); i++){
    message += " " + server->argName(i) + ": " + server->arg(i) + "\n";
  }
  server->send(404, "text/plain", message);
}

void setup(void){

  Serial.begin(9600);
// checkFS 
   createFS();
   wifiManagerSetup();
  //swSer.begin(115200, SWSERIAL_8N1, D5, D6, false, 95, 11);
  //swSer.begin(9600); //connect to PowerMax
  //Serial.begin(115200);
  //Serial.println();
  //Serial.println("ESP PowerMax alarm controll");
  //Serial.println((String)"Core    : "+ESP.getCoreVersion());
  //Serial.println((String)"SDK     : "+ESP.getSdkVersion());
  //Serial.println((String)"WIFI ESP: "+WiFi.macAddress());
  //Serial.println("Reset Reason: " + String(ESP.getResetReason()));
  //Serial.println();

   
  //Serial.print("Connected, IP address: ");
  //Serial.println(WiFi.localIP());
  
  server->on("/", handleRoot);
  server->on("/status", handleStatus);
  server->on("/reboot", handleReboot);


  server->on("/armaway", handleArmaway);
  server->on("/armhome", handleArmHome);
  server->on("/disarm", handleDisarm);

  
  server->on("/log",    handleLog);
  server->on("/inline", [](){
    server->send(200, "text/plain", "this works as well");
  });

  server->onNotFound(handleNotFound);
  server->begin();

#ifdef PM_ENABLE_OTA_UPDATES
  ArduinoOTA.begin();
#endif

#ifdef PM_ENABLE_TELNET_ACCESS
  telnetServer.begin();
  telnetServer.setNoDelay(true);
#endif

  //if you have a fast board (like PowerMax Complete) you can pass 0 to init function like this: pm.init(0);
  //this will speed up the boot process, keep it as it is, if you have issues downloading the settings from the board.
  pm.init();
  //clientMqtt.EspMQTTClient(NULL, NULL, "mqttServerIp", NULL, NULL, "mqttClientName", "mqttServerPort");
 
 
connectMqtt();

    // Optionnal functionnalities of EspMQTTClient : 
//#ifdef DEBUG_SERIAL
//  clientMqtt.enableDebuggingMessages(); // Enable debugging messages sent to serial output
//#endif
  //clientMqtt.enableHTTPWebUpdater(); // Enable the web updater. User and password default to values of MQTTUsername and MQTTPassword. These can be overrited with enableHTTPWebUpdater("user", "password").
  //clientMqtt.enableLastWillMessage("TestClient/lastwill", "I am going offline");  // You can activate the retain flag by setting the third parameter to true
  
}

// This function is called once everything is connected (Wifi and MQTT)
// WARNING : YOU MUST IMPLEMENT IT IF YOU USE EspMQTTClient
void onConnectionEstablished()
{
  
       /*
  // Subscribe to "powermax/test" and display received message to Serial
  clientMqtt.subscribe("powermax/command", [](const String & payload) {
#ifdef DEBUG_SERIAL
    Serial.println("Recibo: "+(String)payload);
#endif
#ifdef PM_ALLOW_CONTROL
    if ( (String)payload == "ARMHOME" ) {
      DEBUG(LOG_NOTICE,"Arming home");
      addLog("MQTT Command: ARMHOME");
      pm.sendCommand(Pmax_ARMHOME);
    }
    else if ( (String)payload == "DISARM" ) {
      DEBUG(LOG_NOTICE,"Disarm");
      addLog("MQTT Command: DISARM");
      pm.sendCommand(Pmax_DISARM);
    }  
    else if ( (String)payload == "ARMAWAY" ) {
      DEBUG(LOG_NOTICE,"Arming away");
      addLog("MQTT Command: ARMAWAY");
      pm.sendCommand(Pmax_ARMAWAY);
    }else if ( (String)payload == "REBOOT" ) {
      addLog("MQTT Command: REBOOT"); //never can see it
      ESP.restart();
    }
#endif
  });

  // Subscribe to "powermax/wildcardtest/#" and display received message to Serial
  clientMqtt.subscribe("powermax/in/#", [](const String & topic, const String & payload) {
    
  });

  // Publish a message to "powermax/test"
  clientMqtt.publish("powermax/init", "Init process....."); // You can activate the retain flag by setting the third parameter to true

  // Execute delayed instructions
 // clientMqtt.executeDelayed(5 * 1000, []() {
 //      clientMqtt.publish("powermax/test", "This is a message sent 5 seconds later");
 //  });
*/

 
}

#ifdef PM_ENABLE_TELNET_ACCESS
void handleNewTelnetClients()
{
  if(telnetServer.hasClient())
  {
    if(telnetClient.connected())
    {
      //no free/disconnected spot so reject
      WiFiClient newClient = telnetServer.available();
      newClient.stop();
    }
    else
    {
      telnetClient = telnetServer.available();
      LOG("Connected to %s, type '?' for help.\r\n", WIFI_SSID); 
      
    }
  }
}

void runDirectRelayLoop()
{
  while(telnetClient.connected())
  { 
    bool wait = true;
    
    //we want to read/write in bulk as it's much faster than read one byte -> write one byte (this is known to create problems with PowerMax)
    unsigned char buffer[256];
    int readCnt = 0;
    while(readCnt < 256)
    {
      if(Serial.available())
      {
        buffer[readCnt] = Serial.read();
        readCnt++;
        wait = false;
      }
      else
      {
        break;
      }
    }
    
    if(readCnt > 0)
    {
      telnetClient.write((const uint8_t *)buffer, readCnt );
    }

    if(telnetClient.available())
    {
      Serial.write( telnetClient.read() );
      wait = false;
    }

    if(wait)
    {
      delay(10);
    }
  } 
}

void handleTelnetRequests(PowerMaxAlarm* pm) {
  char c; 
  if ( telnetClient.connected() && telnetClient.available() )  {
    c = telnetClient.read();

    if(pm->isConfigParsed() == false &&
       (c == 'h' || //arm home
        c == 'a' || //arm away
        c == 'd'))  //disarm
    {
        //DEBUG(LOG_NOTICE,"EPROM is not download yet (no PIN requred to perform this operation)");
        return;
    }

#ifdef PM_ALLOW_CONTROL
    if ( c == 'h' ) {
      DEBUG(LOG_NOTICE,"Arming home");
      pm->sendCommand(Pmax_ARMHOME);
    }
    else if ( c == 'd' ) {
      DEBUG(LOG_NOTICE,"Disarm");
      pm->sendCommand(Pmax_DISARM);
    }  
    else if ( c == 'a' ) {
      DEBUG(LOG_NOTICE,"Arming away");
      pm->sendCommand(Pmax_ARMAWAY);
    }
    else if ( c == 'D' ) {
      DEBUG(LOG_NO_FILTER,"Direct relay enabled, disconnect to stop...");
      runDirectRelayLoop();
    }
#endif
 
    if ( c == 'g' ) {
      DEBUG(LOG_NOTICE,"Get Event log");
      pm->sendCommand(Pmax_GETEVENTLOG);
    }
    else if ( c == 't' ) {
      DEBUG(LOG_NOTICE,"Restore comms");
      pm->sendCommand(Pmax_RESTORE);
    }
    else if ( c == 'v' ) {
      DEBUG(LOG_NOTICE,"Exit Download mode");
      pm->sendCommand(Pmax_DL_EXIT);
    }
    else if ( c == 'r' ) {
      DEBUG(LOG_NOTICE,"Request Status Update");
      pm->sendCommand(Pmax_REQSTATUS);
    }
    else if ( c == 'j' ) {
        ConsoleOutput out;
        pm->dumpToJson(&out);
    }
    else if ( c == 'c' ) {
        DEBUG(LOG_NOTICE,"Exiting...");
        telnetClient.stop();
    }    
    else if( c == 'C' ) {
      DEBUG(LOG_NOTICE,"Reseting...");
      ESP.reset();
    }    
    else if ( c == 'p' ) {
      telnetDbgLevel = LOG_DEBUG;
      DEBUG(LOG_NOTICE,"Debug Logs enabled type 'P' (capital) to disable");
    }    
    else if ( c == 'P' ) {
      telnetDbgLevel = LOG_NO_FILTER;
      DEBUG(LOG_NO_FILTER,"Debug Logs disabled");
    }       
    else if ( c == 'H' ) {
      DEBUG(LOG_NO_FILTER,"Free Heap: %u", ESP.getFreeHeap());
    }  
    else if ( c == '?' )
    {
        DEBUG(LOG_NO_FILTER,"Allowed commands:");
        DEBUG(LOG_NO_FILTER,"\t c - exit");
        DEBUG(LOG_NO_FILTER,"\t C - reset device");        
        DEBUG(LOG_NO_FILTER,"\t p - output debug messages");
        DEBUG(LOG_NO_FILTER,"\t P - stop outputing debug messages");
#ifdef PM_ALLOW_CONTROL        
        DEBUG(LOG_NO_FILTER,"\t h - Arm Home");
        DEBUG(LOG_NO_FILTER,"\t d - Disarm");
        DEBUG(LOG_NO_FILTER,"\t a - Arm Away");
        DEBUG(LOG_NO_FILTER,"\t D - Direct mode (relay all bytes from client to PMC and back with no processing, close connection to exit");  
#endif        
        DEBUG(LOG_NO_FILTER,"\t g - Get Event Log");
        DEBUG(LOG_NO_FILTER,"\t t - Restore Comms");
        DEBUG(LOG_NO_FILTER,"\t v - Exit download mode");
        DEBUG(LOG_NO_FILTER,"\t r - Request Status Update");
        DEBUG(LOG_NO_FILTER,"\t j - Dump Application Status to JSON");  
        DEBUG(LOG_NO_FILTER,"\t H - Get free heap");  

        
    }
  }
}
#endif

#ifdef PM_ENABLE_LAN_BROADCAST
void broadcastPacketOnLan(const PlinkBuffer* commandBuffer, bool packetOk)
{
  udp.beginPacketMulticast(PM_LAN_BROADCAST_IP, PM_LAN_BROADCAST_PORT, WiFi.localIP());
  udp.write("{ data: [");
  for(int ix=0; ix<commandBuffer->size; ix++)
  {
    char szDigit[20];
    //sprintf(szDigit, "%d", commandBuffer->buffer[ix]);
    sprintf(szDigit, "%02x", commandBuffer->buffer[ix]); //IZIZTODO: temp

    if(ix+1 != commandBuffer->size)
    {
      strcat(szDigit, ", ");
    }

    udp.write(szDigit);
  }
  
  udp.write("], ok: ");
  if(packetOk)
  {
    udp.write("true");
  }
  else
  {
    udp.write("false");
  }

  char szTmp[50];
  sprintf(szTmp, ", seq: %u }", packetCnt++);
  udp.write(szTmp);
  
  udp.endPacket();
}
#endif

bool serialHandler(PowerMaxAlarm* pm) {

  bool packetHandled = false;
  
  PlinkBuffer commandBuffer ;
  memset(&commandBuffer, 0, sizeof(commandBuffer));
  
  char oneByte = 0;  
  while (  (os_pmComPortRead(&oneByte, 1) == 1)  ) 
  {     
    if (commandBuffer.size<(MAX_BUFFER_SIZE-1))
    {
      *(commandBuffer.size+commandBuffer.buffer) = oneByte;
      commandBuffer.size++;
    
      if(oneByte == 0x0A) //postamble received, let's see if we have full message
      {
        if(PowerMaxAlarm::isBufferOK(&commandBuffer))
        {
          DEBUG(LOG_INFO,"--- new packet %d ----", millis());

#ifdef PM_ENABLE_LAN_BROADCAST
          broadcastPacketOnLan(&commandBuffer, true);
#endif

          packetHandled = true;
          pm->handlePacket(&commandBuffer);
          commandBuffer.size = 0;
          break;
        }
      }
    }
    else
    {
      DEBUG(LOG_WARNING,"Packet too big detected");
    }
  }

  if(commandBuffer.size > 0)
  {
#ifdef PM_ENABLE_LAN_BROADCAST
    broadcastPacketOnLan(&commandBuffer, false);
#endif
    
    packetHandled = true;
    //this will be an invalid packet:
    DEBUG(LOG_WARNING,"Passing invalid packet to packetManager");
    pm->handlePacket(&commandBuffer);
  }

  return packetHandled;
}

void loop(void){
#ifdef PM_ENABLE_OTA_UPDATES
  ArduinoOTA.handle();
#endif

  server->handleClient();

  static unsigned long lastMsg = 0;
  if(serialHandler(&pm) == true)
  {
    lastMsg = millis();
  }

  if(millis() - lastMsg > 300 || millis() < lastMsg) //we ensure a small delay between commands, as it can confuse the alarm (it has a slow CPU)
  {
    pm.sendNextCommand();
  }

  if(pm.restoreCommsIfLost()) //if we fail to get PINGs from the alarm - we will attempt to restore the connection
  {
    DEBUG(LOG_WARNING,"Connection lost. Sending RESTORE request.");  
    addLog( "Connection lost. Sending RESTORE request.");  
    //Serial.println("Connection lost. Sending RESTORE request.");   
  }   

#ifdef PM_ENABLE_TELNET_ACCESS
  handleNewTelnetClients();
  handleTelnetRequests(&pm);
#endif  
      loopMQTT();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//This file contains OS specific implementation for ESP8266 used by PowerMax library
//If you build for other platrorms (like Linux or Windows, don't include this file, but provide your own)

int log_console_setlogmask(int mask)
{
  int oldmask = telnetDbgLevel;
  if(mask == 0)
    return oldmask; /* POSIX definition for 0 mask */
  telnetDbgLevel = mask;
  return oldmask;
} 

void os_debugLog(int priority, bool raw, const char *function, int line, const char *format, ...)
{
  if(priority <= telnetDbgLevel)
  {
    char buf[PRINTF_BUF];
    va_list ap;
    
    va_start(ap, format);
    vsnprintf(buf, sizeof(buf), format, ap);
    
  #ifdef PM_ENABLE_TELNET_ACCESS
    if(telnetClient.connected())
    { 
      telnetClient.write((const uint8_t *)buf, strlen(buf));
      if(raw == false)
      {
        telnetClient.write((const uint8_t *)"\r\n", 2);
      }    
    }
  #endif
    
    va_end(ap);
  
    yield();
    }


}


void os_usleep(int microseconds)
{
    delay(microseconds / 1000);
}

unsigned long os_getCurrentTimeSec()
{
  static unsigned int wrapCnt = 0;
  static unsigned long lastVal = 0;
  unsigned long currentVal = millis();

  if(currentVal < lastVal)
  {
    wrapCnt++;
  }

  lastVal = currentVal;
  unsigned long seconds = currentVal/1000;
  
  //millis will wrap each 50 days, as we are interested only in seconds, let's keep the wrap counter
  return (wrapCnt*4294967) + seconds;
}

int os_pmComPortRead(void* readBuff, int bytesToRead)
{
    int dwTotalRead = 0;
    while(bytesToRead > 0)
    {
        for(int ix=0; ix<10; ix++)
        {
          if(Serial.available())
          {
            break;
          }
          delay(5);
        }
        
        if(Serial.available() == false)
        {
            break;
        }

        *((char*)readBuff) = Serial.read();
        dwTotalRead ++;
        readBuff = ((char*)readBuff) + 1;
        bytesToRead--;
    }

    return dwTotalRead;
}

int os_pmComPortWrite(const void* dataToWrite, int bytesToWrite)
{
    Serial.write((const uint8_t*)dataToWrite, bytesToWrite);
    return bytesToWrite;
}

bool os_pmComPortClose()
{
    return true;
}

bool os_pmComPortInit(const char* portName) {
    return true;
} 

void os_strncat_s(char* dst, int dst_size, const char* src)
{
    strncat(dst, src, dst_size);
}

int os_cfg_getPacketTimeout()
{
    return PACKET_TIMEOUT_DEFINED;
}

//see PowerMaxAlarm::setDateTime for details of the parameters, if your OS does not have a RTC clock, simply return false
bool os_getLocalTime(unsigned char& year, unsigned char& month, unsigned char& day, unsigned char& hour, unsigned char& minutes, unsigned char& seconds)
{
    return false; //IZIZTODO
}



void loopMQTT(){
 
  if (!clientMqtt.connected()) {
    connectMqtt();

    if (clientMqtt.connect(mqtt_client)) {
       addLog("Mqtt connected");
    } else {
      addLog("Mqtt failed connection"+(String)clientMqtt.state());
    }
    
    }
 
  clientMqtt.loop();
 unsigned long currentMillis = millis();

 //clientMqtt.loop();//mqttLoop
 
    if(currentMillis - prevMqtt > intervalMqtt and clientMqtt.connected()) {
   
     prevMqtt = currentMillis;   
     publishAlarmStat();     
     publishAlarmFlags();  
     publishAlarmZones();
    }

        
}
