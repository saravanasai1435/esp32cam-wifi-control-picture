#include "esp_camera.h"
#include <WiFi.h>
#include <FS.h>
#include <SD_MMC.h>
#include <WebServer.h>

#define FLASH_LED 4
int photoNumber = 1;

const char* ssid = "APFIBER_0978";
const char* password = "0000009055";

// Web server on port 80
WebServer server(80);

// Camera config for AI Thinker
camera_config_t config;

void capturePhotoSD();

// ---------- MJPEG streaming handler ----------
void handleJPGStream(void){
  WiFiClient client = server.client();
  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  server.sendContent(response);

  while(1){
    camera_fb_t * fb = esp_camera_fb_get();
    if(!fb) continue;

    client.printf("--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", fb->len);
    client.write(fb->buf, fb->len);
    client.write("\r\n");
    esp_camera_fb_return(fb);

    if(!client.connected()) break;
  }
}

// ---------- Web page ----------
void handleRoot(){
  String html = "<html><body>";
  html += "<h1>ESP32-CAM Live Stream</h1>";
  html += "<img src=\"/stream\" width=640><br>";
  html += "<a href=\"/capture\">Take Photo</a>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

// ---------- Take photo ----------
void handleCapture(){
  digitalWrite(FLASH_LED, HIGH);
  delay(150);
  capturePhotoSD();
  digitalWrite(FLASH_LED, LOW);
  server.send(200, "text/html", "<html><body>Photo captured!<br><a href='/'>Back</a></body></html>");
}

// ---------- Capture photo to SD ----------
void capturePhotoSD(){
  camera_fb_t * fb = esp_camera_fb_get();
  if(!fb){
    Serial.println("❌ Capture failed");
    return;
  }

  String path = "/" + String(photoNumber) + ".jpg";
  File file = SD_MMC.open(path, FILE_WRITE);
  if(file){
    file.write(fb->buf, fb->len);
    Serial.printf("📸 Saved: %s (%d bytes)\n", path.c_str(), fb->len);
    file.close();
    photoNumber++;
  } else {
    Serial.println("❌ Save failed!");
  }
  esp_camera_fb_return(fb);
}

// ---------- Setup ----------
void setup(){
  Serial.begin(115200);
  pinMode(FLASH_LED, OUTPUT);
  digitalWrite(FLASH_LED, LOW);

  // Camera config
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = 5;
  config.pin_d1 = 18;
  config.pin_d2 = 19;
  config.pin_d3 = 21;
  config.pin_d4 = 36;
  config.pin_d5 = 39;
  config.pin_d6 = 34;
  config.pin_d7 = 35;
  config.pin_xclk = 0;
  config.pin_pclk = 22;
  config.pin_vsync = 25;
  config.pin_href = 23;
  config.pin_sscb_sda = 26;
  config.pin_sscb_scl = 27;
  config.pin_pwdn = 32;
  config.pin_reset = -1;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if(psramFound()){
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  if(esp_camera_init(&config) != ESP_OK){
    Serial.println("❌ Camera init failed!");
    return;
  }

  if(!SD_MMC.begin()){
    Serial.println("❌ SD Card Mount Failed!");
    return;
  }

  // Find last photo number
  File root = SD_MMC.open("/");
  while(true){
    File file = root.openNextFile();
    if(!file) break;
    String name = file.name();
    name.replace("/", "");
    name.replace(".jpg", "");
    int n = name.toInt();
    if(n >= photoNumber) photoNumber = n + 1;
    file.close();
  }
  root.close();

  // Connect Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while(WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("✅ Wi-Fi connected. IP: " + WiFi.localIP().toString());

  // Web server routes
  server.on("/", handleRoot);
  server.on("/stream", handleJPGStream);
  server.on("/capture", handleCapture);

  server.begin();
  Serial.println("Web server started. Open browser at this IP to view live stream.");
}

// ---------- Loop ----------
void loop(){
  server.handleClient();
}
