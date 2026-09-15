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

#pragma once

uint32_t parseRC;
//int32_t callMark = -1;

#define IS_PROGMEM(ptr) ((uint32_t)(ptr) >= 0x40000000)

#define WITH_FLAG_OFF(flag, code) ({  \
    uint32_t ctrlMem = ctrl;          \
    ctrl &= ~(flag);                  \
    __typeof__(code) result = (code); \
    ctrl = ctrlMem;                   \
    result;                           \
})

#define PACKET_BUF_SIZE 256
#define SVC_BUF_SIZE     64

/*=== инициализация таймеров ===*/
#define WAIT_OK_SEC     5
#define WAIT_BIN_SEC   10
#define WAIT_ACT_SEC   10
#define WAIT_FLIST_SEC  2

#define TICKS_PER_SEC   8

#define WAIT_OK_TIMEOUT  (WAIT_OK_SEC  * TICKS_PER_SEC)
#define WAIT_BIN_TIMEOUT (WAIT_BIN_SEC * TICKS_PER_SEC)
#define WAIT_ACT_TIMEOUT (WAIT_ACT_SEC * TICKS_PER_SEC)
#define WAIT_QRY_TIMEOUT TICKS_PER_SEC
#define WAIT_FLIST_TIMEOUT (WAIT_FLIST_SEC * TICKS_PER_SEC)

#define JS_BIN_TIMEOUT   ((WAIT_BIN_SEC * 1000) + 2000) //msec для согласования в JS

/*=== буферы WiFi файловых данных ===*/
#define NUM_CHUNKS 4      // "кольцевой" буфер для 2 чанков
#define CHUNK_SIZE 1230   // Размер чанка (согласованно с JS!)

#define MAX_FNAME_LEN 112 // максимальная длина имени файла

#define CID_ALL       0xF // для осуществления broadcast
#define CID_NOONE (~CID_ALL) // безадресный
#define MAP_NOONE     0   // карта пуста, отправлять некому

/*=== типы WS сообщений ===*/
/*  typedef enum {  // from library WebSockets
      WStype_ERROR,
      WStype_DISCONNECTED,
      WStype_CONNECTED,
      WStype_TEXT,
      WStype_BIN,
      WStype_FRAGMENT_TEXT_START,
      WStype_FRAGMENT_BIN_START,
      WStype_FRAGMENT,
      WStype_FRAGMENT_FIN,
      WStype_PING,
      WStype_PONG,
  } WStype_t;*/

#define WSTYPE_ILLEGAL 11

const char ws_error[] PROGMEM = "WStype_ERROR";
const char ws_discn[] PROGMEM = "WStype_DISCONNECTED";
const char ws_conn[]  PROGMEM = "WStype_CONNECTED";
const char ws_text[]  PROGMEM = "WStype_TEXT";
const char ws_bin[]   PROGMEM = "WStype_BIN";
const char ws_fgtxt[] PROGMEM = "WStype_FRAGMENT_TEXT_START";
const char ws_fgbin[] PROGMEM = "WStype_FRAGMENT_BIN_START";
const char ws_fg[]    PROGMEM = "WStype_FRAGMENT";
const char ws_fgfin[] PROGMEM = "WStype_FRAGMENT_FIN";
const char ws_ping[]  PROGMEM = "WStype_PING";
const char ws_pong[]  PROGMEM = "WStype_PONG";
const char ws_bad[]   PROGMEM = "WStype illegal";

static const char* wsTypeName[] PROGMEM __attribute__((aligned(4))) = {
  ws_error, ws_discn, ws_conn,  ws_text,  ws_bin,   ws_fgtxt,
  ws_fgbin, ws_fg,    ws_fgfin, ws_ping,  ws_pong,  ws_bad };
/*^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

/*=== сетевой режим системы ===*/
enum brWiFiState_t {
  WIFI_STATE_DEF = 0, WIFI_STATE_STA, WIFI_STATE_AP };
brWiFiState_t wifiState = WIFI_STATE_DEF;

/*=== для подсчета WS подключений===*/
static const uint32_t wsNumber[16] PROGMEM __attribute__((aligned(4))) = {
  0, 1, 1, 2,  1, 2, 2, 3,  1, 2, 2, 3,  2, 3, 3, 4 };

