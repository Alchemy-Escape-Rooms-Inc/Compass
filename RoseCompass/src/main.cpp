/**
 * RoseCompass Puzzle Controller
 * Alchemy Escape Rooms - Watchtower Protocol
 * Version: 1.0.0
 *
 * Hardware: ESP32-S3 + Potentiometer (signal on GPIO 4)
 * Puzzle: Rotate compass to SE (Southeast / 135 degrees) to solve
 */

#include <WiFi.h>
#include <PubSubClient.h>

// ============================================
// CONFIGURATION
// ============================================

// Device Identity
#define FIRMWARE_VERSION "1.0.0"
const char* DEVICE_NAME = "RoseCompass";
const char* ROOM_NAME = "MermaidsTale";
const char* VERSION = FIRMWARE_VERSION;

// WiFi Configuration
const char* WIFI_SSID = "AlchemyGuest";
const char* WIFI_PASSWORD = "";

// MQTT Broker (Watchtower)
const char* MQTT_BROKER = "10.1.10.115";
const int MQTT_PORT = 1883;

// Hardware Pins
const int POT_PIN = 4;  // Potentiometer signal on GPIO 4

// Compass Configuration
const int TARGET_DIRECTION = 135;  // SE = 135 degrees
const char* TARGET_NAME = "SE";
const int DIRECTION_TOLERANCE = 10;  // +/- degrees for valid position
const int ANGLE_CHANGE_THRESHOLD = 2;  // Minimum change to report

// Timing
const unsigned long HEARTBEAT_INTERVAL = 300000;  // 5 minutes
const unsigned long LOOP_DELAY = 50;  // 20Hz update rate
const unsigned long DEBOUNCE_TIME = 500;  // Debounce for puzzle solved

// ============================================
// MQTT TOPICS (Watchtower Protocol)
// ============================================

String topicCommand;
String topicStatus;
String topicLog;
String topicDirection;
String topicSolved;

// ============================================
// GLOBAL STATE
// ============================================

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

int currentAngle = 0;
int lastReportedAngle = -1;
bool puzzleSolved = false;
bool puzzleWasSolved = false;
unsigned long lastHeartbeat = 0;
unsigned long solvedTime = 0;

// Direction names for display
const char* DIRECTIONS[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};

// ============================================
// FUNCTION PROTOTYPES
// ============================================

void setupWiFi();
void setupMQTT();
void reconnectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void handleCommand(String command);
int readCompassAngle();
const char* angleToDirection(int angle);
void checkPuzzleState();
void publishStatus();
void publishLog(String message);
void sendHeartbeat();

// ============================================
// SETUP
// ============================================

void setup() {
    Serial.begin(115200);
    delay(100);

    Serial.println();
    Serial.println("========================================");
    Serial.println("RoseCompass Puzzle Controller v" FIRMWARE_VERSION);
    Serial.println("Alchemy Escape Rooms - Watchtower Protocol");
    Serial.println("========================================");

    // Build MQTT topics
    String baseTopic = String(ROOM_NAME) + "/" + String(DEVICE_NAME);
    topicCommand = baseTopic + "/command";
    topicStatus = baseTopic + "/status";
    topicLog = baseTopic + "/log";
    topicDirection = baseTopic + "/direction";
    topicSolved = String(ROOM_NAME) + "/" + String(DEVICE_NAME) + "Solved";

    // Configure ADC for potentiometer
    analogReadResolution(12);  // 12-bit resolution (0-4095)
    analogSetAttenuation(ADC_11db);  // Full range 0-3.3V
    pinMode(POT_PIN, INPUT);

    // Initialize networking
    setupWiFi();
    setupMQTT();

    Serial.print("Setup complete. Target: ");
    Serial.print(TARGET_NAME);
    Serial.print(" (");
    Serial.print(TARGET_DIRECTION);
    Serial.println(" degrees)");
    Serial.println("========================================");
}

// ============================================
// MAIN LOOP
// ============================================

