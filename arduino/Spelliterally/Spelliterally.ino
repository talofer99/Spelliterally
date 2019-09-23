#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266mDNS.h>
#include "secrets.h"
#include <FS.h>   //Include File System Headers
#include "wordList.h"
#include "RFID.h"

const char* host = "kesem";
const char ssid[] = SECRET_SSID;   // your network SSID (name)
const char password[] = SECRET_PASS;   // your network password

ESP8266WebServer server(80); //Server on port 80
ESP8266HTTPUpdateServer httpUpdater;

#define LED 2  //On board LED


#define SSWaitForPlayer 0
#define SSWaitForAnswer 1
#define SSWaitForSolve 2
#define SSSolved 3




byte systemState;
unsigned long systemStateMillis;
byte currentSelectedWordIndex;

//==============================================================
//                  SETUP
//==============================================================
void setup(void) {
  Serial.begin(115200);

  WiFi.begin(ssid, password);     //Connect to your WiFi router
  Serial.println("");

  //Onboard LED port Direction output
  pinMode(LED, OUTPUT);

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  //Initialize File System
  if (SPIFFS.begin())  {
    Serial.println("SPIFFS Initialize....ok");
  } else  {
    Serial.println("SPIFFS Initialization...failed");
  } //edn if

  //If connection successful show IP address in serial monitor
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());  //IP address assigned to your ESP

  httpUpdater.setup(&server);


  server.serveStatic("/", SPIFFS, "/index.html");
  server.serveStatic("/img", SPIFFS, "/img");
  server.serveStatic("/css", SPIFFS, "/css");
  server.serveStatic("/js", SPIFFS, "/js");
  server.on("/newword.json", handleNewWord);
  server.on("/worddata.json", returnImageDataJson);
  server.on("/sync.json", handleSync);


  server.begin();                  //Start server
  Serial.println("HTTP server started");


  if (MDNS.begin(host)) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("MDNS responder started");
    Serial.print("You can now connect to http://");
    Serial.print(host);
    Serial.println(".local");
  }

  // setup rfid pins
  setup_rfid_pins();
  setup_rfid();


  //setting up word list
  setUpWordList();

  //set system state
  set_system_state(SSWaitForPlayer);

  Serial.println("Setup done!!");
}


//==============================================================
//                     LOOP
//==============================================================

void loop(void) {
  //Handle client requests
  server.handleClient();
  //MDNS.update();


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
        }
      } //end if
      break;
  } //end switch

} //end loop




//==============================================================
//   CHECK RFID
//==============================================================
void check_rfid() {
  // if was solved
  if (rfid_change()) {
    if (is_rfid_solved()) {
      set_system_state(SSSolved);
    } //end if
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



//==============================================================
//  RETURN IMAGE DATA JSON
//==============================================================

void returnImageDataJson() {
  String jsonResponse = "{\"word\":\"";
  jsonResponse.concat(spellWordList[currentSelectedWordIndex]);
  jsonResponse.concat("\",");
  jsonResponse.concat("\"path\":\"");
  // now lets get the path
  File wordFile = SPIFFS.open("/spell/" + spellWordList[currentSelectedWordIndex] + ".txt", "r");
  while (wordFile.available()) {
    jsonResponse.concat((char)wordFile.read());
  }
  wordFile.close();
  jsonResponse.concat("\"}");


  server.send(200, "text/plane", jsonResponse);
}



//==============================================================
//   SELECT NEW WORD
//==============================================================
void selectNewWord() {
  byte newRandomWordIndex = currentSelectedWordIndex;
  // ***********************************
  while (newRandomWordIndex == currentSelectedWordIndex) {
    newRandomWordIndex = random(0, spellWordListLength);
  } // while they are the same
  // set new one
  currentSelectedWordIndex = newRandomWordIndex;
  setNewQuestion(spellWordList[currentSelectedWordIndex]); //set new word
}

//==============================================================
//   HANDLE NEW WORD
//==============================================================
void handleNewWord() {
  selectNewWord();
  returnImageDataJson();

  // SET state to wait for answer
  set_system_state(SSWaitForAnswer);
}

//==============================================================
//   HANDLE SYNC
//==============================================================
void handleSync() {
  String answerString = "";
  for (int i = 0; i < NR_OF_READERS; i++) {
    if (answer[i] == 0) {
      answerString += " ";
    } else {
      answerString += (char)answer[i] ;
    }
  }
  
  String jsonResponse = "{\"state\":";
  jsonResponse.concat(String(systemState));
  jsonResponse.concat(",\"q\":\"");
  jsonResponse.concat(spellWordList[currentSelectedWordIndex]);
  jsonResponse.concat("\",\"a\":\"");
  jsonResponse.concat(answerString);
  jsonResponse.concat("\"}");
  //Serial.print(jsonResponse);
  server.send(200, "text/json", jsonResponse);
}
