#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

// Wi-Fi AP credentials
const char* ssid = "serverTest";
const char* password = "0987654321";

// HTTP and WebSocket server instances
AsyncWebServer server(80);
AsyncWebSocket ws("/ws"); // websocket endpoint at ws://<host>/ws

// Simple HTML page served to clients (kept as a raw string for convenience).
// The page opens a WebSocket back to the server and sends "button_pressed"
// when the button is clicked.
const char* index_html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>ESP32 Button Server</title>
  <style>
    body { font-family: Arial, sans-serif; text-align: center; margin-top: 50px; }
    .btn { padding: 15px 30px; font-size: 16px; cursor: pointer; background-color: #4CAF50; color: white; border: none; border-radius: 5px; }
    .btn:hover { background-color: #45a049; }
  </style>
</head>
<body>
  <h1>Control Panel</h1>
  <button class="btn" onclick="sendButtonPress()">Press Me</button>

  <script>
    // build websocket URL from current hostname
    var gateway = `ws://${window.location.hostname}/ws`;
    var websocket;

    window.addEventListener('load', function() {
      initWebSocket();
    });

    function initWebSocket() {
      websocket = new WebSocket(gateway);

      websocket.onopen = function(event) {
        console.log('WebSocket: connection opened');
      };

      websocket.onclose = function(event) {
        console.log('WebSocket: connection closed, retrying in 2s');
        setTimeout(initWebSocket, 2000); // reconnect on close
      };

      websocket.onmessage = function(event) {
        console.log('Message from server: ' + event.data);
      };
    }

    function sendButtonPress() {
      if (websocket && websocket.readyState === WebSocket.OPEN) {
        console.log("Sending 'button_pressed' message...");
        websocket.send("button_pressed");
      } else {
        console.log("WebSocket not open - cannot send");
      }
    }
  </script>
</body>
</html>
)rawliteral";

// handle incoming WebSocket data frames
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  // arg points to AwsFrameInfo when called from WS_EVT_DATA
  AwsFrameInfo *info = (AwsFrameInfo*)arg;

  // Only handle single-frame text messages in this example
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    // ensure NUL termination before creating a C string (safe as we allocate one more byte)
    // create a local buffer to avoid overwriting original data
    std::string msg((char*)data, len);
    // Compare message content
    if (msg == "button_pressed") {
      // Do something when the button is pressed (here we print to serial)
      Serial.println(">>> Serial Monitor Confirmation: Button Pressed! <<<");
      // Optionally broadcast back to all connected websocket clients:
      // ws.textAll("server_received_button_press");
    }
  }
}

// WebSocket event handler
void onEvent(AsyncWebSocket *serverPtr, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      // client connected
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      // client disconnected
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      // data received
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      // ignore for now
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
  }
  IPAddress IP = WiFi.softAPIP(); // usually 192.168.4.1
  Serial.print("AP IP address: ");
  Serial.println(IP);

  // initialize websocket handling
  initWebSocket();

  // Serve index page on root URL
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    // use send (not send_P) because index_html lives in RAM (not in PROGMEM)
    request->send(200, "text/html", index_html);
  });

  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  // AsyncWebServer / AsyncWebSocket are event-driven; you don't need to poll for connections.
  // Call cleanup to free resources used by disconnected clients periodically.
  ws.cleanupClients();

  // Keep loop responsive. Adjust delay as needed for your app.
  delay(50);
}
