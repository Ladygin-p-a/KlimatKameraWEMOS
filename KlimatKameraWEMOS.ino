//Управление освещением гидропоники

#include <Arduino.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Firebase_ESP_Client.h>
//Provide the token generation process info.
#include "addons/TokenHelper.h"
//Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

/*#include <OneWire.h>
  #include <DallasTemperature.h>*/
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#ifdef ESP32
#include <WiFi.h>
#include <AsyncTCP.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#endif
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic

#define REFRIG_ON HIGH//холодильник включен
#define REFRIG_OFF LOW//холодильник выключен
#define FAN_ON HIGH//вытяжка включена
#define FAN_OFF LOW//вытяжка выключена
#define IS_ON "is ON"
#define IS_OFF "is OFF"

#define SEALEVELPRESSURE_HPA (1013.25)

// Insert Firebase project API Key
#define API_KEY "AIzaSyARgv99kxHkM2jDQwaL-2v5SGkWa8NVssA"

// Insert RTDB URLefine the RTDB URL */
#define DATABASE_URL "https://klimatkameradata-default-rtdb.europe-west1.firebasedatabase.app/"

//Define Firebase Data object
FirebaseData fbdo;

FirebaseAuth auth;
FirebaseConfig config;

Adafruit_BME280 bme; // I2C

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

AsyncWebServer server(80);
DNSServer dns;

/*
  const char* ssid = "MoltiWiFi";
  const char* password = "Point-7142462";

  Beeline_2G_F13784-ext
  StrsDYtE
*/

const char* PARAM_MESSAGE = "message";
const char* PARAM_INPUT_1 = "output";
const char* PARAM_INPUT_2 = "state";

const int _GPIO14 = 14;
const int D5_FAN = _GPIO14;

const int _GPIO12 = 12;
const int D6_REF = _GPIO12;

const int _GPIO4 = 4;
const int D2_SDA = _GPIO4;

const int _GPIO5 = 5;
const int D1_SCL = _GPIO5;

const int _GPIO13 = 13;
const int D7 = _GPIO13;

uint32_t TimerNTP, TimerBME;

boolean FAN_is_ON; //вентилятор включен
boolean REF_is_ON; //холодильник включен
boolean REF_enabled; //холодильник подключен
boolean FAN_enabled; //вытяжка подключена

float H;
float T;
char buffer[15];

bool signupOK = false;
int currentHour = 0;
int currentMinutes = 0;


/*
  const int oneWireBus = D7;


  // Setup a oneWire instance to communicate with any OneWire devices
  OneWire oneWire(oneWireBus);

  // Pass our oneWire reference to Dallas Temperature sensor
  DallasTemperature sensors(&oneWire);
*/

/*
  //Week Days
  String weekDays[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

  //Month names
  String months[12] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
*/

const char index_html[] PROGMEM = "";
/*R"rawliteral(
  <!DOCTYPE HTML><html>
  <head>
  <title>ESP Web Server</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" href="data:,">
  <style>
    html {font-family: Arial; display: inline-block; text-align: center;}
    h2 {font-size: 3.0rem;}
    p {font-size: 3.0rem;}
    body {max-width: 600px; margin:0px auto; padding-bottom: 25px;}
      .switch {position: relative; display: inline-block; width: 120px; height: 68px}
      .switch input {display: none}
      .slider {position: absolute; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; border-radius: 6px}
      .slider:before {position: absolute; content: ""; height: 52px; width: 52px; left: 8px; bottom: 8px; background-color: #fff; -webkit-transition: .4s; transition: .4s; border-radius: 3px}
      input:checked+.slider {background-color: #b30000}
      input:checked+.slider:before {-webkit-transform: translateX(52px); -ms-transform: translateX(52px); transform: translateX(52px)}
  </style>
  </head>
  <body>

  <h2>ESP Web Server</h2>
  %BUTTONPLACEHOLDER%
  <script>function toggleCheckbox(element) {
  var xhr = new XMLHttpRequest();
  if(element.checked){ xhr.open("GET", "/update?output="+element.id+"&state=1", true); }
  else { xhr.open("GET", "/update?output="+element.id+"&state=0", true); }
  xhr.send();
  }
  </script>
  </body>
  </html>
  )rawliteral";
*/

