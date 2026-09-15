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

#define STRINGIZE_NX(A) #A
#define STRINGIZE(A) STRINGIZE_NX(A)

// can change it to personalize
#define MY_PREFIX marlin_bridge

/* MQTT discovery auto-configuration strings for HA entities */
// для публикации состояний всех сущностей используется один общий топик.
// поэтому шаблоны val_tpl и json_attr_tpl заставляют HA обращаться к текущим значениям состояния тех сущностей и атрибутов,
// которые "не упоминаются" в принятом сообщении
// в общем случае это позволяет публиковать в топик состояния только действительно обновленные значения
// кроме того, эти шаблоны переключают все сущности в неопределенное состояние при получении в этот же общий топик
// "посмертного" LWT сообщения

// active ID (U)
// number, идентификатор текущего активного клиента
// has attribute "uf" = HH:MM:SS (formatted)
// has attribute "rssi" = WiFi Rx level
// commands are: "0" -> reboot, "1" -> MQTT reset, "2" -> printer emergency stop, "3" -> reset EEPROM to enter config
// переключается online/offline при (не)получении LWT сообщения
const char conf_payload_u[] PROGMEM =
"{\"dev\":{\"name\":\"%s\",\"mdl\":\"Marlin_WiFi\",\"mf\":\"PY sw\'n\'hw\",\"ids\":[\"20260501\"]},\"o\":{\"name\":\"%s\"},\
\"ic\":\"mdi:clock-outline\",\"uniq_id\":\"%s_U\",\"obj_id\":\"%s_U\",\
\"name\":\"U\",\"~\":\"" STRINGIZE(MY_PREFIX) "/%s\",\
\"min\":\"0\",\"max\":\"4294967295\",\"step\":\"1\",\"mode\":\"box\",\
\"cmd_t\":\"~_U/cmd\",\"stat_t\":\"~/state\",\"json_attr_t\":\"~/state\",\
\"avty_t\":\"~/state\",\"avty_tpl\":\"{%% if value_json.status == \'OFFLINE\' %%}offline{%% else %%}online{%% endif %%}\",\
\"val_tpl\":\"{%% if value_json.u is defined %%}{{ value_json.u }}{%% elif this.state is number %%}{{ this.state }}{%% else %%}{{ states('number.' ~ '%s_U') | float(0) }}{%% endif %%}\",\
\"json_attr_tpl\":\"{%% if value_json.uf is defined and value_json.rssi is defined %%}{{ dict(Uptime=value_json.uf, RSSI=value_json.rssi ~ ' dBm') | tojson }}{%% else %%}{{ this.attributes | tojson }}{%% endif %%}\"}";

// Температура хотэнда (TH)
// sensor, hotend temperature
// has attribute "tht" = target hotend temperature
// переключается online/offline при (не)получении LWT сообщения
const char conf_payload_th[] PROGMEM =
"{\"dev\":{\"name\":\"%s\",\"mdl\":\"Marlin_WiFi\",\"mf\":\"PY sw\'n\'hw\",\"ids\":[\"20260501\"]},\"o\":{\"name\":\"%s\"},\
\"ic\":\"mdi:printer-3d-nozzle-heat\",\"uniq_id\":\"%s_TH\",\"obj_id\":\"%s_TH\",\
\"name\":\"TH\",\"~\":\"" STRINGIZE(MY_PREFIX) "/%s\",\
\"stat_t\":\"~/state\",\"unit_of_meas\":\"°C\",\"json_attr_t\":\"~/state\",\
\"avty_t\":\"~/state\",\"avty_tpl\":\"{%% if value_json.status == \'OFFLINE\' %%}offline{%% else %%}online{%% endif %%}\",\
\"val_tpl\":\"{%% if value_json.th is defined %%}{{ value_json.th }}{%% else %%}{{ this.state }}{%% endif %%}\",\
\"json_attr_tpl\":\"{%% if value_json.tht is defined %%}{{ dict(Target_t=value_json.tht) | tojson }}{%% else %%}{{ this.attributes | tojson }}{%% endif %%}\"}";

