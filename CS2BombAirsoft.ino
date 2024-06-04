#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <DFRobotDFPlayerMini.h>
#include <SoftwareSerial.h>

// BOMB STATES
const int BOMB_IDLE = 0;
const int BOMB_PLANTING = 1;
const int BOMB_PLANTED = 2;
const int BOMB_DEFUSED = 3;
const int BOMB_EXPLODING = 4;
const int BOMB_EXPLODED = 5;

// BOMB CONFIG
const int BOMB_TIME = 50; // seconds

// HARDWARE CONFIG
const int KEYPAD_ROWS = 4;
const int KEYPAD_COLS = 3;
char KEYPAD_KEYMAP[KEYPAD_ROWS][KEYPAD_COLS] = {
    {'1', '2', '3'},
    {'4', '5', '6'},
    {'7', '8', '9'},
    {'*', '0', '#'}
};
// SOFTWARE CONFIG
const int BOMB_CODE_LEGTH = 7;
const char BOMB_BEGIN_PLANT_BTN = '*';
const char BOMB_BEGIN_DEFUSE_BTN = '#';
const int BOMB_DEFUSE_TIME = 5; // seconds

// sounds
const int SOUND_LOC_T_EXPODED = 2;
const int SOUND_LOC_T_PLANTED = 5;
const int SOUND_LOC_CT_DEFUSED = 7;
const int SOUND_LOC_CT_DEFUSING = 4;
const int SOUND_LOC_CT_WIN = 3;
const int SOUND_LOC_T_WIN = 1;
const int SOUND_LOC_T_PLANTING = 6;

// PINS DECLARATION
byte PINS_KEYPAD_ROWS[KEYPAD_ROWS] = {9, 8, 7, 6};
byte PINS_KEYPAD_COLS[KEYPAD_COLS] = {13, 10, 5};

const int PIN_PIEZO = A0;
const int PIN_LED_RED = A3; 
const int PIN_LED_GREEN = A1;
const int PIN_LED_WHITE = A4;
const int PIN_LED_BLUE = A2;

const int PIN_RX_TX_SOUNDSYS[] = {11, 12};

// INTERNAL VARIABLES
int bombState = BOMB_IDLE;

int bombCurrentTimeLeft = 0;
long bombLastBeepTime = 0;
long bombDetonationStart = 0;
String bombPlantedPassword = "";

char keyPadLastKey = false;
char keyPadCurrentKey = false;
bool keyPadIsHolding = false;

bool isDefusing = false;
long defusingStartTime = 0;
int def_currentNumber = 0;
int def_currentAttempt = 0;
int def_lastNumberChange = 0;
bool needDisplayUpdate = true;

// init
LiquidCrystal_I2C lcd_1(0x27, 16, 2);
Keypad KEY_PAD = Keypad(makeKeymap(KEYPAD_KEYMAP), PINS_KEYPAD_ROWS, PINS_KEYPAD_COLS, KEYPAD_ROWS, KEYPAD_COLS);
DFRobotDFPlayerMini soundPlayer;
SoftwareSerial softSerial(PIN_RX_TX_SOUNDSYS[0], PIN_RX_TX_SOUNDSYS[1]); // RX, TX - data mezi MPR a Arduino

void setup(){
    lcd_1.init();
    lcd_1.backlight();
    pinMode(PIN_PIEZO, OUTPUT);
    pinMode(PIN_LED_GREEN, OUTPUT);
    pinMode(PIN_LED_RED, OUTPUT);
    pinMode(PIN_LED_WHITE, OUTPUT);
    pinMode(PIN_LED_BLUE, OUTPUT);
    bombCurrentTimeLeft = BOMB_TIME;

    softSerial.begin(9600);
    soundPlayer.begin(softSerial);
    delay(100);
    soundPlayer.volume(30);
    soundPlayer.play(SOUND_LOC_T_PLANTING);
}