// PLACEHOLDER выбранной кнопкой на вашей веб-странице
String processor(const String& var) {
  /*    //Serial.println(var);
      if(var == "BUTTONPLACEHOLDER"){
        String buttons = "";
        buttons += "<h4>Output - GPIO 0</h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"0\" " + outputState(0) + "><span class=\"slider\"></span></label>";
        return buttons;
      }*/
  return String();
}

/*
  String outputState(int output) {
  if (digitalRead(output)) {
    return "checked";
  }
  else {
    return "";
  }
  }
*/

/*
  String getState(int output) {
  if (digitalRead(output)) {
    return "OFF";
  }
  else {
    return "ON";
  }
  }
*/

String _getFANEnabled() {
  if (FAN_enabled) {
    return IS_ON;
  }
  else {
    return IS_OFF;
  }
}

String _getREFEnabled() {
  if (REF_enabled) {
    return IS_ON;
  }
  else {
    return IS_OFF;
  }
}

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

void setup() {

  unsigned status;
  Serial.begin(115200);
  status = bme.begin();

  pinMode(D5_FAN,   OUTPUT);
  digitalWrite(D5_FAN, LOW);//реле включается логическим "1"

  pinMode(D6_REF,   OUTPUT);
  digitalWrite(D6_REF, LOW);//реле включается логическим "1"

  //first parameter is name of access point, second is the password
  AsyncWiFiManager wifiManager(&server, &dns);

  wifiManager.autoConnect("Klimat kamera");

  /*
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
      Serial.printf("WiFi Failed!\n");
      return;
    }

    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  */

  // Маршрут для корневой / веб-страницы
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/html", index_html, processor);
  });

  // Отправить GET запрос в <ESP_IP>/update?output=<inputMessage1>&state=<inputMessage2>
  server.on("/setEn_FANREF", HTTP_GET, [] (AsyncWebServerRequest * request) {
    String inputMessage1;
    String inputMessage2;
    // Получить запрос GET значения input1 от <ESP_IP>/update?output=<inputMessage1>&state=<inputMessage2>
    if (request->hasParam(PARAM_INPUT_1) && request->hasParam(PARAM_INPUT_2)) {

      inputMessage1 = request->getParam(PARAM_INPUT_1)->value();
      inputMessage2 = request->getParam(PARAM_INPUT_2)->value();

      if (inputMessage1 == "REF") {
        if (inputMessage2 == "1") {
          REF_enabled = true;
        } else {
          REF_enabled = false;
        }
      }

      if (inputMessage1 == "FAN") {
        if (inputMessage2 == "1") {
          FAN_enabled = true;
        } else {
          FAN_enabled = false;
        }
      }

      request->send(200, "text/plain", "Successfully");
    }
    else {
      request->send(200, "text/plain", "Not successful. Check that the request parameters are set correctly.");
    }

  });


  // Отправить GET запрос в <ESP_IP>/update?output=<inputMessage1>&state=<inputMessage2>
  server.on("/update", HTTP_GET, [] (AsyncWebServerRequest * request) {
    String inputMessage1;
    String inputMessage2;
    // Получить запрос GET значения input1 от <ESP_IP>/update?output=<inputMessage1>&state=<inputMessage2>
    if (request->hasParam(PARAM_INPUT_1) && request->hasParam(PARAM_INPUT_2)) {
      inputMessage1 = request->getParam(PARAM_INPUT_1)->value();
      inputMessage2 = request->getParam(PARAM_INPUT_2)->value();

      int _inputMessage1 = inputMessage1.toInt();
      int _inputMessage2 = inputMessage2.toInt();

      digitalWrite(_inputMessage1, _inputMessage2);

      if (_inputMessage1 == D5_FAN) {
        if (_inputMessage2 == 1) {
          FAN_is_ON = true;
        } else {
          FAN_is_ON = false;
        }
      }

      if (_inputMessage1 == D6_REF) {
        if (_inputMessage2 == 1) {
          REF_is_ON = true;
        } else {
          REF_is_ON = false;
        }
      }

    }
    else {
      inputMessage1 = "No message sent";
      inputMessage2 = "No message sent";
    }

    Serial.print("GPIO: ");
    Serial.print(inputMessage1);
    Serial.print(" - Set to: ");
    Serial.println(inputMessage2);
    request->send(200, "text/plain", "OK");
  });

  /*// Отправить GET запрос в <ESP_IP>/pwm?output=<inputMessage1>&state=<inputMessage2>
    server.on("/pwm", HTTP_GET, [] (AsyncWebServerRequest * request) {
    String inputMessage1;
    String inputMessage2;
    // Получить запрос GET значения input1 от <ESP_IP>/pwm?output=<inputMessage1>&state=<inputMessage2>
    if (request->hasParam(PARAM_INPUT_1) && request->hasParam(PARAM_INPUT_2)) {
      inputMessage1 = request->getParam(PARAM_INPUT_1)->value();
      inputMessage2 = request->getParam(PARAM_INPUT_2)->value();
      digitalWrite(inputMessage1.toInt(), inputMessage2.toInt());

    }
    else {
      inputMessage1 = "No message sent";
      inputMessage2 = "No message sent";
    }

    Serial.print("GPIO: ");
    Serial.print(inputMessage1);
    Serial.print(" - Set to: ");
    Serial.println(inputMessage2);
    request->send(200, "text/plain", "OK");
    });*/

  /*
    server.on("/macaddr", HTTP_GET, [] (AsyncWebServerRequest * request) {
    request->send(200, "text/plain", WiFi.macAddress());
    });
  */

  /*
    server.on("/getState", HTTP_GET, [] (AsyncWebServerRequest * request) {
    request->send(200, "text/plain", getState(0));
    });
  */

  server.on("/getEn_REF", HTTP_GET, [] (AsyncWebServerRequest * request) {
    request->send(200, "text/plain", _getREFEnabled());
  });

  server.on("/getEn_FAN", HTTP_GET, [] (AsyncWebServerRequest * request) {
    request->send(200, "text/plain", _getFANEnabled());
  });

  server.on("/getT", HTTP_GET, [] (AsyncWebServerRequest * request) {
    request->send(200, "text/plain", dtostrf(T, 5, 2, buffer));
  });

  server.on("/getH", HTTP_GET, [] (AsyncWebServerRequest * request) {
    request->send(200, "text/plain", dtostrf(H, 5, 2, buffer));
  });

  server.onNotFound(notFound);

  server.begin();

  // Initialize a NTPClient to get time
  timeClient.begin();
  // Set offset time in seconds to adjust for your timezone, for example:
  // GMT +1 = 3600
  // GMT +8 = 28800
  // GMT -1 = -3600
  // GMT 0 = 0
  timeClient.setTimeOffset(10800);

  TimerNTP = millis(); // сбросить таймер
  TimerBME = millis();

  H = 0; T = 0;
  FAN_is_ON = false;
  REF_is_ON = false;

  REF_enabled = true;
  FAN_enabled = true;

  H = bme.readHumidity();
  T = bme.readTemperature();

  //sensors.begin();

  /* Assign the api key (required) */
  config.api_key = API_KEY;

  /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;

  /* Sign up */
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("ok");
    signupOK = true;
  }
  else {
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

}