// Температура стола (TB)
// sensor, hotbed temperature
// has attribute "tbt" = target bed temperature
// переключается online/offline при (не)получении LWT сообщения
const char conf_payload_tb[] PROGMEM =
"{\"dev\":{\"name\":\"%s\",\"mdl\":\"Marlin_WiFi\",\"mf\":\"PY sw\'n\'hw\",\"ids\":[\"20260501\"]},\"o\":{\"name\":\"%s\"},\
\"ic\":\"mdi:radiator\",\"uniq_id\":\"%s_TB\",\"obj_id\":\"%s_TB\",\
\"name\":\"TB\",\"~\":\"" STRINGIZE(MY_PREFIX) "/%s\",\
\"stat_t\":\"~/state\",\"unit_of_meas\":\"°C\",\"json_attr_t\":\"~/state\",\
\"avty_t\":\"~/state\",\"avty_tpl\":\"{%% if value_json.status == \'OFFLINE\' %%}offline{%% else %%}online{%% endif %%}\",\
\"val_tpl\":\"{%% if value_json.tb is defined %%}{{ value_json.tb }}{%% else %%}{{ this.state }}{%% endif %%}\",\
\"json_attr_tpl\":\"{%% if value_json.tbt is defined %%}{{ dict(Target_t=value_json.tbt) | tojson }}{%% else %%}{{ this.attributes | tojson }}{%% endif %%}\"}";

// Координата X
// sensor, x coordinate
// has attribute "xs" = steps equivalent for current X value
// переключается online/offline при (не)получении LWT сообщения
const char conf_payload_x[] PROGMEM =
"{\"dev\":{\"name\":\"%s\",\"mdl\":\"Marlin_WiFi\",\"mf\":\"PY sw\'n\'hw\",\"ids\":[\"20260501\"]},\"o\":{\"name\":\"%s\"},\
\"ic\":\"mdi:axis-x-arrow\",\"uniq_id\":\"%s_X\",\"obj_id\":\"%s_X\",\
\"name\":\"X\",\"~\":\"" STRINGIZE(MY_PREFIX) "/%s\",\
\"stat_t\":\"~/state\",\"unit_of_meas\":\"mm\",\"json_attr_t\":\"~/state\",\
\"avty_t\":\"~/state\",\"avty_tpl\":\"{%% if value_json.status == \'OFFLINE\' %%}offline{%% else %%}online{%% endif %%}\",\
\"val_tpl\":\"{%% if value_json.x is defined %%}{{ value_json.x }}{%% else %%}{{ this.state }}{%% endif %%}\",\
\"json_attr_tpl\":\"{%% if value_json.xs is defined %%}{{ dict(Steps_equivalent=value_json.xs) | tojson }}{%% else %%}{{ this.attributes | tojson }}{%% endif %%}\"}";

// Координата Y
// sensor, y coordinate
// has attribute "ys" = steps equivalent for current Y value
// переключается online/offline при (не)получении LWT сообщения
const char conf_payload_y[] PROGMEM =
"{\"dev\":{\"name\":\"%s\",\"mdl\":\"Marlin_WiFi\",\"mf\":\"PY sw\'n\'hw\",\"ids\":[\"20260501\"]},\"o\":{\"name\":\"%s\"},\
\"ic\":\"mdi:axis-y-arrow\",\"uniq_id\":\"%s_Y\",\"obj_id\":\"%s_Y\",\
\"name\":\"Y\",\"~\":\"" STRINGIZE(MY_PREFIX) "/%s\",\
\"stat_t\":\"~/state\",\"unit_of_meas\":\"mm\",\"json_attr_t\":\"~/state\",\
\"avty_t\":\"~/state\",\"avty_tpl\":\"{%% if value_json.status == \'OFFLINE\' %%}offline{%% else %%}online{%% endif %%}\",\
\"val_tpl\":\"{%% if value_json.y is defined %%}{{ value_json.y }}{%% else %%}{{ this.state }}{%% endif %%}\",\
\"json_attr_tpl\":\"{%% if value_json.ys is defined %%}{{ dict(Steps_equivalent=value_json.ys) | tojson }}{%% else %%}{{ this.attributes | tojson }}{%% endif %%}\"}";