void loop(){
    keyPadCurrentKey = KEY_PAD.getKey();
    if(keyPadCurrentKey){
        keyPadLastKey = keyPadCurrentKey;
    }
    keyPadIsHolding = (KEY_PAD.getState() == HOLD);
    updatePlanting();
    updateBombTime();
    updateBeepSound();
    updateDefusing();
    updateDisplay();
}
void updateDisplay(){
    if(!needDisplayUpdate) return;
    needDisplayUpdate = false;
    if(isDefusing){

    }
    if(bombState == BOMB_PLANTING){
        // display current password
        lcd_1.setCursor(0, 0);
        lcd_1.print("Set pass:");
        lcd_1.setCursor(0, 1);
        int passSize = bombPlantedPassword.length();
        for(int i = 0; i < passSize; i++){
            lcd_1.print(bombPlantedPassword.charAt(i));
        }
        for(int i = 0; i < BOMB_CODE_LEGTH - passSize; i++){
            lcd_1.print("_");
        }
        return;
    }
    if(bombState == BOMB_PLANTED || bombState == BOMB_EXPLODING){
        lcd_1.setCursor(0, 0);
        if(!isDefusing)
            lcd_1.print("#######");
        lcd_1.setCursor(0, 1);
        lcd_1.print("Explosion in: ");
        lcd_1.print(bombCurrentTimeLeft);
        lcd_1.print("s    ");
        return;
    }
    if(bombState == BOMB_IDLE){
        lcd_1.setCursor(0, 0);
        lcd_1.print("Press " + String(BOMB_BEGIN_PLANT_BTN));
        //Serial.println("cs");
    }
    if(bombState == BOMB_EXPLODED){
        clearDisplay();
        lcd_1.setCursor(0, 0);
        lcd_1.print("Bomb exploded!");
        lcd_1.setCursor(0, 1);
        lcd_1.print("Terorists win");
        return;
    }
    if(bombState == BOMB_DEFUSED){
        clearDisplay();
        lcd_1.setCursor(0, 0);
        lcd_1.print("Bomb defused!");
        lcd_1.setCursor(0, 1);
        lcd_1.print("Counter-Terorist win");
        return;
    }
}

void updateDefusingAnimation(){
    if(bombState == BOMB_DEFUSED) return;
    int attemptDelay = 10;
    if(millis() - def_lastNumberChange < attemptDelay) return;
    def_lastNumberChange = millis();
    def_currentAttempt++;
    char nextNumber = ' ';
    int loopI = 0;
    do {
        nextNumber = String("0123456789").charAt(random(0, 10));
        loopI++;
    } while(nextNumber == bombPlantedPassword.charAt(def_currentNumber) && loopI < 10);
    if(def_currentAttempt >= (((int)(BOMB_DEFUSE_TIME*1000/attemptDelay/BOMB_CODE_LEGTH)) + 1)){
        def_currentAttempt = 0;
        nextNumber = bombPlantedPassword.charAt(def_currentNumber);
        lcd_1.setCursor(def_currentNumber, 0);
        lcd_1.print(nextNumber);
        def_currentNumber++;
    }
    else {
        lcd_1.setCursor(def_currentNumber, 0);
        lcd_1.print(nextNumber);
    }
}

void clearDisplay(){
    lcd_1.clear();
}

void updatePlanting(){
    if(bombState != BOMB_IDLE && bombState != BOMB_PLANTING) return;
    if(bombState == BOMB_IDLE){
        if(keyPadCurrentKey != BOMB_BEGIN_PLANT_BTN) return;
        clearDisplay();
        bombState = BOMB_PLANTING;
        needDisplayUpdate = true;
    }
    else {
        if(keyPadCurrentKey && keyPadCurrentKey != BOMB_BEGIN_PLANT_BTN && keyPadCurrentKey != BOMB_BEGIN_DEFUSE_BTN){
            bombPlantedPassword += keyPadCurrentKey;
            needDisplayUpdate = true;
        }
        if(bombPlantedPassword.length() == BOMB_CODE_LEGTH){
            bombState = BOMB_PLANTED;
            clearDisplay();
            bombCurrentTimeLeft = BOMB_TIME;
            needDisplayUpdate = true;
            playPlantedSound();
        }
    }
}

