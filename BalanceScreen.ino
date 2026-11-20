/*
  ============================================================
   DeepSeek Balance Screen  (ESP32-S3 + OLED 128x64)
   ============================================================

   A small desk display: shows your DeepSeek API balance,
   today's spend, the current time and a working-hours LED.

   ------------------------------------------------------------
   >>> Change only three things before use (blocks ①②③ below)
   >>> 使用前只改三处（见下面 ① ② ③ 三个方框）

   ① Wi-Fi name and password            WiFi 名称和密码
   ② Your own DeepSeek key (sk-...)     你自己的 DeepSeek 密钥
   ③ Pins, only if rewired              引脚，接线不同才改

   The Chinese in ＜＞ is a placeholder. Replace the whole ＜...＞,
   brackets included, with your own value.
   尖括号里的中文是占位符，把整个 ＜...＞ 连同尖括号一起换成你自己的，
   别把中文留在里面，否则编译能过但连不上。
   ------------------------------------------------------------

   Libraries: install Adafruit SSD1306 and Adafruit NeoPixel
   库：在 Arduino IDE 的库管理器装 Adafruit SSD1306 和 Adafruit NeoPixel

   Board: ESP32S3 Dev Module, Flash 16MB, PSRAM OPI PSRAM
   板子：ESP32S3 Dev Module，Flash 16MB，PSRAM 选 OPI PSRAM

   Screen layout (128x64) 屏幕布局:
     HH:MM:SS                    WiFi
     已花 0.8000 CNY
        16.86  CNY
     [##########] 22...
   ============================================================
*/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <time.h>
#include <Adafruit_NeoPixel.h>
#include <Preferences.h>

/* ============================================================
   >>> ① Wi-Fi
   Fill in your Wi-Fi: replace the Chinese placeholders below
   填 Wi-Fi：把下面三行的中文占位换成你自己的（连同尖括号一起去掉）
   ============================================================ */
/* Wi-Fi name - the one your phone sees in its Wi-Fi list
   WiFi 名字：填你家的 WiFi 名称 */
#ifndef WIFI_SSID
  #define WIFI_SSID "＜这里填你的WiFi名称＞"
#endif
/* Wi-Fi password - leave "" if there is none
   WiFi 密码：没密码就留空 */
#ifndef WIFI_PASS
  #define WIFI_PASS "＜这里填你的WiFi密码＞"
#endif
/* Accessors used by the code below - nothing to change here
   取字符串用，不用改 */
static inline const char* _wifiSsid() { return WIFI_SSID; }
static inline const char* _wifiPass() { return WIFI_PASS; }

/* ============================================================
   >>> ② DeepSeek API key
   填 DeepSeek 密钥
   ============================================================ */
/* DeepSeek sk key - your own, starts with the letters s and k
   Do not use someone else's, and do not push your filled-in copy to GitHub
   DeepSeek sk 密钥：填你自己的，别把自己填好的这份传上去 */
#ifndef DEEPSEEK_KEY
  #define DEEPSEEK_KEY "＜这里填你自己的DeepSeek密钥＞"
#endif
static inline const char* _apiKey() { return DEEPSEEK_KEY; }

/* API server - normally no need to change
   API 服务器地址，一般不用改 */
#define BALANCE_HOST "api.deepseek.com"

/* ============================================================
   >>> ③ Pins
   Already match the default wiring - nothing to change
   引脚：已经和默认接线对上，不用改
   ============================================================ */
/* OLED SDA - default GPIO 8 / SDA 引脚，默认 8 */
#define SDA_PIN 8
/* OLED SCL - default GPIO 9 / SCL 引脚，默认 9 */
#define SCL_PIN 9
/* On-board RGB - default GPIO 48, any free pin if there is no LED
   板载彩灯引脚，默认 48 */
#define RGB_PIN 48

/* ============================================================
   >>> ④ Peak hours
   Only affects the LED colour - nothing to change
   高峰时段：只影响板载彩灯颜色，不用改
   ============================================================ */
/* Peak = weekdays 09:00-12:00 and 14:00-18:00 Beijing time, red LED;
   off-peak is green. Edit the four numbers to change.
   高峰亮红灯，其他时间绿灯 */
