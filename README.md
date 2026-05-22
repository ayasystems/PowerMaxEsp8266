# PowerMaxEsp8266
PowerMax gateway ESP8266

![image](https://github.com/ayasystems/PowerMaxEsp8266/assets/7864168/33828593-8d9b-4c4f-94fd-ac02a5eb6fa0)



Thank to irekzielinski to make the library -> https://github.com/irekzielinski/PowerMaxAlarm

This sketch provide mqtt comunication to Visonic PowerMax Alarms

Added WifiManager to setup wifi and mqtt connection. MQTT credentials can also be changed at any time via the web UI at `http://<ESP-IP>/mqttconfig` without reflashing.

Supports **Home Assistant MQTT Auto-Discovery**: alarm panel and all enrolled sensors appear automatically in HA once the panel config is downloaded (~30 s after boot).

Into folder libraries you can find the used libraries to compile


## POWERMAX PINOUT
![Web](https://github.com/ayasystems/PowerMaxEsp8266/raw/master/powermax_pinout.JPG)
## OUTPUT TOPICS
# powermax/stat
Send info about alarm status (sending info each minute or when event occurs)

Example

* {"stat":"0","stat_str":"Disarmed","lastCom":0,"panelType":"2","panelTypeStr":"PowerMax Pro","panelModelType":"5","alarmState":"0","alarmStateStr":"None","uptime":3600,"freeHeap":28000,"wifiRSSI":-65}
* {"stat":"1","stat_str":"Exit Delay","lastCom":3,"panelType":"2","panelTypeStr":"PowerMax Pro","panelModelType":"5","alarmState":"0","alarmStateStr":"None","uptime":3600,"freeHeap":28000,"wifiRSSI":-65}
# powermax/flags
Send info about alarm status (sending info each minute or when event occurs)
* {"flags":"39","flags_ready":true,"flags_alertInMemory":true,"flags_trouble":true,"flags_bypasOn":false,"flags_last10sec":false,"flags_zoneEvent":true,"flags_armDisarmEvent":false,"flags_alarmEvent":false}

* {"flags":"69","flags_ready":true,"flags_alertInMemory":false,"flags_trouble":true,"flags_bypasOn":false,"flags_last10sec":false,"flags_zoneEvent":false,"flags_armDisarmEvent":true,"flags_alarmEvent":false}

# powermax/zone
Send one topic for each sensor (sending info each minute or when event occurs)

* {"zoneId":1,"zoneName":"Puerta principal","zoneType":4,"zoneTypeStr":"Delay 1","sensorId":149,"sensorType":"Magnet","sensorMake":"Visonic Door/Window Contact","enrolled":true,"signalStrength":0,"lastEvent":false,"lastEventTime":0,"stat_doorOpen":false,"stat_bypased":false,"stat_lowBattery":false}
* {"zoneId":1,"zoneName":"Puerta principal","zoneType":4,"zoneTypeStr":"Delay 1","sensorId":149,"sensorType":"Magnet","sensorMake":"Visonic Door/Window Contact","enrolled":true,"signalStrength":0,"lastEvent":true,"lastEventTime":55622,"stat_doorOpen":true,"stat_bypased":false,"stat_lowBattery":false} 

# powermax/zone\<N\>
Same payload as powermax/zone but published individually per zone (retained). Used by Home Assistant auto-discovery.

# powermax/alarm_state
Home Assistant compatible alarm state string (retained).

* disarmed / armed_home / armed_away / arming / pending / triggered

# powermax/system
Send info about alarm event raised (armed, disarmed, raise alamr, who disarm, who arm..... sending in realtime event occurs)
* {"event":"ARMED","whoArmed":0,"whoArmedStr":"System","armType":83,"armTypeStr":"Quick Arm Home"}
* {"event":"DISARMED","whoDisarmed":39,"whoDisarmedStr":"User 1"}

# powermax/command_ack
Acknowledgement published after each received command.

# powermax/availability
LWT topic: `online` when ESP is connected, `offline` when it disconnects. Used by Home Assistant to mark entities unavailable.

## INPUT TOPICS
# powermax/command
Messages:
* ARMHOME -> Raise arm home command into Powermax pannel
* DISARM -> Raise disarm command into Powermax pannel
* ARMAWAY -> Raise armaway command into Powermax pannel 
* REBOOT -> Reboot ESP8266
* STATUS -> Force immediate publish of all status topics

## Compilation

To import .zip libraries, in Arduino IDE, use *Sketch->Include Library->Add Zip Library*
