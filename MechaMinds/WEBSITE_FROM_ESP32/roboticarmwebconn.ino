#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <ESP32Servo.h>

// --- WI-FI CREDENTIALS ---
#define WIFI_SSID "Jsree"
#define WIFI_PASSWORD "12345678"

// --- FIREBASE CONFIGURATION ---
#define API_KEY "AIzaSyBuyi9KQYRhTFgxEUkLHWkB4h38ENLjlxY"
#define DATABASE_URL "https://robotic-arm-8ca01-default-rtdb.asia-southeast1.firebasedatabase.app"

// --- FIREBASE CORE OBJECTS ---
FirebaseData streamDo;   // The single, permanent real-time listening pipeline
FirebaseAuth auth;
FirebaseConfig config;

// --- SERVO CONFIGURATION ---
Servo baseServo, shoulderServo, armServo, clamperServo;
const int basePin = 13, shoulderPin = 12, armPin = 14, clamperPin = 27;

void setup() {
  Serial.begin(115200);

  // Set up servo hardware frequencies and pins
  baseServo.setPeriodHertz(50);
  baseServo.attach(basePin, 500, 2400);
  shoulderServo.attach(shoulderPin, 500, 2400);
  armServo.attach(armPin, 500, 2400);
  clamperServo.attach(clamperPin, 500, 2400);

  // Connect to Local Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) { delay(300); Serial.print("."); }
  Serial.println("\nConnected Successfully!");

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  // Execute anonymous authentication signup for security handshakes
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase Identity Sign-Up: SUCCESSFUL");
  } else {
    Serial.printf("Auth Error: %s\n", config.signer.signupError.message.c_str());
  }

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // REQUIREMENT: Open a permanent stream ONLY on the arm_state node.
  // The ESP32 is completely blind to /stream and /commands now!
  if (!Firebase.RTDB.beginStream(&streamDo, "/arm_state")) {
    Serial.printf("Stream initialization failed: %s\n", streamDo.errorReason().c_str());
  } else {
    Serial.println("Streaming pipeline locked onto '/arm_state' successfully!");
  }
}

void loop() {
  if (Firebase.ready()) {
    
    // Read from our stream pipeline
    if (!Firebase.RTDB.readStream(&streamDo)) {
      Serial.printf("Stream read error: %s\n", streamDo.errorReason().c_str());
    }
    
    if (streamDo.streamAvailable()) {
      String path = streamDo.dataPath();
      String type = streamDo.dataType();
      
      // 1. Handle single slider adjustments (Live movements / Browser Playback stepping)
      if (path == "/base")         baseServo.write(streamDo.intData());
      else if (path == "/shoulder")   shoulderServo.write(streamDo.intData());
      else if (path == "/arm")        armServo.write(streamDo.intData());
      else if (path == "/clamper")    clamperServo.write(streamDo.intData());
      
      // 2. Handle batch data dumps (Like when page reloads or snaps to Initial State)
      else if (path == "/" && type == "json") {
        FirebaseJson &json = streamDo.jsonObject();
        FirebaseJsonData result;
        
        if (json.get(result, "base"))      baseServo.write(result.intValue);
        if (json.get(result, "shoulder"))  shoulderServo.write(result.intValue);
        if (json.get(result, "arm"))       armServo.write(result.intValue);
        if (json.get(result, "clamper"))   clamperServo.write(result.intValue);
        
        Serial.println("Batch coordinates synced cleanly.");
      }
    }
  }
}