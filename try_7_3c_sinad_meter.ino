/*
主な改修点（try_7.3）

    DDS出力（GPIO5, Ticker割り込み）実装済み

    OLEDにSINADデカ文字表示が出ない問題：display()呼び出しの改善＋描画位置見直し

    1kHzノッチフィルターの性能評価に向けた出力とRMS計算

    入力RMS電圧計算と電圧配列保存（WAVE用）

    FFT機能（ArduinoFFT）修正済み、クラス名 ArduinoFFT に統一

    ロータリーエンコーダーによるモード切替（表示モード＋DDS ON/OFF）

    表示モード：SINAD／WAVE／FFT 切り替え

    GPIO6パルスでADC時間観測可能
*/
//  ** 7.3b 
//    enc 0:SINAD,1.WAVE in, 2. IIR parameter, 3. FFT in, 4.FFT notched
//    offset v 1385->1473
//    drawSINAD format 
//    display.drawLine(i, SCREEN_HEIGHT - 9, i, SCREEN_HEIGHT - bar - 9, SSD1306_WHITE);  // 8bit upper
//    print displayMode
//    On debug for elapsed timing 50uS??
//
//  ** 7.3c
//    displayMode=5: void drawPARAMmonitor() { --> elapsed, loopelapsed OK
//    ADC time over LOOP timing over  corresponding with 950Hz 
//    There is starting up unstability of IIR function
//
//  To be improved
//  1. Sample timing
//  2. Sine wave DDS
//  3. Notch precision
//  4. FFT leveling
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Encoder.h>
#include <arduinoFFT.h>
#include <Ticker.h>

// ========== ハード定義 ========== //
#define ADC_PIN      A0
#define DDS_PIN      5
#define ADC_MONITOR  6
#define OLED_ADDR    0x3C
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// ========== OLED ========== //
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ========== サンプリング設定 ========== //
#define SAMPLE_SIZE 512
float adcBuffer[SAMPLE_SIZE];      // To save the raw ADC values
float inputBuffer[SAMPLE_SIZE];    // To save the input voltage
float notchBuffer[SAMPLE_SIZE];    // To save the notched voltage
float vInRMS = 0.0;
float vNotchRMS = 0.0;
float sinadDB = 0.0;
uint32_t elapsed;
uint32_t loopelapsed;


double vReal[SAMPLE_SIZE], vImag[SAMPLE_SIZE];
// FFT
ArduinoFFT<double> FFT = ArduinoFFT<double>(vReal, vImag, (double)SAMPLE_SIZE, (double)20000.0);


// ========== DDS出力 ========== //
#define DDS_TABLE_SIZE 256
uint8_t ddsTable[DDS_TABLE_SIZE];
volatile int ddsIndex = 0;
bool ddsOutputEnabled = false;
Ticker ddsTicker;

void generateDDSTable() {
  for (int i = 0; i < DDS_TABLE_SIZE; i++) {
    ddsTable[i] = 127 + 127 * sin(2 * PI * i / DDS_TABLE_SIZE);
  }
}

void ddsISR() {
  if (ddsOutputEnabled) {
    analogWrite(DDS_PIN, ddsTable[ddsIndex]);
    ddsIndex = (ddsIndex + 1) % DDS_TABLE_SIZE;
  }
}

// ========== ノッチフィルタ（IIR） ========== //
// パラメータ例：Q=10, f=1kHz
float b0, b1, b2, a1, a2;
float x1 = 0, x2 = 0, y_1 = 0, y2 = 0;

void setupNotchFilter(float freq, float Q) {
  float Fs = 20000.0;
  float omega = 2.0 * PI * freq / Fs;
  float alpha = sin(omega) / (2.0 * Q);

  b0 = 1;
  b1 = -2 * cos(omega);
  b2 = 1;
  float a0 = 1 + alpha;
  a1 = -2 * cos(omega);
  a2 = 1 - alpha;

  // normalize
  b0 /= a0; b1 /= a0; b2 /= a0;
  a1 /= a0; a2 /= a0;
}

