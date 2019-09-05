/*
 * TODO
 * 
 * Make more C++ like?
 * Write http server for retrival of auth codes
 * Make softAP for initial configurate
 * Move location fetching into a separate thread, so I can do some more fun stuff
 * Use SSL cert for more secure communication
 * Reset prefs if switch is closed
 */

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <ESP8266mDNS.h>

#include <base64.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#include <FastLED.h>
#define NUM_LEDS 50
#define DATA_PIN D4
#define ORDER BRG
//#define BRIGHTNESS 5
CRGB leds[NUM_LEDS];

#define ONBOARD_LED 16
#define HARDRESET_PIN D5
#define WEBCONFIG_PIN D6

#define SOFTAP_SSID "eve8266"
ESP8266WebServer server(80);

const char* default_identifier = "esp8266"; //Default identifier for the pref data
typedef struct prefs_t
{
  char identifier[9];
  char ssid[33];
  char password[33];
  char client_id[33];
  char secret_key[65];
  char access_token[257];
  char refresh_token[65];
  time_t access_token_time;
};

prefs_t prefs;

String character_id = "";
String corp_id = "";
String character_name = "";
String expires_on = "";


const String user_agent = "EVE8266 Mood lighting";
String auth_code;

float security;
String system_id;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

StaticJsonDocument<4096> jsonBuffer;

void webconfig_start ();

void wifi_connect ()
{
  //Enable station mode wifi
  
  WiFi.mode(WIFI_STA);
  WiFi.enableAP(false);
  WiFi.hostname("eve8266");
  WiFi.begin(prefs.ssid, prefs.password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
    //Serial.println(WiFi.status());
  }
  Serial.println("Connected to the WiFi network");
  Serial.println (WiFi.localIP());

  if (MDNS.begin("eve8266")) 
    Serial.println("Starting mDNS responder");
  else
    Serial.println("Error setting up MDNS responder!");
  
}


void setup() {
  WiFi.setAutoConnect(false);
  FastLED.addLeds<WS2811, DATA_PIN, ORDER>(leds, NUM_LEDS);
  //FastLED.setBrightness (BRIGHTNESS);
  FastLED.clear();
  FastLED.show();

  pinMode(WEBCONFIG_PIN,INPUT_PULLUP);
  
  Serial.begin(115200);
  Serial.println ();
  Serial.println ("Booting...");
   
  prefs_check ();
  prefs_read ();
  prefs_print (); //TODO probably want to remove this once it's all working, security risk
  detect_reset_pin ();
  if (strcmp (prefs.ssid, "") == 0)
  {
    Serial.println ("No SSID in preferences, starting softap mode");
    softap_start ();
  }
  wifi_connect ();
  
  Serial.println ("Starting NTP");
  timeClient.begin();
  while(!timeClient.update()) 
  {
    Serial.print (".");
    timeClient.forceUpdate();
  }
  
//TODO Error handling
  if (digitalRead (WEBCONFIG_PIN) == 0)
  {
    Serial.println ("Webconfig pin grounded, starting webconfig http server");
    webconfig_start ();
  }

  //TODO Change this to a while so we loop until we get a working refresh token?  or should that loop be in the webservrR?
  if (strcmp (prefs.refresh_token, "") == 0)
  {
    if (strcmp (prefs.secret_key, "") == 0 || strcmp (prefs.client_id, "") == 0)
    {
      Serial.println ("No secret_key / client_id found!");
      Serial.println ("Starting webconfig http server");
      webconfig_start ();
    }
    eve_get_access_token ();
  }
  eve_get_refresh_token ();
  eve_get_token_details ();
  eve_get_corp_id ();
  digitalWrite(ONBOARD_LED,LOW);
}
void prefs_print ()
{
  Serial.print ("Identifier: ");
  Serial.println (prefs.identifier);
  Serial.print ("SSID: ");
  Serial.println (prefs.ssid);
  Serial.print ("Pass: ");
  Serial.println (prefs.password);
  Serial.print ("Client ID: ");
  Serial.println (prefs.client_id);
  Serial.print ("Secret Key: ");
  Serial.println (prefs.secret_key);
  Serial.print ("Access token: ");
  Serial.println (prefs.access_token);
  Serial.print ("Refresh token: ");
  Serial.println (prefs.refresh_token);  
  Serial.print ("Size: ");
  Serial.println (sizeof(prefs));
}

