
#include <Arduino.h>
#include <WiFi.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <EmonLib.h>
//#include <FreeRTOS.h>
#include "config.h"


#include <Firebase_ESP_Client.h>
// Provide the token generation process info.
#include "addons/TokenHelper.h"
// Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

#define ADCINPUT 34  // Pin where the sensor is connected 
#define lcdSDA 13
#define lcdSCL 14

//Wifi
const char* ssid = "";
const char* password = "";


// Insert Firebase project API Key
#define API_KEY ""
// Insert Authorized Email and Corresponding Password
#define USER_EMAIL ""
#define USER_PASSWORD ""
// Insert RTDB URLd efine the RTDB URL
#define DATABASE_URL ""

//for power calc
#define VOLTAGE 200 //Voltage for power calculation

void setupADCinput();
void setupWiFiConnection();
void setupLCD();
void setupFirebase();
bool reauthenticateWithFirebase();
bool i2CAddrTest(uint8_t addr);

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

// Sensor path (where we'll save our readings)
String sensorPath;

// JSON object to save sensor readings and timestamp
//FirebaseJson json;

//LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);
// Emon
EnergyMonitor ctmon;

static double amp = 0.0;
static int watt;

constexpr int timerDelay = 5000; 


unsigned long sendDataPrevMillis = 0;
// task

SemaphoreHandle_t ampMutex;



void firebaseTask(void* parameter) {
    for (;;) {

        // Send new readings to database
        if (Firebase.ready() && (millis() - sendDataPrevMillis > timerDelay || sendDataPrevMillis == 0)){
        //if (Firebase.ready()){             
            sendDataPrevMillis = millis();
            double localAmp;
            
            // Take the mutex before accessing the global amp variable
            if (xSemaphoreTake(ampMutex, (TickType_t) 10) == pdTRUE) {
                localAmp = amp;
                xSemaphoreGive(ampMutex);
            } else {
                // Handle mutex error (e.g., log it, retry, or skip this loop iteration)
                Serial.println("Failed to take mutex in firebaseTask!");
                continue;  // Skip the rest of this iteration and try again
            }

            // Prepare readings for database:
            FirebaseJson json;
            json.set(ampPath.c_str(), String(amp));
            json.set(wattPath.c_str(), String(VOLTAGE*amp));
            json.set(timePath, "timestamp");

            //Serial.printf("Set json... %s\n", Firebase.RTDB.pushJSON(&fbdo, sensorPath.c_str(), &json) ? "ok" : fbdo.errorReason().c_str());
            //send the json to database
            if(!Firebase.RTDB.pushJSON(&fbdo, sensorPath.c_str(), &json)) {
                Serial.printf("Failed to send data to Firebase: %s\n", fbdo.errorReason().c_str());
            } else {
                Serial.println("Data successfully sent to Firebase.");
            }

            // Delay for the time set in timerDelay for interval 
            vTaskDelay(timerDelay / portTICK_PERIOD_MS);
          } else {
            // If not time to send data, just delay for a short while before the next loop iteration
            vTaskDelay(100 / portTICK_PERIOD_MS); // 100ms delay
          }
    }
}


void checkWiFiTask(void* parameter) {
    for (;;) {
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi disconnected! Attempting to reconnect..."); 
        // Update LCD with WiFi error message
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("WiFi Error");
        lcd.setCursor(0, 1);
        lcd.print("Reconnecting...");
    
        if(WiFi.reconnect() == true){
          //delay(1000);
          if(WiFi.status() == true){
            Serial.println("WiFi reconnected on");
            Serial.println(WiFi.localIP());
          }
        } // Attempt to reconnect
        
      } else {
        Serial.println(F("[WIFI] Updating signal strength..."));
        lcd.setCursor(0, 1);
        lcd.print(String(WiFi.RSSI()));
      }
      vTaskDelay(10000 / portTICK_PERIOD_MS); // Delay for 10 seconds before checking again
    }
}

void readSensorTask(void* parameter) {
    for (;;) {
        //Read sensor data and update LCD
        double localAmp;  // local variable to store reading
        
        long int t1 = millis();
        localAmp = ctmon.calcIrms(5588);
        long int t2 = millis();
        Serial.println("Irms calc time : ");
        Serial.print(t2 - t1);
        Serial.println(" milliseconds");
        
        // Store reading to global amp with mutex protection
        if (xSemaphoreTake(ampMutex, (TickType_t) 10) == pdTRUE) {
            amp = localAmp;
            watt = localAmp*VOLTAGE;
            xSemaphoreGive(ampMutex);
        } else {
            // Handle mutex error (e.g., log it, retry, or skip this loop iteration)
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS); // Delay for 1 seconds
    }
}



