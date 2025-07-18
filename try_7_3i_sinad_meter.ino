//  ** 7.3b 
//  ** 7.3c
//  ** 7.3d
//  ** 7.3e
//  7.3f
//  100points, sine: GPIO6
//  7.3g  2025.06.18
//   memori WAVE&FFT
//  7.3h  2025.06.18
//   synchro display not check
//　　DDS rec wave on/off OK
//  7.3i  2025.06.19
//    further study WAVE size
//    1000/200Hz test WAVE for RE-SW on when reset
//  To be checked
//  1.  Sine wave DDS OSCILLO view pending OK restored
//  2.  WAVE triggered Neeh:\nob\AIRBAND RECEIVER\R909-DSP4\DSP4-VER2\DSP4-VER2.inod to check
//
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Encoder.h>
#include <arduinoFFT.h>
#include <Ticker.h>

// ========== Pin assignment ========== //
#define ADC_PIN      A0
#define DDS_PIN      5
#define PULSE_PIN    6
#define ADC_LOOP_MONITOR  7
#define OLED_ADDR    0x3C
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define FUNC_SW       1

// ========== OLED ========== //
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ========== Data sampling ========== //
#define SAMPLE_SIZE 512
#define SAMPLING_RATE 10000.0
float adcBuffer[SAMPLE_SIZE];      // To save the raw ADC values
float inputBuffer[SAMPLE_SIZE];    // To save the input voltage
float notchBuffer[SAMPLE_SIZE];    // To save the notched voltage
float vInRMS = 0.0;
float vNotchRMS = 0.0;
float sinadDB = 0.0;
uint32_t elapsed;                  // ADC operation time uS
uint32_t loopelapsed;              // Timerresult 10kHz * 512 samples shall be 51200 
int sinevolume = 1;                    // Test sine wave multitude

double vReal[SAMPLE_SIZE], vImag[SAMPLE_SIZE];
// FFT
ArduinoFFT<double> FFT = ArduinoFFT<double>(vReal, vImag, (double)SAMPLE_SIZE, (double)SAMPLING_RATE);   // 10kHz sampling

// ========== TEST WAVE ========== //
bool testwaveEnabled = false;       // test TEST TEST
float testwaveSelect;              // 1kHz or 200Hz
// Pulse enable (1kHz pulse)
static bool toggle = false;

// ========== DDS setting========== //
bool ddsOutputEnabled = false;
Ticker ddsTicker;
#define DDS_TABLE_SIZE 256
uint8_t ddsTable[DDS_TABLE_SIZE];
volatile uint32_t phaseAccumulator = 0;
volatile uint32_t phaseIncrement = 0;
volatile uint8_t pulseCounter = 0;

void generateDDSTable() {
  for (int i = 0; i < DDS_TABLE_SIZE; i++) {
    ddsTable[i] = 127 + 127 * sin(2 * PI * i / DDS_TABLE_SIZE);
  }
}

void setDDSFrequency(float frequency) {
  // DDS refresh 2000Hz fixed
  phaseIncrement = (uint32_t)((frequency * DDS_TABLE_SIZE * 65536.0) / 2000.0);
}

void ddsISR() {
  if (ddsOutputEnabled) {
    phaseAccumulator += phaseIncrement;
    uint16_t index = (phaseAccumulator >> 16) & 0xFF;
    analogWrite(DDS_PIN, ddsTable[index]);
// pulse wave for GPIO6
    pulseCounter++;
    if (pulseCounter >= 10) {  // 10回ごとにパルス反転 (10kHz → 1kHz)
      toggle = !toggle;
      digitalWrite(PULSE_PIN, toggle ? HIGH : LOW); 
      pulseCounter = 0;
    }

  } else {
    // ddsOutputEnabled OFF: stop（DDS output 0V, pulse LOW fixed
    analogWrite(DDS_PIN, 0);
    digitalWrite(PULSE_PIN, LOW);

  }

}

// ========== Notch filter（IIR） ========== //
// Example parameter：Q=20, f=1kHz
float b0, b1, b2, a1, a2;
float x1 = 0, x2 = 0, y_1 = 0, y2 = 0;

