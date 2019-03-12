/*
 J.A.P. is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 J.A.P. is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details. <http://www.gnu.org/licenses/>
(c) Alexander Shvetsov, 2019, shv-box@mail.com
    based on work of Vladimir Razin 2017 info@3dlab.su
*/

#include <stepperQ.h>

/// Movement settings
#define STEPS_PER_MM 1600
//#define MOVE_BT_STEPS 32 // 0.02 mm (32 / 1600)

#define MAX_SPEED 5 // mm/s
#define ACC_Z 10    // mm/s2
#define TIMEOUT 10  // minutes

/// Bounds
#define Z_MIN_MM 0
#define Z_MAX_MM 185

/// Pins
#define EN_PIN 8
#define DIR_PIN 7
#define STEP_PIN 4

#define MIN_ENDSTOP 11

#define LED_PIN 10
#define COOLER_PIN A3

#define BT_LED_PIN 12 // LED toggle button
#define BT_MOTOR_PIN A2 // Motor on/off button
#define BT_UP_PIN A0 // Move up button
#define BT_DOWN_PIN A1 // Move down button

/// Serial
#define BAUDRATE 115200
#define BUF_MAX_IDX 255 // Input buffer size - 1

/// Button debounce
#define DEBOUNCE 200UL

/// Debug
//#define DEBUG

const float TRAVEL_SPEED_MMS = MAX_SPEED;
const float TRAVEL_SPEED_STP = MAX_SPEED * STEPS_PER_MM;
const unsigned long STEPPER_TIMEOUT = 60UL * 1000UL * TIMEOUT;
const float ACC_STEPS_S2 = float(ACC_Z) * STEPS_PER_MM;

const char MSG_OK[] = "ok";
const char MSG_ERR[] = "!!";

typedef enum {
    FAIL = 0,
    OK = 1,
    UNSUPPORTED = 2,
    UNOPERATIONAL = 3
} CmdStatus;

/// Stepper
unsigned long motorTime; // Stepper timeout start time

/// Motor control buttons
bool motorIsOn;
unsigned long motorBtTime;
bool motorBtWasPressed;

unsigned long moveBtTime;
bool moveBtWasPressed;

/// LED control buttons
bool ledIsOn;
unsigned long ledBtTime;
bool ledBtWasPressed;

/// Command input buffer 
char buffer[BUF_MAX_IDX + 1];
int bufBound; // Buffer current bound index

/// Z positioning
bool isAbsolute;    // Move coordinates is absolute or relative. Default is 'true'
float z;            // Current Z-axis position
bool isOperational; // Current Z-axis position is actual

/// Serial input parser
float parseValue(char code, float def) {
    char *ptr = buffer;
    while(ptr && *ptr && ptr < buffer + bufBound) {
        if (*ptr == code) {
            return atof(ptr + 1);
        }
        ptr = strchr(ptr, ' ') + 1;
    }
    return def;
}

void enableMotor() {
    if (!motorIsOn) {
        digitalWrite(EN_PIN, LOW);
        motorIsOn = true;
    }
    motorTime = millis();
}

void disableMotor() {
    if (motorIsOn) {
        digitalWrite(EN_PIN, HIGH);
        if (stepperq.distanceToGo() != 0) {
            long pos = stepperq.currentPosition();
            stepperq.setCurrentPosition(pos);
            z = float(pos) / STEPS_PER_MM;
        }
        motorIsOn = false;
        isOperational = false;
    }
}

