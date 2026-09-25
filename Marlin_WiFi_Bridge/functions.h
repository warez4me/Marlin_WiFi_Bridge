#include <sys/types.h>
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

/*--------------------------*/
/*------ bft_uart.ino ------*/
//---- UART1 function -----///
//void beep(uint);
//---- UART0 functions -----//
void IRAM_ATTR uart0_intr_handler(void*, void*);
inline void uartOff();
inline void uartOn();
/*inline char uart_r_touch();
inline char __uart_r_chr(bool flush = true);
char uart_r_chr();
uint32_t uart_r_str(char*, uint32_t);*/
char uart_r_chr(bool flush = true);
inline uint32_t uart_r_avail();
uint uart_w(char*, uint len = 0, bool flush = false);
void clear_uart();
void serial2uart(bool enaInt = false);
void uart2serial();

/*--------------------------*/
/*------- net_io.ino -------*/
inline uint32_t ringBufferPut(uint32_t, const uint8_t*, uint32_t);
inline uint32_t ringBufferGet(uint32_t, uint8_t*, uint32_t);
bool netQueFlush(uint32_t, bool);
bool netQuePut(uint8_t* msg, int32_t msgSize, char* preMsg = NULL, uint8_t cid = WS_ALL);
inline bool netQuePut_pre(uint8_t* msg, int32_t msgSize, char* preMsg) {
  return netQuePut(msg, msgSize, preMsg);
}
inline bool netQuePut_cid(uint8_t* msg, int32_t msgSize, uint8_t cid) {
  return netQuePut(msg, msgSize, NULL, cid);
}
void netQueSend();
void netQueClean();

/*--------------------------*/
/*------- service.ino ------*/
bool getNum(uint32_t*, char**, uint32_t, bool skipSp = false);
void doReboot(bool sidReset = false);
void showTime(uint32_t show_cid);
void showIP(uint32_t);
bool setWrkPath(bool show = false);
void setState();
int confirmStr(strMem_t*, char*, size_t);
inline void fListGet(int showBegIdx = 0);
bool myBridgeCmd(char*, size_t);
void printStat();
void delFileGcode(BridgeState_t, uint32_t txDelay = 0);
void sd_erase();

/*--------------------------*/
/*------- ganswer.ino ------*/
void updFloatValue(gAnswerID_t, uint32_t, float, float val2 = 0.0);
void updIntValue(gAnswerID_t, uint32_t, uint32_t, uint32_t val2 = 0);
void prnFix(int, bool prnDetected = false);
inline bool isWrkPath();
void fListInfo();
bool fileInfo(int mode = -1, bool show = true);
int cmpFPath();
int fileID(bool, uint32_t, bool setPrint = true);
void showFileList(uint32_t toShow = 0);
int parseFileInfo(char*);
void doResend();
gAnswerID_t findID(char*&, uint&, bool);
inline uint32_t csFInfo(char*);
void logMsg(unsigned char, uint32_t);
inline uint16_t xorCS(char*, size_t len = 0);
void getMarlin();

/*--------------------------------*/
/*-------- ota_control.ino -------*/
void onOTAStart();
void onOTAProgress(unsigned int, unsigned int);
void writeFlash();
bool setOTA();

/*--------------------------*/
/*-------- mqtt.ino -------*/
inline void mqttPackLength(uint8_t* &ptr, uint32_t);
bool buildMqttTopic(int, bool*, const char**);
void mqttDisconnect(bool closeSocket = true);
int  mqttConnect(const char*, uint16_t, const char*, const char*, const char*);
bool mqttSubscribe(const char*);
bool mqttPublish(const char*, const char*, bool);
void mqttSocketHandler();
void mqttLoop();

/*--------------------------*/
/*-------- setup.ino -------*/
void clearConfigInEEPROM();
uint16_t calculateCRC16(const uint8_t*, size_t);
bool loadConfigFromEEPROM();
void handleGetConfigPage();
uint16_t parseIP(char*);
void handleSaveConfig();
void sta2ap(const char* msg = NULL);
void syncMarlin();
void setup();

/*--------------------------*/
/*----- WiFi_BFT**.ino -----*/
void timer_125ms_func(void*);
char* fl2chr(const char*);
void wifiConnect(bool init=false);
void initWPath(char*, uint8_t);
inline void init_chunks();
void br_announce(annEvent_t, uint8_t, uint8_t, uint32_t announceID = 0);
WSMessage_t wsMsgID(char*, size_t, uint32_t*);
void webSocketEvent(uint8_t, WStype_t, uint8_t*, size_t);
inline uint32_t csFletchers16(uint32_t, uint8_t);
void sendBFT(size_t, uint8_t, uint8_t pDummy = 0, uint8_t pShrink = 0);
void countData(uint32_t);
void putMarlin();
void checkTimers();
void checkErrors();
void heartbeat(bool now = false);
