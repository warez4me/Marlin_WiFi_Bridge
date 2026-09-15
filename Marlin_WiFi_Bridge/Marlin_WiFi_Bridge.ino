/*
 * Copyright (c) 2026 warez4me
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#include <WebSocketsServer.h>
#include <string.h>
#include <pgmspace.h>

#include "ets_sys.h"
#include "gpio.h"
#include "osapi.h"
#include "user_interface.h"

#include "eagle_soc.h"      //  uart
#include "uart_register.h"  //  uart
#include "uart.h"           //  uart

#include <StreamString.h>   // for err text def in Native OTA

#include "WiFi_BFT.h"
#include "bft_uart.h"
#include "net_io.h"
#include "gcode.h"
#include "functions.h"
#include "stats_monitor.h"
#include "version.h"

///////  MQTT config strings  ///////
#include "MQTT_topics.h"
#include "mqtt.h"

ESP8266WebServer httpServer(80);

WebSocketsServer webSocket = WebSocketsServer(81);

//#include "js_frontend.h" /* - нужно конвертировать через raspberrypi/home/pi/bft/zipper.py */
#include "js_frontend_gzip.h"
//#include "ha_gzip.h"
//#include "cfg_page.h"    /* - нужно конвертировать через raspberrypi/home/pi/bft/zipper.py */
#include "cfg_page_gzip.h"

///////  global variables  ///////

uint32_t errCode = ERR_NO_ERRORS;  // битовая маска всех флагов ошибок
BridgeState_t bridgeState = SYS_IDLE;       // СОСТОЯНИЕ СИСТЕМЫ

struct SocketData {
  uint32_t clGroup_ID;          // идентификатор активного клиента
  uint32_t fSize;               // размер файла, выбранного клиентом для пересылки
  uint32_t progress;            // количество переданных байтов, 0 если файл не выбран или не было "START:"
  uint32_t mqttCtrl_ID;         // идентификатор клиента, чья G-code команда отработана последней (актуален 10 сек.)
  bool clFileSel;               // признак того, что файл выбран на стороне клиента, а не на SD карте принтера
  uint8_t clWS_ID;              // WebSocket идентификатор сессии браузера
  char fName[MAX_FNAME_LEN + 1];// буфер для имени файла
  };
SocketData socket = {0, 0, 0, 0, false, 0, {0}};

struct ChunkBuffer {
  uint8_t* data;                // указатель на buf[CHUNK_SIZE];
  uint32_t time = 0;            // время заполнения/опустошения
  int32_t chSeq = -1011;        // порядковый номер чанка в буфере, (== -1011 до 1-й передачи файла)
                                // (флаг валидности инфы о причине рестарта)
  uint32_t txMark = 0;          // время записи
  uint offset = 0;              // Курсор чтения внутри чанка
  size_t len = 0;               // общее количество байт в чанке
  };
ChunkBuffer chunks[NUM_CHUNKS]; 

uint32_t timeStart = 0, timeEnd = 0;
uint32_t netDrops = 0;

size_t lastSentLen = 0;// Сколько байт в последнем НЕподтвержденном пакете
bool isEndOfFile = false;
uint8_t ch_in = 0;     // Куда пишет WebSocket
uint8_t ch_out = 0;    // Откуда берет Marlin
uint8_t ch_cnt = 0;    // Текущее количество занятых буферов
uint8_t ch_req = 0;    // Текущее количество запрошенных чанков
uint32_t chGotSeq = 0; // порядковый номер последнего принятого чанка
uint32_t chReqSeq = 0; // порядковый номер последнего запрошенного чанка
uint32_t ch_Total = 0; // (fSize / CHUNK_SIZE) + ((fSize % CHUNK_SIZE)? 1: 0)

bool useBFT = false;   // флаг использования BFT
int32_t seqBFT = 0;    // порядковый индекс пакета BFT, устанавливается <0 при инициализации BFT режима

uint32_t wd_BIN_Timer = 0, wd_ACT_Timer = 0, M20_Timer = 0, fListTOut = 0, M30_Timer = 0, otaTimer = 0, txTimer = 0;
uint32_t errCast = 0, wsMap = 0, cmdMode = 0, M30_Seq = 0, fListScanned = FLIST_NOT_SCANNED, sdTryNum = 2;
bool srvSync = false, fListWait = false;

//////
char svcBuf[SVC_BUF_SIZE];
uint8_t packet[PACKET_BUF_SIZE];
//////

bool syncReported = false;
unsigned long wifiLastCheck = 0, timeLastSync = 0, lastUARTTime = 0;
int wifi_timer = 0, apTime = 0;
bool sdCheck = false;           // разрешает WS heartbeat после первой успешной либо любой (при sdFinal == true) попытки прочитать список файлов на SD карте
bool sdFinal = false;           // поднимается для авто- попытки прочитать список файлов на SD карте, разрешает sdCheck для любого результата чnения SD
enum pgsReset_t {PROGRESS_ZERO = 0, PROGRESS_WAIT, PROGRESS_LOCK};
pgsReset_t pgsReset = PROGRESS_LOCK; // флаг для сброса MQTT значения прогресса 100% -> 0% после "press AnyKey"

uint32_t sessionID = 0;         // случайное (WiFi noise) число генерируется при смене контекста (имени файла) для исключения зависаний UI клиентов

volatile bool flag_0125sec = true, flag_05sec = false, flag_1sec_1 = false, flag_1sec_2 = false;
static os_timer_t os_125ms_timer;

void timer_125ms_func(void *arg) {
  (void)arg;
  static uint tick_125ms = 0;
  ++tick_125ms &= 7;
  switch (tick_125ms) {
    case 0:
      flag_1sec_1 = true;
      flag_1sec_2 = true;
    case 4:
      flag_05sec = true;
    default:
      flag_0125sec = true;
      break;
    }
}

char* fl2chr(const char* flPtr) {
  memset((uint8_t*)svcBuf, 0, sizeof(svcBuf));
  if (flPtr)
    // Копируем максимум (размер_буфера - 1), чтобы оставить место для \0
    strncpy_P(svcBuf, flPtr, sizeof(svcBuf) - 1);
   else
    // В случае NULL-указателя возвращаем текст ошибки
    strncpy_P(svcBuf, PSTR("fl2chr: NULL ptr"), sizeof(svcBuf) - 1);
  // strncpy_P НЕ гарантирует наличие \0 в конце, если строка была обрезана,
  // поэтому ставим его вручную на последнее место.
  svcBuf[sizeof(svcBuf) - 1] = '\0'; 
  return svcBuf;
}

void wifiConnect(bool init) {
  // Вместо yield() используем delay(0) — это надежнее сбрасывает WDT ядра
  delay(0); 
  // ограничиваем частоту вызовов:
  // при обычной работе : проверять статус Wi-Fi чаще, чем раз в 250 мс — вредно для процессора
  // state_machine работает под управлением таймера wifi_timer
  // каждую секунду производится изменение значения wifi_timer в сторону 0
  // в диапазоне значений wifi_timer -8..0..5 работаем в режиме WIFI_STA как клиент
  // в диапазоне значений wifi_timer -50..-10 работаем в режиме WIFI_AP как точка доступа для конфигурации системы
  // при работе в конфигурационном AP режиме : раз в 10 сек обновляем таймер AP и восстанавливаем wifi_timer = -50
  // при наличии статуса WL_CONNECTED значение wifi_timer здесь восстанавливается равным 5
  // при отсутствии статуса WL_CONNECTED значение wifi_timer уменьшается от 5 до 0 (за 5 секунд)
  // при значении wifi_timer == 0 изменяем значение до -8 и ждем подтверждения отключения ~3сек (-8 -> -5)
  // при значении wifi_timer == -5 делаем попытку коннекта с роутером и ожидаем WL_CONNECTED
  // при получении WL_CONNECTED либо wifi_timer == 0 цикл возобновляется в зависимости от того, что получено раньше
  // переключение а режим AP производится функцией sta2ap() при невозможности получения WL_CONNECTED в течение 1 минуты
  // при этом wifi_timer начинает работать в диапазоне -50..-10 с восстановлением значения -50 через каждые 10 секунд
  // в режиме AP можно подключиться для переконфигурации параметров подключения к WiFi сети и/или MQTT брокеру
  // после 2 минут работы в режиме AP производится общая перезагрузка, если нет входящего подключения
  unsigned long now = millis();
  if ((now - wifiLastCheck) < ((wifi_timer < -10)? 10000: 250)) return; 
  wifiLastCheck = now;
  if (wifi_timer < -10) {
    const char* ap_msg = ((apTime > 0)? PSTR("M117 Setup mode AP: %d s.\n"): PSTR("M117 WiFi bridge restart..\n"));
    int len = snprintf_P((char*)packet, sizeof(packet), ap_msg, apTime);
    uart_w((char*)packet, len);
    if ((apTime <= 0)) ESP.restart(); // ждём коннект к AP 2 минуты, после чего перезагружаемся
    apTime -= ((apTime > 10)? 10: apTime); // декремент лимита по 10 сек.
    wifi_timer = -50; return; }       // удерживаем WiFi в режиме AP
  static int netUpTime = 5;
  static int net_err = 0;
  if (WiFi.status() == WL_CONNECTED) {
    // Создаем уникальный ID сессии из аппаратного шума радиоэфира
    while (sessionID == 0) sessionID = os_random();
    if (wifi_timer < 5) { wifi_timer = 5 + netUpTime; netUpTime = 0; } // после подключения wifi_timer дает спец. интервал 10..6
    net_err = 0; }
   else {
    if (wifi_timer == 0) {
      if (net_err++ >= 10) {          // ~60 сек. не можем подключиться к WiFi сети
        sta2ap();                     // переключаем WiFi, поднимаем точку доступа (AP) на 120 сек.
        return; }                     // даем возможность переконфигурации параметров подключения к сети
      WiFi.persistent(false);
      delay(1);
      WiFi.disconnect();              // отключаем WiFi модуль, переходим в исходное состояние
      if (mqttSockData.sockState != MQTT_SOCKET_DISCONNECTED)
        mqttDisconnect();             // Жестко закрываем сокет MQTT, очищая буферы
      wifi_timer = -8; wifiState = WIFI_STATE_DEF; sessionID = 0;
      wsMap = 0; socket.clGroup_ID = 0;
      if (!init) errCode = ERR_WIFI_ERR;
      }
    if (wifi_timer < -5)              // -8, -7, -6
      if (WiFi.mode(WIFI_OFF)) {      // если точно выключилось
        // запускаем коннект, от -5 до 0 ждем WL_CONNECTED
        (*cfg.wifi_pass)? WiFi.begin(cfg.wifi_ssid, cfg.wifi_pass): WiFi.begin(cfg.wifi_ssid);
        wifi_timer = -5; netUpTime = 5;
    } }
}