// Координата Z
// sensor, z coordinate
// has attribute "zs" = steps equivalent for current Z value
// переключается online/offline при (не)получении LWT сообщения
const char conf_payload_z[] PROGMEM =
"{\"dev\":{\"name\":\"%s\",\"mdl\":\"Marlin_WiFi\",\"mf\":\"PY sw\'n\'hw\",\"ids\":[\"20260501\"]},\"o\":{\"name\":\"%s\"},\
\"ic\":\"mdi:axis-z-arrow\",\"uniq_id\":\"%s_Z\",\"obj_id\":\"%s_Z\",\
\"name\":\"Z\",\"~\":\"" STRINGIZE(MY_PREFIX) "/%s\",\
\"stat_t\":\"~/state\",\"unit_of_meas\":\"mm\",\"json_attr_t\":\"~/state\",\
\"avty_t\":\"~/state\",\"avty_tpl\":\"{%% if value_json.status == \'OFFLINE\' %%}offline{%% else %%}online{%% endif %%}\",\
\"val_tpl\":\"{%% if value_json.z is defined %%}{{ value_json.z }}{%% else %%}{{ this.state }}{%% endif %%}\",\
\"json_attr_tpl\":\"{%% if value_json.zs is defined %%}{{ dict(Steps_equivalent=value_json.zs) | tojson }}{%% else %%}{{ this.attributes | tojson }}{%% endif %%}\"}";


// Экструдер E
// sensor, extruder position
// has attribute "S_XYZ" = Marlin's debug `S_XYZ` motor tracking output (S)
// переключается online/offline при (не)получении LWT сообщения
const char conf_payload_e[] PROGMEM =
"{\"dev\":{\"name\":\"%s\",\"mdl\":\"Marlin_WiFi\",\"mf\":\"PY sw\'n\'hw\",\"ids\":[\"20260501\"]},\"o\":{\"name\":\"%s\"},\
\"ic\":\"mdi:printer-3d-nozzle\",\"uniq_id\":\"%s_E\",\"obj_id\":\"%s_E\",\
\"name\":\"E\",\"~\":\"" STRINGIZE(MY_PREFIX) "/%s\",\
\"stat_t\":\"~/state\",\"unit_of_meas\":\"mm\",\"json_attr_t\":\"~/state\",\
\"avty_t\":\"~/state\",\"avty_tpl\":\"{%% if value_json.status == \'OFFLINE\' %%}offline{%% else %%}online{%% endif %%}\",\
\"val_tpl\":\"{%% if value_json.e is defined %%}{{ value_json.e }}{%% else %%}{{ this.state }}{%% endif %%}\",\
\"json_attr_tpl\":\"{%% if value_json.s is defined %%}{{ dict(SXYZ=value_json.s) | tojson }}{%% else %%}{{ this.attributes | tojson }}{%% endif %%}\"}";

// Прогресс печати (P)
// sensor, operation progress
// has attribute "pf" = file name
// переключается online/offline при (не)получении LWT сообщения
const char conf_payload_p[] PROGMEM =
"{\"dev\":{\"name\":\"%s\",\"mdl\":\"Marlin_WiFi\",\"mf\":\"PY sw\'n\'hw\",\"ids\":[\"20260501\"]},\"o\":{\"name\":\"%s\"},\
\"ic\":\"mdi:progress-check\",\"uniq_id\":\"%s_P\",\"obj_id\":\"%s_P\",\
\"name\":\"P\",\"~\":\"" STRINGIZE(MY_PREFIX) "/%s\",\
\"stat_t\":\"~/state\",\"unit_of_meas\":\"%%\",\"json_attr_t\":\"~/state\",\
\"avty_t\":\"~/state\",\"avty_tpl\":\"{%% if value_json.status == \'OFFLINE\' %%}offline{%% else %%}online{%% endif %%}\",\
\"val_tpl\":\"{%% if value_json.p is defined %%}{{ value_json.p }}{%% else %%}{{ this.state }}{%% endif %%}\",\
\"json_attr_tpl\":\"{%% if value_json.pf is defined %%}{{ dict(File_name=value_json.pf) | tojson }}{%% else %%}{{ this.attributes | tojson }}{%% endif %%}\"}";