void loop() {

  //Работаем с вытяжным вентилятором
  if (millis() - TimerNTP >= 60000) {//Проверяем время раз в минуту

    TimerNTP = millis(); // сбросить таймер
    timeClient.update();

    currentHour = timeClient.getHours();
    currentMinutes = timeClient.getMinutes();

    if (FAN_enabled) {
      //for (int i = 9; i <= 23; i++) {

      if (!FAN_is_ON) {
        if (((currentHour == 10) || (currentHour == 16) || (currentHour == 23)) && (currentMinutes == 0)) {
          digitalWrite(D5_FAN, FAN_ON);
          FAN_is_ON = true;
          Serial.println("Включаем вытяжку");
        }
      }

      if (FAN_is_ON) {
        if (((currentHour == 10) || (currentHour == 16) || (currentHour == 23)) && (currentMinutes == 10)) {
          digitalWrite(D5_FAN, FAN_OFF);
          FAN_is_ON = false;
          Serial.println("Выключаем вытяжку");
        }
      }

      //}
    }

    if (Firebase.ready() && signupOK) {

      time_t epochTime = timeClient.getEpochTime();

      //Get a time structure
      struct tm *ptm = gmtime ((time_t *)&epochTime);

      int monthDay = ptm->tm_mday;
      /*Serial.print("Month day: ");
        Serial.println(monthDay);*/

      int currentMonth = ptm->tm_mon + 1;
      /*Serial.print("Month: ");
        Serial.println(currentMonth);*/

      int currentYear = ptm->tm_year + 1900;
      /*Serial.print("Year: ");
        Serial.println(currentYear);*/


      String dataPath_T(monthDay);
      dataPath_T += "-";
      dataPath_T += currentMonth;
      dataPath_T += "-";
      dataPath_T += currentYear;
      dataPath_T += "/";
      dataPath_T += currentHour;
      dataPath_T += "-";
      dataPath_T += currentMinutes;
      dataPath_T += "/T";

      // Write an Int number on the database path test/int
      if (Firebase.RTDB.setFloat(&fbdo, dataPath_T.c_str(), T)) {
        Serial.println("PASSED");
        Serial.println("PATH: " + fbdo.dataPath());
        Serial.println("TYPE: " + fbdo.dataType());
      }
      else {
        Serial.println("FAILED");
        Serial.println("REASON: " + fbdo.errorReason());
      }

      String dataPath_H(monthDay);
      dataPath_H += "-";
      dataPath_H += currentMonth;
      dataPath_H += "-";
      dataPath_H += currentYear;
      dataPath_H += "/";
      dataPath_H += currentHour;
      dataPath_H += "-";
      dataPath_H += currentMinutes;
      dataPath_H += "/H";

      // Write an Float number on the database path test/float
      if (Firebase.RTDB.setFloat(&fbdo, dataPath_H.c_str(), H)) {
        Serial.println("PASSED");
        Serial.println("PATH: " + fbdo.dataPath());
        Serial.println("TYPE: " + fbdo.dataType());
      }
      else {
        Serial.println("FAILED");
        Serial.println("REASON: " + fbdo.errorReason());
      }
    }
    /*        Serial.print("Hour: ");
            Serial.println(currentHour);

            Serial.print("Minutes: ");
            Serial.println(currentMinutes);*/

    //Serial.print("IP Address: ");
    //Serial.println(WiFi.localIP());
  }



  //Работаем с холодильником
  if (millis() - TimerBME >= 60000) {
    TimerBME = millis(); // сбросить таймер

    H = bme.readHumidity();
    T = bme.readTemperature();


    if (REF_enabled) {

      if (!REF_is_ON) {
        if (T > 12) {
          digitalWrite(D6_REF, REFRIG_ON);
          REF_is_ON = true;
          Serial.println("Включаем холодильник");
          Serial.print(F("T = "));
          Serial.print(T);
          Serial.println(" *C");

          Serial.print(F("H = "));
          Serial.print(H);
          Serial.println(" %");

        }
      }

      if (REF_is_ON) {
        if (T < 10) {
          digitalWrite(D6_REF, REFRIG_OFF);
          REF_is_ON = false;
          Serial.println("Выключаем холодильник");
          Serial.print(F("T = "));
          Serial.print(T);
          Serial.println(" *C");

          Serial.print(F("H = "));
          Serial.print(H);
          Serial.println(" %");

        }
      }
    }


    /*
      sensors.requestTemperatures();
      float temperatureC = sensors.getTempCByIndex(0);
      float temperatureF = sensors.getTempFByIndex(0);
      Serial.print(temperatureC);
      Serial.println("ºC");
      Serial.print(temperatureF);
      Serial.println("ºF");
    */
  }

}
