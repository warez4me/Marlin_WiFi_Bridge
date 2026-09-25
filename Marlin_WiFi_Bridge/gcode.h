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

/* --- шаблоны ответов Marlin --- */
// 1. Определение структуры
struct gAnswer_t {
  const char* idWord; 
  uint32_t idLen;
  };
enum gAnswerID_t {
  ID_OK = 0,
  ID_RESEND,
  ID_RS,
  ID_SS,
  ID_PFT,
  ID_PTF,
  //ID_CAP,

  ID_ERROR,
  ID_WAIT,
  ID_ECHO,
  ID_ACTION,

  ID_SD_BEGIN,
  ID_SD_END,
  ID_OPEN_FAIL,
  ID_WR_START,
  ID_WR_DONE,
  ID_DEL_FAIL,
  ID_DELETED,
  ID_SELECTED,
  ID_PRN_DONE,
  ID_NO_PRN,
  ID_AREA,

  ID_NONE,

  //самые частые данные
  // Температуры
  ID_TEMP_T,
  ID_TEMP_B,
  // Координаты
  ID_POS_X,
  ID_POS_Y,
  ID_POS_Z,
  ID_POS_E,
  ID_SXYZ,

  ID_SD_OK,
  ID_SD_RELEASED,
  ID_SD_NOSD_1,
  ID_SD_NOSD_2,
  ID_NOTIFY,
  ID_BUSY,
  ID_SD_PROGRESS,

  ID_FINDEX,

  ID_SYNC,
  ID_EXIT,
  ID_QUERY,
  ID_OPEN,
  ID_CLOSE,
  ID_ABORT,

  ID_REBOOT,
  ID_OTA,
  ID_CONFIG,

  ID_DTRANS,
  ID_DUART,
  ID_DSERV,
  ID_DCLIENT,
  ID_DMQTT,
  ID_STAT,
  ID_DALL,

  ID_PGUP,
  ID_PGDOWN,
  ID_SHOWPATH,

  ID_HELP,
  ID_ASK,
  ID_VERSION,
  ID_INFO,
  ID_LIST,

  ID_STEPS,
  ID_XSTEP,
  ID_YSTEP,
  ID_ZSTEP,
  ID_NONE2,

  /*ID_CAP_TB,
  ID_CAP_XYZ,
  ID_CAP_SDPOS,*/
  ID_NONE3
  };
/*
// 2. Имена для индексов (чтобы switch был читаемым)
static const char* gAnswerIDName[] PROGMEM __attribute__((aligned(4))) = {
  "ID_OK",      "ID_RESEND",  "ID_RS",        "ID_SS",        "ID_PFT",       "ID_PTF",      //"ID_CAP",
  "ID_ERROR",   "ID_WAIT",    "ID_ECHO",      "ID_ACTION",    "ID_SD_BEGIN",  "ID_SD_END",
  "ID_OPEN_FAIL", "ID_WR_START", "ID_WR_DONE",  "ID_DEL_FAIL",  "ID_DELETED",  "ID_SELECTED", "ID_PRN_DONE", "ID_NO_PRN", "ID_AREA",
  "ID_NONE",
  "ID_TEMP_T",  "ID_TEMP_B",  "ID_POS_X",     "ID_POS_Y",     "ID_POS_Z",     "ID_POS_E",     "ID_SXYZ",
  "ID_SD_OK",   "ID_SD_RELEASED", "ID_SD_NOSD_1", "ID_SD_NOSD_2", "ID_NOTIFY", "ID_BUSY",   "ID_SD_PROGRESS", "ID_FINDEX",
  "ID_SYNC",    "ID_EXIT",    "ID_QUERY",     "ID_OPEN",      "ID_CLOSE",     "ID_ABORT",
  "ID_REBOOT",  "ID_OTA",     "ID_CONFIG",
  "ID_DTRANS",  "ID_DUART",   "ID_DSERV",     "ID_DCLIENT",   "ID_DMQTT",   "ID_STAT",  "ID_DALL",
  "ID_PGUP",    "ID_PGDOWN",  "ID_SHOWPATH",  "ID_HELP",      "ID_ASK",       "ID_VERSION",   "ID_INFO",    "ID_LIST",
  "ID_STEPS",   "ID_XSTEP",   "ID_YSTEP",     "ID_ZSTEP",     "ID_NONE2",
  //"ID_CAP_TB",  "ID_CAP_XYZ", "ID_CAP_SDPOS",
  "ID_NONE3"
  };
*/
const char ok_str[]     PROGMEM __attribute__((aligned(4))) = "ok";
const char resend_str[] PROGMEM __attribute__((aligned(4))) = "Resend:";
const char rs_str[]     PROGMEM __attribute__((aligned(4))) = "rs";
const char ss_str[]     PROGMEM __attribute__((aligned(4))) = "ss";
const char pft_str[]    PROGMEM __attribute__((aligned(4))) = "PFT:";
const char ptf_str[]    PROGMEM __attribute__((aligned(4))) = "PTF:";
//const char cap_str[]    PROGMEM = "Cap:";