void initWPath(char* msg, uint8_t cid) {
  netQuePut_pre(NULL, 0, msg);
  // перед запуском операции загрузки или печати файла производится листинг с указанием рабочего пути
  // без этого, похоже Марлин не понимает дальнейших команд, содержащих полное имя файла с путем
  size_t len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:<M21\nM20 L \"%s\"\n"),
                                        ((sdFile.wrkPLen)? sdFile.wrkPath: "/"));
  if (ctrl & SERV_LOG) {
    packet[6] = '\x20';                       // маскируем '\n' для вывода в лог
    netQuePut_cid(packet, len, (socket.clGroup_ID & 0xFFFF)? socket.clWS_ID: cid); packet[6] = '\n'; }
  sdFile.selName = 0; sdFile.selIdx = 0;      // ничего не искать и не показывать
  // сбрасываем указатель на запись о файле, индекс начала страницы показа, размер списка файлов
  sdFile.fInfo = NULL; sdFile.pageBegIdx = 0; sdFile.maxIdx = 0;
  fListWait = true; fListTOut = WAIT_FLIST_TIMEOUT;                         // 2 сек
  ok.skip = 1; ok.waiting = true; ok.wdTimer = ok.wdLoad = WAIT_OK_TIMEOUT; // 5 сек 
  uart_w((char*)(packet +3), len - 3);
}

inline void init_chunks() {
  timeEnd = timeStart = millis();
  for (int i = 0; i < NUM_CHUNKS; i++) {  // полностью инициализируем конвейер буферов
    chunks[i].data = (uint8_t*)(&gAnswer_buf[GANSWER_BUF_SIZE - ((i + 1) * CHUNK_SIZE)]);
    chunks[i].chSeq = -1; chunks[i].time = timeStart;
    chunks[i].txMark = 0; chunks[i].len = 0;  chunks[i].offset = 0; }
  ch_req = 0; ch_cnt = 0; ch_in = 0; ch_out = 0;      // Сброс всех индексов и флагов
  chReqSeq = 0; chGotSeq = 0; isEndOfFile = false;
  ch_Total = (socket.fSize / CHUNK_SIZE) + ((socket.fSize % CHUNK_SIZE)? 1: 0);
  lastSentLen = 0; timeResend = 0; ok.waiting = false;  ok.skip = 0;
  // очищаем статистику RESEND и общую
  fData.txSeq = 0;    fData.txTry = 0;        fData.txTotal = 0;
  fData.totalTry = 0; fData.trySeries = 0;
  fData.chunkSym = 0; fData.chunkEOL = 0;     fData.xorState = CS_SKIP;
  fData.payLen = 0;   fData.waitEOL = false;  fData.xorCS = 0;
  socket.progress = 0;
  stat_reset();
}

WSMessage_t wsMsgID(char* msg, size_t msgLen, uint32_t* idNum) {
  char* mPtr = msg;
  uint i = 0, n = 0;
  for (*idNum = 0; ; i++) {
    // Читаем данные структуры из Flash
    const char* msgIDText = (const char*)pgm_read_ptr(&bridgeMsg[i].idText);
    size_t msgIDLen = pgm_read_dword(&bridgeMsg[i].idLen);
    uint32_t msgPrmLen = pgm_read_dword(&bridgeMsg[i].prmLen);
    if (msgIDLen == 0) break; // Конец массива (пустая строка)
                              // Сравнение msg (RAM) с msgIDText (Flash)
    if ((msgLen > msgIDLen) && (strncmp_P(msg, msgIDText, msgIDLen) == 0)) {
      mPtr += msgIDLen;
      while((*mPtr == ' ') && (msgIDLen < msgLen)) { mPtr++; msgIDLen++; } // пропуск пробелов перед числовым параметром
      for (; (n < (msgLen - msgIDLen)); n++)
        // считываем число, контролируем допустимое количество цифр
        if (isdigit((uint)mPtr[n])) {
          if (n >= msgPrmLen) return MSG_ILLEGAL;
          *idNum = (*idNum * 10) + (mPtr[n] & 0xf); }
         else break;
      if (n < (msgLen - msgIDLen)) {
        // если строка продолжается после числового параметра - проверяем разделитель ':' или ' '
        if (mPtr[n] != ((i < MSG_GCODE)? (':'): (' ')))
          n = 0;                  // неверный символ разделителя, 
         else
          n += 1; }               // учитываем символ разделителя в длине параметра
      break; }
    } // for (;; i++)
  if (n == 0) return MSG_ILLEGAL; // ни одной цифры не прочитано
  if (i >= MSG_GCODE) return MSG_GCODE;
  // сдвигаем текст текущего сообщения в начало буфера (удаляем ID и параметр)
  size_t msgIDLen = pgm_read_dword(&bridgeMsg[i].idLen);
  mPtr += n; n = msgLen - (msgIDLen + n);
  if (n <= 0) n = 0;
   else memmove(msg, mPtr, n);
  msg[n] = '\0';                  // гарантируем терминатор строки
  return (WSMessage_t)i;
}

void br_announce(annEvent_t annEvent, uint8_t msg_socket, uint8_t act_socket, uint32_t announceID) {
  size_t len;
  const char* stPtr = (const char*)pgm_read_ptr(&brStateName[bridgeState]);  // состояние системы
  const char* flPtr;
  // Логика проверки: если есть активная сессия И (источник события - не активист ИЛИ событие - смена роли)
  if ((socket.clGroup_ID & 0xFFFF) && (msg_socket != act_socket)) {
    flPtr = (const char*)pgm_read_ptr(&ann[annEvent]); 
    if (flPtr) {
      if (annEvent == ANN_ROLE_TERMINATED)
        len = snprintf_P((char*)packet, NET_DATA_MAX, flPtr, (socket.clGroup_ID  & 0xFFFF));
       else
        len = snprintf_P((char*)packet, NET_DATA_MAX, flPtr, stPtr, announceID);  // ...PSTR("xxxx.... 0x%0*lX"), width, *number);
      netQuePut_cid(packet, len, act_socket); // сообщение в лог текущему активисту
    } }
  flPtr = (annEvent == ANN_ROLE_TERMINATED)?                // меняем текст для нового активиста
          ((msg_socket != act_socket)? ((const char*)pgm_read_ptr(&ann[ANN_ROLE_APPROVED])): NULL):
          (const char*)pgm_read_ptr(&ann[annEvent]);
  if (flPtr) {
    if (annEvent == ANN_ROLE_TERMINATED)
      len = snprintf_P((char*)packet, NET_DATA_MAX, flPtr, announceID);
     else
      len = snprintf_P((char*)packet, NET_DATA_MAX, flPtr, stPtr, announceID);  // ...PSTR("xxxx.... 0x%0*lX"), width, *number);
    netQuePut_cid(packet, len, msg_socket);   // сообщение в лог источнику события
    }
  sessionID = 0;  // по идее, если сюда попали, то нужно обновить контекст UX у клиентов
}

