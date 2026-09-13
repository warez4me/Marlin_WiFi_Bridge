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

void updFloatValue(gAnswerID_t prmID, uint32_t updTime, float val1, float val2) {
  // Раскладываем float значаения параметров по полочкам для MQTT
  switch (prmID) {
    case ID_TEMP_T:
      gData.currentHotend = val1; gData.targetHotend = val2; pubTB = true; timeTB = updTime;
      break;
    case ID_TEMP_B:
      gData.currentBed = val1; gData.targetBed = val2; pubTB = true; timeTB = updTime;
      break;
    case ID_POS_X:
      gData.xPos = val1; pubXYZ = true; timeXYZ = updTime;
      break;
    case ID_POS_Y:
      gData.yPos = val1; pubXYZ = true; timeXYZ = updTime;
      break;
    case ID_POS_Z:
      gData.zPos = val1; pubXYZ = true; timeXYZ = updTime;
      break;
    case ID_POS_E:
      gData.ePos = val1; pubXYZ = true; timeXYZ = updTime;
      break;
    default:
      return;
    }
  if (ctrl & MQTT_PDATA) {
    size_t len;
    if (prmID < ID_POS_X)
      len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:<\"%S %.2f / %.2f\"\n"),
                                          (char*)pgm_read_ptr(&gAnswer[prmID].idWord), val1, val2);
     else
      len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:<\"%S %.2f\"\n"),
                                          (char*)pgm_read_ptr(&gAnswer[prmID].idWord), val1);
    netQuePut_cid(packet, len, socket.clWS_ID);
    }
  return;
}

void updIntValue(gAnswerID_t prmID, uint32_t updTime, uint32_t val1, uint32_t val2) {
  // Раскладываем целочисленные значения параметров по полочкам для MQTT
  switch (prmID) {
    case ID_SXYZ:
      gData.sxyzCode = val1; pubXYZ = true; timeXYZ = updTime;
      break;
    case ID_XSTEP:
      gData.xStep = val1; pubXYZ = true; timeXYZ = updTime;
      break;
    case ID_YSTEP:
      gData.yStep = val1; pubXYZ = true; timeXYZ = updTime;
      break;
    case ID_ZSTEP:
      gData.zStep = val1; pubXYZ = true; timeXYZ = updTime;
      break;
    case ID_SD_PROGRESS: {
      if (bridgeState == SYS_PRINT) { socket.progress = val1; socket.fSize = val2; }
      uint32_t pgs = ((val2)?
                      ((val1 < val2)? ((uint32_t)((uint64_t)(val1 * 100) / val2)): 100):
                      0);
      if (gData.progress == pgs) break;
      gData.progress = pgs; pubProgress = true; timeProgress = updTime; gData.pubArea &= 0xFFFF0000;
      break; }
    default:
      return;
    }
  if (ctrl & MQTT_PDATA) {
    size_t len;
    if (prmID == ID_SD_PROGRESS)
      len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:<\"%S %ld / %ld\"\n"),
                                          (char*)pgm_read_ptr(&gAnswer[prmID].idWord), val1, val2);
     else if (prmID == ID_SXYZ)
      len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:<\"%S %ld\"\n"),
                                          (char*)pgm_read_ptr(&gAnswer[prmID].idWord), val1);
     else
      len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:<\"%S %ld\" (s)\n"),
                                          (char*)pgm_read_ptr(&gAnswer[prmID].idWord), val1);
    netQuePut_cid(packet, len, socket.clWS_ID);
    }
  return;
}

void prnFix(int src, bool prnDetected) {
  // переключение системы SYS_IDLE <-> SYS_PRINT
  int len = 0;
  if (ctrl & SERV_LOG) {
    len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:prnDetected(%d, %d)\n"), src, prnDetected);
    netQuePut_cid(packet, len, socket.clWS_ID); }  // urgent HeartBeat -> 
  if ((bridgeState != SYS_PRINT) && (bridgeState != SYS_WAIT_M23) && !prnDetected) return;
  // если prnDetected - включение, если нет - отключение автоотчетов координат, температуры, прогресса
  uart_w(((prnDetected)? ((char*)PSTR("M155 S7\nM154 S3\nM27 S11\n")): ((char*)PSTR("M155 S0\nM154 S0\nM27 S0\n"))));
  if (bridgeState == SYS_WAIT_M23) {
    if (prnDetected) {
      ok.waiting = true; ok.skip = 2; ok.wdTimer = ok.wdLoad = WAIT_OK_TIMEOUT; // 5 sec
      ok.setState = SYS_PRINT; }
     else {
      uart_w((char*)PSTR("M26 S0\n"));
      ok.waiting = false; ok.skip = 0; ok.wdTimer = ok.wdLoad = 0;
      ok.setState = SYS_IDLE; bridgeState = SYS_IDLE; }
    }
   else
    bridgeState = ((prnDetected)? SYS_PRINT: SYS_IDLE);
  socket.progress = 0; socket.fSize = 0;
  if (!(ok.ack_status & ACK_SELECTED) || !prnDetected) {
    socket.fName[0] = '\0'; }
  sdFile.selName = 0; sdFile.selIdx = 0; ok.ack_status &= ~ACK_SELECTED;
  sdFile.fInfo = NULL;        // сбрасываем указатель на запись о файле
  sdFile.pageBegIdx = 0;      // сбрасываем индекс начала страницы показа
  anchorIdx = gAnswer_idx = 0;
  pubState = ((bridgeState == SYS_PRINT)? HB_PRINT: HB_WAIT);
  len = snprintf_P((char*)packet, NET_DATA_MAX,
                        PSTR("S:0:%d:%d:%d\nS:0:%d:%d:%d\nS:0:%d:%d:%d\nL:Print process %S\n"),
                        HB_WAIT,
                        (socket.clGroup_ID & 0xFFFF) + 1,        // +(сбросить активиста)
                        ((ctrl & CLIENT_LOG) >> 4),
                        HB_WAIT, (socket.clGroup_ID & 0xFFFF) + 1, ((ctrl & CLIENT_LOG) >> 4),
                        pubState,
                        (socket.clGroup_ID & 0xFFFF) + 1, ((ctrl & CLIENT_LOG) >> 4),
                        ((prnDetected)? PSTR("detected"): PSTR("terminated")));
  netQuePut(packet, -len);    // urgent HeartBeat
}

int parseFileInfo(char* if_ptr) {
  // строка содержит "[path]<shortFName><?><fSize>[' '<lohgFName>]"
  // где <?> - пробел либо символ '\1' или '\2' как маркер выбора для операции с этим файлом
  if (!(if_ptr)) return 1;                // недопустимый указатель - это не строка с именем файла
  if (*if_ptr < '0') return 2;            // недопустимый символ в начале - это не строка с именем файла
  sdFile.fInfo = NULL;                    // сбрасываем указатель на начало информационной строки
  char* p_ptr = NULL;
  while (!(p_ptr)) {
    p_ptr = strchr(if_ptr, '\1');
    if (p_ptr) { sdFile.opCode |= 0x10000000; break; } // файл помечен для удаления
    p_ptr = strchr(if_ptr, '\2');
    if (p_ptr) { sdFile.opCode |= 0x20000000; break; } // файл помечен для удаления + создания "пустышки" в этой папке
    p_ptr = strchr(if_ptr, '\x20');
    if (!(p_ptr)) return 3;               // перед размером нет ни пробела, ни спец.пометок - ошибка
    }
  char* n_ptr = p_ptr++;                  // слева - path+short_name, справа - размер, пробел, длинное имя
  if (!getNum(&sdFile.fSize, &p_ptr, 0xFFFFFFFF, true))
    return 4;                             // не удалось прочитать размер - это не строка с именем файла
  // здесь уже точно понятно, что строка содержит полезную инфу и можно начать заполнять структуру sdFile
  sdFile.fInfo = if_ptr;                  // указатель на начало информационной строки
  sdFile.pathLen = sdFile.shortLen = n_ptr - if_ptr; // длина короткого имени вместе с путем
  while (*p_ptr == '\x20') p_ptr++;       // пропускаем пробел(ы) и '/' после размера
  while (*p_ptr == '/') p_ptr++;          // если (*p_ptr != '\0'), то он указывает на начало длинного имени
  sdFile.longName = (*p_ptr)? p_ptr: NULL; // если (*p_ptr == '\0'), то длинное имя в этой записи отсутствует
  while ((sdFile.pathLen) && (*n_ptr != '/')) { sdFile.pathLen--; n_ptr--; } // ищем разделитель в "path/короткое_имя"
  if (*n_ptr == '/') sdFile.pathLen += 1; // длина path
  sdFile.shortLen -= sdFile.pathLen;      // длина короткого имени
  return 0;
}

inline bool isWrkPath() {
  bool res = false;
  if (sdFile.pathLen == sdFile.wrkPLen) {
    if (sdFile.pathLen == 0)
      res = true;
     else if (!(memcmp(sdFile.fInfo, sdFile.wrkPath, sdFile.pathLen)))
      res = true;
    }
  return res;
}

void fListInfo() {
  if (bridgeState >= SYS_PRINT) return;
  // показ строки навигации после вывода страницы списка файлов SD
  // вычисляются индексы, которые будут показаны при листании PgUp/PgDown
  int len = 0;
  uint32_t upIdx, dwnIdx;
  if (sdFile.pageBegIdx > 1) {
    upIdx = (sdFile.pageBegIdx > sdFile.pageSize)? (sdFile.pageBegIdx - sdFile.pageSize): 1;
    dwnIdx = sdFile.pageBegIdx - 1; }
   else {
    upIdx = sdFile.maxIdx - sdFile.pageSize + 1;
    dwnIdx = sdFile.maxIdx; }
  if (upIdx > sdFile.maxIdx) upIdx = 1;
  if (dwnIdx > sdFile.maxIdx) dwnIdx = sdFile.maxIdx;
  if ((upIdx == 1) && (dwnIdx == sdFile.maxIdx))
    len = snprintf_P((char*)packet, NET_DATA_MAX - 3, PSTR("SD: %u (%u) files, %u folders\n"),
                                                      sdFile.sizeCount, sdFile.maxIdx, sdFile.keepCount);
   else {
    if (upIdx == dwnIdx)
      len = snprintf_P((char*)packet, NET_DATA_MAX - 3, PSTR("'-' for %u"), upIdx);
     else
      len = snprintf_P((char*)packet, NET_DATA_MAX - 3, PSTR("'-' for %u..%u"), upIdx, dwnIdx);
    uint32_t lastShown = sdFile.pageBegIdx + sdFile.pageSize - 1;
    if (lastShown > sdFile.maxIdx) lastShown = sdFile.maxIdx;
    if (lastShown < sdFile.maxIdx) {
      upIdx = lastShown + 1;
      dwnIdx = lastShown + sdFile.pageSize; }
     else {
      upIdx = 1;
      dwnIdx = sdFile.pageSize; }
    if (upIdx > sdFile.maxIdx) upIdx = 1;
    if (dwnIdx > sdFile.maxIdx) dwnIdx = sdFile.maxIdx;
    if (upIdx == dwnIdx)
      len += snprintf_P((char*)packet + len, NET_DATA_MAX - len -3, PSTR(" / '+' for %u (of %u)\n"),
                                                                      upIdx, sdFile.maxIdx);
     else
      len += snprintf_P((char*)packet + len, NET_DATA_MAX - len -3, PSTR(" / '+' for %u..%u (of %u)\n"),
                                                                      upIdx, dwnIdx, sdFile.maxIdx);
    }
  netQuePut(packet, len, (char*)PSTR("L:"), socket.clWS_ID);
}