const char error_str[]  PROGMEM __attribute__((aligned(4))) = "Error:";
const char wait_str[]   PROGMEM __attribute__((aligned(4))) = "wait";
const char echo_str[]   PROGMEM __attribute__((aligned(4))) = "echo:";
const char action_str[] PROGMEM __attribute__((aligned(4))) = "//action:";
const char sdBeg_str[]  PROGMEM __attribute__((aligned(4))) = "Begin";    // "Begin file list";

const char sdEnd_str[]  PROGMEM __attribute__((aligned(4))) = "End f";    // "End file list";
const char sdOpen_str[] PROGMEM __attribute__((aligned(4))) = "open f";   // "open failed,
const char sdWrite_str[] PROGMEM __attribute__((aligned(4))) = "Writing"; // "Writing to file:
const char sdDone_str[] PROGMEM __attribute__((aligned(4))) = "Done s";   // "Done saving file
const char sdDelX_str[] PROGMEM __attribute__((aligned(4))) = "Deletion"; // "Deletion failed
const char sdFDel_str[] PROGMEM __attribute__((aligned(4))) = "File d";   // "File deleted:
const char sdFSel_str[] PROGMEM __attribute__((aligned(4))) = "File se";  // "File selected
const char prnDone_str[] PROGMEM __attribute__((aligned(4))) = "Done p";  // "Done printing file"
const char noPrn_str[]  PROGMEM __attribute__((aligned(4))) = "Not SD";   // "Not SD printing"
const char area_str[]   PROGMEM __attribute__((aligned(4))) = "area:{";

const char t_str[]      PROGMEM __attribute__((aligned(4))) = "T:";
const char b_str[]      PROGMEM __attribute__((aligned(4))) = "B:";
const char x_str[]      PROGMEM __attribute__((aligned(4))) = "X:";
const char y_str[]      PROGMEM __attribute__((aligned(4))) = "Y:";
const char z_str[]      PROGMEM __attribute__((aligned(4))) = "Z:";
const char e_str[]      PROGMEM __attribute__((aligned(4))) = "E:";
const char sxyz_str[]   PROGMEM __attribute__((aligned(4))) = "S_XYZ:";

const char sdOK_str[]   PROGMEM __attribute__((aligned(4))) = "SD card ok";
const char sdrls_str[]  PROGMEM __attribute__((aligned(4))) = "SD card re"; // leased
const char sdno1_str[]  PROGMEM __attribute__((aligned(4))) = "No SD card";
const char sdno2_str[]  PROGMEM __attribute__((aligned(4))) = "No media";
const char notify_str[] PROGMEM __attribute__((aligned(4))) = "notification ";
const char busy_str[]   PROGMEM __attribute__((aligned(4))) = "busy: pr";
const char sdprn_str[]  PROGMEM __attribute__((aligned(4))) = "SD printing byte";

const char sync_str[]   PROGMEM __attribute__((aligned(4))) = "SYNC)";
const char exit_str[]   PROGMEM __attribute__((aligned(4))) = "EXIT)";
const char query_str[]  PROGMEM __attribute__((aligned(4))) = "QUERY)";
const char open_str[]   PROGMEM __attribute__((aligned(4))) = "OPEN)";
const char close_str[]  PROGMEM __attribute__((aligned(4))) = "CLOSE)";
const char abort_str[]  PROGMEM __attribute__((aligned(4))) = "ABORT)";
const char rst_str[]    PROGMEM __attribute__((aligned(4))) = "R)";
const char cfg_str[]    PROGMEM __attribute__((aligned(4))) = "X)";
const char ota_str[]    PROGMEM __attribute__((aligned(4))) = "O)";

const char dt_str[]     PROGMEM __attribute__((aligned(4))) = "T)";
const char du_str[]     PROGMEM __attribute__((aligned(4))) = "U)";
const char ds_str[]     PROGMEM __attribute__((aligned(4))) = "S)";
const char dc_str[]     PROGMEM __attribute__((aligned(4))) = "C)";
const char dm_str[]     PROGMEM __attribute__((aligned(4))) = "M)";
const char dp_str[]     PROGMEM __attribute__((aligned(4))) = "P)";
const char dall_str[]   PROGMEM __attribute__((aligned(4))) = "*)";