void loop() {
    // Maintain MQTT connection
    if (!mqtt.connected()) {
        reconnectMQTT();
    }
    mqtt.loop();

    // Read compass angle from potentiometer
    currentAngle = readCompassAngle();

    // Report angle changes
    if (abs(currentAngle - lastReportedAngle) >= ANGLE_CHANGE_THRESHOLD) {
        const char* direction = angleToDirection(currentAngle);

        Serial.print("Compass: ");
        Serial.print(currentAngle);
        Serial.print(" deg (");
        Serial.print(direction);
        Serial.println(")");

        // Publish to MQTT
        String anglePayload = "pre_" + String(currentAngle);
        mqtt.publish(topicDirection.c_str(), anglePayload.c_str());

        lastReportedAngle = currentAngle;
    }

    // Check if puzzle is solved
    checkPuzzleState();

    // Send periodic heartbeat
    if (millis() - lastHeartbeat >= HEARTBEAT_INTERVAL) {
        sendHeartbeat();
        lastHeartbeat = millis();
    }

    delay(LOOP_DELAY);
}

// ============================================
// WIFI FUNCTIONS
// ============================================

void setupWiFi() {
    Serial.print("Connecting to WiFi: ");
    Serial.print(WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println(" Connected!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println(" Failed!");
        Serial.println("Continuing in offline mode...");
    }
}

// ============================================
// MQTT FUNCTIONS (Watchtower Protocol)
// ============================================

void setupMQTT() {
    mqtt.setServer(MQTT_BROKER, MQTT_PORT);
    mqtt.setCallback(mqttCallback);
    mqtt.setBufferSize(512);
}

void reconnectMQTT() {
    if (WiFi.status() != WL_CONNECTED) return;

    Serial.print("Connecting to MQTT broker...");

    // Last Will Testament - publish OFFLINE if we disconnect
    String willTopic = topicStatus;
    String willMessage = "OFFLINE";

    String clientId = String(DEVICE_NAME) + "_" + String(random(0xffff), HEX);

    if (mqtt.connect(clientId.c_str(), NULL, NULL, willTopic.c_str(), 1, true, willMessage.c_str())) {
        Serial.println(" Connected!");

        // Subscribe to command topic
        mqtt.subscribe(topicCommand.c_str());
        Serial.print("Subscribed to: ");
        Serial.println(topicCommand);

        // Announce online status
        mqtt.publish(topicStatus.c_str(), "ONLINE", true);
        publishLog("RoseCompass controller online");

    } else {
        Serial.print(" Failed (rc=");
        Serial.print(mqtt.state());
        Serial.println("). Retrying...");
        delay(2000);
    }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    // Copy topic first to avoid stack corruption
    char topicBuf[128];
    strncpy(topicBuf, topic, sizeof(topicBuf) - 1);
    topicBuf[sizeof(topicBuf) - 1] = '\0';

    // Convert payload to string
    char message[128];
    unsigned int copyLen = (length < sizeof(message) - 1) ? length : sizeof(message) - 1;
    memcpy(message, payload, copyLen);
    message[copyLen] = '\0';
    String command = String(message);
    command.trim();
    command.toUpperCase();

    Serial.print("MQTT: ");
    Serial.print(topicBuf);
    Serial.print(" -> ");
    Serial.println(command);

    if (strcmp(topicBuf, topicCommand.c_str()) == 0) {
        handleCommand(command);
    }
}

void handleCommand(String command) {
    // Watchtower Protocol Standard Commands

    if (command == "PING") {
        mqtt.publish(topicStatus.c_str(), "PONG");
        publishLog("PONG");
    }
    else if (command == "STATUS") {
        publishStatus();
    }
    else if (command == "RESET") {
        publishLog("Resetting device...");
        delay(100);
        ESP.restart();
    }
    else if (command == "PUZZLE_RESET") {
        puzzleSolved = false;
        puzzleWasSolved = false;
        solvedTime = 0;
        publishLog("Puzzle reset - find SE to solve");
        mqtt.publish(topicStatus.c_str(), "PUZZLE_RESET");
    }
    else {
        publishLog("Unknown command: " + command);
    }
}

void publishStatus() {
    const char* direction = angleToDirection(currentAngle);

    String status = String("{") +
        "\"device\":\"" + DEVICE_NAME + "\"," +
        "\"version\":\"" + VERSION + "\"," +
        "\"room\":\"" + ROOM_NAME + "\"," +
        "\"angle\":" + currentAngle + "," +
        "\"direction\":\"" + direction + "\"," +
        "\"target\":\"" + TARGET_NAME + "\"," +
        "\"targetAngle\":" + TARGET_DIRECTION + "," +
        "\"solved\":" + (puzzleSolved ? "true" : "false") + "," +
        "\"ip\":\"" + WiFi.localIP().toString() + "\"," +
        "\"uptime\":" + (millis() / 1000) +
        "}";

    mqtt.publish(topicStatus.c_str(), status.c_str());
    Serial.println("Status published");
}

void publishLog(String message) {
    mqtt.publish(topicLog.c_str(), message.c_str());
    Serial.print("Log: ");
    Serial.println(message);
}

void sendHeartbeat() {
    // Watchtower Protocol standard heartbeat format
    const char* direction = angleToDirection(currentAngle);

    char status[256];
    snprintf(status, sizeof(status),
        "ONLINE | %s | v%s | Solved:%s | Direction:%s | Angle:%d | Uptime:%lums",
        DEVICE_NAME,
        VERSION,
        puzzleSolved ? "YES" : "NO",
        direction,
        currentAngle,
        millis()
    );

    mqtt.publish(topicStatus.c_str(), status, true);
    Serial.print("Heartbeat: ");
    Serial.println(status);
}

// ============================================
// COMPASS FUNCTIONS
// ============================================

int readCompassAngle() {
    // Read potentiometer (0-4095)
    int rawValue = analogRead(POT_PIN);

    // DEBUG: Print raw ADC value only when it changes
    static int lastRawValue = -1;
    if (abs(rawValue - lastRawValue) >= 10) {
        Serial.print("DEBUG Raw ADC: ");
        Serial.println(rawValue);
        lastRawValue = rawValue;
    }

    // Apply simple smoothing filter (50% new, 50% old)
    static int filteredValue = -1;
    if (filteredValue < 0) {
        filteredValue = rawValue;  // Initialize on first read
    }
    filteredValue = (rawValue + filteredValue) / 2;

    // Map to 0-359 degrees
    int angle = map(filteredValue, 0, 4095, 0, 359);

    // Clamp to valid range
    angle = constrain(angle, 0, 359);

    return angle;
}

const char* angleToDirection(int angle) {
    // Convert angle to 8-point compass direction
    // N=0, NE=45, E=90, SE=135, S=180, SW=225, W=270, NW=315

    // Add 22.5 degrees offset so each direction covers 45 degree range
    int index = ((angle + 22) % 360) / 45;
    return DIRECTIONS[index];
}

void checkPuzzleState() {
    // Check if compass is pointing to target direction
    int angleDiff = abs(currentAngle - TARGET_DIRECTION);

    // Handle wrap-around (e.g., 350 deg is close to 0 deg)
    if (angleDiff > 180) {
        angleDiff = 360 - angleDiff;
    }

    bool isAtTarget = (angleDiff <= DIRECTION_TOLERANCE);

    if (isAtTarget && !puzzleSolved) {
        // Debounce - must stay at target briefly
        if (solvedTime == 0) {
            solvedTime = millis();
        } else if (millis() - solvedTime >= DEBOUNCE_TIME) {
            // PUZZLE SOLVED!
            puzzleSolved = true;
            puzzleWasSolved = true;

            Serial.println("========================================");
            Serial.println("PUZZLE SOLVED! RoseCompass points to SE!");
            Serial.println("========================================");

            // Publish to Gravity Games topic
            mqtt.publish(topicSolved.c_str(), "triggered");

            // Publish to status
            mqtt.publish(topicStatus.c_str(), "SOLVED");
            publishLog("PUZZLE SOLVED - RoseCompass aligned to SE");
        }
    } else if (!isAtTarget) {
        // Reset debounce timer if moved away
        solvedTime = 0;
    }
}