// Сущность статуса и ввода G-кода (G)
// text entity to publish HeartBeat state and to get incoming G-code commands
// has attribute dimensions <- ("dim") = JSON string with 3d printer dimensions
// принимает значение OFFLINE при получении LWT сообщения
// используется специальный шаблон json_attr_tpl, в котором дополнительно :
// переменная q используется для генерации символа двойной кавычки в шаблоне атрибутов и затем
// для приведения к каноническому JSON виду имена ключей в парах "ключ":<значение> "оборачиваются" в двойные кавычки,
// которые изначально отсутствуют в строке с габаритами ответа Марлин на команду M115
const char conf_payload_g[] PROGMEM =
"{\"dev\":{\"name\":\"%s\",\"mdl\":\"Marlin_WiFi\",\"mf\":\"PY sw\'n\'hw\",\"ids\":[\"20260501\"]},\"o\":{\"name\":\"%s\"},\
\"ic\":\"mdi:state-machine\",\"uniq_id\":\"%s_G\",\"obj_id\":\"%s_G\",\
\"name\":\"G\",\"~\":\"" STRINGIZE(MY_PREFIX) "/%s\",\
\"min\":\"1\",\"max\":\"60\",\"e\":\"\",\"ret\":\"false\",\
\"cmd_t\":\"~_G/cmd\",\"stat_t\":\"~/state\",\"json_attr_t\":\"~/state\",\
\"val_tpl\":\"{%% if value_json.status == \'OFFLINE\' %%}OFFLINE{%% elif value_json.g is defined %%}{{ value_json.g }}{%% else %%}{{ this.state }}{%% endif %%}\",\
\"json_attr_tpl\":\"{%% if value_json.dim is defined and value_json.dim != '' %%}{%% set q = '{:c}'.format(34) %%}\
{%% set json_str = value_json.dim | replace('{', '{' ~ q) | replace(':', q ~ ':') | replace(',', ',' ~ q) %%}\
{{ dict(dimensions = (json_str | from_json)) | tojson }}\
{%% else %%}{{ this.attributes | tojson }}{%% endif %%}\"}";

const char state_topic[] PROGMEM  = STRINGIZE(MY_PREFIX) "/%s/state";
const char lwt_str[]     PROGMEM  = "{\"status\":\"OFFLINE\"}";
size_t cmd_prefix_len = 0;    //  sizeof(STRINGIZE(MY_PREFIX) "/IoT_" STRINGIZE(UNIQ_ID) "_") - 1;  -->>  setup()

// 1. Уникальный ID устройства и базовый префикс (из макроса компилятора)
// const char mqtt_dev_id[]  PROGMEM = "IoT_" STRINGIZE(UNIQ_ID);
// из-за добавления конфигуратора хранится в переменной cfg.device_id

// 2. Строковые имена типов сущностей
const char name_sensor[]  PROGMEM = "sensor";
const char name_switch[]  PROGMEM = "switch";
const char name_binsens[] PROGMEM = "binary_sensor";
const char name_number[]  PROGMEM = "number";
const char name_light[]   PROGMEM = "light";
const char name_cover[]   PROGMEM = "cover";
const char name_text[]    PROGMEM = "text";
// Таблица указателей на имена типов сущностей
const char* const fn_id[] PROGMEM __attribute__((aligned(4))) = {
    name_sensor, name_switch, name_binsens, name_number, name_light, name_cover, name_text
  };
// 3. Короткие идентификаторы топиков
const char t_id_TH[] PROGMEM = "TH";
const char t_id_TB[] PROGMEM = "TB";
const char t_id_X[]  PROGMEM = "X";
const char t_id_Y[]  PROGMEM = "Y";
const char t_id_Z[]  PROGMEM = "Z";
const char t_id_P[]  PROGMEM = "P";
const char t_id_G[]  PROGMEM = "G";
const char t_id_E[]  PROGMEM = "E";
const char t_id_U[]  PROGMEM = "U";
// 4. Типы данных и структура
typedef enum HA_entity_t { 
    SENSOR_IDX = 0, SWITCH_IDX, BINSENSOR_IDX, NUMBER_IDX, LIGHT_IDX, COVER_IDX, TEXT_IDX 
  } HA_entity_t;
typedef enum discovery_t { 
    DISCOVERY_EXPIRED = 0, DISCOVERY_CONNECTED, DISCOVERY_ANNOUNCED
  } discovery_t;

