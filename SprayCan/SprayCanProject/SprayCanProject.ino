/**
   Sanjay Mistry
   Copyright (C) 2017

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include "credentials.h"

#define SERIAL_DEBUG

// Include API-Headers
extern "C" {
  // #include "ets_sys.h"
  // #include "os_type.h"
  // #include "osapi.h"
  // #include "mem.h"
#include "user_interface.h"
  // #include "cont.h"
}

void setup() {

  WiFi.forceSleepBegin();
  yield();  
  
#ifdef SERIAL_DEBUG
  Serial.begin(74880);
  delay(10);
  Serial.println();
#endif

}

void loop() {

  delay(100);

  if (connectToInternet()) {
    delay(1000);
    disconnectInternet();
  }

}

bool connectToInternet() {
  uint32_t time1 = millis();

  WiFi.forceSleepWake();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.println("----------------->>>>>");

  int count = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
    count += 1;
    if (count > 100) {
      return false;
      Serial.println("");
    }
  }
  
  uint32_t time2 = millis();

  Serial.println("");
  Serial.print("WiFi connected. took: ");
  Serial.println(time2 - time1);
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  return true;

}

void disconnectInternet() {
  Serial.println("WiFi disconnect.");
  WiFi.disconnect();
  WiFi.forceSleepBegin();  // send wifi to sleep to reduce POWER consumption
  yield();
}

