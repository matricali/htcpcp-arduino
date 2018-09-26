/*
Copyright (C) 2018 Jorge Matricali <jorgematricali@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <SPI.h>
#include <Ethernet.h>

#include "pot.h"

#define LISTEN_PORT 80

/* Prototipos */
void send_short_response(EthernetClient client, int status, String message);

byte mac[] = {0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02};
EthernetServer server(LISTEN_PORT);
pot_t *POT = NULL;

void setup()
{
  pinMode(PIN_LED_LINK, OUTPUT);
  pinMode(PIN_LED_STATUS_READY, OUTPUT);
  digitalWrite(PIN_LED_LINK, LOW);
  digitalWrite(PIN_LED_STATUS_READY, LOW);

  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  
  Serial.println("HTCPCP/1.0 Server");
  POT = (pot_t *) malloc(sizeof *POT);
  if (POT == NULL) {
    Serial.println("Cannot allocate POT");
    exit(1);
  }
  pot_init(POT);
  
  Serial.println("Obtaining an IP address...");
  Ethernet.begin(mac);
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
    for (;;) {
      delay(1);
    }
  }
  if (Ethernet.linkStatus() == LinkOFF) {
    Serial.println("Ethernet cable is not connected.");
  }

  server.begin();
  Serial.print("Server started at: ");
  Serial.print(Ethernet.localIP());
  Serial.print(":");
  Serial.println(LISTEN_PORT);

  digitalWrite(PIN_LED_LINK, HIGH);
  pot_refresh(POT);
}

void send_short_response(EthernetClient client, int status, String message)
{
  client.print("HTCPCP/1.0 ");
  client.print(status);
  client.print(" ");
  client.println(message);
  client.println("Content-Type: text/html");
  client.println("Connection: close");  // the connection will be closed after completion of the response
  client.println();
  client.print("<html><h1>");
  client.print(status);
  client.print(" ");
  client.print(message);
  client.println("</h1></html>");
}

void loop()
{
  EthernetClient client = server.available();
  if (client) {
    Serial.print("New incoming connection from ");
    Serial.println(client.remoteIP());

    String method = "";
    String path = "";
    String protocol = "";
    int state = 0;
    boolean currentLineIsBlank = true;

    while (client.connected() && client.available()) {
      char c = client.read();
      Serial.write(c);
      if (c == '\r' || c == '\n') {
        goto bad_request;
      }
      if (c == ' ') {
        /* Continuamos con el path */
        break;
      }
      method.concat(c);
    }

    while (client.connected() && client.available()) {
      char c = client.read();
      Serial.write(c);
      if (c == '\r' || c == '\n') {
        goto bad_request;
      }
      if (c == ' ') {
        /* Continuamos con el protocolo */
        break;
      }
      path.concat(c);
    }

    while (client.connected() && client.available()) {
      char c = client.read();
      Serial.write(c);
      if (c == '\r') {
        continue;
      }
      if (c == '\n') {
        /* Continuamos con los headers */
        break;
      }
      protocol.concat(c);
    }
    
    /* An http request ends with a blank line */
    while (client.connected() && client.available()) {
      char c = client.read();
      Serial.write(c);
      if (c == '\n' && currentLineIsBlank) {
        break;
      }
      if (c == '\n') {
        currentLineIsBlank = true;
      } else if (c != '\r') {
        currentLineIsBlank = false;
      }
    }

    /* Handle method */
    if (!method.equals("BREW") && !method.equals("POST")
        && !method.equals("GET") && !method.equals("PROPFIND")) {
      send_short_response(client, 501, "Not Implemented");
      goto cleanup;
    }

    if (!path.equals("/pot-1")) {
      /* Only 1 pot :D */
      send_short_response(client, 404, "Pot Not Found");
      goto cleanup;
    }

    if (method.equals("BREW") && !method.equals("POST")) {
      pot_refresh(POT);

      if (POT->status == POT_STATUS_BREWING) {
        Serial.println("POT BUSY!");
        send_short_response(client, 510, "Pot Busy");
        goto cleanup;
      }
      
      Serial.println("Brewing...");
      pot_brew(POT);
      send_short_response(client, 200, "OK");
      goto cleanup;
    }

    send_short_response(client, 501, "Not Implemented");
    goto cleanup;

bad_request:
    send_short_response(client, 400, "Bad Request");

cleanup:
    delay(1);
    client.stop();
    Serial.print("Client disconnected.");
  }
}
