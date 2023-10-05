#include <Arduino.h>
#include <WiFi.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <EmonLib.h>
#include <Firebase_ESP_Client.h>

// Provide the token generation process info.
#include "addons/TokenHelper.h"
// Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

//Wifi
const char* ssid = "";
const char* password = "";
//const char* ssid = "ply";
//const char* password = "444555666";

// Insert Firebase project API Key
#define API_KEY ""
// Insert Authorized Email and Corresponding Password
#define USER_EMAIL ""
#define USER_PASSWORD ""
// Insert RTDB URLd efine the RTDB URL
#define DATABASE_URL ""

//PIN 
#define ADCINPUT 34  // Pin where the sensor is connected 
#define lcdSDA 13 // LCD SDA pin
#define lcdSCL 14 // LCD SCL pin

//for power calc
#define VOLTAGE 200 //Voltage for power calculation

// Define Firebase objects
FirebaseData stream;
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Variable to save USER UID
String uid;

// Variables to save database paths
String databasePath;
// Database child nodes
String ampPath = "/current";
String wattPath = "/power";
String timePath = "/timestamp/.sv";

// Path to listen for changes
String listenerPath;
// Sensor path (where we'll save our readings)
String sensorPath;
// JSON object to save sensor readings and timestamp
FirebaseJson json;

//LCD address
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Emon
EnergyMonitor ctmon;
double amp;

// Timer variables 
unsigned long sendDataPrevMillis = 0;

//set this to change the interval data being sent to realtimedatabase
unsigned long timerDelay = 5000; 


//function declarations header
void setupADCinput(); //Set up CT adc input
void setupWiFiConnection(); //start wifi
void checkAndHandleWiFiStatus();
void setupLCD();
void setupNTPClient();
void setupFirebase();// setup firebase with credentials
bool checkFirebaseConnection();
bool reauthenticateWithFirebase();
bool i2CAddrTest(uint8_t addr);
void updateLCD(double amp, int watt);
void streamCallback(FirebaseStream data);
void streamTimeoutCallback(bool timeout);

void setup() {
  Serial.begin(115200);
  //initialize sensor and wifi
  setupLCD();
  setupWiFiConnection();
  //setupNTPClient();
  setupADCinput();
  setupFirebase();

}

void loop() {
  checkAndHandleWiFiStatus();
  if (!checkFirebaseConnection()) {
    delay(5000); // Wait for 5 seconds before retrying
    return;      // Skip the rest of this loop iteration
  }
  
  
  // Send new readings to database
  if (Firebase.ready() && (millis() - sendDataPrevMillis > timerDelay || sendDataPrevMillis == 0)){
    sendDataPrevMillis = millis();

    // Get latest sensor readings
    amp = ctmon.calcIrms(5588);
    updateLCD(amp, amp*VOLTAGE);
    
    

    // Send readings to database:
    json.set(ampPath.c_str(), String(amp));
    json.set(wattPath.c_str(), String(VOLTAGE*amp));
    json.set(timePath, "timestamp");

    Serial.printf("Set json... %s\n", Firebase.RTDB.pushJSON(&fbdo, sensorPath.c_str(), &json) ? "ok" : fbdo.errorReason().c_str());

  }
}


//Set up CT 
void setupADCinput() {
  analogReadResolution(12);
  //pinMode(ADCINPUT, INPUT);

  ctmon.current(ADCINPUT, 60); //set pin for current input with callibration 
                               //calibration = CT ratio/ burden resistance
  
}

//Initialize LCD
void setupLCD() {
  Wire.begin(lcdSDA, lcdSCL);
  if (!i2CAddrTest(0x27)) {
    lcd = LiquidCrystal_I2C(0x3F, 16, 2);
  }
  lcd.init();
  lcd.backlight();
}


//set up Wifi connection
void setupWiFiConnection() {
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Connecting to");
    lcd.setCursor(0, 1);
    lcd.print("WiFi..");
  }
  Serial.println("Connected to WiFi");
  Serial.println(WiFi.localIP());

  //show connected WiFi on LCD screen
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connected to");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());
}

//update LCD to amp and watt input
void updateLCD(double amp, int watt) {
  //lcd.clear();
  lcd.setCursor(0, 0);
  
  
  lcd.print(String(amp, 2) + "A ");
  lcd.print(String(watt) + "W");
  
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP().toString());
  delay(500);
}

