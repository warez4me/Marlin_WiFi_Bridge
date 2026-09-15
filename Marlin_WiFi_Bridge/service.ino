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

#include "MQTT_topics.h"

bool getNum(uint32_t* val, char** srcPtr, uint32_t maxVal, bool skipSp) {
  if (!(val) || !(srcPtr) || !(*srcPtr)) return false; // защита на входе
  char* sPtr = *srcPtr;       //  локальные копии в регистрах
  if (skipSp) while ((*sPtr == ' ') || (*sPtr == '\t')) sPtr +=1;
  uint32_t i = 0, sVal = 0;
  for (; isdigit((unsigned char)*sPtr) && (sVal <= maxVal); i++, sPtr++)
    sVal = (sVal * 10) + (*sPtr & 0xf);
  if (!(i) || (sVal > maxVal))
    return false;
  *val = sVal; *srcPtr = sPtr;
  return true;
}

void doReboot(bool sidReset) {
  int32_t len = 0;
  for (int i = 0; i < 3; i++)
    len += snprintf_P((char*)packet + len, NET_DATA_MAX - len, PSTR("H:%u:%d:%d:%d\n"),
                      sessionID,
                      ((errCode == ERR_NO_ERRORS)? HB_WAIT: HB_ERROR),
                      socket.clGroup_ID + !!(sidReset),  // +(сбросить активиста)
                      ((ctrl & CLIENT_LOG) >> 4));
  len += snprintf_P((char*)packet + len, NET_DATA_MAX - len, PSTR(
                          "L:,WiFi bridge reboot. Just wait..\n"));
  netQuePut(packet, -len);                        // urgent HB
  netQueSend();
  delay(200);
  uartWxStop = false;
  WITH_FLAG_OFF(UART_SEND, (uart_w((char*)PSTR("M117 WiFi bridge reboot..\n"))));  // Сообщаем на LCD принтера
  uartWxStop = true;
  webSocket.disconnect();                         // Закрываем соединения
  delay(200);
  httpServer.stop();
  delay(200);
  ESP.restart();                                  // Или system_restart();
}

void showTime(uint32_t show_cid) {    // cid [ | 0xFFFF0000 ]
  // Высылает в лог клиенту информацию
  // - даты/времени в формате: "Day, DD Mon YYYY HH:MM:SS" (если есть синхро с NTP)
  // - UpTime в формате: "HH:MM:SS"
  // - статус MQTT
  // - инфо о качестве WiFi Rx сигнала 
  // - инфо о количестве подключенных WS клиентов
  // - инфо о количестве файлов/папок на Sd карте
  int len = 0;
  time_t now;
  struct tm* timeinfo;
  if (time(nullptr) > 1451616000) {
    now = time(nullptr);
    timeinfo = localtime(&now);
    // Используем стандартные Си-спецификаторы форматирования:
    // %a - день недели (сокр.), %d - день, %b - месяц (сокр.), %Y - год, %H:%M:%S - время
    len = strftime((char*)packet, NET_DATA_MAX, fl2chr(PSTR("L:,%a, %d %b %Y %H:%M:%S")), timeinfo);
    // Переводим миллисекунды Uptime в тип time_t (секунды)
    uint32_t msecNow = millis(); now = msecNow / 1000;
    // заполняем структуру времени (считается от 00:00:00)
    timeinfo = gmtime(&now);
    len += strftime((char*)packet + len, NET_DATA_MAX - len, fl2chr(PSTR(", uptime %H:%M:%S")), timeinfo);
    if (ctrl & SERV_LOG)
      len += snprintf_P((char*)packet + len, NET_DATA_MAX - len, PSTR(".%d (=%u msec)"), msecNow % 1000, msecNow);
    len += snprintf_P((char*)packet + len, NET_DATA_MAX - len, PSTR("\n"));
    }
  long rssi = WiFi.RSSI();
  // 1. Вычисление процентов (Линейная интерполяция)
  int qWiFi = (rssi <= -100) ? 0 : 100;
  if ((qWiFi > 0) && (rssi < -50)) { qWiFi = 2 * (rssi + 100); }
  // 2. Определение текстового статуса (Строго сверху вниз от лучшего к худшему)
  int qWiFiIdx = 3;
  if       (rssi >= -55) qWiFiIdx = 0;
   else if (rssi >= -70) qWiFiIdx = 1;
   else if (rssi >= -85) qWiFiIdx = 2;
  len += snprintf_P((char*)packet + len, NET_DATA_MAX - len, PSTR("L:,WiFi Rx:%S %d%%, %ld dBm\n"),
                          (const char*)pgm_read_ptr(&qWifiStr[qWiFiIdx]), qWiFi, rssi);
  uint32_t wsConn = pgm_read_dword(wsNumber + wsMap); // &wsNumber[wsMap]
  bool mqttOk = (dState[DISCOVERY_COUNT - 1] == DISCOVERY_ANNOUNCED);
  len += snprintf_P((char*)packet + len, NET_DATA_MAX - len, PSTR("L:,WS:%u connected   MQTT:%S\n"),
                    wsConn, ((mqttOk)? PSTR("Ok"): PSTR("No connection")));
  netQuePut_cid(packet, len, (show_cid & CID_ALL));
  qWiFiIdx = 0;
  if ((bridgeState != SYS_IDLE) && (bridgeState != SYS_PRE_PRINT)) {
    len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:,SD card: busy [%S]\n"),
                                                        (const char*)pgm_read_ptr(&brStateName[bridgeState]));
    qWiFiIdx = 1; }
   else if ((anchorIdx) && !fListMode) {
    len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR( "L:,SD: %u (%u) files, %u folders\n"),
                                                        sdFile.sizeCount, sdFile.maxIdx, sdFile.keepCount);
    qWiFiIdx = 2; }
   else
    len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:,SD card: '0', '+', '-' or <N> to rescan\n"));
  if (qWiFiIdx) len += snprintf_P((char*)packet + len, NET_DATA_MAX - len, PSTR("L:,Work path: \"%s\"\n"),
                                                                  ((sdFile.wrkPLen)? sdFile.wrkPath: "/"));
  netQuePut_cid(packet, len, (show_cid & CID_ALL));
  // инфо о текущем выбранном файле выдаём только активисту по команде (I)
  if ((qWiFiIdx == 2) && (show_cid & 0xFFFF0000)) fileID(true, socket.fSize);//, false);
}