/*=== состояния системы ===*/
enum BridgeState_t {
  SYS_IDLE = 0, SYS_PRE_UPLD, SYS_WAIT_M28, SYS_TRANSFER, SYS_WAIT_M29, SYS_PRE_PRINT, SYS_PRINT,
  SYS_SD_ERASE,
  SYS_WAIT_M20, SYS_WAIT_M21, SYS_WAIT_M22, SYS_WAIT_M23, SYS_WAIT_M24, SYS_WAIT_M27,
  SYS_WAIT_M30, SYS_WAIT_M32,
  SYS_WAIT_OTA, SYS_OTA,  SYS_OTA_END,
  SYS_WAIT_MOVE, SYS_WAIT_TEMP };

const char s_idle[]   PROGMEM = "SYS_IDLE";
const char s_p_upld[] PROGMEM = "SYS_PRE_UPLD";
const char s_w_m28[]  PROGMEM = "SYS_WAIT_M28";
const char s_trans[]  PROGMEM = "SYS_TRANSFER";
const char s_w_m29[]  PROGMEM = "SYS_WAIT_M29";
const char s_p_prn[]  PROGMEM = "SYS_PRE_PRINT";
const char s_print[]  PROGMEM = "SYS_PRINT";
const char s_sd_erase[] PROGMEM = "SYS_SD_ERASE";
const char s_w_m20[]  PROGMEM = "SYS_WAIT_M20";
const char s_w_m21[]  PROGMEM = "SYS_WAIT_M21";
const char s_w_m22[]  PROGMEM = "SYS_WAIT_M22";
const char s_w_m23[]  PROGMEM = "SYS_WAIT_M23";
const char s_w_m24[]  PROGMEM = "SYS_WAIT_M24";
const char s_w_m27[]  PROGMEM = "SYS_WAIT_M27";
const char s_w_m30[]  PROGMEM = "SYS_WAIT_M30";
const char s_w_m32[]  PROGMEM = "SYS_WAIT_M32";
const char s_w_ota[]  PROGMEM = "SYS_WAIT_OTA";
const char s_ota[]    PROGMEM = "SYS_OTA";
const char s_ota_e[]  PROGMEM = "SYS_OTA_END";
const char s_w_move[] PROGMEM = "SYS_WAIT_MOVE";
const char s_w_temp[] PROGMEM = "SYS_WAIT_TEMP";

static const char* brStateName[] PROGMEM __attribute__((aligned(4))) = {
  s_idle,   s_p_upld, s_w_m28,  s_trans,  s_w_m29,  s_p_prn,  s_print,
  s_sd_erase,
  s_w_m20,  s_w_m21,  s_w_m22,  s_w_m23,  s_w_m24,  s_w_m27,
  s_w_m30,  s_w_m32,
  s_w_ota,  s_ota,    s_ota_e,
  s_w_move, s_w_temp };
/*^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

/*=== пользовательская конфигурация ===*/
#define STR_LEN_SHORT 32
#define CONFIG_MAGIC  0x4D425632 // Сигнатура "MBV2"

struct Config {
  uint32_t magic;
  char wifi_ssid[STR_LEN_SHORT * 2];
  char wifi_pass[STR_LEN_SHORT * 2];
  char   mqtt_ip[STR_LEN_SHORT];
  char mqtt_user[STR_LEN_SHORT];
  char mqtt_pass[STR_LEN_SHORT];
  char ha_prefix[STR_LEN_SHORT];
  char device_id[STR_LEN_SHORT];
  uint16_t mqtt_port;
  uint16_t crc;
} cfg;
/*^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

/*=== коды состояния системы для HeartBeat сообщений ===*/
#define HB_IDLE           0   // свободно / простаивает
#define HB_UPLOAD         1   // идет загрузка файла
#define HB_READY          2   // файл загружен и готов к печати
#define HB_PRINT          3   // идет печать [файла]
#define HB_ERROR          1   // код состояния, который передается в сообщении HeartBeat
                              // без имени файла для фиксации всеми клиентами состояния ошибки
#define HB_WAIT           2   // код состояния, который передается в сообщении HeartBeat
                              // без имени файла для фиксации всеми клиентами состояния ожидания
