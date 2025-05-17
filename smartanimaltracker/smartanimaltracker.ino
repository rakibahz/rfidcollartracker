#include <WiFi.h>
#include <TinyGPS++.h>
#include <WebServer.h>
#include <DHT.h>
#include <PulseSensorPlayground.h>

// Wi-Fi credentials
const char* ssid = "Redmi";
const char* password = "12345678";

// Create an instance of the TinyGPSPlus library
TinyGPSPlus gps;

// Use hardware serial (Serial2 for example) for GPS
HardwareSerial mySerial(2);  // Use UART2 (you can use any UART you prefer)

// Create a web server on port 80
WebServer server(80);

// Variable to store GPS data
double lat = 0.0;
double lon = 0.0;

// DHT sensor setup
#define DHTPIN 15  // DHT11 data pin is connected to GPIO15
#define DHTTYPE DHT11  // Type of DHT sensor
DHT dht(DHTPIN, DHTTYPE);

// Pulse sensor setup
#define PULSE_SENSOR_PIN 35  // Pulse sensor pin connected to D35
PulseSensorPlayground pulseSensor;

int bpm = 0;  // Variable to store BPM
float temperature = 0.0;
float humidity = 0.0;

// Buzzer setup
#define BUZZER_PIN 12  // Buzzer pin connected to GPIO12
bool buzzerState = false; // Buzzer state (On/Off)

// Credentials
const char* validEmail = "animaltracker@gmail.com";
const char* validPassword = "animaltracker123";

// SIM800L setup
HardwareSerial sim800l(1);  // Use UART1 (GPIO16 - RX, GPIO17 - TX)
String phoneNumber = "+1234567890";  // Set your phone number here for SMS

