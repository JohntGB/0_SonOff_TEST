#include <ESP8266WiFi.h>
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>
#include <ESP8266SSDP.h>
#include <ESP8266HTTPUpdateServer.h>
ESP8266HTTPUpdateServer httpUpdater;
#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <time.h>
#include <sys/time.h>                   // struct timeval
#include <NtpClientLib.h>

//#include <NTPClient.h>
#include <WiFiUdp.h>

#define SONOFF
//#define SONOFF_TH
//#define SONOFF_S20
//#define SONOFF_TOUCH //ESP8285 !!!!!!!!!!
//#define SONOFF_SV
//#define SONOFF_POW
//#define SONOFF_DUAL
//#define SONOFF_4CH //ESP8285 !!!!!!!!!!
//#define ECOPLUG
//#define SONOFF_IFAN02 //ESP8285 !!!!!!!!!!
//' #define SHELLY


#define TEST_ONLY

#define TZ              -5       // (utc+) TZ in hours
#define DST_MN          60      // use 60mn for summer time in some countries
#define TZ_MN           ((TZ)*60)
#define TZ_SEC          ((TZ)*3600)
#define DST_SEC         ((DST_MN)*60)

int8_t timeZone = -5;
int8_t minutesTimeZone = 0;

timeval cbtime;			// time set in callback
bool cbtime_set = false;

void time_is_set (void)
{
  gettimeofday(&cbtime, NULL);
  cbtime_set = true;
}



#ifdef SONOFF_POW
#include "HLW8012.h"
#endif

#if defined SONOFF
#include <DHT_TT.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#endif


String remoteServerResponse;
String currentDateTime;
String macAddress;
String localIP;


String message = "";

String inputString = "";         // a string to hold incoming data
boolean stringComplete = false;  // whether the string is complete
int currentStatus = LOW;

boolean needUpdate1 = true;
boolean needUpdate2 = true;
boolean needUpdate3 = true;
boolean needUpdate4 = true;
boolean inAutoOff1 = false;
boolean inAutoOff2 = false;
boolean inAutoOff3 = false;
boolean inAutoOff4 = false;
boolean needReboot = false;
boolean shortPress = false;
int previousFanSpeed = -1;

//stores if the switch was high before at all
int state1 = LOW;
int state2 = LOW;
int state3 = LOW;
int state4 = LOW;
int state_ext = LOW;
//stores the time each button went high or low
unsigned long current_high1;
unsigned long current_low1;
unsigned long current_high2;
unsigned long current_low2;
unsigned long current_high3;
unsigned long current_low3;
unsigned long current_high4;
unsigned long current_low4;
unsigned long current_high_ext;
unsigned long current_low_ext;

#if defined SONOFF 
const char * projectName = "Sonoff";
String softwareVersion = "2.0.5";
#endif


const char compile_date[] = __DATE__ " " __TIME__;

unsigned long connectionFailures;

#define FLASH_EEPROM_SIZE 4096
extern "C" {
#include "spi_flash.h"
}
extern "C" uint32_t _SPIFFS_start;
extern "C" uint32_t _SPIFFS_end;
extern "C" uint32_t _SPIFFS_page;
extern "C" uint32_t _SPIFFS_block;

unsigned long failureTimeout = millis();
long debounceDelay = 20;    // the debounce time (in ms); increase if false positives
unsigned long timer1s = 0;
unsigned long timer5s = 0;
unsigned long timer5m = 0;
unsigned long timer1m = 0;
unsigned long timerW = 0;
unsigned long timerUptime;
unsigned long autoOffTimer1 = 0;
unsigned long autoOffTimer2 = 0;
unsigned long autoOffTimer3 = 0;
unsigned long autoOffTimer4 = 0;
unsigned long currentmillis = 0;

#if  defined SONOFF 
unsigned long timerT = 0;
unsigned long timerH = 0;
#endif


#if defined SONOFF 
#define REL_PIN1       12	// D6
#define LED_PIN1       13	// D7
#define KEY_PIN1       0	// D3
#define EXT_PIN        14	// D5
#endif




#if defined SONOFF
#define EXT_PIN        14
#define DS_PIN         14
#define DHTTYPE DHT22
DHT dht(EXT_PIN, DHTTYPE);
float temperature;
float humidity;

OneWire oneWire(DS_PIN);
DallasTemperature ds18b20(&oneWire);

double _dsTemperature = 0;

double getDSTemperature() {
	return _dsTemperature;
}

void dsSetup() {
	ds18b20.begin();
}
#endif

#if defined SONOFF  


#define LEDoff1 digitalWrite(LED_PIN1,HIGH)
#define LEDon1 digitalWrite(LED_PIN1,LOW)
#else
#define LEDoff1 digitalWrite(LED_PIN1,LOW)
#define LEDon1 digitalWrite(LED_PIN1,HIGH)
#endif

#if defined SONOFF
#define Relayoff1 {\
  if (Settings.currentState1) needUpdate1 = true; \
  digitalWrite(REL_PIN1,LOW); \
  Settings.currentState1 = false; \
  LEDoff1; \
}
#define Relayon1 {\
  if (!Settings.currentState1) needUpdate1 = true; \
  digitalWrite(REL_PIN1,HIGH); \
  Settings.currentState1 = true; \
  LEDon1; \
}
#else
#define Relayoff1 {\
  if (Settings.currentState1) needUpdate1 = true; \
  digitalWrite(REL_PIN1,LOW); \
  Settings.currentState1 = false; \
}
#define Relayon1 {\
  if (!Settings.currentState1) needUpdate1 = true; \
  digitalWrite(REL_PIN1,HIGH); \
  Settings.currentState1 = true; \
}
#endif

byte mac[6];

boolean WebLoggedIn = false;
int WebLoggedInTimer = 2;
String printWebString = "";
boolean printToWeb = false;

#define DEFAULT_REMOTEIP			   "40.112.57.194"

#define DEFAULT_HAIP                   "0.0.0.0"
#define DEFAULT_HAPORT                 39500
#define DEFAULT_RESETWIFI              false
#define DEFAULT_POS                    0
#define DEFAULT_CURRENT STATE          ""
#define DEFAULT_IP                     "0.0.0.0"
#define DEFAULT_GATEWAY                "0.0.0.0"
#define DEFAULT_SUBNET                 "0.0.0.0"
#define DEFAULT_DNS                    "0.0.0.0"
#define DEFAULT_USE_STATIC             false
#define DEFAULT_LONG_PRESS             false
#define DEFAULT_REALLY_LONG_PRESS      false
#define DEFAULT_USE_PASSWORD           false
#define DEFAULT_USE_PASSWORD_CONTROL   false
#define DEFAULT_PASSWORD               ""
#define DEFAULT_PORT                   80
#define DEFAULT_SWITCH_TYPE            0
#define DEFAULT_AUTO_OFF1              0
#define DEFAULT_AUTO_OFF2              0
#define DEFAULT_AUTO_OFF3              0
#define DEFAULT_AUTO_OFF4              0
#define DEFAULT_UREPORT                60
#define DEFAULT_DEBOUNCE               20
#define DEFAULT_HOSTNAME               ""

#define DEFAULT_EXT_TYPE               0

struct SettingsStruct
{
	byte          haIP[4];
	unsigned int  haPort;
	boolean       resetWifi;
	int           powerOnState;
	boolean       currentState1;
	byte          IP[4];
	byte          Gateway[4];
	byte          Subnet[4];
	byte          DNS[4];
	boolean       useStatic;
	boolean       longPress;
	boolean       reallyLongPress;
	boolean       usePassword;
	boolean       usePasswordControl;
#if defined SONOFF 
	int           usePort;
	int           switchType;
	int           autoOff1;
	int           uReport;
	int           debounce;
	int           externalType;
	char          hostName[26];

#endif
	char			remoteIP[30];
	boolean			reportToRemote;
	int				reportToRemoteInterval;
	boolean			getResponseFromRemote;
	int				getResponseFromRemoteInterval;
	char			description[50];


} Settings;

struct SecurityStruct
{
	char          Password[26];
	int           settingsVersion;
} SecuritySettings;



// Start WiFi Server
std::unique_ptr<ESP8266WebServer> server;

String padHex(String hex) {
	if (hex.length() == 1) {
		hex = "0" + hex;
	}
	return hex;
}

void handleRoot() {
	server->send(200, "application/json", "{\"message\":\"Sonoff Wifi Switch\"}");
}

void handleNotFound() {
	String message = "File Not Found\n\n";
	message += "URI: ";
	message += server->uri();
	message += "\nMethod: ";
	message += (server->method() == HTTP_GET) ? "GET" : "POST";
	message += "\nArguments: ";
	message += server->args();
	message += "\n";
	for (uint8_t i = 0; i < server->args(); i++) {
		message += " " + server->argName(i) + ": " + server->arg(i) + "\n";
	}
	server->send(404, "text/plain", message);
}

