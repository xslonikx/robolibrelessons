#include <IRLibAll.h>

/* WARNING: These codes is for certain ir remote device only
 * You should dump and redefine it.
 * 
 * See IRLib2/dump example for how to do it
 */

/*
#define IR_UP           0xFD8877
#define IR_DOWN         0xFD9867
#define IR_LEFT         0xFD28D7
#define IR_RIGHT        0xFD6897
#define IR_OK           0xFDA857
#define IR_SHARP        0xFD708F
#define IR_GOT_NOTHING  0x0
#define IR_GOT_REPEAT   0xFFFFFFFF
*/

#define IR_UP           0xFF629D
#define IR_DOWN         0xFFA857
#define IR_LEFT         0xFF22DD
#define IR_RIGHT        0xFFC23D
#define IR_OK           0xFF02FD
#define IR_SHARP        0xFF52AD
#define IR_GOT_NOTHING  0x0
#define IR_GOT_REPEAT   0xFFFFFFFF


#define PIN_IR_IN       2
#define PIN_POWERLED    11
#define PIN_RGB_RED     3
#define PIN_RGB_GREEN   5
#define PIN_RGB_BLUE    6

#define DFL_COLOR_LEVEL 128

#define MIN_LEVEL       0
#define MAX_LEVEL       255

#define REPEAT_TIME     1000
#define BLINK_TIME      25
#define CHANGE_STEP     3

/* let's define the composite type for single RGB channel. 
 * It has two properties: 
 * - the pin that certain channel connected to
 * - the current intensity level
 */
typedef struct RGBLedChannel {
  unsigned short int pin;
  unsigned short int level;
} RGBLedChannel;


/* As can you look below, we'll use the array of RGBLedChannel's
 * for organize all channels in same place.
 * Let's also define the enumerable type for indexes of this array
 * for getting rid of remembering what index means what color and
 * just use human-comfort names. It's another way to organize constants
 */
enum RGBChanNums {
  CHAN_RED,
  CHAN_GREEN,
  CHAN_BLUE,
  /* the end of chans.
   * if something changes in the world and quantum theory,
   * add new channels above
   * The last one will just a constant MAX_COLOR_CHANS that just indicate
   * the end of enumerables and auto-recalculates if this list will be changed.
   */
  MAX_COLOR_CHANNELS
};


IRrecvPCI myReceiver(PIN_IR_IN); //create receiver and pass pin number
IRdecode myDecoder;              //create decoder object


/* Global variable that stores all channels */

RGBLedChannel colorChannels[3]  = {
  {PIN_RGB_RED,   DFL_COLOR_LEVEL}, 
  {PIN_RGB_GREEN, DFL_COLOR_LEVEL}, 
  {PIN_RGB_BLUE,  DFL_COLOR_LEVEL}
};

bool currentDeviceState = true;

/* user-defined functions */

/* This function turns on or off the RGB Led - literally it writes LOW to all channels if off 
 * and values stored to "channels" variable otherwise 
 */ 
void setRGBLedState(bool state) {
  /* Function switches RGB LED on and of depends on state param
   * When it turns LED on, it sets values configured earlier
   * 
   * @param state (bool) - State that device is need to switch to.
   */
  if (state) {
    digitalWrite(PIN_POWERLED, HIGH);    
    for (int curChan = CHAN_RED; curChan < MAX_COLOR_CHANNELS; curChan++) {
      analogWrite(colorChannels[curChan].pin, colorChannels[curChan].level); 
    }
  }
  else {
    digitalWrite(PIN_POWERLED, LOW); 
    for (int curChan = CHAN_RED; curChan < MAX_COLOR_CHANNELS; curChan++) {
      analogWrite(colorChannels[curChan].pin, LOW); 
    }
  }
}

unsigned long int getButtonCode() {
  /*
   * Function returns current button code received from IR device
   * If device sends 0xFFFFFFFF (button repeat), returns last valid code.
   * Remark: earlier we;
   * @return buttonValue (unsigned long int, default 0) - received value
   */

  // variable for storing last pressed button value
  static unsigned long int buttonValue = IR_GOT_NOTHING;
  static unsigned long int lastPress = 0;
    
  // Explicitly clear myReceiver object
  myReceiver.enableIRIn();
  
  if (myReceiver.getResults() && myDecoder.decode()) {
    if (myDecoder.value == IR_GOT_REPEAT) {
      if ((millis() - lastPress) > REPEAT_TIME) {
        buttonValue = IR_GOT_NOTHING;
        Serial.println("getButtonCode(): repeat timeout. Zeroizing buttonState... ");
      }
      else {
        Serial.print("getButtonCode(): Button pressed: ");
        Serial.println(myDecoder.value);
        Serial.print(" (aka repeat of last state): ");
        Serial.println(buttonValue);
      }
    }
    else {
      // Only if button code is not an repeat, update buttonValue and return
      buttonValue = myDecoder.value;
      Serial.print("getButtonCode(): Button pressed: ");
      Serial.println(buttonValue);
    }
    lastPress = millis();
    blinkPowerLED(); // blink led on every real button press
    return buttonValue;
  }
  return IR_GOT_NOTHING; 
}

