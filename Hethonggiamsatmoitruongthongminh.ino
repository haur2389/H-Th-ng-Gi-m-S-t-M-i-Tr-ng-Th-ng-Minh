#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <Wire.h>
#include <BH1750.h>
#include <Adafruit_BMP085.h>  // Thư viện cho BMP180
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

// ========== CẤU HÌNH CHÂN VÀ THIẾT BỊ ==========
#define DHT_PIN 4
#define DHT_TYPE DHT11
#define MQ135_PIN 36    // GPIO36 trên ESP32
#define LED_STATUS_PIN 2
#define BUZZER_PIN 5
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22

// ========== CẤU HÌNH WIFI VÀ THINGSBOARD ==========
const char* ssid = "F";
const char* password = "23890000";
const char* thingsboard_server = "demo.thingsboard.io";
const char* device_token = "iaahmvc62cbxka8by5dk";
const int thingsboard_port = 80;

// ========== KHỞI TẠO CÁC CẢM BIẾN ==========
DHT dht(DHT_PIN, DHT_TYPE);
BH1750 lightMeter;
Adafruit_BMP085 bmp;  // BMP180 sử dụng thư viện BMP085

// ========== CẤU TRÚC DỮ LIỆU CẢM BIẾN ==========
struct SensorData {
    float temperature;
    float humidity;
    float light;
    float pressure;
    float air_quality;
    unsigned long timestamp;
};

// ========== MUTEX VÀ QUEUE FREERTOS ==========
SemaphoreHandle_t xSensorDataMutex;
SemaphoreHandle_t xWiFiMutex;
QueueHandle_t xSensorQueue;

// ========== BIẾN TOÀN CỤC ==========
SensorData currentSensorData;
bool wifiConnected = false;
bool systemRunning = true;
bool bh1750_initialized = false;
bool bmp180_initialized = false;

// ========== HÀM SCAN I2C DEVICES ==========
void scanI2CDevices() {
    Serial.println("Scanning I2C devices...");
    int deviceCount = 0;
    
    for (byte address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        if (Wire.endTransmission() == 0) {
            Serial.printf("I2C device found at address 0x%02X", address);
            
            // Xác định thiết bị dựa trên địa chỉ
            if (address == 0x23 || address == 0x5C) {
                Serial.print(" (Likely BH1750)");
            } else if (address == 0x77) {
                Serial.print(" (Likely BMP180)");   
            }
            Serial.println();
            deviceCount++;
        }
    }
    
    if (deviceCount == 0) {
        Serial.println(" I2C devices found - Check wiring!");
    } else {
        Serial.printf("Found %d I2C device(s)\n", deviceCount);
    }
}

// ========== HÀM KHỞI TẠO BH1750 VỚI RETRY ==========
bool initializeBH1750() {
    Serial.println("Initializing BH1750...");
    
    // Reset I2C bus trước khi khởi tạo
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    delay(100);
    
    // Thử các địa chỉ I2C có thể có của BH1750
    uint8_t bh1750_addresses[] = {0x23, 0x5C};
    
    for (int i = 0; i < 2; i++) {
        Serial.printf("Trying BH1750 at address 0x%02X...\n", bh1750_addresses[i]);
        
        Wire.beginTransmission(bh1750_addresses[i]);
        if (Wire.endTransmission() == 0) {
            Serial.printf("BH1750 responded at address 0x%02X\n", bh1750_addresses[i]);
            
            // Thử khởi tạo với địa chỉ tìm thấy
            if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, bh1750_addresses[i])) {
                Serial.println(" BH1750 initialized successfully");
                
                // Đợi cảm biến ổn định
                delay(300);
                
                // Test đọc một giá trị
                float testReading = lightMeter.readLightLevel();
                Serial.printf("BH1750 test reading: %.2f lux\n", testReading);
                
                if (testReading >= 0 && !isnan(testReading)) {
                    Serial.println(" BH1750 test reading successful");
                    return true;
                } else {
                    Serial.println(" BH1750 test reading ");
                }
            } else {
                Serial.printf(" BH1750 initialization at 0x%02X\n", bh1750_addresses[i]);
            }
        }
    }
    
    // Nếu không tìm thấy địa chỉ cụ thể, thử khởi tạo mặc định
    Serial.println("Trying BH1750 default initialization...");
    if (lightMeter.begin()) {
        delay(300);
        float testReading = lightMeter.readLightLevel();
        if (testReading >= 0 && !isnan(testReading)) {
            Serial.println("✓ BH1750 initialized with default settings");
            return true;
        }
    }
    
    Serial.println(" BH1750 initialization completed ");
    return false;
}

