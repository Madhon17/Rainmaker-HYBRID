/*
 * ESP32 RFID + 8 Relay + ESP RainMaker (v2.2.0 - Hybrid Sync with timestamps)
 *
 * Perubahan utama v2.2.0:
 * - Tambah field updatedAt (timestamp) pada setiap kartu untuk menyelesaikan konflik offline/online.
 * - Saat Add/Remove lokal di ESP32, updatedAt diset ke waktu sekarang (format YYYY-MM-DD HH:MM:SS).
 * - Saat sync dengan server: bandingkan updatedAt; versi terbaru dipakai.
 * - Jika ESP32 memiliki versi lebih baru, push ke server; jika server lebih baru, overwrite lokal.
 *
 * Pastikan server API menerima/menyajikan updated_at:
 * - add_card.php expects uid, mask, updated_at (optional)
 * - remove_card.php expects uid, updated_at (optional)
 * - sync.php returns cards as [{uid, mask, updated_at}, ...]
 * - log_access.php accepts uid, action, relays
 */

#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Preferences.h>
#include "RMaker.h"
#include "WiFi.h"
#include "WiFiProv.h"
#include <time.h>

// HYBRID NETWORK
#include <HTTPClient.h>
#include <ArduinoJson.h>

// =================== Pin Mapping ===================
#define SS_PIN         5    // MFRC522 SS (SDA)
#define RST_PIN        22   // MFRC522 RST
#define BUZZER_PIN     15   // Buzzer aktif HIGH

// 8 pin relay
const int RELAY_PINS[8] = {21, 12, 25, 26, 13, 14, 33, 32};
#define RELAY_ACTIVE_HIGH  false  // ubah ke false jika relay board aktif LOW

// =================== RFID ===================
MFRC522 mfrc522(SS_PIN, RST_PIN);

// =================== NVS ===================
Preferences prefs;
const char *NVS_NS = "rfid";
const char *NVS_KEY_COUNT = "count";  // uint16_t
#define MAX_CARDS 64

struct CardEntry {
  String uid;
  uint8_t mask;
  String updatedAt;  // format: YYYY-MM-DD HH:MM:SS (ISO-like)
};
#include <vector>
std::vector<CardEntry> cards;

// =================== RainMaker ===================
static Node my_node;
static Device rfid_dev("RFID_Controller");   // Device utama
static Device relay_dev("Relay_Controller"); // Device baru
static uint8_t wifiLed = 2;  //D2

// Nama parameter
const char *P_STATUS      = "Status";
const char *P_ADD_MODE    = "AddMode";
const char *P_REMOVE_MODE = "RemoveMode";
const char *P_LISTCARDS   = "ListCards";
const char *P_LASTACCESS  = "LastAccess";

String statusStr = "NORMAL";
bool addMode = false;
bool removeMode = false;
bool mark[8] = {false,false,false,false,false,false,false,false};
bool relayState[8] = {false,false,false,false,false,false,false,false};
unsigned long relayOffAt[8] = {0,0,0,0,0,0,0,0};
const char *P_MARK[8]  = {"LT1","LT2","LT3","LT4","LT5","LT6","LT7","LT8"};
const char *P_RELAY[8] = {"LT1","LT2","LT3","LT4","LT5","LT6","LT7","LT8"};

// Mode timeout
const uint32_t MODE_TIMEOUT_MS = 15000;
uint32_t modeStartMs = 0;

// =================== Provisioning ===================
const char *service_name = "Prov_ESP32";
const char *pop = "abcd1234";

// =================== HYBRID Server Config ===================
String serverURL = "http://192.168.1.13:8080/akseskontrol2/"; // ganti dgn alamat server Anda, akhiri '/'
String apiKey     = "mysecret123";                      // isi sesuai server (optional)
unsigned long lastSync = 0;
const unsigned long syncInterval = 300000;    // 5 menit

// =================== Util: Buzzer ===================
void buzzOnce(uint16_t onMs) {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(onMs);
  digitalWrite(BUZZER_PIN, LOW);
}
void buzzer_add()      { buzzOnce(120); delay(60); buzzOnce(60); }
void buzzer_remove()   { buzzOnce(60);  delay(60); buzzOnce(60); }
void buzzer_granted()  { buzzOnce(200); }
void buzzer_denied()   { buzzOnce(60); }
void buzzer_timeout3() { buzzOnce(60); delay(80); buzzOnce(60); delay(80); buzzOnce(60); }

