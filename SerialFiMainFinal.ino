/**
 * 
 * Project: SerialLink
 * Date of last update: 2/23/22
 * 
 */

#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <lwip/napt.h>
#include <lwip/dns.h>
#include <LwipDhcpServer.h>

#define NAPT 1000
#define NAPT_PORT 10

#ifndef min
#define min(x,y)  ((x)<(y)?(x):(y))
#endif

//how many clients should be able to telnet to this ESP8266
#define MAX_CONNECTIONS 1
#define TCP_PORT (23)

WiFiServer comServer(TCP_PORT);
WiFiClient comServerClients[MAX_CONNECTIONS];
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;


const int EEPROM_SIZE = 30;

long baudrate = 115200;


// function Prototypes
/**
 * Purpose: This function is the main brain for the actually dunctionality of the s-link, this is  what reads and exchanges data between the connected wifi clients 
 * and the hardware serial connection from the esp, being the ecu or serial device 
 * that is connected to the slink.
 * PreConditions: NA
 * PostConditions: Any packets to be sent between decives are exchanged and are handled accordingly
 */
void handleTCPProtocol();
/**
 * Purpose: converts a long numerical value into a string for use in the site.
 * Preconditions: input is a valid long datatype
 * Postconditions: returns a string with the value equvalent to the input long
 */
String longToString(long);
/**
 * Purpose: Reads the eeprom on load to determine what the desired baudrate is and changes it accordingly
 * Preconditions: Device has a valid EEPROM flash storage attached and a valid baud rate is stored in the memory at address 0
 * Postconditions: The saved desired Baud rate has been read and a new serial communication port is opened using that baud value.
 */
void readConfig();
/**
 * Purpose: Writes the desired baudrate to the eeprom for persistant storage across boots
 * Preconditions: User has input a valid baud rate into the website page and has hit the save button
 * Postconditions: The user entered baud rate is saved to the EEPROM and a new serial port has been opened with the new baudrate, old one has been closed
 */
void writeConfig();
/**
 * Purpose: reads the inputs from the webpage configuration and pushing any changes as needed.
 * PreConditions: user has pressed the save button with valid inputs.
 * PostConditions: Confirmation page is opened and changes are handled accordingly.
 */
void handleForm();
/**
 * Purpose: Handles sending the stored site to the connected client when they navigate to the server
 * Preconditions: User has connected a device to the ESP chip and has navigated to 192.168.4.1
 * Postcondition: Site data is sent to the end user device to be displayed in the users web browser
 */
void handleRoot();
/**
 * Purpose: Establishes a connection to an existing wifi network so it can forward communications through the connection to the wirelessly attached device.
 * Preconditions: NONE
 * Postconditions: Will be connected to a wireless network.
 * Return: True if wifi was connected, False if connection failed or ssid not present
 */
 bool initwifi(String);
 /**
  * Purpose: Runs only on the first time the device is booted to set default settings as desired.
  * Preconditions: Device has never been powered on before
  * Postconditions: Default settings are established
  */
 void firstRunFunc();
 /**
  * Purpose: Check the device to see if this is the first time the device is turned on
  * Preconditions: Device has just been powered on
  * Postconditions: Returns true if this is the first time the device is being turned on
  */
 bool checkIfFirstRun();
 /**
  * Purpose: checks a c_str for any present non standard characters
  * Preconditions: NA
  * PostConditions: Returns true if special chars are present, false elsewise.
  */
  bool containsSpecialCharacters(const char*);






//html code for the site
char html[2300];
const char index_html[] PROGMEM = R"(
<!DOCTYPE HTML>
<html>
<head>
  <style>
    label {height:30px;font-size:20pt;}
    input {height:30px;font-size:20pt;}
    select {height:30px;font-size:20pt;}
    small {color:blue; font-size:14pt;}
  </style>
