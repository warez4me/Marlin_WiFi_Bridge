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

#include <EEPROM.h>
#include "WiFi_BFT.h"


// =========================================================================
// СБРОС НАСТРОЕК вызывается из парсера MQTT mqttSocketHandler() или Веб-терминала myBridgeCmd()
// =========================================================================
void clearConfigInEEPROM() {
  EEPROM.begin(512);  //(sizeof(Config));
  uint32_t zero_magic = 0;          // Затираем только магическое число нулями
  EEPROM.put(0, zero_magic);
  EEPROM.commit();
  EEPROM.end();
}

// =========================================================================
// РАСЧЕТ КОНТРОЛЬНОЙ СУММЫ CRC16 CCITT (Без таблиц — экономия IRAM)
// =========================================================================
uint16_t calculateCRC16(const uint8_t* data, size_t length) {
  uint16_t crc = 0xFFFF;                            // Стартовое значение
  for (size_t i = 0; i < length; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (uint8_t bit = 0; bit < 8; bit++) {
      if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;  // Полином CCITT
       else crc <<= 1;
      }
    }
  return crc;
}

bool loadConfigFromEEPROM() {
  EEPROM.begin(512);  //(sizeof(Config));
  EEPROM.get(0, cfg);
  EEPROM.end();
  uint16_t valid_crc = ~cfg.crc;  // присвоение заведомо неравного значения
  // 1. Проверяем сигнатуру первого запуска
  if (cfg.magic == CONFIG_MAGIC)
    // 2. Считаем валидную CRC для прочитанных данных (без учета поля crc)
    valid_crc = calculateCRC16((const uint8_t*)&cfg, sizeof(Config) - sizeof(cfg.crc));
  // 3. Вернет true при равенстве того, что посчитали, с тем, что сохранено в конце структуры
  return (cfg.crc == valid_crc);
}

// ОТДАЧА СЖАТОЙ СТРАНИЦЫ CONFIG (HTTP_GET)
void handleGetConfigPage() {
  // ЗАЩИТА: Если мы подключены к роутеру (STA) — ресурса /config для сети не существует!
  if (WiFi.getMode() == WIFI_STA) {
    httpServer.send(404, PSTR("text/plain"), PSTR("Not Found")); return; }
  // Формируем строку из глобального sessionID (он уже 100% готов)
  snprintf((char*)packet, sizeof(packet), "%u", sessionID); 
  // Отправляем кастомный заголовок безопасности перед выдачей HTML
  httpServer.sendHeader("x", (char*)packet);
  // Если конфигурация уже существует, отдаем данные через HTTP-заголовки
  if (cfg.magic == CONFIG_MAGIC) {
    httpServer.sendHeader("s", cfg.wifi_ssid);
    if (*cfg.mqtt_ip) {
      // Для MQTT IP собираем строку "IP:PORT"
      snprintf((char*)packet, sizeof(packet), "%s:%u", cfg.mqtt_ip, cfg.mqtt_port);
      httpServer.sendHeader("i", (char*)packet);    // всегда идут в паре
      httpServer.sendHeader("d", cfg.device_id);    // всегда идут в паре
      if (*cfg.mqtt_user) httpServer.sendHeader("u", cfg.mqtt_user);
      if (*cfg.ha_prefix) httpServer.sendHeader("h", cfg.ha_prefix);
    } }
  // Говорим браузеру, что контент сжат с помощью GZIP
  httpServer.sendHeader(F("Content-Encoding"), F("gzip"));
  // Отправляем код страницы конфигурации напрямую из PROGMEM
  httpServer.send(200, (const char*)PSTR("text/html"), (const char*)CONFIG_HTML, sizeof(CONFIG_HTML));
  apTime = 900; // продлеваем до ~900 сек. интервал, выделенный для переконфигурации в режиме AP
}