float applyNotchFilter(float x) {
  float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y_1 - a2 * y2;
  x2 = x1; x1 = x;
  y2 = y_1; y_1 = y;
  return y;
}

// ========== ロータリーエンコーダ ========== //
Encoder encoder(2, 3);
#define ENCODER_SW 4
volatile bool swFlag = false;
int displayMode = 0; // 0:SINAD 1:WAVE in,2:WVW notched, 3:FFT in, 4:FFT notched

void IRAM_ATTR onButtonPress() {
  swFlag = true;
}

// ========== RMS & SINAD ========== //
float calculateRMS(float* data, int size) {
  float sumSq = 0;
  for (int i = 0; i < size; i++) {
    sumSq += data[i] * data[i];
  }
  return sqrt(sumSq / size);
}

float calculateSINAD(float signalRMS, float noiseRMS) {
  if (noiseRMS == 0) return 100.0;
  return 20.0 * log10(signalRMS / noiseRMS);
}

// ========== 初期化 ========== //
void setup() {
  Serial.begin(115200);
  pinMode(DDS_PIN, OUTPUT);
  pinMode(ADC_MONITOR, OUTPUT);
  pinMode(ENCODER_SW, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENCODER_SW), onButtonPress, FALLING);
  generateDDSTable();
  ddsTicker.attach_us(50, ddsISR); // 20kHz相当

  setupNotchFilter(1000.0, 10.0); // Q=10仮設定

  Wire.begin();
  display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("SINADmeter     v1.0 ");  
  display.display();
  delay(1000);
}

// ========== 表示関数 ========== //
void drawSINAD() {
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(16, 0);
  display.print("SINAD ");
  display.setCursor(32, 25); 
  display.print(sinadDB, 1);
  display.print("dB");
}

void drawWAVE() {
  display.drawLine(0, 26, 127, 26, SSD1306_WHITE);  // 32->26
  for (int i = 0; i < SAMPLE_SIZE - 1; i++) {
    int x1 = map(i, 0, SAMPLE_SIZE - 1, 0, SCREEN_WIDTH - 1);
    int y1 = map(inputBuffer[i] * 1000, -1000, 1000, 0, SCREEN_HEIGHT - 10);
    int x2 = map(i + 1, 0, SAMPLE_SIZE - 1, 0, SCREEN_WIDTH - 1);
    int y2 = map(inputBuffer[i + 1] * 1000, -1000, 1000, 0, SCREEN_HEIGHT - 10);
    display.drawLine(x1, y1, x2, y2, SSD1306_WHITE);
  }
}

void drawWAVE_n() {                     // Wave after notched
  display.drawLine(0, 28, 127, 28, SSD1306_WHITE);  // 32->28
  for (int i = 0; i < SAMPLE_SIZE - 1; i++) {
    int x1 = map(i, 0, SAMPLE_SIZE - 1, 0, SCREEN_WIDTH - 1);
    int y1 = map(notchBuffer[i] * 1000, -1000, 1000, 0, SCREEN_HEIGHT - 10);
    int x2 = map(i + 1, 0, SAMPLE_SIZE - 1, 0, SCREEN_WIDTH - 1);
    int y2 = map(notchBuffer[i + 1] * 1000, -1000, 1000, 0, SCREEN_HEIGHT - 10);
    display.drawLine(x1, y1, x2, y2, SSD1306_WHITE);
  }
}


