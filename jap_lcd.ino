/*
 J.A.P. is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 J.A.P. is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details. <http://www.gnu.org/licenses/>
(c) Vladimir Razin 2017 info@3dlab.su
*/

#include <stepperQ.h>

#define DEFAULTBAUDRATE 115200
#define MAX_BUF        (64)

#define ACCEL_STP 32000
#define STP_PER_MM 1600
#define ZBUTTON_STEP 16

char buffer[MAX_BUF];
int sofar; 

int flag=1;
int led_flag=1;

int dir_pin = 7;
int step_pin = 4 ;
int led_pin=10;
int zendstop_plus=11;
int ignition=12;
char zbutton_plus=A0;
char zbutton_minus=A1;
char zmotor=A2;

unsigned long t0;

//G-code parser begin

float parsenumber(char code,float val) {
  char *ptr=buffer;
  while(ptr && *ptr && ptr<buffer+sofar) {
    if(*ptr==code) {
      return atof(ptr+1);
    }
    ptr=strchr(ptr,' ')+1;
  }
  return val;
} 

//G-code parser end

//G-code execution begin

void processCommand() {
  int cmd = parsenumber('G',-1);
  switch(cmd) {
    
    case  1: //G01 Zxxx Fxxx processing (Z-axis movement)
      stepperq.setMaxSpeed(STP_PER_MM*parsenumber('F',0)/60);
      stepperq.move(STP_PER_MM*parsenumber('Z',0));
      stepperq.start();
      Serial.write("Z_move_comp\n");
    break;
    
    case  4: //G04 Pxxx processing (pause P milliseconds)
      delay(parsenumber('P',0));
    break;
    
    default:  break;
  }
  cmd = parsenumber('M',-1);
  switch(cmd) {
     
  case 7: //M7 processing (cooler on)
    digitalWrite(A3,HIGH);
  break;
  
  case 9: //M9 processing (cooler off)
    digitalWrite(A3,LOW);
  break;
  
  case 17: //M17 processing (Z motor on)
    digitalWrite(8,LOW);
  break;
    
  case 18://M18 processing (Z motor off)   
    digitalWrite(8,HIGH);
  break;
  
  case 106:  
    digitalWrite(led_pin,LOW);
  break;

  case 107:  
    digitalWrite(led_pin,HIGH);
  break;
    
  case 245: //M245 processing (cooler on)
    digitalWrite(A3,HIGH);
  break;
    
  case 246: //M246 processing (cooler off)
    digitalWrite(A3,LOW);
  break;
  
    default:  break;
  }
} 

//G-code execution end

void setup() {
  Serial.begin(DEFAULTBAUDRATE);
  Serial.print("JAP LCD\n");
  sofar=0;
  stepperq.init(dir_pin, step_pin);
  stepperq.setAcceleration(ACCEL_STP);
  pinMode(led_pin, OUTPUT);
  digitalWrite(led_pin,HIGH);
  pinMode(13, OUTPUT);
  digitalWrite(13,HIGH);
  pinMode(8,OUTPUT);
  digitalWrite(8,HIGH);
  pinMode(A3, OUTPUT);
  digitalWrite(A3,LOW);
  pinMode(zendstop_plus, INPUT_PULLUP);
  //pinMode(zendstop_minus, INPUT_PULLUP);
  pinMode(zbutton_plus, INPUT_PULLUP);
  pinMode(zbutton_minus, INPUT_PULLUP);
  pinMode(zmotor, INPUT_PULLUP);
  pinMode(ignition, INPUT_PULLUP);
}

void loop() {
  if (digitalRead(zendstop_plus)== 0) {
    if(digitalRead(zmotor)==HIGH&&flag==0) {
      delay(50);
      digitalWrite(8,!digitalRead(8));
      flag=1;
    }
    if(digitalRead(zmotor)==LOW&&flag==1) {
      delay(50);
      flag=0;
    }
    if(digitalRead(ignition)==HIGH&&led_flag==0) {
      delay(50);
      digitalWrite(led_pin,!digitalRead(led_pin));
      led_flag=1;
    }
    if(digitalRead(ignition)==LOW&&led_flag==1) {
      delay(50);
      led_flag=0;
    }
    if ( stepperq.distanceToGo() == 0  ) {
      if (Serial.available() > 0) {
        char c=Serial.read();  
        if(sofar<MAX_BUF-1) buffer[sofar++]=c;  
        if(c=='\n') {
          buffer[sofar]=0; 
          processCommand();
          Serial.print("ok\n");
          sofar=0;
        }      
      }
      else {
        int sk=1;
        t0=millis();
        if (digitalRead(zbutton_plus)== 0 || digitalRead(zbutton_minus)== 0) {
          delay(50);
          digitalWrite(8,LOW);      
          while (digitalRead(zbutton_plus)== 0 && digitalRead(zendstop_plus)== 0) {
           stepperq.setMaxSpeed(STP_PER_MM*sk);
           stepperq.move(ZBUTTON_STEP);
           stepperq.start();
           if(millis() - t0 > 1000) sk=4;
          }
          while (digitalRead(zbutton_minus)== 0) {
            stepperq.setMaxSpeed(STP_PER_MM*sk);
            stepperq.move(-ZBUTTON_STEP);
            stepperq.start();
            if(millis() - t0 > 1000) sk=4;
          }
          delay(50);
          digitalWrite(8,HIGH);    
        }
      }
    }
  }
  else {
   stepperq.setMaxSpeed(STP_PER_MM*3);
   stepperq.move(-STP_PER_MM);
   stepperq.start();
   delay(1000);
   digitalWrite(8,HIGH);
  }  
}