// =================== Util: Waktu ===================
// Format: YYYY-MM-DD HH:MM:SS
String formatTimeNow() {
  time_t now = time(nullptr);
  if (now < 1600000000) {
    // Kalau NTP belum sync, fallback ke millis() unik (tapi string tidak berguna untuk compare)
    // Sebaiknya pastikan RMaker/ntp sudah men-set waktu.
    char buf[32];
    snprintf(buf, sizeof(buf), "1970-01-01 %010lu", millis() / 1000);
    return String(buf);
  }
  struct tm tm_info;
  localtime_r(&now, &tm_info);
  char buf[32];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_info);
  return String(buf);
}

// =================== Util: Relay ===================
void setRelay(int idx, bool on) {
  if (idx < 0 || idx >= 8) return;
  relayState[idx] = on;
  digitalWrite(RELAY_PINS[idx], (RELAY_ACTIVE_HIGH ? (on ? HIGH : LOW) : (on ? LOW : HIGH)));
  // pantulkan ke app (device relay)
  relay_dev.updateAndReportParam(P_RELAY[idx], (bool)on);
}

void pulseRelayAutoOff(int idx, uint32_t ms = 5000) {
  setRelay(idx, true);
  relayOffAt[idx] = millis() + ms;
}

// =================== Util: UID & Daftar ===================
String uidToString(const MFRC522::Uid &uid) {
  String s;
  for (byte i = 0; i < uid.size; i++) {
    if (uid.uidByte[i] < 0x10) s += "0";
    s += String(uid.uidByte[i], HEX);
  }
  s.toUpperCase();
  return s;
}

int findCardIndex(const String &uid) {
  for (size_t i = 0; i < cards.size(); i++) {
    if (cards[i].uid == uid) return (int)i;
  }
  return -1;
}

uint8_t currentMarkMask() {
  uint8_t m = 0;
  for (int i=0;i<8;i++) if (mark[i]) m |= (1 << i);
  return m;
}

String maskToList(uint8_t m) {
  String s;
  bool first = true;
  for (int i=0;i<8;i++) if (m & (1<<i)) {
    if (!first) s += ",";
    s += "LT"; s += (i+1);
    first = false;
  }
  if (first) s = "-";
  return s;
}

String buildListCardsText() {
  String s;
  for (size_t i=0;i<cards.size(); i++) {
    s += cards[i].uid;
    s += "  [";
    s += maskToList(cards[i].mask);
    s += "] ";
    s += cards[i].updatedAt;
    if (i != cards.size()-1) s += "\n";
  }
  return s;
}

void updateStatus(const char *st) {
  statusStr = st;
  rfid_dev.updateAndReportParam(P_STATUS, (char*)statusStr.c_str());
}

void updateListCardsParam() {
  String text = buildListCardsText();
  rfid_dev.updateAndReportParam(P_LISTCARDS, (char*)text.c_str());
}

void updateLastAccessParam(const String &uid, bool granted, uint8_t mask) {
  String s = uid + " | " + formatTimeNow() + " | " + (granted ? "GRANTED " : "DENIED ");
  if (granted) { s += "["; s += maskToList(mask); s += "]"; }
  rfid_dev.updateAndReportParam(P_LASTACCESS, (char*)s.c_str());
}

// =================== NVS Save/Load ===================
void saveCardsToNVS() {
  prefs.begin(NVS_NS, false);
  uint16_t prev = prefs.getUShort(NVS_KEY_COUNT, 0);
  uint16_t cnt = (cards.size() > MAX_CARDS) ? MAX_CARDS : (uint16_t)cards.size();
  prefs.putUShort(NVS_KEY_COUNT, cnt);
  for (uint16_t i=0;i<cnt;i++) {
    char ku[8], km[8], kt[8];
    snprintf(ku, sizeof(ku), "u%02u", i);
    snprintf(km, sizeof(km), "m%02u", i);
    snprintf(kt, sizeof(kt), "t%02u", i);
    prefs.putString(ku, cards[i].uid);
    prefs.putUChar(km, cards[i].mask);
    prefs.putString(kt, cards[i].updatedAt);
  }
  for (uint16_t i=cnt;i<prev;i++) {
    char ku[8], km[8], kt[8];
    snprintf(ku, sizeof(ku), "u%02u", i);
    snprintf(km, sizeof(km), "m%02u", i);
    snprintf(kt, sizeof(kt), "t%02u", i);
    prefs.remove(ku);
    prefs.remove(km);
    prefs.remove(kt);
  }
  prefs.end();
}

