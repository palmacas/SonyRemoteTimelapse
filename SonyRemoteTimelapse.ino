/*
   Remote control for Sony Camera
   Based on ESP8266 microcontroller and Sony Remote Camera API

   Creates an AccessPoint to setup the camera's SSID and Password
   Allows to define the timelapse span and shot period
*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>

// AP name and password
const char* ssid_ap = "SonyRemoteAP";
const char* password_ap = "sony1234";

// Create WebServer on port 80
ESP8266WebServer server(80);

// Create a client to access the camera
WiFiClient  client;

// Camera AP name and password
String ssid = {};
String password = {};

// Capture variables
const int start_button = 0;         // GPIO0 to start capture
const String host = "10.0.0.1";     // Camera IP, defined on API documentation
const String http_port = "10000";   // Camera port, defined on API documentation
const String url = "/sony/camera";  // URL to send htto request, defined on API documentation
int timelapse_span;                 // Timelapse span
int shot_period;                    // Time between shots (in seconds)
int shot_period_ms;                 // Time between shots (in miliseconds)
int picture_number = 0;             // Max number of pictures on the timelapse
int op_mode = 0;                    // Operation mode

// JSON structures from API documentation
char setShootMode[] = "{\"method\":\"setShootMode\",\"params\":[\"still\"],\"id\":1,\"version\":\"1.0\"}"; // Set shot mode to "still"
char actTakePicture[] = "{\"method\":\"actTakePicture\",\"params\":[],\"id\":1,\"version\":\"1.0\"}"; // Take a picture
char setFocusMode[] = "{\"method\":\"setFocusMode\",\"params\":[\"AF-S\"],\"id\":1,\"version\":\"1.0\"}"; // Set focus mode to Single AF
char setAutoPowerOff[] = "{\"method\":\"setAutoPowerOff\",\"params\":[{\"autoPowerOff\":60}],\"id\":1,\"version\":\"1.0\"}"; // Set automatic power off after 60 seconds

void httpPost(char* j_request);
void handleRoot();
void handleSave();

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta content="width=device-width, initial-scale=1" name="viewport">
    <meta content="Sony Camera Remote - Timelapse" name="description">
    <title>Sony Camera Remote - Timelapse</title>
    <style>
    body {
  background: #36393e;
  color: #bfbfbf;
  font-family: sans-serif;
  margin-left: 10px; margin-right: 10px;
  text-align: center;
  }
    h2 { font-size: 2.0rem; text-align: center; }
    p { font-size: 1.0rem; }
    .labels {
      font-size: 1.0rem;
      vertical-align:middle;
      padding-bottom: 15px;
    }
    a {
    color: #5281bb;
    text-decoration: none;
    }
    input { font-size: 1.0rem; text-align: center}
    fieldset { display: block; margin-left: 10px; margin-right: 10px}
    .labels {
      font-size: 1.0rem;
      vertical-align:middle;
      padding-bottom: 15px
    }
    .button {background-color: #195B6A; border: none; color: white; padding: 16px 40px; text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}
    </style>
</head>
<body>
<h2>Sony Camera Remote - Timelapse</h2>
<p>Insert the network name and password shown in the Sony camera. Then, insert the span of the timelapse in minutes and the time between each shot in seconds.</p>
<form action="/save" method="POST">
    <fieldset>
        <span class="labels">SSID:</span><br>
        <input type="text" name="ssid"><br><br>
        <span class="labels">Password:</span><br>
        <input type="password" name="password"><br><br>
        <span class="labels">Timelapse span:</span><br>
        <input type="number" name="time" min="1" placeholder="in minutes"><br><br>
        <span class="labels">Time between shots:</span><br>
        <input type="number" name="period" min="1" placeholder="in seconds" ><br><br>
    </fieldset>
    <p>Once the parameters are saved the AP (Access Point) will be disabled and the ESP will try to connect to the camera. A negative edge on GPIO0 will start the capture.</p>
    <button class="button" data-translate="save" id="save" onclick="save">SAVE</button>
</form>
<p>Version 1.0<br>
    © Copyright 2020 palmacas<br>
    <a href="https://palmacas.com/" target="_blank">palmacas.com</a></p>
</body>
</html>)rawliteral";

const char save_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta content="width=device-width, initial-scale=1" name="viewport">
    <meta content="Sony Camera Remote - Timelapse" name="description">
    <title>Sony Camera Remote - Timelapse</title>
    <style>
    body {
  background: #36393e;
  color: #bfbfbf;
  font-family: sans-serif;
  margin-left: 10px; margin-right: 10px;
  text-align: center;
  }
    h2 { font-size: 2.0rem; text-align: center; }
    p { font-size: 1.0rem; }
    .labels {
      font-size: 1.0rem;
      vertical-align:middle;
      padding-bottom: 15px;
    }
    a {
    color: #5281bb;
    text-decoration: none;
    }
    </style>
</head>
<body>
<h2>Sony Camera Remote - Timelapse</h2>
<p>Parameters saved succesfully!</p>
<p>Once the ESP is connected to the camera, press the flash button (negative edge on GPIO0) to start the timelapse.</p>
<p>Version 1.0<br>
    © Copyright 2020 palmacas<br>
    <a href="https://palmacas.com/" target="_blank">palmacas.com</a></p>
</body>
</html>)rawliteral";

void setup() {
  // Serial port for debugging
  Serial.begin(115200);
  Serial.println();

  // Setting the AP to be open
  Serial.print("Setting AP: ");
  Serial.println(ssid_ap);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(ssid_ap);

  // Print AP IP address
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());

  // Print AP MAC address
  Serial.print("AP MAC address: ");
  Serial.println(WiFi.softAPmacAddress());

  // Serve main page
  server.on("/", HTTP_GET, handleRoot);

  // Post request to save parameters
  server.on("/save", HTTP_POST, handleSave);

  // Start the server
  server.begin();
  Serial.println("HTTP server started");

  pinMode(start_button, INPUT_PULLUP);
}

void loop()
{
  switch (op_mode)
  {
    case 0:
      while (picture_number == 0) {
        server.handleClient();
      }
      op_mode = 1;
      Serial.println();
      break;
    case 1:
      // Connecting to camera Access Point
      Serial.print("Connecting to ");
      Serial.print(ssid);
      WiFi.begin(ssid, password);

      // Waits for connection
      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
      }

      Serial.println(" ");
      Serial.println("WiFi connected");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      Serial.println("Still Image Shot Mode");
      httpPost(setShootMode);

      Serial.print("Number of pictures: ");
      Serial.println(picture_number);
      Serial.print("Shot period: ");
      Serial.print(shot_period_ms / 1000);
      Serial.println(" seconds");
      op_mode = 2;
      break;
    case 2:
      Serial.println("Press start button");
      while (digitalRead(start_button) != 0) {
        delay(100);
      }
      op_mode = 3;
      Serial.println("Capture started");
    case 3:
      Serial.println("Single AF Mode");
      httpPost(setFocusMode);
      for (int i = 0; i < picture_number; i++) {
        Serial.print("Picture ");
        Serial.println(i + 1);
        httpPost(actTakePicture);
        delay(shot_period_ms);
      }
      op_mode = 4;      
      Serial.println("Capture finished");
      httpPost(setAutoPowerOff);
      break;
    case 4:
      delay(100);
      break;
  }
}

void handleRoot() {
  server.send(200, "text/html", index_html);
}

void handleSave() {
  if (!server.hasArg("ssid") || !server.hasArg("password") || !server.hasArg("time") || !server.hasArg("period")
      || server.arg("ssid") == NULL || server.arg("password") == NULL || server.arg("time") == NULL || server.arg("period") == NULL) {
    server.send(400, "text/plain", "400: Invalid Request");
    return;
  }
  if (server.hasArg("ssid") && server.hasArg("password") && server.hasArg("time") && server.hasArg("period")
      && server.arg("ssid") != NULL && server.arg("password") != NULL && server.arg("time") != NULL && server.arg("period") != NULL) {
    server.send(200, "text/html", save_html);
    ssid = server.arg("ssid");
    password = server.arg("password");
    timelapse_span = server.arg("time").toInt();
    shot_period = server.arg("period").toInt();
    shot_period_ms = shot_period * 1000;
    picture_number = round((timelapse_span * 60) / shot_period);
  }
  else {
    server.send(401, "text/plain", "401: Unauthorized");
  }
}

void httpPost(char* j_request)
{
  // Creates domain name
  String server_name = "http://" + host + ":" + http_port + url;
  // Check WiFi connection status
  if (WiFi.status() == WL_CONNECTED)
  {
    HTTPClient http;
    // Domain name with IP, port and URL
    http.begin(server_name);
    // Specify content-type
    http.addHeader("Content-Type", "application/json");
    // Send HTTP POST
    int http_responde_code = http.POST(j_request);
    // Free resources
    http.end();
  }
  else {
    Serial.println("WiFi Disconnected");
  }
}