//Check for valid prefs, if not default them.
void prefs_check ()
{
  prefs_read ();
  if (strcmp (prefs.identifier, default_identifier) != 0)
  {
    Serial.println (prefs.identifier);
    Serial.println (strlen(prefs.identifier));
    Serial.println ("Preferences corrupt, defaulting");
    strcpy (prefs.identifier, default_identifier);
    strcpy (prefs.ssid, "");
    strcpy (prefs.password, "");
    strcpy (prefs.client_id, "");
    strcpy (prefs.secret_key, "");
    strcpy (prefs.access_token, "");
    strcpy (prefs.refresh_token, "");
    prefs.access_token_time = 0;
    prefs_write ();
  }
}

void prefs_clear ()
{
  Serial.println ("*********** CLEARING PREFERENCES ************");
  EEPROM.begin (4096);
  for (int i=0; i < 4096; i++)
  {
    EEPROM.put (i, 0);
  }
  EEPROM.commit();
  EEPROM.end();
}

void prefs_write ()
{
  EEPROM.begin(sizeof(prefs_t));
  EEPROM.put (0, prefs);
  EEPROM.commit();
  EEPROM.end ();
}

void prefs_read ()
{
  EEPROM.begin(sizeof(prefs_t));
  EEPROM.get (0, prefs);
  /*
  if (strlen (prefs.refresh_token) > 64)
  {
    Serial.println ("What the actual fook");
    Serial.println (strlen(prefs.refresh_token));
    prefs.refresh_token[65] = '\0';
  }
  */
  EEPROM.end ();
}

void detect_reset_pin ()
{
  Serial.println ("Checking for reset pins");
  int i = 0;
  pinMode(HARDRESET_PIN, INPUT_PULLUP);
  
  while (digitalRead (HARDRESET_PIN) == 0)
  {
    Serial.print ("Hard reset in ");
    Serial.println (5 - i);
    if (i++ > 5)
    {
      Serial.println("Hard reset activated, clearing ALL stored preferences and resetting in 10 seconds");
      //TODO Maybe flash the LEDs or something
      delay(10000);
      prefs_clear ();
      ESP.restart();
    }
    delay (1000);   
  }
}

bool token_expired ()
{
  if ((time(NULL) - prefs.access_token_time) > 900) //Get a new token if there's less than 10 minutes left
    return true;
  else
    return false;
}

void check_access_token ()
{
  if (token_expired ())
    eve_get_refresh_token ();    
}

String eve_get_baseencoded ()
{
  String toEncode = String(prefs.client_id) + ":" + String(prefs.secret_key);
  String encoded = base64::encode(toEncode, false); //Fuck you and your newlines, you stupid piece of shit....
  return encoded;
}

//Uses the auth code to get a new access token
int eve_get_access_token ()
{
  char postdata[500];

  //Encode the client id and key
  Serial.println ("Fetching access token");

  //Setup the connection, add the headers and setup the JSON
  
  std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
  client->setInsecure();

  HTTPClient http;
  //http.begin ("https://login.eveonline.com/oauth/token", "AC:D2:1F:D7:E5:8B:51:C2:CE:32:C6:7B:16:02:71:96:E8:49:FF:C0");
  http.begin (*client, "https://login.eveonline.com/oauth/token");
  http.setUserAgent (user_agent);
  http.addHeader ("Content-Type", "application/json");
  http.addHeader ("Authorization", "Basic " + eve_get_baseencoded());
  
  
  jsonBuffer["grant_type"] = "authorization_code";
  jsonBuffer["code"] = auth_code;

  serializeJson(jsonBuffer, postdata);
  int httpCode = http.POST (postdata);
  if (httpCode == 200) 
  {
    String payload = http.getString();
    Serial.println (payload);
    //Decode payload
    DeserializationError error = deserializeJson (jsonBuffer, payload);
    if (error) 
    {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.c_str());
      return -1;
    }
    String access = jsonBuffer["access_token"];
    String refresh = jsonBuffer["refresh_token"];
    strcpy (prefs.access_token, access.c_str());
    strcpy (prefs.refresh_token, refresh.c_str());
    Serial.print ("Fetched new access token: ");
    Serial.println (prefs.access_token);
    Serial.print ("Fetched new refresh token: ");
    Serial.println (prefs.refresh_token);
    prefs_write ();
    
  }
  else 
  {
    Serial.print("Error on HTTP request: ");
    Serial.println(httpCode);
    Serial.println (http.getString());
    while (1)
      delay (1000);
  }
  http.end();
  return httpCode;
}