const char pgup_str[]   PROGMEM __attribute__((aligned(4))) = "-)";
const char pgdown_str[] PROGMEM __attribute__((aligned(4))) = "+)";
const char showpath_str[] PROGMEM __attribute__((aligned(4))) = ".)";
const char h_str[]      PROGMEM __attribute__((aligned(4))) = "H)";
const char h0_str[]     PROGMEM __attribute__((aligned(4))) = "?)";
const char v_str[]      PROGMEM __attribute__((aligned(4))) = "V)";
const char i_str[]      PROGMEM __attribute__((aligned(4))) = "I)";

const char steps_str[]  PROGMEM __attribute__((aligned(4))) = "Count ";
const char xs_str[]     PROGMEM __attribute__((aligned(4))) = "X:";
const char ys_str[]     PROGMEM __attribute__((aligned(4))) = "Y:";
const char zs_str[]     PROGMEM __attribute__((aligned(4))) = "Z:";

/*const char captemp_str[]  PROGMEM __attribute__((aligned(4))) = "AUTOREPORT_TEMP:";       // ID_CAP_TB 
const char capcoord_str[] PROGMEM __attribute__((aligned(4))) = "AUTOREPORT_POS:";        // ID_CAP_XYZ
const char capsdpos_str[] PROGMEM __attribute__((aligned(4))) = "AUTOREPORT_SD_STATUS:";  // ID_CAP_SDPOS*/

const char zero_str[]   PROGMEM __attribute__((aligned(4))) = "";

// 3. Массив структур в PROGMEM
// Атрибут aligned(4) важен для ESP8266 для быстрого доступа
static const gAnswer_t gAnswer[] PROGMEM __attribute__((aligned(4))) = {
  {ok_str, 2},      // ID_OK
  {resend_str, 7},  // ID_RESEND  (Длинный формат: "Resend: 123")
  {rs_str, 2},      // ID_RS      (Короткий (BFT) формат: "rs123")
  {ss_str, 2},      // ID_SS      BFT ответ на SYNC,  формат: "ss1,96,0.1.0")
  {pft_str, 4},     // ID_PFT     PFT:abcde.. - сообщения о логическом состоянии BFT
  {ptf_str, 4},     // ID_PTF     из-за опечатки в исходнике Марлина - "дублер" PFT:
  /*{cap_str, 4},     // ID_CAP     в ответе на M115*/

  {error_str, 6},   // ID_ERROR     // Можно захватить текст ошибки
  {wait_str, 4},    // ID_WAIT
  {echo_str, 5},    // ID_ECHO
  {action_str, 9},  // ID_ACTION
  // SD-карта
  {sdBeg_str, 5},   // ID_SD_BEGIN
  {sdEnd_str, 5},   // ID_SD_END
  {sdOpen_str, 6},  // ID_OPEN_FAIL
  {sdWrite_str, 7}, // ID_WR_START
  {sdDone_str, 6},  // ID_WR_DONE
  {sdDelX_str, 8},  // ID_DEL_FAIL
  {sdFDel_str, 6},  // ID_DELETED
  {sdFSel_str, 7},  // ID_SELECTED
  {prnDone_str, 6}, // ID_PRN_DONE
  {noPrn_str, 6},   // ID_NO_PRN
  {area_str, 6},    // ID_AREA

  // Стоп-маркер поиска информ.сообщений
  {zero_str, 0},    // ID_NONE

  // Температуры (самые частые данные)
  {t_str, 2},       // ID_TEMP_T
  {b_str, 2},       // ID_TEMP_B
  // Координаты
  {x_str, 2},       // ID_POS_X
  {y_str, 2},       // ID_POS_Y
  {z_str, 2},       // ID_POS_Z
  {e_str, 2},       // ID_POS_E
  {sxyz_str, 6},    // ID_SXYZ
  {sdOK_str, 10},   // ID_SD_OK
  {sdrls_str, 10},  // ID_SD_RELEASED
  {sdno1_str, 10},  // ID_SD_NOSD_1
  {sdno2_str, 8},   // ID_SD_NOSD_2
  {notify_str, 13}, // ID_NOTIFY
  {busy_str, 8},    // ID_BUSY
  {sdprn_str, 16},  // ID_SD_PROGRESS

  // Стоп-маркер для поиска параметров
  {zero_str, 0},    // ID_FINDEX

  {sync_str, 5},    // ID_SYNC
  {exit_str, 5},    // ID_EXIT
  {query_str, 6},   // ID_QUERY
  {open_str, 5},    // ID_OPEN
  {close_str, 6},   // ID_CLOSE
  {abort_str, 6},   // ID_ABORT

  {rst_str, 2},     // ID_REBOOT
  {ota_str, 2},     // ID_OTA
  {cfg_str, 2},     // ID_CONFIG

  {dt_str, 2},      // ID_DTRANS
  {du_str, 2},      // ID_DUART
  {ds_str, 2},      // ID_DSERV
  {dc_str, 2},      // ID_DCLIENT
  {dm_str, 2},      // ID_DMQTT
  {dp_str, 2},      // ID_STAT
  {dall_str, 2},    // ID_DALL

  {pgup_str, 2},    // ID_PGUP,
  {pgdown_str, 2},  // ID_PGDOWN,

  {showpath_str, 2},// ID_SHOWPATH,

  {h_str, 2},       // ID_HELP
  {h0_str, 2},      // ID_ASK
  {v_str, 2},       // ID_VERSION
  {i_str, 2},       // ID_INFO
  // Стоп-маркер поиска myBridgeCmd
  {zero_str, 0},    // ID_LIST

  {steps_str, 6},   // ID_STEPS
  {xs_str, 2},      // ID_XSTEP
  {ys_str, 2},      // ID_YSTEP
  {zs_str, 2},      // ID_ZSTEP
  // Стоп-маркер парсинга шагов
  {zero_str, 0},    // ID_NONE2

  /*{captemp_str, 16}, // ID_CAP_TB
  {capcoord_str, 15},// ID_CAP_XYZ
  {capsdpos_str, 21},// ID_CAP_SDPOS*/
  // Стоп-маркер парсинга "Cap:AUTO.."
  {zero_str, 0}     // ID_NONE3
  };