// --- ОБРАБОТЧИК СОБЫТИЙ WEBSOCKET ---
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  int32_t len;
  if (ctrl & SERV_LOG) {
    const char* flPtr = (const char*)pgm_read_ptr(&wsTypeName[(type < WSTYPE_ILLEGAL)? (uint)type: WSTYPE_ILLEGAL]);
    if (flPtr) {
      len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:-wsTYPE- %S\n"), flPtr);
      netQuePut_cid(packet, len, (socket.clGroup_ID & 0xFFFF)? socket.clWS_ID: num);
    } }
  static uint8_t tMasterID = 0xFF;
  uint32_t tNow = millis();
  switch(type) {
    case WStype_TEXT: {  
      if (length > (MAX_FNAME_LEN + 22)) {    //макс.формат = "FILE:xxxxx:xxxxxxxxxx:<fName>" = (112 + 22) символов
        br_announce(ANN_MSG_ILLEGAL, num, socket.clWS_ID, ANN_ID_BIG_MSG); break; }
      bool pgs_flag = false;                  // выбирает WS message (фактически - "anykey") , для сброса MQTT прогресса 100% -> 0%
      char* msg = (char*)payload;
      uint32_t msgPrm = 0;
      WSMessage_t wsMsg = wsMsgID(msg, length, &msgPrm);
      switch (wsMsg) {
        case MSG_ILLEGAL:
          if (!myBridgeCmd(msg, length))      // пытаемся интерпретировать как терминальную команду
            br_announce(ANN_MSG_ILLEGAL, num, socket.clWS_ID, ANN_ID_BAD_MSG);
           else pgs_flag = true;              // флаг "отложенного" сброса 100% прогресса в публикации MQTT
          break;
        case MSG_FILE: {
          // Анонс файла (приходит при выборе в браузере)
          // и если не происходит передача файла или печать
          // позволяет сменить ID активного клиента
          // формат сообщения серверу: "F:" + myID + ":" + file.size + ":" + file.name
          if ((bridgeState != SYS_IDLE) && (bridgeState != SYS_PRE_PRINT) && (bridgeState != SYS_WAIT_OTA)) {
            br_announce(ANN_FILE_ILLEGAL, num, socket.clWS_ID); break; }
          if (bridgeState == SYS_WAIT_OTA) doReboot();
          static uint32_t tAnnounce = 0;                                  // для блокировки конкурентов
          if (((tNow - tAnnounce) < 10000) && (num != socket.clWS_ID)) {
            br_announce(ANN_ROLE_TIMEOUT, num, socket.clWS_ID); break; }  // блокируем конкурентные анонсы файлов на 10 сек
          char* fSize = strtok(msg, ":");
          char* fName = strtok(NULL, ":");
          if (fName == NULL) {            // активизация клиента без выбора файла
            sdFile.opCode = 0; socket.clFileSel = true; socket.fSize = 0; socket.fName[0] = '\0';
            bridgeState = SYS_IDLE;
            }
           else {
            if (strlen(fName) > MAX_FNAME_LEN) {
              br_announce(ANN_FNAME_ILLEGAL, num, socket.clWS_ID); break; }
            sdFile.opCode = 0; socket.clFileSel = true; bridgeState = SYS_IDLE;
            uint32_t sizeCheck = strtoul(fSize, NULL, 10);
            if (ctrl & SERV_LOG) {
              len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:FReq \"%s\" | %d bytes\n"), fName, sizeCheck);
              netQuePut_cid(packet, len, (socket.clGroup_ID & 0xFFFF)? socket.clWS_ID: num);
              }
            if (sizeCheck < 2) {
              br_announce(ANN_FILE_EMPTY, num, socket.clWS_ID); //break;
              socket.fSize = 0; socket.fName[0] = '\0'; // активизация клиента без выбора файла
              }
             else {
              // готовим поиск по socket.fName & socket.fSize
              socket.fSize = sizeCheck; strcpy(socket.fName, fName); // фиксируем параметры выбранного файла
              ok.setState = SYS_WAIT_M20; // перечитать список при неудаче поиска
              fileID(true, socket.fSize); // искать точное имя+размер в раб. папке , вывод списка вариантов
                                          // в случае успеха - переключить bridgeState в SYS_PRE_PRINT
              }
            }
          // фиксируем реквизиты активной сессии
          len = snprintf_P((char*)packet, NET_DATA_MAX, ((fName)? PSTR("\"%s\"\n"): PSTR("No file selected.\n")), fName);
          netQuePut(packet, -len, (char*)PSTR("L::: "));
          br_announce(ANN_ROLE_TERMINATED, num, socket.clWS_ID, msgPrm);
          socket.clGroup_ID = msgPrm; socket.clWS_ID = num; tAnnounce = tNow;
          pgs_flag = true;                // флаг "отложенного" сброса 100% прогресса в публикации MQTT
          break;
          }
        case MSG_START: {
          if (millis() <= 30000) break;
          uint32_t annID = 0;
          if (!(socket.clGroup_ID & 0xFFFF))  annID |= ANN_ID_GID_ZERO;
          if (num != socket.clWS_ID)          annID |= ANN_ID_WSID_NEQ;
          if ((bridgeState != SYS_IDLE) && (bridgeState != SYS_WAIT_OTA))
                                              annID |= ANN_ID_IDLE;
          if (!sdIsOK && (bridgeState != SYS_WAIT_OTA))
                                              annID |= ANN_ID_NO_SDCARD;
          if (socket.fSize == 0)              annID |= ANN_ID_FSIZE_ZERO;
          if (annID != 0) {
            br_announce(ANN_START_ILLEGAL, num, socket.clWS_ID, annID);
            len = 0; fListScanned = FLIST_NOT_SCANNED;
            for (int i = 0; i < 3; i++)
              len += snprintf_P((char*)packet + len, NET_DATA_MAX - len, PSTR("H:0:%d:%d:%d\n"),
                                HB_ERROR,
                                (socket.clGroup_ID & 0xFFFF)?               // сброс активиста, если он является источником
                                  ((socket.clGroup_ID & 0xFFFF) + !!(num == socket.clWS_ID)): 0,
                                ((ctrl & CLIENT_LOG) >> 4));
            netQuePut_cid(packet, -len, num);                               // urgent HB - переводим клиента в состояние "ошибка"
            if ((socket.clGroup_ID & 0xFFFF) && (num == socket.clWS_ID)) {  // (если от активиста - очищаем сокет)
              socket.clGroup_ID = socket.fSize = socket.progress = 0;
              socket.fName[0] = '\0'; socket.clFileSel = false; }
            break; }
          init_chunks();              // полностью инициализируем конвейер буферов
          if (bridgeState == SYS_WAIT_OTA) {
            otaTimer = 0; wd_ACT_Timer = 0; wd_BIN_Timer = 0; ok.wdTimer = 0; ok.skip = 0; ok.waiting = false; // сброс всех таймеров и флагов ожидания
            M20_Timer = 0; fListTOut = 0; M30_Timer = 0;
            // Считаем пространство заново (для надежности ядра)
            uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
            // ФИЗИЧЕСКИЙ СТАРТ: Стираем первый сектор и готовим SPI-шину к записи
            if (!Update.begin(maxSketchSpace, U_FLASH))
              errCode |= ERR_OTA_BAD_INIT;
             else {
              onOTAStart();           // bridgeState = SYS_OTA;
              heartbeat(true); }      // будет также сформирован запрос 1го BIN
            break; }
          useBFT = (msgPrm != 0);     // Получаем режим (0=ASCII или 1=BFT)
          bridgeState = SYS_PRE_UPLD; // для контроля SD и рабочей папки
          heartbeat(true);            // будет также сформирован запрос 1го BIN
          initWPath((char*)PSTR("L::: Upload started.\n"), num); // "инициализация" рабочего пути
          break; }
        case MSG_END: {
          // Финал передачи
          uint32_t annID = 0;
          if (bridgeState == SYS_OTA_END) break;
          if (!(socket.clGroup_ID & 0xFFFF))  annID |= ANN_ID_GID_ZERO;
          if (num != socket.clWS_ID)          annID |= ANN_ID_WSID_NEQ;
          if (!((bridgeState == SYS_PRE_UPLD) || (bridgeState ==  SYS_TRANSFER) ||
                ((bridgeState == SYS_WAIT_M29) && (ch_req)) || (bridgeState == SYS_OTA)))
                                              annID |= ANN_ID_NO_UPLOAD;
          //if (!(ch_req))              annID |= ANN_ID_NO_UPLOAD;
          if (annID != 0) {
            if (bridgeState != SYS_IDLE) br_announce(ANN_END_ILLEGAL, num, socket.clWS_ID, annID);
            break; }
          ch_req -= 1;                                                  // корректируем количество запрошенных чанков
          if (M20_Timer) break; // если уже идет отсчет перед сменой состояния системы (после M29)
          uint32_t bTotal = 0, cIdx = ch_out;
          for (int i = 0; i < ch_cnt; i++, ++cIdx &= (NUM_CHUNKS - 1))  // подсчет общего количества байтов, еще находящихся в чанках
            bTotal += (chunks[cIdx].len - chunks[cIdx].offset);
          size_t len;
          if ((useBFT) || (bridgeState == SYS_OTA))
            len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:END %d vs %d [%dP + %d(in %d)]\n"),
                        msgPrm, (socket.progress + bTotal), socket.progress, bTotal, ch_cnt);
           else
            len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:END %d vs %d [%dP + %dS + %d(in %d)]\n"),
                        msgPrm, (socket.progress + lastSentLen + bTotal), socket.progress, lastSentLen, bTotal, ch_cnt);
          bTotal += ((useBFT) || (bridgeState == SYS_OTA))? 0: lastSentLen;
          if (msgPrm != (socket.progress + bTotal))
            errCode |= ERR_END_ERR;       // "END:<f_size>" не соответствует сумме ("уже да" + "еще нет")
          if ((ctrl & TRANS_LOG) || (errCode & ERR_END_ERR))
            netQuePut_cid(packet, len, socket.clWS_ID);
          if (errCode & ERR_END_ERR)
            break;
          isEndOfFile = true;             // Помечаем, что новых BIN данных не будет (для логики финала BFT)
          if (msgPrm == socket.progress) {
            updIntValue(ID_SD_PROGRESS, millis(), socket.progress, socket.fSize);  // фиксируем прогресс 100% для MQTT
            if (bridgeState == SYS_TRANSFER) {
              // здесь уже все получено марлин и подтверждено 
              bridgeState = SYS_WAIT_M29;
              ok.waiting = true; ok.skip = 0; ok.setState = SYS_PRE_PRINT; ok.wdTimer = ok.wdLoad = WAIT_OK_TIMEOUT; // 5 sec
              if (useBFT) sendBFT(0, BFT_CLOSE);
               else uart_w((char*)PSTR("M29\n"));
              }
             else if (bridgeState == SYS_OTA) {
              // Завершаем прошивку во флеш-памяти и проверяем MD5 хэш
              printStat();              // печать статистики
              // Передаем true для валидации размера
              if (Update.end(true)) {
                // Отправляем финальный лог во фронтенд
                strcpy_P((char*)packet, PSTR("M117 OTA success!\n"));
                netQuePut(packet + 5, strlen((char*)packet) - 5, (char*)PSTR("L:"));
                uartWxStop = false; uart_w((char*)packet); uartWxStop = true;
                if (dState[DISCOVERY_COUNT - 1] == DISCOVERY_ANNOUNCED) // проверяем состояние MQTT
                  bridgeState = SYS_OTA_END;                            // перезагрузка после MQTT публикации прогресса (mqttLoop())
                 else doReboot();                                       // MQTT не работает - сразу перезагрузка
                }
               else {
                // Ошибка валидации файла (например, битый бинарник или не совпал размер)
                uartWxStop = false; uart_w((char*)PSTR("M117 OTA error!\n")); uartWxStop = true;
                errCode |= ERR_OTA_BAD_FIN;
            } } }
          break; }
        case MSG_PRINT:
          // Запрос начала печати файла (по нажатию соотв. кнопки в интерфейсе клиента)
          // формат сообщения серверу: "P:" + myID
          // м.б. послан с любого терминала и, если все нормально, клиент назначается активистом
          // структура sdFile должна быть заполнена и структура socket - тоже (для одного и того же файла)
          if (bridgeState != SYS_PRE_PRINT) {
            br_announce(ANN_PRNREQ_ILLEGAL, num, socket.clWS_ID); break; }
          if (strlen(socket.fName) == 0) {
            br_announce(ANN_NO_PRN_NAME, num, socket.clWS_ID); break; }
          if (socket.fSize <= 3) {
            br_announce(ANN_BAD_PRN_SIZE, num, socket.clWS_ID); break; }
          // фиксируем реквизиты активной сессии
          if (num != socket.clWS_ID) {
            br_announce(ANN_ROLE_TERMINATED, num, socket.clWS_ID);
            socket.clGroup_ID = msgPrm; socket.clWS_ID = num;
            }
          if (ctrl & SERV_LOG) {
            len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:Print req %s\n"), msg);
            netQuePut_cid(packet, len, (socket.clGroup_ID & 0xFFFF)? socket.clWS_ID: num); }
          socket.fSize = sdFile.fSize; socket.progress = 0; pgs_flag = true; // флаг "отложенного" сброса 100% прогресса в публикации MQTT
          bridgeState = SYS_WAIT_M23; ok.setState = SYS_PRE_PRINT;
          initWPath((char*)PSTR("L::: Print started.\n"), num); // "инициализация" рабочего пути
          break;
        case MSG_TIME: {
          // синхропакет времени от активного клиента либо от первого подключенного при отсутствии активиста
          // формат сообщения серверу: "T:" + UNIX_timestamp
          // определение допустимого источника синхропакетов времени
          uint32_t legalMaster = (socket.clGroup_ID & 0xFFFF)?            // активист есть ?
                                  socket.clWS_ID:                         // да  : это должен быть активист
                                  ((tMasterID == 0xFF)? num: tMasterID);  // нет : может быть текущий мастер, либо источник этого сообщения при отсутствии мастера
          if ((num != legalMaster)                                ||      // фильтр по источнику сообщения
              ((timeLastSync) && ((tNow - timeLastSync) < 60000)) ||      // блокировка "дребезга"
              (msgPrm <= 1451616000))                                     // Базовая валидация (время строго позже 2016 года)
            break;
          tMasterID = num; timeLastSync = tNow;
          timeval tv = { .tv_sec = (time_t)msgPrm, .tv_usec = 0 };
          settimeofday(&tv, NULL);
          if ((time(nullptr) > 1451616000) && !syncReported) {
            // после первой синхронизации сообщаем всем подключенным клиентам инфо о системе
            syncReported = true; showTime(CID_ALL); }
          if (ctrl & SERV_LOG) {
            len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:Time sync WS: %u, Role: none\n"), tMasterID);
            if (socket.clGroup_ID & 0xFFFF) {
              len -= 5;
              len += snprintf_P((char*)packet + len, NET_DATA_MAX - len, PSTR("%u\n"), socket.clGroup_ID & 0xFFFF); }
            netQuePut_cid(packet, len, tMasterID); }
          break; }
        case MSG_SCREAM: {
          // оповещение от активного клиента
          uint32_t annID = 0;
          if (!(socket.clGroup_ID & 0xFFFF))  annID |= ANN_ID_GID_ZERO;
          if (msgPrm != socket.clGroup_ID)    annID |= ANN_ID_GID_NEQ;
          if (memchr_P(PSTR("><!"), *msg, 3)) annID |= ANN_ID_SYM_BAD;
          if (annID != 0) {
            br_announce(ANN_SCREAM_ILLEGAL, num, socket.clWS_ID, annID); break; }
          len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:%s\n"), msg);
          netQuePut(packet, len);
          break; }
        case MSG_GCODE: {
          // G-code команды (G28, M104 и т.д.)
          uint32_t annID = 0;
          if (!(socket.clGroup_ID & 0xFFFF))  annID |= ANN_ID_GID_ZERO;
          if (num != socket.clWS_ID)          annID |= ANN_ID_WSID_NEQ;
          if ((bridgeState != SYS_IDLE) && (bridgeState != SYS_PRE_PRINT))
                                              annID |= ANN_ID_IDLE;             
          if (length > (NET_DATA_MAX - 5))    annID |= ANN_ID_GCODE_BIG;
          if (annID != 0) {
            br_announce(ANN_GCODE_ILLEGAL, num, socket.clWS_ID, annID); break; }
          // канал входящих MQTT-команд может быть "зарезервирован" для socket.mqttCtrl_ID на время mqttBusyTime
          if (socket.mqttCtrl_ID)
            if (((tNow - timeMQTTCtrl) < mqttBusyTime) &&
                (socket.mqttCtrl_ID != (socket.clGroup_ID & 0xFFFF))) {
              br_announce(ANN_GCODE_BUSY, num, socket.clWS_ID); break; }
          ok.doLog = true; ok.waiting = false; ok.skip = 0; ok.wdTimer = ok.wdLoad = 0;
          if (length >= 3) {
            // корректируем автомат поведения программы в зависимости от введенных команд
            uint32_t gStr = (msg[0] | (msg[1] << 8) | (msg[2] << 16) | (msg[3] << 4)); // делаем из строки число (LE) для быстрого сравнения
            if ((((gStr & 0x00FFFFFF) | 0x20) == 0x31326D) || ((gStr | 0x20) == 0x3132206D)) {        // "M21", "m21", "M 21", "m 21"
              fListWait = false; fListScanned = FLIST_NOT_SCANNED; }              // - разрешаем автоскан SD и сообщение о таймауте
             else if ((((gStr & 0x00FFFFFF) | 0x20) == 0x32326D) || ((gStr | 0x20) == 0x3232206D)) {  // "M22", "m22", "M 22", "m 22"
              fListWait = true;  fListScanned = FLIST_NOT_SCANNED; }              // запрещаем автоскан SD
             else if ((((gStr & 0x00FFFFFF) | 0x20) == 0x38326D) || ((gStr | 0x20) == 0x3832206D))    // "M28", "m28", "M 28", "m 28"
              cmdMode += 1;                                                       // запрещаем опрос параметров (mqtt.ino)
            }                                                                     // восстановление опроса - после успешного ответа на M21
          memcpy(packet, payload, length); packet[length++] = '\n';
          uart_w((char*)packet, length);                                          // отправляем введенный G-code в принтер
          netQuePut_pre(packet, length, (char*)PSTR("L:>:: "));                   // отправляем введенный G-code в лог всем
          // "резервируем" канал входящих MQTT-команд для WS клиента socket.clGroup_ID на время mqttBusyTime
          socket.mqttCtrl_ID = (socket.clGroup_ID & 0xFFFF); timeMQTTCtrl = tNow; mqttBusyTime = 10000;
          pgs_flag = true;                                                        // флаг "отложенного" сброса 100% прогресса в публикации MQTT
          break;
          }
        default:
          br_announce(ANN_MSG_PARSE_ERROR, num, socket.clWS_ID);
          break;
        } // switch (wsMsg) {
      if ((pgsReset == PROGRESS_WAIT) && pgs_flag) {
        pgsReset = PROGRESS_LOCK;                                                 // делаем "отложенный" сброс 100% прогресса
        updIntValue(ID_SD_PROGRESS, millis(), socket.progress, socket.fSize); }   // для публикации MQTT
      break; } // case WStype_TEXT:
    case WStype_BIN: {
      uint32_t annID = 0;
      if (!(socket.clGroup_ID & 0xFFFF))  annID |= ANN_ID_GID_ZERO;
      if (num != socket.clWS_ID)          annID |= ANN_ID_WSID_NEQ;
      if (((bridgeState < SYS_PRE_UPLD) || (bridgeState > SYS_TRANSFER)) && (bridgeState != SYS_OTA))
                                          annID |= ANN_ID_NO_UPLOAD;
      if (ch_cnt >= NUM_CHUNKS)           annID |= ANN_ID_CHUNKS_OVF;
      if (annID) { br_announce(ANN_BIN_ILLEGAL, num, socket.clWS_ID, annID); break; }
      if (!(ch_req))  { errCode |= ERR_CH_REQ_ERR; break; }
      wd_BIN_Timer = WAIT_BIN_TIMEOUT;                              // 10 сек. продлеваем WD таймер
      ch_req -= 1; chGotSeq += 1;                                   // корректируем счетчики чанков
      if (chGotSeq >= ch_Total) isEndOfFile = true;                 // по идее бинарных данных больше не будет
      int32_t len;
      uint32_t cN = (ch_in + 1) & (NUM_CHUNKS - 1);                 // "следующий" буфер, NUM_CHUNKS обязательно == 2 или 4
      uint32_t emptyTime = tNow - chunks[ch_in].time;
      stat(MTR_CHUNK_EMPTY, emptyTime);
      if (ctrl & TRANS_LOG) {
        const char* o_equ_i = ((ch_out == ch_in)? "=o": "");
        uint32_t ciTime = ((chGotSeq > 1)? emptyTime: 0);
        const char* o_is_next = ((ch_out == cN)? "o": "");
        uint32_t cnTime = ((chGotSeq > 1)? (tNow - chunks[cN].time): 0);
        len = snprintf_P((char*)packet, NET_DATA_MAX,
                                PSTR("L:+%d i%s%d:%dms:%d/%d/%d %s%d:%dms:%d/%d/%d (%d)\n"),
                                chGotSeq, o_equ_i, ch_in,
                                ciTime, chunks[ch_in].chSeq, chunks[ch_in].offset, chunks[ch_in].len,
                                o_is_next, cN,
                                cnTime, chunks[cN].chSeq, chunks[cN].offset, chunks[cN].len,
                                ch_cnt);
        netQuePut_cid(packet, len, socket.clWS_ID); }
      uint32_t cP = (ch_in + (NUM_CHUNKS - 1)) & (NUM_CHUNKS - 1);  // "предыдущий", последний опустевший буфер,
                                                                    // NUM_CHUNKS обязательно == 2 или 4
      if (!(ch_cnt) && ((chGotSeq > NUM_CHUNKS) && !isEndOfFile)) { // учитываем статистику после первого цикла заполнения всех буферов
          emptyTime = tNow - chunks[cP].time;                       // время пока все буферы были пусты
          stat(MTR_ZERO_BUF, emptyTime); }
      memcpy(chunks[ch_in].data, payload, length);                  // заполняем буфер данными из полученного чанка
      chunks[ch_in].len = length; chunks[ch_in].offset = 0;         // заполняем поля структуры
      chunks[ch_in].chSeq = chGotSeq; chunks[ch_in].time = tNow;    // фиксируем время заполнения буфера
      ch_cnt += 1;                                                  // корректируем количество заполненных буферов
      ch_in = cN;                                                   // (ch_in + 1) % NUM_CHUNKS; Переключаем прием на "следующий" буфер
      // УПРЕЖДЕНИЕ: для накачки конвейера на старте - просим сразу ещё
      uint32_t req_mem = chReqSeq;
      for ( len = 0;
            (((ch_cnt + ch_req) < NUM_CHUNKS) && (chReqSeq < ch_Total));
            ch_req += 1, chReqSeq += 1 )
        len += snprintf_P((char*)packet + len, NET_DATA_MAX - len, PSTR("N:%d\n"), (socket.clGroup_ID & 0xFFFF));
      if (len) {
        netQuePut_cid(packet, -((int32_t)len), socket.clWS_ID); // == urgent
        if (ctrl & TRANS_LOG) {
          if (++req_mem == chReqSeq)
            len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("%u\n"), chReqSeq);
           else
            len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("%u..%u\n"), req_mem, chReqSeq);
          netQuePut(packet, len, (char*)PSTR("L:pre-Request for N:"), socket.clWS_ID); }
        }
      break; }
    case WStype_DISCONNECTED:
      wsMap &= ~(1 << (num & 7)); // сброс бита с номером клиента в общей карте подключений
      // при отключении любого клиента - сбрасываем соответствующий бит в масках всех пакетов в очереди отправки
      // просто накладываем актуальное значение общей карты wsMap на поле header.cMap в заголовках всех сообщений в буфере
      netQueClean();
      if (num == socket.clWS_ID) {
        // если отключается активист - ставим флаг ошибки
        // можно не обрывать печать, если это True-Stateless, но зафиксировать ошибку в логе стоит.
        errCode |= ERR_WS_DISCONNECT;
        // ctrl &= ~(UART_LOG & CLIENT_LOG & CLIENT_BIN & TRANS_LOG & MQTT_LOG & FULL_STAT); // сброс отладки кроме SERV_LOG
        ctrl = 0;                                         // сброс отладки
        socket.clGroup_ID <<= 16;                         // обнуляем в младшей половине слова + копируем в старшую половину
        if (bridgeState != SYS_PRINT) {
      /* !? */
          socket.fSize = socket.progress = 0;
          socket.fName[0] = '\0';
        } }
      if (num == tMasterID) tMasterID = 0xFF;             // если отключился "мастер времени" - сбрасываем его идентификатор
      break;
    case WStype_CONNECTED:
      wsMap |= (1 << (num & 7));                          // установка бита с номером клиента в общей карте подключений
      if (time(nullptr) > 1451616000) showTime(num);      // сообщаем подключившемуся клиенту тек. дату/время
      break;
    default:
      break;
    }
}