void setup() {
  // Start serial monitor
  Serial.begin(115200);

  // Initialize hardware serial for GPS (9600 baud rate)
  mySerial.begin(9600, SERIAL_8N1, 16, 17);  // RX=16, TX=17 (or any other pins you want to use)

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  // Wi-Fi connected, print connection message and IP address
  Serial.println("WiFi Connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Initialize DHT sensor
  dht.begin();

  // Initialize Pulse sensor
  pulseSensor.analogInput(PULSE_SENSOR_PIN);
  pulseSensor.begin();

  // Initialize Buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW); // Start with buzzer off

  // Initialize SIM800L
  sim800l.begin(9600);  // SIM800L baud rate
  delay(1000);  // Allow time for SIM800L to start up

  // Check SIM800L connectivity
  sendSMS("Smart Pet Tracker is ready!");

  // Define the web server route for the login page
  server.on("/", HTTP_GET, []() {
    String html = "<html><head>";
    html += "<style>";
    html += "body { font-family: Arial, sans-serif; background-color: black; color: white; text-align: center; padding-top: 50px; }";
    html += "input[type='text'], input[type='password'] { padding: 10px; margin: 20px; width: 250px; border: 2px solid #FF6F61; color: white; background-color: #333; }";
    html += "button { padding: 10px 20px; background-color: #FF6F61; color: white; border: none; cursor: pointer; font-size: 1.2em; }";
    html += "button:hover { background-color: #FF8C00; }";
    html += "</style>";
    html += "</head><body>";
    html += "<h1>Login to  Animal Tracker</h1>";
    html += "<form method='POST' action='/login'>";
    html += "Email: <input type='text' name='email' required><br>";
    html += "Password: <input type='password' name='password' required><br>";
    html += "<button type='submit'>Login</button>";
    html += "</form>";
    html += "</body></html>";
    
    server.send(200, "text/html", html);
  });

  // Define the login POST handler
  server.on("/login", HTTP_POST, []() {
    String email = server.arg("email");
    String password = server.arg("password");

    // Check credentials
    if (email == validEmail && password == validPassword) {
      server.sendHeader("Location", "/main");  // Redirect to main page after successful login
      server.send(303);
    } else {
      String html = "<html><body style='text-align: center; background-color: black; color: white;'>";
      html += "<h2>Invalid Credentials. Please try again.</h2>";
      html += "<a href='/'>Go back</a>";
      html += "</body></html>";
      server.send(200, "text/html", html);
    }
  });

  // Define the main page route (protected after login)
  server.on("/main", HTTP_GET, []() {
    String html = "<html><head>";
    
    // Include Leaflet.js from a CDN for the map
    html += "<link rel='stylesheet' href='https://unpkg.com/leaflet@1.7.1/dist/leaflet.css' />";
    html += "<script src='https://unpkg.com/leaflet@1.7.1/dist/leaflet.js'></script>";

    // Add custom styles for the map container and page
    html += "<style>";
    html += "body { font-family: Arial, sans-serif; margin: 0; padding: 0; background: linear-gradient(135deg, #FF6F61, #FF8C00); color: white; }";
    html += "h1 { text-align: center; padding: 20px; font-size: 2.5em; text-shadow: 2px 2px 4px rgba(0, 0, 0, 0.5); }";
    html += "#map { height: 80vh; width: 100%; }";  // Map takes 80% of the screen height
    html += "p { text-align: center; font-size: 1.2em; }";
    html += "button { font-size: 1.2em; padding: 10px; margin: 20px; background-color: #FF6F61; color: white; border: none; cursor: pointer; }";
    html += "button:hover { background-color: #FF8C00; }";
    html += "</style>" ;

    html += "</head><body>";
    html += "<h1> Animal Tracking System</h1>"; // Custom text

    html += "<p>Latitude: <span id='lat'>" + String(lat, 6) + "</span></p>";
    html += "<p>Longitude: <span id='lon'>" + String(lon, 6) + "</span></p>";
    html += "<p>Temperature: <span id='temp'>" + String(temperature, 2) + "°C</span></p>";  // Display temperature
    html += "<p>Humidity: <span id='humidity'>" + String(humidity, 2) + "%</span></p>";  // Display humidity
    html += "<p>BPM: <span id='bpm'>" + String(bpm) + "</span></p>";  // Display BPM

    // Add button for buzzer control
    html += "<p><button id='buzzerButton' onclick='toggleBuzzer()'>Toggle Buzzer</button></p>";

    // Map container
    html += "<div id='map'></div>";

    // JavaScript to initialize and display the map
    html += "<script>";
    html += "var map = L.map('map').setView([" + String(lat, 6) + ", " + String(lon, 6) + "], 15);";
    html += "L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {";
    html += "attribution: '©️ OpenStreetMap contributors',";
    html += "maxZoom: 19";
    html += "}).addTo(map);";
    html += "L.marker([" + String(lat, 6) + ", " + String(lon, 6) + "]).addTo(map)";
    html += ".bindPopup('Your Location').openPopup();";
    
    // JavaScript to refresh data via AJAX
    html += "setInterval(function() {";
    html += "  fetch('/data').then(response => response.json()).then(data => {";
    html += "    document.getElementById('lat').innerText = data.lat;";
    html += "    document.getElementById('lon').innerText = data.lon;";
    html += "    document.getElementById('temp').innerText = data.temp + '°C';";
    html += "    document.getElementById('humidity').innerText = data.humidity + '%';";
    html += "    document.getElementById('bpm').innerText = data.bpm;";
    html += "  });";
    html += "}, 5000);";  // Update every 5 seconds

    // Function to toggle buzzer
    html += "function toggleBuzzer() {";
    html += "  fetch('/toggleBuzzer').then(response => response.json()).then(data => {";
    html += "    console.log('Buzzer state:', data.buzzerState);";
    html += "  });";
    html += "}";
    html += "</script>";
    html += "</body></html>";
    
    // Send the HTML response
    server.send(200, "text/html", html);
  });

  // Define the route to provide data in JSON format for AJAX
  server.on("/data", HTTP_GET, []() {
    // Read temperature, humidity, and BPM
    temperature = dht.readTemperature();
    humidity = dht.readHumidity();
    bpm = random(60, 124);  // Simulated BPM value between 60 and 124

    // Handle failed sensor readings
    if (isnan(temperature) || isnan(humidity)) {
      temperature = 0.0;
      humidity = 0.0;
    }

    // Prepare JSON response
    String json = "{";
    json += "\"lat\": " + String(lat, 6) + ",";
    json += "\"lon\": " + String(lon, 6) + ",";
    json += "\"temp\": " + String(temperature, 2) + ",";
    json += "\"humidity\": " + String(humidity, 2) + ",";
    json += "\"bpm\": " + String(bpm);
    json += "}";

    // Send the JSON response
    server.send(200, "application/json", json);
  });

  // Toggle buzzer route
  server.on("/toggleBuzzer", HTTP_GET, []() {
    buzzerState = !buzzerState;
    digitalWrite(BUZZER_PIN, buzzerState ? HIGH : LOW);  // Toggle buzzer state

    // Send back buzzer state as JSON
    String json = "{\"buzzerState\": " + String(buzzerState ? "true" : "false") + "}";
    server.send(200, "application/json", json);
  });

  // Start the server
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  // Handle incoming GPS data
  while (mySerial.available() > 0) {
    gps.encode(mySerial.read());  // Feed GPS data to TinyGPS++

    if (gps.location.isUpdated()) {
      // Get the latest GPS coordinates
      lat = gps.location.lat();
      lon = gps.location.lng();

      // Print to the serial monitor for debugging
      Serial.print("Latitude= ");
      Serial.print(lat, 6);
      Serial.print(", Longitude= ");
      Serial.println(lon, 6);
    }
  }

  // Handle client requests
  server.handleClient();

  // Check for incoming SMS to request location
  if (sim800l.available()) {
    String sms = readSMS();
    if (sms.indexOf("location") != -1) {
      sendSMS("Current location: Lat: " + String(lat, 6) + ", Lon: " + String(lon, 6));
    }
  }
}

// Function to read SMS
String readSMS() {
  String message = "";
  while (sim800l.available()) {
    message += char(sim800l.read());
  }
  return message;
}

// Function to send an SMS
void sendSMS(String message) {
  sim800l.println("AT+CMGF=1");  // Set SMS to text mode
  delay(1000);
  sim800l.print("AT+CMGS=\"" + phoneNumber + "\"\r");  // Set recipient phone number
  delay(1000);
  sim800l.println(message);  // Send the message
  delay(100);
  sim800l.write(26);  // Send Ctrl+Z to send SMS
}