#define PEAK_H1_START 9
#define PEAK_H1_END   12
#define PEAK_H2_START 14
#define PEAK_H2_END   18

/* Screen size and I2C address / 屏幕尺寸和 I2C 地址，一般不用动 */
#define SCREEN_W 128
#define SCREEN_H 64
#define OLED_ADDR 0x3C

/* On-board RGB (WS2812): 1 pixel, GRB, 800KHz / 板载彩灯 */
Adafruit_NeoPixel pixels(1, RGB_PIN, NEO_GRB + NEO_KHZ800);

Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, -1);
bool screenOK = false;
bool wifiReady = false;
bool timeReady = false;

String g_balance = "";
bool g_available = false;
String g_note = "";

Preferences prefs;              /* survives power loss */
float g_todaySpent = 0;         /* today's spend */
String g_todayDate = "";        /* which day it refers to, YYYYMMDD */
float g_firstBalance = -1;      /* balance seen first that day */

#define REFRESH_MS 60000UL
unsigned long g_lastRefresh = 0;

/* ================= peak / off-peak -> RGB LED =================
   Peak pricing: Mon-Fri 09:00-12:00 and 14:00-18:00 Beijing time.
   Peak -> red, off-peak -> green, clock not synced -> LED off.
   Change the hours below if the pricing window ever changes. */
bool isPeakHour() {
  if (!timeReady) return false;
  struct tm ti;
  if (!localTimeNow(ti)) return false;
  bool weekday = (ti.tm_wday >= 1 && ti.tm_wday <= 5);
  bool morning = (ti.tm_hour >= PEAK_H1_START && ti.tm_hour < PEAK_H1_END);
  bool afternoon = (ti.tm_hour >= PEAK_H2_START && ti.tm_hour < PEAK_H2_END);
  return weekday && (morning || afternoon);
}

/* fills ti with the local time; false until NTP has set the clock */
bool localTimeNow(struct tm &ti) {
  if (!timeReady) return false;
  time_t now = time(nullptr);
  localtime_r(&now, &ti);
  return true;
}

void updateLed() {
  if (!timeReady) {
    pixels.clear();
  } else if (isPeakHour()) {
    pixels.setPixelColor(0, pixels.Color(50, 0, 0));
  } else {
    pixels.setPixelColor(0, pixels.Color(0, 50, 0));
  }
  pixels.show();
}

/* ================= time string ================= */
String getTimeStr() {
  if (!timeReady) return String("--:--:--");
  time_t now = time(nullptr);
  struct tm ti;
  localtime_r(&now, &ti);
  char t[10];
  snprintf(t, sizeof(t), "%02d:%02d:%02d", ti.tm_hour, ti.tm_min, ti.tm_sec);
  return String(t);
}


/* ================= tiny CJK glyphs (16x16 bitmap) =================
   Only two characters are needed: 已 (spent) and 花 (spend).
   Generated from WenQuanYi Zen Hei; 1=lit 0=dark.
   Each row is 2 bytes, 16 rows = 32 bytes per glyph.
   Add more glyphs with the same flow if you want other words. */
const uint8_t CN_16[][32] = {
  /* U+5DF2 已 - 'spent' label */
  { 0x3F,0xF8, 0x00,0x08, 0x00,0x08, 0x20,0x08, 0x20,0x08, 0x3F,0xF8, 0x20,0x08, 0x20,0x00,
    0x20,0x00, 0x20,0x00, 0x20,0x02, 0x20,0x02, 0x20,0x06, 0x1F,0xFC, 0x00,0x00, 0x00,0x00 },
  /* U+82B1 花 - 'spend' label */
  { 0x04,0x20, 0x04,0x20, 0x7F,0xFE, 0x04,0x20, 0x00,0x00, 0x04,0x40, 0x04,0x46, 0x08,0x4C,
    0x18,0x58, 0x38,0x60, 0x69,0xC0, 0x4B,0x43, 0x08,0x42, 0x08,0x42, 0x08,0x3E, 0x00,0x00 },
};