#define PUB_ERROR         4   // индекс имени состояния MQTT сенсора IoT_7_U, для HB_ERROR
#define PUB_WAIT          5   // индекс имени состояния MQTT сенсора IoT_7_U, для HB_WAIT
#define PUB_OTA           6   // индекс имени состояния MQTT сенсора IoT_7_U, для HB_UPLOAD в процессе OTA

#define PUB_UNKNOWN (PUB_OTA + 1) // общий код состояния MQTT сенсора IoT_7_U вне границ [HB_IDLE..PUB_AP]
/*^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

//#define POLL_TIME        40   // * 0.125сек. = 5 сек интервал опроса состояния принтера

/*=== битовые флаги ошибок ===*/
#define ERR_NO_ERRORS     0

#define ERR_WS_DISCONNECT 1
#define ERR_UPLOAD_ERR    2
#define ERR_M30_OK_TOUT   4
#define ERR_GANSWER_OVF   8

#define ERR_GANSWER_TOUT  0x10
#define ERR_ACK_OFFRUN    0x20
#define ERR_NAK_OFFRUN    0x40
#define ERR_UART_RX_OVF   0x80

#define ERR_M28_OK_TOUT   0x100
#define ERR_DATA_OK_TOUT  0x200
#define ERR_M29_OK_TOUT   0x400
#define ERR_BIN_TOUT      0x800

#define ERR_BIG_DATA      0x1000
#define ERR_END_ERR       0x2000
#define ERR_RESEND_ERR    0x4000
#define ERR_PFT_BUSY      0x8000

#define ERR_WIFI_ERR      0x10000
#define ERR_NET_BUSY      0x20000
#define ERR_NET_QUE_ERR   0x40000
#define ERR_NET_QUE1_ERR  0x80000

#define ERR_NET_FREEZE    0x100000
#define ERR_CH_REQ_ERR    0x200000
#define ERR_NO_SD_CARD    0x400000
#define ERR_FLIST_TOUT    0x800000

#define ERR_SD_SEL_FAILED 0x1000000
#define ERR_OTA_BAD_INIT  0x2000000
#define ERR_OTA_FLASH_ERR 0x4000000
#define ERR_OTA_BAD_FIN   0x8000000

#define ERR_MQTT_MEM_ERR  0x10000000
#define ERR_PRN_DETECTED  0x20000000
#define ERR_M23_OK_TOUT   0x40000000
#define ERR_MEMORY_ERR    0x80000000