bool setWrkPath(bool show) {
  int len = 0;
  if ((sdFile.pathLen) && (*sdFile.fInfo < 'A')) {
    len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:!Bad pathname: can't set\n"));
    netQuePut_cid(packet, len, socket.clWS_ID);
    return false; }
  sdFile.wrkPLen = sdFile.pathLen; sdFile.wrkPath[sdFile.wrkPLen] = '\0';
  if (sdFile.wrkPLen) memcpy((uint8_t*)sdFile.wrkPath, (uint8_t*)sdFile.fInfo, sdFile.wrkPLen);
  if (show) {
    len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:Work path: \"%s\"\n"),
                                                    ((sdFile.wrkPLen)? sdFile.wrkPath: "/"));
    netQuePut_cid(packet, len, socket.clWS_ID); }
  return true;
}

void setState() {
  // данная функция вызывается из fileID() для переключения системы в SYS_PRE_PRINT под файл, данные которого находятся в структуре sdFile 
  // на входе в sdFile.opCode могут быть установлены битовые флаги в самом старшем (из 8-ми) ниббле:
  // флаг canSetOpCode указывает, что также нужно установить новое значение sdFile.opCode в соответствии с параметрами строки sdFile.fInfo
  // флаг clSel указывает, что файл выбран на стороне (диске) клиента, а не из листинга SD карты, и нужно заполнить структуру socket
  bool canSetOpCode = (!(sdFile.opCode & 0x80000000)), clSel = (!(sdFile.opCode & 0x40000000));
  sdFile.opCode &= 0x3FFFFFFF;
  if (sdFile.fSize < 3) {
    netQuePut(NULL, 0, (char*)PSTR("L:!File too small. Name context lost.\n"), socket.clWS_ID);
    bridgeState = ok.setState; sdFile.opCode = 0; *socket.fName = '\0'; socket.fSize = 0;
    return; }
  bridgeState = SYS_PRE_PRINT; sdFile.pHash = csFInfo(sdFile.fInfo);  // запоминаем хэш текущего выбранного файла
  size_t len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:Selected [%d] to print\n"), sdFile.selIdx);
  netQuePut(packet, len);
  if (!clSel) {                                 // файл выбран по индексу из SD списка, а не на стороне клиента
    if (!fileInfo(1, false)) {                  // только сравнить имена socket и sdFile
      // если имена socket и sdFile не совпадают - копируем имя sdFile в socket 
      if (sdFile.longName)
        memcpy(socket.fName, sdFile.longName, (MAX_FNAME_LEN + 1)); // копируем максимум, терминатор захватим по-любому
       else {
        memcpy(socket.fName, (sdFile.fInfo + sdFile.pathLen), sdFile.shortLen); socket.fName[sdFile.shortLen] = '\0'; }
      socket.fSize = sdFile.fSize; socket.clFileSel = false;
      }
     else                       // выбран файл в другой папке SD списка с аналогичным именем, как в soket, и копирование не требуется
      if (!socket.clFileSel)    // если в socket содержится "эталонная" инфа после выбора на диске клиента (глобальный контекст) - не трогаем
        socket.fSize = sdFile.fSize;  // если в socket содержится инфа файла из SD списка (локальный контекст), меняем размер (контекст)
    }
  sessionID = 0;  // по идее, если сменили имя, то нужно обновить контекст UX у клиентов
  if (canSetOpCode) sdFile.opCode = sdFile.pHash;  // фиксируем хэш выбранного файла как опорный для навигации
}

int confirmStr(strMem_t* cmdMem, char* msg, size_t length) {
  uint16_t xCS = (((msg) && (length))? xorCS(msg, length): 0);
  uint32_t now = millis();
  int res = -1;
  if ((cmdMem->xCS == xCS) && (cmdMem->mLen == length))
    res = ((now - cmdMem->tLog) < 20000)? 1: 0;
  cmdMem->xCS = xCS; cmdMem->mLen = length;
  cmdMem->tLog = now - ((res == 1)? 20001: 0);  // делаем "просроченной" при 1-м подтверждении
  return res;
}

inline void fListGet(int showBegIdx) {
  sdFile.selName = false; sdFile.selIdx = 0;    // сбрасываем флаги поиска при получении списка файлов
  if (showBegIdx >= 0) sdFile.pageBegIdx = showBegIdx;
  ok.skip = 1; ok.waiting = false; fListWait = true; fListTOut = WAIT_FLIST_TIMEOUT;
  size_t len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("M21\nM20 L \"%s\"\n"),
                                          ((sdFile.wrkPLen)? sdFile.wrkPath: "/"));
  uart_w((char*)packet, len);
}

#define STATUS_ON_OFF(mask) ((ctrl & (mask)) ? PSTR("ON") : PSTR("OFF"))