void addHeader(boolean showMenu, String& str)
{
	boolean cssfile = false;

	str += F("<script language=\"javascript\"><!--\n");
	str += F("function dept_onchange(frmselect) {frmselect.submit();}\n");
	str += F("//--></script>");
	str += F("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/><title>");
	str += projectName;
	str += F("</title>");

	str += F("<style>");
	str += F("* {font-family:sans-serif; font-size:12pt;}");
	str += F("h1 {font-size:16pt; color:black;}");
	str += F("h6 {font-size:10pt; color:black; text-align:center;}");
	str += F(".button-menu {background-color:#ffffff; color:#000000; margin: 10px; text-decoration:none}");
	str += F(".button-link {border-radius:0.3rem; padding:5px 15px; background-color:#000000; color:#fff; border:solid 1px #fff; text-decoration:none}");
	str += F(".button-menu:hover {background:#ddddff;}");
	str += F(".button-link:hover {background:#707070;}");
	str += F("th {border-radius:0.3rem; padding:10px; background-color:black; color:#ffffff;}");
	str += F("td {padding:7px;}");
	str += F("table {color:black;}");
	str += F(".div_l {float: left;}");
	str += F(".div_r {float: right; margin: 2px; padding: 1px 10px; border-radius: 7px; background-color:#080; color:white;}");
	str += F(".div_br {clear: both;}");
	str += F("</style>");


	str += F("</head>");
	str += F("<center>");

}

void addFooter(String& str)
{
	str += F("<h6><a href=\"http://smartlife.tech\">smartlife.tech</a></h6></body></center>");
}

void addMenu(String& str)
{
	str += F("<a class=\"button-menu\" href=\".\">Main</a>");
	str += F("<a class=\"button-menu\" href=\"advanced\">Advanced</a>");
	str += F("<a class=\"button-menu\" href=\"control\">Control</a>");
	str += F("<a class=\"button-menu\" href=\"update\">Firmware</a>");
}

void addRebootBanner(String& str)
{
	if (needReboot == true) {
		str += F("<TR><TD bgcolor='#A9A9A9' colspan='2' align='center'><font color='white'>Reboot needed for changes to take effect. <a href='/r'>Reboot</a></font>");
	}
}

void relayControl(int relay, int value) {
	//' TTTT
	Serial.println("extRelayToggle vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv");
	Serial.print("relayControl...relay: ");
	Serial.println(relay);

	Serial.print("relayControl...value: ");
	Serial.println(value);


	switch (relay)
	{
	case 0: //All Switches
	{
		if (value == 0) {
			Relayoff1;

		}

		if (value == 1) {
			if (!inAutoOff1) {
				autoOffTimer1 = millis();
				inAutoOff1 = true;
			}
			Relayon1;

		}
		if (value == 2) {
			if (Settings.currentState1) {
				Relayoff1;
			}
			else {
				if (!inAutoOff1) {
					autoOffTimer1 = millis();
					inAutoOff1 = true;
				}
				Relayon1;
			}
		}
		break;
	}
	case 1: //Relay 1
	{
		if (value == 0) {
			Relayoff1;
#ifdef SONOFF
			LEDoff1;
#endif
		}
		if (value == 1) {
			if (!inAutoOff1) {
				autoOffTimer1 = millis();
				inAutoOff1 = true;
			}
			Relayon1;
#ifdef SONOFF
			LEDon1;
#endif
		}
		if (value == 2) {
			//' TTTT
			if (Settings.currentState1) {
				Serial.println("TURNNING OFF.....................");
				Relayoff1;
			}
			else {
				if (!inAutoOff1) {
					autoOffTimer1 = millis();
					inAutoOff1 = true;
				}
				Serial.println("TURNNING ONNNNNNN.....................");
				Relayon1;
			}
		}
		break;
	}
	case 2: //Relay 2
	{
		break;
	}
	case 3: //Relay 3
	{
		break;
	}
	case 4: //Relay 4
	{
		break;
	}
	}
	if (Settings.powerOnState == 2 || Settings.powerOnState == 3)
	{
		SaveSettings();
	}

	Serial.println("extRelayToggle ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ ");
}

void relayToggle1() {
	int reading = digitalRead(KEY_PIN1);
	if (reading == LOW) {
		current_low1 = millis();
		state1 = LOW;
	}
	if (reading == HIGH && state1 == LOW)
	{
		current_high1 = millis();
		state1 = HIGH;
		if ((current_high1 - current_low1) > (Settings.debounce ? Settings.debounce : debounceDelay) && (current_high1 - current_low1) < 10000)
		{
			relayControl(1, 2);
			shortPress = true;
		}
		else if ((current_high1 - current_low1) >= 10000 && (current_high1 - current_low1) < 20000)
		{
			Settings.longPress = true;
			SaveSettings();
			ESP.restart();
		}
		else if ((current_high1 - current_low1) >= 20000 && (current_high1 - current_low1) < 60000)
		{
			Settings.reallyLongPress = true;
			SaveSettings();
			ESP.restart();
		}
	}
}


#if defined SONOFF 
void extRelayToggle() {
	//' TTTT 
	Serial.println("extRelayToggle vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv ");
	Serial.print("Settings.currentState1: ");
	Serial.println(Settings.currentState1);


	if (Settings.externalType > 0) {
		int reading = digitalRead(EXT_PIN);

		Serial.print("EXT_PIN: ");
		Serial.println(reading);
		Serial.print("state_ext: ");
		Serial.println(state_ext);

		if (reading == LOW) {
			if (state_ext == HIGH)
			{
				//' HIGH
				current_low_ext = millis();
				state_ext = LOW;
				if ((current_low_ext - current_high_ext) > (Settings.debounce ? Settings.debounce : debounceDelay)) {
					relayControl(1, 2);
				}
				else
				{
					Serial.println("LOWLOWLOW not called ======");
					Serial.println(current_low_ext);
					Serial.println(current_high_ext);
					Serial.println(Settings.debounce);
					Serial.println(debounceDelay);
					Serial.println("not called ======");
					//Settings.debounce
				}
			}
			else
			{
				//' LOW
			}
		}
		if (reading == HIGH)
		{
			if (state_ext == LOW)
			{
				current_high_ext = millis();
				state_ext = HIGH;
				if ((current_high_ext - current_low_ext) > (Settings.debounce ? Settings.debounce : debounceDelay))
				{
					if (Settings.externalType == 4) {
						relayControl(1, 2);
					}
				}
				else
				{
					Serial.println("HIGHHIGHHIGH not called ======");
					Serial.println(current_low_ext);
					Serial.println(current_high_ext);
					Serial.println(Settings.debounce);
					Serial.println(debounceDelay);
					Serial.println("not called ======");
					//Settings.debounce
				}
			}
			else
			{
			}
		}

	}
	else {
		//External switch has been disabled
	}

	Serial.print(".......... existing. Last state_ext: ");
	Serial.println(state_ext);
	Serial.println("extRelayToggle ^^^^^^^^^^^^^^^^^^^^^^^^^^");
}
#endif

const char * endString(int s, const char *input) {
	int length = strlen(input);
	if (s > length) s = length;
	return const_cast<const char *>(&input[length - s]);
}


String getStatus() {
	return "{\"power\":\"" + String(digitalRead(REL_PIN1) == 0 ? "off" : "on") + "\", \"uptime\":\"" + uptime() + "\"}";
}



/*********************************************************************************************\
   Tasks each 5 seconds
  \*********************************************************************************************/
void runEach1Seconds()
{
	timer1s = millis() + 1000;
}


/*********************************************************************************************\
   Tasks each 5 seconds
  \*********************************************************************************************/
void runEach5Seconds()
{
	timer5s = millis() + 5000;
	//'Serial.println("runEach5Seconds");
	// sendStatusToRemote();
	//' sendTimeToHub();
}

/*********************************************************************************************\
   Tasks each 1 minutes
  \*********************************************************************************************/
void runEach1Minutes()
{
	timer1m = millis() + 60000;

	if (SecuritySettings.Password[0] != 0)
	{
		if (WebLoggedIn)
			WebLoggedInTimer++;
		if (WebLoggedInTimer > 2)
			WebLoggedIn = false;
	}
}

/*********************************************************************************************\
   Tasks each 5 minutes
  \*********************************************************************************************/
void runEach5Minutes()
{
	timer5m = millis() + 300000;
	sendTimeToHub();

	//sendStatus(99);

}