void blinkPowerLED() {
  /* Just blinking the power led, as we did in BLink sketch 
  * Also we're explicitly save last state of led and return it after blinking
  * rather than assumpting that we can blink it only in ON mode
  */
  digitalWrite(PIN_POWERLED, LOW);
  delay(BLINK_TIME);
  digitalWrite(PIN_POWERLED, HIGH);
  delay(BLINK_TIME);
  digitalWrite(PIN_POWERLED, currentDeviceState);
}

void gotoConfigModeLoop() {
  /* Turns device to another operational mode - configuration mode. In config mode you can 
   * adjust each color channel separately. 
   */
  unsigned int curChannel = CHAN_RED;

  /* Go to infinite loop of reading values from IR Remote */

  Serial.println("\nEntering the configuration mode");
  while (1) {
    switch(getButtonCode()) {
      /* if we got nothing, just break the switch and iterate the loop above */
      case IR_GOT_NOTHING:
        break;
      
      /* when we press left, we decrease the current index of colorChannels[] array
       * to avoid of getting out of array bounds, we're explicitly set the minimal correct
       * value if we got out. 
       * Additional casting is used to handle overflow7  
       * 
       * don't forget to break the switch after action;
       */
      case IR_LEFT:
        curChannel--;
        if ((signed int)curChannel < CHAN_RED) {
          curChannel = CHAN_RED;
        }
        Serial.print("Current channel: ");
        Serial.println(curChannel);
        break;

      /* using the same logic there, but in another direction */
      case IR_RIGHT:
        curChannel++;
        if ((signed int)curChannel >= MAX_COLOR_CHANNELS) {
          curChannel = CHAN_BLUE;
        }
        Serial.print("Current channel: ");
        Serial.println(curChannel);
        break;

      /* when we press up/down, we use the similar logic, but instead of channels index,
       * we're changing the value of selected channel and verifying value the same way as above;
       */
      case IR_UP:
        colorChannels[curChannel].level += CHANGE_STEP;
        if ((signed int)colorChannels[curChannel].level > MAX_LEVEL) {
          colorChannels[curChannel].level = MAX_LEVEL;
        }
        /* In better world, RGB Led would be implemented as a class, like IR Receiver      
         * and instead of raw analogWrite() use something like RGBLed.Set(RED, VALUE)
         * 
         * We'll learn classes and OOP on our lessons in future, so now look how to 
         * do it in procedural way.
         */
        analogWrite(colorChannels[curChannel].pin, colorChannels[curChannel].level);
        Serial.print("Current value for channel ");
        Serial.print(curChannel);
        Serial.print(" : ");
        Serial.println(colorChannels[curChannel].level);
        break;

      case IR_DOWN:
        colorChannels[curChannel].level -= CHANGE_STEP; 
        if ((signed int)colorChannels[curChannel].level < MIN_LEVEL) {
          colorChannels[curChannel].level = MIN_LEVEL;
        }
        analogWrite(colorChannels[curChannel].pin, colorChannels[curChannel].level);
        Serial.print("Current value for channel ");
        Serial.print(curChannel);
        Serial.print(" : ");
        Serial.println(colorChannels[curChannel].level);
        break;
      
      /* using goto is a biggest shame in the world, however in C lang using it inside 
       * a single function for jumping off the nested loop/condition blocks is a good pattern      
       *  
       * At this case we're jumping off all switch(getButtonCode()) and while(1) by one step 
       * and go to end of func 
       */
      case IR_OK: goto post_cfg;
      default:
        Serial.println("Unknown button.");
        break;
    }
  }

  /* We're doin' nuthin' there, just exiting. 
   * However C language need an action, ever empty, after the label, so that's why semicolon there */
  post_cfg:
  Serial.print("Configuration is done.");
}

void toggleDeviceState() {
  /* Function sets device to state that opposite to current.
   * If it's on, it switches it off and vice versa.
   */
  currentDeviceState = !currentDeviceState;
  Serial.print("Set device mode: ");
  Serial.println(currentDeviceState);
  digitalWrite(PIN_POWERLED, currentDeviceState);
  setRGBLedState(currentDeviceState);
}


void setup() {
  /* Standard Arduino device initialization
   * Also writes something to console for being similar to real cool device 
   */
  Serial.begin(9600);

  // Write little beauty...
  Serial.println();
  Serial.println("------------------------------"); 

  pinMode(PIN_POWERLED,       OUTPUT);
  digitalWrite(PIN_POWERLED,  HIGH);
  Serial.println("Power LED is initialized.");

  for (int curChan = 0; curChan < MAX_COLOR_CHANNELS; curChan++) {
    pinMode(colorChannels[curChan].pin, OUTPUT);
    analogWrite(colorChannels[curChan].pin,  colorChannels[curChan].level); 
  }
  Serial.println("LED channels is initialized.");
  
  myReceiver.enableIRIn();
  Serial.println("IR receiver is initialized.");
  
  Serial.println("Device is ready.\n");
}

void loop() {
  /* Captain Obvious says us that loop() is infinite loop as is, so there's no need to wrap
   * switch in another loop like while(1)
   */
  switch(getButtonCode()) {
    case(IR_SHARP):
      /* enter to config mode only if device is on */
      if (currentDeviceState) {
        gotoConfigModeLoop();
      }
      break;

    case(IR_OK):
      toggleDeviceState();
      break;

    case(IR_GOT_NOTHING):
      break;

    default:
      break;
  }
}
