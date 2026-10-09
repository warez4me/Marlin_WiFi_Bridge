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

/* --- шаблоны для парсинга ответов Marlin и терминальных команд--- */
// 1. Определение структуры
struct gAnswer_t {
  const char* idWord;   // указатель на текст для сравнения
  uint32_t idLen;       // длина текста для сравнения
  };

// индексы всех шаблонов перечислены в пределах единого множества, но некоторые позиции
// являются "разделителями" между группами
// процедура поиска ищет совпадение текста, используя массив структур-шаблонов,
// в пределах группы, начиная от переданного ей стартового индекса,
// и останавливается , когда встречает структуру, содержащую поле idLen = 0
// такие структуры соответствуют одному из разделительных индексов
// ID_NONE, ID_FINDEX, ID_LIST, ID_NONE2
// причем ID_FINDEX и ID_LIST дополнительно используются как самостоятельные идентификаторы
// в процедуре обработки терминальных команд myBridgeCmd()
enum gAnswerID_t {
// Парсинг начинаем с поиска подстроки основного ИД в начале строки ответа
// набор структур-шаблонов для этого начинается с индекса ID_OK и заканчиватся индексом ID_AREA
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

// после поиска основного ИД производится поиск пар параметр[:значение]
// параметры могут находиться как в строке с основным ИД, так и в отдельной строке без основного ИД
// после подстроки параметра может размещаться числовое значение или другая информация типа имени файла
// перечень индексов шаблонов параметров начинается с ID_TEMP_T заканчивается ID_SD_PROGRESS
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

// перечень индексов шаблонов терминальных команд
// начинается с индекса ID_SYNC и заканчивается ID_INFO
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

// набор индексов для шаблонов параметров микрошагов
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
// 1.1 Имена для индексов (чтобы switch был читаемым)
static const char* gAnswerIDName[] PROGMEM  = {
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
// 2. текст для поиска совпадения
const char ok_str[]     PROGMEM  = "ok";
const char resend_str[] PROGMEM  = "Resend:";
const char rs_str[]     PROGMEM  = "rs";
const char ss_str[]     PROGMEM  = "ss";
const char pft_str[]    PROGMEM  = "PFT:";
const char ptf_str[]    PROGMEM  = "PTF:";
//const char cap_str[]    PROGMEM = "Cap:";

const char error_str[]  PROGMEM  = "Error:";
const char wait_str[]   PROGMEM  = "wait";
const char echo_str[]   PROGMEM  = "echo:";
const char action_str[] PROGMEM  = "//action:";
const char sdBeg_str[]  PROGMEM  = "Begin";    // "Begin file list";

const char sdEnd_str[]  PROGMEM  = "End f";    // "End file list";
const char sdOpen_str[] PROGMEM  = "open f";   // "open failed,
const char sdWrite_str[] PROGMEM  = "Writing"; // "Writing to file:
const char sdDone_str[] PROGMEM  = "Done s";   // "Done saving file
const char sdDelX_str[] PROGMEM  = "Deletion"; // "Deletion failed
const char sdFDel_str[] PROGMEM  = "File d";   // "File deleted:
const char sdFSel_str[] PROGMEM  = "File se";  // "File selected
const char prnDone_str[] PROGMEM  = "Done p";  // "Done printing file"
const char noPrn_str[]  PROGMEM  = "Not SD";   // "Not SD printing"
const char area_str[]   PROGMEM  = "area:{";

const char t_str[]      PROGMEM  = "T:";
const char b_str[]      PROGMEM  = "B:";
const char x_str[]      PROGMEM  = "X:";
const char y_str[]      PROGMEM  = "Y:";
const char z_str[]      PROGMEM  = "Z:";
const char e_str[]      PROGMEM  = "E:";
const char sxyz_str[]   PROGMEM  = "S_XYZ:";

const char sdOK_str[]   PROGMEM  = "SD card ok";
const char sdrls_str[]  PROGMEM  = "SD card re"; // leased
const char sdno1_str[]  PROGMEM  = "No SD card";
const char sdno2_str[]  PROGMEM  = "No media";
const char notify_str[] PROGMEM  = "notification ";
const char busy_str[]   PROGMEM  = "busy: pr";
const char sdprn_str[]  PROGMEM  = "SD printing byte";

const char sync_str[]   PROGMEM  = "SYNC)";
const char exit_str[]   PROGMEM  = "EXIT)";
const char query_str[]  PROGMEM  = "QUERY)";
const char open_str[]   PROGMEM  = "OPEN)";
const char close_str[]  PROGMEM  = "CLOSE)";
const char abort_str[]  PROGMEM  = "ABORT)";
const char rst_str[]    PROGMEM  = "R)";
const char cfg_str[]    PROGMEM  = "X)";
const char ota_str[]    PROGMEM  = "O)";

const char dt_str[]     PROGMEM  = "T)";
const char du_str[]     PROGMEM  = "U)";
const char ds_str[]     PROGMEM  = "S)";
const char dc_str[]     PROGMEM  = "C)";
const char dm_str[]     PROGMEM  = "M)";
const char dp_str[]     PROGMEM  = "P)";
const char dall_str[]   PROGMEM  = "*)";

const char pgup_str[]   PROGMEM  = "-)";
const char pgdown_str[] PROGMEM  = "+)";
const char showpath_str[] PROGMEM  = ".)";
const char h_str[]      PROGMEM  = "H)";
const char h0_str[]     PROGMEM  = "?)";
const char v_str[]      PROGMEM  = "V)";
const char i_str[]      PROGMEM  = "I)";

const char steps_str[]  PROGMEM  = "Count ";
const char xs_str[]     PROGMEM  = "X:";
const char ys_str[]     PROGMEM  = "Y:";
const char zs_str[]     PROGMEM  = "Z:";

/*const char captemp_str[]  PROGMEM  = "AUTOREPORT_TEMP:";       // ID_CAP_TB 
const char capcoord_str[] PROGMEM  = "AUTOREPORT_POS:";        // ID_CAP_XYZ
const char capsdpos_str[] PROGMEM  = "AUTOREPORT_SD_STATUS:";  // ID_CAP_SDPOS*/

const char zero_str[]   PROGMEM  = "";

// 3. Массив структур в PROGMEM
// Атрибут aligned(4) важен для ESP8266 для быстрого доступа
static const gAnswer_t gAnswer[] PROGMEM  = {
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
uint8_t dBroadcast = WS_UNICAST;    // управление режимом вывода лога отладки : WS_BROADCAST == всем / WS_UNICAST == активисту

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

char gAnswer_buf[GANSWER_BUF_SIZE] ; // !! aligned(4) для scanWiFi()
uint32_t gAnswer_idx = 0, anchorIdx = 0, timeResend = 0, busyMarkerTime = 0;
char* gDataPtr = NULL;
bool fListMode = false, sdIsOK = true, samePos = true, ipKnown = false;

#define RESEND_MAX 5

const char qWifi_ex[]   PROGMEM = "excellent";
const char qWifi_good[] PROGMEM = "good";
const char qWifi_avg[]  PROGMEM = "average";
const char qWifi_bad[]  PROGMEM = "bad";

static const char* qWifiStr[] PROGMEM  = {
  qWifi_ex, qWifi_good, qWifi_avg, qWifi_bad };

