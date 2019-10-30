#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include "BLE2902.h"
#include "BLEHIDDevice.h"
#include "HIDTypes.h"
#include "HIDKeyboardTypes.h"
#include <driver/adc.h>
#include "driver/pcnt.h"

#define ENCODER_PIN_A GPIO_NUM_13
#define ENCODER_PIN_B GPIO_NUM_14

#define PULSE_COUNT_UNIT PCNT_UNIT_0

#define ctrl_z GPIO_NUM_15

int16_t theCounter = 0;

static void initPulseCounter(void){
  pcnt_config_t pcnt_config = {
    ENCODER_PIN_A,
    ENCODER_PIN_B,
    PCNT_MODE_KEEP,
    PCNT_MODE_REVERSE,
    PCNT_COUNT_INC,
    PCNT_COUNT_DIS,
    PULSE_COUNT_UNIT,
    PCNT_CHANNEL_0
  };

  /* Initialize PCNT unit */
    pcnt_unit_config(&pcnt_config);

    /* Configure and enable the input filter */
    pcnt_set_filter_value(PULSE_COUNT_UNIT, 1023); 
    pcnt_filter_enable(PULSE_COUNT_UNIT);

    /* Initialize PCNT's counter */
    pcnt_counter_pause(PULSE_COUNT_UNIT);
    pcnt_counter_clear(PULSE_COUNT_UNIT);

    /* Everything is set up, now go to counting */
    pcnt_counter_resume(PULSE_COUNT_UNIT);
}

BLEHIDDevice* hid;
BLECharacteristic* input;
BLECharacteristic* output;

bool connected = false;

class MyCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer){
    connected = true;
    BLE2902* desc = (BLE2902*)input->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
    desc->setNotifications(true);
  }

  void onDisconnect(BLEServer* pServer){
    connected = false;
    BLE2902* desc = (BLE2902*)input->getDescriptorByUUID(BLEUUID((uint16_t)0x2902));
    desc->setNotifications(false);
  }
};

/*
 * This callback is connect with output report. In keyboard output report report special keys changes, like CAPSLOCK, NUMLOCK
 * We can add digital pins with LED to show status
 * bit 0 - NUM LOCK
 * bit 1 - CAPS LOCK
 * bit 2 - SCROLL LOCK
 */
class MyOutputCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* me){
    uint8_t* value = (uint8_t*)(me->getValue().c_str());
    ESP_LOGI(LOG_TAG, "special keys: %d", *value);
  }
};

void _taskServer(){
  BLEDevice::init("Lightroom Wheel");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyCallbacks());

  hid = new BLEHIDDevice(pServer);
  input = hid->inputReport(1); // <-- input REPORTID from report map
  output = hid->outputReport(1); // <-- output REPORTID from report map

  output->setCallbacks(new MyOutputCallbacks());

  std::string name = "JuHan Cha==-===-=======-====";
  hid->manufacturer()->setValue(name);

  hid->pnp(0x02, 0xe502, 0xa111, 0x0210);
  hid->hidInfo(0x00,0x02);

  BLESecurity *pSecurity = new BLESecurity();
//  pSecurity->setKeySize();
  pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND);

  const uint8_t report[] = {
    USAGE_PAGE(1),      0x01,       // Generic Desktop Ctrls
    USAGE(1),           0x06,       // Keyboard
    COLLECTION(1),      0x01,       // Application
    REPORT_ID(1),       0x01,        //   Report ID (1)
    USAGE_PAGE(1),      0x07,       //   Kbrd/Keypad
    USAGE_MINIMUM(1),   0xE0,
    USAGE_MAXIMUM(1),   0xE7,
    LOGICAL_MINIMUM(1), 0x00,
    LOGICAL_MAXIMUM(1), 0x01,
    REPORT_SIZE(1),     0x01,       //   1 byte (Modifier)
    REPORT_COUNT(1),    0x08,
    HIDINPUT(1),           0x02,       //   Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position
    REPORT_COUNT(1),    0x01,       //   1 byte (Reserved)
    REPORT_SIZE(1),     0x08,
    HIDINPUT(1),           0x01,       //   Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position
    REPORT_COUNT(1),    0x06,       //   6 bytes (Keys)
    REPORT_SIZE(1),     0x08,
    LOGICAL_MINIMUM(1), 0x00,
    LOGICAL_MAXIMUM(1), 0x65,       //   101 keys
    USAGE_MINIMUM(1),   0x00,
    USAGE_MAXIMUM(1),   0x65,
    HIDINPUT(1),           0x00,       //   Data,Array,Abs,No Wrap,Linear,Preferred State,No Null Position
    REPORT_COUNT(1),    0x05,       //   5 bits (Num lock, Caps lock, Scroll lock, Compose, Kana)
    REPORT_SIZE(1),     0x01,
    USAGE_PAGE(1),      0x08,       //   LEDs
    USAGE_MINIMUM(1),   0x01,       //   Num Lock
    USAGE_MAXIMUM(1),   0x05,       //   Kana
    HIDOUTPUT(1),          0x02,       //   Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile
    REPORT_COUNT(1),    0x01,       //   3 bits (Padding)
    REPORT_SIZE(1),     0x03,
    HIDOUTPUT(1),          0x01,       //   Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile
    END_COLLECTION(0)
  };

  hid->reportMap((uint8_t*)report, sizeof(report));
  hid->startServices();

  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->setAppearance(HID_KEYBOARD);
  pAdvertising->addServiceUUID(hid->hidService()->getUUID());
  pAdvertising->start();

  ESP_LOGD(LOG_TAG, "Advertising started!");
  //delay(portMAX_DELAY);

};

void setup() {
  Serial.begin(115200);
  Serial.println("Starting BLE work!");
  _taskServer();

  Serial.printf("Counter: %d\n", theCounter);
  initPulseCounter();

  pinMode(ctrl_z, INPUT_PULLUP);
}


void loop() {
  int16_t thisCount;
  pcnt_get_counter_value(PULSE_COUNT_UNIT, &thisCount);

  int ctrl_z_state = digitalRead(ctrl_z);
  
  if(thisCount > theCounter){
    theCounter = thisCount;
    Serial.printf("CW Counter: %d\n", theCounter);
    if(connected == true){
      KEYMAP map = {0x2e, 0};
      uint8_t msg[] = {map.modifier, 0x0, map.usage, 0x0, 0x0, 0x0, 0x0, 0x0};
      input->setValue(msg, sizeof(msg));
      input->notify();
    }
    delay(50);
  }

  if(thisCount < theCounter){
    theCounter = thisCount;
    Serial.printf("CCW Counter: %d\n", theCounter);
    if(connected == true){
      KEYMAP map = {0x2d, 0};
      uint8_t msg[] = {map.modifier, 0x0, map.usage, 0x0, 0x0, 0x0, 0x0, 0x0};
      input->setValue(msg, sizeof(msg));
      input->notify();
    }
    delay(50);
  }

  if(ctrl_z_state == LOW){
    Serial.println("Ctrl Z");
    if(connected = true){
      KEYMAP map = {0x1d, KEY_CTRL};
      uint8_t msg[] = {map.modifier, 0x0, map.usage, 0x0, 0x0, 0x0, 0x0, 0x0};
      input->setValue(msg, sizeof(msg));
      input->notify();
    }
    delay(100);
  }

  if(thisCount == theCounter && ctrl_z_state == HIGH){
    if(connected == true){
      uint8_t msg[] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
      input->setValue(msg, sizeof(msg));
      input->notify();
    }
    delay(100);
  }
  
}