/* draw a 16x16 glyph at (x,y); idx: 0=spent-label 1=spend-label */
void drawCN(int x, int y, int idx) {
  for (int row = 0; row < 16; row++) {
    uint8_t hi = CN_16[idx][row * 2];
    uint8_t lo = CN_16[idx][row * 2 + 1];
    for (int col = 0; col < 8; col++) {
      if (hi & (0x80 >> col)) display.drawPixel(x + col, y + row, SSD1306_WHITE);
    }
    for (int col = 0; col < 8; col++) {
      if (lo & (0x80 >> col)) display.drawPixel(x + 8 + col, y + row, SSD1306_WHITE);
    }
  }
}

/* ================= top bar ================= */
void drawTopBar() {
  if (!screenOK) return;
  display.fillRect(0, 0, 128, 9, SSD1306_BLACK);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  /* left: time HH:MM:SS */
  display.setCursor(0, 0);
  display.print(getTimeStr());

  /* right: Wi-Fi state */
  const char* w = wifiReady ? "WiFi" : "----";
  display.setCursor(128 - strlen(w) * 6 - 1, 0);
  display.print(w);
}

/* redraw only the clock area (~56x9 px) so the panel does not flicker */
void drawTimeOnly() {
  if (!screenOK) return;
  display.fillRect(0, 0, 54, 9, SSD1306_BLACK);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print(getTimeStr());
  display.display();
}

/* ================= bottom bar =================
   progress bar + seconds on the right + three dots (one every 1/3 s) */
void drawRefreshBar() {
  if (!screenOK) return;
  unsigned long el = millis() - g_lastRefresh;
  if (el > REFRESH_MS) el = REFRESH_MS;
  int left = (int)((REFRESH_MS - el) / 1000);

  /* bar: 86 px wide, right side is reserved for the seconds and dots */
  int w = (int)(86L * el / REFRESH_MS);

  display.fillRect(0, 51, 128, 13, SSD1306_BLACK);
  display.drawRect(0, 54, 86, 6, SSD1306_WHITE);
  if (w > 0 && w <= 84) display.fillRect(1, 55, w, 4, SSD1306_WHITE);

  /* right: seconds */
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(90, 53);          /* 90+12+18 = 120, so the three dots still fit in 128 px */
  display.print(left);

  /* dots stay in step with the clock: 0->1->2->3 within one second,
     and at the third dot the seconds digit ticks over and the dots reset */
  int phase = (int)((el % 1000) / 250);         /* 0,1,2,3 */
  for (int i = 0; i < phase; i++) display.print(".");
  /* note: no display.display() here, the caller pushes the frame */
}

/* ================= connecting screen =================
   "WiFi" in size 2, animated dots below */
void showWifiScreen() {
  if (!screenOK) return;
  display.clearDisplay();

  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  const char* t = "WiFi";
  int w = strlen(t) * 12;                    /* size 2 = 12 px per char */
  int x0 = (128 - w) / 2;
  display.setCursor(x0, 20);
  display.print(t);

  /* dots: size 2, time based, one more every 333 ms */
  int phase = (int)((millis() % 1200) / 300);   /* 0,1,2,3 */
  display.setCursor(x0 + w / 2 - 6, 42);
  for (int i = 0; i < phase; i++) display.print(".");

  display.display();
}

/* ================= main screen =================
   layout:
     top bar:  HH:MM:SS            WiFi
     line 1:   [glyph][glyph] x.xxxx CNY    (today's spend)
     line 2:        16.86  CNY              (balance, size 2)
     bottom:  [#########   ] 22 ...         (bar + seconds + dots)
*/
void showBalance() {
  if (!screenOK) return;
  display.clearDisplay();
  drawTopBar();

  display.setTextColor(SSD1306_WHITE);

  /* today's spend: two glyphs + number, starting at y=11 */
  drawCN(0, 10, 0);      /* U+5DF2 已 */
  drawCN(16, 10, 1);     /* U+82B1 花 */
  display.setTextSize(1);
  display.setCursor(40, 15);
  display.print(g_todaySpent, 4);
  display.print(" CNY");

  /* balance, size 2, shifted down to y=30 */
  const char* amt = g_balance.length() > 0 ? g_balance.c_str() : "--.--";
  display.setTextSize(2);
  display.setCursor(0, 30);
  display.print(amt);

  int w = strlen(amt) * 12;
  display.setTextSize(1);
  display.setCursor(w + 4, 38);
  display.print("CNY");

  /* only show the reason on error, stay clean otherwise */
  if (g_note.length() > 0) {
    display.setCursor(0, 46);
    display.print(g_note.c_str());
  }

  drawRefreshBar();
  display.display();
}

