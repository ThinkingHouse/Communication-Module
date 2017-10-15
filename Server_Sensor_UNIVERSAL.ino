/******************************************************************************
 *                             ЗАГОЛОВОЧНЫЕ ФАЙЛЫ
 ******************************************************************************/
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
//#include "DHT.h"
#include <stdint.h>
#include <stdbool.h>

/******************************************************************************
 *                         ЛОКАЛЬНЫЕ МАКРООПРЕДЕЛЕНИЯ
 ******************************************************************************/
//#define SSID "Main_Sensor_NET"             //Имя подсети
#define PASSWORD "1234567Test"               //Пароль сети
#define MIN_RSSI_LEVEL -70                   //Минимальный уровень RSSI

#define INFO_LOG(data) Serial1.println("INFO :: " + String(data))
#define DEBUG_LOG(data) Serial1.println("DEBUG :: " + String(data))
#define TRACE_LOG(data) Serial1.println("TRACE :: " + String(data))
/******************************************************************************
 *                              СТРУКТУРЫ ДАННЫХ
 ******************************************************************************/
typedef struct info
{
  int8_t is_start;                           //Флаг запуска
  int8_t work_mode;                          //режим сети
  int8_t role;                               //роль
  int32_t sleep_time;                        //период измерений
}info;

typedef enum
{
  UNKNOW        = 0,
  COMMUNICATION = 1,
  SLAVE         = 2
} role_in_net_t;

typedef enum
{
  NOT           = 0,
  MOTION        = 1,
  GET_SENSORS   = 2
} actions_type_t;

/******************************************************************************
 *                              ГЛОБАЛЬНЫЕ ДАННЫЕ
 ******************************************************************************/                  
ESP8266WebServer server_ap;                  //Класс сервера AP
ESP8266WebServer server_client;            //Класс сервера STA
const int8_t role = 0;                        //всегда сервер
role_in_net_t role_in_net = UNKNOW;           //Роль в сети
const IPAddress ap_ip(192, 168, 1, 1);        //IP адрес сервера
const IPAddress mask(255, 255, 255, 0);       //Маска подсети
const IPAddress client_ip;                    //IP локальный клиента
info sensors_settings[100];
String html_answer = "<html><p><input name=\"Data\" type=\"text\" value=\"0\" /><input type=\"submit\" value=\"Update\" /></p></html>";
String ssid_for_ap_string = "Slave_" + String(ESP.getChipId()) + "_Sensor_NET";
const char* password_for_ap = "1300703Dron";
String ssid_for_connect_string = "";
const char* password_for_connect = "1300703DronVip";


/******************************************************************************
 *                         ПРОТОТИПЫ ФУНКЦИЙ
 ******************************************************************************/
void settings_set(int esp_num);         //Добавление сенсора-клиента в базу
void data_from_sensor_get(void);        //Обработчик получения данных из сети
void data_get(void);                    //Обработчик запроса на данные датчиков
void wait_handler(void);                //Обработчик запроса на запуск
void handleNotFound(void);              //Обработчик неизвестного запроса
String string_from_serial_get(void);    //Обработка команд из UaRT
void set_ssid_names(void);              //Генерация SSID
void set_wifi_param(void);              //Настроить WiFi
bool check_command_send(void);          //Проверка отправки команды в UART
/******************************************************************************
 *                             ГЛОБАЛЬНЫЕ ФУНКЦИИ
 ******************************************************************************/
