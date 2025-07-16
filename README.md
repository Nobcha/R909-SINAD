# R909-SINAD
12dBSINAD indicator used R909-ESP-VFO PCB

To change the CPU from ATmega328P to ESP-C3 dev kit super mini, I will introduce improved R909 series radio gadgets hereafter. First of all I would like to try SINAD indicator for radio sensitivity testing.
R909-SINAD indicator SPECs.
/ ADC sampling:10kHz @ 512 pints
/ Calculation: 1kHz notch function on IIR, RMS value, and SINAD
/ Display: SINAD, Input wave form, Notched wave form, Input FFT, Notched FFT, Internal pameters
/ Test signals: 1kHz pulse, 1kHz pseudo sine wave (DDS+PWM+RC)

今までATmega328Pを使ってR909シリーズを展開してきましたが、メモリー容量の限界で自由なスケッチ展開がむつかしくなりました。そこでコスパ抜群、80MHz/8MByteなのにサイズもコンパクトなESP-C3 dev kit super miniにCPUを入れ替えることにします。
まずは手始めにSINAD indicatorを試作することにしました。

![R80 SINAD](https://github.com/Nobcha/R909-SINAD/blob/main/R80_118100_SINAD.jpg)

基板にはESP-C3 dev kit super mini、OLED（SSD1306）用ピンソケット、ロータリーエンコーダ、スイッチが載っています。SINADインディケーターの仕様は次になります。
項目/内容（試作実験を行い、仕様も見直しました。）
・ADCサンプリング	512点 / 10kHz（1kHz信号の取り扱い想定、ADCの速度上から安全観て20ｋHzはやめました））
・信号計算処理	1kHzノッチ（IIR、Q：10） → RMS → SINAD（dB表示）
・表示	SSD1306 OLEDに以下画面を表示：ロータリー回転で表示モード変更
　1. SINAD（および入力電圧rms）、2. 入力信号WAVE、3.ノッチ後波形、4. 入力信号FFT、5.ノッチ後の周波数スペクトル、6.各種内部パラメータ
・RE-SWを押すとD6に1ｋHzパルス、D5に1kHz疑似サイン波が試験用に出る。（20ｋHzDDS＋PWM＋積分回路）

・今後の予定：Q値設定	(EEPROM）に保存・読み出し、ロータリースイッチ長押しで設定MENU（Q値選択など）

・GPIO接続一覧
　 ロータリーエンコーダ	A相	GPIO2、B相	GPIO3、スイッチ（SW）	GPIO4（RE_SW）
　　OLED SSD1306	I2C SDA/SCL	SDA=GPIO8 / SCL=GPIO9
  　入力オーディオ	ADC入力	A0=GPIO1
　　DDS出力、1ｋHzパルス：GPIO6、疑似正弦波出力：GPIO5
　	I2C SDA/SCL	SDA/SCL 共用
　　時間関数でのサンプリング時間チェック用パルス出力　GPIO7

最新スケッチのリリースは"try_7_3i_sinad_meter.ino"です。
