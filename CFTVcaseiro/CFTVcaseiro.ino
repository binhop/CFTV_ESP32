#include "esp_camera.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <HTTPUpdateServer.h>

// Selecione o modelo da câmera
//#define CAMERA_MODEL_WROVER_KIT // Has PSRAM
//#define CAMERA_MODEL_ESP_EYE // Has PSRAM
//#define CAMERA_MODEL_M5STACK_PSRAM // Has PSRAM
//#define CAMERA_MODEL_M5STACK_V2_PSRAM // M5Camera version B Has PSRAM
//#define CAMERA_MODEL_M5STACK_WIDE // Has PSRAM
//#define CAMERA_MODEL_M5STACK_ESP32CAM // No PSRAM
#define CAMERA_MODEL_AI_THINKER // Has PSRAM
//#define CAMERA_MODEL_TTGO_T_JOURNAL // No PSRAM
#include "camera_pins.h" // Precisa incluir depois do define acima


// ----- Definições -----
#define NET_ID      "NOME_REDE_WIFI"
#define NET_PASS    "SENHA_REDE_WIFI"

#define HOST        "camera"


// ----- Variáveis globais -----
WebServer httpServer(80);
HTTPUpdateServer httpUpdater;


unsigned long timer_reconnect = 0;
unsigned long timer_server = 0;

// ----- Protótipo das funções -----
void startCameraServer();


void setup() 
{
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  pinMode(33, OUTPUT);
  digitalWrite(33, LOW);   
  delay(1000);      
  digitalWrite(33, HIGH);    
  delay(1000);
  digitalWrite(33, LOW);

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if(psramFound())
  {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } 
  else 
  {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // Inicialização da câmera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) 
  {
    Serial.printf("Falha ao iniciar câmera, erro 0x%x", err);
    while(1)
    {
      delay(500);
      digitalWrite(33, LOW);
      delay(500);
      digitalWrite(33, HIGH);
    }
  }
  else
  {
    Serial.println("Camera ok!");
  }

  sensor_t * s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1); // flip it back
    s->set_brightness(s, 1); // up the brightness just a bit
    s->set_saturation(s, -2); // lower the saturation
  }
  // drop down frame size for higher initial frame rate
  s->set_framesize(s, FRAMESIZE_QVGA);

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

  WiFi.setHostname(HOST);
  // Força o ESP a ficar acordado o tempo todo
  // se for alimentar ele via energia solar, é bom não mexer
  // para economizar energia. Caso contrário, recomendo
  // descomentar a linha abaixo.
  // WiFi.setSleep(false); 

  WiFi.begin(NET_ID, NET_PASS);

  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(200);
    digitalWrite(33, LOW);
    delay(200);
    digitalWrite(33, HIGH);
  }

  Serial.println("WiFi conectado");

  startCameraServer();

  Serial.print("Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println(" para conectar");
  digitalWrite(33, HIGH);   
  delay(200);      
  digitalWrite(33, LOW);  
  delay(200);    
  digitalWrite(33, HIGH);  

  if (MDNS.begin(HOST)) 
  {
    Serial.println("mDNS iniciado!");
  }

  httpUpdater.setup(&httpServer);
  httpServer.begin();

  MDNS.addService("http", "tcp", 80);
  Serial.printf("E use http://%s.local/update para atualizar à distância (OTA)\n", HOST);
}


void loop() 
{
  // Reconecta no Wi-Fi caso tenha perdido conexão
  if ((WiFi.status() != WL_CONNECTED) && (((millis() - timer_reconnect) > 10000) || millis() < timer_reconnect)) 
  {
    WiFi.disconnect();
    WiFi.begin(NET_ID, NET_PASS);
    
    timer_reconnect = millis();
  }

  // Lida com a resposta do servidor (/update)
  if (((millis() - timer_server) > 1000) || millis() < timer_server)
  {
    httpServer.handleClient(); // OTA

    timer_server = millis();
  }
} 
