# PowerMaxEsp8266
PowerMax gateway ESP8266

Thank to irekzielinski to make the library -> https://github.com/irekzielinski/PowerMaxAlarm

This sketch provide mqtt comunication to Visonic PowerMax Alarms

## OUTPUT TOPICS
# powermax/stat
Send info about alarm status (sending info each minute)

Example

* {"stat":"0","stat_str":"Disarmed","lastCom":0,"panelType":"2","panelTypeStr":"PowerMax Pro","panelModelType":"5","alarmState":"0","alarmStateStr":"None"}
* {"stat":"1","stat_str":"Exit Delay","lastCom":3,"panelType":"2","panelTypeStr":"PowerMax Pro","panelModelType":"5","alarmState":"0","alarmStateStr":"None"}
# powermax/flags
Send info about alarm status (sending info each minute)
* {"flags":"39","flags_ready":true,"flags_alertInMemory":true,"flags_trouble":true,"flags_bypasOn":false,"flags_last10sec":false,"flags_zoneEvent":true,"flags_armDisarmEvent":false,"flags_alarmEvent":false}

* {"flags":"69","flags_ready":true,"flags_alertInMemory":false,"flags_trouble":true,"flags_bypasOn":false,"flags_last10sec":false,"flags_zoneEvent":false,"flags_armDisarmEvent":true,"flags_alarmEvent":false}

# powermax/zone
Send one topic for each sensor (sending info each minute or when event occurs)

* {"zoneId":1,"zoneName":"Puerta principal","zoneType":4,"zoneTypeStr":"Delay 1","sensorId":149,"sensorType":"Magnet","sensorMake":"Visonic Door/Window Contact","enrolled":true,"signalStrength":0,"lastEvent":false,"lastEventTime":0,"stat_doorOpen":false,"stat_bypased":false,"stat_lowBattery":false}
* {"zoneId":1,"zoneName":"Puerta principal","zoneType":4,"zoneTypeStr":"Delay 1","sensorId":149,"sensorType":"Magnet","sensorMake":"Visonic Door/Window Contact","enrolled":true,"signalStrength":0,"lastEvent":true,"lastEventTime":55622,"stat_doorOpen":true,"stat_bypased":false,"stat_lowBattery":false} 
# powermax/system
Send info about alarm event raised (armed, disarmed, raise alamr, who disarm, who arm..... sending in realtime event occurs)
* {"event":"ARMED","whoArmed":0,"whoArmedStr":"System","armType":83,"armTypeStr":"Quick Arm Home"}
* {"event":"DISARMED","whoDisarmed":39,"whoDisarmedStr":"User 1"}

## INPUT TOPICS
# powermax/command
Messages:
* ARMHOME -> Raise arm home command into Powermax pannel
* DISARM -> Raise disarm command into Powermax pannel
* ARMAWAY -> Raise armaway command into Powermax pannel 
* REBOOT -> Reboot ESP8266