CmdStatus processCommand() {
#if defined(DEBUG)
    Serial.println(buffer);
#endif
    int cmd = parseValue('G', -1);
    
    switch(cmd) {
    case 1: { // G1 Zxxx Fxxx (Z-axis movement)
        if (!isOperational) {
            return UNOPERATIONAL;
        }
        
        float gZ = parseValue('Z', -1);
        if (gZ != -1) {
            float newZ = isAbsolute ? gZ : (z + gZ);
            /// Check limits
            if (newZ < Z_MIN_MM) {
                newZ = Z_MIN_MM;
            } else if (newZ > Z_MAX_MM) {
                newZ = Z_MAX_MM;
            }
            
            float speed = parseValue('F', 0.0) / 60;
            if (speed == 0.0 || speed > TRAVEL_SPEED_MMS) {
                speed = TRAVEL_SPEED_MMS;
            }
            
            /// Move
            enableMotor();
            stepperq.setMaxSpeed(speed * STEPS_PER_MM);
            stepperq.move((newZ - z) * STEPS_PER_MM);
            stepperq.start();
            
            z = newZ;
            Serial.println("Z_move_comp");
        }
    }
        return OK;
        
    case 4: // G04 Pxxx (Pause P milliseconds)
        delay(parseValue('P', 0));
        return OK;
        
    case 90: // G90 (Set to absolute positioning)
        isAbsolute = true;
        return OK;
        
    case 91: // G91 (Set to relative positioning)
        isAbsolute = false;
        return OK;
        
    case 92: { // G92 Znnn (Set Z axis position)
        z = parseValue('Z', 0);
#if defined(DEBUG)
        Serial.println(long(z * STEPS_PER_MM));
#endif
        stepperq.setCurrentPosition(long(z * STEPS_PER_MM));
        enableMotor();
        isOperational = true;
    }
        return OK;
        
    default:
        break;
    }
    
    cmd = parseValue('M', -1);
    switch(cmd) {
    case 7:   // M7 (Mist coolant on)
    case 245: // M245 (Start cooler)
        digitalWrite(COOLER_PIN, HIGH);
        break;
        
    case 9:   // M9 (Coolant off)
    case 246: // M246 (Stop cooler)
        digitalWrite(COOLER_PIN, LOW);
        break;
        
    case 17: // M17 (Enable/Power all stepper motors)
        enableMotor();
        break;
        
    case 18: // M18 (Disable all stepper motors)
    case 84: // M84 (Stop idle hold)
        disableMotor();
        break;
        
    case 3:     // M3 (Spindle On, Clockwise)
    case 4:     // M4 (Spindle On, Counter-Clockwise)
    case 106: { // M106 (Fan On)
        ledIsOn = true;
        digitalWrite(LED_PIN, LOW);
    }
        break;
        
    case 5:     // M5 (Spindle Off)
    case 107: { // M106 (Fan Off)
        ledIsOn = false;
        digitalWrite(LED_PIN, HIGH);
    }
        break;
        
    case 114: { // M114 (Get current position)
        Serial.print(isOperational ? MSG_OK : MSG_ERR);
        Serial.print(" C: Z:");
        Serial.print(z);
        Serial.print(" Count Z:");
        Serial.println(float(stepperq.currentPosition()) / STEPS_PER_MM);
    }
        break;
        
    default:
        return UNSUPPORTED;
    }
    
    return OK;
}

void setup() {
    Serial.begin(BAUDRATE);
    Serial.println("JAP LCD");
    bufBound = 0;
    
    stepperq.init(DIR_PIN, STEP_PIN);
    stepperq.setAcceleration(ACC_STEPS_S2);
    
    motorTime = 0;
    isAbsolute = true;
    z = 0.0;
    isOperational = false;
    
    pinMode(EN_PIN, OUTPUT);
    digitalWrite(EN_PIN, HIGH);
    motorIsOn = false;
    
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN,HIGH);
    ledIsOn = false;
    
    pinMode(COOLER_PIN, OUTPUT);
    digitalWrite(COOLER_PIN,LOW);
    
    pinMode(MIN_ENDSTOP, INPUT_PULLUP);
    
    pinMode(BT_UP_PIN, INPUT_PULLUP);
    pinMode(BT_DOWN_PIN, INPUT_PULLUP);
    moveBtTime = 0;
    moveBtWasPressed = false;

    pinMode(BT_MOTOR_PIN, INPUT_PULLUP);
    motorBtTime = 0;
    motorBtWasPressed = false;

    pinMode(BT_LED_PIN, INPUT_PULLUP);
    ledBtTime = 0;
    ledBtWasPressed = false;
}