int fileID(bool nameSearch, uint32_t fIdxSize, bool setPrint) {
  // поиск файла(ов) в списке по набору критериев
  // nameSearch определяет режим поиска - по индексу либо по имени с учетом или без учета размера
  // при nameSearch=false происходит поиск записи с индексом fIdxSize и ее парсинг в структуру sdFile
  // при nameSearch=true сравнение производится с socket.fName и socket.fSize
  // в этом случае ненулевой fIdxSize задает размер для сравнивания, а нулевой определяет сравнение только по имени
  // в результате, если в рабочей папке найдется подходящий файл, последует рекурсивный вызов с его индексом для
  // поиска по индексу и парсинга в структуру sdFile
  // для файла, найденного по индексу при setPrint=true происходит переключение системы в состояние SYS_PRE_PRINT
  size_t len;
  sdFile.fIdx = 0; sdFile.selIdx = 0; sdFile.selNum = 0;
  for (char* fStrPtr = gAnswer_buf; fStrPtr < &gAnswer_buf[anchorIdx]; fStrPtr += (strlen(fStrPtr) + 1)) {
    // строка содержит "[path]<shortFName>' '<fSize>[' '<lohgFName>]"
    if (parseFileInfo(fStrPtr)) {   // пробуем заполнить структуру sdFile (ненулевой результат - ошибка)
      // здесь - парсинг не удался
      netQuePut(NULL, 0, (char*)PSTR("L:!Name list parse error.\n"), socket.clWS_ID);
      anchorIdx = gAnswer_idx = 0;  // сброс флага валидности списка имен (перечитать список файлов)
      break; }                      // выход из цикла
    if (!((++sdFile.fIdx) & 0xF)) { ESP.wdtFeed(); yield(); }
    if (!nameSearch) {
      // в этом режиме - поиск по заданному индексу
      if (sdFile.fIdx < fIdxSize)
        continue;
      sdFile.selIdx = sdFile.fIdx;
      if (setPrint) setState();
      return sdFile.selIdx;         // при успехе поиска по индексу - возврат раньше конца цикла
                                    // достижение конца цикла == индекс не найден, перечитать список
      }
    //здесь параметры поиска задают имя и размер
    sdFile.selNum += ((fileInfo(1))? 1: 0); // подсчет совпадений по имени, фиксация sdFile.selIdx при совпадении размера
    } // for (char* fStrPtr = gAnswer_buf;
  if (!nameSearch || !(anchorIdx)) {        // если !nameSearch - произошла ошибка парсинга либо индекс вне границ буфера
    // необходимо перечитать список файлов
    if (!nameSearch) {
      if (ctrl & SERV_LOG)
        netQuePut(NULL, 0, (char*)PSTR("L:!Index out of list.\n"), socket.clWS_ID);
      //sdFile.selIdx = 0;            // после поиска по индексу - только перечитать список файлов
      }
     else
      sdFile.selIdx = fIdxSize;     // != 0 - учитывать размер при поиске в получаемом списке файлов
    if (ok.setState == SYS_WAIT_M20) {
      sdFile.selName = nameSearch; ok.setState = SYS_IDLE;
      // получить список файлов с заданным режимом поиска
      ok.skip = 1; ok.waiting = false; fListWait = true; fListTOut = WAIT_FLIST_TIMEOUT;
      len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("M21\nM20 L \"%s\"\n"),
                                            ((sdFile.wrkPLen)? sdFile.wrkPath: "/"));
      uart_w((char*)packet, len); }
    return -1;
    }
  // после цикла поиска по имени всегда здесь
  if (sdFile.selNum) {
    len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:Name found in %d folder(s).\n"), sdFile.selNum);
    if (socket.clFileSel)     // если в socket содержится "эталонная" инфа после выбора на диске клиента
      len += (snprintf_P((char*)packet + (len - 1), NET_DATA_MAX - (len - 1), PSTR("  Orig.size %u%S.\n"),
                                          (socket.fSize > 99999)? (socket.fSize >> 10): socket.fSize,
                                          (socket.fSize > 99999)?  PSTR(" KB"): PSTR(" b")) - 1);
    netQuePut_cid(packet, len, socket.clWS_ID); }
  ok.setState = SYS_IDLE;           // запрет рекурсии M20
  if (sdFile.selIdx)
    return fileID(false, sdFile.selIdx, setPrint);
  return sdFile.selIdx;
}

int cmpFPath() {
  int res;
  // сравниваем с socket длинное имя, а при его отсутствии - короткое имя
  if (sdFile.longName)
    res = -(!!(strcmp((const char*)sdFile.longName, (const char*)socket.fName)));
  if ((res) && (sdFile.shortLen == strlen(socket.fName)))
    res = -(!!(strncasecmp((const char*)(sdFile.fInfo + sdFile.pathLen),
                      (const char*)socket.fName,
                      sdFile.shortLen)));
  if (!(res))                          // имя совпадает с сокетом
    if (isWrkPath()) res =1;           // path совпадает с рабочим
  return res;
}

bool fileInfo(int mode, bool show) {
  // для показа строки с информацией о файле (+ о папке)
  // слева - направо
  // - маркер (не)равенства размера при совпадении имени файла и socket.fName
  // - индекс в круглых скобках для обычного файла либо в квадратных для файла, выбранного к печати
  // - точка как маркер того, что файл находится в рабочей папке
  // - имя файла с расширением - длинное, если есть, либо короткое
  // - размер файла на SD (после ASCII загрузки может отличаться от оригинального, после BFT - идентичен)
  // show управляет показом в принципе.
  // mode=-1 : если show=true, то показывать без доп.строки path и не трогать sdFile.selIdx
  // mode>=0 : если show=true, то показывать с доп.строкой path при совпадении имени
  //                            + помечать sdFile.selIdx по условию ->
  // mode=0  : помечать sdFile.selIdx независимо от размера при совпадении path с рабочим
  // mode>0  : помечать sdFile.selIdx при совпадении размера и path с рабочим 
  int cmp_res = -1;
  char sizeMark = ' ';
  if ((*socket.fName) && (socket.fSize)) {
    cmp_res = cmpFPath();
    if (cmp_res >= 0) {                 // имя совпадает с сокетом
      if (cmp_res > 0) {                // path совпадает с рабочим
        if (!mode)
          sdFile.selIdx = sdFile.fIdx;  // выбираем независимо от размера по совпадению имени
         else if ((mode > 0) && ((sdFile.fSize == socket.fSize) || ((sdFile.opCode & 0x0FFFFFFF) == csFInfo(sdFile.fInfo))))
          sdFile.selIdx = sdFile.fIdx;  // критерий выбора - совпадение имя + размер
        }
      sizeMark = ((socket.fSize == sdFile.fSize) && socket.clFileSel)? '=': '~';
    } }
  bool res = (cmp_res >= 0);
  if ((!res && (mode >= 0)) || !show) return res;
  uint32_t len = 0;
  // 0. если mode >= 0 - сначала выводим строку path при его наличии
  if (mode >= 0) {        // режим с выводом строки path
    strcpy_P((char*)packet, PSTR("L:>     "));  len = 8;
    if (sdFile.pathLen > (NET_DATA_MAX - (len + 4))) {
      packet[len++] = '.'; packet[len++] = '.';
      memcpy(packet + len, sdFile.fInfo + (sdFile.pathLen - (NET_DATA_MAX - (len + 2))), (NET_DATA_MAX - (len + 2)));
      packet[NET_DATA_MAX - 2] = '\n'; packet[NET_DATA_MAX - 1] = 0; len = NET_DATA_MAX - 1; }
     else if (!(sdFile.pathLen)) {
      packet[len++] = '/'; packet[len++] = '\n'; packet[len] = 0; }
     else {
      memcpy(packet + len, sdFile.fInfo, sdFile.pathLen); len += sdFile.pathLen;
      packet[len++] = '\n'; packet[len] = 0; }
    netQuePut_cid(packet, len, socket.clWS_ID);
    }
  // 1. в начале строки формируем: метку совпадения размера, индекс (выравнивание вправо), метку рабочей папки
  uint brcIdx = !!((isWrkPath() && (csFInfo(sdFile.fInfo) == (sdFile.opCode & 0x0FFFFFFF))) || (res && (sdFile.selIdx == sdFile.fIdx)));
  if (!(brcIdx) && res && isWrkPath()) brcIdx = 2;
    if (ctrl & SERV_LOG) {
      size_t len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:cS=%u wP=%u fI=0x%08lX%soC=0x%08lX res=%u sX=%u%sfX=%u\n"),
                                                                  socket.clFileSel, isWrkPath(), csFInfo(sdFile.fInfo),
                                                                  ((csFInfo(sdFile.fInfo) == (sdFile.opCode & 0x0FFFFFFF))? "==": "  "),
                                                                  (sdFile.opCode & 0x0FFFFFFF), res, sdFile.selIdx,
                                                                  ((sdFile.selIdx == sdFile.fIdx)? "==": "  "), sdFile.fIdx);
      netQuePut_cid(packet, len, socket.clWS_ID); }
  const char *snpFmt, *brc[3] = {"()", "[]", "--"}, *p_brc = brc[brcIdx];
  char colorMark = ((sizeMark != ' ')? ((brcIdx)? '<': '>'): ','), wrkPMark = ((isWrkPath())? '.': ' ');
  if       (sdFile.fIdx < 10)  snpFmt = PSTR("L:%c%c  %c%d%c%c");
   else if (sdFile.fIdx < 100) snpFmt = PSTR("L:%c%c %c%d%c%c");
   else                        snpFmt = PSTR("L:%c%c%c%d%c%c");
  char* cursor = (char*)packet;
  len = snprintf_P(cursor, 12, snpFmt, colorMark, sizeMark, p_brc[0], sdFile.fIdx, p_brc[1], wrkPMark); // начало строки - в выходной буфер
  cursor += len;          // текущее смещение от начала выходного буфера 
  // 2. Определяем актуальное имя и его длину
  char* namePtr;
  uint32_t nameLen;
  if (sdFile.longName) {
    namePtr = sdFile.longName; nameLen = strlen(namePtr); }
   else {
    namePtr = sdFile.fInfo + sdFile.pathLen; nameLen = sdFile.shortLen; }
  uint32_t spare = NET_DATA_MAX - (len + 1 + 2 + snprintf((char*)netSendBuf, 16, "%u", sdFile.fSize)); // индекс, пробел, \n\0, размер
  if (nameLen > spare) {
    // получится показать только часть имени и размер
    memcpy(cursor, namePtr, spare - 2); cursor += (spare - 2);
    *cursor++ = '.'; *cursor++ = '.'; }
   else if (nameLen == spare) {
    // получится показать только имя и размер
    memcpy(cursor, namePtr, nameLen); cursor += nameLen; }
   else {
    if (mode < 0) {       // строка с папкой не выводилась
      spare -= nameLen;   // имя влазит - можно поместить path
      // 3. Копируем path (если он есть и если есть место для этого)
      if ((sdFile.pathLen > 0) && (spare >= 3)) {
        if (sdFile.pathLen > spare) {
          memcpy(cursor, sdFile.fInfo, spare - 3); cursor += (spare - 3);
          *cursor++ = '.'; *cursor++ = '.'; *cursor++ = '/'; }
         else {
          memcpy(cursor, sdFile.fInfo, sdFile.pathLen);
          cursor += sdFile.pathLen;
      } } }
    // 4. Копируем актуальное имя - здесь уже без проверок
    memcpy(cursor, namePtr, nameLen);
    cursor += nameLen; }
  *cursor++ = '\x20'; // пробел после имени
  // 5. Размер
  if (sdFile.fSize > 99999) {
    ultoa((sdFile.fSize >> 10), cursor, 10); // Быстрое деление на 1024
    cursor += strlen(cursor);
    *cursor++ = 'K'; *cursor++ = 'B'; }
   else {
    ultoa(sdFile.fSize, cursor, 10);
    cursor += strlen(cursor);
    *cursor++ = 'b'; } 
  // 6. Завершаем строку
  *cursor++ = '\n'; *cursor = '\0';
  // 7. Отправляем в очередь
  netQuePut_cid(packet, cursor - (char*)packet, socket.clWS_ID);
  return res;
}

