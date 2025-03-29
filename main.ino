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
#include <ESP32Servo.h>


Servo Servo1; // Vytvoření objektu pro servo
const int servo_1_pin = 12; // Pin, na který je připojeno servo
int servo_pos;
int servo_last_pos = 0;



const int motor_1_pin_1 = 32; // piny pro motor 1
const int motor_1_pin_2 = 33;
const int motor_2_pin_1 = 25; // piny pro motor 2
const int motor_2_pin_2 = 26;


int steering;
int speed;

















// SSID and password of Wifi connection:
const char* ssid = "BBot";
const char* password = "niganiga";

// The String below "webpage" contains the complete HTML code that is sent to the client whenever someone connects to the webserver
const char* webpage PROGMEM = R"webpage(
<!DOCTYPE html>
<html lang="cs">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=375, height=667, initial-scale=1, maximum-scale=1, user-scalable=no">
    <title>Optimalizovaná stránka</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        body {
            display: flex;
            flex-direction: column;
            justify-content: center;
            align-items: center;
            height: 100vh;
            background-color: #f5f5f5;
            font-family: Arial, sans-serif;
            overflow: hidden;
        }
        /* Horizontální slider */
        .horizontal-slider-container {
            width: 100%;
            display: flex;
            flex-direction: column;
            align-items: center;
            margin-top: 100px;
        }
        .horizontal-slider {
            width: 40%;
            scale: 200%;
        }
        /* Kontejner pro vertikální slider */
        .vertical-slider-container {
            display: flex;
            flex-direction: column;
            align-items: center;
            margin-top: 50px;
        }
        /* Vertikální slider */
        .vertical-slider {
            writing-mode: bt-lr;
            transform: rotate(270deg);
            height: 450px;
            scale: 200%;
        }
        /* Druhý horizontální slider */
        .small-horizontal-slider {
            width: 50%; /* Half the size of the top slider */
            scale: 200%;
            margin-top: 30px;
        }
    </style>
</head>
<body>

    <!-- První horizontální slider -->
    <div class="horizontal-slider-container">
        <input type='range' min='-255' max='255' value='0' class='horizontal-slider' id='0' oninput='SendData(id,value)' ontouchend='ResetSlider("0")'>
    </div>

    <!-- Vertikální slider s druhým horizontálním sliderem pod ním -->
    <div class="vertical-slider-container">
        <input type='range' min='-255' max='255' value='0' class='vertical-slider' id='1' oninput='SendData(id,value)' ontouchend='ResetSlider("1")'>

        <input type='range' min='0' max='90' value='0' class='small-horizontal-slider' id='2' oninput='SendData(id,value)' >
    </div>

    <script>
        var Socket;

        function init() {
            Socket = new WebSocket('ws://' + window.location.hostname + ':81/');
        }

        function SendData(id, value) {
            var data = id + value;
            console.log(data);
            Socket.send(data);
        }

        function ResetSlider(id) {
            var slider = document.getElementById(id);
            slider.value = 0;
            SendData(id, 0);
        }

        window.onload = function(event) {
            init();
        }
    </script>

</body>
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

pinMode(motor_1_pin_1, OUTPUT); // right motor
pinMode(motor_1_pin_2, OUTPUT);
pinMode(motor_2_pin_1, OUTPUT);
pinMode(motor_2_pin_2, OUTPUT);

digitalWrite(motor_1_pin_1, LOW);
digitalWrite(motor_1_pin_2, LOW);
digitalWrite(motor_2_pin_1, LOW);
digitalWrite(motor_2_pin_2, LOW);

Servo1.attach(servo_1_pin); // Připojení serva k zadanému pinu
Servo1.write(0);








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
  //
  server.handleClient();                              // Needed for the webserver to handle all clients
  webSocket.loop();  


  if (servo_pos != servo_last_pos){
    //Serial.println(servo_pos);
    if (servo_pos >75){
      Servo1.attach(servo_1_pin); // Připojení serva k zadanému pinu
      Servo1.write(0);
    }
    if(servo_pos> 30 && servo_pos <60){
      Servo1.attach(servo_1_pin); // Připojení serva k zadanému pinu
      Servo1.write(45);
    }
    if(servo_pos <15){
      Servo1.attach(servo_1_pin); // Připojení serva k zadanému pinu
      Servo1.write(90);
    }  
    servo_last_pos = servo_pos;
 }
  
  if(speed<0){
      analogWrite(motor_1_pin_1,-speed + steering ); //going back
      analogWrite(motor_1_pin_2,0 );

      analogWrite(motor_2_pin_2,-speed - steering ); //going back
      analogWrite(motor_2_pin_1,0 );
  
    }
    else{
      analogWrite(motor_1_pin_2,speed + steering ); // going forward
      analogWrite(motor_1_pin_1,0 );

      analogWrite(motor_2_pin_1,speed - steering ); //going forward
      analogWrite(motor_2_pin_2,0);
     
    }                              // Update function for the webSockets 
  
  
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

    if(id == 2){
      servo_pos = cislo;
    }
    if(id == 1){
      steering = cislo;
    }
    if (id == 0){
      speed = cislo;
    }
      break;
  }
}