bool myBridgeCmd(char* msg, size_t length) {
  if (length >= 10) return false;
  char cBuf[10] = {'\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0'};
  if (length == 1) {
    // если введенный символ является легальной командой
    // формируем полную форму сообщения с этим символом
    // Строка живет только во Flash, в RAM места ноль
    if (memchr_P(PSTR("-+.?*hHvViIcCsStTmMuUpP"), *msg, sizeof("-+.?*hHvViIcCsStTmMuUpP") - 1)) {
      cBuf[0] = '('; cBuf[1] = *msg; cBuf[2] = ')'; length = 3; }
     else cBuf[0] = *msg;
    }
   else memcpy((uint8_t*)cBuf, (uint8_t*)msg, length);
  static strMem_t cmdMem = {0, 0, 0};
  msg = cBuf;
  size_t len;
  char* cmdMsg = msg + 1;
  uint32_t numVal = 0;
  bool hasNum = getNum(&numVal, &msg, 200, true); // проверка, что эта строка - число для вывода списка файлов
                                                  // msg сдвигается на 1й символ после числа, если не число - остается в начале строки
                                                  // 200 соотв. числу элементов лога у JS клиента
  if (hasNum && (*msg)) return false;             // если строка начинается с числа, то д.б. только число
  if (!hasNum && (*msg != '(')) return false;     // иначе строка должна начинаться с '('
  uint cmdID = (uint)ID_SYNC;                     // начало списка шаблонов
  gAnswerID_t myID = (hasNum)? ID_LIST: findID(&cmdMsg, &cmdID, true); // ID_LIST, если число, либо поиск совпадения с шаблоном
  if ((myID >= ID_CONFIG) && (myID <= ID_DALL))   // команды управления логированием вида (x)N
    // cmdMsg указывает на 1-й символ параметра сразу после ')'
    hasNum = getNum(&numVal, &cmdMsg, 0xFFFFFFFF, true);  // читаем числовой параметр, если он есть
  static bool fListMem = false;
  bool fListCmd = ((myID == ID_LIST) || (myID == ID_PGUP) || (myID == ID_PGDOWN));
  if (fListCmd) fListScanned = FLIST_NOT_SCANNED;
  if ((myID != ID_NONE) && (!fListMem || !fListCmd)) {
    len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L::: %s\n"), cBuf);
    netQuePut(packet, len);                       // вывод в лог текста введенной команды
    }
  fListMem = fListCmd;                            // чтобы не показывать текст команды при листании списка файлов
  //if (ctrl & SERV_LOG) {
  //  len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:~myID %d cmdID %d\n"), (uint)myID, cmdID);
  //  netQuePut_cid(packet, len, socket.clWS_ID);
  //  }
  bool res = true;
  switch(myID) {
    case ID_SYNC:
      ok.doLog = true;
      sendBFT(0, BFT_SYNC);
      break;
    case ID_EXIT:
      ok.doLog = true;
      sendBFT(0, BFT_EXIT);
      break;
    case ID_QUERY:
      ok.doLog = true;
      sendBFT(0, BFT_QUERY);
      break;
    case ID_OPEN:
      ok.doLog = true;
      sendBFT(0, BFT_OPEN);
      break;
    case ID_CLOSE:
      ok.doLog = true;
      sendBFT(0, BFT_CLOSE);
      break;
    case ID_ABORT:
      ok.doLog = true;
      sendBFT(0, BFT_ABORT);
      break;
    case ID_INFO:
      showTime(socket.clWS_ID | 0xFFFF0000);
      break;
    case ID_PGUP: {
      if (ctrl & SERV_LOG) {
        char lPtr = '?';
        if (sdFile.fInfo)
          lPtr = ((sdFile.fInfo > &gAnswer_buf[anchorIdx])? '>': '=');
        len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:pgUp- ptr%ca%d i%d x%d p%d\n"),
                                        lPtr, anchorIdx, sdFile.pageBegIdx, sdFile.maxIdx,
                                        sdFile.pageSize);
        netQuePut_cid(packet, len, socket.clWS_ID); }
      if (!(anchorIdx) || !(sdFile.pageBegIdx)) {
        // тут похоже список непроинициализирован
        if (ctrl & SERV_LOG) netQuePut(NULL, 0, (char*)PSTR("L:Name list rescan.\n"), socket.clWS_ID);
        fListGet(1); break; }
      if (sdFile.pageBegIdx <= 1) {
        sdFile.pageBegIdx = sdFile.maxIdx - sdFile.pageSize + 1;
        if (sdFile.pageBegIdx < 1) sdFile.pageBegIdx = 1;
        showFileList(((sdFile.pageBegIdx + sdFile.pageSize - 1) <= sdFile.maxIdx)? sdFile.pageSize: (sdFile.maxIdx - sdFile.pageBegIdx)); }
       else {
        uint32_t toShow = sdFile.pageSize;
        if(sdFile.pageBegIdx > sdFile.pageSize) sdFile.pageBegIdx -= sdFile.pageSize;
         else {
          toShow = sdFile.pageBegIdx - 1; sdFile.pageBegIdx = 1; }
        showFileList(toShow);
        }
      break; }
    case ID_LIST:
      if (!(numVal) || !(anchorIdx) || !(sdFile.pageBegIdx)) {
        // тут похоже список непроинициализирован
        if (ctrl & SERV_LOG) netQuePut(NULL, 0, (char*)PSTR("L:Name list rescan.\n"), socket.clWS_ID);
        if (numVal) {
          sdFile.pageSize = numVal; fListGet(1); }
         else fListGet(65535);
        break; }
      sdFile.pageBegIdx += sdFile.pageSize;
      if (sdFile.pageBegIdx > sdFile.maxIdx) sdFile.pageBegIdx = 1;
      sdFile.pageSize = ((numVal > sdFile.maxIdx)? sdFile.maxIdx: numVal);
      showFileList(((sdFile.pageBegIdx + sdFile.pageSize - 1) <= sdFile.maxIdx)? sdFile.pageSize: (sdFile.maxIdx - sdFile.pageBegIdx + 1));
      break;
    case ID_PGDOWN: {
      if (ctrl & SERV_LOG) {
        len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:pgDn- beg %d pSz %d max %d\n"), sdFile.pageBegIdx, sdFile.pageSize, sdFile.maxIdx);
        netQuePut_cid(packet, len, socket.clWS_ID); }
      if (!(anchorIdx) || !(sdFile.pageBegIdx)) {
        // тут похоже список непроинициализирован
        if (ctrl & SERV_LOG) netQuePut(NULL, 0, (char*)PSTR("L:Name list rescan.\n"), socket.clWS_ID);
        fListGet(1); break; }
      sdFile.pageBegIdx += sdFile.pageSize;
      if (sdFile.pageBegIdx > sdFile.maxIdx) sdFile.pageBegIdx = 1;
      showFileList(((sdFile.pageBegIdx + sdFile.pageSize - 1) <= sdFile.maxIdx)? sdFile.pageSize: (sdFile.maxIdx - sdFile.pageBegIdx + 1));
      break; }
    case ID_SHOWPATH:
      if (sdFile.wrkPLen)
        len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:Work path: \"%s\"\n"), sdFile.wrkPath);
       else
        len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:Work path: \"/\"\n"));
      netQuePut(packet, len);
      break;
    case ID_FINDEX: {
      if (!(anchorIdx) || !(cmdID) || (cmdID > sdFile.maxIdx)) { // cmdID = индекс для поиска в списке файлов
        len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:!%d is illegal, max is %d.(%u %u)\n"),
                                                              cmdID, sdFile.maxIdx, anchorIdx, sdIsOK);
        netQuePut_cid(packet, len, socket.clWS_ID);
        break; }
      // ищем файл по заданному индексу и для команды (N) при успехе переключаем систему в SYS_PRE_PRINT
      sdFile.opCode |= 0x40000000;                      // флаги для setState()
      if (fileID(false, cmdID, (cmdMsg == (msg + length - 1))) <= 0)  // при успехе sdFile должна заполниться по индексу cmdID
                                                                      // и для команды (N) переключить систему в SYS_PRE_PRINT
        break;                                          // если поиск неудачный - возврат в loop()
      if (cmdMsg == (msg + length - 1)) {               // если была команда (N) (поиск успешный)
        // socket.fName скопировано из sdFile (setState()), socket.fSize - ?
        if (setWrkPath()) //{
          fileInfo(0);                                  // показать с маркировкой для условия (не)полного соответствия
        //  socket.clFileSel = false; }                   // сбрасываем признак выбора файла на стороне клиента
        break; }                                        // возврат в loop()
      // вместо сравнения строк будем сравнивать их числовое представление
      // открывающая скобка и число N пропускаются
      // закрывающая скобка ('0x29') идет в младший байт и дальше остальные символы - соответственно
      // "(N).",  установить такой рабочий путь, как у заданного файла (=0x2E29)
      // "(N)-!", удаление только указанного файла (=0x003D2D29)
      // "(N)-*", удаление на SD всех файлов с таким же (длинным) именем, как у указанного (=0x002A2D29)
      // "(N)-.", удаление всех файлов в папке с указанным файлом (=0x002E2D29)
      numVal = 0;                                       // все (4) нули в "строке" сравнения
      if (cmdMsg == (msg + length - 2)) {
        numVal = (cmdMsg[0] | (cmdMsg[1] << 8)) ;       // делаем из строки число (LE) для быстрого сравнения
        if (numVal != 0x2E29) {                         // ")."
          res = false; break; }                         // для такой length допустимо только 0x2E29
        // устанавливаем такой рабочий путь, как у файла, определенного индексом из команды (sdFile)
        // при этом может измениться необходимая операция для текущего выбранного файла (socket)
        // либо вообще сброс файлового контекста, если он "локальный"
        if (setWrkPath(true)) {                         // +show
          netQueSend();                                 // пробуем сбросить весь лог в сеть
          bool netEmpty = (netBufEnd == netBufBeg);     // флаг "пустоты" сетевого буфера
          sdFile.opCode |= 0x80000000;                  // запрещаем изменение указателя выбора файла для печати
          if (fileID(true, socket.fSize) <= 0) {        // ищем файл, аналогичный параметрам socket в новой раб.папке и
                                                        // если файл найден - устанавливаем SYS_PRE_PRINT и выходим в loop()
            bridgeState = SYS_IDLE;                     // здесь файл с параметрами socket в раб.папке не найден, предлагаем загрузку
            sdFile.opCode &= 0x3FFFFFFF;                // восстанавливаем указатель (при успехе поиска это делается в setState())
            if (!socket.clFileSel) {                    // удаляем имя, если оно не было выбрано на стороне клиента
              *socket.fName = 0; sdFile.opCode = 0;     // поскольку мы не можем произвести загрузку такого файла
              if (netEmpty) netBufEnd = netBufBeg;      // убираем "лишний" листинг, если есть возможность
              netQuePut(NULL, 0, (char*)PSTR("L:Name context removed.\n"), socket.clWS_ID);
              sessionID = 0; }                          // меняем контекст UX клиента
          } }
        break; }
      if (cmdMsg != (msg + length - 3)) {
        res = false; break; }                           // допустимы варианты только "(N)xx"
      numVal = (cmdMsg[0] | (cmdMsg[1] << 8) | (cmdMsg[2] << 16)) ; // делаем из строки число (LE) для быстрого сравнения
      if ((numVal != 0x00212D29) && (numVal != 0x002A2D29) && (numVal != 0x002E2D29)) {
        res = false; break; }                           // ввод не соответствует ни одному варианту - возврат
      if (confirmStr(&cmdMem, msg, length) <= 0) {      // выполняем удаление только после быстрого (<=15 сек) повторного ввода
        netQuePut(NULL, 0, (char*)PSTR("L:Repeat command to delete file(s).\n"), socket.clWS_ID);
        break; }                                        // первый ввод - выходим
      struct {
        char *fInfo, *longName, *fICurPath;
        uint8_t* lastMark;
        uint32_t pathLen, shortLen, fSize, curPathLen, pathFiNum, pathDelNum;
        } delData = { sdFile.fInfo, sdFile.longName, NULL, NULL, 
                      sdFile.pathLen, sdFile.shortLen, sdFile.fSize, 0, 0, 0 };
      sdFile.fIdx = 1; sdFile.sizeCount = 0; sdFile.keepCount = 0; // счетчики удаленных файлов/[созданных placeholders]
      bool matchFld = false;              // флаг окончания листинга "совпадающей" папки для "(N)-!" и "(N)-."
      for (char* fStrPtr = gAnswer_buf;   // проход по всему списку файлов
           ((fStrPtr < &gAnswer_buf[anchorIdx]) && (sdFile.fIdx <= sdFile.maxIdx));
           fStrPtr += (strlen(fStrPtr) + 1), sdFile.fIdx += 1) {
        int parseRC = parseFileInfo(fStrPtr); // пробуем заполнить структуру sdFile
        if (parseRC) {                        // ненулевой результат - ошибка
          if (ctrl & SERV_LOG) {
            len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:SD scan fidx %d rc %d\n"), sdFile.fIdx, parseRC);
            netQuePut_cid(packet, len, socket.clWS_ID); }
          break; }                        // досрочный выход из цикла for == признак ошибки
        bool matchFil = false;            // флаг соответствия имени файла заданному паттерну(индекс, имя, папка)
        switch (numVal) {                 //          паттерн соответствия
          case 0x00212D29:                // "(N)-!"  по заданному индексу (конкретный файл в списке)
            matchFil = (sdFile.selIdx == sdFile.fIdx);
            break;
          case 0x002A2D29:                // "(N)-*"  по заданному имени в любой папке
            if (delData.longName)
              matchFil = (!(strcmp(delData.longName, sdFile.longName)));
             else if (delData.shortLen == sdFile.shortLen)
              matchFil = (!(strncasecmp((delData.fInfo + delData.pathLen),
                                        (sdFile.fInfo + sdFile.pathLen),
                                        delData.shortLen)));
            break;
          case 0x002E2D29:                // "(N)-."  любой файл в заданной папке
            if (delData.pathLen == sdFile.pathLen)
              matchFil = ((sdFile.pathLen)? !(strncmp(delData.fInfo, sdFile.fInfo, delData.pathLen)): true);
          default:
            break;
          }
        if (matchFil)
          if (sdFile.shortLen == 12)
            if (!(strncasecmp_P((sdFile.fInfo + sdFile.pathLen), PSTR("PATHKEEP.GCO"), 12)))
              matchFil = false;
        bool samePath = (delData.curPathLen == sdFile.pathLen);
        if (samePath) samePath &= (!(memcmp(delData.fICurPath, sdFile.fInfo, sdFile.pathLen)));
        delData.fICurPath = sdFile.fInfo; delData.curPathLen = sdFile.pathLen;  // фиксируем текущий путь
        if (!samePath) {                                  // смена папки - подытожим для предыдущей папки
          if (delData.lastMark)                           // в пред. папке было(ли) совпадение(я) ?
            if (delData.pathDelNum >= delData.pathFiNum) { // после удаления файла(ов) папка останется пустой ?
              *delData.lastMark = 2; sdFile.keepCount += 1; } // перед удалением этого файла создать placeholder
          delData.lastMark = NULL; delData.pathFiNum = 0; delData.pathDelNum = 0; // очистим статистику
          if (matchFld) {                                 // закончился листинг именно "совпадающей" папки
            sdFile.fIdx = sdFile.maxIdx + 1;              // в других папках совпадений уже не будет
            break; }                                      // дальше можно не сканировать
          }
        delData.pathFiNum += 1;                           // общее кол-во файлов в текущей папке
        if (matchFil) {                                   // текущий файл соответствует шаблону выбора
          matchFld = (numVal != 0x002A2D29);              // сейчас идет сканирование "совпадающей" папки
          delData.pathDelNum += 1; sdFile.sizeCount += 1; // кол-во соответствий : в тек.папке и общее
          delData.lastMark = (uint8_t*)(sdFile.fInfo + sdFile.pathLen + sdFile.shortLen); // указатель на метку
          *delData.lastMark = 1;                          // подмена пробела после короткого имени = удалить файл
          }
        if (sdFile.fIdx == sdFile.maxIdx)                 // последняя запись в списке - подытожим для тек. папки
          if (delData.lastMark)                           // в этой папке было(ли) совпадение(я) ?
            if (delData.pathDelNum >= delData.pathFiNum) { // после удаления файла(ов) папка останется пустой ?
              *delData.lastMark = 2; sdFile.keepCount += 1; } // перед удалением этого файла создать placeholder
        } // for (char* fStrPtr = gAnswer_buf;..
      if (sdFile.fIdx > sdFile.maxIdx) {                  // был просмотрен весь список
        if (sdFile.sizeCount) {                           // общее кол-во соответствий (файлов для удаления)
          ok.setState = bridgeState = SYS_SD_ERASE;       // были помечены файлы для удаления - изменяем состояние системы
          heartbeat(true);                                // "срочный" heartbeat клиентам
          sdFile.fInfo = gAnswer_buf; sdFile.fIdx = 1;    // устанавливаем указатель и индекс в начало списка
          sdFile.pageBegIdx = sdFile.sizeCount << 16;     // перенесли кол-во удаленных файлов в старшую половину
          sdFile.keepCount <<= 16;                        // перенесли кол-во созданных файлов в старшую половину
          if (ctrl & SERV_LOG) {                          // печать статистики удалений при отладке
            len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR(
                              "L:scan: fIdx %u fMax %u\n"
                              "L:scan: -%u file(s) +%u placeholder(s)\n" ),
                              sdFile.fIdx, sdFile.maxIdx,
                              (sdFile.pageBegIdx >> 16), (sdFile.keepCount >> 16));
            netQuePut_cid(packet, len, socket.clWS_ID); }
          sdFile.sizeCount = 0;                           // будем использовать как счетчик освобожденного места
                                                          // в следующем loop() должна начать работу sd_erase()
          }
         else {
          netQuePut(NULL, 0, (char*)PSTR("L:SD scan: !Can't delete.\n"), socket.clWS_ID);
          anchorIdx = gAnswer_idx = 0;
          }
        break;                                            // весь список файлов просмотрен - выходим штатно
        }
      netQuePut(NULL, 0, (char*)PSTR("L:SD scan: !Can't get Name list.\n"), socket.clWS_ID);
      anchorIdx = gAnswer_idx = 0;                        // были ошибки - удаляем список для нового автосканирования
      break; } // case ID_FINDEX:
    case ID_DTRANS: {
      if (hasNum) {
        if (numVal) ctrl |= TRANS_LOG; else ctrl &= ~TRANS_LOG; }
      len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:,Transfer log now %d (%S).\n"),
                                      !!(ctrl & TRANS_LOG), STATUS_ON_OFF((ctrl & TRANS_LOG)));
      netQuePut_cid(packet, len, socket.clWS_ID);
      break; }
    case ID_DUART: {
      if (hasNum) {
        ctrl &= ~UART_LOG;
        if (numVal) ctrl |= (numVal & UART_LOG);}
      len = snprintf_P((char*)packet, NET_DATA_MAX,
                        PSTR("L:,UART log mask now %d (%S %S %S %S)\n"),
                        (ctrl & UART_LOG),
                        ((ctrl & UART_SEND)?  PSTR("TX") : PSTR("tx")),
                        ((ctrl & UART_READ)?  PSTR("RX") : PSTR("rx")),
                        ((ctrl & UART_COUNT)? PSTR("CNT"): PSTR("cnt")),
                        ((ctrl & UART_ACK)?   PSTR("ACK"): PSTR("ack")));
      netQuePut_cid(packet, len, socket.clWS_ID);
      break; }
    case ID_DSERV: {
      if (hasNum) {
        if (numVal) ctrl |= SERV_LOG; else ctrl &= ~SERV_LOG; }
      len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:,Service log now %d (%S).\n"),
                                      !!(ctrl & SERV_LOG), STATUS_ON_OFF((ctrl & SERV_LOG)));
      netQuePut_cid(packet, len, socket.clWS_ID);
      break; }
    case ID_DMQTT: {
      if (hasNum) {
        ctrl &= ~MQTT_LOG;
        if (numVal) ctrl |= ((numVal << 8) & MQTT_LOG);}
      len = snprintf_P((char*)packet, NET_DATA_MAX,
                        PSTR("L:,MQTT log mask now %d (%S %S %S %S)\n"),
                        ((ctrl & MQTT_LOG) >> 8),
                        ((ctrl & MQTT_SUB)?   PSTR("SUB") : PSTR("sub")),
                        ((ctrl & MQTT_SDATA)? PSTR("SRAW"): PSTR("sraw")),
                        ((ctrl & MQTT_PUB)?   PSTR("PUB") : PSTR("pub")),
                        ((ctrl & MQTT_PDATA)? PSTR("PRAW"): PSTR("praw")) );
      netQuePut_cid(packet, len, socket.clWS_ID);
      break; }
    case ID_DCLIENT: {
      if (hasNum) {
        if (numVal) ctrl |= CLIENT_LOG; else ctrl &= ~CLIENT_LOG; }
      len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:,Client log now %d (%S).\n"),
                                      !!(ctrl & CLIENT_LOG), STATUS_ON_OFF((ctrl & CLIENT_LOG)));
      netQuePut_cid(packet, len, socket.clWS_ID);
      break; }
    case ID_STAT: {
      if (hasNum) {
        if (numVal) {
          ctrl |= FULL_STAT; mtr_count = MTR_TOTAL_COUNT; stat_init(); }
         else {
          ctrl &= ~FULL_STAT; mtr_count = MTR_OFF; }
        len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:,Statistics now %d (%S).\n"),
                                      !!(ctrl & FULL_STAT), STATUS_ON_OFF((ctrl & FULL_STAT)));
        if (chunks[0].chSeq == -1011) {
          chunks[0].chSeq = 0; // если еще была инф-я о причине перезагрузки - удаляем
          len += snprintf_P((char*)(packet + len), NET_DATA_MAX - len, PSTR("L:,Reboot reason info removed.\n")); }
        netQuePut(packet, len);
        break; }
      if (chunks[0].chSeq == -1011) {
        // выдаем инф-ю о причине перезагрузки
        // до запуска первой передачи файла размер gAnswer_buf -= PACKET_BUF_SIZE
        netQuePut_cid((uint8_t*)(&gAnswer_buf[GANSWER_BUF_SIZE - PACKET_BUF_SIZE]), chunks[0].len, socket.clWS_ID);
        netQuePut(NULL, 0, (char*)PSTR("L:,Switch to stat : (P)0 or (P)1\n"), socket.clWS_ID); }
       else if (mtr_count == MTR_TOTAL_COUNT) {
        len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR(
                        "L:,- Memory usage (bytes) [min/avg/max(upT)]:\n"
                        "L:,Free RAM  : %u(%u) / %u / %u\n"
                        "L:,Free block: %u(%u) / %u / %u\n"
                        "L:,RX  queue : %u / %u / %u(%u)\n"
                        "L:,Net queue : %u / %u / %u(%u)\n"),
                        ram.min, ram.minT, ram.cur, ram.max,
                        mblk.min, mblk.minT, mblk.cur, mblk.max,
                        rxSize.min, rxSize.cur, rxSize.max, rxSize.maxT,
                        netSize.min, netSize.cur, netSize.max, netSize.maxT );
        netQuePut_cid(packet, len, socket.clWS_ID);
        len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR(
                          "L:,- Net/UART data gone (ms) [min/avg/max(upT)]:\n"
                          "L:,Unicast  : %u / %u / %u(%u)\n"
                          "L:,Broadcast: %u / %u / %u(%u) / %u drops\n"
                          "L:,OK wTime : %u / %u / %u(%u)\n"
                          "L:,parseTime: %u / %u / %u(%u)\n"),
                          ucGone.min, ucGone.cur, ucGone.max, ucGone.maxT,
                          bcGone.min, bcGone.cur, bcGone.max, bcGone.maxT, netDrops,
                          okWTime.min, okWTime.cur, okWTime.max, okWTime.maxT,
                          rxLoopTime.min, rxLoopTime.cur, rxLoopTime.max, rxLoopTime.maxT);
        netQuePut_cid(packet, len, socket.clWS_ID);
        len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR(
                          "L:,wsLoop   : %u / %u / %u(%u)\n"
                          "L:,wsSendBIN: %u / %u / %u(%u)\n"),
                          wsLoopTime.min, wsLoopTime.cur, wsLoopTime.max, wsLoopTime.maxT,
                          wsSendTime.min, wsSendTime.cur, wsSendTime.max, wsSendTime.maxT);
        netQuePut_cid(packet, len, socket.clWS_ID);
        len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR(
                          "L:,- MQTT parsing (usec)\n"
                          "L:,mqLoopTim: %u / %u / %u(%u)\n"
                          "L:,mqPktTime: %u / %u / %u(%u)\n"),
                          mqLoopTime.min, mqLoopTime.cur, mqLoopTime.max, mqLoopTime.maxT,
                          mqPktTime.min, mqPktTime.cur, mqPktTime.max, mqPktTime.maxT);
        netQuePut_cid(packet, len, socket.clWS_ID); }
       else
        netQuePut(NULL, 0, (char*)PSTR("L:,Statistics now OFF. (P)1 to ON.\n"), socket.clWS_ID);
      break; }
    case ID_DALL: {
      if (hasNum) {
        ctrl &= ~MQTT_LOG; ctrl &= ~UART_LOG;
        if (numVal) ctrl |= CLIENT_LOG; else ctrl &= ~CLIENT_LOG;
        if (numVal) ctrl |= SERV_LOG;   else ctrl &= ~SERV_LOG;
        if (numVal) ctrl |= TRANS_LOG;  else ctrl &= ~TRANS_LOG;
        if (numVal) ctrl |= ((numVal << 8) & MQTT_LOG);
        if (numVal) ctrl |= (numVal & UART_LOG);
        if (numVal) {
          ctrl |= FULL_STAT; mtr_count = MTR_TOTAL_COUNT; stat_init(); }
         else {
          ctrl &= ~FULL_STAT; mtr_count = MTR_OFF; }}
      len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR(
                        "L:,Client log now %d (%S)\n"
                        "L:,Service log now %d (%S)\n"
                        "L:,Transfer log now %d (%S)\n"
                        ),
                        !!(ctrl & CLIENT_LOG),STATUS_ON_OFF((ctrl & CLIENT_LOG)),
                        !!(ctrl & SERV_LOG),  STATUS_ON_OFF((ctrl & SERV_LOG)),
                        !!(ctrl & TRANS_LOG), STATUS_ON_OFF((ctrl & TRANS_LOG)) );
      netQuePut_cid(packet, len, socket.clWS_ID);
      len = snprintf_P((char*)packet, NET_DATA_MAX,
                        PSTR("L:,MQTT log mask now %d (%S %S %S %S)\n"),
                        ((ctrl & MQTT_LOG) >> 8),
                        ((ctrl & MQTT_SUB)?   PSTR("SUB") : PSTR("sub")),
                        ((ctrl & MQTT_SDATA)? PSTR("SRAW"): PSTR("sraw")),
                        ((ctrl & MQTT_PUB)?   PSTR("PUB") : PSTR("pub")),
                        ((ctrl & MQTT_PDATA)? PSTR("PRAW"): PSTR("praw")) );
      netQuePut_cid(packet, len, socket.clWS_ID);
      len = snprintf_P((char*)packet, NET_DATA_MAX,
                        PSTR("L:,UART log mask now %d (%S %S %S %S)\n"),
                        (ctrl & UART_LOG),
                        ((ctrl & UART_SEND)?  PSTR("TX") : PSTR("tx")),
                        ((ctrl & UART_READ)?  PSTR("RX") : PSTR("rx")),
                        ((ctrl & UART_COUNT)? PSTR("CNT"): PSTR("cnt")),
                        ((ctrl & UART_ACK)?   PSTR("ACK"): PSTR("ack")));
      netQuePut_cid(packet, len, socket.clWS_ID);
      len = snprintf_P((char*)packet, NET_DATA_MAX,
                        PSTR("L:,Statistics now %d (%S)\n"),
                        !!(ctrl & FULL_STAT),STATUS_ON_OFF((ctrl & FULL_STAT)));
      netQuePut_cid(packet, len, socket.clWS_ID);
      break; }
    case ID_REBOOT:
      if (confirmStr(&cmdMem, msg, length) <= 0) {  // отрабатываем команду только после быстрого (<=15 сек) повторного ввода
        netQuePut(NULL, 0, (char*)PSTR("L:Repeat command for reboot.\n"), socket.clWS_ID);
        break;
        }
      doReboot();
      break;
    case ID_CONFIG: {
      if (confirmStr(&cmdMem, msg, length) <= 0) {  // отрабатываем команду только после быстрого (<=15 сек) повторного ввода
        strcpy_P((char*)packet, (hasNum && (numVal))? PSTR("factory reset.\n"): PSTR("config mode.\n"));
        netQuePut(packet, strlen((char*)packet), (char*)PSTR("L:Repeat command for "), socket.clWS_ID);
        break;
        }
      const char *apMsg = PSTR("L:,1.Connect AP Marlin-Bridge-Setup\nL:,2.Browse 192.168.9.1/config\n");
      if (hasNum && (numVal)) {
        clearConfigInEEPROM();
        strcpy_P((char*)packet, PSTR("M117 WiFi factory reset!\n"));
        uartWxStop = false;       // разрешаем работу UART TX
        uart_w((char*)packet);
        strcat_P((char*)packet, apMsg);
        netQuePut(packet + 5, strlen((char*)packet) - 5, (char*)PSTR("L:"));
        netQueSend();
        delay(500);
        ESP.restart();            // Устройство уйдет в ребут и при старте включит SoftAP точку
        }
      sta2ap(apMsg);              // переключаемся в режим WIFI_AP, из программы не выходим
      break; }
    case ID_OTA: {
      if (confirmStr(&cmdMem, msg, length) <= 0) {  // выполняем переключение только после быстрого (<=15 сек) повторного ввода
        netQuePut(NULL, 0, (char*)PSTR("L:Repeat command for OTA mode.\n"), socket.clWS_ID);
        break; }                                    // первый ввод - выходим
      if (!setOTA()) break;                         // проверяем размеры файла/памяти, даем приглашение
      netQuePut(NULL, 0, (char*)PSTR("L:OTA : 120 sec waiting for upload.\n"));
      bridgeState = SYS_WAIT_OTA; otaTimer = 121;   // отсчет 2 мин ожидания начала OTA, потом - сброс
      pubProgress = true; timeProgress = millis() - 50; // "срочная" публикация "OTA_MODE"
      break; }
    case ID_HELP: {
      len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR(
                        "L:,-Log control, (*) : all, check w/o <N>\n"
                        "L:, (C)N : client log [0/1] now %d\n"
                        "L:, (S)N : service log [0/1] now %d\n"
                        "L:, (T)N : transfer log [0/1] now %d\n"
                        "L:, (M)N : mqtt log mask [0..15] now %d\n"
                        "L:, (U)N : uart log mask [0..15] now %d\n"),
                        !!(ctrl & CLIENT_LOG), !!(ctrl & SERV_LOG), !!(ctrl & TRANS_LOG),
                        ((ctrl & MQTT_LOG) >> 8), (ctrl & UART_LOG));
      netQuePut_cid(packet, len, socket.clWS_ID);
      len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR(
                        "L:, (P)N : statistics [0/1] now %d\n"
                        "L:,-Send BFT packet :\n"
                        "L:, (SYNC), (EXIT), (QUERY), (OPEN), (CLOSE), (ABORT)\n"
                        "L:,-OTA mode : (O)\n"
                        "L:,-Set config AP : (X) / Factory reset : (X)1\n"
                        "L:,-Restart server : (R)\n"),
                        !!(ctrl & FULL_STAT));
      netQuePut_cid(packet, len, socket.clWS_ID);
    case ID_ASK:
      len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR(
                        "L:,-File list:\nL:, '0' : renew list\n"
                        "L:, N : next N names, set page size (now %d)\nL:, '-' : PgUp   '+' : PgDown\n"
                        "L:,-File/Path management (N - index number):\nL:, (N)   : select file N for print\n"),
                        sdFile.pageSize);
      netQuePut_cid(packet, len, socket.clWS_ID);
      len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR(
                        "L:, '.'   : show working path\nL:, (N).  : set path by file N\n"
                        "L:, (N)-! : del file by index N\nL:, (N)-* : del ALL files by N's name\n"
                        "L:, (N)-. : del ALL files in N's folder\n"
                        "L:,-Show help|info : '?','H','V','I','P','*'\n" ));
      netQuePut_cid(packet, len, socket.clWS_ID);
      break; }
    case ID_VERSION: {
      uint32_t realFlash = ESP.getFlashChipRealSize() >> 10;
      len = snprintf_P((char*)packet, NET_DATA_MAX, 
                        PSTR( "L:,Flash (Real/Map)  : %uK / %uK\n"     // Физический/замапленный размер памяти в КБ
                              "L:,Size (Sketch/OTA) : %uK / %uK\n"     // Текущий вес прошивки/максимальный размер OTA .BIN в КБ
                              "L:,%S compiled at: %S\n"                // Вывод даты сборки в лог интерфейса
                              "L:,(c) warez4me 2026\n"),
                              realFlash, (ESP.getFlashChipSize() >> 10),
                              (ESP.getSketchSize() >> 10), (realFlash > 1024) ? (1016 - 4) : (464 - 4),
                              _VERSION_, COMPILE_TIMESTAMP);
      netQuePut_cid(packet, len, socket.clWS_ID);
      break; }
    case ID_NONE:
    default:
      res = false;
      break; }
  if (res)
    ok.waiting = false;
  return res;
}

