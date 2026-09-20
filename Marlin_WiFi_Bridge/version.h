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

const char _VERSION_[] PROGMEM = "v.1.2.0";
// Объединяем системные макросы в единую константу времени сборки
const char COMPILE_TIMESTAMP[] PROGMEM = __DATE__ " " __TIME__;