boolean sendStatus(int number) {
	String authHeader = "";
	boolean success = false;
	String message = "";
	char host[20];
	sprintf_P(host, PSTR("%u.%u.%u.%u"), Settings.haIP[0], Settings.haIP[1], Settings.haIP[2], Settings.haIP[3]);

	//client.setTimeout(1000);
	if (Settings.haIP[0] + Settings.haIP[1] + Settings.haIP[2] + Settings.haIP[3] == 0) { // HA host is not configured
		Serial.println("!!!! haIP not set!!!!. Set that to 192.168.100.185");
		return false;
	}
	if (connectionFailures >= 3) { // Too many errors; Trying not to get stuck
		if (millis() - failureTimeout < 1800000) {
			return false;
		}
		else {
			failureTimeout = millis();
		}
	}
	// Use WiFiClient class to create TCP connections
	WiFiClient client;
	if (!client.connect(host, Settings.haPort))
	{
		connectionFailures++;
		return false;
	}
	if (connectionFailures)
		connectionFailures = 0;

	switch (number) {
		case 0: {
			message = "{\"type\":\"relay\", \"number\":\"0\", \"power\":\"" + String(Settings.currentState1 == true ? "on" : "off") + "\"}";
			break;
		}
		case 1: {
			message = "{\"type\":\"relay\", \"number\":\"1\", \"power\":\"" + String(Settings.currentState1 == true ? "on" : "off") + "\"}";
			break;
		}


		case 99: {
			message = "{\"uptime\":\"" + uptime() + ", " + String(currentDateTime) + "\"}";
			break;
		}
		case 4000:{
				message = "{\"type\":\"InfoXYZ\", \"macAddress\":\"" + String(macAddress)+ "\", \"currentDateTime\":\"" + String(currentDateTime) + "\", \"ip\":\"" + String(localIP) + "\"}";

				break;
		}

	}

	Serial.println("message: " + String(message));

	// We now create a URI for the request
	String url = F("/");
	//url += event->idx;

	client.print(String("POST ") + url + " HTTP/1.1\r\n" +
		"Host: " + host + ":" + Settings.haPort + "\r\n" + authHeader +
		"Content-Type: application/json;charset=utf-8\r\n" +
		"Content-Length: " + message.length() + "\r\n" +
		"Server: " + projectName + "\r\n" +
		"Connection: close\r\n\r\n" +
		message + "\r\n");

	unsigned long timer = millis() + 200;
	while (!client.available() && millis() < timer)
		delay(1);

	// Read all the lines of the reply from server and print them to Serial
	while (client.available()) {
		String line = client.readStringUntil('\n');
		if (line.substring(0, 15) == "HTTP/1.1 200 OK")
		{
			success = true;
		}
		delay(1);
	}

	client.flush();
	client.stop();

	return success;
}

boolean sendReport(int number) {
	String authHeader = "";
	boolean success = false;
	char host[20];
	const char* report;
	float value;


	sprintf_P(host, PSTR("%u.%u.%u.%u"), Settings.haIP[0], Settings.haIP[1], Settings.haIP[2], Settings.haIP[3]);

	//client.setTimeout(1000);
	if (Settings.haIP[0] + Settings.haIP[1] + Settings.haIP[2] + Settings.haIP[3] == 0) { // HA host is not configured
		return false;
	}
	if (connectionFailures >= 3) { // Too many errors; Trying not to get stuck
		if (millis() - failureTimeout < 1800000) {
			return false;
		}
		else {
			failureTimeout = millis();
		}
	}
	// Use WiFiClient class to create TCP connections
	WiFiClient client;
	if (!client.connect(host, Settings.haPort))
	{
		connectionFailures++;
		return false;
	}
	if (connectionFailures)
		connectionFailures = 0;

	// We now create a URI for the request
	String url = F("/");
	//url += event->idx;

#if not defined SONOFF_TH
	String PostData = "{\"" + String(report) + "\":\"" + String(value) + "\"}" + "\r\n";
	client.print(String("POST ") + url + " HTTP/1.1\r\n" +
		"Host: " + host + ":" + Settings.haPort + "\r\n" + authHeader +
		"Content-Type: application/json;charset=utf-8\r\n" +
		"Content-Length: " + PostData.length() + "\r\n" +
		"Server: " + projectName + "\r\n" +
		"Connection: close\r\n\r\n" +
		PostData);
#endif

	unsigned long timer = millis() + 200;
	while (!client.available() && millis() < timer)
		delay(1);

	// Read all the lines of the reply from server and print them to Serial
	while (client.available()) {
		String line = client.readStringUntil('\n');
		if (line.substring(0, 15) == "HTTP/1.1 200 OK")
		{
			success = true;
		}
		delay(1);
	}

	client.flush();
	client.stop();

	return success;
}


/********************************************************************************************\
  Convert a char string to IP byte array
  \*********************************************************************************************/
boolean str2ip(char *string, byte* IP)
{
	byte c;
	byte part = 0;
	int value = 0;

	for (int x = 0; x <= strlen(string); x++)
	{
		c = string[x];
		if (isdigit(c))
		{
			value *= 10;
			value += c - '0';
		}

		else if (c == '.' || c == 0) // next octet from IP address
		{
			if (value <= 255)
				IP[part++] = value;
			else
				return false;
			value = 0;
		}
		else if (c == ' ') // ignore these
			;
		else // invalid token
			return false;
	}
	if (part == 4) // correct number of octets
		return true;
	return false;
}

String deblank(const char* input)
{
	String output = String(input);
	output.replace(" ", "");
	return output;
}

void SaveSettings(void)
{
	SaveToFlash(0, (byte*)&Settings, sizeof(struct SettingsStruct));
	SaveToFlash(32768, (byte*)&SecuritySettings, sizeof(struct SecurityStruct));
}

boolean LoadSettings()
{
	LoadFromFlash(0, (byte*)&Settings, sizeof(struct SettingsStruct));
	LoadFromFlash(32768, (byte*)&SecuritySettings, sizeof(struct SecurityStruct));
}

/********************************************************************************************\
  Save data to flash
  \*********************************************************************************************/
void SaveToFlash(int index, byte* memAddress, int datasize)
{
	if (index > 33791) // Limit usable flash area to 32+1k size
	{
		return;
	}
	uint32_t _sector = ((uint32_t)&_SPIFFS_start - 0x40200000) / SPI_FLASH_SEC_SIZE;
	uint8_t* data = new uint8_t[FLASH_EEPROM_SIZE];
	int sectorOffset = index / SPI_FLASH_SEC_SIZE;
	int sectorIndex = index % SPI_FLASH_SEC_SIZE;
	uint8_t* dataIndex = data + sectorIndex;
	_sector += sectorOffset;

	// load entire sector from flash into memory
	noInterrupts();
	spi_flash_read(_sector * SPI_FLASH_SEC_SIZE, reinterpret_cast<uint32_t*>(data), FLASH_EEPROM_SIZE);
	interrupts();

	// store struct into this block
	memcpy(dataIndex, memAddress, datasize);

	noInterrupts();
	// write sector back to flash
	if (spi_flash_erase_sector(_sector) == SPI_FLASH_RESULT_OK)
		if (spi_flash_write(_sector * SPI_FLASH_SEC_SIZE, reinterpret_cast<uint32_t*>(data), FLASH_EEPROM_SIZE) == SPI_FLASH_RESULT_OK)
		{
			//Serial.println("flash save ok");
		}
	interrupts();
	delete[] data;
	//String log = F("FLASH: Settings saved");
	//addLog(LOG_LEVEL_INFO, log);
}

/********************************************************************************************\
  Load data from flash
  \*********************************************************************************************/
void LoadFromFlash(int index, byte* memAddress, int datasize)
{
	uint32_t _sector = ((uint32_t)&_SPIFFS_start - 0x40200000) / SPI_FLASH_SEC_SIZE;
	uint8_t* data = new uint8_t[FLASH_EEPROM_SIZE];
	int sectorOffset = index / SPI_FLASH_SEC_SIZE;
	int sectorIndex = index % SPI_FLASH_SEC_SIZE;
	uint8_t* dataIndex = data + sectorIndex;
	_sector += sectorOffset;

	// load entire sector from flash into memory
	noInterrupts();
	spi_flash_read(_sector * SPI_FLASH_SEC_SIZE, reinterpret_cast<uint32_t*>(data), FLASH_EEPROM_SIZE);
	interrupts();

	// load struct from this block
	memcpy(memAddress, dataIndex, datasize);
	delete[] data;
}

void EraseFlash()
{
	uint32_t _sectorStart = (ESP.getSketchSize() / SPI_FLASH_SEC_SIZE) + 1;
	uint32_t _sectorEnd = _sectorStart + (ESP.getFlashChipRealSize() / SPI_FLASH_SEC_SIZE);

	for (uint32_t _sector = _sectorStart; _sector < _sectorEnd; _sector++)
	{
		noInterrupts();
		if (spi_flash_erase_sector(_sector) == SPI_FLASH_RESULT_OK)
		{
			interrupts();
			//Serial.print(F("FLASH: Erase Sector: "));
			//Serial.println(_sector);
			delay(10);
		}
		interrupts();
	}
}