void loadCardsFromNVS() {
  cards.clear();
  prefs.begin(NVS_NS, true);
  uint16_t cnt = prefs.getUShort(NVS_KEY_COUNT, 0);
  cnt = min<uint16_t>(cnt, MAX_CARDS);
  for (uint16_t i=0;i<cnt;i++) {
    char ku[8], km[8], kt[8];
    snprintf(ku, sizeof(ku), "u%02u", i);
    snprintf(km, sizeof(km), "m%02u", i);
    snprintf(kt, sizeof(kt), "t%02u", i);
    String uid = prefs.getString(ku, "");
    uint8_t mask = prefs.getUChar(km, 0);
    String updated = prefs.getString(kt, "");
    if (uid.length()) {
      if (updated.length() == 0) updated = "1970-01-01 00:00:00";
      cards.push_back({uid, mask, updated});
    }
  }
  prefs.end();
}

// =================== Mode helpers ===================
void enterAddMode() {
  addMode = true; removeMode = false;
  modeStartMs = millis();
  updateStatus("ADD_MODE");
}

void enterRemoveMode() {
  removeMode = true; addMode = false;
  modeStartMs = millis();
  updateStatus("REMOVE_MODE");
}

void exitToNormal(bool fromTimeout=false) {
  addMode = false;
  removeMode = false;
  updateStatus("NORMAL");
  rfid_dev.updateAndReportParam(P_ADD_MODE, false);
  rfid_dev.updateAndReportParam(P_REMOVE_MODE, false);

  // Reset semua mark ke false
  for (int i = 0; i < 8; i++) {
    mark[i] = false;
    rfid_dev.updateAndReportParam(P_MARK[i], false);
  }

  if (fromTimeout) buzzer_timeout3();
}

// =================== HYBRID: Helpers Local Ops ===================
void addCardToLocal(const String &uid, uint8_t mask, bool pushServer = true) {
  int idx = findCardIndex(uid);
  String now = formatTimeNow();
  if (idx >= 0) {
    cards[idx].mask = mask;
    cards[idx].updatedAt = now;
  } else if ((int)cards.size() < MAX_CARDS) {
    cards.push_back({uid, mask, now});
  } else {
    // jika penuh, skip (atau Anda bisa implement queue)
    Serial.println("[NVS] max cards reached, cannot add");
    return;
  }
  saveCardsToNVS();
  updateListCardsParam();

  if (pushServer && WiFi.status() == WL_CONNECTED) {
    // push to server with updated_at
    HTTPClient http;
    http.setTimeout(5000);
    http.begin(serverURL + "add_card.php");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    if (apiKey.length()) http.addHeader("X-API-Key", apiKey);
    String postData = "uid=" + uid + "&mask=" + String(mask) + "&updated_at=" + now + (apiKey.length() ? "&api_key=" + apiKey : "");
    int code = http.POST(postData);
    if (code != 200) {
      Serial.printf("[PUSH ADD] HTTP %d\n", code);
    }
    http.end();
  }
}

void removeCardLocal(const String &uid, bool pushServer = true) {
  int idx = findCardIndex(uid);
  String now = formatTimeNow();
  if (idx >= 0) {
    cards.erase(cards.begin() + idx);
    saveCardsToNVS();
    updateListCardsParam();
  }
  if (pushServer && WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.setTimeout(5000);
    http.begin(serverURL + "remove_card.php");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    if (apiKey.length()) http.addHeader("X-API-Key", apiKey);
    String postData = "uid=" + uid + "&updated_at=" + now + (apiKey.length() ? "&api_key=" + apiKey : "");
    int code = http.POST(postData);
    if (code != 200) {
      Serial.printf("[PUSH REM] HTTP %d\n", code);
    }
    http.end();
  }
}

