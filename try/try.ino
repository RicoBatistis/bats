#include <SPI.h>
#include <Wire.h>
#include <MFRC522.h>
#include <LiquidCrystal_I2C.h>
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WebSocketsServer.h>
#include <ESP8266WebServer.h>
#include <Hash.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>


#define SS_PIN D8
#define RST_PIN D0
#define LED_PIN D3
#define BUZZER_PIN D4

const char* Wifi_ssid = "WifiName";
const char* Wifi_passwd = "wifipassword";

// WiFi access point credentials that the ESP will create
const char* AP_ssid = "Feeder";
const char* AP_passwd = "password123";

MFRC522 mfrc522(SS_PIN, RST_PIN);
LiquidCrystal_I2C lcd(0x27, 16, 2);
float totalAmount = 0.0;
bool shampooAdded = false;
bool cantonAdded = false;
bool sardinesAdded = false;
bool noodlesAdded = false;
bool tumblerAdded = false;
bool ketchupAdded = false;
bool soapAdded = false;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

ESP8266WiFiMulti    WiFiMulti;
ESP8266WebServer    server(80);
WebSocketsServer    webSocket = WebSocketsServer(81);

/* Front end code (i.e. HTML, CSS, and JavaScript) */
char html_template[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta http-equiv="X-UA-Compatible" content="IE=edge">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>E Cart using IoT</title>
    <style>
        table {
            border-collapse: collapse;
        }
        th {
            background-color: #3498db;
            color: white;
        }
        table, td {
            border: 4px solid black;
            font-size: x-large;
            text-align: center;
            border-style: groove;
            border-color: rgb(255, 0, 0);
        }
    </style>
</head>
<body>
    <center>
        <h1>Smart Shopping Cart using IoT</h1><br><br>
        <table id="itemTable" style="width: 1200px; height: 450px;">
            <tr>
                <th>ITEMS</th>
                <th>QUANTITY</th>
                <th>COST</th>
            </tr>
            <!-- Rows will be dynamically added by WebSocket -->
        </table><br>
        <input type="button" id="payNowButton" name="Pay Now" value="Pay Now" style="width: 200px; height: 50px">
    </center>

    <script>
        var socket = new WebSocket("ws://" + location.host + ":81");

        socket.onopen = function (e) {
            console.log("[socket] socket.onopen ");
        };

        socket.onerror = function (e) {
            console.error("[socket] socket.onerror ", e);
        };

        socket.onmessage = function (e) {
            console.log("[socket] " + e.data);
            var jsonData = JSON.parse(e.data);
            var cmd = jsonData.cmd;

            if (cmd === 1) {
                updateItemTable(jsonData);
            }
        };

        window.onload = function () {
            var payNowButton = document.getElementById("payNowButton");
            payNowButton.addEventListener("click", function () {
                // SAON PAG CONNECT ANI SA PRINTER
            });
        };

        function updateItemTable(data) {
            var table = document.getElementById("itemTable");

            // Clear existing rows
            while (table.rows.length > 1) {
                table.deleteRow(1);
            }

            // Add new rows based on WebSocket data
            var items = data.items;
            for (var i = 0; i < items.length; i++) {
                var item = items[i];
                var row = table.insertRow(-1);
                var itemNameCell = row.insertCell(0);
                var quantityCell = row.insertCell(1);
                var costCell = row.insertCell(2);
                itemNameCell.innerHTML = item.productName;
                quantityCell.innerHTML = item.quantity;
                costCell.innerHTML = item.price;
            }

            // Update Grand Total
            var grandTotalRow = table.insertRow(-1);
            var totalLabelCell = grandTotalRow.insertCell(0);
            var totalCountCell = grandTotalRow.insertCell(1);
            var totalCostCell = grandTotalRow.insertCell(2);
            totalLabelCell.innerHTML = "<th>Grand Total</th>";
            totalCountCell.innerHTML = "<th>" + data.totalItems + "</th>";
            totalCostCell.innerHTML = "<th>" + data.totalCost + "</th>";
        }
    </script>
</body>
</html>
)=====";

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length); // It handle all web socket responses
void handleMain(); 
void handleNotFound(); 

void setup() {
    Serial.begin(9600);
    SPI.begin();
    mfrc522.PCD_Init();

     // Initialize ESP8266 in AP mode
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_ssid, AP_passwd);
  Serial.print("AP started: ");
  Serial.print(AP_ssid);
  Serial.print(" ~ ");
  Serial.println(AP_passwd);

  // Connect to a WiFi as client
  int attempt = 0;
  WiFiMulti.addAP(Wifi_ssid, Wifi_passwd);
  Serial.print("Connecting to: ");
  Serial.print(Wifi_ssid);
  while (WiFiMulti.run() != WL_CONNECTED) {
    delay(20);
    Serial.print(".");
    attempt++;
    if (attempt > 5 ) break;
  }
  Serial.println();
  if (WiFiMulti.run() == WL_CONNECTED) {
    Serial.print("Connected to: ");
    Serial.println(Wifi_ssid);
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else Serial.println("Error: Unable to connect to the WiFi");

    lcd.init();
    lcd.backlight();
    pinMode(LED_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);

    if (!mfrc522.PCD_PerformSelfTest()) {
        Serial.println("RFID connection failed");
        lcd.setCursor(0, 0);
        lcd.print("Closed");
    } else {
        Serial.println("RFID connected and working");
        lcd.setCursor(0, 0);
        lcd.print("Happy Shopping");
    }
    delay(2000);
    lcd.clear();
    displayTotalAmount();
}

