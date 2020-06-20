#include <LiquidCrystal.h>
#include <EEPROM.h>
#include "Enerlib.h"
#include "HX711.h"

LiquidCrystal lcd(4, 5, 10, 11, 12, 13);
Energy        energy;
HX711         scale;

#define LC_PWR_PIN        7
#define LC_GL_PWR_PIN     8
#define SC_PWR_PIN        6
#define R_BUTTON_PIN      2
#define L_BUTTON_PIN      3

#define SS_DOUT_PIN       A2
#define SS_SCK_PIN        A1

#define SLEEP_DELAY       5000
#define SCALE_VALUE       1402.5

#define FILTER_ENABLE     0
#define WEIGHT_CACHE_SIZE 10

#define EEP_T_ADDR        0
#define EEP_L_ADDR        4

void EEPROMWritelong(int address, long value)
{
  //Decomposition from a long to 4 bytes by using bitshift.
  //One = Most significant -> Four = Least significant byte
  byte four = (value & 0xFF);
  byte three = ((value >> 8) & 0xFF);
  byte two = ((value >> 16) & 0xFF);
  byte one = ((value >> 24) & 0xFF);

  //Write the 4 bytes into the eeprom memory.
  EEPROM.write(address, four);
  EEPROM.write(address + 1, three);
  EEPROM.write(address + 2, two);
  EEPROM.write(address + 3, one);
}

//This function will return a 4 byte (32bit) long from the eeprom
//at the specified address to adress + 3.
long EEPROMReadlong(long address)
{
  //Read the 4 bytes from the eeprom memory.
  long four = EEPROM.read(address);
  long three = EEPROM.read(address + 1);
  long two = EEPROM.read(address + 2);
  long one = EEPROM.read(address + 3);

  //Return the recomposed long by using bitshift.
  return ((four << 0) & 0xFF) + ((three << 8) & 0xFFFF) + ((two << 16) & 0xFFFFFF) + ((one << 24) & 0xFFFFFFFF);
}

long  total_weight = 0;
long  last_weight  = 0;

long  getTotalWeight()
{
//  Serial.print("get:");
//  Serial.println(total_weight);
  return total_weight;
}

void reloadTotalWeight()
{
  total_weight = EEPROMReadlong(EEP_T_ADDR);
//  Serial.print("reload:");
//  Serial.println(total_weight);
}

void  setTotalWeight(long value)
{
  total_weight = value;
  EEPROMWritelong(EEP_T_ADDR, total_weight);
//  Serial.print("set:");
//  Serial.println(total_weight);
}

long  getLastWeight()
{
//  Serial.print("get:");
//  Serial.println(last_weight);
  return last_weight;
}

void reloadLastWeight()
{
  last_weight = EEPROMReadlong(EEP_L_ADDR);
//  Serial.print("reload:");
//  Serial.println(last_weight);
}

void  setLastWeight(long value)
{
  last_weight = value;
  EEPROMWritelong(EEP_L_ADDR, last_weight);
//  Serial.print("set:");
//  Serial.println(last_weight);
}

void wakeUpAction()
{
  detachInterrupt(0);
  digitalWrite(SC_PWR_PIN, HIGH);
  digitalWrite(LC_PWR_PIN, HIGH);
  digitalWrite(LC_GL_PWR_PIN, HIGH);
  reloadTotalWeight();
  reloadLastWeight();
//  Serial.begin(9600);
  lcd.begin(16, 2);
  scale.begin(SS_DOUT_PIN, SS_SCK_PIN);
  scale.set_scale(SCALE_VALUE);
  delay(500);
  scale.tare();
  sleepRefresh();
}

void sleepAction()
{
//  Serial.end();
  digitalWrite(SC_PWR_PIN, LOW);
  digitalWrite(LC_PWR_PIN, LOW);
  digitalWrite(LC_GL_PWR_PIN, LOW);
  attachInterrupt(0, INT0_ISR, HIGH);
  energy.PowerDown();
}