void clearAllCards() {
  cards.clear();
  saveCardsToNVS();
  updateListCardsParam();
}

// =================== HYBRID: Networking ===================
// send access log
void sendLog(const String &uid, const String &action, const String &relays) {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  http.setTimeout(4000);
  http.begin(serverURL + "log_access.php");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  if (apiKey.length()) http.addHeader("X-API-Key", apiKey);
  String postData = "uid=" + uid + "&action=" + action + "&relays=" + relays + (apiKey.length() ? "&api_key=" + apiKey : "");
  int code = http.POST(postData);
  if (code != 200) {
    Serial.printf("[LOG] HTTP %d\n", code);
  }
  http.end();
}

// Push card data to server (used when ESP32 has newer version)
void pushCardToServer(const CardEntry &c) {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  http.setTimeout(5000);
  http.begin(serverURL + "add_card.php");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  if (apiKey.length()) http.addHeader("X-API-Key", apiKey);
  String postData = "uid=" + c.uid + "&mask=" + String(c.mask) + "&updated_at=" + c.updatedAt + (apiKey.length() ? "&api_key=" + apiKey : "");
  int code = http.POST(postData);
  if (code != 200) {
    Serial.printf("[PUSH CARD] HTTP %d\n", code);
  }
  http.end();
}

// Sync from server and resolve by updated_at
void syncCardsFromServer() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.setTimeout(8000);
  String url = serverURL + "sync.php";
  if (apiKey.length()) url += "?api_key=" + apiKey;
  http.begin(url);
  int code = http.GET();

  if (code == 200) {
    String payload = http.getString();
    Serial.println(String("[SYNC] payload size: ") + payload.length());
    // parse JSON
    // allocate doc — size depends on expected number of cards
    const size_t CAP = 16384;
    DynamicJsonDocument doc(CAP);
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
      Serial.printf("[SYNC] JSON err: %s\n", err.c_str());
      http.end();
      return;
    }
    bool ok = doc["ok"] | false;
    if (!ok) {
      Serial.println("[SYNC] server ok=false");
      http.end();
      return;
    }
    JsonArray arr = doc["cards"].as<JsonArray>();

    // Build a map of server cards for quick lookup (we will iterate server side)
    // For each server card, compare with local by uid
    for (JsonObject obj : arr) {
      String uid = obj["uid"].as<String>();
      uint8_t mask = (uint8_t)(obj["mask"].as<int>());
      String updated = obj["updated_at"].as<String>();
      if (updated.length() == 0) updated = "1970-01-01 00:00:00";

      int idx = findCardIndex(uid);
      if (idx >= 0) {
        String localUpdated = cards[idx].updatedAt;
        // Lexicographical compare works for YYYY-MM-DD HH:MM:SS
        if (updated > localUpdated) {
          // server lebih baru -> overwrite lokal
          cards[idx].mask = mask;
          cards[idx].updatedAt = updated;
          Serial.printf("[SYNC] uid %s: server newer -> overwrite local\n", uid.c_str());
        } else if (updated < localUpdated) {
          // local lebih baru -> push local to server
          Serial.printf("[SYNC] uid %s: local newer -> push to server\n", uid.c_str());
          pushCardToServer(cards[idx]);
        } else {
          // same timestamp -> nothing
        }
      } else {
        // tidak ada lokal -> ambil dari server
        cards.push_back({uid, mask, updated});
        Serial.printf("[SYNC] uid %s: added from server\n", uid.c_str());
      }
    }

    // Nota bene: server mungkin memiliki kartu yang telah dihapus, but local still has it.
    // To handle deletions from server: we will check local cards that are not present in server list.
    // Build a set of server UIDs to detect deletions.
    std::vector<String> serverUIDs;
    for (JsonObject obj : arr) {
      serverUIDs.push_back(obj["uid"].as<String>());
    }
    // remove local cards that are not in server only if server's updated_at indicates a deletion? 
    // For safety, we will NOT auto-delete local-only cards to avoid losing offline adds.
    // Instead: if admin wants deletion, they should call remove_card.php which will be pushed to ESP32 via sync with empty/marker.
    // (Optional improvement: implement tombstones.)

    // Save local changes
    saveCardsToNVS();
    updateListCardsParam();
    Serial.printf("[SYNC] completed. Local count: %u\n", (unsigned)cards.size());
  } else {
    Serial.printf("[SYNC] HTTP error %d\n", code);
  }

  http.end();
}

