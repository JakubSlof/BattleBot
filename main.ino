// ---------------------------------------------------------------------------------------
//
// Code for a simple webserver on the ESP32 (device used for tests: ESP32-WROOM-32D).
// The code generates two random numbers on the ESP32 and uses Websockets to continuously
// update the web-clients. For data transfer JSON encapsulation is used.
//
// For installation, the following libraries need to be installed:
// * Websockets by Markus Sattler (can be tricky to find -> search for "Arduino Websockets"
// * ArduinoJson by Benoit Blanchon
//
// NOTE: in principle this code is universal and can be used on Arduino AVR as well. However, AVR is only supported with version 1.3 of the webSocketsServer. Also, the Websocket
// library will require quite a bit of memory, so wont load on Arduino UNO for instance. The ESP32 and ESP8266 are cheap and powerful, so use of this platform is recommended. 
//
// Refer to https://youtu.be/15X0WvGaVg8
//
// Written by mo thunderz (last update: 27.08.2022)
//
// ---------------------------------------------------------------------------------------

#include <WiFi.h>                                     // needed to connect to WiFi
#include <WebServer.h>                                // needed to create a simple webserver (make sure tools -> board is set to ESP32, otherwise you will get a "WebServer.h: No such file or directory" error)
#include <WebSocketsServer.h>                         // needed for instant communication between client and server through Websockets
#include <ArduinoJson.h>                              // needed for JSON encapsulation (send multiple variables with one string)

// SSID and password of Wifi connection:
const char* ssid = "BBot";
const char* password = "niganiga";

// The String below "webpage" contains the complete HTML code that is sent to the client whenever someone connects to the webserver
const char* webpage PROGMEM = R"webpage(
  <!DOCTYPE html>
<html>
<head>
<title>Page Title</title>
</head>
<body style='background-color: #EEEEEE;'>

<span style='color: #003366;'>

<h1>Lets generate a random number</h1>
<p>The first random number is: <span id='rand1'>-</span></p>
<p>The second random number is: <span id='rand2'>-</span></p>
<p><button type='button' id='BTN_SEND_BACK'>
Send info to ESP32
</button></p>

</span>


<input type='range' min='-255' max='255' value='0' class='slider' id='0' oninput='SendData(id,value)' ontouchend='ResetSlider("0")'>



</body>
<script>
//id 0 throttle

  var Socket;
  document.getElementById('BTN_SEND_BACK').addEventListener('click', button_send_back);
  function init() {
    Socket = new WebSocket('ws://' + window.location.hostname + ':81/');
    Socket.onmessage = function(event) {
      processCommand(event);
    };
  }
  function button_send_back() {
    var msg = {
	brand: 'Gibson',
	type: 'Les Paul Studio',
	year:  2010,
	color: 'white'
	};
	Socket.send(JSON.stringify(msg));
  }



  function SendData(id,value) { //odesila data o 
    var data = id + value;
        console.log(data);
        Socket.send(data);
  }

  function ResetSlider(id) {  // po pusteni slideru ho vrati do puvodni pozice
        var slider = document.getElementById(id);
        slider.value = 0; //vrati slider na pozici 0
        SendData(id, 0); //odesle data 
      }






  function processCommand(event) {
	var obj = JSON.parse(event.data);
	document.getElementById('rand1').innerHTML = obj.rand1;
	document.getElementById('rand2').innerHTML = obj.rand2;
    console.log(obj.rand1);
	console.log(obj.rand2);
  }
  window.onload = function(event) {
    init();
  }
</script>
</html>

)webpage";

// The JSON library uses static memory, so this will need to be allocated:
// -> in the video I used global variables for "doc_tx" and "doc_rx", however, I now changed this in the code to local variables instead "doc" -> Arduino documentation recomends to use local containers instead of global to prevent data corruption

// We want to periodically send values to the clients, so we need to define an "interval" and remember the last time we sent data to the client (with "previousMillis")
int interval = 1000;                                  // send data to the client every 1000ms -> 1s
unsigned long previousMillis = 0;                     // we use the "millis()" command for time reference and this will output an unsigned long

// Initialization of webserver and websocket
WebServer server(80);                                 // the server uses port 80 (standard port for websites
WebSocketsServer webSocket = WebSocketsServer(81);    // the websocket uses port 81 (standard port for websockets

void setup() {
  Serial.begin(115200);                               // init serial port for debugging
 
  WiFi.begin(ssid, password);                         // start WiFi interface
  Serial.println("Establishing connection to WiFi with SSID: " + String(ssid));     // print SSID to the serial interface for debugging
 
  while (WiFi.status() != WL_CONNECTED) {             // wait until WiFi is connected
    delay(1000);
    Serial.print(".");
  }
  Serial.print("Connected to network with IP address: ");
  Serial.println(WiFi.localIP());                     // show IP address that the ESP32 has received from router
  
  server.on("/", []() {                               // define here wat the webserver needs to do
    server.send(200, "text/html", webpage);           //    -> it needs to send out the HTML string "webpage" to the client
  });
  server.begin();                                     // start server
  
  webSocket.begin();                                  // start websocket
  webSocket.onEvent(webSocketEvent);                  // define a callback function -> what does the ESP32 need to do when an event from the websocket is received? -> run function "webSocketEvent()"
}

void loop() {
  server.handleClient();                              // Needed for the webserver to handle all clients
  webSocket.loop();                                   // Update function for the webSockets 
  
  unsigned long now = millis();                       // read out the current "time" ("millis()" gives the time in ms since the Arduino started)
  if ((unsigned long)(now - previousMillis) > interval) { // check if "interval" ms has passed since last time the clients were updated
    
    String jsonString = "";                           // create a JSON string for sending data to the client
    StaticJsonDocument<200> doc;                      // create a JSON container
    JsonObject object = doc.to<JsonObject>();         // create a JSON Object
    object["rand1"] = random(100);                    // write data into the JSON object -> I used "rand1" and "rand2" here, but you can use anything else
    object["rand2"] = random(100);
    serializeJson(doc, jsonString);                   // convert JSON object to string
    Serial.println(jsonString);                       // print JSON string to console for debug purposes (you can comment this out)
    webSocket.broadcastTXT(jsonString);               // send JSON string to clients
    
    previousMillis = now;                             // reset previousMillis
  }
}

void webSocketEvent(byte num, WStype_t type, uint8_t * payload, size_t length) {      // the parameters of this callback function are always the same -> num: id of the client who send the event, type: type of message, payload: actual data sent and length: length of payload
  switch (type) {                                     // switch on the type of information sent
    case WStype_DISCONNECTED:                         // if a client is disconnected, then type == WStype_DISCONNECTED
      Serial.println("Client " + String(num) + " disconnected");
      break;
    case WStype_CONNECTED:                            // if a client is connected, then type == WStype_CONNECTED
      Serial.println("Client " + String(num) + " connected");
      // optionally you can add code here what to do when connected
      break;
    case WStype_TEXT:                                 // if a client has sent data, then type == WStype_TEXT

    // Parsování ID
    int id = payload[0] - '0'; // Převod ASCII číslice na int

    // Parsování čísla
    char buffer[5] = {0}; // Buffer pro číslo (max 4 znaky + null terminátor)
    memcpy(buffer, &payload[1], length - 1); // Kopírování zbytku do bufferu
    int cislo = atoi(buffer); // Převod na celé číslo

    // Výpis
    Serial.print("ID: ");
    Serial.println(id);
    Serial.print("Cislo: ");
    Serial.println(cislo);
      break;
  }
}