// сообщения об ошибках
const char disconnect_msg[]   PROGMEM = "L:!-Client %d disconnected\n"; // 1
const char upload_error_msg[] PROGMEM = "L:!-Transfer data loss.\n";    // 2
const char m30_ok_tout_msg[]  PROGMEM = "L:!-M30 OK timeout.\n";        // 4
const char ganswer_ovf_msg[]  PROGMEM = "L:!-G_ANSWER overflow.\n";     // 8
const char act_tout_msg[]     PROGMEM = "L:!-Marlin freeze.\n";         // 16
const char ack_offrun_msg[]   PROGMEM = "L:!-BFT_ACK offrun.\n";        // 32
const char nak_offrun_msg[]   PROGMEM = "L:!-BFT_NAK offrun.\n";        // 64
const char uart_rx_ovf_msg[]  PROGMEM = "L:!-UART Rx overflow\n";       // 128
const char m28_ok_tout_msg[]  PROGMEM = "L:!-M28 OK timeout.\n";        // 256
const char data_ok_tout_msg[] PROGMEM = "L:!-Data OK timeout.\n";       // 512
const char m29_ok_tout_msg[]  PROGMEM = "L:!-M29 OK timeout.\n";        // 1024
const char bin_tout_msg[]     PROGMEM = "L:!-WS BIN timeout.\n";        // 2048
const char data_too_big_msg[] PROGMEM = "L:!-G-code too long.\n";       // 4096
const char bad_progress_msg[] PROGMEM = "L:!-EOF out of progress.\n";   // 8192
const char data_resend_msg[]  PROGMEM = "L:!-Resend error.\n";          // 16384
//const char resend_fix_msg[]   PROGMEM = "L:!Resend fixed.\n";           // 32768
const char net_busy_msg[]     PROGMEM = "L:!-Net is busy.\n";           // 65536
const char net_que_msg[]      PROGMEM = "L:!-Net send bad header.\n";   // 65536
const char net_que1_msg[]     PROGMEM = "L:!-Net send queue crush.\n";  // 65536
const char net_que2_msg[]     PROGMEM = "L:!-Net clean queue crush.\n"; // 65536
const char net_freeze_msg[]   PROGMEM = "L:!-Net freeze.\n";            // 65536
const char bad_ch_num_msg[]   PROGMEM = "L:!-Chunk num error.\n";       // 65536
const char no_sd_card_msg[]   PROGMEM = "L:!-No SD card.\n";            // 65536
const char flist_tout_msg[]   PROGMEM = "L:!-SD fList timeout.\n";        // 
const char file_open_err[]    PROGMEM = "L:!-Can't open file.\n";        // 
const char ota_bad_init_msg[] PROGMEM = "L:!-OTA init error.\n";        // 65536
const char ota_flsh_err_msg[] PROGMEM = "L:!-OTA flash error.\n";       // 65536
const char ota_bad_fin_msg[]  PROGMEM = "L:!-OTA finalize error.\n";    // 65536
const char mqtt_bad_cfg_msg[] PROGMEM = "L:!-MQTT config error.\n";     // 65536
const char prn_detected_msg[] PROGMEM = "L:!-Print process detected.\n"; // 65536
const char m23_ok_tout_msg[]  PROGMEM = "L:!-M23 OK timeout.\n";        // 
const char memory_err_msg[]   PROGMEM = "L:!-Memory corrupted.\n";        // 

static const char* msgArray[32] PROGMEM __attribute__((aligned(4))) = {
  disconnect_msg,   upload_error_msg, m30_ok_tout_msg,  ganswer_ovf_msg,
  act_tout_msg,     ack_offrun_msg,   nak_offrun_msg,   uart_rx_ovf_msg,
  m28_ok_tout_msg,  data_ok_tout_msg, m29_ok_tout_msg,  bin_tout_msg,
  data_too_big_msg, bad_progress_msg, data_resend_msg,  NULL,
  net_busy_msg,     net_que_msg,      net_que1_msg,     net_que2_msg,
  net_freeze_msg,   bad_ch_num_msg,
  no_sd_card_msg,   flist_tout_msg,   file_open_err,
  ota_bad_init_msg, ota_flsh_err_msg, ota_bad_fin_msg,
  mqtt_bad_cfg_msg, prn_detected_msg, m23_ok_tout_msg,  memory_err_msg
  };
/*^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

#define FLIST_SCANNED     0           // блокирует сообщение об ошибке таймаута чтения SD
#define FLIST_NOT_SCANNED 0xFFFFFFFF  // разрешает сообщить об ошибке таймаута чтения SD

/*=== сообщения протокола "клиент-сервер" ===*/
enum WSMessage_t {MSG_FILE = 0, MSG_START, MSG_END, MSG_PRINT, MSG_TIME, MSG_SCREAM, MSG_GCODE, MSG_ILLEGAL} ;

// структура шаблона сообщения
struct BridgeMsg_t {
  const char* idText;   // идентификатор, часть строки в её начале
  size_t idLen;         // длина идентификатора
  uint32_t prmLen;      // допустимое количество цифр в числовом параметре
};

// допустимые шаблоны начала строк WSMessage
const char bmFile[]  PROGMEM __attribute__((aligned(4))) = "F:";  // MSG_FILE
const char bmStart[] PROGMEM __attribute__((aligned(4))) = "S:";  // MSG_START
const char bmEnd[]   PROGMEM __attribute__((aligned(4))) = "E:";  // MSG_END
const char bmPrint[] PROGMEM __attribute__((aligned(4))) = "P:";  // MSG_PRINT
const char bmTime[]  PROGMEM __attribute__((aligned(4))) = "T:";  // MSG_TIME
const char bmL[]     PROGMEM __attribute__((aligned(4))) = "L:";  // MSG_SCREAM
const char bmG[]     PROGMEM __attribute__((aligned(4))) = "G";   // MSG_GCODE
const char bmM[]     PROGMEM __attribute__((aligned(4))) = "M";   // MSG_GCODE
const char bmT[]     PROGMEM __attribute__((aligned(4))) = "T";   // MSG_GCODE
const char bmEmpty[] PROGMEM __attribute__((aligned(4))) = "";

