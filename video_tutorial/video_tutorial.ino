/*
 * Created on Sept 5 2022
 *
 * Copyright (c) 2020 - Daniel Hajnal
 * hajnal.daniel96@gmail.com
 *
*/

#include <WiFi.h>

#include "Shellminator.hpp"
#include "Shellminator-IO.hpp"

#include "Commander-API.hpp"
#include "Commander-IO.hpp"

#include <ESP32Servo.h>

#define SERVER_PORT 11000
#define SERVO_PIN 33
#define RED_LED_PIN 32

// WiFi credentials.
const char* ssid     = "DIGI-b4vC";
const char* password = "vyFJ6mU8";

const char logo[] = 

"\r\n"
"  ______      __             _       __\r\n"
" /_  __/_  __/ /_____  _____(_)___ _/ /\r\n"
"  / / / / / / __/ __ \\/ ___/ / __ `/ / \r\n"
" / / / /_/ / /_/ /_/ / /  / / /_/ / /  \r\n"
"/_/  \\__,_/\\__/\\____/_/  /_/\\__,_/_/   \r\n"
"                                       \r\n"

;

void terminalClientTask( void * parameter );

// Prints a green, bold OK.
void printOK();

void abortHandler();

void joggingUpEvent();
void joggingDownEvent();

// Prototype functions for terminal commands.
void ledOn_func( char *args, commandResponse *response );
void ledOff_func( char *args, commandResponse *response );
void ledState_func( char *args, commandResponse *response );
void setServo_func( char *args, commandResponse *response );
void getServo_func( char *args, commandResponse *response );
void joggingOn_func( char *args, commandResponse *response );
void joggingOff_func( char *args, commandResponse *response );
void wifiStat_func( char *args, commandResponse *response );
void wifiScanner_func( char *args, commandResponse *response );
void reboot_func( char *args, commandResponse *response );

Shellminator shell( &Serial );

Commander commander;

Servo servo;

WiFiServer server( SERVER_PORT );

WiFiClient client;

Shellminator shellWiFi( &client );

// Commander API-tree
Commander::API_t API_tree[] = {
    apiElement( "ledOn", "Turn on the LED.", ledOn_func ),
    apiElement( "ledOff", "Turn off the LED", ledOff_func ),
    apiElement( "ledState", "Print out the state of the LED.", ledState_func ),
    apiElement( "setServo", "Set the position of the servo\r\n\targs:\r\n\t- position in degrees [ 0 - 180 ].", setServo_func ),
    apiElement( "getServo", "Print out the position of the servo.", getServo_func ),
    apiElement( "joggingOn", "Enable jogging with Up-Down arrows.", joggingOn_func ),
    apiElement( "joggingOff", "Disable jogging with Up-Down arrows.", joggingOff_func ),
    apiElement( "wifiStat", "Print WiFi statistics.", wifiStat_func ),
    apiElement( "wifiScanner", "Scanns nearby networks.", wifiScanner_func ),
    apiElement( "reboot", "Reboot the device.", reboot_func )
};


void setup() {

  // Initialise Serial with 115200 baudrate.
  Serial.begin( 115200 );

  shell.clear();
  shell.clear();

  // Startup message.
  Serial.println( "Program Start!" );

  // Servo init section
  Serial.printf( "[ %5d ] Servo init... ", millis() );
  servo.attach( SERVO_PIN );
  printOK();
  
  // LED init section
  Serial.printf( "[ %5d ] LED init... ", millis() );
  pinMode( RED_LED_PIN, OUTPUT );
  digitalWrite( RED_LED_PIN, 0 );
  printOK();

  Serial.printf( "[ %5d ] Commander init... ", millis() );
  commander.attachTree( API_tree );
  commander.init();
  
  shell.attachCommander( &commander );
  shell.overrideAbortKey( abortHandler );
  shell.attachLogo( logo );

  shellWiFi.attachCommander( &commander );
  shellWiFi.overrideAbortKey( abortHandler );
  shellWiFi.attachLogo( logo );

  printOK();

  // WiFi configuration section
  Serial.printf( "[ %5d ] Connect to  WiFi: ", millis() );
  Serial.print( ssid );

  WiFi.mode( WIFI_STA );
  WiFi.begin( ssid, password );

  while( WiFi.status() != WL_CONNECTED ){

    delay( 1000 );
    Serial.print( "." );

  }
  
  server.begin();
  
  printOK();

  Serial.println( "Connected!" );
  Serial.print( "Device IP: " );
  Serial.print( WiFi.localIP() );
  Serial.print( " at port: " );
  Serial.println( SERVER_PORT );
  
  // Startup finished message.
  Serial.println( "Startup sequence finished!\r\n" );

  // Initialise shell object.
  shell.begin( "demo" );

  xTaskCreate( terminalClientTask, "Terminal Client Task", 10000, NULL, 1, NULL );

}