//Uses the refresh token to get a new access token
void eve_get_refresh_token ()
{
  char postdata[500];

  //Encode the client id and key
  Serial.println ("Using refresh token to get new access token");

  //Setup the connection, add the headers and setup the JSON
  std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
  client->setInsecure();
  HTTPClient http;
  http.begin (*client, "https://login.eveonline.com/oauth/token");
  http.setUserAgent (user_agent);
  http.addHeader ("Content-Type", "application/json");
  http.addHeader ("Authorization", "Basic " + eve_get_baseencoded());
  
  jsonBuffer["grant_type"] = "refresh_token";
  jsonBuffer["refresh_token"] = String (prefs.refresh_token);

  serializeJson(jsonBuffer, postdata);
  int httpCode = http.POST (postdata);
  
  if (httpCode == 200) 
  {
    String payload = http.getString();
    //Decode payload
    DeserializationError error = deserializeJson (jsonBuffer, payload);
    if (error) 
    {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.c_str());
      return;
    }
    strcpy (prefs.access_token, jsonBuffer["access_token"]);
    Serial.println ("Fetched new access token");
    Serial.println (prefs.access_token);
    prefs.access_token_time = time(NULL);
    prefs_write ();
  }
  else 
  {
    Serial.print("Error on HTTP request: ");
    Serial.println(httpCode);
    Serial.println(http.getString());
  }
  
  http.end();
}

void eve_get_token_details ()
{
  std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
  client->setInsecure();
  HTTPClient http;
  http.begin (*client, "https://login.eveonline.com/oauth/verify");
  http.setUserAgent (user_agent);
  http.addHeader ("Authorization", "Bearer "+String(prefs.access_token));
  
  int httpCode = http.GET();                                        //Make the request
  if (httpCode > 0) 
  {
    String payload = http.getString();
    DeserializationError error = deserializeJson (jsonBuffer, payload);
    if (error) 
    {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.c_str());
      return;
    }
    expires_on = jsonBuffer["ExpiresOn"].as<String>();
    character_id = jsonBuffer["CharacterID"].as<String>();
    character_name = jsonBuffer["CharacterName"].as<String>();
  }
  else 
  {
    Serial.println("Error on HTTP request");
  }

  char text[80];
  sprintf (text, "Character: %s (%s)", character_name.c_str(), character_id.c_str());
  Serial.println (text);
  
  http.end();
}

String eve_get_location ()
{
  return eve_get_generic ("https://esi.evetech.net/latest/characters/"+ character_id + "/location/?datasource=tranquility", "solar_system_id", true);
}

void eve_get_security ()
{
  String system_id_old = system_id;
  system_id = eve_get_location ();
  if (system_id == "Error")
  {
    Serial.println ("Error fetching new system_id");
    system_id = system_id_old;
  }
  
  if (system_id_old == system_id) //Don't bother updating, we haven't moved
    return;
  String esi_return = eve_get_generic ("https://esi.evetech.net/latest/universe/systems/"+system_id+"/?datasource=tranquility&language=en-us", "security_status", false);
  if (esi_return != "Error")
    security = esi_return.toFloat();
  return;
}

String eve_get_generic (String url, String key, bool auth)
{
  check_access_token ();
  std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
  client->setInsecure();
  HTTPClient http;
  http.begin (*client, url);
  http.setUserAgent (user_agent);
  if (auth)
    http.addHeader ("Authorization", "Bearer "+String(prefs.access_token));
  int httpCode = http.GET();                                        //Make the request
  if (httpCode == 200) 
  { //Check for the returning code
    String payload = http.getString();
    //Decode payload
    Serial.println(httpCode);
    Serial.println(payload);   
    DeserializationError error = deserializeJson (jsonBuffer, payload);
    
    if (error) 
    {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.c_str());
      http.end();
      return "Error";
    }
    http.end();
    return jsonBuffer[key].as<String>();  
  }
  else 
  {
    Serial.println("Error on HTTP request");
    http.end(); //Free the resources
    return "Error";
  }
}

int eve_get_corp_id ()
{
  check_access_token ();
  HTTPClient http;
  std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
  client->setInsecure();
  http.begin (*client, "https://esi.evetech.net/latest/characters/"+ character_id + "/?datasource=tranquility");
  http.setUserAgent (user_agent);
  http.addHeader ("Authorization", "Bearer "+ String(prefs.access_token));
  int httpCode = http.GET();                                        //Make the request
  if (httpCode > 0) 
  { //Check for the returning code
    String payload = http.getString();
    //Decode payload   
    DeserializationError error = deserializeJson (jsonBuffer, payload);
    
    if (error) 
    {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.c_str());
      return -1;
    }
    corp_id = jsonBuffer["corporation_id"].as<String>();  
  }
  else 
  {
    Serial.println("Error on HTTP request");
  }
  http.end(); //Free the resources
  return 0;
}

