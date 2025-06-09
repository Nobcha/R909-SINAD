// ****************** filter section *****************
// *************************************************************
//  簡単なデジタルフィルタの実装
//  https://www.utsbox.com/
//
//#pragma once;
//
// How to call
//
/*
// Low Pass
// 4kHz low pass filtering    
    for (int i = 0; i < size1; i++) {    
      input[i] = dataBuff[i];
    }
    math_setup(low_pass);
    math_loop();

    for (int i = 0; i < size1; i++) {    
      dataBuff[i] = output[i];
    }
*/
/*
    // 1kHz notch filteing  
    for (int i = 0; i < size1; i++) {
      input[i] = dataBuff[i];     // SAVE DATA IN input[i] 
    }
    math_setup(notch);
    math_loop();
    for (int i = 0; i < size1; i++) {    
      dataBuff[i] = output[i];   // Processed data is in output[i]
    }  
  }
*/
//
//
#include <math.h>

// --------------------------------------------------------------------------------
// CMyFilter
// --------------------------------------------------------------------------------
class CMyFilter
{
private:
  // フィルタの係数
  float a0, a1, a2, b0, b1, b2;
  // バッファ
  float out1, out2;
  float in1, in2;

public:
  inline CMyFilter();

  // 入力信号にフィルタを適用する関数
  inline float Process(float in);

  // フィルタ係数を計算するメンバー関数
  inline void LowPass  (float freq, float q , float samplerate);
  inline void HighPass (float freq, float q , float samplerate );
  inline void BandPass (float freq, float bw, float samplerate );
  inline void Notch    (float freq, float bw, float samplerate );
  inline void LowShelf (float freq, float q , float gain, float samplerate );
  inline void HighShelf(float freq, float q , float gain, float samplerate );
  inline void Peaking  (float freq, float bw, float gain, float samplerate );
  inline void AllPass  (float freq, float q , float samplerate );
};

// --------------------------------------------------------------------------------
// コンストラクタ
// --------------------------------------------------------------------------------
CMyFilter::CMyFilter()
{
  // メンバー変数を初期化
  a0 = 1.0f; // 0以外にしておかないと除算でエラーになる
  a1 = 0.0f;
  a2 = 0.0f;
  b0 = 1.0f;
  b1 = 0.0f;
  b2 = 0.0f;

  in1 = 0.0f;
  in2 = 0.0f;

  out1 = 0.0f;
  out2 = 0.0f;
}

// それぞれの変数は下記のとおりとする
// 　float input[]  …nominal 20000Hzでサンプリングされた入力信号の格納されたバッファ。
// 　float output[] …フィルタ処理した値を書き出す出力信号のバッファ。
// 　int   size     …入力信号・出力信号のバッファのサイズ。
 
CMyFilter filter;




void math_setup(int fil_mode){
  if(fil_mode == notch)
// カットオフ周波数 1000Hz、Q値 1/√2のnotchフィルタに設定
    filter.Notch(1000.0f, 4, 19910);
  if(fil_mode == low_pass) 
    filter.LowPass(5000.0f, 0.5, 19910);
}

void math_loop(void){
  for(int i = 0; i < size1; i++)
  {
// 入力信号にフィルタを20kHzで取得
//    input[i] = 3.3*analogRead(analogPORT)/4095;  // 3.3V 入力　12ビット4095でアナログ値
  }  

  // 入力信号にフィルタを適用していく。
  for(int i = 0; i < size1; i++)
  {
    // 入力信号にフィルタを適用し、出力信号として書き出す。
    output[i] = filter.Process(input[i]);
  }
 }

// --------------------------------------------------------------------------------
// 入力信号にフィルタを適用する関数
// --------------------------------------------------------------------------------
float CMyFilter::Process(float in)
{
  // 入力信号にフィルタを適用し、出力信号変数に保存。
  float out = b0 / a0 * in + b1 / a0 * in1 + b2 / a0 * in2
    - a1 / a0 * out1 - a2 / a0 * out2;

  in2 = in1; // 2つ前の入力信号を更新
  in1 = in;  // 1つ前の入力信号を更新

  out2 = out1; // 2つ前の出力信号を更新
  out1 = out;  // 1つ前の出力信号を更新

  // 出力信号を返す
  return out;
}

// --------------------------------------------------------------------------------
// フィルタ係数を計算するメンバー関数
// --------------------------------------------------------------------------------
void CMyFilter::LowPass(float freq, float q, float samplerate)
{
  // フィルタ係数計算で使用する中間値を求める。
  float omega = 2.0f * 3.14159265f *  freq / samplerate;
  float alpha = sin(omega) / (2.0f * q);

  // フィルタ係数を求める。
  a0 = 1.0f + alpha;
  a1 = -2.0f * cos(omega);
  a2 = 1.0f - alpha;
  b0 = (1.0f - cos(omega)) / 2.0f;
  b1 = 1.0f - cos(omega);
  b2 = (1.0f - cos(omega)) / 2.0f;
}