void loop() {
    /// Motor timeout
    if (motorIsOn && (millis() - motorTime) > STEPPER_TIMEOUT) {
        disableMotor();
    }
    
    /// Motor button
    bool motorBtPressed = digitalRead(BT_MOTOR_PIN) == HIGH;
    if (motorBtPressed && !motorBtWasPressed && (millis() - motorBtTime) > DEBOUNCE) {
        if (motorIsOn) {
            disableMotor();
        } else {
            enableMotor();
        }
    }
    motorBtWasPressed = motorBtPressed;
    
    /// LED button
    bool ledBtPressed = digitalRead(BT_LED_PIN) == HIGH;
    if (ledBtPressed && !ledBtWasPressed && (millis() - ledBtTime) > DEBOUNCE) {
        ledIsOn = !ledIsOn;
        ledBtTime = millis();
        digitalWrite(LED_PIN, ledIsOn ? LOW : HIGH);
    }
    ledBtWasPressed = ledBtPressed;
    
    if (digitalRead(MIN_ENDSTOP) == LOW) {
        if (stepperq.distanceToGo() != 0) return;
        
        int count = Serial.available();
        int i = 0;
        while (i < count && bufBound < BUF_MAX_IDX) {
            char c = Serial.read();
            if (c == '\n') {
                buffer[bufBound++] = 0;
                String msg = "";
                switch (processCommand()) {
                case OK:
                    msg = MSG_OK;
                    break;
                case UNSUPPORTED:
                    msg = String(MSG_ERR) + ": Unsupported command \"" + String(buffer) + "\"";
                    break;
                case UNOPERATIONAL:
                    msg = String(MSG_ERR) + ": Printer is not operational. Z-axis position must be setted. Use G92.";
                    break;
                default:
                    msg = MSG_ERR;
                    break;
                }
                Serial.println(msg);
                bufBound = 0;
                
            } else {
                buffer[bufBound++] = c;
            }
            ++i;
        }
        
        /// Manual moves
        if (count == 0) {
            bool upBtPressed = digitalRead(BT_UP_PIN) == LOW;
            bool downBtPressed = digitalRead(BT_DOWN_PIN) == LOW;
            bool moveBtPressed = upBtPressed || downBtPressed;
            
            if (moveBtPressed && !moveBtWasPressed) {
                moveBtTime = millis();
            }
            
            if (moveBtPressed && (millis() - moveBtTime) > DEBOUNCE) {
                bool motorWasOn = motorIsOn;
                int moveDirection = upBtPressed ? 1 : -1;
                long maxSteps = (moveDirection > 0 ? (Z_MAX_MM - z) : (z - Z_MIN_MM)) * ((long) STEPS_PER_MM);
#if defined(DEBUG)
                Serial.print("maxSteps = ");
                Serial.println(maxSteps);
#endif

                if (maxSteps > 0L) {
                    enableMotor();
                    
                    if (moveDirection > 0) {
                        // Stage 1
                        stepperq.setMaxSpeed(0.5 * STEPS_PER_MM);
                        stepperq.move(moveDirection * STEPS_PER_MM);
                        stepperq.start();
                        while (moveBtPressed && digitalRead(MIN_ENDSTOP) == LOW && stepperq.distanceToGo() != 0) {
                            delay(50);
                            moveBtPressed = digitalRead(moveDirection == 1 ? BT_UP_PIN : BT_DOWN_PIN) == LOW;
                        }
                        maxSteps -= STEPS_PER_MM;
                        
                        // Stage 2
                        if (moveBtPressed && digitalRead(MIN_ENDSTOP) == LOW) {
                            stepperq.setMaxSpeed(STEPS_PER_MM);
                            stepperq.move(moveDirection * STEPS_PER_MM);
                            stepperq.start();
                            while (moveBtPressed && digitalRead(MIN_ENDSTOP) == LOW && stepperq.distanceToGo() != 0) {
                                delay(50);
                                moveBtPressed = digitalRead(moveDirection == 1 ? BT_UP_PIN : BT_DOWN_PIN) == LOW;
                            }
                            maxSteps -= STEPS_PER_MM;
                        }
                    }
                    
                    // Stage 3
                    if (moveBtPressed && digitalRead(MIN_ENDSTOP) == LOW) {
                        stepperq.setMaxSpeed(TRAVEL_SPEED_STP);
                        stepperq.move(moveDirection * maxSteps);
                        stepperq.start();
                        while (moveBtPressed && digitalRead(MIN_ENDSTOP) == LOW && stepperq.distanceToGo() != 0) {
                            delay(50);
                            moveBtPressed = digitalRead(moveDirection == 1 ? BT_UP_PIN : BT_DOWN_PIN) == LOW;
                        }
                    }
                    
                    // Stop if moving
                    if (stepperq.distanceToGo() != 0) {
                        stepperq.stop();
                    }
                    // Let's finish move
                    while (stepperq.distanceToGo() != 0) delay(5);

                    z = float(stepperq.currentPosition()) / STEPS_PER_MM;

                    if (!motorWasOn) {
                        disableMotor();
                    }
                }
 
#if defined(DEBUG)
                Serial.print("z = ");
                Serial.println(z);
#endif
            }
            moveBtWasPressed = moveBtPressed;

//            if (moveBtPressed && (millis() - moveBtTime) > DEBOUNCE) {
//                isOperational = false;
//                bool motorWasOn = motorIsOn;
//                int moveDirection = upBtPressed ? 1 : -1;
//                const float RATE1 = 0.5;
//                const float RATE2 = 1.0;
//                const float RATE_MAX = 5.0;
//                float rate = RATE1;
                
//                enableMotor();
                
//                while (moveBtPressed && digitalRead(MIN_ENDSTOP) == LOW) {
//                    if (rate < RATE_MAX) {
//                        unsigned long btDownTime = millis() - moveBtTime;
//                        rate = btDownTime < 1000 ? RATE1 :
//                              (btDownTime < 3000 ? RATE2 : RATE_MAX);
//                    }
                    
//                    stepperq.setMaxSpeed(rate * STEPS_PER_MM);
//                    stepperq.move(moveDirection * MOVE_BT_STEPS);
//                    stepperq.start();
                    
//                    moveBtPressed = digitalRead(moveDirection == 1 ? BT_UP_PIN : BT_DOWN_PIN) == LOW;
//                }
                
//                if (!motorWasOn) {
//                    while (stepperq.distanceToGo() != 0) delay(5); // Let's finish move
//                    disableMotor();
//                }
//            }
//            moveBtWasPressed = moveBtPressed;
        }
        
    } else {
        enableMotor();
        
        stepperq.setMaxSpeed(STEPS_PER_MM * 3);
        stepperq.move(-STEPS_PER_MM);
        stepperq.start();
        
        delay(1000);
        disableMotor();
    }
}
