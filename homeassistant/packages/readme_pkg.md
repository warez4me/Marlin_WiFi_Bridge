## 📊 Package Architecture & Virtual Sensors

The `packages/iot_7.yaml` template uses the native Home Assistant modern `template:` domain. It shifts processing loads away from your dashboard UI by pre-calculating raw MQTT states and attributes on the backend level using highly optimized Jinja2 expressions.

### 🌡️ Temperature Telemetry Simplification
Instead of forcing you to place multiple raw badges onto your dashboard view, the package automatically aggregates state values and custom attributes into human-readable strings:
* **`sensor.iot_7_th_str` (Hotend):** Extracts the primary state from `sensor.iot_7_th` and the `Target_t` attribute, compiling them instantly into a unified string layout (e.g., `215.0°C / 215.0°C`). It uses the dedicated `mdi:printer-3d-nozzle-heat` icon entity.
* **`sensor.iot_7_tb_str` (Heated Bed):** Implements identical layout consolidation for the heated bed utilizing `sensor.iot_7_tb` data, paired with the `mdi:radiator` icon structure.

### 📶 Dynamic Wi-Fi RSSI Diagnostics (`sensor.iot_7_rssi`)
The bridge firmware transmits the signal strength as a combined string inside the attributes matrix. The package parses this payload using a clean string cleaning routine:
1. **String Stripping:** The Jinja2 expression safely captures the `RSSI` attribute from `number.iot_7_u` and uses a `| replace(' dBm', '') | int` filter chain to isolate the raw numeric integer.
2. **Device Class Mapping:** By defining `device_class: signal_strength` and assigning `unit_of_measurement: "dBm"`, Home Assistant automatically flags this sensor as a standardized diagnostic tracker.
3. **Automated Adaptive Icon Rendering:** The frontend Lovelace engine evaluates the integer value and dynamically alters the bars of the active Wi-Fi icon using the following native system thresholds:
   * **4 Bars** (`mdi:wifi-strength-4`): From `-50 dBm` and above *(Excellent connection)*.
   * **3 Bars** (`mdi:wifi-strength-3`): From `-66 dBm` to `-51 dBm` *(Good/Stable signal)*.
   * **2 Bars** (`mdi:wifi-strength-2`): From `-78 dBm` to `-67 dBm` *(Fair/Medium connection)*.
   * **1 Bar** (`mdi:wifi-strength-1`): From `-89 dBm` to `-79 dBm` *(Weak connection / Potential BFT packet delay dropouts)*.
   * **Empty Outline** (`mdi:wifi-strength-outline`): `-90 dBm` and below *(Critical / Hardware timeout warning)*.