// =================== RainMaker Callbacks ===================
void sysProvEvent(arduino_event_t *sys_event) {
  switch (sys_event->event_id) {
    case ARDUINO_EVENT_PROV_START:
#if CONFIG_IDF_TARGET_ESP32
      Serial.printf("\nProvisioning Started with name \"%s\" and PoP \"%s\" on BLE\n", service_name, pop);
      printQR(service_name, pop, "ble");
#else
      Serial.printf("\nProvisioning Started with name \"%s\" and PoP \"%s\" on SoftAP\n", service_name, pop);
      printQR(service_name, pop, "softap");
#endif
      break;

    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      Serial.println("\nConnected to Wi-Fi!");
      digitalWrite(wifiLed, HIGH);  // LED ON
      break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println("\nDisconnected from Wi-Fi!");
      digitalWrite(wifiLed, LOW);   // LED OFF
      break;
  }
}


void write_callback(Device *device, Param *param, const param_val_t val, void *priv, write_ctx_t *ctx)
{
  const char *pname = param->getParamName();

  // Add / Remove mode
  if (!strcmp(pname, P_ADD_MODE)) {
    bool v = val.val.b;
    if (v) enterAddMode(); else exitToNormal(false);
    param->updateAndReport(val);
    return;
  }
  if (!strcmp(pname, P_REMOVE_MODE)) {
    bool v = val.val.b;
    if (v) enterRemoveMode(); else exitToNormal(false);
    param->updateAndReport(val);
    return;
  }

  // Mark toggles
  for (int i=0;i<8;i++) {
    if (!strcmp(pname, P_MARK[i])) {
      mark[i] = val.val.b;
      param->updateAndReport(val);
      return;
    }
  }

  // Manual Relay buttons
  for (int i=0;i<8;i++) {
    if (!strcmp(pname, P_RELAY[i])) {
      bool v = val.val.b;
      if (v) {
        pulseRelayAutoOff(i, 5000);
      } else {
        setRelay(i, false);
        relayOffAt[i] = 0;
      }
      param->updateAndReport(val);
      return;
    }
  }

  // RO param reflect back
  param_val_t cur;
  if (!strcmp(pname, P_STATUS)) { cur = value((char*)statusStr.c_str()); param->updateAndReport(cur); }
  if (!strcmp(pname, P_LISTCARDS)) { String t = buildListCardsText(); cur = value((char*)t.c_str()); param->updateAndReport(cur); }
}

