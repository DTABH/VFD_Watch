#define ESP_DRD_USE_SPIFFS true
#include <esp_task_wdt.h> 
// Include Libraries
 
// WiFi Library
#include <WiFi.h>
// File System Library
#include <FS.h>
// SPI Flash Syetem Library
#include <SPIFFS.h>
// WiFiManager Library
#include <WiFiManager.h>
// Arduino JSON library
#include <ArduinoJson.h>
 
// JSON configuration file
#define JSON_CONFIG_FILE "/test_config.json"
 
// Flag for saving data
bool shouldSaveConfig = false;
 
// Variables to hold data from custom textboxes
char testString[50] = "test value";
int testNumber = 1234;
 
// Define WiFiManager Object
WiFiManager wm;
 
bool callWifiManagerConfig(int startConfigPortal)
{
  bool withRestart;

    // Setup Serial monitor
  Serial.begin(115200);
  delay(10);

  WiFiManager wfm;   
  
  // reset settings if in portal mode or - for testing
  if (startConfigPortal > 1)
  {
    wm.resetSettings();
  }
  // Change to true when testing to force configuration every time we run
  bool forceConfig = false;
 
  bool spiffsSetup = loadConfigFile();
  if (!spiffsSetup)
  {
    Serial.println(F("Forcing config mode as there is no saved config"));
    forceConfig = true;
  }

  delay(2000);
 	
  // Explicitly set WiFi mode
  WiFi.mode(WIFI_STA);
 
  // Set config save notify callback
  wm.setSaveConfigCallback(saveConfigCallback);
 
  // Set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wm.setAPCallback(configModeCallback);

  char* ntp = "de.pool.ntp.org";
  char* tz = "CET-1CEST,M3.5.0,M10.5.0/3";
  if(strlen(ntpserver) > 0){ntp = ntpserver;}
  if(strlen(timezone) > 0){tz = timezone;}

  // Add custom parameter	
	WiFiManagerParameter param_ntpserver("ntpserver", "NTP-Server", ntp, 50);  
	WiFiManagerParameter param_timezone("timezone", "Timezone-Server", tz, 50);  

  wfm.addParameter(&param_ntpserver);
  wfm.addParameter(&param_timezone);

  // set configportal timeout
  wfm.setConfigPortalTimeout(timeoutConfig);
  bool connected;
  if(startConfigPortal > 0)
  { 
    connected = wfm.startConfigPortal("(Wearable) Watch configuration");
    withRestart = true;
  }
  else
  {
    connected = wfm.autoConnect("(Wearable) Watch configuration");
  }
 
  if (!connected) 
  {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    if(withRestart)
    { 
      Serial.println("Restarting");
      ESP.restart();
      delay(5000);
    }
     Serial.println("NO Restart");
  }
 
  // Connected!
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());    
  
    // Save the custom parameters to FS only via Configportal
    if(startConfigPortal > 0)
    { 
      Serial.println("save Configportal parameters");
      strcpy(ntpserver, param_ntpserver.getValue());
      strcpy(timezone, param_timezone.getValue());      
      saveConfigFile();
    }

   return true;

}

void saveConfigFile()
// Save Config in JSON format
{
  Serial.println(F("Saving configuration..."));
  
  // Create a JSON document
  JsonDocument json;
  json["ntp"] = ntpserver;
  json["tz"] = timezone;
 
  // Open config file
  File configFile = SPIFFS.open(JSON_CONFIG_FILE, "w");
  if (!configFile)
  {
    // Error, file did not open
    Serial.println("failed to open config file for writing");
  }
 
  // Serialize JSON data to write to file
  serializeJsonPretty(json, Serial);
  if (serializeJson(json, configFile) == 0)
  {
    // Error writing file
    Serial.println(F("Failed to write to file"));
  }
  // Close file
  configFile.close();
}
 
bool loadConfigFile()
// Load existing configuration file
{
  // Uncomment if we need to format filesystem
  // SPIFFS.format();
 
  // Read configuration from FS json
  Serial.println("Mounting File System...");
 
  // May need to make it begin(true) first time you are using SPIFFS
  if (SPIFFS.begin(false) || SPIFFS.begin(true))
  {
    Serial.println("mounted file system");
    if (SPIFFS.exists(JSON_CONFIG_FILE))
    {
      // The file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open(JSON_CONFIG_FILE, "r");
      if (configFile)
      {
        Serial.println("Opened configuration file");
        JsonDocument json;
        DeserializationError error = deserializeJson(json, configFile);
        serializeJsonPretty(json, Serial);
        if (!error)
        {
          try 
          { 
            Serial.println("Parsing JSON"); 
            String ntp = json["ntp"];
            ntpserver = strdup(ntp.c_str());
            Serial.println("ntpserver : " + String(ntpserver)); 
            String tz =json["tz"];    
            timezone = strdup(tz.c_str());
            Serial.println("timezone : " + String(timezone));        
            return true;
          }
          catch(String error) 
          {
          }
        }
        else
        {
          // Error loading JSON data
          Serial.println("Failed to load json config");
        }
      }
    }
  }
  else
  {
    // Error mounting file system
    Serial.println("Failed to mount FS");
  }
 
  return false;
}
 
 
void saveConfigCallback()
// Callback notifying us of the need to save configuration
{
  Serial.println("Should save config");
  shouldSaveConfig = true;
}
 
void configModeCallback(WiFiManager *myWiFiManager)
// Called when config mode launched
{
  Serial.println("Entered Configuration Mode");
 
  Serial.print("Config SSID: ");
  Serial.println(myWiFiManager->getConfigPortalSSID());
 
  Serial.print("Config IP Address: ");
  Serial.println(WiFi.softAPIP());
}
 