void showFileList(uint32_t toShow) {
  sdFile.fIdx = 0;
  char* fStrPtr = gAnswer_buf;
  if (!(toShow)) toShow = sdFile.pageSize;
  for (; (sdFile.fIdx < (sdFile.pageBegIdx + toShow - 1)) && (fStrPtr < &gAnswer_buf[anchorIdx]);
          fStrPtr += (strlen(fStrPtr) + 1)) {
    if (++sdFile.fIdx < sdFile.pageBegIdx) continue;
    // строка содержит "[path]<shortFName>' '<fSize>[' '<lohgFName>]"
    if (parseFileInfo(fStrPtr)) {       // пробуем заполнить структуру sdFile (ненулевой результат - ошибка)
      // текущая строка не соответствует заданной структуре, делаем повторный запрос, индекс начала показа остается
      netQuePut(NULL, 0, (char*)PSTR("L:sh- !Name list corrupted. Rescan.\n"), socket.clWS_ID);
      fListGet(-1);                     // получаем список, параметры показа не трогаем
      return; }
    if (!(sdFile.fIdx & 0xF)) { ESP.wdtFeed(); yield(); }
    fileInfo(); }
  size_t len = 0;
  if (ctrl & SERV_LOG) {
    len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:shFL ptr%ca%d i%d b%d p%d x%d\n"),
              ((fStrPtr > &gAnswer_buf[anchorIdx])? '>': ((fStrPtr < &gAnswer_buf[anchorIdx])? '<': '=')),
                                                anchorIdx, sdFile.fIdx,
                                                sdFile.pageBegIdx, sdFile.pageSize, sdFile.maxIdx);
    netQuePut_cid(packet, len, socket.clWS_ID); }
  fListInfo();                          // выдать строку навигации по списку файлов
}

inline uint32_t csFInfo(char* fInfo) {
  uint16_t csLen = (!(fInfo) || !(*fInfo))? 0: strlen(fInfo);
  return ((csLen)? ((csLen << 16) | xorCS(fInfo, csLen)): 0);
}

/*----------------------------*/
/*-------- Log manage --------*/
/*----------------------------*/
void logMsg(unsigned char mark) {
  if (!(gDataPtr) || !(*gDataPtr)) return;
  static strMem_t lMem = {0, 0, 0};
  // указатель gDataPtr находится в пределах строки, ограниченной '\0'
  // индексы anchorIdx и gAnswer_idx при этом уже могут быть сброшены в 0
  // поэтому нужно найти начало и конец строки
  char* l_ptr = gDataPtr;
  char* e_ptr = (char*)memchr(l_ptr, 0, NET_DATA_MAX);  // ищем '\0' терминатор на разумной длине
  if (!(e_ptr)) return;
  while ((l_ptr > gAnswer_buf) && (*(l_ptr - 1))) l_ptr -= 1;  // ищем начало строки
  // для одинаковых строк ставим признак повторения в интервале 0.5 сек. - 10 мин.
  uint32_t mLen = e_ptr - l_ptr;
  uint16_t xCS = xorCS(l_ptr, mLen);
  uint32_t tLog = millis();
  bool diff = true, dt = false;
  if ((lMem.xCS == xCS) && (lMem.mLen == mLen)) {
    dt = (((tLog - lMem.tLog) < 500) || ((tLog - lMem.tLog) > 600000));
    diff = dt; }
  int len = 0;
  lMem.xCS = xCS; lMem.mLen = mLen;
  lMem.tLog = tLog;
  if (diff || (ctrl & UART_READ) || (ctrl & UART_ACK)) {  // лог, если не повтор или включена отладка
    if (mLen >= (NET_DATA_MAX - 3)) l_ptr[NET_DATA_MAX - 4] = '\x0'; // гарантируем допустимую длину
    uint32_t m_str = (uint32_t)mark;
    len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:%s%s\n"), (char*)&m_str, l_ptr);
    netQuePut(packet, len);
    }
}

inline uint16_t xorCS(char* xStr, size_t len) {
  if (!(xStr)) return 0;
  uint8_t xCS = 0, sCS = 0;
  if (!(len))
    while ((*xStr) && (*xStr != '\n')) {
      sCS += *xStr; xCS ^= *xStr++; }
   else
    for (size_t i = 0; i < len; i++) {
      sCS += *xStr; xCS ^= *xStr++; }
  return (sCS + ((uint16_t)xCS << 8));
}

/*-------------------------------*/
/*--- Marlin(UART) read/parse ---*/
/*-------------------------------*/
inline uint32_t sdCS(uint32_t csWord) {
  for (uint32_t i = anchorIdx; i < gAnswer_idx; i++) {
    csWord += (uint32_t)gAnswer_buf[i];
    uint32_t xCS = (((uint32_t)gAnswer_buf[i] << 20) & 0xffff0000) | (((uint32_t)(~gAnswer_buf[i]) << 10) & 0x0000ffff);
    csWord ^= xCS;
    }
  return csWord;
}

void doResend() {
  timeResend = 0;
  fData.totalTry += 1;
  if (fData.txTry == 0) fData.trySeries += 1;
}

gAnswerID_t findID(char** dataStr, uint* dataID, bool orNum) {
  /*
        |   -   |   +   |  isdigit() в начале строки
  orNum +-------+-------+
      - | while | none  |
      + | while |getnum |*/
  uint32_t dataLen = ((*dataStr) && (**dataStr))? strlen(*dataStr): 0;
  if (!(dataLen) || !(dataID)) return ID_NONE;
  // 1. Если строка начинается с ЦИФРЫ (столбец "+" в таблице)
  // при наличии разрешения (orNum == true) результат будет зависеть от getNum()
  // при отсутствии разрешения orNum (orNum == false) возвращаем результат ID_NONE
  if (isdigit(**dataStr))
    return ((orNum)?
            (getNum(dataID, dataStr, 999, true)? ID_FINDEX : ID_NONE): // Строка "+", столбец "+" -> только getNum
            ID_NONE);                                                  // Строка "-", столбец "+" -> none
  // 2. Если строка начинается НЕ с цифры (столбец "-" в таблице)
  // Здесь мы крутим цикл while независимо от значения orNum (строки "+" и "-")
  // сравниваем начало строки с шаблоном, последовательно подставляем шаблоны из массива
  while (true) {                            // Крутим, пока не найдем совпадение с шаблоном или встретим стоп-маркер
    char* id_ptr = (char*)pgm_read_ptr(&gAnswer[*dataID].idWord);
    uint32_t currentIdLen = pgm_read_dword(&gAnswer[*dataID].idLen);
    if (!(currentIdLen) || !(id_ptr)) break;// Стоп-маркер. Выходим из цикла.
    if (dataLen >= currentIdLen)
      if (strncasecmp_P(*dataStr, id_ptr, currentIdLen) == 0) {
        *dataStr += currentIdLen;           // при успехе сдвигаем, пропускаем идентификатор в строке
        return (gAnswerID_t)(*dataID);      // возвращаем идентификатор, соответствующий найденному шаблону
        }
    *dataID += 1;
    }
  // Если цикл while открутился и ничего не нашёл, то независимо от значения orNum - результат ID_NONE. 
  return ID_NONE; 
}