void ZeroFillFlash()
{
	// this will fill the SPIFFS area with a 64k block of all zeroes.
	uint32_t _sectorStart = ((uint32_t)&_SPIFFS_start - 0x40200000) / SPI_FLASH_SEC_SIZE;
	uint32_t _sectorEnd = _sectorStart + 16; //((uint32_t)&_SPIFFS_end - 0x40200000) / SPI_FLASH_SEC_SIZE;
	uint8_t* data = new uint8_t[FLASH_EEPROM_SIZE];

	uint8_t* tmpdata = data;
	for (int x = 0; x < FLASH_EEPROM_SIZE; x++)
	{
		*tmpdata = 0;
		tmpdata++;
	}


	for (uint32_t _sector = _sectorStart; _sector < _sectorEnd; _sector++)
	{
		// write sector to flash
		noInterrupts();
		if (spi_flash_erase_sector(_sector) == SPI_FLASH_RESULT_OK)
			if (spi_flash_write(_sector * SPI_FLASH_SEC_SIZE, reinterpret_cast<uint32_t*>(data), FLASH_EEPROM_SIZE) == SPI_FLASH_RESULT_OK)
			{
				interrupts();
				//Serial.print(F("FLASH: Zero Fill Sector: "));
				//Serial.println(_sector);
				delay(10);
			}
	}
	interrupts();
	delete[] data;
}

String uptime()
{
		currentDateTime = NTP.getTimeDateString();
	currentmillis = millis();
	long days = 0;
	long hours = 0;
	long mins = 0;
	long secs = 0;
	secs = currentmillis / 1000; //convect milliseconds to seconds
	mins = secs / 60; //convert seconds to minutes
	hours = mins / 60; //convert minutes to hours
	days = hours / 24; //convert hours to days
	secs = secs - (mins * 60); //subtract the coverted seconds to minutes in order to display 59 secs max
	mins = mins - (hours * 60); //subtract the coverted minutes to hours in order to display 59 minutes max
	hours = hours - (days * 24); //subtract the coverted hours to days in order to display 23 hours max

	if (days > 0) // days will displayed only if value is greater than zero
	{
		return String(days) + " days and " + String(hours) + ":" + String(mins) + ":" + String(secs) + ", " + String(currentDateTime);
	}
	else {
		return String(hours) + ":" + String(mins) + ":" + String(secs)  + ", " + String(currentDateTime);
	}
}

unsigned char h2int(char c)
{
    if (c >= '0' && c <='9'){
        return((unsigned char)c - '0');
    }
    if (c >= 'a' && c <='f'){
        return((unsigned char)c - 'a' + 10);
    }
    if (c >= 'A' && c <='F'){
        return((unsigned char)c - 'A' + 10);
    }
    return(0);
}

String urldecode(String str)
{
    
    String encodedString="";
    char c;
    char code0;
    char code1;
    for (int i =0; i < str.length(); i++){
        c=str.charAt(i);
      if (c == '+'){
        encodedString+=' ';  
      }else if (c == '%') {
        i++;
        code0=str.charAt(i);
        i++;
        code1=str.charAt(i);
        c = (h2int(code0) << 4) | h2int(code1);
        encodedString+=c;
      } else{
        
        encodedString+=c;  
      }
      
      yield();
    }
    
   return encodedString;
}

String urlencode(String str)
{
    String encodedString="";
    char c;
    char code0;
    char code1;
    char code2;
    for (int i =0; i < str.length(); i++){
      c=str.charAt(i);
      if (c == ' '){
        encodedString+= '+';
      } else if (isalnum(c)){
        encodedString+=c;
      } else{
        code1=(c & 0xf)+'0';
        if ((c & 0xf) >9){
            code1=(c & 0xf) - 10 + 'A';
        }
        c=(c>>4)&0xf;
        code0=c+'0';
        if (c > 9){
            code0=c - 10 + 'A';
        }
        code2='\0';
        encodedString+='%';
        encodedString+=code0;
        encodedString+=code1;
        //encodedString+=code2;
      }
      yield();
    }
    return encodedString;
    
}

int timezone = 3;
int dst = 0;

