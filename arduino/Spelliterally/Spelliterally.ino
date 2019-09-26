/* Spelliterally - By Tal Ofer (talofer99@hotmail.com)
   The RFID spelling game, with web interface.
   RFID pinouts are in the RFID file
   secret.h contain your wifi info.
   you will need the SPIF upload tool and to define a SPIFF that will enought to uploade the files in the /data folder
*/


#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266mDNS.h>
#include "secrets.h"

#define SSWaitForPlayer 0
#define SSWaitForAnswer 1
#define SSWaitForSolve 2
#define SSSolved 3
#define SSLettersAdmin 99
byte systemState;
unsigned long systemStateMillis;
byte currentCardId[4];

#define MAX_RFIDCARDS 50
char letters[MAX_RFIDCARDS];
byte letters_id[MAX_RFIDCARDS][4];
byte lettersArrayActualSize;

#include <FS.h>   //Include File System Headers
#include "RFID.h"
#include "wordList.h"
#include "WS.h"

const char* host = "kesem";
const char ssid[] = SECRET_SSID;   // your network SSID (name)
const char password[] = SECRET_PASS;   // your network password

ESP8266WebServer server(80); //Server on port 80
ESP8266HTTPUpdateServer httpUpdater;



//==============================================================
//                  SETUP
//==============================================================
void setup(void) {
  Serial.begin(115200);

  //Connect to your WiFi router
  WiFi.begin(ssid, password);
  Serial.print("Connecting");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Done");

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
  WS_setup();

  server.serveStatic("/", SPIFFS, "/index.html");
  server.serveStatic("/adminCards/", SPIFFS, "/AdminCards.html");

  server.serveStatic("/img", SPIFFS, "/img");
  server.serveStatic("/css", SPIFFS, "/css");
  server.serveStatic("/js", SPIFFS, "/js");

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

  
  // setup letter list
  Serial.println("Setting up letters");
  setUpLetterList();
  Serial.println("DONE Setting up letters");

  
  //setting up word list
  Serial.println("Setting up words");
  setUpWordList();
  Serial.println("Done Setting up letters");

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
  WS_loop();

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

} //end loop

//==============================================================
//   Card_Admin_Process
//==============================================================
void card_admin_process() {
  if (rfid_check_first_reader()) {
    // lets check the status of data
    byte card_state = 0;
    // if card id is empty - no card
    if (currentCardId[0] + currentCardId[1] + currentCardId[2] + currentCardId[3] == 0) {
      Serial.println("NO CARD");
    }
    // if we gto answer so its a known card
    else if (answer[0])
    {
      Serial.print("KNOWN CARD - ");
      Serial.println(answer[0]);
      card_state  =1 ;
    }
    // if no answer its a new card
    else
    {
      Serial.println("NEW CARD -  ");
      Serial.print(currentCardId[0]);
      Serial.print(currentCardId[1]);
      Serial.print(currentCardId[2]);
      Serial.println(currentCardId[3]);
      card_state =2;
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
