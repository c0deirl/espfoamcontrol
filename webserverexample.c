#include <WiFi.h>

const char* ssid     = "CLAROQNK37";
const char* password = "9j98g4NuEaq496Yk";

WiFiServer server(80);

void setup()
{
    Serial.begin(115200);
    pinMode(5, OUTPUT);      // set the LED pin mode
    pinMode(4, OUTPUT); 
    delay(10);
    // We start by connecting to a WiFi network
    Serial.println();
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) 
    {
        delay(100);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected.");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    server.begin();
}

int value = 0;

void loop()
{
 WiFiClient client = server.available();   // listen for incoming clients

  if (client)                                // if you get a client,
  {                             
    
    Serial.println("New Client.");           // print a message out the serial port
    
    String currentLine = "";                // make a String to hold incoming data from the client
    
    while (client.connected())              // loop while the client's connected
    {            
      
      if (client.available())               // if there's bytes to read from the client,
      {             
        
        char c = client.read();             // read a byte, then
        
        Serial.write(c);                    // print it out the serial monitor
        //
        if (c == '\n')                      // if the byte is a newline character
        {                    

          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          //
          if (currentLine.length() == 0) 
          {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();

            // the content of the HTTP response follows the header:
            client.print("<head><title>Test Led http protocol</title></head>");
            client.print("<center><h1><b>ESP32 Web Control!!</b></h1></center>");
            client.print("<center><p><b>GREEN LED</b><a href=\"ON1\"><button>ON</button></a>&nbsp;<a href=\"OFF1\"><button>OFF</button></a></p></center>"); 
            client.print("<center><p><b>Yellow LED</b><a href=\"ON2\"><button>ON</button></a>&nbsp;<a href=\"OFF2\"><button>OFF</button></a></p></center>"); 
            client.print("<center><h1>Effects!!!!</h1></center>");
            client.print("<center><p>Blink<b>(2 Secs Aprox)</b><a href=\"BLINK\"><button>ON</button></center>");
            client.print("<center><p>Wave<b>(2 Secs Aprox)</b><a href=\"WAVE\"><button>ON</button></center>");
            // The HTTP response ends with another blank line:
            client.println();
            // break out of the while loop:
            break;
          } 
          //
          else 
          {    // if you got a newline, then clear currentLine:
            currentLine = "";
          }
          //
        } 
        //
        else if (c != '\r') // if you got anything else but a carriage return character,
        {  
          currentLine += c;      // add it to the end of the currentLine
        }
        //
        // Check to see if the client request was "GET /ON" or "GET /OFF":
        //
        if (currentLine.endsWith("GET /ON1")) 
        {
          digitalWrite(5, HIGH);// GET /ON turns the LED on
        }
        //
        if (currentLine.endsWith("GET /OFF1")) 
        {
          digitalWrite(5, LOW);// GET /OFF turns the LED off
        }
        if (currentLine.endsWith("GET /ON2")) 
        {
          digitalWrite(4, HIGH);// GET /ON turns the LED on
        }
        //
        if (currentLine.endsWith("GET /OFF2")) 
        {
          digitalWrite(4, LOW);// GET /OFF turns the LED off
        }
        //
        if (currentLine.endsWith("GET /BLINK"))
        { 
          
          digitalWrite(5, HIGH);
          digitalWrite(4, HIGH);
          delay(500);
          digitalWrite(5, LOW);
          digitalWrite(4, LOW);
          delay(500);
          digitalWrite(5, HIGH);
          digitalWrite(4, HIGH);
          delay(500);
          digitalWrite(5, LOW);
          digitalWrite(4, LOW);
          delay(500);
        }
        if (currentLine.endsWith("GET /WAVE"))
        {
          
          digitalWrite(5, HIGH);
          digitalWrite(4, LOW);
          delay(500);
          digitalWrite(5, LOW);
          digitalWrite(4, HIGH);
          delay(500);
          digitalWrite(5, HIGH);
          digitalWrite(4, LOW);
          delay(500);
          digitalWrite(5, LOW);
          digitalWrite(4, HIGH);
          delay(500);
          digitalWrite(4, LOW);          
        }
        //
      }
    }
    // close the connection:
    client.stop();
    Serial.println("Client Disconnected.");
  }
}