void setupTime()
{
  //configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
	//configTime(TZ_SEC, DST_SEC, "pool.ntp.org");
  //Serial.println("\nWaiting for time");
  //while (!time(nullptr)) {
  //  Serial.print(".");
  //  delay(1000);
  //}
	Serial.println("@@@@@@@@@@@@@@ getting NTP time ");
	NTP.begin("pool.ntp.org", timeZone, true, minutesTimeZone);
	Serial.println("@@@@@@@@@@@@ NTP ==> " + String(NTP.getTimeDateString()));
	Serial.println("");

}
void setupServer()
{
	server->on("/description.xml", HTTP_GET, []() {
		SSDP.schema(server->client());
	});

	server->on("/reboot", []() {

		if (SecuritySettings.Password[0] != 0 && Settings.usePasswordControl == true)
		{
			if (!server->authenticate("admin", SecuritySettings.Password))
				return server->requestAuthentication();
		}
		server->send(200, "application/json", "{\"message\":\"device is rebooting\"}");
		Relayoff1;
		LEDoff1;
		delay(2000);
		ESP.restart();
	});

	server->on("/r", []() {

		if (SecuritySettings.Password[0] != 0 && Settings.usePasswordControl == true)
		{
			if (!server->authenticate("admin", SecuritySettings.Password))
				return server->requestAuthentication();
		}
		server->send(200, "text/html", "<META http-equiv=\"refresh\" content=\"15;URL=/\">Device is rebooting...");
		Relayoff1;
		LEDoff1;
		delay(2000);
		ESP.restart();
	});

	server->on("/reset", []() {
		server->send(200, "application/json", "{\"message\":\"wifi settings are being removed\"}");
		Settings.reallyLongPress = true;
		SaveSettings();
		ESP.restart();
	});

	server->on("/status", []() {
		sendTimeToHub();
		sendStatusToRemote();
		String status = getStatus();
		Serial.println("..........status ==> " + status);
		server->send(200, "application/json", status);
	});

	server->on("/info", []() {
		//server->send(200, "application/json", "{\"deviceType\":\"" + String(projectName) + "\", \"version\":\"" + softwareVersion + "\", \"date\":\"" + compile_date + "\", \"mac\":\"" + padHex(String(mac[0], HEX)) + padHex(String(mac[1], HEX)) + padHex(String(mac[2], HEX)) + padHex(String(mac[3], HEX)) + padHex(String(mac[4], HEX)) + padHex(String(mac[5], HEX)) + "\"}");

		//' https://github.com/esp8266/Arduino/blob/master/libraries/esp8266/examples/CheckFlashConfig/CheckFlashConfig.ino

		String realSize = String(ESP.getFlashChipRealSize());
		String ideSize = String(ESP.getFlashChipSize());
		FlashMode_t ideMode = ESP.getFlashChipMode();
		String flashRealId = String(ESP.getFlashChipId());
		String flashChipSpeed = String(ESP.getFlashChipSpeed());

		String ideModDesc = (ideMode == FM_QIO ? "QIO" : ideMode == FM_QOUT ? "QOUT" : ideMode == FM_DIO ? "DIO" : ideMode == FM_DOUT ? "DOUT" : "UNKNOWN");

		String json = "{\"realSize\":\"" + String(realSize) + "\", \"ideSize\":\"" + String(ideSize) + "\", \"ideModDesc\":\"" + String(ideModDesc) + "\", \"flashRealId\":\"" + String(flashRealId) + "\", \"flashChipSpeed\":\"" + String(flashChipSpeed) + "\", \"deviceType\":\"" + String(projectName) + "\", \"version\":\"" + softwareVersion + "\", \"date\":\"" + compile_date + "\", \"mac\":\"" + padHex(String(mac[0], HEX)) + padHex(String(mac[1], HEX)) + padHex(String(mac[2], HEX)) + padHex(String(mac[3], HEX)) + padHex(String(mac[4], HEX)) + padHex(String(mac[5], HEX)) + "\"}";

		server->send(200, "application/json", json);
		//' Serial.printf("Flash ide mode:  %s\n", (ideMode == FM_QIO ? "QIO" : ideMode == FM_QOUT ? "QOUT" : ideMode == FM_DIO ? "DIO" : ideMode == FM_DOUT ? "DOUT" : "UNKNOWN"));

		//if (ideSize != realSize) {
		//	Serial.println("Flash Chip configuration wrong!\n");
		//}
		//else {
		//	Serial.println("Flash Chip configuration ok.\n");
		//}

	});

	server->on("/advanced", []() {

		if (SecuritySettings.Password[0] != 0 && Settings.usePassword == true)
		{
			if (!server->authenticate("admin", SecuritySettings.Password))
				return server->requestAuthentication();
		}

		char tmpString[64];
		String haIP = server->arg("haip");
		String haPort = server->arg("haport");
		String powerOnState = server->arg("pos");
		String ip = server->arg("ip");
		String gateway = server->arg("gateway");
		String subnet = server->arg("subnet");
		String dns = server->arg("dns");
		String usestatic = server->arg("usestatic");
		String usepassword = server->arg("usepassword");
		String usepasswordcontrol = server->arg("usepasswordcontrol");
		String username = server->arg("username");
		String password = server->arg("password");
		String port = server->arg("port");
		String autoOff1 = server->arg("autooff1");
		String uReport = server->arg("ureport");
		String debounce = server->arg("debounce");
		String hostname = server->arg("hostname");
#if defined SONOFF 
		String switchType = server->arg("switchtype");
		String externalType = server->arg("externaltype");
#endif
		String remoteIP = server->arg("remoteIP");
		String description = server->arg("description");
		
		String reportToRemote = server->arg("reportToRemote");


		if (server->args() > 0) {
			if (haPort.length() != 0)
			{
				Settings.haPort = haPort.toInt();
			}
			if (powerOnState.length() != 0)
			{
				Settings.powerOnState = powerOnState.toInt();
			}
			if (haIP.length() != 0)
			{
				haIP.toCharArray(tmpString, 26);
				str2ip(tmpString, Settings.haIP);
			}

			if (ip.length() != 0 && subnet.length() != 0)
			{
				if (ip != String(Settings.IP[0]) + "." + String(Settings.IP[1]) + "." + String(Settings.IP[2]) + "." + String(Settings.IP[3]) && Settings.useStatic) needReboot = true;
				ip.toCharArray(tmpString, 26);
				str2ip(tmpString, Settings.IP);
				if (subnet != String(Settings.Subnet[0]) + "." + String(Settings.Subnet[1]) + "." + String(Settings.Subnet[2]) + "." + String(Settings.Subnet[3]) && Settings.useStatic) needReboot = true;
				subnet.toCharArray(tmpString, 26);
				str2ip(tmpString, Settings.Subnet);
			}
			if (gateway.length() != 0)
			{
				gateway.toCharArray(tmpString, 26);
				str2ip(tmpString, Settings.Gateway);
			}
			if (dns.length() != 0)
			{
				dns.toCharArray(tmpString, 26);
				str2ip(tmpString, Settings.DNS);
			}
			if (usestatic.length() != 0)
			{
				if ((usestatic == "yes") != Settings.useStatic) needReboot = true;
				Settings.useStatic = (usestatic == "yes");
			}
			if (usepassword.length() != 0)
			{
				if ((usepassword == "yes") != Settings.usePassword) needReboot = true;
				Settings.usePassword = (usepassword == "yes");
			}
			if (usepasswordcontrol.length() != 0)
			{
				Settings.usePasswordControl = (usepasswordcontrol == "yes");
			}
			if (password != SecuritySettings.Password && Settings.usePassword) needReboot = true;
			strncpy(SecuritySettings.Password, password.c_str(), sizeof(SecuritySettings.Password));
			if (port.length() != 0)
			{
				//if(port.toInt() != Settings.usePort) needReboot = true;
				Settings.usePort = port.toInt();
			}
			if (autoOff1.length() != 0)
			{
				Settings.autoOff1 = autoOff1.toInt();
			}
			if (uReport.length() != 0)
			{
				if (uReport.toInt() != Settings.uReport) {
					Settings.uReport = uReport.toInt();
					timerUptime = millis() + Settings.uReport * 1000;
				}
			}
			if (debounce.length() != 0)
			{
				Settings.debounce = debounce.toInt();
			}
			if (hostname != Settings.hostName) needReboot = true;
			WiFi.hostname(hostname.c_str());
			strncpy(Settings.hostName, hostname.c_str(), sizeof(Settings.hostName));
#if defined SONOFF 
			if (externalType.length() != 0)
			{
				if (externalType.toInt() != Settings.externalType) needReboot = true;
				Settings.externalType = externalType.toInt();
			}
#endif

			if (reportToRemote.length() != 0)
			{
				Settings.reportToRemote = (reportToRemote == "yes");
			}
			if (remoteIP != Settings.remoteIP && Settings.reportToRemote) needReboot = true;
			strncpy(Settings.remoteIP, remoteIP.c_str(), sizeof(Settings.remoteIP));

			strncpy(Settings.description, description.c_str(), sizeof(Settings.description));

		}

		SaveSettings();

		String reply = "";
		char str[20];
		addHeader(true, reply);

		reply += F("<script src='http://ajax.googleapis.com/ajax/libs/jquery/1/jquery.min.js'></script>");

		reply += F("<form name='frmselect' class='form' method='post'><table>");
		reply += F("<TH colspan='2'>");
		reply += projectName;
		reply += F(" Settings");
		reply += F("<TR><TD><TD><TR><TD colspan='2' align='center'>");
		addMenu(reply);
		addRebootBanner(reply);

		reply += F("<TR><TD>Host Name:<TD><input type='text' id='hostname' name='hostname' value='");
		reply += WiFi.hostname();
		reply += F("'>");

		reply += F("<TR><TD>Description:<TD><input type='text' id='description' name='description' value='");
		reply += Settings.description;
		reply += F("'>");


		//' report to remote
		reply += F("<TR><TD>Remote IP<TD><input type='text' id='remoteIP' name='remoteIP' value='");
		reply += Settings.remoteIP;
		reply += F("'>");

		reply += F("<TR><TD>Report to remote:<TD>");
		reply += F("<input type='radio' name='reportToRemote' value='yes'");
		if (Settings.reportToRemote)
			reply += F(" checked ");
		reply += F(">Yes");
		reply += F("</input>");

		reply += F("<input type='radio' name='reportToRemote' value='no'");
		if (!Settings.reportToRemote)
			reply += F(" checked ");
		reply += F(">No");
		reply += F("</input>");

		reply += F("<TR><TD><TD><TR><TD>Password Protect<BR><BR>Configuration:<TD><BR><BR>");

		reply += F("<input type='radio' name='usepassword' value='yes'");
		if (Settings.usePassword)
			reply += F(" checked ");
		reply += F(">Yes");
		reply += F("</input>");

		reply += F("<input type='radio' name='usepassword' value='no'");
		if (!Settings.usePassword)
			reply += F(" checked ");
		reply += F(">No");
		reply += F("</input>");

		reply += F("<TR><TD>Control:<TD>");

		reply += F("<input type='radio' name='usepasswordcontrol' value='yes'");
		if (Settings.usePasswordControl)
			reply += F(" checked ");
		reply += F(">Yes");
		reply += F("</input>");

		reply += F("<input type='radio' name='usepasswordcontrol' value='no'");
		if (!Settings.usePasswordControl)
			reply += F(" checked ");
		reply += F(">No");
		reply += F("</input>");

		reply += F("<TR><TD>\"admin\" Password:<TD><input type='password' id='user_password' name='password' value='");
		SecuritySettings.Password[25] = 0;
		reply += SecuritySettings.Password;

		reply += F("'><input type='checkbox' id='showPassword' name='show' value='Show'> Show");

		reply += F("<script type='text/javascript'>");

		reply += F("$(\"#showPassword\").click(function() {");
		reply += F("var showPasswordCheckBox = document.getElementById(\"showPassword\");");
		reply += F("$('.form').find(\"#user_password\").each(function() {");
		reply += F("if(showPasswordCheckBox.checked){");
		reply += F("$(\"<input type='text' />\").attr({ name: this.name, value: this.value, id: this.id}).insertBefore(this);");
		reply += F("}else{");
		reply += F("$(\"<input type='password' />\").attr({ name: this.name, value: this.value, id: this.id }).insertBefore(this);");
		reply += F("}");
		reply += F("}).remove();");
		reply += F("});");

		reply += F("$(document).ready(function() {");
		reply += F("$(\"#user_password_checkbox\").click(function() {");
		reply += F("if ($('input.checkbox_check').attr(':checked')); {");
		reply += F("$(\"#user_password\").attr('type', 'text');");
		reply += F("}});");
		reply += F("});");

		reply += F("</script>");

		reply += F("<TR><TD>Static IP:<TD>");

		reply += F("<input type='radio' name='usestatic' value='yes'");
		if (Settings.useStatic)
			reply += F(" checked ");
		reply += F(">Yes");
		reply += F("</input>");

		reply += F("<input type='radio' name='usestatic' value='no'");
		if (!Settings.useStatic)
			reply += F(" checked ");
		reply += F(">No");
		reply += F("</input>");


		reply += F("<TR><TD>IP:<TD><input type='text' name='ip' value='");
		sprintf_P(str, PSTR("%u.%u.%u.%u"), Settings.IP[0], Settings.IP[1], Settings.IP[2], Settings.IP[3]);
		reply += str;

		reply += F("'><TR><TD>Subnet:<TD><input type='text' name='subnet' value='");
		sprintf_P(str, PSTR("%u.%u.%u.%u"), Settings.Subnet[0], Settings.Subnet[1], Settings.Subnet[2], Settings.Subnet[3]);
		reply += str;

		reply += F("'><TR><TD>Gateway:<TD><input type='text' name='gateway' value='");
		sprintf_P(str, PSTR("%u.%u.%u.%u"), Settings.Gateway[0], Settings.Gateway[1], Settings.Gateway[2], Settings.Gateway[3]);
		reply += str;

		//reply += F("'><TR><TD>DNS:<TD><input type='text' name='dns' value='");
		//sprintf_P(str, PSTR("%u.%u.%u.%u"), Settings.DNS[0], Settings.DNS[1], Settings.DNS[2], Settings.DNS[3]);
		//reply += str;

		reply += F("'><TR><TD>HA Controller IP:<TD><input type='text' name='haip' value='");
		sprintf_P(str, PSTR("%u.%u.%u.%u"), Settings.haIP[0], Settings.haIP[1], Settings.haIP[2], Settings.haIP[3]);
		reply += str;

		reply += F("'><TR><TD>HA Controller Port:<TD><input type='text' name='haport' value='");
		reply += Settings.haPort;


		byte choice = Settings.powerOnState;
		reply += F("'><TR><TD>Boot Up State:<TD><select name='");
		reply += "pos";
		reply += "'>";
		if (choice == 0) {
			reply += F("<option value='0' selected>Off</option>");
		}
		else {
			reply += F("<option value='0'>Off</option>");
		}
		if (choice == 1) {
			reply += F("<option value='1' selected>On</option>");
		}
		else {
			reply += F("<option value='1'>On</option>");
		}
		if (choice == 2) {
			reply += F("<option value='2' selected>Previous State</option>");
		}
		else {
			reply += F("<option value='2'>Previous State</option>");
		}
		if (choice == 3) {
			reply += F("<option value='3' selected>Inverse Previous State</option>");
		}
		else {
			reply += F("<option value='3'>Inverse Previous State</option>");
		}
		reply += F("</select>");
#if not defined SONOFF_4CH
		reply += F("<TR><TD>Auto Off:<TD><input type='text' name='autooff1' value='");
		reply += Settings.autoOff1;
		reply += F("'>");
#else
		reply += F("<TR><TD>Auto Off Relay 1:<TD><input type='text' name='autooff1' value='");
		reply += Settings.autoOff1;
		reply += F("'>");
#endif 



		reply += F("<TR><TD>Switch Debounce:<TD><input type='text' name='debounce' value='");
		reply += Settings.debounce;
		reply += F("'>");
#if defined SONOFF
		choice = Settings.externalType;
		reply += F("<TR><TD>External Device Type:<TD><select name='");
		reply += "externaltype";
		reply += "'>";

		if (choice == 1) {
			reply += F("<option value='1' selected>Temperature - AM2301</option>");
		}
		else {
			reply += F("<option value='1'>Temperature - AM2301</option>");
		}
		if (choice == 2) {
			reply += F("<option value='2' selected>Temperature - DS18B20</option>");
		}
		else {
			reply += F("<option value='2'>Temperature - DS18B20</option>");
		}
		if (choice == 3) {
			reply += F("<option value='3' selected>Switch - Momentary</option>");
		}
		else {
			reply += F("<option value='3'>Switch - Momentary</option>");
		}
		if (choice == 4) {
			reply += F("<option value='4' selected>Switch - Toggle</option>");
		}
		else {
			reply += F("<option value='4'>Switch - Toggle</option>");
		}

		if (choice == 0) {
			reply += F("<option value='0' selected>Disabled</option>");
		}
		else {
			reply += F("<option value='0'>Disabled</option>");
		}

		reply += F("</select>");
#endif

		reply += F("<TR><TD>Uptime Report Interval:<TD><input type='text' name='ureport' value='");
		reply += Settings.uReport;
		reply += F("'>");

		reply += F("<TR><TD><TD><input class=\"button-link\" type='submit' value='Submit'>");
		reply += F("</table></form>");
		addFooter(reply);
		server->send(200, "text/html", reply);
	});

	server->on("/control", []() {

		if (SecuritySettings.Password[0] != 0 && Settings.usePasswordControl == true)
		{
			if (!server->authenticate("admin", SecuritySettings.Password))
				return server->requestAuthentication();
		}

		char tmpString[64];
		String value = server->arg("relayValue");



		if (value == "on")
		{
			relayControl(1, 1);
		}
		else if (value == "off")
		{
			relayControl(1, 0);
		}


		String reply = "";
		char str[20];
		addHeader(true, reply);

		reply += F("<table>");
		reply += F("<TH colspan='2'>");
		reply += projectName;
		reply += F(" Control");
		reply += F("<TR><TD><TD><TR><TD colspan='2' align='center'>");
		addMenu(reply);
		addRebootBanner(reply);


		reply += F("<TR><TD><TD><TR><TD>Current State:");

		if (Settings.currentState1) {
			reply += F("<TD>ON<TD>");
		}
		else {
			reply += F("<TD>OFF<TD>");
		}
		reply += F("<TR><TD><form name='powerOn' method='post'><input type='hidden' name='relayValue' value='on'><input class=\"button-link\" type='submit' value='On'></form>");
		reply += F("<TD><form name='powerOff' method='post'><input type='hidden' name='relayValue' value='off'><input class=\"button-link\" type='submit' value='Off'></form>");


		reply += F("</table>");
		addFooter(reply);
		server->send(200, "text/html", reply);
	});

	server->on("/", []() {

		char tmpString[64];

		String reply = "";
		char str[20];
		addHeader(true, reply);
		reply += F("<table>");
		reply += F("<TH colspan='2'>");
		reply += projectName;
		reply += F(" Main");
		reply += F("<TR><TD><TD><TR><TD colspan='2' align='center'>");
		addMenu(reply);
		addRebootBanner(reply);

		reply += F("<TR><TD><TD><TR><TD>Main:");

		reply += F("<TD><a href='/advanced'>Advanced Config</a><BR>");
		reply += F("<a href='/control'>Relay Control</a><BR>");
		reply += F("<a href='/update'>Firmware Update</a><BR>");
		reply += F("<a href='http://tiny.cc/wnxady'>Documentation</a><BR>");
		reply += F("<a href='/r'>Reboot</a><BR>");

		reply += F("<TR><TD>JSON Endpoints:");

		reply += F("<TD><a href='/status'>status</a><BR>");
		//reply += F("<a href='/config'>config</a><BR>");
		reply += F("<a href='/configSet'>configSet</a><BR>");
		reply += F("<a href='/configGet'>configGet</a><BR>");
		reply += F("<a href='/on'>on</a><BR>");
		reply += F("<a href='/off'>off</a><BR>");

		reply += F("<a href='/info'>info</a><BR>");
		reply += F("<a href='/reboot'>reboot</a><BR>");

		reply += F("</table>");
		addFooter(reply);
		server->send(200, "text/html", reply);
	});

	server->on("/configGet", []() {

		if (SecuritySettings.Password[0] != 0 && Settings.usePassword == true)
		{
			if (!server->authenticate("admin", SecuritySettings.Password))
				return server->requestAuthentication();
		}

		char tmpString[64];
		boolean success = false;
		String configName = server->arg("name");
		String reply = "";
		char str[20];

		if (configName == "haip") {
			sprintf_P(str, PSTR("%u.%u.%u.%u"), Settings.haIP[0], Settings.haIP[1], Settings.haIP[2], Settings.haIP[3]);
			reply += "{\"name\":\"haip\", \"value\":\"" + String(str) + "\", \"success\":\"true\", \"type\":\"configuration\"}";
		}
		if (configName == "haport") {
			reply += "{\"name\":\"haport\", \"value\":\"" + String(Settings.haPort) + "\", \"success\":\"true\", \"type\":\"configuration\"}";
		}
		if (configName == "pos") {
			reply += "{\"name\":\"pos\", \"value\":\"" + String(Settings.powerOnState) + "\", \"success\":\"true\", \"type\":\"configuration\"}";
		}
		if (configName == "autooff1") {
			reply += "{\"name\":\"autooff1\", \"value\":\"" + String(Settings.autoOff1) + "\", \"success\":\"true\", \"type\":\"configuration\"}";
		}
		if (configName == "debounce") {
			reply += "{\"name\":\"debounce\", \"value\":\"" + String(Settings.debounce) + "\", \"success\":\"true\", \"type\":\"configuration\"}";
		}
		if (configName == "ureport") {
			reply += "{\"name\":\"ureport\", \"value\":\"" + String(Settings.uReport) + "\", \"success\":\"true\", \"type\":\"configuration\"}";
		}
#if defined SONOFF 
		if (configName == "externaltype") {
			reply += "{\"name\":\"externaltype\", \"value\":\"" + String(Settings.externalType) + "\", \"success\":\"true\", \"type\":\"configuration\"}";
		}
#endif

		if (reply != "") {
			server->send(200, "application/json", reply);
		}
		else {
			server->send(200, "application/json", "{\"success\":\"false\", \"type\":\"configuration\"}");
		}
	});

	server->on("/configSet", []() {

		if (SecuritySettings.Password[0] != 0 && Settings.usePassword == true)
		{
			if (!server->authenticate("admin", SecuritySettings.Password))
				return server->requestAuthentication();
		}

		char tmpString[64];
		boolean success = false;
		String configName = server->arg("name");
		String configValue = server->arg("value");
		String reply = "";
		char str[20];

		if (configName == "haip") {
			if (configValue.length() != 0)
			{
				configValue.toCharArray(tmpString, 26);
				str2ip(tmpString, Settings.haIP);
			}
			reply += "{\"name\":\"haip\", \"value\":\"" + String(tmpString) + "\", \"success\":\"true\"}";
		}
		if (configName == "haport") {
			if (configValue.length() != 0)
			{
				Settings.haPort = configValue.toInt();
			}
			reply += "{\"name\":\"haport\", \"value\":\"" + String(Settings.haPort) + "\", \"success\":\"true\", \"type\":\"configuration\"}";
		}
		if (configName == "pos") {
			if (configValue.length() != 0)
			{
				Settings.powerOnState = configValue.toInt();
			}
			reply += "{\"name\":\"pos\", \"value\":\"" + String(Settings.powerOnState) + "\", \"success\":\"true\", \"type\":\"configuration\"}";
		}
		if (configName == "autooff1") {
			if (configValue.length() != 0)
			{
				Settings.autoOff1 = configValue.toInt();
			}
			reply += "{\"name\":\"autooff1\", \"value\":\"" + String(Settings.autoOff1) + "\", \"success\":\"true\", \"type\":\"configuration\"}";
		}
		if (configName == "ureport") {
			if (configValue.length() != 0)
			{
				if (configValue.toInt() != Settings.uReport) {
					Settings.uReport = configValue.toInt();
					timerUptime = millis() + Settings.uReport * 1000;
				}
			}
			reply += "{\"name\":\"ureport\", \"value\":\"" + String(Settings.uReport) + "\", \"success\":\"true\", \"type\":\"configuration\"}";
		}
		if (configName == "debounce") {
			if (configValue.length() != 0)
			{
				Settings.debounce = configValue.toInt();
			}
			reply += "{\"name\":\"debounce\", \"value\":\"" + String(Settings.debounce) + "\", \"success\":\"true\", \"type\":\"configuration\"}";
		}
#if defined SONOFF 
		if (configName == "externaltype") {
			if (configValue.length() != 0)
			{
				Settings.externalType = configValue.toInt();
			}
			reply += "{\"name\":\"externaltype\", \"value\":\"" + String(Settings.externalType) + "\", \"success\":\"true\", \"type\":\"configuration\"}";
		}
#endif


		if (reply != "") {
			SaveSettings();
			server->send(200, "application/json", reply);
		}
		else {
			server->send(200, "application/json", "{\"success\":\"false\", \"type\":\"configuration\"}");
		}
	});


	server->on("/off", []() {

		if (SecuritySettings.Password[0] != 0 && Settings.usePasswordControl == true)
		{
			if (!server->authenticate("admin", SecuritySettings.Password))
				return server->requestAuthentication();
		}

		relayControl(0, 0);
		String stateJSON = "{\"type\":\"relay\", \"number\":\"1\", \"power\":\"" + String(Settings.currentState1 ? "on" : "off") + "\"}";
		Serial.println("..... stateJSON => " + stateJSON);
		server->send(200, "application/json", stateJSON);
	});




	server->on("/on", []() {

		if (SecuritySettings.Password[0] != 0 && Settings.usePasswordControl == true)
		{
			if (!server->authenticate("admin", SecuritySettings.Password))
				return server->requestAuthentication();
		}

		relayControl(0, 1);
		String stateJSON = "{\"type\":\"relay\", \"number\":\"1\", \"power\":\"" + String(Settings.currentState1 ? "on" : "off") + "\"}";
		Serial.println("..... stateJSON => " + stateJSON);
		server->send(200, "application/json", stateJSON);
	});


	if (ESP.getFlashChipRealSize() > 524288) {
		if (Settings.usePassword == true && SecuritySettings.Password[0] != 0) {
			httpUpdater.setup(&*server, "/update", "admin", SecuritySettings.Password);
			//' httpUpdater.setProjectName(projectName);
		}
		else {
			httpUpdater.setup(&*server);
			//' httpUpdater.setProjectName(projectName);
		}
	}

	server->onNotFound(handleNotFound);

	server->begin();
}