/*
*        Установить ssid для AP и для Client
* \brief 
*/
void set_ssid_names()
{
  int n = WiFi.scanNetworks();
  DEBUG_LOG("Сканирование доступных сетей");
  if (n == 0)
  {
    DEBUG_LOG("Нет доступных сетей");
    ssid_for_ap_string = "Slave_" + String(ESP.getChipId()) + "_Sensor_NET";
    ssid_for_connect_string = "";
  }
  else
  {
    int last_rssi = -100;
    String last_ssid = "";
    DEBUG_LOG(String(n) + " Найдено сетей");
    for (int i = 0; i < n; ++i)
    {
      if(WiFi.SSID(i).startsWith("Main_Sensor_NET"))
      {
        DEBUG_LOG("Найдена Main_Sensor_NET");
        ssid_for_connect_string = WiFi.SSID(i);
        if(WiFi.RSSI(i) < MIN_RSSI_LEVEL)
        {
          last_rssi = WiFi.RSSI(i);
          ssid_for_ap_string = "Slave_" + String(ESP.getChipId()) + "_Sensor_NET";
          role_in_net = COMMUNICATION;
          DEBUG_LOG("Включен режим COMMUNICATION");
        }
        else
        {
          ssid_for_ap_string = "";
          role_in_net = SLAVE;
          DEBUG_LOG("Включен режим SLAVE");
        }
        break;
      }
      else if(WiFi.SSID(i).endsWith("Sensor_NET") && WiFi.SSID(i).startsWith("Slave"))
      {
        DEBUG_LOG("Найдена Slave_Sensor_NET с уровнем RSSI=" + String(WiFi.RSSI(i)) + " last=" + String(last_rssi));
        if(last_rssi < WiFi.RSSI(i))
        {
          ssid_for_ap_string = WiFi.SSID(i);
          last_rssi = WiFi.RSSI(i);
        }
        if(WiFi.RSSI(i) < MIN_RSSI_LEVEL)
        {
          ssid_for_ap_string = "Slave_" + String(ESP.getChipId()) + "_Sensor_NET";
          role_in_net = COMMUNICATION;
          DEBUG_LOG("Включен режим COMMUNICATION");
        }
        else
        {
          ssid_for_ap_string = "";
          role_in_net = SLAVE;
          DEBUG_LOG("Включен режим SLAVE");
        }
      }
      delay(10);
    }
  }
}

/*
*        Установить ssid для AP и для Client
* \brief 
*/
void set_wifi_param()
{
    unsigned char* buf = new unsigned char[100];
  ssid_for_ap_string.getBytes(buf, 100, 0);
  const char *ssid_for_ap = (const char*)buf;

  unsigned char* buf2 = new unsigned char[100];
  ssid_for_connect_string.getBytes(buf2, 100, 0);
  const char *ssid_for_connect = (const char*)buf2;
//  dht.begin();
  
  // Connect to WiFi network
  DEBUG_LOG("");
  DEBUG_LOG("");
  DEBUG_LOG("Connecting to " + String(ssid_for_connect));

  if(role_in_net == COMMUNICATION)
  {
    INFO_LOG("Настройка в режиме COMMUNICATION ssid_ap=" + String(ssid_for_ap) + " ssid_client=" + String(ssid_for_connect));
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAPConfig(ap_ip, ap_ip, mask);
    WiFi.softAP(ssid_for_ap, password_for_ap);
    delay(50);
    WiFi.begin(ssid_for_connect, password_for_connect);
    INFO_LOG("IP Сервера(AP)" + String(WiFi.softAPIP()));
    INFO_LOG("WiFi connected");
    server_ap = ESP8266WebServer(WiFi.softAPIP(), 80);                  //Класс сервера AP
    server_client = ESP8266WebServer(WiFi.localIP(), 80);            //Класс сервера STA
  }
  else
  {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid_for_connect, password_for_connect);
    INFO_LOG("WiFi connected");
    server_client = ESP8266WebServer(WiFi.localIP(), 80);            //Класс сервера STA
  }

  if (MDNS.begin("esp8266")) 
  {
    INFO_LOG("MDNS responder started");
  }
}

/**
*        Проверка отправки команды в UART
*/
bool check_command_send()
{
  bool result = false;
  int timeout = 0;

  while(timeout < 1000)
  {
    if(Serial.available())
    {
        String string_from_serial;
        while (Serial.available()) 
        {
          // get the new byte:
          char char_from_serial = (char)Serial.read();
          // add it to the inputString:
          string_from_serial += char_from_serial;
          // if the incoming character is a newline, set a flag
          // so the main loop can do something about it:
          if (char_from_serial == '\n') 
          {
            if(string_from_serial == "Command OK")
              result = true;
            else
              result = false;
          }
          break;
        }
    }
  }

  return result;
}

/**
*        Первоначальная настройка
* \brief Первая запускаемая функция после сброса, установка параметров
*/
void setup(void)
{
  Serial1.begin(115200);
  Serial.begin(115200);
  delay(10);
  Serial.setDebugOutput(false);
  Serial1.setDebugOutput(false);
  INFO_LOG("");
  set_ssid_names();
  set_wifi_param();

  if(role_in_net == COMMUNICATION)
  {
    server_ap.on("/get", data_get);
    server_ap.on("/", []() { server_ap.send(200, "text/plain", "<p><input name=\"Start\" type=\"button\" value=\"Start\" /></p>"); } );
    server_ap.onNotFound(ap_handleNotFound);
    server_ap.begin();

    server_client.on("/", []() { server_client.send(200, "text/plain", "<p><input name=\"Start\" type=\"button\" value=\"Start\" /></p>"); } );
    server_client.on("/get_agent_info", get_agent_info_handle);
    server_client.on("/do_action", do_action_handle);
    server_client.onNotFound(client_handleNotFound);
    server_client.begin();
  }
  else
  {
    server_client.on("/", []() { server_client.send(200, "text/plain", "<p><input name=\"Start\" type=\"button\" value=\"Start\" /></p>"); } );
    server_client.on("/get_agent_info", get_agent_info_handle);
    server_client.on("/do_action", do_action_handle);
    server_client.onNotFound(client_handleNotFound);
    server_client.begin();
  }
  
  INFO_LOG("HTTP server started");
}
/**
*        Основное тело программы
* \brief Выполняется циклически
*/
void loop(void)
{
  String command_from_serial = "";
  String answer = "";
  if (Serial.available())
  {
    command_from_serial = string_from_serial_get();
    
  }
  
  delay(100);
  
  server_client.handleClient();
}