int map_security_to_color (float security)
{
  /*
   * Colors from http://web.archive.org/web/20120219150840/http://blog.evepanel.net/eve-online/igb/colors-of-the-security-status.html are out of date
#2FEFEF 1.0
#48F0C0 0.9
#00EF47 0.8
#00F000 0.7
#8FEF2F 0.6
#EFEF00 0.5
#D77700 0.4
#F06000 0.3
#F04800 0.2
#D73000 0.1
#F00000 0.0
   */
   //Security levels round up to the nearest 0.1
  if (security >= 0.85)
    return 0x42c8a1;
  else if (security >= 0.75)
    return 0x08c842;
  else if (security >= 0.65)
    return 0x08c808;
  else if (security >= 0.55)
    return 0x7bc62f;
  else if (security >= 0.45)
    return 0xf6f641;
  else if (security >= 0.35)
    return 0xC3C203;
  else if (security >= 0.25)
    return 0xc75509;
  else if (security >= 0.15)
    return 0xc74209;
  else if (security >= 0.05)
    return 0xb42f09;  
  else if (security >= 0)
    return 0xB02903;  
  else
    return 0xC30303;
}

// Helper function that blends one uint8_t toward another by a given amount
void nblendU8TowardU8( uint8_t& cur, const uint8_t target, uint8_t amount)
{
  if( cur == target) return;
  
  if( cur < target ) {
    uint8_t delta = target - cur;
    delta = scale8_video( delta, amount);
    cur += delta;
  } else {
    uint8_t delta = cur - target;
    delta = scale8_video( delta, amount);
    cur -= delta;
  }
}

// Blend one CRGB color toward another CRGB color by a given amount.
// Blending is linear, and done in the RGB color space.
// This function modifies 'cur' in place.
CRGB fadeTowardColor( CRGB& cur, const CRGB& target, uint8_t amount)
{
  nblendU8TowardU8( cur.red,   target.red,   amount);
  nblendU8TowardU8( cur.green, target.green, amount);
  nblendU8TowardU8( cur.blue,  target.blue,  amount);
  return cur;
}

// Fade an entire array of CRGBs toward a given background color by a given amount
// This function modifies the pixel array in place.
void fadeTowardColor( CRGB* L, uint16_t N, const CRGB& bgColor, uint8_t fadeAmount)
{
  for( uint16_t i = 0; i < N; i++) {
    fadeTowardColor( L[i], bgColor, fadeAmount);
  }
}

void loop() 
{
  CRGB color;
  CRGB color_old;
  eve_get_security ();

  color_old = color;
  color = map_security_to_color (security);

  if (color != color_old)
  {
    int i;
    for (i = 0;i < 255; i++)
    {
      fadeTowardColor(leds, NUM_LEDS, color, 1);
      FastLED.show();
      delay (10);
    }
  }
  Serial.println (security);
  Serial.println (map_security_to_color (security), HEX);
  /*
  int i;
  for (i = 0; i < NUM_LEDS; i++)
  {
    leds[i] = color; 
  }
  FastLED.show();
  */
  delay (250);

  /*
  if ((WiFi.status() == WL_CONNECTED)) 
  {
 
    Serial.println (timeClient.getFormattedTime());
    eve_get_corp_wallet ();
    eve_get_security ();
  }
 
  delay(10000);
  */
 
}
/*
 * SoftAP Stuffs
 * TODO:  Prettify the webpage, change it to a so we aren't storing sensitive details in the URL
 */

IPAddress softap_ip(192,168,4,1);
IPAddress softap_gateway(192,168,4,1);
IPAddress softap_subnet(255,255,255,0);


const char wifi_page[] = R"=====(
<!DOCTYPE html>
<html>
<body>
 
<h2>EVE8266 EVE Mood Lighting<h2>
<h3>Wifi configuration</h3>
 
<form action="/wifiset" method="POST">
  SSID:<br>
  <input type="text" name="ssid" value="" >
  <br>
  Password:<br>
  <input type="text" name="password" value="">
  <br><br>
  <input type="submit" value="Submit">
</form> 
 
</body>
</html>
)=====";

void softap_handleRoot() 
{
 String s = wifi_page; //Read HTML contents
 server.send(200, "text/html", s); //Send web page
}

void softap_handleForm() {
  //TODO Handle string lengths, wait before restarting
 server.arg("ssid").toCharArray(prefs.ssid, sizeof(prefs.ssid));
 server.arg("password").toCharArray(prefs.password, sizeof(prefs.password));
 /*
 strcpy (prefs.ssid, server.arg("ssid").c_str()); 
 strcpy (prefs.password, server.arg("password").c_str());
 */
 prefs_write();
 String s = "<a href='/'> SSID:" + String (prefs.ssid) + " Password: " + String (prefs.password)+ "Resetting in 5 seconds </a>";
 server.send(200, "text/html", s); //Send web page
 ESP.restart();
}

