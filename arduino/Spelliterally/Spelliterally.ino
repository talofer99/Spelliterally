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
byte currentCardIdx;
byte card_state = 0;

#define MAX_RFIDCARDS 50
char letters[MAX_RFIDCARDS];
byte letters_id[MAX_RFIDCARDS][4];
byte lettersArrayActualSize;

#include <ArduinoJson.h>
#include <FS.h>   //Include File System Headers
#include "RFID.h"
#include "wordList.h"
#include "WS.h"

const char* host = "kesem";
const char ssid[] = SECRET_SSID;   // your network SSID (name)
const char password[] = SECRET_PASS;   // your network password

ESP8266WebServer server(80); //Server on port 80
ESP8266HTTPUpdateServer httpUpdater;
File fsUploadFile;              // a File object to temporarily store the received file


//==============================================================
//                  SETUP
//==============================================================
void setup(void) {
  Serial.begin(115200);

  //Connect to your WiFi router
  WiFi.begin(ssid, password);
  Serial.print(F("Connecting"));

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(F("."));
  }
  Serial.println(F("Done"));

  //Initialize File System
  if (SPIFFS.begin())  {
    Serial.println(F("SPIFFS Initialize....ok"));
  } else  {
    Serial.println(F("SPIFFS Initialization...failed"));
  } //edn if

  //If connection successful show IP address in serial monitor
  Serial.print(F("\n"));
  Serial.print(F("Connected to "));
  Serial.println(ssid);
  Serial.print(F("IP address: "));
  Serial.println(WiFi.localIP());  //IP address assigned to your ESP

  httpUpdater.setup(&server);
  WS_setup();

  server.serveStatic("/", SPIFFS, "/index.html");
  server.serveStatic("/Admin.html", SPIFFS, "/Admin.html");
  server.serveStatic("/AdminCards.html", SPIFFS, "/AdminCards.html");
  server.serveStatic("/cards.txt", SPIFFS, "/settings/cards.txt");
  server.on("/restoreCards", HTTP_POST, []() {
    server.send(200);
  }, handleFileUpload);
  server.serveStatic("/AdminWords.html", SPIFFS, "/AdminWords.html");
  server.serveStatic("/spell.json", SPIFFS, "/settings/spell.json");
   server.on("/restoreSpell", HTTP_POST, []() {
    server.send(200);
  }, handleFileUpload);
  
  server.serveStatic("/img", SPIFFS, "/img");
  server.serveStatic("/css", SPIFFS, "/css");
  server.serveStatic("/js", SPIFFS, "/js");

  server.begin();                  //Start server
  Serial.println(F("HTTP server started"));

  if (MDNS.begin(host)) {
    MDNS.addService("http", "tcp", 80);
    Serial.println(F("MDNS responder started"));
    Serial.print(F("You can now connect to http://"));
    Serial.print(host);
    Serial.println(F(".local"));
  }

  // setup rfid pins
  setup_rfid_pins();
  setup_rfid();


  // setup letter list
  Serial.println(F("Setting up letters"));
  setUpLetterList();
  Serial.println(F("DONE Setting up letters"));


  //setting up word list
  Serial.println(F("Setting up words"));
  setUpWordList();
  Serial.println(F("Done Setting up letters"));

  //set system state
  set_system_state(SSWaitForPlayer);

  Serial.println(F("Setup done!!"));
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
//   handle file upload
//==============================================================
void handleFileUpload() { // upload a new file to the SPIFFS
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (!filename.startsWith("/")) filename = "/settings/" + filename;
    Serial.print(F("handleFileUpload Name: ")); Serial.println(filename);
    fsUploadFile = SPIFFS.open(filename, "w");            // Open the file for writing in SPIFFS (create if it doesn't exist)
    filename = String();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize); // Write the received bytes to the file
  } else if (upload.status == UPLOAD_FILE_END) {
    if (fsUploadFile) {                                   // If the file was successfully created
      fsUploadFile.close();                               // Close the file again
      Serial.print(F("handleFileUpload Size: ")); Serial.println(upload.totalSize);

      if (server.uri() == "/restoreCards") {
        setUpLetterList();
        server.sendHeader("Location", "/AdminCards.html");     // Redirect the client to the success page
        server.send(303);
      } 
      else if (server.uri() ==  "/restoreSpell") {
        setUpWordList();
        server.sendHeader("Location", "/AdminWords.html");     // Redirect the client to the success page
        server.send(303);
      }
      else 
      {
        Serial.println(F("UPLOADED FROM UNKNOWN SOURCE !!!!!"));
      }

      //server.send(200, "text/plain", "Success");
    } else {
      server.send(500, "text/plain", "500: couldn't create file");
    }
  }
}



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