void setupNotchFilter(float freq, float Q) {
  float Fs = SAMPLING_RATE;                         // 10kHz sampling
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

// ========== RE ========== //
Encoder encoder(2, 3);
#define ENCODER_SW 4
volatile bool swFlag = false;
int displayMode = 0; // 0:SINAD 1:WAVE in,2:WVW notched, 3:FFT in, 4:FFT notched

void IRAM_ATTR onButtonPress() {
  swFlag = true;
}

// ========== RMS & SINAD ========== // To discard the top 100 samples
float calculateRMS(float* data, int size) {
  if (size <= 50) return 0.0; // For safety
  float sumSq = 0;
  int neglectCount = 100;     // To neglect prior 100 samples
  int count = size - neglectCount;
  for (int i = neglectCount; i < size; i++) {
    sumSq += data[i] * data[i];
  }
  return sqrt(sumSq / count);
}

float calculateSINAD(float signalRMS, float noiseRMS) {
  if (noiseRMS == 0) return 100.0;
  return 20.0 * log10(signalRMS / noiseRMS);
}

// ========== Starting ========== //
void setup() {
  Serial.begin(115200);
  pinMode(DDS_PIN, OUTPUT); 
  pinMode(PULSE_PIN, OUTPUT); 
  digitalWrite(PULSE_PIN, LOW);
  pinMode(ADC_LOOP_MONITOR, OUTPUT);
  pinMode(ENCODER_SW, INPUT_PULLUP);
 
  attachInterrupt(digitalPinToInterrupt(ENCODER_SW), onButtonPress, FALLING);

  pinMode(FUNC_SW, INPUT_PULLUP);
  if( digitalRead(FUNC_SW) == 0 ) testwaveEnabled = true;       // test TEST TEST
  generateDDSTable();
  ddsTicker.attach_us(50, ddsISR);      // 20kHz compatible

  setupNotchFilter(1000.0, 10.0);       // Q=10

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

// ========== Display ========== //
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
  int syncIndex = findSyncIndex(inputBuffer, SAMPLE_SIZE);
  display.drawLine(16, 26, 127, 26, SSD1306_WHITE);  // 32->26, avoid overlap 16
  drawWAVEMEMORI();
  for (int i = 0; i < SAMPLE_SIZE - 1; i++) {
    int idx1 = (syncIndex + i) % SAMPLE_SIZE;       
    int idx2 = (syncIndex + i + 1) % SAMPLE_SIZE;   

    int x1 = map(i, 0, SAMPLE_SIZE - 1, 0, SCREEN_WIDTH - 1);
    int y1 = map(inputBuffer[idx1] * 1000, -1000, 1000, 0, SCREEN_HEIGHT - 10);
    int x2 = map(i + 1, 0, SAMPLE_SIZE - 1, 0, SCREEN_WIDTH - 1);
    int y2 = map(inputBuffer[idx2] * 1000, -1000, 1000, 0, SCREEN_HEIGHT - 10);
    display.drawLine(x1, y1, x2, y2, SSD1306_WHITE);
  }
}

void drawWAVE_n() {                     // Wave after notched
  int syncIndex = findSyncIndex(inputBuffer, SAMPLE_SIZE);
  display.drawLine(16, 26, 127, 26, SSD1306_WHITE);  // 32->26, avoid overlap 16
  drawWAVEMEMORI();
  for (int i = 0; i < SAMPLE_SIZE - 1; i++ ) {       
    int idx1 = (syncIndex + i) % SAMPLE_SIZE;        
    int idx2 = (syncIndex + i + 1) % SAMPLE_SIZE;    

    int x1 = map(i, 0, SAMPLE_SIZE - 1, 0, SCREEN_WIDTH - 1);
    int y1 = map(notchBuffer[idx1] * 1000, -1000, 1000, 0, SCREEN_HEIGHT - 10);
    int x2 = map(i + 1, 0, SAMPLE_SIZE - 1, 0, SCREEN_WIDTH - 1);
    int y2 = map(notchBuffer[idx2] * 1000, -1000, 1000, 0, SCREEN_HEIGHT - 10);
    display.drawLine(x1, y1, x2, y2, SSD1306_WHITE);
  }
}

int findSyncIndex(float data[], int sampleCount) {
  float threshold = 0.0; // ゼロクロス
  for (int i = 1; i < sampleCount; i++ ) {
    float prev = data[i - 1];
    float curr = data[i];
    if (prev < threshold && curr >= threshold) {
      return i;
    }
  }
  return 0;
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
    display.drawLine(i, SCREEN_HEIGHT - 16, i, SCREEN_HEIGHT - bar - 16, SSD1306_WHITE);  // 8bit upper
  }
  drawFFTMEMORI();  
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
    display.drawLine(i, SCREEN_HEIGHT - 16, i, SCREEN_HEIGHT - bar - 16, SSD1306_WHITE);
  }
  drawFFTMEMORI();  
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
  display.setCursor(0, 40);
  display.printf(ddsOutputEnabled ? "DDS ON" : "DDS OFF");  
  display.setCursor(0, 48);
  display.printf("Notched RMS:%.3f", vNotchRMS);
}

