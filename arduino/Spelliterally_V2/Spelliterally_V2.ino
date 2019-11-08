#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESP8266mDNS.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>         //https://github.com/tzapu/WiFiManager
#include <SPIFFSEditor.h>


DNSServer dns;
AsyncWebServer server(80);
AsyncWebSocket webSocket("/ws");
AsyncEventSource events("/events");
const char* host = "kesem";

#define SSWaitForPlayer 0
#define SSWaitForAnswer 1
#define SSWaitForSolve 2
#define SSSolved 3
#define SSLettersAdmin 99
byte systemState;
unsigned long systemStateMillis;
byte currentCardId[4];
byte currentCardIdx;
byte card_state = 0;

#define MAX_RFIDCARDS 50
char letters[MAX_RFIDCARDS];
byte letters_id[MAX_RFIDCARDS][4];
byte lettersArrayActualSize;
File fsUploadFile;
bool shouldReboot = false;

#include <ArduinoJson.h>

#include "RFID.h"
#include "wordList.h"
#include "WS.h"


void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  //********************************
  // FOR WIFI MANGER !!!!
  AsyncWiFiManager wifiManager(&server, &dns);
  //reset saved settings
  //wifiManager.resetSettings();
  //and goes into a blocking loop awaiting configuration
  wifiManager.autoConnect("AutoConnectAP");
  //********************************

  // MDNS
  if (MDNS.begin(host)) {
    MDNS.addService("http", "tcp", 80);
    Serial.println(F("MDNS responder started"));
    Serial.print(F("You can now connect to http://"));
    Serial.print(host);
    Serial.println(F(".local"));
  }
  MDNS.addService("http", "tcp", 80);

  // SPIFFS
  SPIFFS.begin();

  // seb socket
  WS_setup();
  server.addHandler(&webSocket);

  // EVENTS ---- NOT WORKING YET !!!
  events.onConnect([](AsyncEventSourceClient * client) {
    Serial.println("here ??");
    client->send("hello!", NULL, millis(), 1000);
  });
  server.addHandler(&events);

  // spell list of json
  server.on("/spell_file_list.json", HTTP_GET, [] (AsyncWebServerRequest * request) {
    request->send(200, "application/json", getSpellJsonList());
  });

  //settign list of json
  // Send a GET request to <IP>/get?message=<message>
  server.on("/setWordList", HTTP_GET, [] (AsyncWebServerRequest * request) {
    String message;
    if (request->hasParam("setCurrent")) {
      // response 
      message = "OK";
      // change the path
      spellListJsonPath = request->getParam("setCurrent")->value();
      // load the list 
      setUpWordList();
      
    } else {
      message = "FAIL";
    }
    request->send(200, "text/plain", "Hello, GET: " + message);
  });


  // SPIFFS FILE SERVICE TO WEB SERVER
  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

  // NOT FOUND HANDLER
  server.onNotFound([](AsyncWebServerRequest * request) {
    Serial.printf("NOT_FOUND: ");
    if (request->method() == HTTP_GET)
      Serial.printf("GET");
    else if (request->method() == HTTP_POST)
      Serial.printf("POST");
    else if (request->method() == HTTP_DELETE)
      Serial.printf("DELETE");
    else if (request->method() == HTTP_PUT)
      Serial.printf("PUT");
    else if (request->method() == HTTP_PATCH)
      Serial.printf("PATCH");
    else if (request->method() == HTTP_HEAD)
      Serial.printf("HEAD");
    else if (request->method() == HTTP_OPTIONS)
      Serial.printf("OPTIONS");
    else
      Serial.printf("UNKNOWN");
    Serial.printf(" http://%s%s\n", request->host().c_str(), request->url().c_str());

    if (request->contentLength()) {
      Serial.printf("_CONTENT_TYPE: %s\n", request->contentType().c_str());
      Serial.printf("_CONTENT_LENGTH: %u\n", request->contentLength());
    }

    int headers = request->headers();
    int i;
    for (i = 0; i < headers; i++) {
      AsyncWebHeader* h = request->getHeader(i);
      Serial.printf("_HEADER[%s]: %s\n", h->name().c_str(), h->value().c_str());
    }

    int params = request->params();
    for (i = 0; i < params; i++) {
      AsyncWebParameter* p = request->getParam(i);
      if (p->isFile()) {
        Serial.printf("_FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
      } else if (p->isPost()) {
        Serial.printf("_POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
      } else {
        Serial.printf("_GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
      }
    }

    request->send(404);
  });



  // RESOTR CARDS
  server.on("/restoreCards", HTTP_POST, [](AsyncWebServerRequest * request) {},
  [](AsyncWebServerRequest * request, const String & filename, size_t index, uint8_t *data,
     size_t len, bool final) {
    handleUpload(request, filename, "/AdminCards.html", index, data, len, final);
  });

  // RESOTR WORDS
  server.on("/restoreSpell", HTTP_POST, [](AsyncWebServerRequest * request) {},
  [](AsyncWebServerRequest * request, const String & filename, size_t index, uint8_t *data,
     size_t len, bool final) {
    handleUpload(request, filename, "/AdminWords.html", index, data, len, final);
  });



  // Simple Firmware Update Form
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(200, "text/html", "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>");
  });
  server.on("/update", HTTP_POST, [](AsyncWebServerRequest * request) {
    shouldReboot = !Update.hasError();
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", shouldReboot ? "OK" : "FAIL");
    response->addHeader("Connection", "close");
    request->send(response);


  }, [](AsyncWebServerRequest * request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (!index) {
      Serial.printf("Update Start: %s\n", filename.c_str());
      Update.runAsync(true);
      if (!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000)) {
        Update.printError(Serial);
      }
    }
    if (!Update.hasError()) {
      if (Update.write(data, len) != len) {
        Update.printError(Serial);
      }
    }
    if (final) {
      if (Update.end(true)) {
        Serial.printf("Update Success: %uB\n", index + len);
      } else {
        Update.printError(Serial);
      }
    }
  });



  // start server
  server.begin();


  // setup rfid pins
  setup_rfid_pins();
  setup_rfid();

  // setup letter list
  setUpLetterList();

  //setting up word list
  setUpWordList();

  //set system state
  set_system_state(SSWaitForPlayer);

  Serial.println(F("Setup done!!"));
}

