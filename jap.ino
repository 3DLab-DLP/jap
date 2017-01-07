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
#define ZBUTTON_STEP 640

char buffer[MAX_BUF];
int sofar; 

int servo_angle;                 
int pulse_width;

int flag=1;

int dir_pin = 7;
int step_pin = 4 ;
int servo_pin=12;
int zendstop_plus=11;
int zendstop_minus=12;
char zbutton_plus=A0;
char zbutton_minus=A1;
char zmotor=A2;

unsigned long t0;

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

void servoPulse(int servo_pin, int servo_angle) {
  pulse_width = (servo_angle * 11) + 500;  
  digitalWrite(servo_pin, HIGH);       
  delayMicroseconds(pulse_width);      
  digitalWrite(servo_pin, LOW);        
  delay(1);                        
}

void processCommand() {
  int cmd = parsenumber('G',-1);
  switch(cmd) {
    
    case  1:
      stepperq.setMaxSpeed(STP_PER_MM*parsenumber('F',0)/60);
      stepperq.move(STP_PER_MM*parsenumber('Z',0));
      stepperq.start();
    break;
    
    case  4:
      delay(parsenumber('P',0));
    break;
    
    default:  break;
  }
  cmd = parsenumber('M',-1);
  switch(cmd) {
    
  case 3:
    digitalWrite(servo_pin, LOW);
    for (int i=1;i<=180;i++){        
      servoPulse(servo_pin, parsenumber('S',0));
    }
  break;
  
  case 7:
    digitalWrite(A3,HIGH);
  break;
  
  case 9:
    digitalWrite(A3,LOW);
  break;
  
  case 17: 
    digitalWrite(8,LOW);
  break;
    
  case 18:  
    digitalWrite(8,HIGH);
  break;
    
  case 245:
    digitalWrite(A3,HIGH);
  break;
    
  case 246:
    digitalWrite(A3,LOW);
  break;
  
    default:  break;
  }
} 


void setup() {
  Serial.begin(DEFAULTBAUDRATE);
  sofar=0;
  stepperq.init(dir_pin, step_pin);
  stepperq.setAcceleration(ACCEL_STP);
  pinMode(8,OUTPUT);
  digitalWrite(8,HIGH);
  pinMode(A3,LOW);
  pinMode(zendstop_plus, INPUT_PULLUP);
  pinMode(zendstop_minus, INPUT_PULLUP);
  pinMode(zbutton_plus, INPUT_PULLUP);
  pinMode(zbutton_minus, INPUT_PULLUP);
  pinMode(zmotor, INPUT_PULLUP);
  pinMode(servo_pin, OUTPUT);
  digitalWrite(servo_pin, LOW);
  for (int i=1;i<=180;i++){        
    servoPulse(servo_pin, 0);
  } 
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