void sendTimeToHub()
{
	
	currentDateTime = NTP.getTimeDateString();
	Serial.println("......sendTimeToHub: "+ String(currentDateTime));
	sendStatus(4000);
}
HTTPClient http;
void sendStatusToRemote()
{
	if (!Settings.reportToRemote)
	{
		Serial.println("reportToRemote not set..........");
		return;
	}

	String  postData = "status=HereHere&MAC=" + urlencode(macAddress) + "&IP=" + urlencode(localIP);

	String remoteUrl = "http://" + String(Settings.remoteIP) + "/status/report"; //'"http://40.112.57.194/status/report"
	Serial.println("..............sendStatusToRemote......" + String(remoteUrl));

	http.begin(remoteUrl);              //Specify request destination
	http.addHeader("Content-Type", "application/x-www-form-urlencoded");    //Specify content-type header

	int httpCode = http.POST(postData);   //Send the request
	remoteServerResponse = http.getString();    //Get the response payload

	Serial.println(httpCode);   //Print HTTP return code
	if (httpCode == 200)
	{
		Serial.println(remoteServerResponse);    //Print request response payload
	}
	else
	{
		Serial.println("http response error!!! httpCode: " + String(httpCode));
		Serial.println("http response error!!! remoteServerResponse: " + String(remoteServerResponse));
		//' put quotes around it so that it will be the same as http.getString()
		//' remoteServerResponse="httpCode: \"" + String(httpCode) + "\"";
		remoteServerResponse= "\"HTTP ERROR - " + String(millis()) + "\"";
	}
	sendStatus(3000);

	http.end();  //Close connection
}