void CMyFilter::HighPass(float freq, float q, float samplerate)
{
  // フィルタ係数計算で使用する中間値を求める。
  float omega = 2.0f * 3.14159265f *  freq / samplerate;
  float alpha = sin(omega) / (2.0f * q);

  // フィルタ係数を求める。
  a0 = 1.0f + alpha;
  a1 = -2.0f * cos(omega);
  a2 = 1.0f - alpha;
  b0 = (1.0f + cos(omega)) / 2.0f;
  b1 = -(1.0f + cos(omega));
  b2 = (1.0f + cos(omega)) / 2.0f;
}

void CMyFilter::BandPass(float freq, float bw, float samplerate)
{
  // フィルタ係数計算で使用する中間値を求める。
  float omega = 2.0f * 3.14159265f *  freq / samplerate;
  float alpha = sin(omega) * sinh(log(2.0f) / 2.0 * bw * omega / sin(omega));

  // フィルタ係数を求める。
  a0 = 1.0f + alpha;
  a1 = -2.0f * cos(omega);
  a2 = 1.0f - alpha;
  b0 = alpha;
  b1 = 0.0f;
  b2 = -alpha;
}

void CMyFilter::Notch(float freq, float bw, float samplerate)
{
  // フィルタ係数計算で使用する中間値を求める。
  float omega = 2.0f * 3.14159265f *  freq / samplerate;
  float alpha = sin(omega) * sinh(log(2.0f) / 2.0 * bw * omega / sin(omega));

  // フィルタ係数を求める。
  a0 = 1.0f + alpha;
  a1 = -2.0f * cos(omega);
  a2 = 1.0f - alpha;
  b0 = 1.0f;
  b1 = -2.0f * cos(omega);
  b2 = 1.0f;
}

void CMyFilter::LowShelf(float freq, float q, float gain, float samplerate)
{
  // フィルタ係数計算で使用する中間値を求める。
  float omega = 2.0f * 3.14159265f *  freq / samplerate;
  float alpha = sin(omega) / (2.0f * q);
  float A = pow(10.0f, (gain / 40.0f));
  float beta = sqrt(A) / q;

  // フィルタ係数を求める。
  a0 = (A + 1.0f) + (A - 1.0f) * cos(omega) + beta * sin(omega);
  a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cos(omega));
  a2 = (A + 1.0f) + (A - 1.0f) * cos(omega) - beta * sin(omega);
  b0 = A * ((A + 1.0f) - (A - 1.0f) * cos(omega) + beta * sin(omega));
  b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cos(omega));
  b2 = A * ((A + 1.0f) - (A - 1.0f) * cos(omega) - beta * sin(omega));
}

void CMyFilter::HighShelf(float freq, float q, float gain, float samplerate)
{
  // フィルタ係数計算で使用する中間値を求める。
  float omega = 2.0f * 3.14159265f *  freq / samplerate;
  float alpha = sin(omega) / (2.0f * q);
  float A = pow(10.0f, (gain / 40.0f));
  float beta = sqrt(A) / q;

  // フィルタ係数を求める。
  a0 = (A + 1.0f) - (A - 1.0f) * cos(omega) + beta * sin(omega);
  a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cos(omega));
  a2 = (A + 1.0f) - (A - 1.0f) * cos(omega) - beta * sin(omega);
  b0 = A * ((A + 1.0f) + (A - 1.0f) * cos(omega) + beta * sin(omega));
  b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cos(omega));
  b2 = A * ((A + 1.0f) + (A - 1.0f) * cos(omega) - beta * sin(omega));
}


void CMyFilter::Peaking(float freq, float bw, float gain, float samplerate)
{
  // フィルタ係数計算で使用する中間値を求める。
  float omega = 2.0f * 3.14159265f *  freq / samplerate;
  float alpha = sin(omega) * sinh(log(2.0f) / 2.0 * bw * omega / sin(omega));
  float A = pow(10.0f, (gain / 40.0f));

  // フィルタ係数を求める。
  a0 = 1.0f + alpha / A;
  a1 = -2.0f * cos(omega);
  a2 = 1.0f - alpha / A;
  b0 = 1.0f + alpha * A;
  b1 = -2.0f * cos(omega);
  b2 = 1.0f - alpha * A;
}

void CMyFilter::AllPass(float freq, float q, float samplerate)
{
  // フィルタ係数計算で使用する中間値を求める。
  float omega = 2.0f * 3.14159265f *  freq / samplerate;
  float alpha = sin(omega) / (2.0f * q);

  // フィルタ係数を求める。
  a0 = 1.0f + alpha;
  a1 = -2.0f * cos(omega);
  a2 = 1.0f - alpha;
  b0 = 1.0f - alpha;
  b1 = -2.0f * cos(omega);
  b2 = 1.0f + alpha;
}