// fletchers 16 checksum
inline uint32_t csFletchers16(uint32_t cs, uint8_t value) {
  uint32_t cs_low = (((cs & 0xFF) + value) % 255);
  return ((((cs >> 8) + cs_low) % 255) << 8)  | cs_low;
}

void sendBFT(size_t dataSize, uint8_t opMeta, uint8_t pDummy, uint8_t pShrink) {
  packet[0] = BFT_TOKEN_LOW; packet[1] = BFT_TOKEN_HIGH;
  packet[2] = ((seqBFT >= 0)? (uint8_t)(seqBFT & 0xFF): 0); // Sync (Seq.Num)
  packet[3] = ((seqBFT >= 0)? opMeta: BFT_SYNC);            // Meta
  size_t paySize = 0;
  const char* m_ptr = NULL;
  if (opMeta != BFT_WRITE) {
    packet[8] = pDummy; packet[9] = pShrink; }
  switch (opMeta) {
    case BFT_OPEN:
      paySize = 3 + snprintf_P((char*)(packet + 10), PACKET_BUF_SIZE - 10, PSTR("%s%s"),
                                ((sdFile.wrkPLen)? sdFile.wrkPath: ""), socket.fName);
      m_ptr = PSTR("BFT_OPEN");
      break;
    case BFT_WRITE:
      paySize = dataSize; // начиная с packet[8] д.б. размещены данные длиной dataSize
      m_ptr = PSTR("BFT_WRITE");
      break;
    case BFT_CLOSE:
      m_ptr = PSTR("BFT_CLOSE");
      break;
    case BFT_ABORT:
      m_ptr = PSTR("BFT_ABORT");
      break;
    case BFT_QUERY:
      m_ptr = PSTR("BFT_QUERY");
      break;
    case BFT_SYNC:
      m_ptr = PSTR("BFT_SYNC");
      break;
    case BFT_EXIT:
      m_ptr = PSTR("BFT_EXIT");
      break;
    default:
      break;
    }
  packet[4] = (uint8_t)(paySize & 0xFF); 
  packet[5] = (uint8_t)(paySize >> 8);
  // CS заголовка (байты 2,3,4,5)
  uint32_t cs = 0;
  for (int i = 2; i <= 5; i++) cs = csFletchers16(cs, packet[i]);
  packet[6] = (uint8_t)cs;                    //(cs & 0xFF);
  packet[7] = (uint8_t)(cs >> 8);
  cs = csFletchers16(cs, packet[6]);
  cs = csFletchers16(cs, packet[7]);
  // CS данных (Payload начинается с 9-го байта)
  for (size_t i = 0; i < paySize; i++) cs = csFletchers16(cs, packet[8 + i]);
  packet[8 + paySize] = (uint8_t)cs;          //(h_cs & 0xFF);
  packet[9 + paySize] = (uint8_t)(cs >> 8);

  if (useBFT && (bridgeState >= SYS_WAIT_M28) && (bridgeState <= SYS_WAIT_M29)) {
    ok.waiting = true;  ok.skip = 0; ok.wdTimer = ok.wdLoad = WAIT_OK_TIMEOUT;   // 5 sec
    }
  WITH_FLAG_OFF(UART_SEND, (uart_w((char*)packet, 10 + paySize, true)));
  ok.time = millis();               // новый отсчет до получения подтверждения от Марлин
  if (ctrl & SERV_LOG) {
    char lBuf[12];
    if (m_ptr)
      strcpy_P(lBuf, m_ptr);
     else
      snprintf_P(lBuf, sizeof(lBuf), PSTR("meta 0x%02X"), packet[3]);
    size_t len = snprintf_P((char*)packet, NET_DATA_MAX,
                            PSTR("L:BFTmsg#%d %s cs:0x%02X:0x%02X %d bytes\n"),
                            packet[2], lBuf, packet[8], packet[9], paySize);
    netQuePut_cid(packet, len, socket.clWS_ID);
    }
  if (opMeta >= BFT_QUERY)
    seqBFT = ((seqBFT >= 0)? ((seqBFT + 1) & 0xFF): seqBFT);
   else if (opMeta == BFT_EXIT) {
    seqBFT = -1;
    delay(200); 
    //memset((uint8_t*)svcBuf, 0, SVC_BUF_SIZE);    
    // Шлем чистый код точки с запятой (59) и перевода строки (10)
    // Без использования PSTR и fl2chr!
    //const char cleanExit[] = {';', 10}; 
    //uart_w((char*)cleanExit, 2, true);
    WITH_FLAG_OFF(UART_SEND, (uart_w((char*)PSTR("\n"), 1, true)));
    }
}