/* ================= networking ================= */
bool connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(_wifiSsid(), _wifiPass());

  Serial.print("WiFi");
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 40) {
    showWifiScreen();              /* keep the dots moving */
    delay(400);
    Serial.print(".");
    tries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiReady = true;
    Serial.println();
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    return true;
  }
  return false;
}

void syncTime() {
  configTime(8 * 3600, 0, "ntp.aliyun.com", "cn.pool.ntp.org");
  time_t now = time(nullptr);
  int guard = 0;
  while (now < 8 * 3600 * 2 && guard < 40) {
    delay(500);
    now = time(nullptr);
    guard++;
  }
  timeReady = (now > 8 * 3600 * 2);
  Serial.printf("time sync: %s\n", timeReady ? "OK" : "FAILED");
  updateLed();
}

/* ================= today's spend bookkeeping ================= */
String todayKey() {
  if (!timeReady) return "";
  time_t now = time(nullptr);
  struct tm ti;
  localtime_r(&now, &ti);
  char buf[16];
  snprintf(buf, sizeof(buf), "%04d%02d%02d", ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday);
  return String(buf);
}

void saveSpent() {
  prefs.begin("money", false);
  prefs.putFloat("spent", g_todaySpent);
  prefs.putString("date", g_todayDate);
  prefs.putFloat("first", g_firstBalance);
  prefs.end();
}

void loadSpent() {
  prefs.begin("money", true);
  g_todaySpent = prefs.getFloat("spent", 0);
  g_todayDate = prefs.getString("date", "");
  g_firstBalance = prefs.getFloat("first", -1);
  prefs.end();
  Serial.printf("loaded: date=%s spent=%.4f first=%.2f\n",
                g_todayDate.c_str(), g_todaySpent, g_firstBalance);
}

/* call after every successful fetch: reset on a new day, else keep adding */
void updateSpent(float balance) {
  String today = todayKey();
  if (today.length() == 0) return;          /* clock not synced yet, do not count */

  if (today != g_todayDate) {
    /* new day: reset */
    g_todayDate = today;
    g_firstBalance = balance;
    g_todaySpent = 0;
    saveSpent();
    Serial.printf("new day %s, first balance=%.2f\n", today.c_str(), balance);
    return;
  }

  if (g_firstBalance < 0) {
    g_firstBalance = balance;
    saveSpent();
  }

  /* spend = first balance of the day - current balance.
     If the balance jumps up (a top-up), raise the baseline too,
     so the money already spent is not erased. */
  float spent = g_firstBalance - balance;
  if (spent < -0.01) {
    /* balance went up -> treat as a top-up, raise the baseline as well */
    float recharge = -spent;
    g_firstBalance += recharge;
    Serial.printf(">>> top-up detected %.2f, baseline now %.2f\n", recharge, g_firstBalance);
    spent = g_firstBalance - balance;
    if (spent < 0) spent = 0;
  }
  g_todaySpent = spent;
  saveSpent();
}

/* ================= fetch balance ================= */
int fetchBalance() {
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(12000);

  if (!client.connect(BALANCE_HOST, 443)) return -1;

  String req = "";
  req += "GET /user/balance HTTP/1.1\r\n";
  req += "Host: " BALANCE_HOST "\r\n";
  req += "Authorization: Bearer " + String(_apiKey()) + "\r\n";
  req += "Connection: close\r\n\r\n";
  client.print(req);
  client.flush();

  unsigned long t0 = millis();
  while (!client.available() && millis() - t0 < 12000) delay(10);

  String resp = "";
  while (client.available()) {
    resp += (char)client.read();
    if (resp.length() > 4000) break;
  }
  client.stop();

  int code = -1;
  int sp = resp.indexOf(' ');
  if (sp > 0) code = resp.substring(sp + 1, sp + 4).toInt();

  if (code == 200) {
    /* pull out total_balance */
    int p = resp.indexOf("\"total_balance\":\"");
    if (p >= 0) {
      p += 17;
      int e = resp.indexOf('"', p);
      if (e > p) {
        g_balance = resp.substring(p, e);
        updateSpent(g_balance.toFloat());
      }
    }
    g_available = resp.indexOf("\"is_available\":true") >= 0;
    g_note = "";
  } else if (code == 401) {
    g_note = "key err";
    g_available = false;
  } else {
    g_note = "http" + String(code);
    g_available = false;
  }
  return code;
}

