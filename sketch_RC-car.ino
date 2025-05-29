#include "FspTimer.h"
#include "WiFiS3.h"

#include "secrets.h"

FspTimer fsp_timer;
WiFiUDP Udp;

/*
* farward : M1==LOW, M2==HIGH
* back : M1==HIGH, M2==LOW
* 
* E1,E2 : rotation speed(0~255)
*
*GND=yellow
*5V=black
*A5=white
*/

unsigned char E1 = 5;
unsigned char E2 = 10;
unsigned char M1 = 6;
unsigned char M2 = 9;
unsigned char value = 254;
char checkMove = 's';

void moved(int left, int right, int value, char word) {
  digitalWrite(M1, left);
  digitalWrite(M2, right);
  analogWrite(E1, value);
  analogWrite(E2, value);
  checkMove = word;
}

//fsp
int val = 0;      // 受信するシリアルデータのために準備
float dis = 0;    // 距離の計算値
float l_t = 0.7;  // センサのフィルタ定数
int sDis = 20.0;

void sencer_f() {
  val = analogRead(A5);
  if (val < 82) { val = 82; }  // AD値が小さい場合に無限になってしまうので、AD値が82未満は82とする
  dis = l_t * 25391 * pow(val, -1.272);
}
void interrupt_f(timer_callback_args_t *arg) {
  sencer_f();
  if (dis <= sDis && checkMove == 'g') {
    moved(0, 0, 0, 's');
  }
}

//UDP
char ssid[] = _SSID;
char pass[] = _PASS;

int status = WL_IDLE_STATUS;
IPAddress staticIP(192, 168, 0, 154);
unsigned int localPort = 2390;

char packetBuffer[256];
char ReplyBuffer[] = "acknowledged\n";

void setup() {
  WiFi.config(staticIP);
  pinMode(M1, OUTPUT);
  pinMode(M2, OUTPUT);
  Serial.begin(9600);
  while (!Serial) {
    ;
  }

  if (WiFi.status() == WL_NO_MODULE) {
    while (true)
      ;
  }

  while (status != WL_CONNECTED) {
    status = WiFi.begin(ssid, pass);
    delay(10000);
  }
  IPAddress ip = WiFi.localIP();

  Udp.begin(localPort);

  uint8_t type;
  int8_t ch = FspTimer::get_available_timer(type);
  if (ch < 0) {
    return;
  }

  fsp_timer.begin(TIMER_MODE_PERIODIC, type, static_cast<uint8_t>(ch), 4.0f, 50.0f, interrupt_f, nullptr);
  fsp_timer.setup_overflow_irq();
  fsp_timer.open();
  fsp_timer.start();
}

void loop() {
  char word;
  int packetSize = Udp.parsePacket();
  if (packetSize) {
    int len = Udp.read(packetBuffer, 255);
    if (len > 0) {
      packetBuffer[len] = 0;
    }
    word = packetBuffer[0];
    Serial.println(word);

    Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
    Udp.write(ReplyBuffer);
    Udp.endPacket();
  }

  switch (word) {
    case 's':
      if(dis > sDis){
        moved(0, 0, 0, word);
      }
      break;
    case 'g':
      moved(0, 1, value, word);
      break;
    case 'b':
      moved(1, 0, value, word);
      break;
    case 'r':
      moved(0, 0, (value/2), word);
      break;
    case 'l':
      moved(1, 1, (value/2), word);
      break;
    default:
      break;
  }
}