// ПРИЕМ И ПАРСИНГ ДАННЫХ CONFIG (HTTP_POST)
// =========================================================================
// УЛЬТРА-БЫСТРЫЙ НИЗКОУРОВНЕВЫЙ ПАРСЕР-ВАЛИДАТОР IP И ПОРТА
// =========================================================================
uint16_t parseIP(char* ip_str) {
  uint32_t strVal, i;
  for(i = 0; i < 4; i++) {
    if (!getNum(&strVal, &ip_str, 255, false)) break; // считываем октет IP адреса с проверкой диапазона значений
    if (i < 3)
      if (*ip_str++ != '.') break;                    // проверяем разделители октетов
    }
  strVal = 0;
  if (i == 4) {                                       // если успешно прочитали все 4 октета
    strVal = 1883;                                    // значение порта по умолчанию
    if (*ip_str == ':') {                             // проверяем наличие разделителя перед номером порта
      *ip_str++ = '\0';                               // фиксируем конец строки IP и переходим к номеру порта
      getNum(&strVal, &ip_str, 65535, false); }       // считываем номер порта с проверкой диапазона значений
     else *ip_str = '\0';                             // фиксируем конец строки IP и отсекаем возможный мусор
    }
  return (uint16_t)(strVal & 0xFFFF);
}