void loop() {
  webSocket.cleanupClients();
  MDNS.update();

  switch (systemState) {
    case SSWaitForAnswer:
      //check rf
      check_rfid();
      break;
    case SSSolved:
      // now wait for removing of the
      if (rfid_change()) {
        // if all cards removed
        if (is_rfid_cards_clear()) {
          //get new word
          selectNewWord();
          set_system_state(SSWaitForAnswer);
          WS_SendStatsUpdate(true);
        }
      } //end if
      break;
    case SSLettersAdmin:
      card_admin_process();
      break;
  } //end switch
}


//==============================================================
//   handleUpload
//==============================================================
void handleUpload(AsyncWebServerRequest *request, String filename, String redirect, size_t index, uint8_t *data, size_t len, bool final) {
  if (!index) {
    if (!filename.startsWith("/")) filename = "/settings/" + filename;
    Serial.println((String)"UploadStart: " + filename);
    fsUploadFile = SPIFFS.open(filename, "w");            // Open the file for writing in SPIFFS (create if it doesn't exist)
  }
  for (size_t i = 0; i < len; i++) {
    fsUploadFile.write(data[i]);
    Serial.write(data[i]);
  }
  if (final) {
    Serial.println((String)"UploadEnd: " + filename);
    fsUploadFile.close();
    request->redirect(redirect);
  }
}



//==============================================================
//   Card_Admin_Process
//==============================================================
void card_admin_process() {
  if (rfid_check_first_reader()) {
    // lets check the status of data
    card_state = 0;
    // if card id is empty - no card
    if (currentCardId[0] + currentCardId[1] + currentCardId[2] + currentCardId[3] == 0) {
      Serial.println(F("NO CARD"));
    }
    // if we gto answer so its a known card
    else if (answer[0])
    {
      Serial.print(F("KNOWN CARD - "));
      Serial.println(answer[0]);
      card_state  = 1 ;
    }
    // if no answer its a new card
    else
    {
      Serial.println(F("NEW CARD -  "));
      Serial.print(currentCardId[0]);
      Serial.print(currentCardId[1]);
      Serial.print(currentCardId[2]);
      Serial.println(currentCardId[3]);
      card_state = 2;
    } //end if

    WS_SendSCardpdate(card_state, answer[0]);
  } //end if
} //emd card_admin_process


//==============================================================
//   CHECK RFID
//==============================================================
void check_rfid() {
  // if was solved
  if (rfid_change()) {
    if (is_rfid_solved()) {
      set_system_state(SSSolved);
    } //end if
    // send update to all the sockets
    WS_SendStatsUpdate(false);
  } //end if
} //end if


//==============================================================
//   SET SYSTEM STATE
//==============================================================
void set_system_state(byte newState) {
  switch (newState) {
    case SSWaitForPlayer:
      currentSelectedWordIndex = 0; // just to rest once
      break;
    case SSSolved:
      // do nthing for now
      break;

  } //end switch

  systemState = newState;
  systemStateMillis = millis();
} //end set_system_state