void countData(uint32_t addCount) {
  chunks[ch_out].offset += addCount;
  if (chunks[ch_out].offset >= chunks[ch_out].len) {    // если тек.буфер исчерпан
    timeEnd = millis();                                 // потенциально м.б. окончанием всего процесса передачи файла
    // вычисляем время передачи чанка в марлин
    uint32_t txTime = (timeEnd - chunks[ch_out].txMark);
    stat(MTR_CHUNK_TX, txTime);
    // вычисляем общее время нахождения чанка в буфере
    uint32_t goneTime = (timeEnd - chunks[ch_out].time);
    stat(MTR_CHUNK_GONE, goneTime);
    if (((socket.progress + addCount + (!(useBFT)? lastSentLen: 0)) != (((uint32_t)chunks[ch_out].chSeq) * CHUNK_SIZE)) &&
        ((((uint32_t)chunks[ch_out].chSeq) * CHUNK_SIZE) <= socket.fSize))
      errCode |= ERR_UPLOAD_ERR;
    int32_t len;
    if ((ctrl & TRANS_LOG) || (errCode & ERR_UPLOAD_ERR)) {
      uint32_t wrSize = ((bridgeState == SYS_OTA)? CHUNK_SIZE: BFT_SIZE);
      if (useBFT || (bridgeState == SYS_OTA))
        len = snprintf_P((char*)packet, NET_DATA_MAX,
                  PSTR("L:gone%d %ums %d:%u/%u bft%u(%u*%u+%u) pgs%u(%up+%u) ch%u*%u=%u(%d)\n"),
                  chunks[ch_out].chSeq, goneTime,
                  ch_out, chunks[ch_out].offset, chunks[ch_out].len,
                  (fData.txSeq * wrSize) + addCount, fData.txSeq, wrSize, addCount,
                  (socket.progress + addCount), socket.progress, addCount,
                  chunks[ch_out].chSeq, CHUNK_SIZE, (chunks[ch_out].chSeq * CHUNK_SIZE),
                  ch_cnt);
       else
        len = snprintf_P((char*)packet, NET_DATA_MAX,
                  PSTR("L:gone%d %ums %d %u/%u eol%d%c sym%d tot%d(%ds+%dl+%da) %d*%d=%d(%d)\n"),
                  chunks[ch_out].chSeq, goneTime,
                  ch_out, chunks[ch_out].offset, chunks[ch_out].len,
                  fData.chunkEOL, ((fData.waitEOL)? '+': '='), fData.chunkSym,
                  (socket.progress + lastSentLen + addCount), socket.progress, lastSentLen, addCount,
                  chunks[ch_out].chSeq, CHUNK_SIZE, (chunks[ch_out].chSeq * CHUNK_SIZE),
                  ch_cnt);
      netQuePut_cid(packet, len, socket.clWS_ID);
      }
    if (!(errCode & ERR_UPLOAD_ERR)) {
      chunks[ch_out].chSeq = -1; chunks[ch_out].time = timeEnd; chunks[ch_out].txMark = 0;
      chunks[ch_out].offset = 0; chunks[ch_out].len = 0 ; // помечаем пустым
      ch_cnt -= 1; ++ch_out &= (NUM_CHUNKS - 1); // корректируем число свободных буферов, сдвигаем индекс приемника
      if ((ch_cnt + ch_req) < NUM_CHUNKS) {
        len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("N:%d\n"), (socket.clGroup_ID & 0xFFFF));
        netQuePut_cid(packet, ((isEndOfFile)? len: -(len)), socket.clWS_ID); ch_req += 1; chReqSeq += 1; // (not)urgent
        if (ctrl & TRANS_LOG) {
          len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:gone-Request for N:%u\n"), chReqSeq);
          netQuePut_cid(packet, len, socket.clWS_ID); }
    } } }
}