void getMarlin() {
  // --- Марлин: Чтение из UART, парсинг ответов и логика подтверждений ---
  if (timeResend)                           // ненулевое время фиксации прихода запроса RESEND
    if ((millis() - timeResend) > 20)       // если за 20 мсек не было получено ни одной строки от Марлин
      doResend();                           // отрабатываем "висящий" запрос RESEND
  getMarlinRXTime = 0;                      // сбрасываем время начала парсинга, ненулевое значение на выходе
                                            // служит признаком наличия RX потока и точкой отсчета для статистики
  size_t len = 0;
  uint uartSymLin = 0, uartSymAll = 0, uartEOL = 0;
  gDataPtr = NULL;
  unsigned char logMark = '\xff';           // флаг необходимости вывода в лог, а также цветовой маркер
  BridgeState_t inState = bridgeState;
  static uint32_t gBufSizeNow = 0;
  enum drain_t {DRAIN_NONE = 0, DRAIN_FLAG, DRAIN_BREAK};
  drain_t drain = (drain_t)(!!(gAnswer_idx != anchorIdx));  // если индексы не равны - находимся в процессе приема строки ответа
                                                            // присваиваем DRAIN_NONE либо DRAIN_FLAG
  // При передаче файлов выделяются 4 буфера в конце gAnswer_buf (4*1230)
  // также, функция setup() заносит в конец gAnswer_buf информацию о причине перезагрузки для просмотра по команде (P)
  // поэтому размер буфера для приема листинга файлов и ответов марлин иногда нужно изменять в соответствии с набором условий
  uint32_t gBufSizeLim = ((((bridgeState == SYS_WAIT_M28) && (ok.setState != SYS_SD_ERASE)) ||
                           (bridgeState == SYS_TRANSFER) || (bridgeState == SYS_WAIT_OTA) || (bridgeState == SYS_OTA))?
                          (GANSWER_BUF_SIZE - (NUM_CHUNKS * CHUNK_SIZE)):
                          ((chunks[0].chSeq == -1011)? (GANSWER_BUF_SIZE - PACKET_BUF_SIZE): GANSWER_BUF_SIZE));
  if (gBufSizeNow != gBufSizeLim) {             // нужно изменить размер буфера листинга файлов
    if (drain == DRAIN_NONE) {                  // можем изменять размер буфера только между строками ответов
      gBufSizeNow = gBufSizeLim;                // изменяем размер буфера листинга файлов
      gAnswer_idx = 0; anchorIdx = 0;           // сбрасываем буфер полностью
      fListMode = false; fListWait = false; fListTOut = 0; // чистим флаги
      sdFile.fInfo = NULL;                      // сбрасываем указатель на запись о файле
      sdFile.selName = 0; sdFile.selIdx = 0;
      sdFile.pageBegIdx = 0; sdFile.maxIdx = 0; // сбрасываем индекс начала страницы показа и размер списка файлов
    } }
  while ((uart_r_avail())                             &&                      // есть символы в кольцевом буфере
         ((errCode & ~ERR_PFT_BUSY) == ERR_NO_ERRORS) &&                      // нет ошибок, исключая ожидание статуса PFT
         (inState == bridgeState)                     &&                      // bridgeState не было изменено при парсинге
         ((netBufEnd - netBufBeg) < (NET_BUF_SIZE - (NET_BUF_SIZE >> 2)))) {  // заполнено менее 0.75 от размера сетевого буфера
    stat(MTR_RX_BUF_SIZE, (uart_rx_buf_end - uart_rx_buf_beg));               // статистика
    uint32_t gTime = millis();
    if (!(getMarlinRXTime))
      getMarlinRXTime = (gTime)? gTime: 0xFFFFFFFF; // статистика
    wd_ACT_Timer = WAIT_ACT_TIMEOUT;            // 10 сек.
    if (logMark != '\xff') {                    // если (logMark != '\xff'), значит нужен лог строки после предыдущего прохода
      logMsg(logMark); logMark = '\xff'; }
    if (drain == DRAIN_BREAK) break;            // возврат, если выставлен флаг на предыдущем проходе
    char c = uart_r_chr(); uartSymAll += 1;     // собственно считывание символа из кольцевого буфера RX
    if (c == '\r') continue;                    // пропуск лишних символов
    if (c != '\n') {
      if (gAnswer_idx < (gBufSizeNow - 1)) {
        gAnswer_buf[gAnswer_idx++] = c;         // сохраняем полученный символ в буфере парсинга по тек.индексу, сдвигаем индекс
        if (gAnswer_idx >= (gBufSizeNow - 1))   // контроль переполнения буфера
          errCode |= ERR_GANSWER_OVF;           // общий обработчик ошибок сбросит в ERR_NO_ERRORS,
                                                // но индексы "висят" до прихода '\n', либо срабатывания wd_ACT_Timer
        }
      gAnswer_buf[gAnswer_idx] = '\0';          // оформляем текущую позицию буфера как конец строки
      continue; }                               // продолжаем считывание, если принятый символ != '\n'
    // здесь тек. принятый символ == '\n', но в буфере вместо него будет '\0',
    // а gAnswer_idx будет установлен дальше, на позицию начала новой строки в буфере
    wd_ACT_Timer = 0;
    if (gAnswer_idx >= (gBufSizeNow - 1)) {
      // восстановление после переполнения буфера
      gAnswer_idx = anchorIdx = 0; gAnswer_buf[gAnswer_idx] = '\0'; // оформляем текущую позицию буфера как конец строки
      continue; }
    if (gAnswer_idx <= (anchorIdx + 1)) {
      // аномально короткая строка ('\n')
      gAnswer_idx = anchorIdx; gAnswer_buf[gAnswer_idx] = '\0';
      continue; }
    gAnswer_buf[gAnswer_idx++] = '\0';          // оформляем (повторно) конец строки, смещаем текущий индекс приема
    if (gAnswer_idx <= (gBufSizeNow - 1)) gAnswer_buf[gAnswer_idx] = '\0'; // оформляем (предварительно) конец следующей строки
    if ((drain == DRAIN_FLAG) && (gBufSizeNow != gBufSizeLim)) 
      drain = DRAIN_BREAK;                      // признак необходимости возврата в loop() после парсинга текущей строки
    if (ctrl & UART_READ) logMark = 0;          // отладочный маркер для текущей строки
    if (ctrl & UART_COUNT) {
      len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:RX: %d sym line\n"), (uartSymAll - uartSymLin));
      netQuePut_cid(packet, len, socket.clWS_ID); }
    uartEOL += 1; uartSymLin = uartSymAll;
    if (timeResend)
      doResend();                               // при заряженном таймере отрабатываем для любой текущей строки
    gDataPtr = &gAnswer_buf[anchorIdx];
    uint idID = (uint)ID_OK;                    // начало списка шаблонов
    gAnswerID_t infoID = findID(&gDataPtr, &idID, false); // фиксируем наличие одного из шаблонов infoID в начале строки и,
                                                          // если есть - ставим указатель gDataPtr непосредственно за infoID
    /* --- сканируем остаток строки на наличие пар "имя:значение" известных параметров --- */
    int hasPrm = 0;
    char* prmIDptr = gDataPtr;
    do {
      if (!(prmIDptr) || !(*prmIDptr)) break;
      prmIDptr = strpbrk(prmIDptr, "TBXYZESNtbxyzesn"); // пробуем найти в строке указатель на первый символ ID параметра
      if (!(prmIDptr)) break;
      char* prmValPtr = prmIDptr;
      idID = (uint)ID_TEMP_T;
      gAnswerID_t prmID = findID(&prmValPtr, &idID, false);
      if (prmID != ID_NONE) {
        hasPrm += 1;
        float fval1 = 0, fval2 = 0;
        long lval1 = 0, lval2 = 0;
        switch (prmID) {
          case ID_TEMP_T:
          case ID_TEMP_B: {
            // 1. Вычитываем текущую температуру. 
            // На строке "26.82 /0.00" strtof прочитает "26.82" и остановится на пробеле перед слэшем.
            fval1 = strtof(prmValPtr, &prmValPtr);
            // 2. Пропускаем любые пробелы после числа, двигаем указатель prmValPtr вперед
            while (prmValPtr && isspace((unsigned char)*prmValPtr)) { 
              prmValPtr++; }
            // 3. Теперь prmValPtr смотрит ровно на слэш. Если он там есть — парсим целевую температуру
            if (prmValPtr && (*prmValPtr == '/'))
              // prmValPtr + 1 перепрыгивает слэш. strtof сама пропустит пробелы после слэша, если они там будут
              fval2 = strtof(prmValPtr + 1, &prmValPtr);
             else fval2 = 0.0;              // Страховка, если целевой температуры почему-то нет в строке
            updFloatValue(prmID, gTime, fval1, fval2);
            break; }
          case ID_POS_X: {
            static strMem_t lMem = {0, 0, 0};
            static uint32_t newPosTime = 0;
            if (confirmStr(&lMem, gDataPtr, &gAnswer_buf[gAnswer_idx - 1] - gDataPtr) < 0) newPosTime = gTime;
             else if (srvSync && (lMem.mLen)) samePos = false;
            if (((gTime - newPosTime) >= 15000) && (socket.fSize))              // ~15 сек на месте
              if ((socket.progress == socket.fSize) && (bridgeState == SYS_PRINT))
                prnFix(1) ;                                                     // сбрасываем состояние в SYS_IDLE
          case ID_POS_Y:
          case ID_POS_Z:
          case ID_POS_E: {
            // вычитываем координату оси (strtof сама поймет знак минус, как в X:-29.0000)
            fval1 = strtof(prmValPtr, &prmValPtr);
            updFloatValue(prmID, gTime, fval1);
            if (prmID == ID_POS_E) {
              prmIDptr = prmValPtr;
              for (int i = 0; i < 4; i++) {
                uint32_t idLen = pgm_read_dword(&gAnswer[ID_STEPS + i].idLen);
                PGM_P id_ptr = (PGM_P)pgm_read_ptr(&gAnswer[ID_STEPS + i].idWord);
                char* nextPtr = strstr_P((char*)prmIDptr, (const char*)id_ptr);
                if (i == 0) {
                  if (!(nextPtr)) break;                          // "Count " в строке не найдено
                  prmIDptr = nextPtr + idLen; }
                 else {
                  if (!nextPtr) continue;
                  lval1 = strtol(nextPtr + idLen, &nextPtr, 10);  // Вдруг порядок следования нарушен...
                  updIntValue((gAnswerID_t)(ID_STEPS + i), gTime, lval1);
                  if (prmValPtr < nextPtr) prmValPtr = nextPtr;   // запоминаем наибольшее смещение
              } } }
            break; } }
          case ID_SXYZ:
            lval1 = strtol(prmValPtr, &prmValPtr, 10);  // Быстрый парсинг целого числа (long), смещение prmValPtr
            updIntValue(ID_SXYZ, gTime, lval1);
            break;
          case ID_SD_OK:
            sdIsOK = (ok.skip == 0);
            if (cmdMode) cmdMode -= 1;                  // восстановление опроса параметров, если был запрет (ввод M28, листинг файлов)
            if (bridgeState != SYS_PRE_UPLD)            // для SYS_PRE_UPLD лог не выводим
              if (logMark == '\xff') logMark = '<';     // маркируем для лога, если это 1й маркер строки
            prmValPtr = &gAnswer_buf[gAnswer_idx - 1];  // смещаем указатель в конец строки - параметров больше не будет
            break;
          case ID_SD_RELEASED:
          case ID_SD_NOSD_1:
          case ID_SD_NOSD_2:
            fListMode = false; sdIsOK = false; fListTOut = 0;
            fListWait = (prmID == ID_SD_RELEASED);      // M22 запрещает автоскан
            if (sdFinal) sdCheck = true;                // разрешаем heartbeat даже при отсутствии SD
            if (cmdMode) cmdMode -= 1;                  // восстановление опроса параметров, если был запрет (ввод M28, листинг файлов)
            gAnswer_idx = anchorIdx = 0;                // сбрасываем буфер полностью
            sdFile.selName = 0; sdFile.selIdx = 0;
            sdFile.fInfo = NULL;                        // сбрасываем указатель на запись о файле
            sdFile.pageBegIdx = 0;                      // сбрасываем индекс начала страницы показа
            sdFile.maxIdx = 0;                          // сбрасываем размер списка файлов
            //if (bridgeState != SYS_IDLE)
            //  errCode |= ERR_NO_SD_CARD;
            // else 
            // маркируем для лога '!', если нет отладочного маркера и если это не вторая попытка автоскана
            if ((logMark) && (sdTryNum != 1)) logMark = '!';
            break;
          case ID_SD_PROGRESS: {
            if (bridgeState != SYS_PRINT) {
              // если печать запущена вручную на принтере
              bool conflict = (bridgeState != SYS_IDLE);
              // переключить в SYS_PRINT
              ok.ack_status &= ~ACK_SELECTED;
              prnFix(2, true);
              if (conflict) {
                // прерываем любую активность сообщением о конфликте
                errCode |= ERR_PRN_DETECTED; break; }
              }
            // 1. Вычитываем тек. значение. 
            // strtol остановится на пробеле перед слэшем.
            lval1 = strtol(prmValPtr, &prmValPtr, 10);        // Быстрый парсинг целого числа (long)
            // 2. Пропускаем любые пробелы после числа, двигая указатель вперед
            while (prmValPtr && isspace((unsigned char)*prmValPtr)) prmValPtr++;
            // 3. Теперь prmValPtr смотрит ровно на слэш. Если он там есть — парсим второе число
            if (prmValPtr && *prmValPtr == '/')
              // prmValPtr + 1 перепрыгивает слэш. strtol сама пропустит пробелы после слэша, если они там будут
              lval2 = strtol(prmValPtr + 1, &prmValPtr, 10);  // Быстрый парсинг целого числа (long)
             else lval2 = 0;                                  // Страховка
            updIntValue(ID_SD_PROGRESS, gTime, lval1, lval2);
            srvSync= false; bridgeState = SYS_PRINT;
            break; }
          case ID_NOTIFY: {
            // извлекаем имя файла из строки "//action:notification <f_name.gco>" (еще м.б. .gcode)
            // prmValPtr указывает на начало строки (имени файла) после "//action:notification "
            // gAnswer_idx соответствует позиции сразу _за_ терминирующим '\0'
            size_t nLen = (&gAnswer_buf[gAnswer_idx - 1] - prmValPtr);
            while (((bridgeState == SYS_IDLE) || (bridgeState == SYS_PRINT)) && (nLen > 5)) { // если строка длиннее мин.имени файла ("x.gco")
              uint32_t extStr;
              memcpy((uint8_t*)&extStr, (uint8_t*)&gAnswer_buf[gAnswer_idx - 5], 4);  // копируем ".gco" или (.g)"code"
              if ((extStr != 0x45444F43) && (extStr != 0x65646F63) && (extStr != 0x4F43472E) && (extStr != 0x6F63672E)) break;
              ok.ack_status |= ACK_SELECTED;
              if (bridgeState != SYS_PRINT) {
                if (srvSync) {
                  srvSync= false; bridgeState = SYS_PRINT; }
                 else prnFix(5, true);
                }
               else if (*socket.fName) break;
              if (nLen > MAX_FNAME_LEN) nLen = MAX_FNAME_LEN;
              memcpy((uint8_t*)&socket.fName, (uint8_t*)prmValPtr, nLen);
              socket.clFileSel = false; socket.fName[nLen] = '\0';
              break;
              }
            prmValPtr = &gAnswer_buf[gAnswer_idx - 1];  // смещаем указатель в конец строки - параметров больше не будет
            break; }
          case ID_BUSY: {
            //srvSync = false; bridgeState = SYS_PRINT;
            if (srvSync && !(busyMarkerTime)) uart_w((char*)PSTR("M27\n")); // прогресс
            busyMarkerTime = gTime;
            prmValPtr = &gAnswer_buf[gAnswer_idx - 1];  // смещаем указатель в конец строки - параметров больше не будет
            break; }
          default:
            if (hasPrm == 1) hasPrm = 0;                // пока что в строке не находилось значений для известных параметров
            break;
          }
        if ((errCode != ERR_NO_ERRORS) || !(gAnswer_idx)) break;
        if (!(prmValPtr)) prmValPtr = prmIDptr;
        if (prmValPtr >= &gAnswer_buf[gAnswer_idx - 1]) break;
        } // if (prmID != ID_NONE)
      // Ищем ближайший пробел после прочитанных символов
      prmIDptr = strchr(prmValPtr, ' ');                // в конце строки будет присвоен NULL
      } while (prmIDptr);
    static strMem_t pathMem = {0, 0, 0};
    static int checkSize;
    if (!(hasPrm) && (infoID == ID_NONE)) {             // в строке не нашлось служебной информации (logMark = '\0xff' или 0)
      if (fListMode) {
        // включен режим листинга содержимого SD - парсим строки, и если формат соответствует - помещаем в буфер, а также -
        // если нужно, ищем совпадения по имени/размеру, и при необходимости - выводим на экран
        fListTOut = WAIT_FLIST_TIMEOUT;                 // подкармливаем таймер
        bool canPut = ((gAnswer_idx + PACKET_BUF_SIZE) <= gBufSizeNow);
        if (!canPut)
          // после помещения текущей строки в буфере останется места меньше, чем PACKET_BUF_SIZE
          netQuePut(NULL, 0, (char*)PSTR("L:!Too many files. Clean the SD.\n"));
         else canPut &= (parseFileInfo(&gAnswer_buf[anchorIdx]) == 0); // пробуем заполнить структуру sdFile
        if (canPut) {
          // успех - заполняем буфер и попутно либо ищем совпадение с параметрами анонсируемого файла,
          // либо передаем порядковый индекс и инфо о файле на экран клиентам с учетом ограничения количества позиций размером страницы
          sdFile.fIdx += 1;
          sdFile.sizeCount += (!(strncasecmp_P(sdFile.fInfo + sdFile.pathLen, PSTR("PATHKEEP.GCO"), 12))? 0: 1);  // считаем файлы
          if (confirmStr(&pathMem, ((sdFile.pathLen)? sdFile.fInfo: (char*)"\n"), sdFile.pathLen) < 0)
            sdFile.keepCount += 1;                                                                                // считаем папки
          if (sdFile.selName) {                 // был установлен флаг поиска совпадения с socket.fName ( + socket.fSize )
            if (sdFile.fIdx == 1) {
              // если задан поиск - в начале задаем режим учета размера и сбрасываем значение выбранного индекса
              checkSize = (int)sdFile.selIdx; sdFile.selIdx = 0; }
            if (fileInfo(checkSize))            // проверяем совпадение в заданном режиме
              sdFile.selNum += 1;               // считаем совпадения
            }
           else if ((sdFile.pageBegIdx) && (sdFile.fIdx >= sdFile.pageBegIdx) && (sdFile.fIdx < (sdFile.pageBegIdx + sdFile.pageSize)))
            fileInfo();                         // индекс в рамках страницы - передаем индекс и инфо о файле на экран клиентам
          anchorIdx = gAnswer_idx;              // имя файла оставляем в буфере, смещаем якорь-указатель начала буфера приема
          } // if (canPut)
         else                                   // если не файл-инфо либо некуда складывать
          if (logMark == '\xff') logMark = '<'; // маркируем для лога, если это 1й маркер строки
                                                // если это - вывод после введенной команды типа M115, M122
        } // if (fListMode)
       else if ((gData.pubArea & 0xFFFF0000) != 0x01100000)
        // необрабатываемые ответы Марлин выводятся в лог за исключением случаев, когда
        // это запрос M115 для получения габаритов
        if (logMark == '\xff') logMark = '<';   // маркируем для лога, если это 1й маркер строки
                                                // если это - вывод после введенной команды типа M115, M122
      gAnswer_idx = anchorIdx;                  // если неподдекживаемое сообщение - указатель приема в начало строки
      continue;                                 // продолжить считывание потока от Marlin
      }
    if (!PowerUp && !fListMode && (wifi_timer > 0)) {         // в начале сеанса показываем IP
      PowerUp = true;
      IPAddress ip = WiFi.localIP();
      len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("M117 IP:%d.%d.%d.%d\n"), ip[0], ip[1], ip[2], ip[3]);
      uart_w((char*)packet, len, true);
      }
    // отрабатываем информационные строки ответа
    bool ackBFT = false;
    uint32_t seqACK = 0;
    if ((infoID == ID_RS) || (infoID == ID_SS) || ((infoID == ID_OK) && !(ok.skip))) {
      // это - Ок-ответ для многострочной команды
      char* s_ptr = gDataPtr;
      ackBFT = getNum(&seqACK, &s_ptr, 255, false);
      if (ctrl & UART_ACK)
        logMark = 0;                                          // "отладочный" маркер
       else if ((bridgeState == SYS_IDLE) || (bridgeState == SYS_PRE_PRINT)) {
        if ((logMark == '\xff') && ok.doLog) logMark = '<'; } // маркируем для лога, если это 1й маркер строки
                                                              // учитываем ok.doLog=true для wsMessage команд
      ok.doLog = false; }                                     // снимаем флаг, игнорируем последующие Ок
    switch (infoID) {
      case ID_SS: {
        if (ackBFT) {
          bool initBFT = (seqBFT < 0);
          seqBFT = (int32_t)(seqACK & 0xFF);
          if (initBFT && (bridgeState == SYS_WAIT_M28)) {
            if (ctrl & SERV_LOG) {
              len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:BFT M28_OK >> open file %s%s\n"),
                                                    ((sdFile.wrkPLen)? sdFile.wrkPath: ""), socket.fName);
              netQuePut_cid(packet, len, socket.clWS_ID); }
            sendBFT(0, BFT_OPEN);
            drain = DRAIN_BREAK; }
           else if ((M30_Timer) && (M30_Seq)) ok.ack_status |= ACK_SS; // при выполнении последовательности удаления файла
          }
        break; }
      case ID_RESEND:
        if (ok.waiting && (bridgeState == SYS_TRANSFER))
          errCode |= ERR_RESEND_ERR;
         else {
          ok.waiting = false; ok.skip = 0; ok.wdTimer = 0;
          uart_w((char*)PSTR("M29\n"));
          sdIsOK = false; gAnswer_idx = anchorIdx = 0;          // сбрасываем флаг валидности буфера имен
          if (ctrl & SERV_LOG)
            netQuePut(NULL, 0, (char*)PSTR("L:Ghost RESEND: SD reset.\n"), socket.clWS_ID);
          }
        if (ctrl & UART_ACK)
          logMark = 0;                                          // маркируем для лога "отладочным" маркером
         else if ((bridgeState == SYS_IDLE) || (bridgeState == SYS_PRE_PRINT)) {
          if ((logMark == '\xff') && ok.doLog) logMark = '<'; } // маркируем для лога, если это 1й маркер строки
        ok.doLog = false;                                       // снимаем флаг, игнорируем последующие Ок
        drain = DRAIN_BREAK;
        break;
      case ID_RS: {
        if (ackBFT && ok.waiting && (bridgeState == SYS_TRANSFER) && !(timeResend)) {
          bool rsErr = true;
          uint8_t seqRS = (uint8_t)(seqBFT & 0xFF) - 1;
          seqBFT = (int32_t)seqRS;
          if ((int32_t)seqACK == seqBFT) {
            if (++fData.txTry <= RESEND_MAX) {
              for(timeResend = millis(); !(timeResend); timeResend = millis())
                delay(1);
              // !!при (timeResend != 0) обработка OK-таймаутов и событий case ID_OK: отменяется!!
              ok.waiting = false;  ok.skip = 0; ok.wdTimer = 0;
              rsErr = false;
            } }
           else errCode |= ERR_NAK_OFFRUN;
          if (rsErr)
            errCode |= ERR_RESEND_ERR;
          drain = DRAIN_BREAK;
          }
        break; }
      case ID_OK: {
        if (ok.skip) {
          ok.skip -= 1;
          if (ctrl & UART_ACK) logMark = 0;       // маркируем для лога "отладочным" маркером
          if (ok.waiting) ok.wdTimer = ok.wdLoad;
          if (ctrl & SERV_LOG) {
            len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:[%S]: OK skipped\n"),
                                              (const char*)pgm_read_ptr(&brStateName[bridgeState]));
            netQuePut_cid(packet, len, socket.clWS_ID); }
          break; }
        if (!ok.waiting) break;
        drain = DRAIN_BREAK;
        ok.time = gTime - ok.time;
        ok.waiting = false; ok.wdTimer = 0;
        if(useBFT && ackBFT && (seqBFT >= 0)) {
          uint8_t seqOK = (uint8_t)(seqBFT & 0xFF) - 1;
          if (seqACK == (int32_t)((uint8_t)(seqOK - 1)))
            break;
          if (seqACK != (int32_t)seqOK) {
            errCode |= ERR_ACK_OFFRUN; break; }
          }
        switch (bridgeState) {
          case SYS_PRE_UPLD: {
            if (!sdIsOK) {
              errCode |=  ERR_NO_SD_CARD; break; }
            bridgeState = SYS_WAIT_M28;
            ok.waiting = true; ok.wdTimer = ok.wdLoad = WAIT_OK_TIMEOUT;  // 5 sec
            // сохраняем полный путь открываемого на запись файла в безопасной области памяти
            // будем его использовать для удаления этого файла в случае возникновения ошибки в процессе записи
            char* fullFilePath = &gAnswer_buf[(GANSWER_BUF_SIZE - (NUM_CHUNKS * CHUNK_SIZE)) - (2 * PACKET_BUF_SIZE)];
            uint8_t* l_ptr = (uint8_t*)(fullFilePath++);                  // по смещению +1 пишем полный путь файла
            len = snprintf_P(fullFilePath, 2 * MAX_FNAME_LEN, PSTR("%s%s"),
                                                ((sdFile.wrkPLen)? sdFile.wrkPath: ""), socket.fName);
            if (len > (2 * MAX_FNAME_LEN)) len = 2 * MAX_FNAME_LEN;       // на всякий случай ограничиваем
            *l_ptr++ = len;                                               // по смещению +0 пишем получившуюся длину
            // гарантируем терминатор \0 и дублируем длину по максимальному смещению
            l_ptr += (2 * MAX_FNAME_LEN); *l_ptr++ = 0; *l_ptr = len;
            if (useBFT) {
              len = snprintf_P((char*)packet, NET_DATA_MAX - 3, PSTR("M28 B1\n"));
              seqBFT = -1;  // признак необходимости выполнения инициализации seqBFT и открытия файла
              errCode |= ERR_PFT_BUSY; }
             else {
              seqBFT = 0;  // доп.признак инициализации в ASCII режиме
              // Формируем команду M28 с именем, которое получили ранее через FILE:
              // устанавливаем пропуск ОК, ответного для M110 N0
              len = snprintf_P((char*)packet, NET_DATA_MAX - 3, PSTR("M110 N0\nM28 \"%s\"\n"), fullFilePath);
              ok.skip = 1; ok.ack_status &= ~(ACK_OPEN_FAIL | ACK_WR_START); }
            uart_w((char*)packet, len);
            if (ctrl & SERV_LOG) {
              if (!useBFT) { l_ptr = (uint8_t*)memchr(packet, '\n', len); *l_ptr = '\\'; }
              netQuePut(packet, len, (char*)PSTR("L:<"), (socket.clGroup_ID & 0xFFFF)? socket.clWS_ID: CID_NOONE); }
            break; }
          case SYS_WAIT_M28:
            if (ok.setState == SYS_SD_ERASE) {
              bridgeState = SYS_WAIT_M29;
              ok.waiting = true; ok.skip = 0; ok.wdTimer = ok.wdLoad = WAIT_OK_TIMEOUT; // 5 sec
              uart_w((char*)PSTR("M29\n"));            // последовательность : M29 -> OK -> M30 -> OK
              if (ctrl & SERV_LOG) {
                len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:M28_OK for \""));
                if (sdFile.pathLen) memcpy(packet + len, (uint8_t*)sdFile.fInfo, sdFile.pathLen);
                len += sdFile.pathLen;
                len += snprintf_P((char*)(packet + len), NET_DATA_MAX - len, PSTR("PATHKEEP.GCO\"\n"));
                netQuePut_cid(packet, len, socket.clWS_ID);
                uint32_t pLen = sdFile.pathLen + sdFile.shortLen;
                len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:M28_OK: op=%u pLen=%u for 0x%08lX \""),
                                                                    (sdFile.opCode & 0x30000000), pLen, (uint32_t)sdFile.fInfo);
                if (pLen) { memcpy(packet + len, (uint8_t*)sdFile.fInfo, pLen); len += pLen; }
                packet[len++] = '\"'; packet[len++] = '\n';
                netQuePut_cid(packet, len, socket.clWS_ID); }
              break; }
            if (seqBFT >= 0) {
              if (!useBFT && !(ok.ack_status & ACK_WR_START))
                errCode |= ERR_UPLOAD_ERR;
               else {
                bridgeState = SYS_TRANSFER; ok.ack_status &= ~ACK_WR_START;
                if (ctrl & SERV_LOG) {
                  len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:%sM28_OK >> set SYS_TRANSFER [n%d + %S + %u]\n"),
                                                                          ((useBFT)? "BFT ": ""), ch_cnt,
                                                                          (const char*)pgm_read_ptr(&brStateName[bridgeState]),
                                                                          timeResend );
                  netQuePut_cid(packet, len, socket.clWS_ID);
              } } }
             else {
              if (ctrl & SERV_LOG)
                netQuePut(NULL, 0, (char*)PSTR("L:BFT M28_OK >> fire SYNC\n"), socket.clWS_ID);
              sendBFT(0, BFT_SYNC);
              }
            break;
          case SYS_TRANSFER:
            // здесь только фиксация успешной передачи, OK от RESEND отрабатывается в doResend()
            stat(MTR_OK_WAIT_TIME, ok.time);
            if (ctrl & SERV_LOG) {
              if (useBFT)
                len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:BFT_BLOCK_OK after %u msec\n"), ok.time);
               else
                len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:ASCII_CMD_OK after %u msec\n"), ok.time);
              netQuePut_cid(packet, len, socket.clWS_ID); }
            timeResend = fData.txTry = 0; // очищаем счетчик попыток RESEND
            if (!useBFT) {                // в режиме ASCII
              socket.progress += lastSentLen; lastSentLen = 0; }
             else {                       // в режиме BFT
              while ((lastSentLen > 0) && (ch_cnt > 0)) {
                size_t avail = chunks[ch_out].len - chunks[ch_out].offset;
                size_t take = (lastSentLen > avail) ? avail : lastSentLen;
                countData(take);          // Двигаем указатели и закрываем чанки
                lastSentLen -= take;      // Списываем из полета
                socket.progress += take;  // Прибавляем к прогрессу (ЯВНО)
                fData.txTotal += take; fData.txSeq += 1;
              } }
            break;
          case SYS_WAIT_M29:
            if (ok.setState == SYS_SD_ERASE) {
              bridgeState = SYS_WAIT_M30;
              ok.waiting = true; ok.skip = 0; ok.wdTimer = ok.wdLoad = WAIT_OK_TIMEOUT; // 5 sec
              ok.ack_status &= ~(ACK_OPEN_FAIL | ACK_DEL_FAIL | ACK_DELETED);
              uint32_t pLen = sdFile.pathLen + sdFile.shortLen;
              if (ctrl & SERV_LOG) {
                len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:M29_OK: op=%u pLen=%u for 0x%08lX \""),
                                                                    (sdFile.opCode & 0x30000000), pLen, (uint32_t)sdFile.fInfo);
                if (pLen) { memcpy(packet + len, (uint8_t*)sdFile.fInfo, pLen); len += pLen; }
                packet[len++] = '\"'; packet[len++] = '\n';
                netQuePut_cid(packet, len, socket.clWS_ID); }
              txTimer = 2;                            // включаем задержку выдачи команды в UART ~0.2 сек.
              uart_w((char*)PSTR("M30 \""));          // удаление файла с указанием короткого имени
              if (pLen) uart_w(sdFile.fInfo, pLen);
              uart_w((char*)PSTR("\"\n"));            // последовательность : M30 -> OK
              sdFile.keepCount += 1;                  // счетчик созданных
              if (ctrl & SERV_LOG) {
                len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:M29_OK : (%d/%d) \""),
                                                (sdFile.keepCount & 0xFFFF), (sdFile.keepCount >> 16));
                if (sdFile.pathLen) memcpy(packet + len, (uint8_t*)sdFile.fInfo, sdFile.pathLen);
                len += sdFile.pathLen;
                len += snprintf_P((char*)(packet + len), NET_DATA_MAX - len, PSTR("PATHKEEP.GCO\" created\n"));
                netQuePut_cid(packet, len, socket.clWS_ID); }
              break; }
            bridgeState = SYS_IDLE; // предварительная смена, после проверки SD м.б. изменено на SYS_PRE_PRINT
            heartbeat(true);        // "срочный" heartbeat
            if (ctrl & SERV_LOG) netQuePut(NULL, 0, (char*)PSTR("L:M29_OK received\n"), socket.clWS_ID);
            if (useBFT) { useBFT = false; sendBFT(0, BFT_EXIT); }
            socket.progress = 0;    // socket.fSize = fData.txTotal; // для ASCII может изменяться из-за комментариев
            ok.ack_status &= ~ACK_QRY_SIZE; // не учитывать размер при сканировании SD
            sdFile.opCode = 0;      // сбрасываем указатель выбора файла для печати
            M20_Timer = 4;          // через ~500 мсек просканировать SD на наличие переданного файла в рабочей папке и
                                    // в случае успеха - перейти к bridgeState == SYS_PRE_PRINT
            printStat();
            break;
          case SYS_WAIT_M30: {
            if (ok.setState == SYS_SD_ERASE) {
              bridgeState = SYS_SD_ERASE;
              static uint32_t tryMem = 0;                                     // счетчик попыток
              if (!(ok.ack_status & ACK_DELETED)) {
                ok.ack_status &= ~(ACK_OPEN_FAIL | ACK_DEL_FAIL);             // очищаем флаги ошибок
                if (tryMem != (uint32_t)sdFile.fInfo) {                       // попробуем удалить со второй попытки
                  tryMem = (uint32_t)sdFile.fInfo;
                  if (sdFile.opCode & 0x10000000) break;                      // если было просто удаление
                  uint8_t* op_ptr = (uint8_t*)strchr(sdFile.fInfo, '\2');     // если была комбинированная операция
                  if (op_ptr) { *op_ptr = 1; break; }                         // меняем код операции на просто удаление
                  }
                errCode |= ERR_M30_OK_TOUT; tryMem = 0;                       // на третий раз выставляем флаг ошибки
                break; }
              ok.ack_status &= ~ACK_DELETED; tryMem = 0;
              sdFile.pageBegIdx += 1; sdFile.sizeCount += sdFile.fSize;       // счетчики удаленных/освобожденного места
              if (ctrl & SERV_LOG) {
                len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:M30_OK : (%d/%d) \""),
                                        (sdFile.pageBegIdx & 0xFFFF), (sdFile.pageBegIdx >> 16));
                uint32_t pLen = sdFile.pathLen + sdFile.shortLen;
                memcpy(packet + len, (uint8_t*)sdFile.fInfo, pLen); len += pLen;
                len += snprintf_P((char*)(packet + len), NET_DATA_MAX - len, PSTR("\" deleted\n"));
                netQuePut_cid(packet, len, socket.clWS_ID); }
              sdFile.fInfo += (strlen(sdFile.fInfo) + 1); sdFile.fIdx += 1;   // смещаем индексы
              if ((sdFile.pageBegIdx & 0xFFFF) == (sdFile.pageBegIdx >> 16))  // если все запланированные удалены
                M20_Timer = 4;  // через ~500 мсек просканировать SD на наличие активного файла в рабочей папке и
                                // в случае успеха - перейти к bridgeState == SYS_PRE_PRINT
              }
             else {
              M20_Timer = 4;    // через ~500 мсек просканировать SD на наличие активного файла в рабочей папке и
                                // в случае успеха - перейти к bridgeState == SYS_PRE_PRINT
              if (ctrl & SERV_LOG) netQuePut(NULL, 0, (char*)PSTR("L:M30_OK received\n"), socket.clWS_ID); }
            ok.ack_status |= ACK_QRY_SIZE;  // учитывать размер при сканировании SD
            break; }
          case SYS_WAIT_M23: {
            switch (ok.setState) {
              case SYS_PRE_PRINT:
                sdFile.fIdx = 1;
                for (sdFile.fInfo = gAnswer_buf; sdFile.fInfo < &gAnswer_buf[anchorIdx]; sdFile.fInfo += (strlen(sdFile.fInfo) + 1), sdFile.fIdx++) {
                  if (sdFile.pHash == csFInfo(sdFile.fInfo)) {
                    if (!(parseFileInfo(sdFile.fInfo))) // пробуем заполнить структуру sdFile (ненулевой результат - ошибка)
                      if (cmpFPath() > 0)               // имя совпадает с сокетом, path совпадает с рабочим
                        break;                          // успешно найдена запись и заполнена sdFile
                    sdFile.fInfo = &gAnswer_buf[anchorIdx] + 1;
                    break; }
                  if (!(sdFile.fIdx & 0x3F)) ESP.wdtFeed();
                  }
                if (sdFile.fInfo >= &gAnswer_buf[anchorIdx]) {
                  // здесь - поиск и парсинг не удались
                  netQuePut(NULL, 0, (char*)PSTR("L:!Can't select file.\n"), socket.clWS_ID);
                  ok.setState = SYS_IDLE; bridgeState = SYS_IDLE;
                  anchorIdx = gAnswer_idx = 0;          // сброс флага валидности списка имен (перечитать список файлов)
                  break; }                              // отмена старта печати и выход из цикла
                ok.setState = SYS_WAIT_M23; ok.ack_status &= ~(ACK_OPEN_FAIL | ACK_SELECTED);
                ok.waiting = true; ok.skip = 0; ok.wdTimer = ok.wdLoad = WAIT_OK_TIMEOUT; // 5 sec
                uart_w((char*)PSTR("M23 \""));          // имя файла будет в кавычках
                if (sdFile.pathLen)
                  uart_w(sdFile.fInfo, sdFile.pathLen); // путь, при наличии, берем из sdFile
                uart_w(socket.fName);                   // имя файла - из socket
                WITH_FLAG_OFF(UART_SEND, (uart_w((char*)PSTR("\"\n"))));
                break;
              case SYS_WAIT_M23:
                if (!(ok.ack_status & ACK_SELECTED)) 
                  errCode |= ERR_SD_SEL_FAILED;
                 else
                  prnFix(6, true);
                break;
              case SYS_PRINT:
                ok.setState = SYS_IDLE; bridgeState = SYS_PRINT;  //ok.ack_status &= ~ACK_SELECTED;
                uart_w((char*)PSTR("M24\n"));             // запуск на печать
              default:
                break;
              }
            break; }
          case SYS_WAIT_MOVE:
          case SYS_WAIT_TEMP: {
            BridgeState_t gState = bridgeState;
            bridgeState = (((ok.setState == SYS_PRE_PRINT) || (ok.setState == SYS_IDLE))? ok.setState: SYS_IDLE);
            timeMQTTCtrl = gTime; mqttBusyTime = 10000;         // "резервируем" канал входящих MQTT-команд для socket.mqttCtrl_ID на 10 сек
            // окончание обработки MQTT G-команды перемещения/установки температуры
            uart_w(((gState == SYS_WAIT_MOVE)? (char*)PSTR("M114\n"): (char*)PSTR("M105\n"))); // запрос координат/температуры
            if (ctrl & SERV_LOG) {
              len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:[%S]G-code OK. %S fired.\n"),
                                (const char*)pgm_read_ptr(&brStateName[bridgeState]),
                                ((gState == SYS_WAIT_MOVE)? (char*)PSTR("M114"): (char*)PSTR("M105")));
              netQuePut_cid(packet, len, socket.clWS_ID); }
            break; }
          case SYS_IDLE:
            if ((M30_Timer) && (M30_Seq)) ok.ack_status |= ACK_OK; // при выполнении последовательности удаления файла после ошибки
            break;
          default:
            break; 
          }
        break; } // case ID_OK
      case ID_OPEN_FAIL:
        ok.ack_status |= ACK_OPEN_FAIL;
        if (logMark) logMark = '!';             // если еще нет отладочного маркера, маркируем '!'
        break;
      case ID_WR_START:
        ok.ack_status |= ACK_WR_START;
        if (logMark == '\xff') logMark = '<';   // маркируем, если это 1й маркер строки
        break;
      case ID_WR_DONE:
        if (logMark == '\xff') logMark = '<';   // маркируем, если это 1й маркер строки
        break;
      case ID_DEL_FAIL:
        ok.ack_status |= ACK_DEL_FAIL;
        if (logMark) logMark = '!';             // если еще нет отладочного маркера, маркируем '!'
        break;
      case ID_DELETED:
        ok.ack_status |= ACK_DELETED;
        if (logMark == '\xff') logMark = '<';   // маркируем, если это 1й маркер строки
        break;
      case ID_SELECTED:
        ok.ack_status |= ACK_SELECTED;          // Просто взводим флаг! Транзакция еще не закрыта.
        if (logMark == '\xff') logMark = '<';   // маркируем, если это 1й маркер строки
        break;
      case ID_PRN_DONE:
        prnFix(3);
        break;
      case ID_PFT:
      case ID_PTF: {
        char* t_ptr = NULL;
        // gDataPtr уже указывает на текст СРАЗУ после "PFT:"
        if (strstr(gDataPtr, "success")) {
          errCode &= ~ERR_PFT_BUSY;             // Сбрасываем флаг занятости, если был
        if (logMark == '\xff') logMark = '<'; } // маркируем, если это 1й маркер строки
         else if (strstr(gDataPtr, "busy")) {
          errCode |= ERR_PFT_BUSY;              // по таймеру отправка QUERY каждые 500мс
          if (logMark) logMark = '!';           // если еще нет отладочного маркера, маркируем '!'
          t_ptr = (char*)PSTR("busy"); }
         else if (strstr(gDataPtr, "fail") || strstr(gDataPtr, "ioerror")) {
          errCode |= ERR_UPLOAD_ERR;            // Жесткая ошибка
          if (logMark) logMark = '!';           // если еще нет отладочного маркера, маркируем '!'
          t_ptr = (char*)PSTR("fail"); }
        if ((ctrl & SERV_LOG) && t_ptr) {
          const char* m_ptr = (const char*)pgm_read_ptr(&brStateName[bridgeState]);
          len = snprintf_P((char*)packet, NET_DATA_MAX,
                                  PSTR("L:PFT:%S [n%d * %S * rs%u]\n"),
                                  t_ptr, ch_cnt, m_ptr, timeResend );
          netQuePut_cid(packet, len, socket.clWS_ID); }
        break; }
      case ID_ERROR: {
        if (logMark) logMark = '!';             // если еще нет отладочного маркера, маркируем '!'
        break; }
      case ID_WAIT: {
        if ((bridgeState != SYS_PRINT) || (ctrl & FULL_STAT))
          // вывод в лог за исключением случаев, когда идет печать с отключенной подробной статистикой
          if (logMark == '\xff') logMark = '<';   // маркируем, если это 1й маркер строки
        break; }
      case ID_ECHO: {
        if ((bridgeState != SYS_PRINT) || (ctrl & FULL_STAT))
          // вывод в лог за исключением случаев, когда идет печать с отключенной подробной статистикой
          if (logMark == '\xff') logMark = '<';   // маркируем, если это 1й маркер строки
        break; }
      case ID_ACTION: {
        if ((bridgeState != SYS_PRINT) || (ctrl & FULL_STAT))
          // вывод в лог за исключением случаев, когда идет печать с отключенной подробной статистикой
          if (logMark == '\xff') logMark = '<';   // маркируем, если это 1й маркер строки
        break; }
      case ID_SD_BEGIN:
        fListMode = true; sdIsOK = false; fListWait = true; fListTOut = WAIT_FLIST_TIMEOUT;
        cmdMode += 1;                           // запрет опроса параметров
        gAnswer_idx = 0; anchorIdx = 0; // сбрасываем буфер полностью
        pathMem.mLen = 0; pathMem.tLog = 0; pathMem.xCS = 0;  // для подсчета кол-ва папок
        sdFile.sizeCount = 0; sdFile.keepCount = 0;           // для подсчета кол-ва папок
        sdFile.fIdx = 0; sdFile.maxIdx = 0;
        sdFile.selNum = 0;              // для подсчета количества файлов с заданным именем поиска
        if (ok.doLog) {
          // это вывод после wsMessage
          sdFile.pageBegIdx = 1;        // сбрасываем отображение в начало, если это вывод после wsMessage
          sdFile.selName = false;       // сбрасываем флаг выбора по совпадению с socket.fName & socket.fSize
          sdFile.selIdx = 0;            // сбрасываем текущий выбор / индекс для выбора при полном листинге
          }
        break;
      case ID_SD_END:
        fListMode = false; sdIsOK = true; fListWait = false; fListTOut = 0;
        sdCheck = true;                 // разрешаем heartbeat
        if (cmdMode) cmdMode -= 1;      // восстановление опроса параметров, если был запрет (ввод M28, листинг файлов)
        if (bridgeState == SYS_PRE_UPLD) break;
        sdFile.maxIdx = sdFile.fIdx;
        if (!sdFile.selName) {
          // без поиска совпадения с socket.fName & socket.fSize - был вывод в лог
          if (sdFile.pageSize > sdFile.maxIdx)
            sdFile.pageSize = sdFile.maxIdx;
          if (((sdFile.pageBegIdx + sdFile.pageSize) - 1) >= sdFile.maxIdx) {
            len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:SD: %u (%u) files. %u folders.\n"),
                                            sdFile.sizeCount, sdFile.maxIdx, sdFile.keepCount);
            netQuePut_cid(packet, len, socket.clWS_ID);
            if (sdFile.pageBegIdx > sdFile.maxIdx) sdFile.pageBegIdx = sdFile.maxIdx + 1;
            }
           else fListInfo();  
          }
        if (ctrl & SERV_LOG) {
          len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR(
                                          "L:sd-: fidx %d sNum %d sIdx %d\n"
                                          "L:sd- beg %d pSz %d max %d\n" ),
                                          sdFile.fIdx, sdFile.selNum, sdFile.selIdx,
                                          sdFile.pageBegIdx, sdFile.pageSize, sdFile.maxIdx);
          netQuePut_cid(packet, len, socket.clWS_ID); }
        if ((sdFile.fIdx) && (sdFile.selNum)) {
          len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:Name found in %d folder(s).\n"), sdFile.selNum);
          if (socket.clFileSel)     // если в socket содержится "эталонная" инфа после выбора на диске клиента
            len += (snprintf_P((char*)packet + (len - 1), NET_DATA_MAX - (len - 1), PSTR("  Orig.size %u%S.\n"),
                                                (socket.fSize > 99999)? (socket.fSize >> 10): socket.fSize,
                                                (socket.fSize > 99999)?  PSTR(" KB"): PSTR(" b")) - 1);
          netQuePut_cid(packet, len, socket.clWS_ID);
          }
        if (sdFile.selIdx) {
          bridgeState = SYS_IDLE; ok.setState = SYS_IDLE;  // запрет рекурсии M20
          fileID(false, sdFile.selIdx); // заполнить sdfile и установить bridgeState = SYS_PRE_PRINT
          }
        break;
      /*case ID_CAP: {
        if ((*gDataPtr != 'A') && (*gDataPtr != 'a')) break;
        idID = (uint)ID_CAP_TB;
        prmIDptr = gDataPtr;
        infoID = findID(&prmIDptr, &idID, true);
        bool* autoreport = NULL;
        switch (infoID) {
          case ID_CAP_TB:
            autoreport = &autoTB;
            break;
          case ID_CAP_XYZ:
            autoreport = &autoXYZ;
            break;
          case ID_CAP_SDPOS:
            autoreport = &autoProgress;
          default:
            break;
          }
        if (autoreport) {
          *autoreport = (*prmIDptr == '1'); getCap = true; }
        if ((gDataPtr) && doLog)
          logMsg(infoID, ':');
        break; }*/
      case ID_NO_PRN:
        prnFix(4);
        srvSync= false;
        break;
      case ID_AREA:
        // ответ на команду M115 содержит JSON-подобное описание габаритов рабочей области принтера
        // публикация этого описания реализована через атрибут dimensions сущности text.iot_7_g
        // приведение к каноническому JSON виду производится через специальный шаблон (MQTT_topics.h)
        // для запуска процедуры генерации команды M115 и публикации габаритов нужно
        // присвоить переменной gData.pubArea значение 0 , после чего при очередной автопубликации пакета progress/uptime
        // произойдет генерация команды M115 и присвоение переменной gData.pubArea значения 0x01100000
        // при парсинге ответа (см. код ниже) в младшие 2 байта переменной gData.pubArea заносится индекс начала строки ответа
        //                                  а в старшие 2 байта переменной gData.pubArea заносится значениe 0x0220
        // далее якорный индекс смещается в конец строки ответа для её сохранения в буфере до публикации
        // при очередной автопубликации пакета progress/uptime индекс начала строки ответа считывается из переменной gData.pubArea
        // производится публикация строки ответа, а якорный индекс восстанавливается на ее начало
        // переменная gData.pubArea получает значение 0x03300000 в качестве флага завершения процедуры опроса габаритов
        if (gData.pubArea == 0x01100000) {            // если это - ответ на авто запрос M115
          gData.pubArea = 0x02200000 | anchorIdx;     // запоминаем индекс начала строки ответа + флаг 0x02200000
          anchorIdx = gAnswer_idx;                    // изолируем строку габаритов из ответа для её публикации MQTT
          pubProgress = true; timeProgress = gTime;   // требование публикации пакета системного статуса
          cmdMode += 1; }                             // запрещаем опрос принтера до публикации
         else if (logMark == '\xff') logMark = '<';   // это ручной M115, маркируем, если это 1й маркер строки
        break;
      case ID_NONE:
        break;
      default:
        break;
      } // switch (infoID)
    gAnswer_idx = anchorIdx;                // сбрасываем текущую позицию записи на якорную
    } // while (uart_r_avail() && ((errCode & ~(ERR_PFT_BUSY)) == ERR_NO_ERRORS))
  if (logMark != '\xff') logMsg(logMark);  // если (logMark != '0\xff'), значит есть строка в лог
  if (ctrl & UART_COUNT)
    if (uartSymAll) {
      len = snprintf_P((char*)packet, NET_DATA_MAX, PSTR("L:RX= %d line(s), %d sym(s)\n"), uartEOL, uartSymAll);
      netQuePut_cid(packet, len, socket.clWS_ID); // Трансляция лога в браузер ### debug
      }
} // answer()
