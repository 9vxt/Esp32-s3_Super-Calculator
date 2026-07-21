# Esp32-s3_Super-Calculator

A hardware-software integrated system built on **ESP32-S3** and an **ST7735 1.8" TFT Display (128x160)**. The project features a real-time web interface powered by **WebSockets** and an **Async Web Server**, supporting ultra-low latency interaction and dynamic display updates.

---

## 🛠️ Tech Stack & Architecture

* **Microcontroller:** ESP32-S3 (PlatformIO / Arduino Framework)
* **Display:** 1.8" ST7735 SPI TFT Display ($128 \times 160$ Resolution)
* **Networking:** ESPAsyncWebServer + AsyncWebSocket (Station Mode / Local Wi-Fi)
* **Frontend:** HTML5, CSS3, JavaScript (Canvas API, Bitwise Operations)

---

## 📌 Hardware Pinout

| Signal | ESP32-S3 GPIO | Description |
| :--- | :--- | :--- |
| **MOSI** | GPIO 11 | SPI Data Out |
| **SCLK** | GPIO 12 | SPI Clock |
| **CS** | GPIO 10 | Chip Select |
| **DC** | GPIO 8 | Data / Command Control |
| **RST** | GPIO 9 | Hardware Reset |
| **VCC** | 3.3V | Main Power Supply |
| **GND** | GND | Common Ground |

---

## 🧪 System Validation & Stress Testing (Image Streaming Phase)

Before implementing the core calculator logic, the WebSocket communication pipeline and SPI rendering throughput were stress-tested by streaming raw pixel binary data from a web-based Canvas directly to the ST7735 display.

### 💡 Engineering Challenges & Low-Level Solutions

1. **Display Alignment & Offset Debugging:**
   * **Issue:** Panel boundary misalignment (snow/garbage pixels on frame edges).
   * **Solution:** Identified panel specs and set driver configurations to `ST7735_REDTAB` via PlatformIO build flags.

2. **WebSocket Chunk Fragmentation Handling:**
   * **Issue:** High-throughput binary frames ($128 \times 160 \times 2 = 40,960 \text{ bytes/frame}$) caused frame tearing due to TCP packet fragmentation.
   * **Solution:** Implemented a contiguous $40.9 \text{ KB}$ frame buffer on ESP32-S3 to accumulate incoming binary chunks before issuing a single SPI flush via `tft.pushImage()`.

3. **Client-Side Color Space Offloading (RGB888 to BGR565):**
   * **Issue:** Color channel swapping (Red appearing as Blue) and high CPU overhead if converted on the microcontroller.
   * **Solution:** Offloaded bit-packing and color space conversion to the client-side JavaScript engine. The JS code converts 32-bit RGBA pixels into 16-bit BGR565 format using bitwise operations:
     $$\text{RGB565} = ((B \ \& \ \text{0xF8}) \ll 8) \mid ((G \ \& \ \text{0xFC}) \ll 3) \mid (R \gg 3)$$

---

## 🚀 Key Takeaways

By successfully achieving continuous $40.9 \text{ KB/frame}$ real-time image rendering over WebSockets, the data pipeline proved to be more than capable of handling real-time calculator state synchronization with near-zero latency.