void putMarlin() {
  // --- Марлин : Автомат передачи данных ---
  if ((ch_cnt) && !ok.waiting && (errCode == ERR_NO_ERRORS) && !(timeResend)) {
    // при наличии данных в буфере передачи и сброшенном ok.waiting готовим и передаем в UART :
    // - в режиме ASCII - строку G-команды
    // - в режиме BFT - блок BFT_SIZE символов
    // после чего устанавливаем ok.waiting и выполнение этой части программы блокируется до сброса ok.waiting ответом ОК от Марлин
    uint32_t tNow = millis();
    if (ctrl & TRANS_LOG) {
      uint32_t cI = (ch_out + 1) & (NUM_CHUNKS - 1);
      const char* sO;
      if (!useBFT || (chunks[ch_out].len == 0)) sO = "";
       else if ((chunks[ch_out].len - chunks[ch_out].offset) < BFT_SIZE) sO = "<";
       else if ((chunks[ch_out].len - chunks[ch_out].offset) == BFT_SIZE) sO = "=";
       else sO = ">";
      const char* sI;
      if (!useBFT || (chunks[cI].len == 0)) sI = "";
       else if ((chunks[cI].len - chunks[cI].offset) < BFT_SIZE) sI = "<";
       else if ((chunks[cI].len - chunks[cI].offset) == BFT_SIZE) sI = "=";
       else sI = ">";
      const char* i_equ_o = ((ch_out == ch_in)? "=i": "");
      uint32_t outTime = (tNow - chunks[ch_out].time);
      const char* i_is_next = ((ch_in == cI)? "i": "");
      uint32_t nextTime = (tNow - chunks[cI].time);
      size_t len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:-%d o%s%d:%dms:%d/%d/%d%s %s%d:%dms:%d/%d/%d%s (%d)\n"),
                                                        chunks[ch_out].chSeq,
                                                        i_equ_o, ch_out, outTime,
                                                        chunks[ch_out].chSeq, chunks[ch_out].offset, chunks[ch_out].len, sO,
                                                        i_is_next, cI, nextTime,
                                                        chunks[cI].chSeq, chunks[cI].offset, chunks[cI].len, sI,
                                                        ch_cnt);
      netQuePut_cid(packet, len, socket.clWS_ID);
      }
    if (chunks[ch_out].chSeq == 1)
      if (!(chunks[ch_out].offset)) timeStart = tNow;       // фиксируем время начала передачи данных в Марлин
    size_t avail0 = chunks[ch_out].len - chunks[ch_out].offset;
    char* pData = (char*)(chunks[ch_out].data + chunks[ch_out].offset);
    if (!useBFT) {
      // --- ASCII: Ищем \n ---
      if (chunks[ch_out].txMark == 0) {
        // начало передачи этого чанка в марлин
        // для ASCII режима очищаем символьную и строчную статистику
        fData.chunkSym = 0; fData.chunkEOL = 0;
        // фиксируем время начала передачи
        chunks[ch_out].txMark = ((tNow)? tNow: 0xFFFFFFFF); // д.б. ненулевое значение
        }
      uint8_t* l_ptr = packet;
      if (ctrl & TRANS_LOG) {
        strcpy_P((char*)l_ptr, PSTR("L:$\""));
        l_ptr += 4;
        }
      uint8_t* tx_ptr = l_ptr;
      uint8_t* pEOL = (uint8_t*)memchr(pData, '\n', avail0);
      if (!fData.waitEOL) {
        // Если начало новой строки — инициируем префикс, сбрасываем xorCS инфо 
        int n = snprintf_P((char*)l_ptr, 16, PSTR("N%d "), ++fData.txSeq);
        fData.xorCS = 0; fData.xorState = CS_SKIP;
        for (int i = 0; i < n; i++) fData.xorCS ^= *l_ptr++;
        }
      fData.waitEOL = (pEOL == nullptr);
      size_t sendLen = (!fData.waitEOL) ? ((pEOL - (uint8_t*)pData) + 1) : avail0;
      fData.chunkSym += sendLen;
      size_t xorLen = 0; fData.payLen = 0;
      for (; (fData.xorState <= CS_COUNT) && (xorLen < sendLen); xorLen++) {
        char c = pData[xorLen];
        if ((fData.xorState == CS_SKIP) && (c == '\x20'))
          continue;
        fData.xorState = CS_COUNT;
        if ((c == ';') || (c == '\r') || (c == '\n'))
          fData.xorState = CS_FOUND;// флаг = "конец данных для xorCS"
         else {
          if ((size_t)(l_ptr - packet) < (NET_DATA_MAX - 7)) { // "*123\"\n"+'\0' = 7 символов
            fData.xorCS ^= (uint8_t)c; *l_ptr++ = c;
            fData.payLen += 1; fData.txTotal += 1; }
           else {
            errCode |= ERR_BIG_DATA; return; }
        } }
      if (fData.xorState == CS_FOUND) {
        l_ptr += snprintf((char*)l_ptr, 10, "*%d", fData.xorCS);
        fData.xorState = CS_SENT;   // флаг = "xorCS передана Марлин" 
        }
      if (!fData.waitEOL) {
        *l_ptr++ = '\n';
        ok.waiting = true; ok.time = millis(); ok.skip = 0; ok.wdTimer = ok.wdLoad = WAIT_OK_TIMEOUT; // Нашли \n — ждем ok до 5 сек
        fData.chunkEOL += 1; fData.txTotal += 2;  // в конце каждой строки марлин пишет 2 символа (\r\n)
        }
      if (l_ptr > tx_ptr)
        WITH_FLAG_OFF(UART_SEND, (uart_w((char*)tx_ptr, (l_ptr - tx_ptr), true)));
      if (ctrl & TRANS_LOG) {
        if (!fData.waitEOL) l_ptr -= 1;
        l_ptr += snprintf_P((char*)l_ptr, NET_DATA_MAX - (l_ptr - packet),
                            PSTR("\" %d(%d)[%d/%d]\n"), sendLen, fData.payLen,
                            (chunks[ch_out].offset + sendLen), chunks[ch_out].len);
        netQuePut_cid(packet, l_ptr - packet, socket.clWS_ID);
        }
      // Если \n нет - не ждем ok, просто идем дальше (Marlin склеит в буфере)
      countData(sendLen);
      lastSentLen += sendLen; // учтем ранее переданную часть этой строки
      }
     else {
      // Binary File Transfer mode
      // 1. Считаем суммарно доступные байты в очереди (Peek)
      size_t avail1 = 0;
      uint8_t nextIdx = (ch_out + 1) & (NUM_CHUNKS - 1); // (ch_out + 1) % NUM_CHUNKS;
      if (ch_cnt > 1) {       // Для NUM_CHUNKS > 2 здесь должен быть цикл по ch_cnt
        if (chunks[nextIdx].len >= chunks[nextIdx].offset)
          avail1 = chunks[nextIdx].len - chunks[nextIdx].offset;
        }
      size_t total = avail0 + avail1;
      // 2. Шлем только полный пакет (BFT_SIZE) или финал файла
      //bool isFinal = (isEndOfFile && (ch_cnt == 1) && (avail0 < BFT_SIZE));
      //if ((total >= BFT_SIZE) || isFinal) {
      if ((total >= BFT_SIZE) || isEndOfFile) {
      //// 2. Шлем пакет
      //if (total) {  // если пакет короче BFT_SIZE, а дополнять неоткуда (ch_cnt == 1) - все равно шлем
        size_t sendLen = (total > BFT_SIZE) ? BFT_SIZE : total;
        // Сборка из первого буфера (или его остатка)
        size_t part0Len = (avail0 > sendLen) ? sendLen : avail0;
        memcpy(packet + 8, pData, part0Len);
        if (chunks[ch_out].txMark == 0)
          // чанк является ch_out - начинаем учет
          // фиксируем время начала передачи этого чанка
          chunks[ch_out].txMark = ((tNow)? tNow: 0xFFFFFFFF);
        // Склейка: добор из второго буфера, если в первом не хватило
        size_t part1Len = 0;
        if (part0Len < sendLen) {
          part1Len = sendLen - part0Len;
          memcpy(packet + (8 + part0Len), chunks[nextIdx].data + chunks[nextIdx].offset, part1Len);
          }
        lastSentLen = sendLen; // Сохраняем длину для обработки в getMarlin() при получении BFT_ACK
        sendBFT(sendLen, BFT_WRITE);
        if (ctrl & TRANS_LOG) {
          size_t pLen = snprintf_P((char*)packet, NET_DATA_MAX,
                          PSTR("L:BFT %u(%u.%u)=%u %u:%d:%ums%u|%u/%u%c%u:%d:%ums%u|%u/%u =%d:o%d:i%d\n"),
                          fData.txSeq, (seqBFT & 0xFF), fData.txTry, sendLen,
                          ch_out, chunks[ch_out].chSeq, (tNow - chunks[ch_out].time), part0Len, chunks[ch_out].offset, chunks[ch_out].len,
                          ((part1Len != 0)? '+': '\x20'),
                          nextIdx, chunks[nextIdx].chSeq, (tNow - chunks[nextIdx].time), part1Len, chunks[nextIdx].offset, chunks[nextIdx].len,
                          ch_cnt, ch_out, ch_in);
          netQuePut_cid(packet, pLen, socket.clWS_ID);
    } } } }
}