//i2C connection
bool i2CAddrTest(uint8_t addr) {
  Wire.begin();
  Wire.beginTransmission(addr);
  if (Wire.endTransmission() == 0) {
    return true;
  }
  return false;
}

// Handle WiFi disconnection
void checkAndHandleWiFiStatus() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected! Attempting to reconnect..."); 
    // Update LCD with WiFi error message
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Error");
    lcd.setCursor(0, 1);
    lcd.print("Reconnecting...");
    
    if(WiFi.reconnect() == true){
      delay(3000);
      if(WiFi.status() == true){
        Serial.println("WiFi reconnected on");
        Serial.println(WiFi.localIP());
      }
    } // Attempt to reconnect
    delay(5000); // Delay for 5 seconds before rechecking
  }

}




void setupFirebase(){
  // Assign the api key (required)
  config.api_key = API_KEY;

  // Assign the user sign in credentials
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  // Assign the RTDB URL (required)
  config.database_url = DATABASE_URL;

  fbdo.setBSSLBufferSize(4096 /* Rx buffer size in bytes from 512 - 16384 */, 1024 /* Tx buffer size in bytes from 512 - 16384 */);
  fbdo.setResponseSize(4096);

  // Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h

  // Assign the maximum retry of token generation
  config.max_token_generation_retry = 5;

  // Initialize the library with the Firebase authen and config
  Firebase.begin(&config, &auth);
  Firebase.reconnectNetwork(true);

  // Getting the user UID might take a few seconds
  Serial.println("Getting User UID");
  while ((auth.token.uid) == "") {
    Serial.print('.');
    delay(1000);
  }
  // Print user UID
  uid = auth.token.uid.c_str();
  Serial.print("User UID: ");
  Serial.println(uid);

  // Update database path
  databasePath = "/UsersData/" + uid;
  // Update database path for listening
  listenerPath = databasePath + "/outputs/";
  //Update database path for sensor readings
  sensorPath = databasePath +"/sensor/";

  // Streaming (whenever data changes on a path)
  // Begin stream on a database path --> UsersData/<user_uid>/outputs
  if (!Firebase.RTDB.beginStream(&stream, listenerPath.c_str()))
    Serial.printf("stream begin error, %s\n\n", stream.errorReason().c_str());

  // Assign a calback function to run when it detects changes on the database
  Firebase.RTDB.setStreamCallback(&stream, streamCallback, streamTimeoutCallback);
 
  delay(2000);
}



// use for handle firebase token error
bool checkFirebaseConnection() {
  if (!Firebase.ready()) {
    Serial.println("Lost connection to Firebase. Reauthenticating...");
    return reauthenticateWithFirebase();
  }
  return true; // Already connected
}


//reauthentication firebase
bool reauthenticateWithFirebase() {
    Firebase.reset(&config);
    setupFirebase();

    if (Firebase.ready()) {
        Serial.println("Successfully reauthenticated with Firebase.");
        return true;
    } else {
        Serial.println("Failed to reauthenticate with Firebase.");
        Serial.println(fbdo.errorReason());
        return false;
    }
}



// Callback function that runs on database changes
void streamCallback(FirebaseStream data){
  Serial.printf("stream path, %s\nevent path, %s\ndata type, %s\nevent type, %s\n\n",
                data.streamPath().c_str(),
                data.dataPath().c_str(),
                data.dataType().c_str(),
                data.eventType().c_str());
  printResult(data); //see addons/RTDBHelper.h
  Serial.println();

  // Get the path that triggered the function
  String streamPath = String(data.dataPath());

  //
  //Any stream callback function goes here
  //


  //This is the size of stream payload received (current and max value)
  //Max payload size is the payload size under the stream path since the stream connected
  //and read once and will not update until stream reconnection takes place.
  //This max value will be zero as no payload received in case of ESP8266 which
  //BearSSL reserved Rx buffer size is less than the actual stream payload.
  Serial.printf("Received stream payload size: %d (Max. %d)\n\n", data.payloadLength(), data.maxPayloadLength());
}

void streamTimeoutCallback(bool timeout){
  if (timeout)
    Serial.println("stream timeout, resuming...\n");
  if (!stream.httpConnected())
    Serial.printf("error code: %d, reason: %s\n\n", stream.httpCode(), stream.errorReason().c_str());
}