void displayProductInfo(const String& productName, const String& price, const String& total) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(productName + ": " + price);
    lcd.setCursor(0, 1);
    lcd.print("Total: " + total);
}
void displayTotalAmount() {
    lcd.setCursor(0, 1);
    lcd.print("Total: " + String(totalAmount));
}



void handleMain() {
  server.send_P(200, "text/html", html_template ); 
}
void handleNotFound() {
  server.send(404,   "text/html", "<html><body><p>404 Error</p></body></html>" );
}
void loop() {
  displayTotalAmount();
    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
        String cardUID = "";

        for (byte i = 0; i < mfrc522.uid.size; i++) {
            cardUID += (mfrc522.uid.uidByte[i] < 0x10 ? "0" : "") + String(mfrc522.uid.uidByte[i], HEX);
        }

        if (cardUID.equals("61611626") || cardUID.equals("c1a89221")) {
            if (shampooAdded) {
                totalAmount -= 10.00;
                displayProductInfo("Shampoo Removed", "10.00", String(totalAmount));
                shampooAdded = false;
            } else {
                totalAmount += 10.00;
                displayProductInfo("Shampoo", "10.00", String(totalAmount));
                shampooAdded = true;
            }
        } 
        else if (cardUID.equals("fbe27ff3") || cardUID.equals("d37e151e")) {
            if (cantonAdded) {
                totalAmount -= 15.00;
                displayProductInfo("Canton Removed", "15.00", String(totalAmount));
                cantonAdded = false;
            } else {
                totalAmount += 15.00;
                displayProductInfo("Canton", "15.00", String(totalAmount));
                cantonAdded = true;
            }
        }
        else if (cardUID.equals("f1ff655e") || cardUID.equals("eb1176f3")) {
            if (sardinesAdded) {
                totalAmount -= 20.00;
                displayProductInfo("Sardines Removed", "20.00", String(totalAmount));
                sardinesAdded = false;
            } else {
                totalAmount += 20.00;
                displayProductInfo("Sardines", "20.00", String(totalAmount));
                sardinesAdded = true;
            }
        }
         else if (cardUID.equals("f19f8a6b") || cardUID.equals("5bf083f3")) {
            if (noodlesAdded) {
                totalAmount -= 30.00;
                displayProductInfo("Noodles Removed", "30.00", String(totalAmount));
                noodlesAdded = false;
            } else {
                totalAmount += 30.00;
                displayProductInfo("Noodles", "30.00", String(totalAmount));
                noodlesAdded = true;
            }
        }
        else if (cardUID.equals("91755b5e") || cardUID.equals("8b1c85f3")) {
            if (tumblerAdded) {
                totalAmount -= 50.00;
                displayProductInfo("Tumbler Removed", "50.00", String(totalAmount));
                tumblerAdded = false;
            } else {
                totalAmount += 50.00;
                displayProductInfo("Tumbler", "50.00", String(totalAmount));
                tumblerAdded = true;
            }
        }
         else if (cardUID.equals("b1f3856b") || cardUID.equals("db49f7f6")) {
            if (ketchupAdded) {
                totalAmount -= 25.00;
                displayProductInfo("Ketchup Removed", "25.00", String(totalAmount));
                ketchupAdded = false;
            } else {
                totalAmount += 25.00;
                displayProductInfo("Ketchup", "25.00", String(totalAmount));
                tumblerAdded = true;
            }
        }
        else if (cardUID.equals("616c846b") || cardUID.equals("b1a8995e")) {
            if (soapAdded) {
                totalAmount -= 30.00;
                displayProductInfo("Soap Removed", "30.00", String(totalAmount));
                soapAdded = false;
            } else {
                totalAmount += 30.00;
                displayProductInfo("Soap", "30.00", String(totalAmount));
                soapAdded = true;
            }
        }
       
        // Add similar logic for other products...

        digitalWrite(BUZZER_PIN, HIGH);
        digitalWrite(LED_PIN, HIGH);
        delay(500);
        digitalWrite(BUZZER_PIN, LOW);
        digitalWrite(LED_PIN, LOW);

        mfrc522.PICC_HaltA();
        mfrc522.PCD_StopCrypto1();
        delay(3000);
        lcd.clear();
        displayTotalAmount(); 
    }
}