void printStat() {
  size_t len;
  len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR(
                        "L:,%s upload finished.\n"
                        "L:,%u bytes read%S%u bytes written.\n"
                        "L:,Speed(eff.) %.2f bytes/sec.\n"),
                  ((*(socket.fName))? socket.fName: "???"),
                  socket.fSize, 
                  ((socket.fSize == fData.txTotal)? PSTR(" == "):
                    ((socket.fSize < fData.txTotal)? PSTR(" < "): PSTR(" > "))),
                  fData.txTotal, (socket.fSize * 1000.0)/(timeEnd - timeStart));
  const char *pFmt = (ctrl & FULL_STAT)?
                      PSTR("L:,Elapsed %.2f sec (Tx %.2f sec.)\n"):
                      PSTR("L:,Elapsed %.2f sec.\n");
  len += snprintf_P((char*)packet + len, NET_DATA_MAX -  len, pFmt,
                                (timeEnd - timeStart)/1000.0, chTx.sum/1000.0);
  netQuePut(packet, len);
  if (ctrl & FULL_STAT) {
    len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR(
                    "L:,%u chunks (*%u bytes).\n"
                    "L:,Chunk Tx speed %.2f bytes/sec.\n"
                    "L:,- Time (ms) [min/avg/max(upT)]:\n"
                    "L:,chTxTime        : %u / %u / %u(%u)\n"),
                chGotSeq, CHUNK_SIZE,
                (socket.fSize * 1000.0)/chTx.sum,
                chTx.min, chTx.cur, chTx.max, chTx.maxT);
    netQuePut_cid(packet, len, socket.clWS_ID);
    len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR(
                    "L:,chGoneTime      : %u / %u / %u(%u)\n"
                    "L:,chEmptyTime     : %u / %u / %u(%u)\n"
                    "L:,allEmptyTime(%u): %u / %u / %u(%u)\n"
                    "L:,%u resends in %u series(avg.%.2f)\n"),
                chGone.min, chGone.cur, chGone.max, chGone.maxT,
                chEmpty.min, chEmpty.cur, chEmpty.max, chEmpty.maxT,
                zWait.count, zWait.min, zWait.cur, zWait.max, zWait.maxT,
                fData.totalTry, fData.trySeries,
                ((fData.trySeries > 0) ? ((float)fData.totalTry / fData.trySeries): 0));
    netQuePut_cid(packet, len, socket.clWS_ID);
    }
}