void playPlantedSound(){
    updateDisplay();
    soundPlayer.play(SOUND_LOC_T_PLANTED);
}
void playDefusedSound(){
    updateDisplay();
    soundPlayer.play(SOUND_LOC_CT_DEFUSED);
    delay(2000);
    soundPlayer.play(SOUND_LOC_CT_WIN);
}
void playDefuseStartSound(){
    updateDisplay();
    soundPlayer.play(SOUND_LOC_CT_DEFUSING);
}
void playExplodeSound(){
    updateDisplay();
    soundPlayer.play(SOUND_LOC_T_EXPODED);
    delay(2000);
    soundPlayer.play(SOUND_LOC_T_WIN);
}

long lastBombCountTime = 0;
void updateBombTime(){
    if ((bombState == BOMB_PLANTED || bombState == BOMB_EXPLODING) && ((millis() - lastBombCountTime) >= 1000)){
        bombCurrentTimeLeft--;
        needDisplayUpdate = true;
        lastBombCountTime = millis();
    }
    if(bombCurrentTimeLeft < 0) bombCurrentTimeLeft = 0;
}

void updateBeepSound(){
    if (bombState == BOMB_PLANTED){
        if (millis() - bombLastBeepTime >= max((double)0.1 + (double)0.9 * ((double)bombCurrentTimeLeft / (double)BOMB_TIME), (double)0.15) * 1000){
            bombLastBeepTime = millis();
            tone(PIN_PIEZO, 1000, 10);
            digitalWrite(PIN_LED_RED, HIGH);
            delay(10);
            digitalWrite(PIN_LED_RED, LOW);
        }
        if(bombCurrentTimeLeft == 1){
            bombState = BOMB_EXPLODING;
            needDisplayUpdate = true;
        }
        return;
    }
    if(bombState == BOMB_EXPLODING){
        if (bombDetonationStart == 0){
            bombDetonationStart = millis();
        }
        long timeR = (millis() - bombDetonationStart);
        if(timeR <= 1500){
            analogWrite(PIN_LED_GREEN, ((double) timeR)/1500*1023);
        }

        if (timeR <= 500){
            tone(PIN_PIEZO, 1000 + timeR / 2, 100);
        }
        else {
            tone(PIN_PIEZO, 1500, 10);
            delay(100);
        }
        if(timeR >= 1500){
            bombState = BOMB_EXPLODED;
            needDisplayUpdate = true;
            analogWrite(PIN_LED_GREEN, 0);
            digitalWrite(PIN_LED_WHITE, HIGH);
            playExplodeSound();
        }
    }   
}

void updateDefusing(){
    if(bombState == BOMB_PLANTED || bombState == BOMB_EXPLODING){
        // safe to defuse
        bool isDefusingNow = ((keyPadIsHolding || keyPadCurrentKey == BOMB_BEGIN_DEFUSE_BTN) && (keyPadLastKey == BOMB_BEGIN_DEFUSE_BTN) && (bombCurrentTimeLeft > 0));
        if(isDefusing){
            if(!isDefusingNow){
                isDefusing = false;
                needDisplayUpdate = true;
                return;
            }

            if((millis() - defusingStartTime) > (BOMB_DEFUSE_TIME * 1000)){
                // bomb defused
                isDefusing = false;
                bombState = BOMB_DEFUSED;
                needDisplayUpdate = true;
                analogWrite(PIN_LED_GREEN, 0);
                digitalWrite(PIN_LED_BLUE, HIGH);
                playDefusedSound();
            }
            updateDefusingAnimation();
        }
        else {
            if(isDefusingNow){
                def_currentNumber = 0;
                def_currentAttempt = 0;
                isDefusing = true;
                defusingStartTime = millis();
                playDefuseStartSound();
            }
        }
        return;
    }
    isDefusing = false;
}