//update LCD to amp and watt input task
void updateLCDTask(void* parameter) {
    for(;;){
        //serial.println("LCD updating..");
        double localAmp;  // Local variable to temporarily store the amp value
        int localWatt;    
  
        // Store reading to global amp with mutex protection
        if (xSemaphoreTake(ampMutex, (TickType_t) 10) == pdTRUE) {
            localAmp = amp;
            localWatt = watt;
            xSemaphoreGive(ampMutex);
        } else {
            Serial.println("Failed to take mutex in updateLCDTask!");
            continue;
            // Handle mutex error
        }

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(String(amp, 2) + "A ");
        lcd.print(String(watt) + "W");
        lcd.setCursor(0, 1);
        lcd.print(WiFi.localIP().toString());

        // Delay before updating the LCD again. 
        // Adjust the delay based on how frequently you want to refresh the LCD.
        vTaskDelay(3000 / portTICK_PERIOD_MS); // 3 seconds delay
    }
}

void keepFirebaseAliveTask(void* parameter) {
    for (;;) {
        // Check Firebase connection
        if (!Firebase.ready()) {
            Serial.println("Lost connection to Firebase. Attempting to reestablish...");

            // Attempt to reauthenticate and reconnect
            if (reauthenticateWithFirebase()) {
                Serial.println("Reconnected to Firebase successfully.");
            } else {
                Serial.println("Failed to reconnect to Firebase. Will retry...");
            }

            // Wait and then check the connection again.
            // If it's still not connected, the loop will try reconnecting again.
            vTaskDelay(10000 / portTICK_PERIOD_MS); // Delay for 10 seconds
        } else {
            // If Firebase is connected, delay a while before checking again
            vTaskDelay(30000 / portTICK_PERIOD_MS); // Delay for 30 seconds
        }
    }
}





void setup() {
  
  Serial.begin(115200);
  setupLCD();
  setupWiFiConnection();
  setupADCinput();
  setupFirebase();

  ampMutex = xSemaphoreCreateMutex();
  if (ampMutex == NULL) {
    Serial.println("Failed to create mutex for amp!");
    // Handle error appropriately, maybe halt execution.
  }

  //send data to firebase task
  xTaskCreate(
      firebaseTask,
      "Firebase Task",
      8192,
      NULL,
      3,
      NULL
  );

  //Update LCD task
  xTaskCreate(
      updateLCDTask,
      "Update LCD Screen",
      2048,
      NULL,
      1,
      NULL
  );

  //Wifi checking task
  xTaskCreate(
      checkWiFiTask,
      "Check and Handle Wifi",
      2048,
      NULL,
      4,
      NULL
  );

  //Firebase checking task 
  xTaskCreate(
      keepFirebaseAliveTask,
      "Check Firebase and try to keep it alive",
      3072,
      NULL,
      5,
      NULL
  );

// Sensor Reading Task
  xTaskCreate(
      readSensorTask,
      "ReadSensor",
      2048,                   // Sensor reading might be simpler, so we allocate less stack
      NULL,
      2,                      // Medium priority
      NULL
  );

  
}

void loop() {
  // put your main code here, to run repeatedly:
}




//Set up CT 
void 7890setupADCinput() {
  analogReadResolution(12);
  //pinMode(ADCINPUT, INPUT);

  ctmon.current(ADCINPUT, 66); //set pin for current input with callibration 
                               //calibration = 3000/47 = 63.82978723404255
  
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

//i2C connection
bool i2CAddrTest(uint8_t addr) {
  Wire.begin();
  Wire.beginTransmission(addr);
  if (Wire.endTransmission() == 0) {
    return true;
  }
  return false;
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
  //listenerPath = databasePath + "/outputs/";
  //Update database path for sensor readings
  sensorPath = databasePath +"/sensor/";

  // Streaming (whenever data changes on a path)
  // Begin stream on a database path --> UsersData/<user_uid>/outputs
  //if (!Firebase.RTDB.beginStream(&stream, listenerPath.c_str()))
  //  Serial.printf("stream begin error, %s\n\n", stream.errorReason().c_str());

  // Assign a calback function to run when it detects changes on the database
  //Firebase.RTDB.setStreamCallback(&stream, streamCallback, streamTimeoutCallback);
 
  delay(2000);
}

bool reauthenticateWithFirebase() {
    // Reset Firebase configurations
    Firebase.reset(&config);

    // Reinitialize Firebase library with credentials
    Firebase.begin(&config, &auth);
    
    // Optionally, enable automatic reconnection if network disconnects
    Firebase.reconnectNetwork(true);

    // Check if we're connected and authenticated with Firebase
    if (Firebase.ready()) {
        // If connected, print UID for feedback (this step is optional)
        Serial.println("Successfully reauthenticated with Firebase.");
        Serial.println("User UID: " + String(auth.token.uid.c_str()));
        return true;
    } else {
        // Print the error reason for feedback
        Serial.println("Failed to reauthenticate with Firebase.");
        Serial.println(fbdo.errorReason());
        return false;
    }
}