void softap_start ()
{
  WiFi.softAPConfig(softap_ip, softap_gateway, softap_subnet);
  WiFi.softAP(SOFTAP_SSID);
  Serial.print ("SoftAP SSID: ");
  Serial.println (SOFTAP_SSID);
  Serial.print("SoftAP IP address: ");
  Serial.println(WiFi.softAPIP());
  server.on("/", softap_handleRoot);
  server.on("/wifiset", softap_handleForm);
  server.begin();
  Serial.println("SoftAP HTTP server started");
  while (1)
    server.handleClient();
}

/*
 * Webserver config stuffs
 * 
 */

bool webconfig_active;

String webconfig_page = R"=====(
<!DOCTYPE html>
<html>
<body>
<h2>EVE8266 Mood Lighting<h2>
<h3>Callback URL and client_id/secret_key setup</h3>
Step 1:  Log into <a href="https://developers.eveonline.com">https://developers.eveonline.com</a> and click on 'Manage Applications'
<br>
Step 2: Click on 'Create New Application'
<br>
Step 3: Fill out the name and description fields with something, it's not vitally important but at least be descriptive
<br>
Step 4: Set connection type to "Authentication & API Access"
<br>
Step 5: Under 'Permissions', add 'esi-location.read_location.v1' to the requested scopes list
<br>
Step 6: Copy and paste the following into the callback url box and click 'Create application'
<br>
<br>
$CALLBACKURL
<br>
<br>
Step 7: On the <a href="https://developers.eveonline.com/applications">Applications page</a>, find your newly created application and click on 'View Application'
<br>
Step 8: Copy and paste the client ID and secret key settings into the boxes below and click submit
<br>
<form action="/webconfigset" method="POST">
  client_id:<br>
  <input type="text" name="client_id" value="" size=64>
  <br>
  secret key:<br>
  <input type="text" name="secret_key" value="" size=64>
  <br>
  <input type="submit" value="Submit"> 
</form> 
</body>
</html>
)=====";

String authcode_page = R"=====(
<!DOCTYPE html>
<html>
<body>
<h2>EVE8266 Mood Lighting<h2>
<h3>Authorise reading character location</h3>
<br>
Step 9: Log into EVE Online and authorise the application to locate your characters location
<a href="$AUTH_URL">
<img src="https://web.ccpgamescdn.com/eveonlineassets/developers/eve-sso-login-white-large.png">
</a>
</body>
</html>
)=====";

void webconfig_handleRoot() 
{
 String callback_url = "http://"+WiFi.localIP().toString()+"/callback";
 String s = webconfig_page;
 s.replace("$CALLBACKURL", callback_url);
 server.send(200, "text/html", s);
}

void webconfig_handleForm() 
{
  //TODO Handle string lengths, wait before restarting, fetch a valid auth code before dipping out of the loop
 server.arg("client_id").toCharArray(prefs.client_id, sizeof(prefs.client_id));
 server.arg("secret_key").toCharArray(prefs.secret_key, sizeof(prefs.secret_key));
 prefs_write ();

 String callback_url = "http://"+WiFi.localIP().toString()+"/callback";
 String auth_url = "https://login.eveonline.com/oauth/authorize?response_type=code&redirect_uri="+callback_url+"/&client_id="+prefs.client_id+"&scope=esi-location.read_location.v1";
 String s = authcode_page;
 s.replace("$AUTH_URL", auth_url);
 server.send(200, "text/html", s);
}

void webconfig_handleCallback ()
{
  String s;
  auth_code = server.arg("code");
  if (eve_get_access_token () != 200)
  {
    s = "<!DOCTYPE html><html><body><h2>Failed to fetch access token<h2></body></html>";
  }
  else
  {
    s = "<!DOCTYPE html><html><body><h2>Fetched new access token!</h2><br>Rebooting in 5 seconds, don't forget to remove webconfig jumper</body></html>";
  }
  server.send(200, "text/html", s);
  delay(5000);
  ESP.restart();
}

void webconfig_start ()
{
  webconfig_active = true;
  Serial.print ("Webconfig IP: ");
  Serial.println (WiFi.localIP());
  server.on("/", webconfig_handleRoot);
  server.on("/webconfigset", webconfig_handleForm);
  server.on("/callback/", webconfig_handleCallback);
  server.begin();
  Serial.println("Webconfig server started");
  while (webconfig_active)
    server.handleClient();
}