// ========== HÀM KHỞI TẠO BMP180 ==========
bool initializeBMP180() {
    Serial.println("Initializing BMP180...");
    
    if (bmp.begin()) {
        Serial.println("✓ BMP180 initialized successfully");
        
        // Test đọc dữ liệu
        float testTemp = bmp.readTemperature();
        float testPressure = bmp.readPressure() / 100.0; // chuyển sang hPa
        
        Serial.printf("BMP180 test - Temp: %.2f°C, Pressure: %.2f hPa\n", testTemp, testPressure);
        
        if (!isnan(testTemp) && !isnan(testPressure) && testPressure > 0) {
            return true;
        } else {
            Serial.println(" BMP180 test reading ");
            return false;
        }
    } else {
        Serial.println(" BMP180 initialization ");
        return false;
    }
}

// ========== TASK: ĐỌC CẢM BIẾN DHT11 ==========
void taskReadDHT(void *parameter) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(3000);
    
    while(systemRunning) {
        float temp = dht.readTemperature();
        float hum = dht.readHumidity();
        
        if (!isnan(temp) && !isnan(hum)) {
            if (xSemaphoreTake(xSensorDataMutex, portMAX_DELAY)) {
                currentSensorData.temperature = temp;
                currentSensorData.humidity = hum;
                
                Serial.printf("[DHT Task] Temp: %.2f°C, Humidity: %.2f%%\n", temp, hum);
                
                if (temp > 35.0) {
                    digitalWrite(BUZZER_PIN, HIGH);
                    delay(100);
                    digitalWrite(BUZZER_PIN, LOW);
                }
                
                xSemaphoreGive(xSensorDataMutex);
            }
        } else {
            Serial.println("[DHT Task] Lỗi đọc DHT11!");
        }
        
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
    vTaskDelete(NULL);
}

// ========== TASK: ĐỌC CẢM BIẾN ÁNH SÁNG BH1750 ==========
void taskReadLight(void *parameter) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(2500); // Tăng thời gian để giảm lỗi
    
    while(systemRunning) {
        if (bh1750_initialized) {
            float lux = lightMeter.readLightLevel();
            
            if (lux >= 0 && !isnan(lux)) {
                if (xSemaphoreTake(xSensorDataMutex, portMAX_DELAY)) {
                    currentSensorData.light = lux;
                    Serial.printf("[Light Task] Ánh sáng: %.2f lux\n", lux);
                    xSemaphoreGive(xSensorDataMutex);
                }
            } else {
                Serial.println("[Light Task]  đọc BH1750");
                bh1750_initialized = initializeBH1750();
                
                if (!bh1750_initialized) {
                    if (xSemaphoreTake(xSensorDataMutex, portMAX_DELAY)) {
                        currentSensorData.light = -1; // Giá trị báo lỗi
                        xSemaphoreGive(xSensorDataMutex);
                    }
                }
            }
        } else {
            Serial.println("[Light Task]  khởi tạo BH1750...");
            bh1750_initialized = initializeBH1750();
            
            if (!bh1750_initialized) {
                if (xSemaphoreTake(xSensorDataMutex, portMAX_DELAY)) {
                    currentSensorData.light = -1;
                    xSemaphoreGive(xSensorDataMutex);
                }
                // Đợi lâu hơn trước khi thử lại
                vTaskDelay(pdMS_TO_TICKS(5000));
            }
        }
        
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
    vTaskDelete(NULL);
}