// --- битовая маска управления выводом лога ---
#define UART_LOG    0xF
#define UART_ACK    1
#define UART_COUNT  2
#define UART_READ   4
#define UART_SEND   8
#define CLIENT_LOG  0x10
#define CLIENT_BIN  0x20
#define SERV_LOG    0x40
#define TRANS_LOG   0x80
#define MQTT_LOG    0xF00
#define MQTT_PDATA  0x100
#define MQTT_PUB    0x200
#define MQTT_SDATA  0x400
#define MQTT_SUB    0x800
#define FULL_STAT   0x1000

uint32_t ctrl = 0;                  // маска управления выводом лога отладки
//uint32_t ctrl = CLIENT_LOG | UART_LOG | SERV_LOG;  // если сразу нужна отладка ставим по вкусу
bool dBroadcast = false;            // управление режимом вывода лога отладки : true == всем / false == активисту

// ----- структура для листинга файлов -----
struct sdFile_t {
  char *fInfo, *longName;
  uint32_t pathLen, shortLen, fSize;
  uint32_t fIdx, pageBegIdx, pageSize, maxIdx;
  uint32_t pHash;   //  & 0x0FFFFFFF == хеш fInfo файла, выбранного для запуска печати
  uint32_t opCode;  //  & 0x0FFFFFFF == хеш fInfo файла, выбранного для печати при навигации
                    //  & 0xF0000000 == флаги пометки при удалении и навигации
  uint32_t sizeCount; // используется для подсчета размера при удалении
  uint32_t keepCount; // используется для подсчета создаваемых при удалении
  // параметры сравнения для выбора при получении полного списка файлов
  uint32_t selNum;  // количество файлов, попадавших под шаблон сравнения
  uint32_t selIdx;  // >0 - в конце получения найти/выбрать с таким индексом
  bool selName;     // true - при получении списка имен найти/выбрать совпадение с socket.fName & socket.fSize
  // параметры строки пути, выбранного для записи/печати файлов
  uint32_t wrkPLen; // длина строки рабочего пути sPath[]
  char wrkPath[MAX_FNAME_LEN + 1];  // буфер для хранения строки рабочего пути
  };
sdFile_t sdFile = {NULL, NULL, 0, 0, 0, 0, 0, 10, 0, 0, 0, 0, 0, 0, 0, false, 0, {0}}; 

// ----- структура для публикации данных MQTT -----
struct gData_t {
  float xPos, yPos, zPos, ePos;
  long xStep, yStep, zStep;
  float currentHotend, targetHotend, currentBed, targetBed;
  uint32_t progress;
  //char* fileName;
  uint32_t upTime;
  uint32_t pubArea = 0;
  long sxyzCode;
  };
gData_t gData;

struct strMem_t {
  uint32_t mLen, tLog; uint16_t xCS; };

#define GANSWER_BUF_SIZE  8192

char gAnswer_buf[GANSWER_BUF_SIZE];
uint32_t gAnswer_idx = 0, anchorIdx = 0, timeResend = 0, busyMarkerTime = 0;
char* gDataPtr = NULL;
bool fListMode = false, sdIsOK = true, samePos = true, ipKnown = false;

#define RESEND_MAX 5

const char qWifi_ex[]   PROGMEM = "excellent";
const char qWifi_good[] PROGMEM = "good";
const char qWifi_avg[]  PROGMEM = "average";
const char qWifi_bad[]  PROGMEM = "bad";

static const char* qWifiStr[] PROGMEM __attribute__((aligned(4))) = {
  qWifi_ex, qWifi_good, qWifi_avg, qWifi_bad };