void drawFFT() {
 
  for (int i = 0; i < SAMPLE_SIZE; i++) {
    vReal[i] = inputBuffer[i];
    vImag[i] = 0;
  }
  FFT.windowing(vReal, SAMPLE_SIZE, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  FFT.compute(vReal, vImag, SAMPLE_SIZE, FFT_FORWARD);
  FFT.complexToMagnitude(vReal, vImag, SAMPLE_SIZE);

  for (int i = 1; i < SAMPLE_SIZE / 2; i++) {
    int bar = min(50, int(vReal[i] / 5));
    display.drawLine(i, SCREEN_HEIGHT - 9, i, SCREEN_HEIGHT - bar - 9, SSD1306_WHITE);  // 8bit upper
  }
}
void drawFFT_n() {
 
  for (int i = 0; i < SAMPLE_SIZE; i++) {
    vReal[i] = notchBuffer[i];
    vImag[i] = 0;
  }
  FFT.windowing(vReal, SAMPLE_SIZE, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  FFT.compute(vReal, vImag, SAMPLE_SIZE, FFT_FORWARD);
  FFT.complexToMagnitude(vReal, vImag, SAMPLE_SIZE);

  for (int i = 1; i < SAMPLE_SIZE / 2; i++) {
    int bar = min(50, int(vReal[i] / 5));
    display.drawLine(i, SCREEN_HEIGHT - 9, i, SCREEN_HEIGHT - bar - 9, SSD1306_WHITE);
  }
}

void drawPARAMmonitor() {
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.printf("b0:%.3f", b0);
  display.setCursor(0, 8);
  display.printf("b1:%.3f", b1);
  display.setCursor(0, 16);
  display.printf("b2:%.3f", b2);
  display.setCursor(64, 0);
  display.printf("a1:%.3f", a1);
  display.setCursor(64, 8);
  display.printf("a2:%.3f", a2);

  display.setCursor(0, 24);
  display.printf("adc time:%d", (int)elapsed);
  display.setCursor(0, 32);
  display.printf("loop time:%d", (int)loopelapsed);
  display.display();
}

void drawSmallInfo() {
  display.setTextSize(1);
  display.setCursor(0, SCREEN_HEIGHT - 8);
  display.printf("Vin:%.3fV %.1fdB %d", vInRMS, sinadDB, displayMode);
}

// ========== メインループ ========== //
void loop() {
  static unsigned long lastSampleTime = 0;
  static long lastEncoderPos = 0;

  // SW処理
  if (swFlag) {
    ddsOutputEnabled = !ddsOutputEnabled;
    Serial.println(ddsOutputEnabled ? "DDS ON" : "DDS OFF");
    swFlag = false;
  }

  // エンコーダ処理
  long pos = encoder.read();
  if (abs(pos - lastEncoderPos) > 6) {      // 3->5->6
    displayMode = (displayMode + 1) % 6;    // 3->5->6
    lastEncoderPos = pos;
  }

  // サンプリング周期制御（約100msごと）
  if (millis() - lastSampleTime >= 200) {
    lastSampleTime = millis();
   
      uint32_t t1 = micros();
    for (int i = 0; i < SAMPLE_SIZE; i++) {

      uint32_t t0 = micros();
      digitalWrite(ADC_MONITOR, HIGH);
      adcBuffer[i] =  analogRead(ADC_PIN);
  
      digitalWrite(ADC_MONITOR, LOW);
      elapsed = micros() - t0;              // To count time of ADC
      if (elapsed < 50) {
        delayMicroseconds(50 - elapsed);    // To tune for 20kHz=50uS
      }
    }
      loopelapsed = micros() - t1;  // 20kHz(50uS) * 512 

    for (int i = 0; i < SAMPLE_SIZE; i++) {
      inputBuffer[i] = ((float)(adcBuffer[i] - 1473) / 4096.0) * 3.3;  // offset:0.07v 87 1385->1473
      notchBuffer[i] = applyNotchFilter(inputBuffer[i]);
    }

    vInRMS = calculateRMS(inputBuffer, SAMPLE_SIZE);
    vNotchRMS = calculateRMS(notchBuffer, SAMPLE_SIZE);
    sinadDB = calculateSINAD(vInRMS, vNotchRMS);
    if (sinadDB < 0.0f) sinadDB = 0.0f;

    // 表示
    display.clearDisplay();
    switch (displayMode) {
      case 0: drawSINAD(); break;
      case 1: drawWAVE(); break;
      case 2: drawWAVE_n(); break;

      case 3: drawFFT(); break;
      case 4: drawFFT_n(); break;
      case 5: drawPARAMmonitor(); break; 
    }
    drawSmallInfo();
    display.display();
  }
}