static const BridgeMsg_t bridgeMsg[] PROGMEM __attribute__((aligned(4))) = {
  {bmFile, 2, 5}, {bmStart, 2, 1},  {bmEnd, 2, 9},  {bmPrint, 2, 5}, 
  {bmTime, 2, 10},   {bmL, 2, 5},    {bmG, 1, 4},   {bmM, 1, 4},    {bmT, 1, 2},
  {bmEmpty, 0, 0}};
/*^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

/*=== флаги и переменные для обработки ответа ОК ===*/
#define ACK_OPEN_FAIL 1
#define ACK_WR_START  2
#define ACK_WR_DONE   8
#define ACK_DEL_FAIL  0x10
#define ACK_DELETED   0x20
#define ACK_SELECTED  0x40
#define ACK_OK        0x100
#define ACK_SS        0x200
#define ACK_QRY_SIZE  0x1000

struct oK_t {
  int wdTimer, wdLoad, skip;
  uint32_t ack_status;
  uint32_t time;  // для статистики MTR_OK_WAIT_TIME (время (мсек) ожидания подтверждения от Марлин)
  bool doLog;
  bool waiting;
  BridgeState_t setState;
};
oK_t ok = {0, 0, 0, 0, 0, false, false, SYS_IDLE};
/*^^^^^^^^^^^^^^^^^^^
struct answerStat_t {
  uint32_t time;  // для статистики MTR  _OK_WAIT_LOOPS (число циклов loop() ожидания подтверждения от Марлин)
  bool got;
};
answerStat_t answer = {0, false};
^^^^^^^^^^^^^^^^^^^^^^^^*/
uint32_t getMarlinRXTime = 0; // для статистики MTR_RX_LOOP_TIME

/*=== флаги и переменные для режима SYS_TRANSFER ===*/
enum xorState_t {CS_SKIP = 0, CS_COUNT, CS_FOUND, CS_SENT};

struct fData_t {
  uint32_t chunkSym;
  uint32_t chunkEOL;
  uint32_t txSeq;
  uint32_t txTry;
  uint32_t txTotal;
  uint32_t totalTry;
  uint32_t trySeries;
  xorState_t xorState;
  uint32_t payLen;
  bool waitEOL;
  uint8_t xorCS;
};
fData_t fData = {0, 0, 0, 0, 0, 0, 0, CS_COUNT, 0, false, 0};

#define BFT_TOKEN_LOW   0xAD  // младший байт метки начала BFT-пакета
#define BFT_TOKEN_HIGH  0xB5  // старший байт метки начала BFT-пакета
// мета-коды команды назначения BFT-пакета
// управление режимом работы: 0x00 | <0xN>
#define BFT_SYNC   0x01  // 0x01 = Сбрасывает состояние парсера и заставляет Marlin ответить
                              // строкой ss<sync>,<buffer_size>,<version>. Если принтер молчит — шли это.
#define BFT_EXIT   0x02  // 0x02 = закрытие всего бинарного канала, возврат принтера в текстовый ASCII-режим.
// файловые операции :        0x10 | <0xN>
#define BFT_QUERY  0x10  // 0x00 узнать версию протокола и тип сжатия (PFT:version...).
#define BFT_OPEN   0x11  // 0x01 открыть
#define BFT_CLOSE  0x12  // 0x02 закрыть
#define BFT_WRITE  0x13  // 0x03 писать
#define BFT_ABORT  0x14  // 0x04 прервать+удалить