void handleSaveConfig() {
  apTime = 900; // продлеваем до ~900 сек. интервал, выделенный для переконфигурации в режиме AP
  const char* eMsg = PSTR("Error. Invalid configuration request.");
  // Проверяем, что запрос содержит ВСЕ 8 ключей нашей формы
  if (!httpServer.hasArg("o") || !httpServer.hasArg("s") ||
      !httpServer.hasArg("p") || !httpServer.hasArg("i") ||
      !httpServer.hasArg("u") || !httpServer.hasArg("w") ||
      !httpServer.hasArg("h") || !httpServer.hasArg("d")) {
      httpServer.send(400, (const char*)PSTR("text/plain"), eMsg);
      return; }
  // ПРОВЕРКА ANTI-CSRF ТОКЕНА БЕЗОПАСНОСТИ
  // Сравниваем прилетевшую из формы "o" строку токена с текущим sessionID
  if (sessionID != strtoul(httpServer.arg("o").c_str(), NULL, 10)) {
    httpServer.send(403, (const char*)PSTR("text/plain"), eMsg);
    return; }
  int len = 0;
  eMsg = PSTR("Error. Field ");
  struct { const char* sym; int fieldSeq; } prm[6] = {{"s", 1}, {"u", 4}, {"w", 5}, {"h", 6}, {"d", 7}};
  for (int i = 0; i < 5; i++) {
    if (httpServer.arg(prm[i].sym).length() >= STR_LEN_SHORT) {
      if (!(len)) len = snprintf_P((char*)packet, sizeof(packet), eMsg);
      len += snprintf_P((char*)(packet + len), sizeof(packet) - len, PSTR("%d,"), prm[i].fieldSeq);
    } }
  if (len) len += snprintf_P((char*)(packet + len - 1), sizeof(packet) - len + 1, PSTR(" too long (>%d)"), STR_LEN_SHORT - 1);
  if (httpServer.arg("p").length() >= ((STR_LEN_SHORT * 2) - 1)) {
    if (!(len)) len = snprintf_P((char*)packet, sizeof(packet), eMsg);
     else len += snprintf_P((char*)(packet + len), sizeof(packet) - len, PSTR("; "));
    snprintf_P((char*)(packet + len), sizeof(packet) - len, PSTR("2 too long (>%d)"), ((STR_LEN_SHORT * 2) - 1));
    }
  if (len) {
    httpServer.send(400, (const char*)PSTR("text/plain"), (char*)packet);
    uartWxStop = false;
    uart_w((char*)PSTR("M117 Fields oversize..\n"));
    return; }
  // ВАЛИДАЦИЯ КОНТЕНТА
  // SSID обязателен к заполнению всегда
  if (httpServer.arg("s").length() == 0) {
    httpServer.send(400, (const char*)PSTR("text/plain"), (const char*)PSTR("Error. Missing SSID."));
    uartWxStop = false;
    uart_w((char*)PSTR("M117 Missing SSID..\n"));
    return; }
  uint16_t mqtt_port = 0;
  // поля "i" и "d" для MQTT должны быть либо заполнены, либо пусты!
  if (!!(httpServer.arg("i").length()) ^ !!(httpServer.arg("d").length())) {
    httpServer.send(400, (const char*)PSTR("text/plain"),
                    (const char*)PSTR("Error. Fields 3,7: enter both or none."));
    uartWxStop = false;
    uart_w((char*)PSTR("M117 Bad MQTT setup..\n"));
    return; }
  if (httpServer.arg("i").length()) {   // Если блок MQTT был заполнен — запускаем парсер октетов и порта
    mqtt_port = parseIP((char*)httpServer.arg("i").c_str());
    if (!mqtt_port) {
      httpServer.send(400, (const char*)PSTR("text/plain"), (const char*)PSTR("Error. Invalid MQTT IP address."));
      uartWxStop = false;
      uart_w((char*)PSTR("M117 Bad MQTT IP..\n"));
      return;
    } }
  // НАДЁЖНАЯ ЗАПИСЬ СТРУКТУРЫ
  memset(&cfg, 0, sizeof(Config)); // Очищаем всё нулями
  // Копируем Wi-Fi параметры (безопасный strncpy)
  strncpy(cfg.wifi_ssid, httpServer.arg("s").c_str(), (STR_LEN_SHORT - 1));
  strncpy(cfg.wifi_pass, httpServer.arg("p").c_str(), (STR_LEN_SHORT * 2 - 1));
  // Копируем MQTT, только если блок полностью валиден
  if (mqtt_port) {
    strncpy(cfg.mqtt_ip,   httpServer.arg("i").c_str(), STR_LEN_SHORT - 1); // Чистый IP без порта
    strncpy(cfg.mqtt_user, httpServer.arg("u").c_str(), STR_LEN_SHORT - 1);
    strncpy(cfg.mqtt_pass, httpServer.arg("w").c_str(), STR_LEN_SHORT - 1);
    strncpy(cfg.ha_prefix,        // "h" - по умолчанию подставляется homeassistant
            (httpServer.arg("h").length())? httpServer.arg("h").c_str(): fl2chr(PSTR("homeassistant")),
            STR_LEN_SHORT - 1);
    strncpy(cfg.device_id, httpServer.arg("d").c_str(), STR_LEN_SHORT - 1);
    cfg.mqtt_port = mqtt_port; }
  // Заполняем системный маркер и CRC16 структуры без самого поля cfg.crc
  cfg.magic = CONFIG_MAGIC;
  cfg.crc = calculateCRC16((const uint8_t*)&cfg, sizeof(Config) - sizeof(cfg.crc));
  // Бинарная запись put-операцией в последний сектор флэш-памяти
  EEPROM.begin(512);  //(sizeof(Config));
  EEPROM.put(0, cfg);
  EEPROM.commit();
  EEPROM.end();
  // Финал и уход в перезагрузку
  httpServer.send(200, (const char*)PSTR("text/plain"), (const char*)PSTR("Config saved. Trying new setup.."));
  uartWxStop = false;
  uart_w((char*)PSTR("M117 WiFi bridge reconfigured..\n"));
  delay(200);
  ESP.restart(); 
}