</head>
  <body>
  <center>
  <h1>Configuration Page</h1><br>
  <form action="/action_page">
    BAUDRATE:
    <select name = "BAUDRATE">
            <option disabled selected value> %s </option>
            <option value = 115200>115200 (MS2/MS3)</option>
            <option value = 9600>9600 (MS1)</option>
            <option value = 230400>230400</option>
            <option value = 74880>74880</option>
            <option value = 57600>57600</option>
            <option value = 38400>38400</option>
            <option value = 19200>19200</option>
            <option value = 4800>4800</option>
            <option value = 2400>2400</option>
            <option value = 1200>1200</option>
            <option value = 300>300</option>
    </select>
    
    <br>
    <br>
  
  <h1>Your Existing Network Settings (Optional)</h2>
  <label>SSID:</label><br><input name='stassid' pattern="[a-zA-z0-9]+" placeholder='%s' length=32><br>
  <label>Password:</label><br><input type='password' placeholder='********' pattern="[a-zA-z0-9]+" name='stapass' minlength=8 maxlength=32><br><small>Leave blank for open networks.</small><br><small>No Special Characters Allowed</small>
  <h1>Serial Link Access Settings</h2>
  <label>SSID:</label><br><input name='apssid' pattern="[a-zA-z0-9]+" placeholder='%s' length=32><br>
  <label>Password:</label><br><input type='password' placeholder='%s' name='appass' pattern="[a-zA-z0-9]+" minlength=8 maxlength=32><br><small>Must be at least 8 characters or left blank to be an open network.</small><br><small>No Special Characters Allowed</small><br>
  <p><small>Press SAVE ALL to save all changes when finished. baudrate changes will take effect immediately. Wireless settings require a reboot.</small></p>
  <input type="submit" value="SAVE ALL">
  </form>
  
  </center>
</body></html>)";


/**
 * Main program loop
 */
void loop() {
  server.handleClient();
  handleTCPProtocol();
  
}

/**
 * Runs once on startup, use to read the eeprom and set settings accordingly along with setting up the wifi AP and all the server and communication objects.
 */
void setup() {

  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);
  //read and set baudrate
  Serial.begin(baudrate);
  readConfig();
  Serial.println();

  //configure and start Station and softAP settings
  WiFi.setPhyMode(WIFI_PHY_MODE_11N);
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin();
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1),IPAddress(192, 168, 4, 1),IPAddress(255, 255, 255, 0));
  WiFi.softAP(WiFi.softAPSSID(), WiFi.softAPPSK());

  //check and handle first run special case
  if(checkIfFirstRun()){
    firstRunFunc();
  }
  initwifi(WiFi.SSID());
  
  // Start TCP listener on port TCP_PORT
  comServer.begin();
  comServer.setNoDelay(true);

  
  httpUpdater.setup(&server);
  // Send web page with input fields to client
  server.on("/", handleRoot);
  server.on("/action_page", handleForm);
  server.on("/reset", []() {
    server.send(200, "text/html", "<a href = '/'>Resetting...</a>");
    firstRunFunc();
    ESP.reset();
  });
  
  server.begin();
  digitalWrite(2, HIGH);
}





/**
 * User Function Implementations, as outlined above
 */

String longToString(long in){
  char baud[16];
  ltoa(baudrate, baud, 10);
  return baud;
}

void readConfig(){
  EEPROM.begin(EEPROM_SIZE);
  long value;
  EEPROM.get(0, value);
  EEPROM.end();
  //check for gibberish baud and correct as needed
  if(value >= 300 && value <= 115200 && !containsSpecialCharacters(longToString(value).c_str()))
    baudrate = value;
  Serial.end();
  Serial.begin(baudrate);
}

void writeConfig(){
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.put(0,baudrate);
  EEPROM.commit();
  EEPROM.end();
}

void handleForm(){

 if(server.arg("BAUDRATE").length() > 0){
  long bdr = server.arg("BAUDRATE").toInt();
  if(bdr != baudrate){
    baudrate = bdr;
    writeConfig();
  
    Serial.end();
    Serial.begin(baudrate);
  }
 }

  //set new AP values
  String apssid = server.arg("apssid");
  String appass = server.arg("appass");
  if(apssid.length() > 0){
    WiFi.persistent(true);
    WiFi.softAP(apssid, appass);
  }
  //set new station values, (requires a restart to take effect)
  String stassid = server.arg("stassid");
  String stapass = server.arg("stapass");
  if(stassid.length() > 0){
    WiFi.persistent(true);
    WiFi.begin(stassid, stapass);
  }
  server.send(200, "text/html", "<center><a href = '/'><font size=\"20pt\">Saved, Click to Return</font></a></center>");
}