void sd_erase() {
  size_t len;
  // выполнение разрешено только при bridgeState == SYS_SD_ERASE, когда получен (нет ожидания) ответ марлин
  if (ok.waiting || (ok.setState != SYS_SD_ERASE)) return;
  if (!(sdFile.fInfo)) errCode |= ERR_MEMORY_ERR;
  // начинаем от текущего значения указателя sdFile.fInfo
  while ((sdFile.fInfo < &gAnswer_buf[anchorIdx]) && (sdFile.fIdx <= sdFile.maxIdx) && (errCode == ERR_NO_ERRORS)) {
    if (ctrl & SERV_LOG) {                      // печать статистики удалений при отладке
      len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR(
                        "L:del: fIdx %u fMax %u\n"
                        "L:del: - %u / %u file | + %u / %u plhld\n" ),
                        sdFile.fIdx, sdFile.maxIdx,
                        (sdFile.pageBegIdx & 0xFFFF), (sdFile.pageBegIdx >> 16),
                        (sdFile.keepCount & 0xFFFF), (sdFile.keepCount >> 16));
      netQuePut_cid(packet, len, socket.clWS_ID); }
    sdFile.opCode &= 0xCFFFFFFF;                // сбрасываем пометку выбора
    int parseRC = parseFileInfo(sdFile.fInfo);  // пробуем заполнить структуру sdFile
    if (parseRC) {                              // ненулевой результат - ошибка
      if (ctrl & SERV_LOG) {
        len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:del fidx%d rc%u\n"), sdFile.fIdx, parseRC);
        netQuePut_cid(packet, len, socket.clWS_ID); }
      break; }                              // досрочный выход из цикла = признак ошибки
    uint32_t pLen = sdFile.pathLen + sdFile.shortLen;
    if (ctrl & SERV_LOG) {
      len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:del: op=%u pLen=%u for 0x%08lX \""),
                                                          (sdFile.opCode & 0x30000000), pLen, (uint32_t)sdFile.fInfo);
      if (pLen) {
        memcpy(packet + len, (uint8_t*)sdFile.fInfo, pLen); len += pLen; }
      packet[len++] = '\"'; packet[len++] = '\n';
      netQuePut_cid(packet, len, socket.clWS_ID); }
    switch (sdFile.opCode & 0x30000000) {
      case 0x10000000:                      // файл был помечен на удаление
        bridgeState = SYS_WAIT_M30;
        ok.waiting = true; ok.skip = 0; ok.setState = SYS_SD_ERASE; ok.wdTimer = ok.wdLoad = WAIT_OK_TIMEOUT; // 5 sec
        txTimer = 2;                        // включаем задержку ~250 мсек выдачи команды в UART
        uart_w((char*)PSTR("M30 \""));      // удаление файла с указанием короткого имени
        uart_w(sdFile.fInfo, pLen);
        uart_w((char*)PSTR("\"\n"));        // последовательность : M30 -> OK
        return;                             // выходим из цикла до прихода завершающего последовательность "ОК" от марлин
      case 0x20000000:                      // файл был помечен на удаление с предварительным созданием файла-placeholder
        bridgeState = SYS_WAIT_M28;
        ok.waiting = true; ok.skip = 0; ok.setState = SYS_SD_ERASE; ok.wdTimer = ok.wdLoad = WAIT_OK_TIMEOUT; // 5 sec
        txTimer = 2;                        // включаем задержку ~250 мсек выдачи команды в UART
        uart_w((char*)PSTR("M28 \""));      // создание файла с указанием короткого имени
        if (sdFile.pathLen) uart_w(sdFile.fInfo, sdFile.pathLen);
        uart_w((char*)PSTR("PATHKEEP.GCO\"\n")); // последовательность : M28 -> OK -> M29 -> OK -> M30 -> OK
        return;                             // выходим из цикла до прихода завершающего последовательность "ОК" от марлин
      default:
        sdFile.fInfo += (strlen(sdFile.fInfo) + 1); sdFile.fIdx += 1;
        continue;                           // итерация
    } }
  if (sdFile.fIdx > sdFile.maxIdx) {        // был просмотрен весь список
    len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:%u/%u file(s) removed.\n"),
                                                          (sdFile.pageBegIdx & 0xFFFF),
                                                          (sdFile.pageBegIdx >> 16));
    netQuePut(packet, len);
    if (sdFile.sizeCount <= 99999)
      len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:%u bytes freed.\n"), sdFile.sizeCount);
     else
      len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:%u Kb freed.\n"), (sdFile.sizeCount >> 10));
    if (ctrl & (UART_LOG | SERV_LOG)) {
      netQuePut(packet, len);
      len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:%d/%d placeholder(s) created.\n"),
                                                      (sdFile.keepCount & 0xFFFF),
                                                      (sdFile.keepCount >> 16));
      }
    netQuePut(packet, len);
    }
   else
    netQuePut(NULL, 0, (errCode == ERR_NO_ERRORS)?
                        (char*)PSTR("L:!SD erase: Can't get Name list.\n"):
                        (char*)PSTR("L:!Error while deleting files.\n"), 
              socket.clWS_ID);
  bridgeState = SYS_IDLE;
  anchorIdx = gAnswer_idx = 0;
}
