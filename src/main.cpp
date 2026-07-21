#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <TFT_eSPI.h>

// 🔑 Wi-Fi บ้านของคุณ
const char* ssid = "u wifi name";
const char* password = "u pass";

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
TFT_eSPI tft = TFT_eSPI();

// Buffer ขนาด 40,960 Bytes สำหรับรวมก้อนข้อมูลภาพ (128x160x2 bytes)
uint8_t frameBuffer[128 * 160 * 2];
size_t frameBufferIdx = 0;

// หน้าเว็บ HTML + JavaScript แปลงภาพ และส่งข้อมูลแบบ Chunk-Safe
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>ESP32-S3 Image Uploader</title>
    <style>
        body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; text-align: center; background: #121212; color: #fff; margin: 0; padding: 20px; }
        .card { background: #1e1e1e; padding: 25px; border-radius: 16px; margin: 20px auto; max-width: 380px; box-shadow: 0 4px 20px rgba(0,0,0,0.5); }
        h2 { color: #00E676; margin-bottom: 20px; }
        input[type=file] { display: none; }
        .btn { background: #2979FF; color: white; padding: 12px 24px; border: none; border-radius: 8px; cursor: pointer; font-size: 16px; font-weight: bold; transition: 0.2s; display: inline-block; margin: 10px 0; }
        .btn:hover { background: #2962FF; }
        #status { margin-top: 15px; color: #888; font-size: 14px; }
        #preview { margin-top: 15px; max-width: 128px; border: 2px solid #333; border-radius: 4px; display: none; }
        canvas { display: none; }
    </style>
</head>
<body>
    <div class="card">
        <h2>📷 ESP32-S3 Display</h2>
        <label for="imageInput" class="btn">เลือกรูปภาพ</label>
        <input type="file" id="imageInput" accept="image/*">
        <br>
        <img id="preview" alt="Preview">
        <div id="status">Status: Connecting WebSocket...</div>
    </div>

    <canvas id="canvas" width="128" height="160"></canvas>

    <script>
        const ws = new WebSocket(`ws://${location.host}/ws`);
        const imageInput = document.getElementById('imageInput');
        const preview = document.getElementById('preview');
        const status = document.getElementById('status');
        const canvas = document.getElementById('canvas');
        const ctx = canvas.getContext('2d');

        ws.onopen = () => { status.innerText = 'Status: Connected! Ready to upload.'; status.style.color = '#00E676'; };
        ws.onclose = () => { status.innerText = 'Status: Disconnected!'; status.style.color = '#FF5252'; };

        imageInput.addEventListener('change', (e) => {
            const file = e.target.files[0];
            if (!file) return;

            const img = new Image();
            img.onload = () => {
                // แสดงภาพ Preview บนหน้าเว็บ
                preview.src = img.src;
                preview.style.display = 'inline-block';

                // ถ่ายลง Canvas 128x160 แบบ Keep Aspect Ratio
                ctx.fillStyle = "#000000";
                ctx.fillRect(0, 0, 128, 160);
                
                let scale = Math.min(128 / img.width, 160 / img.height);
                let w = img.width * scale;
                let h = img.height * scale;
                let x = (128 - w) / 2;
                let y = (160 - h) / 2;
                
                ctx.drawImage(img, x, y, w, h);

                // ดึงพิกเซล RGBA
                const imgData = ctx.getImageData(0, 0, 128, 160).data;
                const buffer = new Uint8Array(128 * 160 * 2);
                let idx = 0;

                for (let i = 0; i < imgData.length; i += 4) {
                    const r = imgData[i];
                    const g = imgData[i + 1];
                    const b = imgData[i + 2];

                    // ✅ แก้เป็น BGR Format (สลับ B มาหน้า เอา R ไปหลัง)
                    const rgb565 = ((b & 0xF8) << 8) | ((g & 0xFC) << 3) | (r >> 3);
                    
                    // สลับ LSB / MSB ให้ตรงกับ TFT_eSPI
                    buffer[idx++] = rgb565 & 0xFF;        // LSB
                    buffer[idx++] = (rgb565 >> 8) & 0xFF; // MSB
                }

                // ยิง Binary Buffer ข้าม WebSocket
                if (ws.readyState === WebSocket.OPEN) {
                    ws.send(buffer);
                    status.innerText = 'Status: Image sent successfully!';
                    status.style.color = '#00E676';
                } else {
                    status.innerText = 'Status: WebSocket disconnected!';
                    status.style.color = '#FF5252';
                }
            };
            img.src = URL.createObjectURL(file);
        });
    </script>
</body>
</html>
)rawliteral";

// ฟังก์ชันรับก้อน WebSocket และสะสมข้อมูลจนครบเฟรมก่อนสั่งพล็อตลงจอ
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_DATA) {
        AwsFrameInfo *info = (AwsFrameInfo*)arg;
        
        if (info->opcode == WS_BINARY) {
            // ถ้าเป็นจุดเริ่มต้นของเฟรมใหม่ ให้รีเซ็ตตำแหน่ง Buffer
            if (info->index == 0) {
                frameBufferIdx = 0;
            }

            // ทยอยก๊อปปี้ข้อมูล Chunk ที่ส่งเข้ามาใส่ Buffer
            if (frameBufferIdx + len <= sizeof(frameBuffer)) {
                memcpy(frameBuffer + frameBufferIdx, data, len);
                frameBufferIdx += len;
            }

            // เมื่อรับข้อมูลสมบูรณ์ครบทั้งเฟรม (40,960 bytes) ค่อยยิงขึ้นจอทีเดียว
            if (frameBufferIdx >= info->len) {
                tft.pushImage(0, 0, 128, 160, (uint16_t*)frameBuffer);
                frameBufferIdx = 0;
            }
        }
    }
}

void setup() {
    Serial.begin(115200);

    // Init TFT Display
    tft.init();
    tft.setRotation(0); // แนวตั้ง (128x160)
    tft.fillScreen(TFT_BLACK);
    tft.setSwapBytes(true);
    
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString("Connecting WiFi...", 5, 10, 2);
    tft.drawString(ssid, 5, 30, 2);

    // เชื่อมต่อ Wi-Fi บ้าน
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    int timeout = 0;
    while (WiFi.status() != WL_CONNECTED && timeout < 30) {
        delay(500);
        Serial.print(".");
        timeout++;
    }

    tft.fillScreen(TFT_BLACK);
    if (WiFi.status() == WL_CONNECTED) {
        IPAddress IP = WiFi.localIP();
        tft.drawString("WiFi Connected!", 5, 10, 2);
        tft.drawString("Open URL in Browser:", 5, 35, 2);
        
        // แสดง IP บนหน้าจอ ST7735
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.drawString("http://" + IP.toString(), 5, 60, 2);
        
        Serial.print("Web Server IP: ");
        Serial.println(IP);
    } else {
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawString("Connect Failed!", 5, 10, 2);
    }

    // ตั้งค่า WebSocket & WebServer
    ws.onEvent(onEvent);
    server.addHandler(&ws);
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", index_html);
    });

    server.begin();
}

void loop() {
    ws.cleanupClients();
}