void sta2ap(const char* msg) {
  // переключает WiFi в режим AccessPoint (WIFI_AP)
  uartOff(); uartWxStop = false; // запрещаем прием UART RX, разрешаем передачу UART TX
  if (mqttSockData.sockState != MQTT_SOCKET_DISCONNECTED)
    mqttDisconnect();         // Жестко закрываем сокет MQTT, очищая буферы
  if (msg) {
    int32_t len = 0;
    for (int i = 0; i < 3; i++)
      len += snprintf_P((char*)packet + len, NET_DATA_MAX - len, PSTR("S:%u:%d:%d:%d\n"),
                        sessionID, HB_WAIT, socket.clGroup_ID + 1,  // +(сбросить активиста)
                        ((ctrl & CLIENT_LOG) >> 4));
    len += snprintf_P((char*)packet + len, NET_DATA_MAX - len, PSTR("%S"), msg);
    netQuePut(packet, -len);  // urgent HB
    netQueSend();
    delay(50); }
  webSocket.disconnect();     // Закрываем WS соединения
  wsMap = 0; socket.clGroup_ID = 0; // очищаем данные о подключенных WS клиентах
  delay(1);
  WiFi.persistent(false);
  delay(1);
  WiFi.disconnect(true);      // Полностью выключаем клиентский WiFi модуль
  delay(1);                   // Даем lwIP 1 мс на очистку таблиц сокетов
  IPAddress local_IP(192,168,9,1);
  IPAddress gateway(192,168,9,1);
  IPAddress subnet(255,255,255,0);
  WiFi.softAPConfig(local_IP, gateway, subnet);
  delay(1);
  // Переключаем железо в чистый AP
  WiFi.mode(WIFI_AP);
  delay(1);                   // UART RX отключен, переполнения буфера не боимся
  // Разрешаем подключение только 1 устройству на 1-м Wi-Fi канале):
  WiFi.softAP(fl2chr(PSTR("Marlin-Bridge-Setup")), NULL, 1, false, 1);
  delay(1); 
  httpServer.begin();         // Пересаживаем веб-сервер на 192.168.9.1
  wifi_timer = -50; wifiLastCheck = 0; wifiState = WIFI_STATE_AP; // переводим loop() в режим WiFi_AP
  apTime = 120;               // ограничиваем время ожидания коннекта в режиме AP до 120 сек.
}

void handleMainPage() {
  // Блокируем главную страницу, если сервер ушел в режим настройки AP:
  if (WiFi.getMode() == WIFI_AP) {
    httpServer.send(404, PSTR("text/plain"), PSTR("Not Found")); return; }
  httpServer.sendHeader(F("Content-Encoding"), F("gzip"));
  httpServer.send(200, (const char*)PSTR("text/html"), (const char*)INDEX_HTML, sizeof(INDEX_HTML));
}

void syncMarlin() {
  // пытаемся определить текущее состояние принтера путем прослушивания потока UART от Марлин
  // при обнаружении характерных сообщений может быть включено bridgeState = SYS_PRINT
  // учитывается свойство Марлин при выполнении длительных операций отправлять сообщение каждые 2 сек.
  // соответственно, ищется пауза в потоке сообщений длиннее 2 сек.
  // на процесс синхронизации выделено общее время 20 сек
  // первые 6 сек пауза в потоке сообщений не отслеживается
  srvSync = true;                             // флаг - признак работы в режиме синхронизации
  uint32_t syncStart = 0, tNow = 0;
  static uint32_t pauseStart = 0;
  do {
    tNow = millis();
    if (!(syncStart)) syncStart = tNow;       // фиксируем время начала всего цикла
    getMarlin();                              // пробуем читать и обрабатывать RX поток
    // (getMarlinRXTime != 0) - флаг наличия RX данных
    // если были RX данные, либо uptime < 6 сек - сбрасываем время начала отсчета паузы
    if ((getMarlinRXTime) || ((tNow - syncStart) < 6000)) pauseStart = tNow;
    ESP.wdtFeed();                            // сбрасываем watchdog таймер
                                              // проверяем условия продолжения цикла
    } while (((tNow - syncStart) < 20000) &&  // общее время выполнения менее 20 сек
              (errCode == ERR_NO_ERRORS)  &&  // после getMarlin() нет ошибок
              srvSync                     &&  // после getMarlin() флаг не сбросился
              ((tNow - pauseStart) < 2500));  // "молчание" Марлин более 2.5 сек не зафиксировано
  if (errCode != ERR_NO_ERRORS) ESP.restart();// при обнаружении ошибок - рестарт
  if ((bridgeState == SYS_PRINT) || (pubXYZ && !samePos))   // || ((tNow - syncStart) >= 20000)
    prnFix(0, true);                          // Включаем фиолетовый UI (состояние печати)
}

