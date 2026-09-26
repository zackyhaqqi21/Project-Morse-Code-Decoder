#include <Wire.h>
#include <LiquidCrystal_AIP31068_I2C.h>

// ======================================================================
// VERSI DISEDERHANAKAN: cuma 1 fitur -> ketukan (titik/garis) dibaca dari
// tombol, digabung jadi kode morse, lalu otomatis diterjemahkan jadi huruf.
// Tidak ada lagi menu, mode ketik manual (KeyPad), atau fitur playback.
//
// Alamat I2C LCD: 0x3E (chip AIP31068). Samakan dengan properti "I2C Address"
// di komponen LCD SimulIDE (62 desimal = 0x3E).
// ======================================================================
LiquidCrystal_AIP31068_I2C lcd(0x3E, 16, 2);

// Pin Tombol
const int buttonPin = 2;   // Tombol ketuk (input Morse: titik/garis)
const int resetPin  = 7;   // Tombol reset
const int spacePin  = 8;   // Tombol spasi antar kata
const int buzzerPin = 9;   // Buzzer feedback ketukan

// LED RGB -- indikator titik (Hijau), garis (Merah), huruf selesai (Merah+Hijau)
const int rgbR = A0;
const int rgbG = A1;

bool isPressed        = false;
bool resetPressed     = false;
bool lastSpaceState   = HIGH;
bool rgbIsOn          = false;

unsigned long pressStart        = 0;
unsigned long lastReleaseTime   = 0;
unsigned long rgbOnTime         = 0;
unsigned long lastSpaceDebounce = 0;

bool letterDecoded = true;

String currentSymbol = "";  // titik/garis yang lagi dibangun jadi 1 huruf
String message       = "";  // huruf-huruf hasil terjemahan

const unsigned long DOT_DASH_LIMIT = 250; // < ini = titik, >= ini = garis
const unsigned long LETTER_GAP     = 700; // jeda diam sebelum simbol diterjemahkan
const unsigned long RGB_DURATION   = 400;

struct MorseCode {
  char letter;
  const char* code;
};

MorseCode table[] = {
  {'A', ".-"},   {'B', "-..."},  {'C', "-.-."},  {'D', "-.."},
  {'E', "."},    {'F', "..-."},  {'G', "--."},   {'H', "...."},
  {'I', ".."},   {'J', ".---"},  {'K', "-.-"},   {'L', ".-.."},
  {'M', "--"},   {'N', "-."},    {'O', "---"},   {'P', ".--."},
  {'Q', "--.-"}, {'R', ".-."},   {'S', "..."},   {'T', "-"},
  {'U', "..-"},  {'V', "...-"},  {'W', ".--"},   {'X', "-..-"},
  {'Y', "-.--"}, {'Z', "--.."}
};

const int TABLE_SIZE = sizeof(table) / sizeof(MorseCode);

void buzzerOff() {
  noTone(buzzerPin);
  digitalWrite(buzzerPin, LOW);
}

void setRGB(bool r, bool g) {
  digitalWrite(rgbR, r ? HIGH : LOW);
  digitalWrite(rgbG, g ? HIGH : LOW);
}

void flashRGB(bool r, bool g) {
  setRGB(r, g);
  rgbOnTime = millis();
  rgbIsOn   = true;
}

char decode(String code) {
  for (int i = 0; i < TABLE_SIZE; i++) {
    if (code == table[i].code) return table[i].letter;
  }
  return '?';
}

void refreshLCD() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Input: " + currentSymbol);
  lcd.setCursor(0, 1);
  String shown = message;
  if (shown.length() > 16) {
    shown = shown.substring(shown.length() - 16);
  }
  lcd.print(shown);
}

void showReady() {
  message = "";
  currentSymbol = "";
  buzzerOff();
  setRGB(false, false);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("MORSE DECODER");
  lcd.setCursor(0, 1);
  lcd.print("Ketuk utk mulai");
}

void setup() {
  pinMode(rgbR, OUTPUT); digitalWrite(rgbR, LOW);
  pinMode(rgbG, OUTPUT); digitalWrite(rgbG, LOW);
  pinMode(buzzerPin, OUTPUT); digitalWrite(buzzerPin, LOW);

  buzzerOff();
  setRGB(false, false);

  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(resetPin,  INPUT_PULLUP);
  pinMode(spacePin,  INPUT_PULLUP);

  isPressed      = false;
  resetPressed   = false;
  lastSpaceState = HIGH;
  rgbIsOn        = false;

  lcd.init();
  showReady();
}

void loop() {
  // Reset Sistem (Pin 7)
  bool resetNow = (digitalRead(resetPin) == LOW);
  if (resetNow && !resetPressed) resetPressed = true;
  if (!resetNow && resetPressed) {
    resetPressed = false;
    showReady();
    return;
  }

  if (rgbIsOn && (millis() - rgbOnTime >= RGB_DURATION)) {
    setRGB(false, false);
    rgbIsOn = false;
  }

  // Tombol Spasi (Pin 8) -- selesaikan huruf yang lagi dibangun, lalu tambah spasi
  bool currentSpaceState = digitalRead(spacePin);
  if (currentSpaceState == LOW && lastSpaceState == HIGH && (millis() - lastSpaceDebounce > 50)) {
    lastSpaceDebounce = millis();
    tone(buzzerPin, 600, 150);

    if (!letterDecoded && currentSymbol.length() > 0) {
      message      += decode(currentSymbol);
      currentSymbol = "";
      letterDecoded = true;
    }

    if (message.length() > 0 && message.charAt(message.length() - 1) != ' ') {
      message += ' ';
      refreshLCD();
    }
  }
  lastSpaceState = currentSpaceState;

  // Input Morse Manual (Pin 2) -- tekan sebentar = titik, tekan lama = garis
  bool pressedNow = (digitalRead(buttonPin) == LOW);

  if (pressedNow && !isPressed) {
    isPressed  = true;
    pressStart = millis();
    tone(buzzerPin, 1000);
  }

  if (!pressedNow && isPressed) {
    isPressed = false;
    buzzerOff();

    unsigned long duration = millis() - pressStart;

    if (duration > 40) { // anti getar/debounce
      if (duration < DOT_DASH_LIMIT) {
        currentSymbol += ".";
        flashRGB(false, true); // Hijau = titik
      } else {
        currentSymbol += "-";
        flashRGB(true, false); // Merah = garis
      }
      lastReleaseTime = millis();
      letterDecoded   = false;
      refreshLCD();
    }
  }

  // Auto-decode saat idle -- kalau diam cukup lama, simbol diterjemahkan jadi huruf
  if (!isPressed) {
    unsigned long gap = millis() - lastReleaseTime;

    if (!letterDecoded && currentSymbol.length() > 0 && gap >= LETTER_GAP) {
      message      += decode(currentSymbol);
      currentSymbol = "";
      letterDecoded = true;
      flashRGB(true, true); // Merah+Hijau = huruf berhasil diterjemahkan
      refreshLCD();
    }
  }
}