long sleepDelayPointer = 0;

void sleepRefresh()
{
  sleepDelayPointer = millis();
}

bool sleepCheck()
{
  if (millis() - sleepDelayPointer >= SLEEP_DELAY) return true;
  else return false;
}

void INT0_ISR(void)
{
  if (energy.WasSleeping())
  {
    wakeUpAction();
  }
  else
  {

  }
}

long weight_value_buff[WEIGHT_CACHE_SIZE];
char weight_value_pointer = 0;
long filter(long value)
{
#if FILTER_ENABLE
  long sum = 0;
  weight_value_buff[weight_value_pointer++] = value;
  if (weight_value_pointer == WEIGHT_CACHE_SIZE) weight_value_pointer = 0;
  for (char count = 0; count < WEIGHT_CACHE_SIZE; count++)
    sum += weight_value_buff[count];
  return (long)(sum / WEIGHT_CACHE_SIZE);
#else
  return value;
#endif
}



void setup() {


  pinMode(SC_PWR_PIN, OUTPUT);
  pinMode(LC_PWR_PIN, OUTPUT);
  pinMode(LC_GL_PWR_PIN, OUTPUT);
  pinMode(L_BUTTON_PIN, INPUT);
  pinMode(R_BUTTON_PIN, INPUT);
  sleepAction();
}

void rightButtonAction()
{
  scale.tare();
  sleepRefresh();
}

long leftButtonPressTime = 0;
bool leftButtonPressed  = false;
bool leftButtonClicked  = false;
void leftButtonClickAction()
{
//  Serial.println("click");
  long  last = getLastWeight();
  long  total = getTotalWeight();
  setTotalWeight(total - last);
  setLastWeight(0);
  lcd.clear();
}

void leftButtonPressAction()
{
//  Serial.println("Press");
  setTotalWeight(0);
  setLastWeight(0);
  lcd.clear();
}

long pre_weight_value = 0;
long pre_t_value = 0;
long pre_l_value = 0;
void loop() {
//  Serial.println(digitalRead(L_BUTTON_PIN));
  if (digitalRead(R_BUTTON_PIN) == 1) rightButtonAction();
  if (leftButtonClicked)
  {
    if (digitalRead(L_BUTTON_PIN) == 0)
    {
      leftButtonClicked = false;
      if (!leftButtonPressed)leftButtonClickAction();

    } else
    {
      if (millis() - leftButtonPressTime > 2000 && !leftButtonPressed)
      {
        leftButtonPressed = true;
        leftButtonPressAction();
      }
    }
  } else
  {
    if (digitalRead(L_BUTTON_PIN) == 1)
    {
      leftButtonClicked = true;
      leftButtonPressed = false;
      leftButtonPressTime = millis();
      sleepRefresh();
    }
  }

  if (scale.is_ready()) {
    
    long weight = filter(scale.get_units(10) * 10);
    if (abs(pre_weight_value - weight) > 2)
    {
      sleepRefresh();
      lcd.clear();
    }
    if (abs(pre_weight_value - weight) > 2 || weight == 0)
    {
      pre_weight_value = weight;
    }
    long t = getTotalWeight();
    long l = getLastWeight();
    String tStr = "T:" + String(t / 10) + "." + String(t % 10);
    lcd.setCursor(0, 0);
    lcd.print(tStr);
    String lStr = "L:" + String(l / 10) + "." + String(l % 10);
    lcd.setCursor(9, 0);
    lcd.print(lStr);
    float formatWeight = abs(pre_weight_value / 10.0f);
    lcd.setCursor(6, 1);
    lcd.print(formatWeight, 1);
    lcd.print("g");
  } else {

  }
  if (sleepCheck())
  {
    setLastWeight(abs(pre_weight_value));
    setTotalWeight(getTotalWeight() + abs(pre_weight_value));
    sleepAction();
  }
}