void handleRoot(){
  char buffer[2300];
  //format the html to contain values needed from the backend
  sprintf(buffer, index_html, longToString(baudrate), WiFi.SSID(), WiFi.softAPSSID(), WiFi.softAPPSK());
  //sprintf(buffer, index_html, longToString(baudrate), WiFi.SSID());
  server.send(200, "text/html" ,buffer);
}

void handleTCPProtocol(){
  WiFiClient client = comServer.available();
  uint8_t i;
  uint8_t buf[1024];
  int bytesAvail, bytesIn;
  if (client) {
    if(client.connected())
    {
      digitalWrite(2, LOW);
    }
    
    while(client.connected()){      
        while((bytesAvail = client.available()) > 0){
          // read data from the connected client
          bytesIn = client.readBytes(buf, min(sizeof(buf), bytesAvail));
          if (bytesIn > 0) {
            Serial.write(buf, bytesIn);
          }
        }
        //Send Data to connected client
        while((bytesAvail = Serial.available()) > 0){
          bytesIn = Serial.readBytes(buf, min(sizeof(buf), bytesAvail));
          if (bytesIn > 0) {
            client.write((uint8_t*)buf, bytesIn);
          }
        }
        
    }
    client.stop();
    digitalWrite(2, HIGH); 
  }
}

void firstRunFunc(){
  WiFi.persistent(true);
  WiFi.softAP("SerialLink", "seriallink");
  baudrate = 115200;
  writeConfig();
  
}

bool initwifi(String ssidin) {

  //Check if Wifi Network is even present before attempting a connection
  if(ssidin == ""){
    return false; //no network specified
  }
  delay(100);
  WiFi.scanNetworks();
  delay(1000);
  int n = WiFi.scanComplete();
  for(int i = 0; i < n; i++){
    if(WiFi.SSID(i) == ssidin){ //SSID is Present;
      WiFi.scanDelete();
      WiFi.begin();
      break;
    }else{
      if(i == n-1){ //if last item
        //SSID not found skip NAPT init with false
        return false;
      }
    }
  }
  //NAPT init
  int count = 0;
  digitalWrite(2,LOW);
  while(count < 20){ //allow 20 seconds for AP to give the ESP an ip
  if (WiFi.status() == WL_CONNECTED) { //when ESP obtains an ip
      //pass DNS server
      dhcpSoftAP.dhcps_set_dns(0, WiFi.dnsIP(0));
      dhcpSoftAP.dhcps_set_dns(1, WiFi.dnsIP(1));
      digitalWrite(2,HIGH);
      err_t ret = ip_napt_init(NAPT, NAPT_PORT);
      if (ret == ERR_OK) {
          ret = ip_napt_enable_no(SOFTAP_IF, 1);
            if (ret == ERR_OK) {
            return true; //NAPT started sucessfully
          }
      }
      if (ret != ERR_OK) {
          return false; //NAPT failed to start
      }
      
    }
    //failed, try again in 1 second
    delay(1000);
    count++;
  }
  return false; //No ip was given to the ESP
}

bool checkIfFirstRun()
{
    //grabs stored AP SSID, if it is the first run random characters will be in this memory location
    const char* str = WiFi.softAPSSID().c_str();
    
    return containsSpecialCharacters(str);
}

bool containsSpecialCharacters(const char* str)
{
    int found = 0;
    int index = 0;
    int len = strlen(str);

    //loop through the stored c_str looking for special characters
    while (index < len) {
        if (str[index] >= 122 || str[index] <= 31) { //has special character or control characters
            return true; //has special chars
        }
        index++;
    }
    return false; //no chars found
}