#define BFT_SIZE  246  // размер payload в составе BFT-пакета (без служебных байтов)
/*^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/

/*=== ошибки при обработке сообщений протокола "клиент-сервер"*/
// коды ошибок
#define ANN_ID_BIG_MSG    1
#define ANN_ID_BAD_MSG    2
#define ANN_ID_GID_ZERO   4
#define ANN_ID_WSID_NEQ   8
#define ANN_ID_IDLE       0x10
#define ANN_ID_FSIZE_ZERO 0x20
#define ANN_ID_NO_SDCARD  0x40
#define ANN_ID_NO_UPLOAD  0x80
#define ANN_ID_GID_NEQ    0x100
#define ANN_ID_SYM_BAD    0x200
#define ANN_ID_CHUNKS_OVF 0x400
#define ANN_ID_GCODE_BIG  0x800
#define ANN_ID_GCODE_BUSY 0x1000

enum annEvent_t {
  ANN_FILE_ILLEGAL = 0, ANN_FNAME_ILLEGAL,  ANN_FILE_EMPTY,     ANN_ROLE_TIMEOUT,
  ANN_ROLE_APPROVED,    ANN_ROLE_TERMINATED,   ANN_START_ILLEGAL,  ANN_END_ILLEGAL,
  ANN_PRNREQ_ILLEGAL,   ANN_NO_PRN_NAME,    ANN_BAD_PRN_NAME,   ANN_BAD_PRN_SIZE,
  ANN_SCREAM_ILLEGAL,   ANN_OK_ILLEGAL,     ANN_GCODE_ILLEGAL,  ANN_GCODE_BUSY,
  ANN_MSG_PARSE_ERROR,  ANN_MSG_ILLEGAL,    ANN_BIN_ILLEGAL };

  // формат печати %0*lX означает:
  // 0 - дополнять нулями
  // * - ширина берется из аргумента width
  // lX - long 16-ричный в верхнем регистре
  //int printed = snprintf_P((char*)dst, 12, PSTR("0x%0*lX"), width, *number);
const char ann_file_bad[]   PROGMEM = "L:![%S]Illegal FILE access request\n";
const char ann_fname_bad[]  PROGMEM = "L:![%S]Offered fName is empty or too long\n";
const char ann_file_empty[] PROGMEM = "L:![%S]Offered file is empty\n";
const char ann_role_tout[]  PROGMEM = "L:![%S]Concurrent announce ignored\n";

const char ann_role_apv[]   PROGMEM = "L:,Assigned role %d\n";
const char ann_role_trm[]   PROGMEM = "L:,Terminated role %d\n";
const char ann_start_bad[]  PROGMEM = "L:![%S]Illegal START request. ID 0x%04lX\n";
const char ann_end_bad[]    PROGMEM = "L:![%S]Illegal END upload request. ID 0x%04lX\n";

const char ann_prnreq_bad[] PROGMEM = "L:![%S]Illegal PRINT request\n";
const char ann_prn_noname[] PROGMEM = "L:![%S]Print File name unknown\n";
const char ann_prn_x_name[] PROGMEM = "L:![%S]Print File name illegal\n";
const char ann_prn_x_size[] PROGMEM = "L:![%S]Print File size illegal\n";

const char ann_scream_bad[] PROGMEM = "L:![%S]Illegal client scream\n";
const char ann_ok_bad[]     PROGMEM = "L:![%S]Illegal OK message. ID 0x%04lX\n";
const char ann_gcode_bad[]  PROGMEM = "L:![%S]Illegal GCODE. ID 0x%04lX\n";
const char ann_gcode_busy[] PROGMEM = "L:![%S]Concurrent GCODE ignored\n";

const char ann_parse_err[]  PROGMEM = "L:![%S]Message parse error\n";
const char ann_msg_bad[]    PROGMEM = "L:![%S]Illegal WS message. ID 0x%04lX\n";
const char ann_bin_bad[]    PROGMEM = "L:![%S]Illegal BIN message. ID 0x%04lX\n";

const char* const ann[] PROGMEM __attribute__((aligned(4))) = {
  ann_file_bad,   ann_fname_bad,  ann_file_empty, ann_role_tout,
  ann_role_apv,   ann_role_trm,   ann_start_bad,  ann_end_bad,
  ann_prnreq_bad, ann_prn_noname, ann_prn_x_name, ann_prn_x_size,
  ann_scream_bad, ann_ok_bad,     ann_gcode_bad,  ann_gcode_busy,
  ann_parse_err,  ann_msg_bad,    ann_bin_bad };
/*^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/