void doRefresh() {
  g_lastRefresh = millis();
  int c = fetchBalance();
  Serial.printf("balance code=%d val=%s\n", c, g_balance.c_str());
  showBalance();
}

/* ================= setup / loop ================= */
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("=== BALANCE v2 ===");

  pixels.begin();
  pixels.setBrightness(40);
  pixels.clear();
  pixels.show();

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);          /* 100 kHz, more tolerant of long wires */
  screenOK = display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  Serial.println(screenOK ? "screen OK" : "screen NOT FOUND");

  loadSpent();

  if (screenOK) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 20);
    display.print("WiFi...");
    display.display();
  }

  if (!connectWiFi()) {
    g_note = "no wifi";
    showBalance();
    return;
  }

  syncTime();
  doRefresh();
}

/* dim the panel between 22:00 and 07:00 */
void updateBrightness() {
  if (!screenOK || !timeReady) return;
  struct tm ti;
  if (!localTimeNow(ti)) return;
  bool night = (ti.tm_hour >= 22 || ti.tm_hour < 7);
  static bool lastNight = false;
  if (night != lastNight) {
    lastNight = night;
    display.dim(night);
    Serial.printf("brightness: %s\n", night ? "dim" : "normal");
  }
}

void loop() {
  /* ---------- Wi-Fi reconnect ---------- */
  static unsigned long lastWifiTry = 0;
  static unsigned long retryGap = 30000;
  static int retryStage = 0;

  if (WiFi.status() != WL_CONNECTED) {
    if (wifiReady) {
      wifiReady = false;
      Serial.println(">>> WiFi lost");
    }
    showWifiScreen();                 /* called several times a second, that is what animates the dots */
    delay(100);

    if (millis() - lastWifiTry > retryGap) {
      lastWifiTry = millis();
      Serial.printf(">>> WiFi retry (gap=%lus)\n", retryGap / 1000);
      WiFi.disconnect();
      WiFi.begin(_wifiSsid(), _wifiPass());
      retryStage++;
      if (retryStage == 1) retryGap = 60000;
      else if (retryStage == 2) retryGap = 120000;
      else if (retryStage >= 3) retryGap = 300000;
    }
    return;
  }

  /* ---------- connected ---------- */
  if (!wifiReady) {
    wifiReady = true;
    retryStage = 0;
    retryGap = 30000;
    Serial.print(">>> WiFi OK, IP=");
    Serial.println(WiFi.localIP());
    if (!timeReady) syncTime();
    doRefresh();
  }

  updateBrightness();

  /* clock: polled every 500 ms, redrawn only when the second changes */
  static unsigned long lastSec = 0;
  static String lastTimeStr = "";
  String nowStr = getTimeStr();
  if (nowStr != lastTimeStr) {
    lastTimeStr = nowStr;
    if (screenOK) {
      display.fillRect(0, 0, 54, 9, SSD1306_BLACK);
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0, 0);
      display.print(nowStr);
    }
  }

  /* bottom bar: every 80 ms, otherwise the dots would freeze */
  static unsigned long lastBar = 0;
  if (millis() - lastBar > 80) {
    lastBar = millis();
    if (screenOK) {
      drawRefreshBar();
      display.display();
    }
  }

  /* LED refreshed every 30 s */
  static unsigned long lastLed = 0;
  if (millis() - lastLed > 30000) {
    lastLed = millis();
    updateLed();
  }

  /* time to fetch again */
  if (millis() - g_lastRefresh > REFRESH_MS) {
    doRefresh();
  }

  delay(20);
}