// =================== Setup ===================
void setup() {
  Serial.begin(115200);

  pinMode(wifiLed, OUTPUT);
  digitalWrite(wifiLed, LOW);  // awalnya OFF

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  for (int i=0;i<8;i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    setRelay(i, false);
  }

  SPI.begin();
  mfrc522.PCD_Init();

  loadCardsFromNVS();

  my_node = RMaker.initNode("ESP32_RFID_Controller");

  // Device 1: RFID_Controller
  {
    Param p(P_STATUS, NULL, value((char*)"NORMAL"), PROP_FLAG_READ);
    p.addUIType(ESP_RMAKER_UI_TEXT);
    rfid_dev.addParam(p);
  }
  {
    Param p1(P_ADD_MODE, NULL, value(false), PROP_FLAG_READ | PROP_FLAG_WRITE | PROP_FLAG_PERSIST);
    p1.addUIType(ESP_RMAKER_UI_TOGGLE);
    rfid_dev.addParam(p1);
    Param p2(P_REMOVE_MODE, NULL, value(false), PROP_FLAG_READ | PROP_FLAG_WRITE | PROP_FLAG_PERSIST);
    p2.addUIType(ESP_RMAKER_UI_TOGGLE);
    rfid_dev.addParam(p2);
  }
  for (int i=0;i<8;i++) {
    Param pm(P_MARK[i], NULL, value(false), PROP_FLAG_READ | PROP_FLAG_WRITE | PROP_FLAG_PERSIST);
    pm.addUIType(ESP_RMAKER_UI_TOGGLE);
    rfid_dev.addParam(pm);
  }
  {
    Param p3(P_LISTCARDS, NULL, value((char*)buildListCardsText().c_str()), PROP_FLAG_READ);
    p3.addUIType(ESP_RMAKER_UI_TEXT);
    rfid_dev.addParam(p3);
    Param p4(P_LASTACCESS, NULL, value((char*)"-"), PROP_FLAG_READ);
    p4.addUIType(ESP_RMAKER_UI_TEXT);
    rfid_dev.addParam(p4);
  }
  rfid_dev.addCb(write_callback);
  my_node.addDevice(rfid_dev);

  // Device 2: Relay_Controller
  for (int i=0;i<8;i++) {
    Param pr(P_RELAY[i], NULL, value(false), PROP_FLAG_READ | PROP_FLAG_WRITE);
    pr.addUIType(ESP_RMAKER_UI_TOGGLE);
    relay_dev.addParam(pr);
  }
  relay_dev.addCb(write_callback);
  my_node.addDevice(relay_dev);

  RMaker.enableOTA(OTA_USING_PARAMS);
  RMaker.setTimeZone("Asia/Jakarta");
  RMaker.enableSchedule();

  RMaker.start();

  WiFi.onEvent(sysProvEvent);
#if CONFIG_IDF_TARGET_ESP32
  WiFiProv.beginProvision(WIFI_PROV_SCHEME_BLE, WIFI_PROV_SCHEME_HANDLER_FREE_BTDM, WIFI_PROV_SECURITY_1, pop, service_name);
#else
  WiFiProv.beginProvision(WIFI_PROV_SCHEME_SOFTAP, WIFI_PROV_SCHEME_HANDLER_NONE, WIFI_PROV_SECURITY_1, pop, service_name);
#endif

  updateListCardsParam();
}

// =================== Loop ===================
void loop() {
  uint32_t now = millis();
  for (int i = 0; i < 8; i++) {
    if (relayOffAt[i] && (int32_t)(now - relayOffAt[i]) >= 0) {
      setRelay(i, false);
      relayOffAt[i] = 0;
    }
  }

  if ((addMode || removeMode) && (now - modeStartMs >= MODE_TIMEOUT_MS)) {
    exitToNormal(true);
  }

  // Periodic Sync (only when WiFi connected)
  if (WiFi.status() == WL_CONNECTED && (millis() - lastSync >= syncInterval)) {
    Serial.println("[LOOP] Starting sync with server...");
    syncCardsFromServer();
    lastSync = millis();
  }

  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    String uid = uidToString(mfrc522.uid);
    mfrc522.PICC_HaltA();
    int idx = findCardIndex(uid);

    if (addMode) {
      uint8_t m = currentMarkMask();
      // update lokal + push server
      addCardToLocal(uid, m, true);
      updateLastAccessParam(uid, true, m);
      sendLog(uid, "ADD", maskToList(m));
      buzzer_add();
      exitToNormal(false);
    }
    else if (removeMode) {
      if (idx >= 0) {
        removeCardLocal(uid, true);
        updateLastAccessParam(uid, true, 0);
        sendLog(uid, "REMOVE", "-");
        buzzer_remove();
      } else {
        updateLastAccessParam(uid, false, 0);
        sendLog(uid, "REMOVE_FAIL", "-");
        buzzer_denied();
      }
      exitToNormal(false);
    }
    else {
      if (idx >= 0 && cards[idx].mask != 0) {
        uint8_t m = cards[idx].mask;
        for (int i=0;i<8;i++) if (m & (1<<i)) {
          pulseRelayAutoOff(i, 5000);
        }
        updateLastAccessParam(uid, true, m);
        sendLog(uid, "GRANTED", maskToList(m));
        buzzer_granted();
      } else {
        updateLastAccessParam(uid, false, 0);
        sendLog(uid, "DENIED", "-");
        buzzer_denied();
      }
    }
  }

  delay(10);
}