void checkTimers() {
  size_t len = 0;
  if (flag_0125sec) {           // проверка таймеров таймаута
    flag_0125sec = false;
    if (ok.wdTimer)
      if (--ok.wdTimer == 0) {
        if (ok.waiting) {
          switch (bridgeState) {
            case SYS_PRE_UPLD:
              errCode |= ERR_NO_SD_CARD;
              break;
            case SYS_WAIT_M28:
              errCode |= ERR_M28_OK_TOUT;
              break;
            case SYS_WAIT_M23:
              errCode |= ERR_M23_OK_TOUT;
              break;
            case SYS_TRANSFER:
              errCode |= ERR_DATA_OK_TOUT;
              break;
            case SYS_WAIT_M29:
              errCode |= ERR_M29_OK_TOUT;
              break;
            case SYS_WAIT_M30:
              errCode |= ERR_M30_OK_TOUT;
              break;
            case SYS_WAIT_MOVE:
            case SYS_WAIT_TEMP:
              bridgeState = SYS_IDLE;
              errCode = ERR_NO_ERRORS;
            default:
              break;
            }
          if (fListWait) fListTOut = 1;
          switch (gData.pubArea & 0xFFFF0000) {                     // если нужно, завершаем M115
            case 0x02200000:
              gAnswer_idx = anchorIdx = gData.pubArea & 0x0000FFFF; // восстанавливаем индексы
            case 0x01100000:
              gData.pubArea = 0x03300000;                           // ставим флаг "M115 отработано"
              if (cmdMode) cmdMode -= 1;                            // восстановление опроса параметров
              break; }
          }
        ok.skip = 0; ok.waiting = false;
        } // if (--ok.wdTimer == 0)
    if (wd_BIN_Timer)                                   // таймер таймаута получения BIN чанков
      if (--wd_BIN_Timer == 0)
        if (bridgeState == SYS_TRANSFER) errCode |= ERR_BIN_TOUT;
    if (wd_ACT_Timer)                                   // таймер таймаута чтения строки UART
      if (--wd_ACT_Timer == 0) errCode |= ERR_GANSWER_TOUT;
    if (M30_Timer) {                                    // таймер реализации последовательности удаления файла при ошибке
      M20_Timer = 0;                                    // на всякий случай сбрасываем "родственный" таймер сканирования SD
      uint32_t ackFlag = 0;
      if ((M30_Seq >= 2) || (M30_Seq <= 7))             // определяем, какой флаг ожидается на текущем шаге
        ackFlag = ((M30_Seq == 5) || (M30_Seq == 7))? ACK_SS: ACK_OK;
      if (ok.ack_status & ackFlag) M30_Timer = 1;       // форсируем сброс таймера при наличии ожидаемого ответа
      if (--M30_Timer == 0) {
        if (!(ok.ack_status & ackFlag) && (M30_Seq != 1)) { // если ожидаемый ответ не пришел (исключение: M30_Seq == 1)
          netQuePut(NULL, 0, (char*)PSTR("L:!Can't delete! Check the SD.\n"));
          M30_Seq = 0; }                                // сбрасываем последовательность при отсутствии ответа Марлин
         else {
          ok.waiting = true; ok.skip = 0; ok.wdTimer = ok.wdLoad = WAIT_OK_TIMEOUT; }
        ok.ack_status &= ~(ACK_SS | ACK_OK); len = 0;
        switch (M30_Seq) {                              
          case 7:                                       // здесь начинаем при useBFT==true, имеем (ok.ack_status & ACK_SS)
            sendBFT(0, BFT_CLOSE);                      // выходим из режима BFT: SYNC -> CLOSE -> SYNC -> EXIT
            break;
          case 6:                                       // здесь имеем (ok.ack_status & ACK_OK)
            sendBFT(0, BFT_SYNC);
            break;
          case 5:                                       // здесь имеем (ok.ack_status & ACK_SS)
            sendBFT(0, BFT_EXIT); ok.skip = 1;
            break;
          case 4:                                       // здесь начинаем при useBFT==false, имеем (ok.ack_status & ACK_OK)
            if (sdIsOK) {
              len = snprintf_P((char*)packet, 8, PSTR("M22\n"));  // SD release
              break; }
            M30_Seq = 3;                                // если release было сделано автоматически ранее - проходим дальше
          case 3:                                       // здесь имеем (ok.ack_status & ACK_OK)
            len = snprintf_P((char*)packet, 8, PSTR("M21\n"));    // SD mount
          case 2:                                       // здесь имеем (ok.ack_status & ACK_OK), даем + ~0.5сек на монтирование SD
            break;
          case 1: {                                     // здесь имеем ((ok.ack_status & (ACK_SS | ACK_OK)) == 0)
            if (ok.ack_status & ACK_OPEN_FAIL) {        // если файл не был открыт - удалять нечего
              len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:Just try again..\n"));
              ok.ack_status = 0; }                      // выйдем из switch() после netQuePut() через default
             else {
              // получаем сохраненный полный путь, проверяем целостность записи и наличие терминатора \0
              char* fullFilePath = &gAnswer_buf[(GANSWER_BUF_SIZE - (NUM_CHUNKS * CHUNK_SIZE)) - (2 * PACKET_BUF_SIZE)];
              uint8_t pLen = *((uint8_t*)fullFilePath++); // длина строки записана в начале по смещению 0 и по макс. смещению в конце
              if ((pLen == *((uint8_t*)(fullFilePath + (2 * MAX_FNAME_LEN) + 1))) && (!(*((uint8_t*)(fullFilePath + pLen))))) {
                len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("M30 \"%s\"\n"), fullFilePath);  // формируем команду удаления
                bridgeState = SYS_WAIT_M30; break; }    // выходим из switch() штатно
              len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:!Name was lost. Can't remove..\n")); }// при ошибке проходим к сбросу последовательности
            netQuePut(packet, len); }
          default:
            M30_Seq = 0;
            break;
          }
        if (M30_Seq) {
          M30_Timer = (--M30_Seq != 0)? 4: 0;
          if (len) { uart_w((char*)packet, len, true); useBFT = false; } } // пишем в UART
      } } // if (M30_Timer) 
    if (M20_Timer)                                      // таймер запуска получения листинга SD с поиском
      if (--M20_Timer == 0) {
        // готовим поиск только по socket.fName
        gAnswer_idx = anchorIdx = 0;                    // сбрасываем флаг валидности буфера имен
        // искать имя(+размер) в раб. папке , вывод списка вариантов, установить состояние системы
        bridgeState = SYS_IDLE; ok.setState = SYS_WAIT_M20; // перечитать список при неудаче поиска
        uint32_t chkSize = !!((socket.fSize)? ok.ack_status & ACK_QRY_SIZE: 0); ok.ack_status &= ~ACK_QRY_SIZE;
        fileID((*socket.fName != '\0'), chkSize); }
    if (fListTOut)                                      // таймер таймаута получения листинга SD
      if (--fListTOut == 0) {
        errCode |= (ERR_FLIST_TOUT & fListScanned); fListScanned = FLIST_SCANNED; // блокируем повторение сообщений
        fListMode = false; fListWait = false; sdIsOK = false;
        gAnswer_idx = anchorIdx = 0;                    // сбрасываем флаг валидности буфера имен
        if (sdFinal) sdCheck = true; }                  // разрешаем heartbeat даже при отсутствии SD
    if (txTimer)                                        // таймер задержки выдачи команды в UART
      if (--txTimer == 0)                               // разрешаем аппаратное прерывание, включаем передачу UART
        SET_PERI_REG_MASK(UART_INT_ENA(UART0), UART_TXFIFO_EMPTY_INT_ENA);
    } // if (flag_0125sec)
  if (flag_1sec_1) {
    flag_1sec_1 = false;
    static int statR_timer = STAT_R_TIME;
    stat(MTR_RAM_TOTAL, ESP.getFreeHeap());
    stat(MTR_RAM_BLOCK, ESP.getMaxFreeBlockSize());
    if (statR_timer-- == 0) {
      statR_timer = STAT_R_TIME;
      stat_reset(); }
    if (otaTimer) {
      if (--otaTimer == 0)
        doReboot();
       else if (!(otaTimer % 10)) {
        len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("M117 OTA awaiting %d\n"), otaTimer);
        uartWxStop = false; WITH_FLAG_OFF(UART_SEND, (uart_w((char*)packet, len))); uartWxStop = true;
      } }
    uint32_t tNow = millis();
    // --- АВТОМАТИЧЕСКИЙ СБРОС RTC ПРИ ПОЛНОМ ОТСУТСТВИИ КЛИЕНТОВ ---
    // wsMap == 0 означает, что к веб-сокетам платы не подключен вообще никто
    // 16 минут = 960 000 миллисекунд, дольше 3 циклов синхронизации
    if ((wsMap == MAP_NOONE) && (timeLastSync)) {
      if ((tNow - timeLastSync) > 960000) {
        timeLastSync = 0; syncReported = false; // Останавливаем проверку до следующей синхронизации
        timeval tv = { .tv_sec = 0, .tv_usec = 0 };
        settimeofday(&tv, NULL);                // Сбрасываем RTC в 1970 год
      } }
    if (wifi_timer) wifi_timer -= (wifi_timer > 0)? 1: -1;
    // Переменная wifi_timer > 0 гарантирует, что мы физически подключены к Wi-Fi
    if ((wifi_timer > 0) && (wifi_timer <= 5))
      wifiState = WIFI_STATE_STA;               // разрешаем mqtt ~через 5 сек после WiFi коннекта
    if (wifi_timer == 8) { uartOn(); lastUARTTime = tNow; } // включаем UART через 2 сек после WiFi коннекта
    if ((tNow > 60000) && (wifiState == WIFI_STATE_STA)) sdCheck = true;
    } // if (flag_1sec_1)
}

void checkErrors() {
  size_t len = 0;
  const char* msgPtr = (const char*)pgm_read_ptr(&brStateName[bridgeState]);
  memset(packet, 0, PACKET_BUF_SIZE);
  if (ctrl & SERV_LOG)
    snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:![%S] error(s) :\n"), msgPtr);  // +[bridgeState]
  uint32_t eCode = 1;                                         // макс. 32 ошибки
  uint32_t gID = socket.clGroup_ID & 0xFFFF;                  // gID находится в младшей половине
  if ((errCode & ERR_WS_DISCONNECT) && !(gID)) {              // при дисконнекте активиста gID в младшей половине обнуляется
    gID = socket.clGroup_ID >> 16; socket.clGroup_ID = 0; }   // используем копию его gID, временно созданную в старшей половине
  for (uint16_t errIdx = 0; (eCode) && (eCode <= errCode); errIdx++, eCode <<= 1)
    if (errCode & eCode) {                                    // если битовый флаг (==eCode) установлен
      msgPtr = (const char*)pgm_read_ptr(&msgArray[errIdx]);  // указатель на строку описания ошибки во флеш-памяти
      if (msgPtr) {                                           // Крайне важна проверка на NULL
        len = strlen((char*)packet);
        if ((len + strlen_P(msgPtr)) < (NET_DATA_MAX - ((eCode == ERR_WS_DISCONNECT)? 5: 0))) // для disconnect добавится 5-значный gID
          snprintf_P((char*)(packet + len), NET_DATA_MAX - len, msgPtr, gID);
      } }
  netQuePut(packet, strlen((char*)packet));                   // сообщение об ошибках в лог всем клиентам
  if ((ok.setState == SYS_SD_ERASE) && ((errCode == ERR_WS_DISCONNECT) || (errCode == ERR_WIFI_ERR))) {
    errCode = ERR_NO_ERRORS; return; }                        // для SYS_SD_ERASE игнорируем ERR_WS_DISCONNECT и ERR_WIFI_ERR
  if ((bridgeState == SYS_PRINT) && ((errCode & ~(ERR_WS_DISCONNECT | ERR_PRN_DETECTED)) == ERR_NO_ERRORS)) {
    errCode = ERR_NO_ERRORS; return; }                        // если печать, игнорируем ERR_WS_DISCONNECT и ERR_PRN_DETECTED
  if(errCode & (ERR_MQTT_MEM_ERR | ERR_MEMORY_ERR)) doReboot(true);   // ошибка работы с памятью -> reboot
  otaTimer = 0; wd_ACT_Timer = 0; wd_BIN_Timer = 0; ok.wdTimer = 0; ok.skip = 0; ok.waiting = false; // сброс всех таймеров и флагов ожидания
  BridgeState_t okSetState = ok.setState;
  uint32_t ackStatus = ok.ack_status;
  ok.ack_status = 0; ok.setState = SYS_IDLE;
  if (okSetState == SYS_SD_ERASE) {                 // Если идет удаление файлов
    uart_w((char*)PSTR("M29\n"));                   // на всякий случай закрываем, если создавался PATHKEEP.GCO
    netQuePut(NULL, 0, (char*)PSTR("L:!Error while deleting files.\n"));
    sdFile.selName = false; sdFile.selIdx = 0;      // сбрасываем флаги поиска при получении списка файлов
    gAnswer_idx = anchorIdx = 0; sdFile.pageBegIdx = 0; // сбрасываем параметры буфера имен
    errCast = 3;                                    // установка флага передачи err_sequence
    heartbeat(true); }                              // "срочный" heartbeat
   else if ((bridgeState >= SYS_WAIT_M28) && (bridgeState <= SYS_WAIT_M29)) { // Если шла запись файла — это фатально
    // --- БЕЗОПАСНЫЙ СБРОС ПРИНТЕРА ---
    // блокируем интерфейс активного клиента + дополнительный лог сообщений об ошибках
    // Выходим из режима записи и, если файл был открыт - закрываем и удаляем с SD карты
    if ((bridgeState == SYS_WAIT_M28) && (ackStatus & ACK_OPEN_FAIL)) // Если не получилось даже открыть файл
      ok.ack_status |= ACK_OPEN_FAIL;
    if (useBFT) {                                   // из режима BFT выходим: SYNC->[CLOSE->SYNC->]EXIT
      useBFT = false; sendBFT(0, BFT_SYNC); M30_Seq = ((ok.ack_status)? 5: 7); }  // пропускаем CLOSE, если ACK_OPEN_FAIL
     else {
      uart_w((char*)PSTR("M29\n")); M30_Seq = 4; }  // режим ASCII : закрываем файл, выходим из режима записи
    ok.waiting = true; ok.wdTimer = ok.wdLoad = WAIT_OK_TIMEOUT;
    M30_Timer = 4;                                  // через ~0.5сек - запуск последовательности удаления файла
    if (!(ok.ack_status)) {                         // если нет флага ACK_OPEN_FAIL - сообщаем об операции удаления
      if (ctrl & (UART_LOG | SERV_LOG | TRANS_LOG))
        len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("eCode: 0x%08lX. Check the SD.\n"), errCode);
       else
        len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("Check the SD.\n"));
      netQuePut(packet, len, (char*)PSTR("L:!Upload terminated. Removing...\nL:!")); }
    socket.progress = 0;
    errCast = 3;                                    // установка флага передачи err_sequence
    heartbeat(true); }                              // "срочный" heartbeat
   else if (bridgeState == SYS_PRINT) {             // если идет печать
    len = snprintf_P((char*)packet, NET_DATA_MAX,
                      PSTR("L:!Error: %s.\nL:!eCode: %s. Check printer.\n"), errCode);
    netQuePut(packet, len);
    //socket.progress = 0;
    errCast = 3;                    // установка флага передачи err_sequence
    heartbeat(true); }              // "срочный" heartbeat
   else if (bridgeState == SYS_WAIT_OTA) doReboot(true);  // перезагрузка + сброс активиста
   else if (bridgeState == SYS_OTA) {
    if (!(errCode & ERR_OTA_BAD_FIN))
      Update.end(true);             // 0. Завершаем процесс, проверяем хэш
    StreamString errorString;       // 1. Создаем специальный строковый поток ядра
    Update.printError(errorString); // 2. заносим в errorString текстовое описание ошибки (например, "MD5 Failed")
    len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:![OTA] : %s\n"), errorString.c_str());
    netQuePut(packet, len);
    doReboot(true); }               // перезагрузка + сброс активиста
   else if ((bridgeState == SYS_WAIT_M23) && (errCode & ERR_M23_OK_TOUT)) prnFix(7); // восстанавливаем поллинг параметров
   else if (ctrl & (UART_LOG | SERV_LOG | TRANS_LOG)) {   // при отладке сообщаем код неотработанной ошибки
    len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:!eCode: 0x%04lX.\n"), errCode);
    netQuePut(packet, len);
    }
  if (errCode & (ERR_GANSWER_TOUT | ERR_UART_RX_OVF)) {   // ошибки, связанные с UART - сбрасываем буфер
    fData.txTry = gAnswer_idx = anchorIdx = 0;
    fListMode = false;
    }
  errCode = ERR_NO_ERRORS;
  if (bridgeState != SYS_PRINT) bridgeState = SYS_IDLE;
}