void drawSmallInfo() {
  display.setTextSize(1);
  display.setCursor(0, SCREEN_HEIGHT - 8);
  display.printf("Vin:%.3fV %.1fdB %d", vInRMS, sinadDB, displayMode);
}
void drawWAVEMEMORI() {
// ★ ここに数値目盛り追加 ★
display.setTextSize(1);
display.setTextColor(WHITE);
// +1.0V
display.setCursor(0, 2);
display.print("+1V");
// 0V（中央）
display.setCursor(0, 24);
display.print("0V");
// -1.0V
display.setCursor(0, 50);
display.print("-1V");
}
void drawFFTMEMORI(){
  display.setTextColor(WHITE);
  display.setTextSize(1);
  int m_p = SCREEN_HEIGHT - 16;
  display.setCursor(0, m_p);
  display.print("0");
  display.setCursor(51, m_p);
  display.print("1k");
  display.setCursor(102, m_p);
  display.print("2k");
}


// ========== Main ========== //
void loop() {
  static unsigned long lastSampleTime = 0;
  static long lastEncoderPos = 0;

  // RE-SW
  if (swFlag) {
    ddsOutputEnabled = !ddsOutputEnabled;
    Serial.println(ddsOutputEnabled ? "DDS ON" : "DDS OFF");

    swFlag = false;
  }

  // RE
  long pos = encoder.read();
  if (abs(pos - lastEncoderPos) > 6) {      // 3->5->6
    displayMode = (displayMode + 1) % 6;    // 3->5->6
    lastEncoderPos = pos;
  }

  // Operation sampling（about every 200ms）
  if (millis() - lastSampleTime >= 200) {
    lastSampleTime = millis();
   
      uint32_t t1 = micros();
    for (int i = 0; i < SAMPLE_SIZE; i++) {

      uint32_t t0 = micros();
      digitalWrite(ADC_LOOP_MONITOR, HIGH);
      adcBuffer[i] =  analogRead(ADC_PIN);
  
      digitalWrite(ADC_LOOP_MONITOR, LOW);
      elapsed = micros() - t0;              // To count time of ADC
      if (elapsed < 100) {
        delayMicroseconds(100 - elapsed - 5);    // To tune for 10kHz=100uS
      }
    }
      loopelapsed = micros() - t1;  // 10kHz(100uS) * 512 = 51.2mS

    for (int i = 0; i < SAMPLE_SIZE; i++) {

      if(testwaveEnabled == true){
        if(ddsOutputEnabled == true) testwaveSelect = 1000.0 ;
        else testwaveSelect = 200.0 ;
        // SAMPLE_SIZE=512, SAMPLE_RATE=10000, 1 size = 100uS, 1kHz = 1000uS
        inputBuffer[i] = sin(2.0 * PI * testwaveSelect * i / 10000.0);  // 1kHz, Fs=10kHz or 200Hz
        //sin( (2*PI / (SAMPLE_SIZE/20) ) * i) * sinevolume;
      }
      else{
        inputBuffer[i] = ((float)(adcBuffer[i] - 1473) / 4096.0) * 3.3;  // offset:0.07v 87 1385->1473
      }
      notchBuffer[i] = applyNotchFilter(inputBuffer[i]);
    }

    vInRMS = calculateRMS(inputBuffer, SAMPLE_SIZE);
    vNotchRMS = calculateRMS(notchBuffer, SAMPLE_SIZE);
    sinadDB = calculateSINAD(vInRMS, vNotchRMS);
    if (sinadDB < 0.0f) sinadDB = 0.0f;

    // Display
    display.clearDisplay();
    switch (displayMode) {
      case 0: drawSINAD(); break;
      case 1: drawWAVE(); break;
      case 2: drawWAVE_n(); break;

      case 3: drawFFT(); break;
      case 4: drawFFT_n(); break;
      case 5: drawPARAMmonitor(); break; 
    }
    drawSmallInfo();  // show SINAD on bottom line
    display.display();
  }
}