/**
*        Настройка WiFi
* \brief задает имена сетей для работы
*/
void settings_set(int32_t esp_name, int8_t is_start, int8_t work_mode, int8_t role, int32_t sleep_time)
{
    sensors_settings[esp_name].is_start = is_start;
    sensors_settings[esp_name].work_mode = work_mode;
    sensors_settings[esp_name].role = role;
    sensors_settings[esp_name].sleep_time = sleep_time;
}

/**
*        Запрос информации о агенте
*/
void get_agent_info_handle()
{
  DEBUG_LOG("Получен запрос от сервера на получение информации");
  String message = "{ ";
  message += " name=";
  message += "Slave_" + String(ESP.getChipId()) + "_Sensor_NET,";
  message += " role=";
  message += String(role_in_net);
  message += " }";
  server_client.send(200, "application/json", message);
}

/**
*        Прием команд для агента
*/
void do_action_handle()
{
  INFO_LOG("Получена команда от сервера " + String(server_client.args()));

  switch((server_client.arg(0)).toInt())
  {
    case NOT:
      DEBUG_LOG("Действие не найдено");
      server_client.send(200, "text/plain", "1");
      break;

    case MOTION:
      DEBUG_LOG("Задание параметров движения");
      Serial.println("SET_MOTION PWM=" + String(server_client.arg(1)) + " SERVO=" + String(server_client.arg(2)));
      if(!check_command_send())
      {
        Serial.println("SET_MOTION PWM=" + String(server_client.arg(1)) + " SERVO=" + String(server_client.arg(2)));
        TRACE_LOG("Команда не распознана mbed (Повторная отправка)");
      }
      if(check_command_send())
      {
        server_client.send(200, "text/plain", "1");
      }
      else
      {
        server_client.send(200, "text/plain", "0");
      }
      
      break;

    case GET_SENSORS:

      break;

    default:

      break;
  }

}

/**
*        Получение данных от клиентов
* \brief Получение данных от клиентов и отправка их в UaRT
*/
void data_from_sensor_get()
{
  String data = "";
  for (int8_t i=0; i<server_client.args(); i++)
  {
    data += server_client.argName(i) + "=" + server_client.arg(i) + " ";
  }
  server_client.send(200, "text/plain", "OK");
  Serial.println(data);
}
/**
*        Запрос температуры от сервера
*/
void data_get() 
{
  server_client.send(200, "text/html", html_answer);
}

/**
*        Ответ на несуществующий запрс
*/
void client_handleNotFound()
{
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server_client.uri();
  message += "\nMethod: ";
  message += (server_client.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server_client.args();
  message += "\n";
  for (uint8_t i=0; i<server_client.args(); i++)
  {
    message += " " + server_client.argName(i) + ": " + server_client.arg(i) + "\n";
  }
  server_client.send(404, "text/plain", message);
}

/**
*        Ответ на несуществующий запрс
*/
void ap_handleNotFound()
{
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server_ap.uri();
  message += "\nMethod: ";
  message += (server_ap.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server_ap.args();
  message += "\n";
  for (uint8_t i=0; i<server_ap.args(); i++)
  {
    message += " " + server_ap.argName(i) + ": " + server_ap.arg(i) + "\n";
  }
  server_ap.send(404, "text/plain", message);
}

/**
*        Получение данных из UART
*/
String string_from_serial_get() 
{
  String string_from_serial;
  while (Serial.available()) 
  {
    // get the new byte:
    char char_from_serial = (char)Serial.read();
    // add it to the inputString:
    string_from_serial += char_from_serial;
    // if the incoming character is a newline, set a flag
    // so the main loop can do something about it:
    if (char_from_serial == '\n') 
    {
      return string_from_serial;
    }
  }
}

