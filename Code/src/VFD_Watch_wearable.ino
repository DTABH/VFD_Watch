#include <Arduino.h>
#include <Vfd_Display.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <WiFiUdp.h>
#include "ManualTimeSet.h"
#include "RTClib.h"
#include "WiFiManager.h"
#include <esp_task_wdt.h>
#include <Preferences.h> // Keynames may not be longer than 15 characters

#define FW_VERSION "2.0.0"

WiFiUDP udp;

TaskHandle_t vfdTask;
vfdDisplay vfd;
#include "Animations.h" // must be placed after vfdDisplay vfd;

int seconds = 0;
int minutes = 0;
struct tm local;

Ticker updateTime;

bool RTC_Exists;
DateTime RTC_now;
RTC_DS3231 rtc;

int timeoutConfig = 120; // seconds to run for configuration AP
char* ntpserver = "de.pool.ntp.org";
char* timezone = "CET-1CEST,M3.5.0,M10.5.0/3"; // Berlin

String info ="Info ";
int RTCOnly;
Preferences preferences;
int ShowDate;

void initialBoot(){
  Serial.begin(115200);
  Serial.println("InitialBoot");
      
  char buffer[40];
  time_t now;
  struct tm timeinfo;
  bool wifiConnected;

  // For RTC Begin I2C communication on the second i2c bus
  // Default I2C-Bus is used for driving the multiplexer. driver and vfd 
  Wire1.setPins(27, 14); 
  Wire1.begin();
  if ( rtc.begin(&Wire1)) 
  {
    RTC_Exists = true; // RTC wxists and works
    RTC_now = rtc.now();
    Serial.println(" RTC found unixtime since 1/1/1970: " + String(RTC_now.unixtime() )); 
    info = info + " RTC Exists unixtime: " + String(RTC_now.unixtime());
    // RTCOnly lesen
    preferences.begin("Clock", false);
    RTCOnly = preferences.getUInt("RTCOnly", 0);
    timezone = strdup(preferences.getString("timezone").c_str());
    preferences.end();
    setenv("TZ", timezone, 1); // reset timezone
    tzset(); 
  }
  else
  {
    Serial.println("Couldn't find RTC");
    info = info + " No RTC found ";
  } 

  Serial.println("Set mode WIFI_STA");

  if(RTCOnly)  
  {   
    RTC_now = rtc.now();
    long unsigned int unixtime = RTC_now.unixtime();
    sntp_set_system_time(unixtime,0);
    Serial.println("RTCOnly Use RTC time : " + String(unixtime));
    info = info + " RTCOnly Use RTC time : " + String(unixtime);
  }
  else
  {
    wifiConnected = callWifiManagerConfig(0);

    if(!wifiConnected) {
      Serial.println("Failed to connect");
      if (!RTC_Exists)
      {
        startManualTimeSet(vfd, ShowDate); 
        WiFi.mode( WIFI_OFF );        
        return;
      }
    }  

    if (wifiConnected)
    {
      IPAddress  myip = WiFi.localIP();      
      Serial.println(WiFi.macAddress());
      info = info + " WiFi connected IP: " + String(myip[0]) + "." + myip[1] + "." + myip[2] + "." + myip[3] + " macAddress: " + WiFi.macAddress();
      // Set time via NTP
      delay(2000);
      configTime(0, 0,ntpserver);  // 0, 0 because we will use TZ in the next line
      getLocalTime(&local, 10000);  // get time for 10s  
      setenv("TZ", timezone, 1); // reset timezone
      tzset(); 
      // Read time back
      time(&now);
      localtime_r(&now, &timeinfo);   
    
      // NTP Server not reached 
      // So check and use local gateway if timeserver is activated for example on a fritzbox
      // this gets the HTP Time even the device has no access to internet due to restrictions
      char* gatewayIp = strdup(String(WiFi.gatewayIP()).c_str());
      if (timeinfo.tm_year < 124 && gatewayIp != ntpserver) //  124 for 2024    1900 unixyear + 124 = 2024 !!
      {        
        Serial.println(" NTPServer " + String(ntpserver) + " not reached try Gateway as times server " + gatewayIp);
        configTime(0, 0,gatewayIp);  // 0, 0 because we will use TZ in the next line
        getLocalTime(&local, 10000);  // get time for 10s  
        setenv("TZ", timezone, 1); // reset timezone
        tzset(); 
        // Read time back
        time(&now);
        localtime_r(&now, &timeinfo);  
        if (timeinfo.tm_year < 124) //  124 for 2024    1900 unixyear + 124 = 2024 !!
        {     
          info = info + " Both NTPServer: " + String(ntpserver) + " and via Gateway : " + String(gatewayIp)  + "  failed. year: " + String(timeinfo.tm_year + 1900) ;
        }
        else
        {     
          info = info + " NTPServer: " + String(ntpserver) + " failed but Found  via Gateway : " + String(gatewayIp) + " timezone: " + String(timezone)  + " year: " + String(timeinfo.tm_year+ 1900) ;
        }
      }
      else
      {
        info = info + " Found NTPServer: " + String(ntpserver) + " timezone: " + String(timezone) + " year: " + String(timeinfo.tm_year+ 1900);
      }

      WiFi.mode( WIFI_OFF );
    }
  }

  if (RTC_Exists)
  {
     Serial.println(" RTC_Exists");
    // Time is too old  NTP Time not get
    // Use RTC
    unsigned long unixtime;
    if (timeinfo.tm_year < 124) //  124 for 2024 !!
    {
      Serial.println("NTP time not available! ");
      RTC_now = rtc.now();
      unixtime = RTC_now.unixtime();
      sntp_set_system_time(unixtime,0);
      Serial.println("Use RTC time instead of NTP : " + String(unixtime));
      info = info + " Use RTC time instead of NTP : " + String(unixtime);
    }
    else
    {
      // Set RTC
      unixtime= getTime();
      Serial.println("Set RTC time " + String(unixtime));
      rtc.adjust(DateTime(unixtime));  // Set RTC time using NTP epoch time
      info = info + " RTC time " + String(unixtime) + " set by NTPserver.";
    }
  }

  time(&now);
  localtime_r(&now, &timeinfo);
  sprintf(buffer,"%04d.%02d.%02d %02d:%02d:%02d",timeinfo.tm_year + 1900,timeinfo.tm_mon + 1,timeinfo.tm_mday,timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec);
  Serial.println(buffer);

  // check time must be > 2024 (124) or ntpserver not reached
  if (timeinfo.tm_year < 124)
  {
    Serial.println("Call startManualTimeSet ");
    info = info + " Call startManualTimeSet ";
    startManualTimeSet(vfd, ShowDate); //
    if (RTC_Exists)
    {
      unsigned long unixtime;
      unixtime= getTime();
      Serial.println("Set (manual) RTC time " + String(unixtime));
      rtc.adjust(DateTime(unixtime));  // Set RTC time using NTP epoch time
      info = info + " RTC time " + String(unixtime) + " set by ManualTimeSet.";
    }
    
    Serial.println(info);
  }

  // Show all digits at startup
  vfd.begin(140, 1000, 10000);  
  for(int i=48; i< 58;i++)
  {
    vfd.setCharacter((char)(i),0); 
    vfd.setCharacter((char)(i),1);
    vfd.setCharacter((char)(i),2);
    vfd.setCharacter((char)(i),3);
    delay(500);
  }

  Serial.println("End  initialBoot ");
}