void heartbeat(bool forced) {
  // полный формат сообщения H:sessionID:s-Код:ActiveID:Ctrl:Прогресс:Режим_ASCII_BFT:Имя_файла
  //                                     s-Код = HB_IDLE,HB_UPLOAD,HB_READY,HB_PRINT
  // в момент запуска передачи файла присоединяется запрос на получение 1-го чанка
  // короткий формат         H:sessionID:е-Код:ActiveID:Ctrl
  //                                     е-Код = HB_IDLE,HB_ERROR,HB_WAIT,HB_PRINT
  // HB_ERROR (==HB_UPLOAD) без имени файла -> состояние UX клиентов "ERROR"
  // HB_WAIT  (==HB_READY)  без имени файла -> состояние UX клиентов "WAIT"
  // публикация MQTT (pubState) использует набор s-Код + PUB_ERROR, PUB_WAIT коды 
  flag_1sec_2 = false;
  size_t len = 0;
  bool lastCast = false;
  int hbState = HB_WAIT; pubState = PUB_WAIT;
  if (errCast > 0) {
    errCast--; lastCast = !(errCast);
    hbState = HB_ERROR; pubState = PUB_ERROR;
    }
   else if (ok.setState != SYS_SD_ERASE)
    switch (bridgeState) {
      case SYS_IDLE:
      case SYS_WAIT_OTA:
        pubState = hbState = HB_IDLE;
        break;
      case SYS_PRE_UPLD:
      case SYS_WAIT_M28:
      case SYS_TRANSFER:
      case SYS_WAIT_M29:
      case SYS_OTA:
        pubState = hbState = HB_UPLOAD;
        break;
      case SYS_PRE_PRINT:
        pubState = hbState = HB_READY;
        break;
      case SYS_PRINT:
        pubState = hbState = HB_PRINT;
        break;
      default:            // SYS_SD_ERASE, SYS_OTA, SYS_WAIT_xx ..
        lastCast = true;  // hbState = HB_WAIT
        break;
      }
   else
    lastCast = true;      // hbState = HB_WAIT
  uint32_t hb_Ctrl = ((ctrl & CLIENT_LOG) >> 4);      // бит разрешения писать в лог у клиента
  uint32_t hbTime = millis(), pgs = 0;
  if ((errCast > 0) || lastCast ||
      (((socket.fSize == 0) || (socket.fName[0] == '\0')) && (hbState != HB_PRINT))) {        // формируем короткий формат HeartBeat
    len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("H:%u:%d:%d:%d\n"),
                                  sessionID, hbState, (socket.clGroup_ID & 0xFFFF), hb_Ctrl);
    if (hbState != HB_PRINT) socket.progress = 0; }
   else {                                                                                     // формируем полный формат HeartBeat
    pgs  = ((socket.fSize)? ((uint32_t)((uint64_t)(socket.progress * 100) / socket.fSize)): 0);
    uint32_t f_bin = !!(((bridgeState >= SYS_PRE_UPLD) && (bridgeState <= SYS_TRANSFER)) || (bridgeState == SYS_OTA));
    hb_Ctrl |= (f_bin << 1);                          // бит разрешения передавать бинарные чанки и контроля BIN-watchdog у клиента
    len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("H:%u:%d:%d:%d:%d:%d:%s\n"),
                                                  (sessionID + ((*socket.fName)? 0: 1)),      // для обновления UX, когда SYS_PRINT получает имя из notification
                                                  hbState, (socket.clGroup_ID & 0xFFFF), hb_Ctrl, ((useBFT)? 1: 0), pgs, socket.fName);
    static uint32_t ctrlMem = 0;
    if (!forced && ((ctrl ^ ctrlMem) & CLIENT_LOG)) {                                         // бит CLIENT_LOG изменился
      if (ctrl & CLIENT_LOG)                                                                  // бит CLIENT_LOG установлен (лог включен)
        len += snprintf_P((char*)(packet + len), NET_DATA_MAX - len, PSTR("L:,Uptime %u msec\n"), hbTime);        // при отладке даем ориентир синхронизации
      ctrlMem = ctrl; }
    if (((bridgeState == SYS_PRE_UPLD) || (bridgeState == SYS_OTA)) && forced && !(chGotSeq)) {                   // момент старта файловой операции
      len += snprintf_P((char*)(packet + len), NET_DATA_MAX - len, PSTR("N:%d\n"), (socket.clGroup_ID & 0xFFFF)); // присоединяем запрос самого первого чанка
      ch_req += 1; chReqSeq += 1; }                                                           // тек.кол-во запросов, последний запрошенный номер
    }
  if (forced)
    netQuePut_cid(packet, -((int32_t)len), socket.clWS_ID);
   else {
    netQuePut(packet, len);
    if (((bridgeState >= SYS_WAIT_M28) && (bridgeState <= SYS_WAIT_M29)) || (bridgeState == SYS_OTA)) {
      updIntValue(ID_SD_PROGRESS, hbTime, socket.progress, socket.fSize);                     // вывод прогресса для публикации MQTT
      if (bridgeState == SYS_OTA) {
        len = snprintf_P((char*)packet, sizeof(packet), PSTR("M117 OTA: %d%%\n"), pgs);
        uartWxStop = false;
        WITH_FLAG_OFF(UART_SEND, (uart_w((char*)packet, len)));
        uartWxStop = true; } } }                                                              // вывод прогресса на LCD принтера
}

void loop() {
  static bool wifi_init = true;
  uint32_t tNow = 0;

  wifiConnect(wifi_init);
  if (wifi_timer > 0)
    wifi_init = false;

  BridgeState_t lState;
  switch (bridgeState) {
    case SYS_OTA:
      webSocket.loop();
      writeFlash();
    case SYS_WAIT_OTA:
    case SYS_OTA_END:
      break;
    case SYS_SD_ERASE:
      lState = bridgeState;
      sd_erase();
      if ((lState != bridgeState) && (ctrl & SERV_LOG)) {
        size_t len = snprintf_P((char*)packet, NET_DATA_MAX,
                              PSTR("L:sd_erase: %S -> %S\n"),
                              (const char*)pgm_read_ptr(&brStateName[lState]),
                              (const char*)pgm_read_ptr(&brStateName[bridgeState]));
        netQuePut_cid(packet, len, socket.clWS_ID); }
    default:
      lState = bridgeState;
      getMarlin();        // UART опрашивается всегда
      tNow = millis();
      if (getMarlinRXTime) {
        stat(MTR_RX_LOOP_TIME, (tNow - getMarlinRXTime)); lastUARTTime = tNow; }
      if ((lState != bridgeState) && (ctrl & SERV_LOG)) {
        size_t len = snprintf_P((char*)packet, NET_DATA_MAX,
                              PSTR("L:getMarlin: %S -> %S\n"),
                              (const char*)pgm_read_ptr(&brStateName[lState]),
                              (const char*)pgm_read_ptr(&brStateName[bridgeState]));
        netQuePut_cid(packet, len, socket.clWS_ID); }
      if (bridgeState == SYS_TRANSFER) putMarlin();
      break;
    }
  netQueSend(); 
  httpServer.handleClient();
  if (!fListMode) {
    lState = bridgeState;
    uint32_t wsLoopT = millis();
    webSocket.loop();
    stat(MTR_WS_LOOP_TIME, (millis() - wsLoopT));
    if ((lState != bridgeState) && (ctrl & SERV_LOG)) {
      size_t len = snprintf_P((char*)packet, NET_DATA_MAX,
                            PSTR("L:wsMessage: %S -> %S\n"),
                            (const char*)pgm_read_ptr(&brStateName[lState]),
                            (const char*)pgm_read_ptr(&brStateName[bridgeState]));
      netQuePut_cid(packet, len, socket.clWS_ID); }}
  switch (wifiState) {
    case WIFI_STATE_STA:
      mqttLoop();    // MQTT операции начинаем через ~5 сек после подключения к WiFi
    case WIFI_STATE_DEF: {
      if (flag_0125sec || flag_1sec_1) checkTimers();                 // обслуживание таймеров
      if ((errCode & ~ERR_PFT_BUSY) != ERR_NO_ERRORS) checkErrors();  // проверка состояния errCode
      if ((bridgeState == SYS_IDLE) || (bridgeState == SYS_PRE_PRINT)) {
        // при переключении в эти режимы ставим флаг сброса gData.progress в webSocketEvent()
        if (pgsReset == PROGRESS_ZERO) pgsReset = PROGRESS_WAIT;
        // проверяем доступность SD карты и наличие списка файлов в памяти
        if (sdIsOK | uartWxStop) sdTryNum = 2;
        if (!(anchorIdx) && !uartWxStop)
          if ((sdTryNum) && ((tNow - lastUARTTime) > 1000) && !(M30_Timer | M20_Timer | fListWait)) {
            fListGet(65535);                                          // пытаемся прочитать SD карту
            sdTryNum -= 1; lastUARTTime = tNow; sdFinal = true;       // разрешаем поднять sdCheck при любом результате чтения SD
        }   }
       else { pgsReset = PROGRESS_ZERO; lastUARTTime = tNow; sdTryNum = 2; }
      if (flag_1sec_2 && !fListMode && sdCheck) heartbeat();
      break; }
    case WIFI_STATE_AP:
      break;
    }
}
