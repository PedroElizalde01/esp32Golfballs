#include <WiFi.h>
#include <PubSubClient.h>
#include <Stepper.h>
#include <LiquidCrystal_I2C.h>
#include <HTTPClient.h>

// Seteamos columnas y filas del display (también vienen de 20x4)
int lcdColumns = 16;
int lcdRows = 2;

// seteo de dirección,columnas,filas del  LCD
LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);

const int stepsPerRevolution = 2048;  // change this to fit the number of steps per revolution
const int stepsToMove = int(stepsPerRevolution * 45.0 / 360.0);

// ULN2003 Motor Driver Pins
#define IN1 19
#define IN2 18
#define IN3 5
#define IN4 17

#define GREEN 25
#define RED 32
#define BLUE 33

// initialize the stepper library
Stepper myStepper(stepsPerRevolution, IN1, IN3, IN2, IN4);

const char *ssidList[] = {"UA-Alumnos", "PersonalWIFI"};
const char *passwordList[] = {"41umn05WLC", "PersonalPASSWORD"}; // I can add the amount of wifis that i want - Pedro
const int numberOfNetworks = sizeof(ssidList); 

int currentNetwork = 0;

const char *server = process.env.SERVER_IP;

const char *mqtt_server = process.env.BROKER_IP; // broker
const char *mqtt_topic = "golf/#";

int stock = 0;

WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi() {
  delay(10);
  Serial.begin(115200);
  Serial.println();

  while (currentNetwork < numberOfNetworks) {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("CONNECTING TO: ");
    lcd.setCursor(0,1);
    lcd.print(ssidList[currentNetwork]);
    ledAnimation();
    Serial.print("Connecting to ");
    Serial.println(ssidList[currentNetwork]);
    WiFi.begin(ssidList[currentNetwork], passwordList[currentNetwork]);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      ledAnimation();
      attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("");
      Serial.println("WiFi connected");
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());
      break; // Exit the loop if connection is successful
    } else {
      Serial.println("Connection failed. Trying next network...");
      currentNetwork++;
    }
  }

  if (currentNetwork >= numberOfNetworks) {
    Serial.println("All WiFi connection attempts failed. Please check your credentials.");
  }
}

void callback(char *topic, byte *payload, unsigned int length) {
  if (strcmp(topic, "golf/ventas") == 0) {
    int amount = 0;
    for (int i = 0; i < length; i++) {
      amount = atoi((char *)payload); // Convert the payload to an integer
    }
    if(amount > stock){
      lcd.clear();
      lcd.setCursor(2, 0);
      lcd.print("NOT ENOUGH");      
      lcd.setCursor(4, 1);
      lcd.print("STOCK");      
      delay(1500);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Stock:" + String(stock));
    } else {
      stock -= amount;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("DROPING BALLS!");
      digitalWrite(BLUE, HIGH);
      for (int i = 0; i < amount; i++) {
        delay(500);
        lcd.setCursor(7, 1);
        int remainingStock = amount - i;
        if (remainingStock< 10) {
          lcd.print("0");  // Agrega un 0 antes de los números de una cifra
        }
        lcd.print(remainingStock);
        myStepper.step(stepsToMove);
      }
      digitalWrite(BLUE, LOW);
      delay(1000); 
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Stock:" + String(stock));
    }
  } else if (strcmp(topic, "golf/stock") == 0) {
    Serial.print("payload: ");
    Serial.println(atoi((char *)payload));
    stock = atoi((char *)payload);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Stock:" + String(stock));
  }
}

void ledAnimation() {
  digitalWrite(GREEN, LOW);
  digitalWrite(RED, LOW);
  digitalWrite(BLUE, LOW);
  digitalWrite(GREEN, HIGH);
  delay(200);
  digitalWrite(GREEN, LOW);
  digitalWrite(RED, HIGH);
  delay(200);
  digitalWrite(RED, LOW);
  digitalWrite(BLUE, HIGH);
  delay(200);
  digitalWrite(BLUE, LOW);
}

void reconnect() {
  while (!client.connected()) {
    digitalWrite(GREEN, LOW);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("RECONNECTING");
    ledAnimation();
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP8266Client")) {
      ledAnimation();
      Serial.println("connected");
      client.subscribe(mqtt_topic);
    } else {
      digitalWrite(RED, HIGH);
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
      digitalWrite(RED, LOW);
    }
  }
}

void setup() {
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("OFFLINE");
  pinMode(GREEN, OUTPUT);
  pinMode(RED, OUTPUT);
  pinMode(BLUE, OUTPUT);
  digitalWrite(RED, HIGH);
  myStepper.setSpeed(5);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
    digitalWrite(RED, LOW);
    digitalWrite(GREEN, HIGH);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("CONNECTED");
    delay(500);
    getStock();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Stock:" + String(stock));
  }
  digitalWrite(RED, LOW);
  client.loop();
}

void getStock() {
  String endpoint = "http://" + String(server) + ":8080/stock";
  HTTPClient http;
  http.begin(endpoint);
  int httpCode = http.GET();
  if (httpCode > 0) {
    Serial.printf("[HTTP] GET... code: %d\n", httpCode);
    if (http.getSize() > 0) {
      String payload = http.getString();
      Serial.println(payload);
      int colonPos = payload.indexOf(':');
      int closingBracePos = payload.indexOf('}');
      String stockValueStr = payload.substring(colonPos + 1, closingBracePos);
      int stockValue = stockValueStr.toInt();

      Serial.print("Stock: ");
      Serial.println(stockValue);
      stock = stockValue;
    }
  } else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();
}