// ========== TASK: ĐỌC CẢM BIẾN ÁP SUẤT BMP180 ==========
void taskReadPressure(void *parameter) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(4000);
    
    while(systemRunning) {
        if (bmp180_initialized) {
            float pressure = bmp.readPressure() / 100.0; // Chuyển đổi sang hPa
            float bmpTemp = bmp.readTemperature(); // Đọc nhiệt độ từ BMP180
            
            if (pressure > 0 && !isnan(pressure)) {
                if (xSemaphoreTake(xSensorDataMutex, portMAX_DELAY)) {
                    currentSensorData.pressure = pressure;
                    Serial.printf("[Pressure Task] Áp suất: %.2f hPa, BMP Temp: %.2f°C\n", pressure, bmpTemp);
                    xSemaphoreGive(xSensorDataMutex);
                }
            } else {
                Serial.println("[Pressure Task] đọc BMP180...");
                bmp180_initialized = initializeBMP180();
            }
        } else {
            Serial.println("[Pressure Task] khởi tạo BMP180...");
            bmp180_initialized = initializeBMP180();
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
        
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
    vTaskDelete(NULL);
}

// ========== TASK: ĐỌC CẢM BIẾN CHẤT LƯỢNG KHÔNG KHÍ MQ135 ==========
void taskReadAirQuality(void *parameter) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(5000);
    
    while(systemRunning) {
        int sensorValue = analogRead(MQ135_PIN);
        float airQuality = (sensorValue / 4095.0) * 1000; // ppm ước tính
        
        if (xSemaphoreTake(xSensorDataMutex, portMAX_DELAY)) {
            currentSensorData.air_quality = airQuality;
            Serial.printf("[Air Quality Task] Raw: %d, PPM: %.2f\n", sensorValue, airQuality);
            
            if (airQuality > 400) {
                digitalWrite(LED_STATUS_PIN, HIGH);
                digitalWrite(BUZZER_PIN, HIGH);
                delay(200);
                digitalWrite(BUZZER_PIN, LOW);
                digitalWrite(LED_STATUS_PIN, LOW);
            }
            
            xSemaphoreGive(xSensorDataMutex);
        }
        
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
    vTaskDelete(NULL);
}

// ========== TASK: QUẢN LÝ WIFI ==========
void taskWiFiManager(void *parameter) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(10000);
    
    while(systemRunning) {
        if (xSemaphoreTake(xWiFiMutex, portMAX_DELAY)) {
            if (WiFi.status() != WL_CONNECTED) {
                Serial.println("[WiFi Task] Kết nối lại WiFi...");
                WiFi.begin(ssid, password);
                
                int attempts = 0;
                while (WiFi.status() != WL_CONNECTED && attempts < 20) {
                    delay(500);
                    Serial.print(".");
                    attempts++;
                }
                
                if (WiFi.status() == WL_CONNECTED) {
                    wifiConnected = true;
                    Serial.println("\n[WiFi Task] WiFi đã kết nối!");
                    Serial.printf("[WiFi Task] IP: %s\n", WiFi.localIP().toString().c_str());
                } else {
                    wifiConnected = false;
                    Serial.println("\n[WiFi Task] Không thể kết nối WiFi!");
                }
            } else {
                wifiConnected = true;
            }
            xSemaphoreGive(xWiFiMutex);
        }
        
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
    vTaskDelete(NULL);
}

// ========== TASK: GỬI DỮ LIỆU LÊN THINGSBOARD ==========
void taskSendData(void *parameter) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(15000);
    
    while(systemRunning) {
        if (wifiConnected) {
            SensorData dataToSend;
            
            if (xSemaphoreTake(xSensorDataMutex, portMAX_DELAY)) {
                dataToSend = currentSensorData;
                dataToSend.timestamp = millis();
                xSemaphoreGive(xSensorDataMutex);
            }
            
            DynamicJsonDocument doc(1024);
            doc["temperature"] = dataToSend.temperature;
            doc["humidity"] = dataToSend.humidity;
            doc["pressure"] = dataToSend.pressure;
            doc["air_quality"] = dataToSend.air_quality;
            doc["timestamp"] = dataToSend.timestamp;
            doc["light"] = dataToSend.light;
            // Chỉ gửi dữ liệu ánh sáng nếu BH1750 hoạt động
            if (bh1750_initialized && dataToSend.light >= 0) {
                doc["light"] = dataToSend.light;
            }
            
            String jsonString;
            serializeJson(doc, jsonString);
            
            if (xSemaphoreTake(xWiFiMutex, portMAX_DELAY)) {
                HTTPClient http;
                String url = "http://" + String(thingsboard_server) + "/api/v1/" + String(device_token) + "/telemetry";
                
                http.begin(url);
                http.addHeader("Content-Type", "application/json");
                
                int httpResponseCode = http.POST(jsonString);
                
                if (httpResponseCode > 0) {
                    Serial.printf("[Send Task] Gửi thành công! Code: %d\n", httpResponseCode);
                    Serial.printf("[Send Task] Data: %s\n", jsonString.c_str());
                } else {
                    Serial.printf("[Send Task] Lỗi gửi dữ liệu: %d\n", httpResponseCode);
                }
                
                http.end();
                xSemaphoreGive(xWiFiMutex);
            }
        } else {
            Serial.println("[Send Task] WiFi chưa kết nối");
        }
        
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
    vTaskDelete(NULL);
}

// ========== TASK: HIỂN THỊ TRẠNG THÁI ==========
void taskStatusDisplay(void *parameter) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(8000);
    
    while(systemRunning) {
        if (xSemaphoreTake(xSensorDataMutex, portMAX_DELAY)) {
            Serial.println("\n========== TRẠNG THÁI HỆ THỐNG ==========");
            Serial.printf("WiFi: %s\n", wifiConnected ? "Kết nối" : "Mất kết nối");
            Serial.printf("Nhiệt độ: %.2f°C\n", currentSensorData.temperature);
            Serial.printf("Độ ẩm: %.2f%%\n", currentSensorData.humidity);
            
            if (bh1750_initialized && currentSensorData.light >= 0) {
                Serial.printf("Ánh sáng: %.2f lux (BH1750 OK)\n", currentSensorData.light);
            } else {
                Serial.println("Ánh sáng: -1lx");
            }
            
            if (bmp180_initialized) {
                Serial.printf("Áp suất: %.2f hPa (BMP180 OK)\n", currentSensorData.pressure);
            } else {
                Serial.println("Áp suất: 0hPa");
            }
            
            Serial.printf("Chất lượng không khí: %.2f ppm\n", currentSensorData.air_quality);
            Serial.printf("RAM trống: %d bytes\n", ESP.getFreeHeap());
            Serial.println("=========================================\n");
            xSemaphoreGive(xSensorDataMutex);
        }
        
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
    vTaskDelete(NULL);
}

// ========== HÀM KHỞI TẠO CÁC CẢM BIẾN ==========
void initializeSensors() {
    Serial.println("Khởi tạo các cảm biến...");
    
    // Khởi tạo DHT11
    dht.begin();
    Serial.println("✓ DHT11 initialized");
    
    // Khởi tạo I2C với tốc độ chậm cho ổn định
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(50000); // 50kHz - chậm hơn để ổn định
    Serial.println("✓ I2C bus initialized at 50kHz");
    
    delay(500); // Đợi I2C ổn định
    
    // Scan các thiết bị I2C
    scanI2CDevices();
    
    // Khởi tạo BH1750
    bh1750_initialized = initializeBH1750();
    
    // Khởi tạo BMP180
    bmp180_initialized = initializeBMP180();
    
    // Khởi tạo các chân GPIO
    pinMode(LED_STATUS_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(MQ135_PIN, INPUT);
    
    digitalWrite(LED_STATUS_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    
    // Báo hiệu khởi tạo hoàn tất
    for (int i = 0; i < 3; i++) {
        digitalWrite(LED_STATUS_PIN, HIGH);
        delay(100);
        digitalWrite(LED_STATUS_PIN, LOW);
        delay(100);
    }
    
    Serial.println("Hoàn tất khởi tạo cảm biến!");
}

// ========== HÀM SETUP ==========
void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("=== HỆ THỐNG GIÁM SÁT MÔI TRƯỜNG - BH1750 & BMP180 ===");
    
    initializeSensors();
    
    // Khởi tạo WiFi
    WiFi.begin(ssid, password);
    Serial.println("Đang kết nối WiFi...");
    
    // Khởi tạo mutex và queue
    xSensorDataMutex = xSemaphoreCreateMutex();
    xWiFiMutex = xSemaphoreCreateMutex();
    xSensorQueue = xQueueCreate(10, sizeof(SensorData));
    
    if (xSensorDataMutex == NULL || xWiFiMutex == NULL) {
        Serial.println("Lỗi tạo mutex!");
        return;
    }
    
    memset(&currentSensorData, 0, sizeof(SensorData));
    
    // Tạo các task với priority và core phân bổ hợp lý
    xTaskCreatePinnedToCore(taskReadDHT, "DHT_Task", 4096, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(taskReadLight, "Light_Task", 4096, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(taskReadPressure, "Pressure_Task", 4096, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(taskReadAirQuality, "AirQuality_Task", 4096, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(taskWiFiManager, "WiFi_Task", 8192, NULL, 3, NULL, 0);
    xTaskCreatePinnedToCore(taskSendData, "Send_Task", 8192, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(taskStatusDisplay, "Status_Task", 4096, NULL, 1, NULL, 0);
    
    Serial.println("Tất cả task đã được tạo!");
    Serial.println("Hệ thống đang chạy...\n");
}

// ========== HÀM LOOP ==========
void loop() {
    // Watchdog và monitoring
    delay(1000);
    
    // Có thể thêm emergency restart nếu cần
    static unsigned long lastHeapCheck = 0;
    if (millis() - lastHeapCheck > 10000) { // Check mỗi 10 giây
        if (ESP.getFreeHeap() < 50000) { // Nếu RAM < 50KB
            Serial.println("WARNING: Low memory, consider restart");
        }
        lastHeapCheck = millis();
    }
}