void checkFlashConfig()
{
  uint32_t realSize = ESP.getFlashChipRealSize();
  uint32_t ideSize = ESP.getFlashChipSize();
  FlashMode_t ideMode = ESP.getFlashChipMode();

  Serial.printf("Flash real id:   %08X\n", ESP.getFlashChipId());
  Serial.printf("Flash real size: %u bytes\n\n", realSize);

  Serial.printf("Flash ide  size: %u bytes\n", ideSize);
  Serial.printf("Flash ide speed: %u Hz\n", ESP.getFlashChipSpeed());
  Serial.printf("Flash ide mode:  %s\n", (ideMode == FM_QIO ? "QIO" : ideMode == FM_QOUT ? "QOUT" : ideMode == FM_DIO ? "DIO" : ideMode == FM_DOUT ? "DOUT" : "UNKNOWN"));

  if (ideSize != realSize) {
    Serial.println("Flash Chip configuration wrong!\n");
  } else {
    Serial.println("Flash Chip configuration ok.\n");
  }

  
}

void setupOthers()
{
	LoadSettings();


	if (Settings.externalType == 3 || Settings.externalType == 4) {
		pinMode(EXT_PIN, INPUT_PULLUP);
		attachInterrupt(EXT_PIN, extRelayToggle, CHANGE);
	}
	else if (Settings.externalType == 2) {
		dsSetup();
	}
	else if (Settings.externalType == 1) {
		dht.begin();
	}


	if (Settings.longPress == true) {
		for (uint8_t i = 0; i < 3; i++) {
			LEDoff1;
			delay(250);
			LEDon1;
			delay(250);
		}
		Settings.longPress = false;
		Settings.useStatic = false;
		Settings.resetWifi = true;
		SaveSettings();
		LEDoff1;
	}
	//Settings.reallyLongPress = true;
	if (Settings.reallyLongPress == true) {
		for (uint8_t i = 0; i < 5; i++) {
			LEDoff1;
			delay(1000);
			LEDon1;
			delay(1000);
		}
		EraseFlash();
		ZeroFillFlash();
		ESP.restart();
	}



	Serial.print("Settings.externalType: ");
	Serial.println(Settings.externalType);
	Serial.print("Settings.currentState1: ");
	Serial.println(Settings.currentState1);
	//
	Serial.println("---------------------------------");

	switch (Settings.powerOnState)
	{
	case 0: //Switch Off on Boot
	{
		relayControl(0, 0);
		break;
	}
	case 1: //Switch On on Boot
	{
		relayControl(0, 1);
		break;
	}
	case 2: //Saved State on Boot
	{
		if (Settings.currentState1) relayControl(1, 1);
		else relayControl(1, 0);
		break;
	}
	case 3: //Opposite Saved State on Boot
	{
		if (!Settings.currentState1) relayControl(1, 1);
		else relayControl(1, 0);
		break;
	}
	default: //Optional
	{
		relayControl(0, 0);
	}
	}

	if (Settings.currentState1 == 0)
	{
		state_ext = 1;
	} else if (Settings.currentState1 == 1)
	{
		state_ext = 0;
	}

	Serial.print("Settings.currentState1: ");
	Serial.println(Settings.currentState1);

	Serial.print("SecuritySettings.settingsVersion: ");
	Serial.println(SecuritySettings.settingsVersion);
	boolean saveSettings = false;

	if (SecuritySettings.settingsVersion < 200) {
		str2ip((char*)DEFAULT_HAIP, Settings.haIP);

		Settings.haPort = DEFAULT_HAPORT;

		Settings.resetWifi = DEFAULT_RESETWIFI;

		Settings.powerOnState = DEFAULT_POS;

		str2ip((char*)DEFAULT_IP, Settings.IP);
		str2ip((char*)DEFAULT_SUBNET, Settings.Subnet);

		str2ip((char*)DEFAULT_GATEWAY, Settings.Gateway);

		Settings.useStatic = DEFAULT_USE_STATIC;

		strncpy(Settings.remoteIP, DEFAULT_REMOTEIP, sizeof(Settings.remoteIP));
		Settings.reportToRemote = false;

		Settings.usePassword = DEFAULT_USE_PASSWORD;

		Settings.usePasswordControl = DEFAULT_USE_PASSWORD_CONTROL;

		Settings.longPress = DEFAULT_LONG_PRESS;
		Settings.reallyLongPress = DEFAULT_REALLY_LONG_PRESS;

		strncpy(SecuritySettings.Password, DEFAULT_PASSWORD, sizeof(SecuritySettings.Password));

		SecuritySettings.settingsVersion = 200;

		saveSettings = true;
	}

	if (SecuritySettings.settingsVersion < 201) {

		SecuritySettings.settingsVersion = 201;

		saveSettings = true;
	}

	if (SecuritySettings.settingsVersion < 202) {
		Settings.usePort = DEFAULT_PORT;
		SecuritySettings.settingsVersion = 202;

		saveSettings = true;
	}

	if (SecuritySettings.settingsVersion < 203) {
		Settings.switchType = DEFAULT_SWITCH_TYPE;
		Settings.autoOff1 = DEFAULT_AUTO_OFF1;
		SecuritySettings.settingsVersion = 203;

		saveSettings = true;
	}

	if (SecuritySettings.settingsVersion < 204) {
		Settings.uReport = DEFAULT_UREPORT;
		SecuritySettings.settingsVersion = 204;
		saveSettings = true;
	}

	if (SecuritySettings.settingsVersion < 205) {

#if defined SONOFF
		if (Settings.switchType == 0) Settings.externalType = 3;
		if (Settings.switchType == 1) Settings.externalType = 4;
#endif
		Settings.debounce = DEFAULT_DEBOUNCE;
		SecuritySettings.settingsVersion = 205;
		saveSettings = true;
	}

	if (SecuritySettings.settingsVersion < 206) {
		strncpy(Settings.hostName, DEFAULT_HOSTNAME, sizeof(Settings.hostName));
		SecuritySettings.settingsVersion = 206;
		saveSettings = true;
	}

	if (SecuritySettings.settingsVersion < 207) {
		strncpy(Settings.description, "", sizeof(Settings.description));
		SecuritySettings.settingsVersion = 207;
		saveSettings = true;
	}

	if (saveSettings == true) {
		SaveSettings();
	}

	Serial.print("Settings.externalType: ");
	Serial.println(Settings.externalType);


	Serial.println("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
	Serial.println("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");

	WiFiManager wifiManager;

	wifiManager.setConnectTimeout(30);
	wifiManager.setConfigPortalTimeout(300);

	if (Settings.useStatic == true) {
		wifiManager.setSTAStaticIPConfig(Settings.IP, Settings.Gateway, Settings.Subnet);
	}

	if (Settings.hostName[0] != 0) {
		//' wifiManager.setHostName(Settings.hostName);
		wifi_station_set_hostname(Settings.hostName);

	}

	if (Settings.resetWifi == true) {
		wifiManager.resetSettings();
		Settings.resetWifi = false;
		SaveSettings();
	}

	WiFi.macAddress(mac);

	String apSSID = deblank(projectName) + "." + String(mac[0], HEX) + String(mac[1], HEX) + String(mac[2], HEX) + String(mac[3], HEX) + String(mac[4], HEX) + String(mac[5], HEX);

	if (!wifiManager.autoConnect(apSSID.c_str(), "configme")) {
		//Serial.println("failed to connect, we should reset as see if it connects");
		delay(3000);
		ESP.reset();
		delay(5000);
	}

	//Serial1.println("");

	if (Settings.usePort > 0 && Settings.usePort < 65535) {
		server.reset(new ESP8266WebServer(WiFi.localIP(), Settings.usePort));
	}
	else {
		server.reset(new ESP8266WebServer(WiFi.localIP(), 80));
	}

	

	setupServer();
	setupTime();

	Serial.printf("Starting SSDP...\n");
	SSDP.setSchemaURL("description.xml");
	SSDP.setHTTPPort(80);
	SSDP.setName(projectName);
	SSDP.setSerialNumber(ESP.getChipId());
	SSDP.setURL("index.html");
	SSDP.setModelName(projectName);
	SSDP.setModelNumber(deblank(projectName) + "_SL");
	SSDP.setModelURL("http://smartlife.tech");
	SSDP.setManufacturer("Smart Life Automated");
	SSDP.setManufacturerURL("http://smartlife.tech");
	SSDP.begin();

	Serial.println("HTTP server started");
	macAddress =  WiFi.macAddress();
	Serial.println("MAC address is " + macAddress);
	localIP = WiFi.localIP().toString();
	Serial.println(localIP);

	timerUptime = millis() + Settings.uReport * 1000;
}
void setup()
{
	pinMode(KEY_PIN1, INPUT_PULLUP);
	attachInterrupt(KEY_PIN1, relayToggle1, CHANGE);
	pinMode(REL_PIN1, OUTPUT);



	pinMode(LED_PIN1, OUTPUT);


	// Setup console
	Serial.begin(115200);
	delay(500);


	//Serial1.println();
	//Serial1.println();

#if not define TEST_ONLY

	setupOthers();

#endif


}



void loop()
{
	
#if not define TEST_ONLY

	server->handleClient();


	if (needUpdate1 == true || needUpdate2 == true || needUpdate3 == true || needUpdate4 == true) {
		sendStatus(0);
	}


	if (needUpdate1 == true) {

		sendStatus(1);
		needUpdate1 = false;
	}

	if (needUpdate2 == true) {
		sendStatus(2);
		needUpdate2 = false;
	}

	if (needUpdate3 == true) {
		sendStatus(3);
		needUpdate3 = false;
	}

	if (needUpdate4 == true) {
		sendStatus(4);
		needUpdate4 = false;
	}

	if (millis() > timer1s)
		runEach1Seconds();

	if (millis() > timer5s)
		runEach5Seconds();

	if (millis() > timer1m)
		runEach1Minutes();

	if (millis() > timer5m)
		runEach5Minutes();

	if (Settings.uReport > 0 && millis() > timerUptime) {
		currentDateTime = NTP.getTimeDateString();
		sendStatus(99);
		timerUptime = millis() + Settings.uReport * 1000;
	}




	if ((Settings.autoOff1 != 0) && inAutoOff1 && ((millis() - autoOffTimer1) > (1000 * Settings.autoOff1))) {
		relayControl(1, 0);
		autoOffTimer1 = 0;
		inAutoOff1 = false;
	}

#endif
}