void setup() {
  //system_update_cpu_freq(160);
  // Получаем информацию о последнем сбросе
  // и помещаем в в конец буфера gAnswer_buf, где она сохранится до первого вызова init_chunks()
  char* buf = &gAnswer_buf[GANSWER_BUF_SIZE - sizeof(packet)];  // до первого запуска передачи файла размер gAnswer_buf -= sizeof(packet)
  rst_info* rInfo = ESP.getResetInfoPtr();
  chunks[0].len = snprintf_P(buf, NET_DATA_MAX, PSTR(
                                "L:Reset reason is %s.\n"
                                "L:Reason code: %d, Exception cause: %d\n"
                                "L:-------\n"),
                                ESP.getResetReason().c_str(), rInfo->reason, rInfo->exccause);
  // Детальный код причины
  // REASON_WDT_RST = 1 (Превышение времени выполнения / Soft WDT)
  // REASON_EXCEPTION_RST = 2 (Аппаратный краш: деление на ноль, неверный адрес)
  // REASON_SOFT_RESTART = 3
  // REASON_HARD_RESTART = 4 (Hardware WDT / Питание / кнопка Reset)
  // Если упало по исключению, выводим адреса регистров (EPC)
  if (rInfo->reason == REASON_EXCEPTION_RST) {
    chunks[0].len += snprintf_P(buf + chunks[0].len, NET_DATA_MAX - chunks[0].len, PSTR(
                                  "L:Fatal exception:\n"
                                  "L:  epc1=0x%08x, epc2=0x%08x, epc3=0x%08x\n"
                                  "L:  excvaddr=0x%08x, depc=0x%08x\n"
                                  "L:-------\n"),
                                  rInfo->epc1, rInfo->epc2, rInfo->epc3,
                                  rInfo->excvaddr, rInfo->depc);
    }

  // Инициализация UART, включение прерываний RX
  serial2uart(true);  // flush&close Arduino's Serial, init UART0&UART1 for interrupt-driven operations
  //WiFi.setOutputPower(14);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);

  // Постоянные обработчики для страницы /config (доступ программно разрешаем только в режиме WIFI_AP)
  httpServer.on("/config", HTTP_GET,  handleGetConfigPage);
  httpServer.on("/config", HTTP_POST, handleSaveConfig);
  // Читаем память. Если данных НЕТ (первый запуск) — реализуем цикл WIFI_AP
  if (!loadConfigFromEEPROM()) {
    // Если сигнатура или CRC не совпали — данные повреждены!
    while (sessionID == 0) sessionID = os_random(); // создаем токен
    sta2ap();                         // поднимаем AP для доступа к /config
    while (apTime > 0) {              // цикл первой настройки ~120 сек
      int len = snprintf_P((char*)packet, sizeof(packet), PSTR("M117 Setup mode AP: %d s.\n"), apTime);
      uart_w((char*)packet, len);
      for (int i = 0; i < 10000; i++) {
        httpServer.handleClient(); delay(1); }
      apTime -= 10; }                 // декремент таймера на 10 сек
    ESP.restart();                    // перезагружаемся через 2 минуты, если не было коннекта
    }
  // вычислим смещение в строках командных топиков до идентификатора-селектора ("G/" или "U/")
  cmd_prefix_len = snprintf_P((char*)packet, sizeof(packet), PSTR(STRINGIZE(MY_PREFIX) "/%s_"), cfg.device_id);
  
  // 0,125 second interval soft timer
  os_timer_disarm(&os_125ms_timer);
  os_timer_setfn(&os_125ms_timer, (os_timer_func_t*)timer_125ms_func, NULL);
  os_timer_arm(&os_125ms_timer, 125, 1);   // load os_timer counter for 0,125 second period

  syncMarlin();
  uartOff();
  
  // создаем обработчики веб-страницы WS клиента
  httpServer.on("/", HTTP_GET, handleMainPage);
  httpServer.on("/ha", HTTP_GET, handleMainPage); // для iFrame карточки Home Assistant

  httpServer.begin();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  stat_init();                        // инициализируем блок статистики
}