void loop() {

  // Update terminal.
  shell.update();

}

void printOK(){

  Serial.print( "[ " );
  shell.setTerminalCharacterColor( Shellminator::BOLD, Shellminator::GREEN );
  Serial.print( "OK" );
  shell.setTerminalCharacterColor( Shellminator::REGULAR , Shellminator::WHITE );
  Serial.print( " ]\r\n" );

}

void ledOn_func( char *args, commandResponse *response ){

    digitalWrite( RED_LED_PIN, 1 );

}

void ledOff_func( char *args, commandResponse *response ){

    digitalWrite( RED_LED_PIN, 0 );

}

void ledState_func( char *args, commandResponse *response ){

  response -> print( "LED state: " );
  
  if( digitalRead( RED_LED_PIN ) ){

    response -> println( "ON" );
    
  }

  else{

    response -> println( "OFF" );
    
  }
  
  response -> println();
  
}

void setServo_func( char *args, commandResponse *response ){

  int servoPos = 0;
  int argCount = 0;

  argCount = sscanf( args, "%d", &servoPos );

  if( argCount != 1 ){

    response -> println( "No argument. See help!" );
    return;
    
  }

  if( servoPos < 0 ){

    servoPos = 0;
    
  }

  if( servoPos > 180 ){

    servoPos = 180;
    
  }

  servo.write( servoPos );
  
}

void getServo_func( char *args, commandResponse *response ){

  response -> print( "Servo position: " );
  response -> println( servo.read() );
  response -> println();

}

void joggingOn_func( char *args, commandResponse *response ){

  shell.overrideUpArrow( joggingUpEvent );
  shell.overrideDownArrow( joggingDownEvent );
  response -> println( "Jogging enabled! " );
  response -> println();

}

void joggingOff_func( char *args, commandResponse *response ){

  shell.freeUpArrow();
  shell.freeDownArrow();
  response -> println( "Jogging disabled! " );
  response -> println();

}

void wifiStat_func( char *args, commandResponse *response ){

  String localIP = WiFi.localIP().toString();
  char localIP_cahr_arr[ 20 ];

  localIP.toCharArray( localIP_cahr_arr, 20 );
  
  response -> print( "WiFi RSSI:\t" );
  response -> println( WiFi.RSSI() );
  response -> print( "IP address:\t" );
  response -> println( localIP_cahr_arr );
  response -> print( "Port:\t\t" );
  response -> println( SERVER_PORT );

}

void wifiScanner_func( char *args, commandResponse *response ){

  int i;
  int j;
  int j_old;
  char buff[ 32 ];
  
  response -> println( "---- Press any key to abort ----" );
  response -> println( "Available WiFi networks:" );

  j = WiFi.scanNetworks();

  while( response -> available() <= 0 ){

    for( i = 0; i < j; i++ ){

      WiFi.SSID(i).toCharArray( buff, 32 );
      response -> print( '\t' );
      response -> print( i + 1 );
      response -> print( ' ' );
      response -> println( buff );
      
    }

    j_old = j;

    j = WiFi.scanNetworks();

    if( response -> available() > 0 ){

      break;
      
    }

    for( i = 0; i < j_old; i++ ){

      // Clear the line.
      response -> write( 27 );
      response -> print( (const char*)"[2K" );
      
      // Move up the cursor.
      response -> write( 27 );
      response -> print( (const char*)"[A" );
      
    }    
    
  }

  response -> println( "Scan finished." );
  
}

void reboot_func( char *args, commandResponse *response ){

  response -> print( "Rebooting" );
  delay( 500 );
  response -> print( '.' );
  delay( 500 );
  response -> print( '.' );
  delay( 500 );
  response -> print( '.' );
  delay( 500 );
  ESP.restart();

}

void abortHandler(){

  // Safety commands first!
  servo.write( 90 );
  digitalWrite( RED_LED_PIN, 0 );

  Serial.println();
  Serial.println( "Abort key pressed!" );
  
}

void joggingUpEvent(){

  int servoPos;

  servoPos = servo.read();

  servoPos += 10;

  if( servoPos > 180 ){

    servoPos = 180;
    
  }

  servo.write( servoPos );
  
}

void joggingDownEvent(){
  
  int servoPos;

  servoPos = servo.read();

  servoPos -= 10;

  if( servoPos < 0 ){

    servoPos = 0;
    
  }

  servo.write( servoPos );
 
}

void terminalClientTask( void * parameter ){

  for( ;; ){

    client = server.available();
  
    if( client ){

      shellWiFi.clear();
      shellWiFi.begin( "WiFi" );

      while( client.connected() ){
  
          shellWiFi.update();
          vTaskDelay( 10 );
  
      }
      
    }

    vTaskDelay( 10 );
    
  }
  
}