void setup() {
  Serial.begin(115200);
  Serial.println("setup");
  vfd.begin(160, 1000, 10000);
  esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause(); // check wakeup reason
  Serial.println(wakeup_cause);
  if (wakeup_cause != ESP_SLEEP_WAKEUP_EXT0) initialBoot();  // external reset -> initialize


  preferences.begin("Clock", false);
    ShowDate = preferences.getUInt("ShowDate", 0);
  preferences.end();

  // Attach the update ot the clock to an timer excuted every second
  updateTime.attach(1, []()
    {
      // setup time   
      time_t now;
      time(&now);
      tm *time = localtime(&now);
      int secs = (int)time->tm_sec;
          
      Serial.println(time, "Date: %d.%m.%y  Time: %H:%M:%S"); // print formated time    
      // Show day and month
      if(ShowDate > 0 && (secs%30 == 15 || secs%30 == 16))
      {
        vfd.setDP(1,0);
        vfd.setMinutes(time->tm_mon + 1);
        vfd.setHours(time->tm_mday);
      }
      else
      { 
        vfd.setMinutes(time->tm_min);
        vfd.setHours(time->tm_hour);      
        // colon on off every second
        if(secs%2 == 0)
        {
          vfd.setDP(1,1);
        }
        else
        {
          vfd.setDP(0,0);
        }
      }
    }
  );

  Serial.println("setup End RTC_Exists: " + String(RTC_Exists));
} // End setup

 
int pressedtimeBTN0;
int pressedtimeBTN1;
int pressedtimeBTN2;
void loop()
{
  
  // is configuration portal requested?
  while ( digitalRead(BTN0) == LOW ) 
  {
    pressedtimeBTN0  = pressedtimeBTN0 + 1;   
    delay(1000);
  }

  // Button was pressed
  if (pressedtimeBTN0 > 0)
  {
    Serial.println(" pressedtimeBTN0: " + String(pressedtimeBTN0));
    Serial.println(info);

    preferences.begin("Clock", false);
    RTCOnly = 0;
    preferences.putUInt("RTCOnly", 0);

       
    if (pressedtimeBTN0 > 5)
    {
      // button pressed for more than 7 seconds
      // Delete old config and start configwebpage
      rtc.adjust( DateTime(1970, 1, 1)); // set rtc to begin time so that it would not be used
      preferences.begin("Clock", false);
      preferences.clear();
      preferences.end();
      callWifiManagerConfig(2);
    }      
    else
    {    // button pressed for more than 3 seconds
      if (pressedtimeBTN0 > 2)
      {
        // start configwebpage without deleting configuration
        rtc.adjust( DateTime(1970, 1, 1)); // set rtc to begin time so that it would not be used
        callWifiManagerConfig(1);
      }    
    }
  }

  // start ManualTimeSet?
  while (digitalRead(BTN1) == LOW ) 
  {
    pressedtimeBTN1  = pressedtimeBTN1 + 1;   
    delay(1000);
  }

  if (pressedtimeBTN1 > 0)
  {
     Serial.println(" pressedtimeBTN1: " + String(pressedtimeBTN1));
     Serial.println(info);

    if (pressedtimeBTN1 > 1)
    {
      if (pressedtimeBTN1 > 5)
      {
          preferences.begin("Clock", false);
          RTCOnly = 0;
          preferences.putUInt("RTCOnly", 0);
          preferences.end();
          preferences.begin("Clock", false);
          // its neccessary to set rtc and internal time at the beginning
          // so that they are not used at startup
          rtc.adjust( DateTime(1970, 1, 1)); // set rtc to begin time
          struct timeval now = { .tv_sec = 0 }; //set internal time to to begin time
          settimeofday(&now, NULL);
          delay(1000);
          ESP.restart();
      }
      else
      {
        Serial.println("Call startManualTimeSet ");
        info = info + " Call startManualTimeSet ";
        unsigned long unixtime;
        unixtime= startManualTimeSet(vfd, ShowDate); //
        
        if (RTC_Exists)
        {
          Serial.println("Set (manual) RTC time " + String(unixtime));
          rtc.adjust(DateTime(unixtime));  // Set RTC time using NTP epoch time
          info = info + " RTC time " + String(unixtime) + " set by ManualTimeSet.";
          preferences.begin("Clock", false);
          RTCOnly = 1;
          preferences.putUInt("RTCOnly", 1);
          preferences.putString("timezone", timezone);
          preferences.end();
        }
      }
    }
  } // End loop

   // start ManualTimeSet?
  while (digitalRead(BTN2) == LOW ) 
  {
    pressedtimeBTN2  = pressedtimeBTN2 + 1;   
    delay(1000);
  }

  if (pressedtimeBTN2 > 1)
  {
    preferences.begin("Clock", false);
    if(ShowDate == 0)
    {
      ShowDate =1;
    }
    else
    {
      ShowDate = 0;
    }

    preferences.putUInt("ShowDate", ShowDate);
    preferences.end();
    preferences.begin("Clock", false);
  }


  pressedtimeBTN0 = 0;
  pressedtimeBTN1 = 0;
  pressedtimeBTN2 = 0;
 
}

unsigned long getTime() 
{
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) 
  {
    Serial.println("Failed to obtain time");
    return(0);
  }

  time(&now);
  return now;
}