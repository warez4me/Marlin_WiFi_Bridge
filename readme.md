# WiFi-UART Bridge for Marlin 3D Printers (ESP8266)

An ultra-optimized, high-performance software-hardware stack that transforms an ESP8266 into an intelligent industrial terminal for Marlin-based 3D printers.


## ⚠️ Project Status & Translation Disclaimer

* **Target Audience:** This project is tailored for **tech-savvy enthusiasts**. Successfully deploying this bridge requires comfortable handling of microelectronics, understanding your printer's motherboard pinouts, and the ability to compile custom Marlin firmware. If you face any configuration bottlenecks, remember that modern AI assistants are exceptionally good at helping you locate specific pins or debugging Marlin compilation errors.

- **This project is currently shared "As-Is".** The source code comments and some pieces of documentation are primarily written in Russian.

- **Tip for international users :** You can easily read and understand the codebase by leveraging modern AI tools (like ChatGPT, Claude, or DeepL) to translate the files or individual blocks on the fly.

- **Contributions welcome!** If you find this project useful and want to help the community, feel free to submit a Pull Request (PR) with English translations for code comments or documentation.

---

<a id="table-of-contents"></a>

## 📋 Table of Contents
* [🗺️ System Architecture Overview](#architecture-overview)
* [⚡ Core Technical Features](#core-features)
  * [1. Server-Side Kernel (ESP8266 C/C++)](#kernel-details)
  * [2. Networking & Buffering Engines](#networking-engines)
  * [3. Ultra-Lightweight Web Client (JS/HTML)](#web-client)
* [📂 File Management & Transfer Modes](#file-management)
  * [Transfer Pipelines](#transfer-pipelines)
  * [Selection & Navigation Mechanics](#selection-mechanics)
  * [💾 Printer SD Card Preparation & Limitations](#sd-limits)
* [🔌 Hardware Selection & Requirements](#hardware-selection)
  * [Recommended Boards & Modules](#recommended-boards)
  * [📐 Printer Motherboard Port & Logic Levels](#motherboard-levels)
* [🛠️ DIY Hardware Assembly & Signal Integrity](#hardware-assembly)
  * [🧵 Connecting Cable Specifications](#cable-specs)
  * [📐 ESP-12F Bare Module Pinout & Boot Configuration](#bare-module-pinout)
  * [🎛️ Unified Service Header Recommendation (For Bare Modules)](#service-header)
* [💾 Initial Firmware Installation & Compilation](#firmware-installation)
  * [Method 1: Flashing Pre-Compiled Binary (Recommended)](#method-1-binary)
  * [Method 2: Manual Compilation from Source (For Advanced Users)](#method-2-source)
  * [Subsequent Wireless Updates (OTA)](#ota-updates)
  * [🎛️ Web Assets Optimization & Automating OTA Builds (Python Utilities)](#assets-optimization)
* [⚙️ Marlin Firmware Configuration Requirements](#marlin-requirements)
* [🤖 Home Assistant Integration & MQTT Discovery](#ha-integration)
  * [Command Topics & Subscriptions](#mqtt-topics)
  * [🎛️ Custom Home Assistant Frontend Card (JS Script)](#ha-custom-card)
  * [🛠️ Advanced Home Assistant Deployment & Packages](#ha-advanced-deployment)
* [🌐 Web Client User Interface (UI)](#web-ui)
* [💻 Terminal Commands Overview](#terminal-commands)
* [⚙️ Initial Configuration](#initial-config)
  * [🌐 Wi-Fi Network & Router Optimization](#initial-config)
  * [🚀 Step-by-Step Initial Setup Sub-Routine](#server-config)
* [License & Acknowledgments](#license-and-aknowledgments)

---

<a id="architecture-overview"></a>

## 🗺️ System Architecture Overview

The bridge functions completely on a **Client-Server** model. Online/stream printing is intentionally **not supported**. The main goal is providing fast, resilient wireless access to the printer's SD card to upload files and trigger local printing.

**\[ Web Browser (Vanilla JS) \]**   *(via WebSocket Engine)*  
▲  
▼  
**\[ ESP8266 Server \] ◄──► \[ Home Assistant \]**   *(via Custom Non-Blocking MQTT Stack)*  
▲  
▼  
**\[ Marlin 3D Printer \]**   *(Low-level ISR UART @ 500,000 baud)*

<p align="right"><a href="#table-of-contents">▲ Back to Top</a></p>

---

<a id="core-features"></a>

## ⚡ Core Technical Features

<a id="kernel-details"></a>

### 1. Server-Side Kernel (ESP8266 C/C++)

- **CPU clock :**  is set to **160 MHz**.

- **Low-Level UART Driver :** Implemented via direct register access inside `IRAM_ATTR`. Operates at **500,000 baud** using dedicated 1024-byte ring buffers and interrupts.

- **Binary File Transfer (BFT) :** Full integration of Marlin's Binary File Transfer protocol with speeds up to **32–34 KB/sec**. Uses a 4-chunk ring buffer mechanism to keep the UART pipe completely saturated.

- **Memory Management Optimization :** To save RAM, the 4-chunk BFT buffer array reuses the exact same memory space allocated for caching the SD card file list. All text constants and web templates are stored strictly in `PROGMEM`. Free RAM stays stable at ~20 KB, while IRAM utilization is at 93%.

- **Zero-Copy Parser :** Responses from Marlin are evaluated and parsed on the fly directly inside the receive buffer.

- **State Machine :** Logic is isolated into precise system states, spanning from `SYS_IDLE` and `SYS_TRANSFER` to specific Marlin command waiting blocks (`SYS_WAIT_M20` to `SYS_WAIT_M32`), OTA updates, and configuration sub-routines.

<a id="networking-engines"></a>

### 2. Networking & Buffering Engines

- **Custom WebSocket Ring Buffer :** A 2 KB ring buffer that manages data streams to connected WS clients. Features a dedicated **Urgent packet injection system** to prioritize critical control commands over standard payloads.

- **Custom Lightweight MQTT Stack :** Built directly on top of  `WiFiClient` raw sockets using a non-blocking byte-level state machine.

  - Compares incoming topics/strings instantly using a **Little-Endian memory approach**.

  - Eliminates broker flooding via strict message rate throttling.

  - Supports data ingestion interchangeably from standard RAM and `PROGMEM`.

<a id="web-client"></a>

### 3. Ultra-Lightweight Web Client (JS/HTML)

- **Extreme Compression :** The entire client asset payload (including all UI components and operational logic) is compressed via Zopfli/Gzip into **~3.7 KB**. The interface completely loads over just **3 IP packets** (under 4,380 bytes), bypassing heavy frameworks (React/Vue) for raw Vanilla JS.

- **Reactive UI Framework :** The client UI renders and updates synchronously with 1-second `HeartBeat` packages pushed from the server.

- **Activist vs. Observer Access Pattern :** Multi-client environments are regulated automatically using `cid == sid` handshakes. Only one browser session can be the **Activist** (holding the lock to upload files, execute commands, or start prints). Other clients safely transition to passive **Observers** (read-only monitoring). During system idle periods (when no file is selected or an upload has not been initiated), any **Observer** can intercept the **Activist** role by selecting and announcing their own file.

- **Intelligent WebSocket Time Synchronization :** The system features an elegant, event-driven time synchronization engine over WebSockets that requires zero configuration on the server side. Clock updates seamlessly follow the *Activist/Observer* matrix : by default, the first connected client becomes the temporary "Time Master", passing this role to the **Activist** as soon as it takes control.

- **Connection Endpoints:** The server can accept incoming requests on two paths:
  * **`http://<ESP_IP_Address>`** — Used for standard desktop and mobile web browsers.
  * **`http://<ESP_IP_Address>/ha`** — Reserved specifically for embedding via Home Assistant iFrame panels (currently serves the same optimized lightweight file to save memory, providing a clean upgrade path for future layout experiments).
  * ➔ **Note:** Upon the startup procedure, the server automatically reports its obtained **`<ESP_IP_Address>`** directly onto the physical printer's LCD display for effortless discovery.

<p align="right"><a href="#table-of-contents">▲ Back to Top</a></p>

---

<a id="file-management"></a>

## 📂 File Management & Transfer Modes

The bridge offers two ways to select files for printing and two distinct UART transfer pipelines:

<a id="transfer-pipelines"></a>

### Transfer Pipelines

1. **ASCII Mode :** Text-based G-code is stripped *on the fly* (removing spaces, empty lines, and comments) before entering the UART. This saves **20–30% of UART traffic** and physical SD storage space. No retry/resend logic is supported.

2. **BFT Mode (Binary) :** Utilizes Marlin's binary protocol secured with **Fletcher-16 checksums** and double buffering. Guarantees 1:1 file integrity and automatically performs a soft-exit to safely restore Marlin back to standard ASCII mode if an interruption occurs. Supports up to 5 automatic chunk re-sends on error.

<a id="selection-mechanics"></a>

### Selection & Navigation Mechanics

- **Local Disk Upload (Method 1) :** The user selects a file using the client's [graphical UI](#web-ui) (via the native system file picker). The client UI then automatically announces the file name and size to the server. The server scans the printer's SD card. If a matching name and size are found in the current working directory, it prompts to print immediately. If missing, it prompts to upload.

- **Printer SD Terminal Listing (Method 2) :** Files on the printer's SD are fetched using an 8 KB static buffer supporting names up to 112 characters. The list is outputted using a scannable, adjustable paginated viewport (`PgUp`/`PgDown`). To select a file from this listing for printing, the user can explicitly input its index wrapped in parentheses (using the system's [standard command format](#file-command)).

- **Safe Operations :** Destructive processes use an atomic, two-pass deletion algorithm with initial status flag marking. Directories that are completely empty are injected with a placeholder file to prevent Marlin from dropping directory trees out of index listings.

<a id="sd-limits"></a>

### 💾 Printer SD Card Preparation & Limitations

Since the Marlin firmware does not possess a fully featured Operating System for advanced file and directory manipulation, you must follow strict guidelines when preparing your physical SD card on a PC before inserting it into the printer:

* **Single-Level Directory Structure:** It is highly recommended to design a **flat, single-level folder layout**. Avoid deep, multi-layered nested subdirectories (folders inside folders), as Marlin's lightweight indexer can easily glitch, skip branches, or experience system freezes during deep directory traversal.
* **No Empty Folders (The Directory Placeholder):** Marlin can completely "lose" empty directories from its file listing. Every folder you create must contain at least one file. If you need to create an empty folder for future uploads, place a zero-byte placeholder file inside it named exactly:
  `PATHKEEP.GCO`
  The bridge's internal routing engine is optimized to recognize these placeholders, preventing Marlin from dropping your directory trees out of the general listing.
* **Naming Conventions:** All files and folders **must strictly use the English language** (standard ASCII/EN_US charset). Never use Cyrillic or special characters, as Marlin does not natively support UTF-8/Unicode for filenames and will render them as corrupted data. Keep your names to a reasonable length to ensure smooth paginated terminal viewing.
* **Baseline Integrity Check:** Before attempting to stream and write files wirelessly through the WiFi-UART bridge, **always verify your SD card health first**. Ensure that your printer can flawlessly read and print G-code files that were written to the SD card the "normal" way directly via your PC's card reader. If the printer struggles with local files, the wireless bridge will not stabilize them.

<p align="right"><a href="#table-of-contents">▲ Back to Top</a></p>

---

<a id="hardware-selection"></a>

## 🔌 Hardware Selection & Requirements

Depending on the module you select, pay close attention to the power delivery and logic level matching:

<a id="recommended-boards"></a>

### Recommended Boards & Modules
* **NodeMCU V2/V3 (ESP-12E/F based) or WeMos D1 Mini (4MB Flash):** 
  * Ideal for quick deployment and effortless development.
  * These boards already integrate an onboard **3.3V voltage regulator** and a USB-to-UART bridge. 
  * ⚠️ **Power Delivery Warning:** While it is perfectly fine to power these boards via their Micro-USB port from your computer during initial programming, **do not power them from the printer motherboard's USB port during permanent deployment**. The USB ports on 3D printer mainboards often have weak power rails that cannot handle the ESP8266's sudden 400mA Wi-Fi transmission spikes, leading to voltage sags, ESP reboots, or printer mainboard freezes midway through a print. Instead, power the board's `5V` (or `VIN`) and `GND` pins directly from the printer motherboard's dedicated internal power expansion headers (such as AUX or TFT ports), ensuring the rail can supply at least 1A of clean current.
* **Bare ESP Modules (ESP-12F, ESP-07) or 1MB Modules (ESP-01 / ESP-01S):** 
  * Compact form factors with 1MB of Flash are fully supported by the codebase.
  * **Warning:** Bare modules **require an external 3.3V stabilizer** (e.g., AMS1117-3.3). Connecting the 5V, 12V, or 24V power rails from the 3D printer motherboard directly to the ESP chip will instantly destroy it.
  * Ensure proper pull-up/pull-down boot configurations (`CH_PD/EN` to VCC, `GPIO15` to GND) for hardware stability.

<a id="motherboard-levels"></a>

### 📐 Printer Motherboard Port & Logic Levels

* **Printer Motherboard Port Selection:** When moving the bridge from your PC to the 3D printer, you will need to locate an available hardware UART/Serial port on your printer's mainboard (often shared with AUX or TFT headers). Due to the massive variety of 3D printer motherboards on the market, we cannot provide a universal wiring diagram. You will need to consult your motherboard's official pinout documentation, schematic diagrams, or utilize an AI tool to identify the correct `TX`, `RX`, `GND`, and power rails.

* ⚠️ **Crucial Motherboard Logic Level Warning**
Many industrial 3D printer motherboards (especially older 8-bit AVR boards like RAMPS or Anet) operate on **5V logic levels**, while the ESP8266 strictly uses **3.3V logic**. 
* Connecting the printer's 5V TX pin directly to the ESP8266 RX pin without a **logic level shifter** (or at least a simple voltage divider using 1kΩ and 2kΩ resistors) can degrade or permanently damage the ESP8266 GPIO over time. 
* Modern 32-bit ARM boards (SKR, MKS, etc.) typically use 3.3V logic and can be wired directly.

<p align="right"><a href="#table-of-contents">▲ Back to Top</a></p>

---

<a id="hardware-assembly"></a>

## 🛠️ DIY Hardware Assembly & Signal Integrity

Operating at a high speed of **500,000 baud** in close proximity to a 3D printer's high-current stepper motors and heaters creates a noisy electromagnetic environment. To prevent data corruption, checksum errors, or frozen transfers, you must follow strict wiring practices.

<a id="cable-specs"></a>

### 🧵 Connecting Cable Specifications
The total length of the UART connection cable between the ESP8266 and the printer motherboard **must not exceed 10..12 cm (4..5 inches)**. 

#### Option A: DIY Twisted Pair Wiring (Highly Recommended)
To eliminate electromagnetic interference (EMI) and cross-talk, do not use loose jumper wires. Instead, manufacture a custom cable harness consisting of **three individually twisted pairs**:
1. **The Architecture:** Create three distinct pairs where each active signal line (`VCC`, `RX`, and `TX`) is tightly twisted together with its own dedicated **Power Ground (Minus / GND)** wire.
2. **The Twist Rate:** Aim for approximately **2 to 3 full twists per centimeter** (tighter is better). Tight twisting ensures excellent common-mode noise rejection, protecting the sensitive serial data streams from high-frequency radiation emitted by the printer's stepper drivers.
3. **Termination:** At both ends of the completed ~10 cm cable, all three ground (Minus) conductors must be connected together into a single shared node using the shortest path possible.

#### Option B: Ready-Made Factory Alternatives
If you prefer not to build a cable from scratch, the following ready-made components provide excellent signal integrity within the ~10 cm limit:
* **Short Premium JST-XH / Dupont Bundles:** Pre-crimped multi-conductor ribbon cables. You will need to manually separate and twist the lines into pairs before plugging them into the headers.
* **Premium Shielded Internal PC Cables:** Salvaged short internal computer cables (such as front-panel USB or HD Audio extensions) often feature high-quality factory-shielded twisted pairs, making them exceptional for low-noise UART links after swapping the terminal connectors.

#### Option C: Flat Ribbon Cable Layering (_Alternative / At Your Own Risk_)
If twisted pairs are completely unavailable, you can use a standard flat ribbon cable as an emergency alternative, provided you follow a strict layout scheme to mimic shielding:
1. **The Grid Layout:** Use a flat cable with a minimum of **7 conductors**. Arrange the lines so that every active signal line is structurally isolated and surrounded by ground (Minus) wires:
   `[GND] ➔ [VCC] ➔ [GND] ➔ [RX] ➔ [GND] ➔ [TX] ➔ [GND]`
2. **The Physics:** Placing a dedicated ground line between each active signal creates an electrostatic barrier that drastically reduces cross-talk and outside EMI.
3. **Termination:** Just like with twisted pairs, all **4 ground (Minus) conductors** must be merged together at both ends of the 10 cm (maximum) cable into a single shared node using the shortest path possible.

<a id="bare-module-pinout"></a>

### 📐 ESP-12F Bare Module Pinout & Boot Configuration
If you are building your bridge using a bare **ESP-12F / ESP-07** module instead of a plug-and-play development board, use the following pin connections and hardware configurations to ensure a stable boot cycle:


| ESP-12F Pin | Connection Type | Target Destination / Pull Resistor | Operational Purpose |
| :--- | :--- | :--- | :--- |
| **VCC** | Power Input | `+3.3V` Stable Output Rail | Main chip power source |
| **GND** | Power Ground | System Power Minus (`GND`) | Main chip ground reference |
| **CH_PD / EN** | Hard Hardware Pull | `10kΩ` Resistor ➔ `+3.3V` | Enables the chip (Chip Select) |
| **REST / RESET**| RC Reset Circuit | `10kΩ` Pull-up to `+3.3V` + `0.1µF` Capacitor to `GND`. *Optional: Tactile push-button in parallel with the capacitor.* | Provides a clean power-on hardware reset delay and filters out high-frequency electrical noise. The optional button acts as a manual **Hardware Reset Button** for easier maintenance. |
| **GPIO15** | Hard Hardware Pull | `10kΩ` Resistor ➔ `GND` | Mandatory pull-down for successful internal bootloader initialization |
| **GPIO2** | Hard Hardware Pull | `10kΩ` Resistor ➔ `+3.3V` | Optional, recommended to enshure boot stability |
| **GPIO0** | Boot Mode Switch | `10kΩ` Resistor ➔ `+3.3V` | **Normal Boot:** Kept High via resistor.<br>**UART Flash Mode:** Pull directly to `GND` during power-on |
| **TXD** | UART Signal Output| Printer Motherboard `RX` pin | Serial data transmit |
| **RXD** | UART Signal Input | Printer Motherboard `TX` pin | Serial data receive (<u>Use a divider if the printer uses 5V logic</u>) |

#### 🔌 Mandatory Decoupling Capacitors
Always solder a **100µF electrolytic capacitor** in parallel with a **0.1µF ceramic capacitor** directly across the physical `VCC` and `GND` pins of the bare ESP module. 

The ESP8266 draws sudden, massive surges of current (up to 400mA) during WiFi transmission bursts. Without these decoupling capacitors placed immediately at the chip pins, voltage sags will cause unpredictable watchdog timer (`wdt`) resets and brownouts midway through file uploads.

<a id="service-header"></a>

### 🎛️ Unified Service Header Recommendation (For Bare Modules)
To drastically simplify the initial flashing process and future maintenance of a bare module, it is highly recommended to route your critical lines to a centralized, multi-pin male header (Dupont/JST grid). 

* ⚠️ **Critical Proximity Rule:** This service header **must be physically located as close as possible to the ESP module RX/TX pins**. Minimizing the trace length between the header and the chip is vital to maintain high signal integrity at 500,000 baud and to prevent voltage drops on the power rails during heavy WiFi transmission bursts.

This layout allows you to use a single modular cable assembly to rapidly hot-swap the bridge between your **3D Printer Motherboard** and your **USB-to-UART PC Flashing Adapter**:

* **Main Interface Block (4 Pins):** Route your Main Power Input (the input rail entering your 3.3V voltage stabilizer), `RX`, `TX`, and `GND` to this main block. During standard deployment, this connects to the printer motherboard. During initial setup, it connects directly to your USB-to-TTL serial adapter.
* **Boot Flash Trigger Block (2 Pins) :** Route `GPIO0` and an adjacent `GND` pin to two separate, closely spaced pins.
  * For **Wired Flashing :** Plug a standard latching switch (or a temporary jumper cap) onto these two pins to clamp `GPIO0` safely to ground.
  * For **Normal Boot / Operation :** Simply remove the jumper or release the switch to let the `10kΩ` resistor pull the pin High.
  * 🔄 **Hardware Reset Requirement :** Note that any change to the state of `GPIO0` will only take effect during the chip's boot cycle. Therefore, after toggling the switch or jumper, you must either cycle the system power or trigger a **hard reset** (which is where the recommended hardware reset button on the `RESET` pin becomes extremely useful).

<p align="right"><a href="#table-of-contents">▲ Back to Top</a></p>

---

<a id="firmware-installation"></a>

## 💾 Initial Firmware Installation & Compilation

The project supports two installation methods: flashing a pre-compiled ready-to-use binary (recommended for a quick setup) or compiling the native Arduino IDE sketch manually from the source code.

<a id="method-1-binary"></a>

### Method 1: Flashing Pre-Compiled Binary (Recommended)
If you do not wish to install tools and compile the source code manually, you can flash a pre-compiled `.bin` file directly to your ESP8266. You can find it in the **Releases** section on the right side of this GitHub repository page.

- 🧩 **Cross-Hardware Compatibility :** Thanks to the integration of dynamic flash mapping architecture, the compiled binary is completely cross-platform. This single production file can be safely flashed to **any ESP8266 module flavor** (including legacy 1MB ESP-01/01S boards as well as standard 4MB modules like the ESP-12E/F, NodeMCU, or WeMos D1 Mini). 

* 🎛️ **Flash Mode Warning (Use DOUT for Flashing):** The pre-compiled universal binary is built to support the highly compatible **DOUT (Dual Output)** flash access mode. This ensures that the code will execute flawlessly on any standard or clone ESP8266 chip without hardware-level blocking. When using flashing software like NodeMCU PyFlasher or Espressif Flash Download Tools, you **must explicitly select DOUT** in the tool's settings. Choosing QOUT or QIO on a module that doesn't support it will prevent the ESP8266 from reading the firmware headers, resulting in instant boot loops. If your module physically supports faster modes (like QOUT on ESP-12F) and you want to compile from source via **[Method 2](#method-2-source)**, you can safely experiment with those advanced parameters in the Arduino IDE.

You can use any of the following widely available, free software utilities to upload the ready `.bin` file (please consult their respective online documentation or community guides for specific step-by-step usage instructions):
* **NodeMCU PyFlasher:** The simplest cross-platform GUI tool (Windows/macOS). Ideal for a quick setup - just select your serial port, point to the `.bin` file, and click flash.
* **Espressif Flash Download Tools:** The official advanced flashing factory utility provided directly by the chip manufacturer (Windows only). Offers complete, low-level control over flashing addresses, SPI modes, and speeds.
* **esptool.py:** A powerful, universal command-line tool written in Python. It is completely cross-platform and is the preferred choice for automated scripts or command-line advanced users (`esptool.py --port COM_PORT write_flash 0x00000 firmware.bin`).

<a id="method-2-source"></a>

### Method 2: Manual Compilation from Source (For Advanced Users)
If you want to modify the internal code or track updates manually, the project can be compiled as a native **Arduino IDE sketch** (`.ino`). Follow these environmental steps to build it without errors:

#### 1. Required Libraries & Dependencies
The following libraries must be installed and active inside your Arduino IDE environment before hitting compile:
* **Built-in Core Libraries:** `ESP8266WiFi`, `ESP8266WebServer`, `EEPROM`, `Hash`.
* **Third-Party Libraries:** `WebSockets` (Version **2.7.2** or compatible by *Links2004*).

#### 2. Mandatory Arduino IDE Tools Menu Settings
Open your Arduino IDE, navigate to the **Tools** menu, and apply the exact parameters required by the system architecture to ensure memory and network stability:

* **Board:** `Generic ESP8266 Module` *(Mandatory for clean hardware-agnostic production builds)*.
* **Flash Size:** `Mapping defined by Hardware and Sketch` ➔ <u>**Crucial.**</u> Enables adaptive layout discovery, removes `Flash size mismatch` boot locks, and automatically pushes the emulated EEPROM sector to the safe physical end of any chip (1MB or 4MB).
* **MMU:** `32KB cache + 32KB IRAM` *(Balanced)* OR `16KB cache + 48KB IRAM` *(IRAM)*
  * *Balanced (Recommended for Performance):* Allocates a larger 32KB cache to ensure maximum file transfer speeds during high-speed BFT uploads.
  * *IRAM (Alternative for High Load Stability):* Expands Instruction RAM up to 48KB. This drops IRAM utilization down to ~68%, offering a defensive safety margin against low-level timing conflicts in highly congested network setups.
* **CPU Frequency:** `160 MHz` *(Required for the 500k baud low-level ISR UART engine)*.
* **lwIP Variant:** `v2 Higher Bandwidth` *(Crucial for high-speed WebSocket execution under load)*.
* **Non-32-Bit Access:** `Use pgm_read macros for IRAM/PROGMEM` ➔ <u>**Strictly required.**</u>
* **Flash Mode:** `DOUT` *(Ensures universal compatibility across all chip variants and clones)*.
* **Reset Method:** `no dtr (aka ck)`
* **VTables:** `Flash`

#### 3. First-Time Wired Flashing Process
1. Download the repository files to your local disk, open the Arduino IDE, and open the project from the 📁 `Marlin_WiFi_Bridge/` folder.
2. Connect your ESP8266 module to your computer using a reliable USB cable (for dev boards) or an external USB-to-TTL UART serial adapter (for bare modules).
3. If compiling for a bare module, pull `GPIO0` down to `GND` (via your switch or jumper) and trigger a **hardware reset** (either by cycling the system power or simply pressing and releasing your dedicated **Reset button**) to force the chip into **UART Bootloader Mode**.
4. Compile the sketch inside Arduino IDE and upload the firmware.

<a id="ota-updates"></a>

### Subsequent Wireless Updates (OTA)
Once the initial firmware is successfully flashed via a wired connection (using either [Method 1](#method-1-binary) or [Method 2](#method-2-source)), you can disconnect the ESP8266 from your PC. At this stage, the module's UART pins must be permanently rewired to the chosen serial interface on your 3D printer motherboard.

 Before permanently rewiring the pins, it is highly recommended to first switch the module back into normal operation mode right on your desk to verify that the fresh firmware is fully functional. Upon a successful clean boot, the server should automatically launch an emergency access point *"Marlin-Bridge-Setup"*; you can connect your device to it and navigate to `http://192.168.9.1/config` to ensure the local configuration web server responsive and stable.

From this point forward, the bridge natively supports Over-the-Air (OTA) wireless firmware updates during standard operation, eliminating the need to ever reconnect the module back to a computer. To switch the bridge into wireless upgrade mode, please refer to the [specific guidelines](#ota-command) outlined in the general **[💻 Terminal Commands Overview](#terminal-commands)** section of this document.

<p align="right"><a href="#table-of-contents">▲ Back to Top</a></p>

---

<a id="assets-optimization"></a>

### 🎛️ Web Assets Optimization & Automating OTA Builds (Python Utilities)

To ensure extreme runtime efficiency, zero memory fragmentation, and a compact binary footprint, the server-side web interface pages (`INDEX_HTML` and `CONFIG_HTML`) are aggressively minified, compressed using the Gzip algorithm, and embedded into the C++ source files as byte arrays stored strictly in `PROGMEM`. 

#### 1. The Frontend Compression Engine (`zipper.py`)
Located in your repository under 📁 `addendum/software/`. This script executes raw HTML/JS code squeezing, stripping comments, redundant spaces, and line breaks before packaging the buffer into a production-ready Gzip header using `EXTREME` mode.

* **Pipeline Execution Condition:** The repository already contains pre-compiled, production-ready version-aligned variants of `js_frontend_gzip.h` and `cfg_page_gzip.h`. Therefore, running the Python scripts is **fully optional** and strictly required only if you have manually modified the source code within `js_frontend.h` or `cfg_page.h`. You only need to process the specific file that was altered.

* **Pipeline Execution Sequence:** If you made changes to the source web templates *(located under 📁 `Marlin_WiFi_Bridge/`)*, process the modified file inside your terminal using the Python builder engine:
  ```
  # Run ONLY for the file that has been modified:
  python3 zipper.py js_frontend.h --hard
  python3 zipper.py cfg_page.h --hard
  ```

* **Asset Allocation & Overwriting:** Once execution completes, locate the newly generated production file — **`js_frontend_gzip.h`** or **`cfg_page_gzip.h`** — and move it directly into your core project folder alongside the rest of the Arduino firmware files, **overwriting the existing compressed asset**.

* ➔ **Mandatory Suffix Variable Patch:** Before compilation in the Arduino IDE, switch to the tab containing the newly generated `cfg_page_gzip.h` file and perform a global find-and-replace operation. You **must change the default declared variable substring from `INDEX` to `CONFIG`** (modifying `INDEX_HTML` to `CONFIG_HTML` throughout the entire file). This ensures proper structural separation between the main client interface and the isolated configuration landing page inside the ESP8266 routing engine.

#### 2. The Verification Parser (`js_pretty_s.py`)
During the execution of `zipper.py`, the engine automatically outputs a temporary uncompressed intermediate debug file named `debug_minified.html`. To safeguard your code against breaking syntax rules caused by aggressive minification, utilize the structural validation utility:
```bash
python3 js_pretty_s.py debug_minified.html
```
The parser will reconstruct it into two formatted, human-readable alignment files: `r_pretty.html` and `l_pretty.html`, allowing you to visually audit the resulting HTML DOM nodes tree and JavaScript code blocks block-by-block before hitting compile.

<a id="bin-generation"></a>

#### 3. Automatic OTA Bin Generation Script (`sizes.py`)
Wireless updates (OTA) strictly require a raw compiled `.bin` firmware file. Since the standard Arduino IDE interface does not provide a streamlined, automated way to export binary files directly upon hitting the "Verify/Compile" button, the native compilation layout script has been modified to bridge this gap.

The repository includes a customized **`sizes.py`** utility script equipped with an internal automated post-compile trigger block. Upon every successful compilation, it instantly extracts, re-maps, and saves the binary package into a dedicated output folder `c:\Users\Public\Documents\Arduino\OTA_BIN\file4ota.bin` (printing `[AUTO-OTA] .bin saved to OTA_BIN!` in your IDE status log console).

* **Installation Sub-Routine:** 
  1. Locate the modified script in your repository under:
     📁 `addendum/software/Arduino/sizes.py`
  2. Navigate to your local system Arduino core tools directory (approximate path depending on your Windows user profile name and target ESP8266 core version, tested on **core 3.1.2**):
     `C:\Users\<user_name>\AppData\Local\Arduino15\packages\esp8266\hardware\esp8266\3.1.2\tools\`
  3. Backup your original native compiler script, then copy and overwrite it with the modified `sizes.py` from this repository.

<p align="right"><a href="#table-of-contents">▲ Back to Top</a></p>

---

<a id="marlin-requirements"></a>

## ⚙️ Marlin Firmware Configuration Requirements

To unlock the full potential of the bridge (especially high-speed **BFT binary transfers** and **Home Assistant telemetry**), you will most likely need to recompile your printer's Marlin firmware, e.g. using _vsCode_. The bridge relies heavily on specific Marlin capabilities and supports actual version 2.1.x.x. 

Ensure the following configuration options are enabled in your Marlin source files before flashing your printer:

### 1. In `Configuration.h`
* **Baud Rate Configuration:** The serial port assigned to the ESP8266 must match the bridge speed. A speed of **500,000 baud** is required to be set.
  ```cpp
  #define SERIAL_PORT_2 0   // Set according to your actual connected port index
  #define BAUDRATE_2 500000 // Match the bridge speed
  ```

### 2. In `Configuration_adv.h`

* **Basic SD and LFN support :** 
  ```cpp
  #define SDSUPPORT
  //#define SDCARD_SORT_ALPHA         // Don't need it
  //#define UTF_FILENAME_SUPPORT      // Will use only basic EN_US charset
  #define LONG_FILENAME_HOST_SUPPORT
  #define LONG_FILENAME_WRITE_SUPPORT
  //#define M20_TIMESTAMP_SUPPORT
  #define SCROLL_LONG_FILENAMES
  #define AUTO_REPORT_SD_STATUS
  #define SDCARD_CONNECTION ONBOARD
  ```
* **High-Speed Buffers :** To prevent data dropouts and maximize SPI/SD transfer throughput, allocate sufficient host receive buffers (minimum 1024 bytes):
  ```cpp
  #define RX_BUFFER_SIZE 1024
  #define TX_BUFFER_SIZE 128
  #define BLOCK_BUFFER_SIZE 32 // Optimal for SDSUPPORT
  ```
* **Binary File Transfer (BFT) :** Essential for secure, rapid file uploading with Fletcher-16 checksums. Ensure your `M115` report output responds with `Cap: BINARY_FILE_TRANSFER:1`.
  ```cpp
  #define BINARY_FILE_TRANSFER
  #define MAX_CMD_SIZE 256
  #define BUFSIZE 16
  ```
* **Emergency Command Parser :** Crucial for the bridge's Home Assistant emergency stop button (`M112`/`M410`) to intercept commands instantly even if the motion queue is full.
  ```cpp
  #define EMERGENCY_PARSER
  ```
* **Realtime Reporting :** Required for automatic, Grbl-style continuous positioning and status updates pushed to the host, which feeds the Home Assistant coordinates telemetry.
  ```cpp
  #define REALTIME_REPORTING_COMMANDS
  #define FULL_REPORT_TO_HOST_FEATURE 
  ```
* **Advanced OK :** Enhances transaction pacing and progress tracking.
  ```cpp
  #define ADVANCED_OK
  ```

* ➔ **Reference Log:** A complete example of Marlin's response to the `M115` command can be found in the [📁 `addendum/m115_log.md`](addendum/m115_log.md) file. All critical capabilities and parameters required for the stable operation of the WiFi-UART bridge are explicitly marked with an asterisk (`*`).

<p align="right"><a href="#table-of-contents">▲ Back to Top</a></p>

---

<a id="ha-integration"></a>

## 🤖 Home Assistant Integration & MQTT Discovery

The bridge includes an autonomous **MQTT Discovery** script that registers the printer as a fully featured IoT device within Home Assistant.

### Non-Blocking Discovery Logic

To avoid crashing or overwhelming the MQTT broker during initialization, a registration matrix cycles through states (`DISCOVERY_EXPIRED` ➔ `DISCOVERY_CONNECTED` ➔ `DISCOVERY_ANNOUNCED`) modifying **only a single entity configuration per main loop iteration**, and then skipping multiple loop iterations between steps. Previous entity definitions are systematically wiped prior to an announcement to keep the HA registry pristine.

### Exposed Entities & Attributes

All Home Assistant entity and topic names are generated dynamically by appending a specific alphabetical suffix to the **Device Identifier** specified during [initial user configuration](#server-config) (e.g., if the identifier is `IoT_7`, full entity names will automatically be generated in lowercase as sensor.iot_7_th, text.iot_7_g, etc.).

| Entity Suffix | Type | Description | Attributes |
| - | - | - | - |
| `_TH` | Sensor | Extruder temperature (°C, 0.1 precision) | `Target_t` |
| `_TB` | Sensor | Heated bed temperature (°C, 0.1 precision) | `Target_t` |
| `_X`, `_Y`, `_Z` | Sensor | Absolute position axes (mm, 0.01 precision) | `Steps_equivalent` (Microsteps) |
| `_E` | Sensor | Extruder position (mm, 0.01 precision) | `SXYZ` (Marlin's debug `S_XYZ` motor tracking output) |
| `_P` | Sensor | Upload / printing process completion state (0–100%) | `File_name` |
| `_U` | Number | Active session locking token (blocks concurrent command sources for 10s) | `RSSI `(dBm), `Uptime` (HH:MM:SS) |
| `_G` | Text | Global bridge state: `IDLE`, `UPLOAD`, `READY`, `PRINT`, etc. | `dimensions` (JSON build volume metrics parsed via `M115`) |

<a id="mqtt-topics"></a>

### Command Topics & Subscriptions

All current telemetry values, entity attributes, and state parameters are automatically aggregated and published into a single global state topic:

 `marlin_bridge/<device_id>/state`

Dedicated command execution pipelines are dynamically opened for interactive entities. These command topics follow a strict syntax layout: 

 `marlin_bridge/<device_id>_<suffix>/cmd`

*(For example, if your configured device identifier is `IoT_7`, the state updates feed into `marlin_bridge/IoT_7/state`, while instructions are received at `marlin_bridge/IoT_7_U/cmd` and `marlin_bridge/IoT_7_G/cmd`).*

- **Emergency Control (`..._U/cmd`) :** Receives single-character integers to trigger system interrupts:

  - `0`: Force reboot ESP8266.

  - `1`: Force restart MQTT Discovery loop.

  - `2`: Emergency Stop (Instantly kills all motion and heaters via UART).

  - `3`: Query `M115` to re-fetch print bed geometry bounds.

  - `4`: Force ESP8266 into Access Point configuration mode.

- **G-Code Stream Pipeline (`..._G/cmd`) :** Accepts multi-line G-code segments separated by `'\n'` symbol. For safety validation, incoming payloads must end with a precise validation suffix.

  -- G-Code Suffix Structure.
  Example syntax : `;mid=1234567890:HAG0\n` *(Note: All numeric values in this example are placeholders for actual operational variables).*

  -- The suffix components are decoded as follows:

  - `;` - The standard G-code comment delimiter placed immediately after the last instruction.

  - `mid=1234567890` - **Message Source Identifier**. An unique, random, and persistent numeric token assigned to each specific control interface instance to prevent concurrent data conflicts or command mixing.

  - `:` - Internal payload block separator.

  - `HAG` - **Command Type Key**. Specifies the instruction category from the available set:

    - `HAM` (Movement) / `HAT` (Temperature): Triggers a 25-second or 35-second software watchdog timer respectively, forcing the bridge to await Marlin's explicit `OK` before polling new axis coordinates or temperatures.

    - `HAG` (Generic): Processes standard G-codes immediately without additional polling routines.

  - `0` - **Acknowledge Skip Counter**. Defines the precise number of Marlin `OK` responses the bridge must safely ignore before the current transaction is marked as successfully completed.

<a id="ha-custom-card"></a>

### 🎛️ Custom Home Assistant Frontend Card (JS Script)
To provide seamless remote control directly from the Home Assistant Lovelace dashboard, a specialized custom frontend card based on native JavaScript has been developed. 

It implements several intelligent UX and safety features:
* **Smart Axis Joystick :** Features an interactive virtual joystick with an **incremental distance accumulation mechanism** for precise step-by-step axis movements ($X$, $Y$, $Z$).
* **Configurable Temperature Presets :** Provides physical buttons for rapid heater management. The total number of buttons, target heating values, and target tools (Nozzle/Bed) are fully customizable.
* **Configurable G-Code Macros :** Includes a modular button layout to execute custom G-code command sequences with flexible parameters.
* **Interactive Soft Endstops UI :** Integrates a responsive software-level endstop toggler with an explicit interactive UI checkbox to safeguard the printer from physical axis over-travel.
* **Auto-Sizing Bed Bounds:** The card automatically captures and defines the visual printable bed boundaries by listening to the bridge's parsed JSON metadata extracted from Marlin's `M115` response.
* **Anti-Race Condition Lock :** Multiple browser tabs or different users opening the card simultaneously can cause command overlaps. To prevent this, the card automatically generates a unique runtime instance identifier. Upon sending any command, it locks the control channel to this specific ID for **10 seconds**, safely ignoring commands originating from any other source dashboard or card instance during this window.

<a id="ha-advanced-deployment"></a>

### 🛠️ Advanced Home Assistant Deployment & Packages
Integrating custom frontend cards and managing raw MQTT attributes inside Home Assistant can quickly become complex. To lower the barrier to entry, all advanced configurations, dashboards deployment guides, and helper files are isolated in a dedicated subdirectory of this repository.

#### Pre-Configured YAML Packages
Instead of forcing you to manually write filters and templates, this project provides a ready-to-use **Home Assistant Package** file (`packages/iot_7.yaml`). By simply dropping this file into your Home Assistant `packages/` directory and registering it inside your `configuration.yaml` file, the system will automatically generate highly optimized virtual helper entities, including:
* **Combined Temperature Telemetry:** Formats current and target temperatures into clean, human-readable strings (e.g., `215°C / 215°C`) for streamlined dashboard monitoring.
* **Intelligent Wi-Fi RSSI Diagnostics:** Strips raw string data and converts telemetry into a standardized `signal_strength` device class sensor (`dBm`). Home Assistant automatically uses this to dynamic-render responsive signal bars on your dashboard icons based on real-time signal degradation zones (from strong `-50 dBm` down to critical `-90 dBm` thresholds).

*For a step-by-step frontend installation walkthrough and complete YAML package deployments, please check the dedicated guide inside the `/homeassistant/` folder.*

<p align="right"><a href="#table-of-contents">▲ Back to Top</a></p>

---

<a id="web-ui"></a>

## 🌐 Web Client User Interface (UI)
The ultra-optimized Web-UI is specifically tailored for both desktop and mobile browsers, utilizing a responsive vertical layout where interface elements are stacked sequentially from top to bottom. 

The active components dynamically morph, shift colors, or animate (e.g., pulsing states) based on the server's 1-second `HeartBeat` synchronization stream:

1. **Connection Status Indicator (Top Bar):** 
   * Displays a static red `OFFLINE` banner if the socket disconnects.
   * Displays a green, pulsing `ONLINE` banner when actively synchronized with the server's HeartBeat.
2. **Transfer Mode Radio Buttons:** A dual-toggle selector interface with explicit labeled options for `ASCII` and `BFT` transport pipelines to configure the upcoming file upload protocol.
3. **Primary Action Button (Upper Large Block):** A wide, color-shifting multi-functional block. 
   * In the default state, it reads **"Select file"** and fires up the native OS file picker.
   * Once a local file is chosen, it morphs into an interactive **"Upload <FileName>"** button.
   * After a successful upload, it transitions into a **"Print <FileName>"** validation switch to launch the Marlin execution buffer.
   * Depending on specific system situations, it also serves as a critical information display field, locking the interface and outputting system alert statuses such as **"Error"** or **"Wait"**.
4. **Secondary Control & Progress Tracker (Lower Large Block):** A wide secondary button that serves as both a control key and a graphic rendering bar.
   * Acts as a global **"Cancel"** or verification button depending on the system context.
   * During upload or print procedures, it embeds a real-time responsive visual progress slider running from `0%` to `100%`.
5. **Interactive Diagnostic Log (Main Viewport):** A clean text container caching up to 200 chronological server/client message entries with an auto-scroll buffer. 
   * Features strict color-coding for incoming event classification (Blue/Green/Yellow/Red/Grey logs).
   * Double-clicking anywhere inside this viewport downloads the current session trace file directly to your local drive (if [*logging features*](#debug-cmd) are enabled).
6. **Command Entry Prompt & Submit (Bottom Input Strip):** A raw text input bar paired with a physical **"Send"** submit button on the right. This field accepts both control G-codes and short parameterized console management instructions decoded in the summary below. The `<Enter>` key acts exactly like the physical **"Send"** button for rapid desktop execution.

<p align="right"><a href="#table-of-contents">▲ Back to Top</a></p>

---

<a id="terminal-commands"></a>

## 💻 Terminal Commands Overview

As mentioned above, the UI features a raw control prompt input strip. Simple commands can be single letters (case-insensitive), but the general form contains the command in parentheses, optionally followed by a numeric or symbolic parameter. Destructive and system control commands require re-entry to ensure safety.

### Operational Commands

- `(H)` : Print general usage help.

- `(?)` : Print SD card management utilities help.

- `(V)` : Print the current and maximum allowed firmware file size, program version, and compilation date.

- `(I)` : Print active client connection status profiles.

- `(P)` : Dual-purpose. Outputs the last boot crash/reset flags at startup. Later morphs into real-time operational statistic reports (RAM usage, data latency metrics, Marlin answers parsing speed, buffers usage metrics).

  - `(P)0` / `(P)1` : Switch between brief and comprehensive upload telemetry output. This toggle also enables or disables the verbose mode for notification messages from Marlin during file upload and printing.

- `(X)` : Terminate connections and launch local AP config server (keeps existing settings in EEPROM).

  - `(X)1` : **(Destructive)** Clear EEPROM data and launch AP config server (factory reset).

  <a id="ota-command"></a>

- `(O)` : Transition bridge into OTA firmware update state.

  - *Note:* Before entering this command, you must select your compiled [*binary firmware file*](#bin-generation) via the client's UI exactly like you would [choose a G-code file](#selection-mechanics) for printer upload.

- `(R)` : Gracefully reboot the server.

<a id="file-command"></a>

### SD Card File/Directory Commands

- `(N)` : Target file entry number `N` from the current listing partition for immediate printing.

- `(N).` : Change working directory to the folder containing file entry `N`.

- `(N)-!` : Delete file `N` on the SD card.

- `(N)-*` : Delete all files on the SD card sharing the same long filename as `N`.

- `(N)-.` : Delete every single file contained within the same directory path as `N`.

- `0` : Force re-scan the printer SD card and reset index pointer back to the list head.

- `Number` : Input an isolated integer to change the maximum page size for pagination.

- `(+)` / `(-)` : Advance to the next listing page / fall back to the previous listing page.

<a id="debug-cmd"></a>

### Low-Level Debug Controllers

Use without a parameter to get current state of controls. Append `0` as a parameter to disable, or a positive integer flag to enable background streaming into the browser log window.

- `(C)` : Toggle recording of all transaction communications into browser cache *(Exportable to local disk via double-clicking log viewport)*.

- `(S)` : Toggle general system service logs.

- `(T)` : Toggle file chunk transfer status and SD write logs.

- `(M)` : Toggle detailed MQTT transactions. Accepts bitmasks 0–15 (`SUB : bit_3`, `SRAW : bit_2`, `PUB : bit_1`, `PRAW : bit_0`).

- `(U)` : Toggle granular raw Marlin UART tracing. Accepts bitmasks 0–15 ( `TX lines : bit_3`, `RX lines : bit_2`, `CNT line metrics : bit_1`, `ACK responses : bit_0`).

- `(*)` : Instant macro to toggle all diagnostic engines simultaneously.

- **BFT Protocol Diagnostics :** Manually dispatch raw Marlin token controls in case of unexpected transfer interrupts : `(SYNC)`, `(EXIT)`, `(QUERY)`, `(OPEN)`, `(CLOSE)`, `(ABORT)`.

<p align="right"><a href="#table-of-contents">▲ Back to Top</a></p>

---

<a id="initial-config"></a>

## ⚙️ Initial Configuration

### 🌐 Wi-Fi Network & Router Optimization (Signal Stability Warning)
During long-term stress testing, the legacy ESP8266 hardware core exhibited hidden connection dropouts, lost WebSocket packets, and MQTT disconnections when interacting with modern high-tier Wi-Fi routers. This is not a software bug, but a hardware limitation of the ESP8266 chip itself - it is completely incompatible with advanced, modern wireless optimizations.

To achieve 100% data stream stability and zero transfer dropouts under heavy 500k baud UART loads, ensure your local 2.4 GHz Wi-Fi network conforms to the following strict router settings:
* **Channel Width:** Force to **`20 MHz` strictly**. Do not use `20/40 MHz` auto-toggling, as channel bonding causes massive packet loss on the ESP core.
* **Disable Modern Enhancements:** In your router's advanced 2.4 GHz parameters, **explicitly turn OFF** the following features:
  * `TX Burst` / `Frame Burst`
  * `256-QAM` (TurboQAM)
  * `Airtime Fairness` (This option will aggressively drop the ESP bridge from the airwaves to prioritize faster modern clients like smartphones).
* **Network Isolation (Best Practice for Enthusiasts):** To isolate the high-speed printer bridge from heavy household traffic (streaming, gaming), it is highly recommended to isolate the ESP8266 within a **dedicated IoT network segment** (Guest Wi-Fi network or a separate VLAN), pinned tightly to a fixed wireless channel (e.g., Channel 1, 6, or 11).

* 📌 **DHCP Static IP Recommendation:** Upon startup, the bridge automatically receives its temporary IP address via DHCP and displays it directly on your 3D printer's physical LCD screen, so you can easily type it into your browser. However, since Home Assistant configuration and web iFrame panels rely on a fixed URL link, it is highly recommended to open your Wi-Fi router settings and assign a **Static DHCP Lease (IP Reservation)** to the ESP8266. Without a fixed IP, every time your router reboots, you will have to manually look at the printer's screen to find the new address and re-type it into your browser or update your Home Assistant dashboard configuration.

<a id="server-config"></a>

### 🚀 Step-by-Step Initial Setup Sub-Routine
If the system detects a corrupted EEPROM checksum on startup, drops its WiFi connection for more than 1 minute, or receives an external terminal/MQTT interruption request, it automatically fires up an emergency Access Point:

1. **Connect to the AP:** Connect your computer or smartphone to the local open Wi-Fi network named **"Marlin-Bridge-Setup"** hosted by the ESP8266.
2. **Open the Interface:** Direct your preferred web browser to the configuration gateway: **`http://192.168.9.1/config`**
3. **Submit Credentials:** Input your home network SSID/Password (<u>required</u>), MQTT broker location, Home Assistant discovery prefix, and your desired device identifier token.
4. **Connection Timeout Rules:** The setup environment triggers a safe automatic restart if no configuration client connects within the first 2 minutes. Once a client successfully opens the configuration webpage, this operational window automatically scales up to 15 minutes.
5. **Validation & Errors Handling:** The setup process is entirely user-friendly. Upon clicking save, if any required validation conditions are not met, explicit error messages are displayed on the screen, and the configuration session time is extended by an additional 15 minutes to allow for safe correction.

<p align="right"><a href="#table-of-contents">▲ Back to Top</a></p>

---

<a id="license-and-aknowledgments"></a>

## License & Acknowledgments

Copyright (c) 2026 warez4me. All rights reserved.

This project is open-source software licensed under the **GNU General Public License v3.0 (GPLv3)**. You are free to modify and distribute it, provided that any derivative works also remain open-source under the same license.

### Third-Party Libraries
* **ESP8266 Arduino Core & Community Libraries** — This firmware relies on the official ESP8266 core, WebSockets, and various other open-source libraries.

### AI Assistance
* Parts of the code architecture and optimization ideas were developed with the assistance of Generative AI tools.

<p align="right"><a href="#table-of-contents">▲ Back to Top</a></p>