typedef struct {
    const HA_entity_t fn_id;  // 4 байта (выровнено изначально)
    const char* topic_id;     // 4 байта (выровнено изначально)
    const char* payload;      // 4 байта (выровнено изначально)
    const uint16_t magic;     // 2 байта
    bool isCmdTopic;          // 1 байт
    uint8_t padding;          // 1 байт (добавляем вручную для ровного счета до 16 байт)
  } __attribute__((aligned(4))) pub_data_t;

#define DISCOVERY_COUNT 11    // число необходимых операций для реализации HA discovery
#define MQTT_MAGIC 0x4D51     // Магическое число "MQ" (Marlin MQTT)

// 5. Динамический массив discovery-состояний (ЕДИНСТВЕННОЕ, что создается в RAM — всего 11 значений)
discovery_t dState[DISCOVERY_COUNT] __attribute__((aligned(4))) = {
  DISCOVERY_EXPIRED, DISCOVERY_EXPIRED, DISCOVERY_EXPIRED, DISCOVERY_EXPIRED,
  DISCOVERY_EXPIRED, DISCOVERY_EXPIRED, DISCOVERY_EXPIRED, DISCOVERY_EXPIRED,
  DISCOVERY_EXPIRED, DISCOVERY_EXPIRED, DISCOVERY_EXPIRED
  };
// 6. Массив структур полностью в PROGMEM
const pub_data_t pub_data[DISCOVERY_COUNT] PROGMEM __attribute__((aligned(4))) = {
  {SENSOR_IDX, t_id_TH,  conf_payload_th,  MQTT_MAGIC,  false,  0}, // hotend temperature config
  {SENSOR_IDX, t_id_TB,  conf_payload_tb,  MQTT_MAGIC,  false,  0}, // hotbed temperature config
  {SENSOR_IDX,  t_id_X,  conf_payload_x,   MQTT_MAGIC,  false,  0}, // X coordinate config
  {SENSOR_IDX,  t_id_Y,  conf_payload_y,   MQTT_MAGIC,  false,  0}, // Y coordinate config
  {SENSOR_IDX,  t_id_Z,  conf_payload_z,   MQTT_MAGIC,  false,  0}, // Z coordinate config
  {SENSOR_IDX,  t_id_P,  conf_payload_p,   MQTT_MAGIC,  false,  0}, // operation progress config
  {TEXT_IDX,    t_id_G,  conf_payload_g,   MQTT_MAGIC,  false,  0}, // G-code command config
  {TEXT_IDX,    t_id_G,  conf_payload_g,   MQTT_MAGIC,  true,   0}, // command topic for G-code command set
  {SENSOR_IDX,  t_id_E,  conf_payload_e,   MQTT_MAGIC,  false,  0}, // extruder position config
  {NUMBER_IDX,  t_id_U,  conf_payload_u,   MQTT_MAGIC,  false,  0}, // active ID config
  {NUMBER_IDX,  t_id_U,  conf_payload_u,   MQTT_MAGIC,  true,   0}  // command topic for reset/control command
  };

bool pubTB = false, pubXYZ = false, pubProgress = false;
uint32_t timeTB = 0, timeXYZ = 0, timeProgress = 0, timeMQTTCtrl = 0, mqttBusyTime = 10000;

int pubState = HB_IDLE;       // defined in "WiFi_BFT.h"
// Строковые имена публикуемых состояний системы
const char state_idle[]   PROGMEM =  "IDLE";
const char state_upload[] PROGMEM =  "UPLOAD";
const char state_ready[]  PROGMEM =  "READY";
const char state_print[]  PROGMEM =  "PRINT";
const char state_error[]  PROGMEM =  "ERROR";
const char state_wait[]   PROGMEM =  "WAIT";
const char state_ota[]    PROGMEM =  "OTA MODE";
const char state_unknown[] PROGMEM = "UNKNOWN";
// Таблица указателей на имена публикуемых состояний системы
const char* const pubState_id[] PROGMEM __attribute__((aligned(4))) = {
    state_idle, state_upload, state_ready, state_print, state_error, state_wait, state_ota, state_unknown
  };
