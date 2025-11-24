#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include <string> // Required for std::string

// Wi-Fi AP credentials
const char* ssid = "serverTest";
const char* password = "0987654321";

// HTTP and WebSocket server instances
AsyncWebServer server(80);
AsyncWebSocket ws("/ws"); // websocket endpoint at ws://<host>/ws

// HTML page for Actuator Control with 3-state toggle and WebSocket client logic.
const char* index_html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <title>Actuator Control (WebSockets)</title>
    <style>
        body { font-family: Arial, sans-serif; text-align: center; margin-top: 50px; }
        h1 { color: #007bff; }
        .toggle-switch-container {
            display: flex;
            border: 2px solid #007bff; /* Highlight border */
            border-radius: 8px;
            overflow: hidden;
            align-items: center;
            max-width: 450px; 
            margin: 20px auto; 
            box-shadow: 0 4px 6px rgba(0, 0, 0, 0.1);
        }
        .toggle-switch-container input[type="radio"] {
            display: none;
        }
        .toggle-switch-container label {
            padding: 20px 10px; 
            cursor: pointer;
            background-color: #f8f9fa; /* Light background */
            color: #343a40;
            transition: background-color 0.3s ease, color 0.3s ease;
            flex: 1;
            text-align: center;
            font-size: 1.2em;
            font-weight: bold;
        }
        .toggle-switch-container label:hover {
            background-color: #e2e6ea;
        }
        .toggle-switch-container input[type="radio"]:checked + label {
            background-color: #007bff;
            color: #fff;
        }
        #status {
            margin-top: 20px;
            font-size: 1em;
            color: #6c757d;
        }
    </style>
</head>
<body>
    <h1>Actuator Control</h1>
    <fieldset class="toggle-switch-container" onchange="sendToggleState()">
        <input type="radio" id="option1" name="toggle-state" value="Retract" checked>
        <label for="option1">Retract</label>

        <input type="radio" id="option2" name="toggle-state" value="Off">
        <label for="option2">Off</label>

        <input type="radio" id="option3" name="toggle-state" value="Extend">
        <label for="option3">Extend</label>
    </fieldset>

    <p id="status">Connecting...</p>

    <script>
        var gateway = `ws://${window.location.hostname}/ws`;
        var websocket;

        window.addEventListener('load', function() {
            initWebSocket();
        });

        function initWebSocket() {
            websocket = new WebSocket(gateway);

            websocket.onopen = function(event) {
                console.log('WebSocket: connection opened');
                document.getElementById('status').innerText = 'Status: Connected';
                // Send the initial state of the checked button upon connecting
                sendToggleState(); 
            };

            websocket.onclose = function(event) {
                console.log('WebSocket: connection closed, retrying in 2s');
                document.getElementById('status').innerText = 'Status: Disconnected, Retrying...';
                setTimeout(initWebSocket, 2000);
            };

            websocket.onmessage = function(event) {
                console.log('Message from server: ' + event.data);
            };

            websocket.onerror = function(event) {
                console.log('WebSocket: connection error');
            };
        }

        // Function to read the checked radio button and send its value
        function sendToggleState() {
            if (websocket && websocket.readyState === WebSocket.OPEN) {
                // Find the currently checked radio button by name
                var checkedOption = document.querySelector('input[name="toggle-state"]:checked');
                
                if (checkedOption) {
                    var state = checkedOption.value;
                    console.log("Sending state: " + state);
                    websocket.send(state);
                }
            } else {
                console.log("WebSocket not open - cannot send state");
                document.getElementById('status').innerText = 'Status: Disconnected - cannot send';
            }
        }
    </script>
</body>
</html>
)rawliteral";

// handle incoming WebSocket data frames
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;

    // Only handle single-frame text messages
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
        // Create a std::string from the received data.
        std::string msg((char*)data, len);

        // Print the received state to the Serial Monitor
        Serial.print(">>> Actuator State Received: ");
        Serial.print(msg.c_str());
        Serial.println(" <<<");

        /* // Example logic for controlling an actuator based on the message:
        if (msg == "Extend") {
            Serial.println("Action: Extending Actuator...");
            // digitalWrite(EXTEND_PIN, HIGH);
        } else if (msg == "Retract") {
            Serial.println("Action: Retracting Actuator...");
            // digitalWrite(RETRACT_PIN, HIGH);
        } else if (msg == "Off") {
            Serial.println("Action: Stopping Actuator.");
            // digitalWrite(EXTEND_PIN, LOW);
            // digitalWrite(RETRACT_PIN, LOW);
        }
        */
    }
}

// WebSocket event handler
void onEvent(AsyncWebSocket *serverPtr, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
            break;
        case WS_EVT_DISCONNECT:
            Serial.printf("WebSocket client #%u disconnected\n", client->id());
            break;
        case WS_EVT_DATA:
            handleWebSocketMessage(arg, data, len);
            break;
        case WS_EVT_PONG:
        case WS_EVT_ERROR:
            // Ignore for this example
            break;
    }
}

void initWebSocket() {
    // attach event handler and add websocket to the HTTP server
    ws.onEvent(onEvent);
    server.addHandler(&ws);
}

void setup() {
    Serial.begin(115200);
    delay(10);
    Serial.println("\nStarting ESP32 AP Web Server...");

    // Start ESP32 in Access Point mode with given SSID/password
    WiFi.mode(WIFI_AP);
    bool ok = WiFi.softAP(ssid, password);
    if (!ok) {
        Serial.println("Failed to start AP");
        return;
    }
    IPAddress IP = WiFi.softAPIP(); // usually 192.168.4.1
    Serial.print("AP SSID: ");
    Serial.println(ssid);
    Serial.print("AP IP address: ");
    Serial.println(IP);

    // initialize websocket handling
    initWebSocket();

    // Serve index page on root URL
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", index_html);
    });

    server.begin();
    Serial.println("HTTP server started");
}

void loop() {
    // Call cleanup to free resources used by disconnected clients periodically.
    ws.cleanupClients();

    // Keep loop responsive.
    delay(50);
}
