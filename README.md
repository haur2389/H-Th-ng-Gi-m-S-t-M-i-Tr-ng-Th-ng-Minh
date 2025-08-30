# HỆ THỐNG GIÁM SÁT MÔI TRƯỜNG THÔNG MINH

## 1. TỔNG QUAN DỰ ÁN

### 1.1 Giới thiệu
Hệ thống giám sát môi trường thông minh là một giải pháp IoT tích hợp nhiều cảm biến để theo dõi các thông số môi trường quan trọng như nhiệt độ, độ ẩm, cường độ ánh sáng và chất lượng không khí. Hệ thống sử dụng vi điều khiển ESP32 với hệ điều hành thời gian thực FreeRTOS để xử lý đa tác vụ, đồng thời truyền dữ liệu lên nền tảng ThingsBoard để giám sát và phân tích.

### 1.2 Mục tiêu dự án
- **Mục tiêu chính**: Xây dựng hệ thống giám sát môi trường tự động, liên tục 24/7
- **Mục tiêu phụ**: 
  - Thu thập dữ liệu môi trường chính xác và ổn định
  - Truyền dữ liệu real-time lên cloud platform
  - Tối ưu hóa hiệu suất thông qua lập trình đa tác vụ
  - Cung cấp giao diện trực quan để theo dõi và phân tích dữ liệu

## 2. CƠ SỞ LÝ THUYẾT

### 2.1 Hệ điều hành thời gian thực FreeRTOS

#### 2.1.1 Khái niệm
FreeRTOS là một hệ điều hành thời gian thực (Real-Time Operating System - RTOS) mã nguồn mở, được thiết kế cho các vi điều khiển. FreeRTOS cung cấp khả năng xử lý đa tác vụ (multitasking) với độ trễ thấp và tính xác định cao.

#### 2.1.2 Đặc điểm chính
- **Preemptive multitasking**: Cho phép hệ thống chuyển đổi giữa các tác vụ dựa trên độ ưu tiên
- **Memory footprint nhỏ**: Chỉ sử dụng khoảng 4-9KB RAM
- **Tính di động cao**: Hỗ trợ nhiều kiến trúc vi điều khiển khác nhau
- **Real-time**: Đảm bảo thời gian phản hồi xác định

#### 2.1.3 Các khái niệm cơ bản

**Task (Tác vụ)**
- Là một đơn vị thực thi độc lập trong FreeRTOS
- Mỗi task có stack riêng và có thể ở các trạng thái: Running, Ready, Blocked, Suspended
- Task được quản lý bởi scheduler theo độ ưu tiên

**Task States (Trạng thái Task)**
- **Running**: Task đang được thực thi
- **Ready**: Task sẵn sàng chạy nhưng đang chờ CPU
- **Blocked**: Task đang chờ sự kiện (delay, semaphore, queue, etc.)
- **Suspended**: Task bị tạm dừng bằng lệnh

**Priority (Độ ưu tiên)**
- Giá trị số nguyên từ 0 (thấp nhất) đến configMAX_PRIORITIES-1 (cao nhất)
- Task có độ ưu tiên cao hơn sẽ được thực thi trước

### 2.2 Cơ chế đồng bộ hóa trong FreeRTOS

#### 2.2.1 Mutex (Mutual Exclusion)
Mutex là cơ chế đồng bộ hóa ngăn chặn nhiều task truy cập đồng thời vào tài nguyên chia sẻ.

**Đặc điểm của Mutex:**
- Chỉ có một task được phép sở hữu mutex tại một thời điểm
- Task khác muốn truy cập phải chờ cho đến khi mutex được giải phóng
- Hỗ trợ priority inheritance để tránh priority inversion

**Ứng dụng trong dự án:**
- Bảo vệ việc truy cập vào biến chia sẻ chứa dữ liệu cảm biến
- Đồng bộ hóa việc gửi dữ liệu qua WiFi
- Bảo vệ tài nguyên hardware (I2C, SPI bus)

#### 2.2.2 Queue
Queue là cơ chế truyền dữ liệu giữa các task theo nguyên tắc FIFO (First In, First Out).

**Đặc điểm:**
- Thread-safe: An toàn khi sử dụng đa tác vụ
- Blocking operations: Task có thể chờ khi queue đầy hoặc rỗng
- Configurable size: Kích thước có thể cấu hình

### 2.3 Internet of Things (IoT)

#### 2.3.1 Kiến trúc IoT
Hệ thống IoT thường gồm 4 tầng:
1. **Perception Layer**: Tầng cảm biến và thiết bị thu thập dữ liệu
2. **Network Layer**: Tầng truyền thông (WiFi, Bluetooth, LoRa...)
3. **Processing Layer**: Tầng xử lý dữ liệu và logic
4. **Application Layer**: Tầng ứng dụng và giao diện người dùng

#### 2.3.2 Giao thức truyền thông

**MQTT (Message Queuing Telemetry Transport)**
- Giao thức publish/subscribe nhẹ cho IoT
- Sử dụng TCP/IP làm giao thức nền
- Phù hợp cho mạng có băng thông thấp và độ tin cậy không cao

**HTTP/HTTPS**
- Giao thức client/server truyền thống
- Phù hợp cho việc truyền dữ liệu định kỳ
- Dễ implement và debug

## 3. THIẾT BỊ VÀ LINH KIỆN

### 3.1 Vi điều khiển ESP32

#### 3.1.1 Tổng quan
ESP32 là vi điều khiển 32-bit dual-core với tích hợp WiFi và Bluetooth, được phát triển bởi Espressif Systems.

#### 3.1.2 Đặc điểm kỹ thuật
- **CPU**: Dual-core Xtensa 32-bit LX6, tốc độ lên đến 240MHz
- **Memory**: 520KB SRAM, flash memory tùy chọn
- **Connectivity**: WiFi 802.11 b/g/n, Bluetooth v4.2 BR/EDR và BLE
- **GPIO**: 34 GPIO pins có thể lập trình
- **ADC**: 2 ADC 12-bit
- **Interfaces**: SPI, I2C, I2S, UART, CAN, PWM

#### 3.1.3 Ưu điểm trong dự án
- Hỗ trợ FreeRTOS native
- Kết nối WiFi tích hợp
- Nhiều interface cho cảm biến
- Hiệu suất cao với tiêu thụ điện năng thấp
- Cộng đồng phát triển lớn và tài liệu phong phú

### 3.2 Cảm biến và Module

#### 3.2.1 DHT22 - Cảm biến nhiệt độ và độ ẩm
**Nguyên lý hoạt động:**
- Sử dụng cảm biến điện dung để đo độ ẩm
- Sử dụng thermistor để đo nhiệt độ
- Tích hợp ADC và vi điều khiển để số hóa tín hiệu

**Thông số kỹ thuật:**
- Độ ẩm: 0-100% RH, độ chính xác ±2% RH
- Nhiệt độ: -40 đến +80°C, độ chính xác ±0.5°C
- Giao thức: Single-wire digital protocol
- Thời gian phản hồi: 2s

#### 3.2.2 MQ135 - Cảm biến chất lượng không khí
**Nguyên lý hoạt động:**
- Sử dụng cảm biến khí bán dẫn SnO2
- Điện trở của cảm biến thay đổi khi tiếp xúc với khí
- Đo được: NH3, NOx, benzene, CO2, alcohol, smoke

**Thông số kỹ thuật:**
- Điện áp hoạt động: 5V DC
- Dải phát hiện: 10-300ppm (NH3), 10-1000ppm (Benzene)
- Thời gian ổn định: 24h (lần đầu sử dụng)
- Thời gian phản hồi: <10s

#### 3.2.3 BH1750 - Cảm biến cường độ ánh sáng
**Nguyên lý hoạt động:**
- Photodiode chuyển đổi ánh sáng thành dòng điện
- ADC tích hợp chuyển đổi thành giá trị số
- Tự động điều chỉnh độ nhạy theo cường độ ánh sáng

**Thông số kỹ thuật:**
- Dải đo: 1-65535 lux
- Giao thức: I2C
- Độ phân giải: 1 lux
- Thời gian chuyển đổi: 120ms (mode cao)

#### 3.2.4 BMP180 - Cảm biến áp suất khí quyển
**Nguyên lý hoạt động:**
- Sử dụng cảm biến áp suất điện dung
- Tính toán nhiệt độ và áp suất từ giá trị thô
- Có thể tính độ cao dựa trên áp suất

**Thông số kỹ thuật:**
- Dải áp suất: 300-1100 hPa
- Độ chính xác: ±0.12 hPa
- Giao thức: I2C
- Điện áp: 1.8-3.6V

## 4. NỀN TẢNG THINGSBOARD

### 4.1 Giới thiệu ThingsBoard
ThingsBoard là nền tảng IoT mã nguồn mở cung cấp khả năng thu thập, xử lý, hiển thị và quản lý dữ liệu từ các thiết bị IoT.

### 4.2 Tính năng chính
- **Device Management**: Quản lý thiết bị và attributes
- **Data Collection**: Thu thập telemetry data từ thiết bị
- **Data Visualization**: Tạo dashboard và widget trực quan
- **Rule Engine**: Xử lý business logic và alert
- **API Support**: REST API và WebSocket
- **Multi-tenancy**: Hỗ trợ nhiều tenant

### 4.3 Kiến trúc ThingsBoard
- **Transport Layer**: MQTT, HTTP, CoAP protocols
- **Core Services**: Device management, telemetry processing
- **Rule Engine**: Event processing và business logic
- **Database**: Time-series và attributes storage
- **Web UI**: Dashboard và administration interface

## 5. THIẾT KẾ HỆ THỐNG

### 5.1 Kiến trúc tổng thể
```
┌─────────────┐    ┌──────────────┐    ┌─────────────┐
│  Sensors    │───▶│   ESP32      │───▶│ ThingsBoard │
│ DHT22       │    │  + FreeRTOS  │    │  Platform   │
│ MQ135       │    │              │    │             │
│ BH1750      │    │              │    │             │
│ BMP180      │    │              │    │             │
└─────────────┘    └──────────────┘    └─────────────┘
```

### 5.2 Thiết kế FreeRTOS Tasks

#### 5.2.1 Task Architecture
```
High Priority    │ WiFi Communication Task
                 │ 
Medium Priority  │ Sensor Reading Tasks
                 │ - DHT22 Task
                 │ - MQ135 Task  
                 │ - BH1750 Task
                 │ - BMP180 Task
                 │
Low Priority     │ System Monitoring Task
```

#### 5.2.2 Task Specifications

**Sensor Reading Tasks:**
- **Priority**: Medium (5-8)
- **Stack Size**: 2048-4096 bytes
- **Period**: 10-30 seconds tùy cảm biến
- **Function**: Đọc dữ liệu từ cảm biến, lưu vào shared memory

**WiFi Communication Task:**
- **Priority**: High (10)
- **Stack Size**: 8192 bytes  
- **Period**: 60 seconds hoặc khi có dữ liệu mới
- **Function**: Gửi dữ liệu lên ThingsBoard via MQTT/HTTP

**System Monitor Task:**
- **Priority**: Low (2)
- **Stack Size**: 2048 bytes
- **Period**: 300 seconds
- **Function**: Monitor heap memory, WiFi status, system health

### 5.3 Đồng bộ hóa và chia sẻ dữ liệu

#### 5.3.1 Shared Data Structure
```c
typedef struct {
    float temperature;      // DHT22
    float humidity;         // DHT22  
    float light_intensity;  // BH1750
    float air_quality;      // MQ135
    float pressure;         // BMP180
    uint32_t timestamp;
    bool data_valid;
} sensor_data_t;
```

#### 5.3.2 Mutex Usage
- **sensor_data_mutex**: Bảo vệ shared sensor data structure
- **wifi_mutex**: Bảo vệ WiFi communication
- **i2c_mutex**: Bảo vệ I2C bus (BH1750, BMP180)

#### 5.3.3 Queue Usage  
- **data_queue**: Truyền dữ liệu từ sensor tasks đến WiFi task
- **command_queue**: Nhận lệnh điều khiển từ ThingsBoard

### 5.4 Data Flow Diagram
```
Sensor Tasks ──┐
               ├──▶ Mutex ──▶ Shared Memory ──▶ WiFi Task ──▶ ThingsBoard
               │
System Task ───┘
```

## 6. GIAO THỨC TRUYỀN THÔNG

### 6.1 MQTT Protocol
#### 6.1.1 Topics Structure
- **Telemetry**: `v1/devices/me/telemetry`
- **Attributes**: `v1/devices/me/attributes`  
- **RPC Request**: `v1/devices/me/rpc/request/+`
- **RPC Response**: `v1/devices/me/rpc/response/`

#### 6.1.2 Message Format
```json
{
  "temperature": 25.6,
  "humidity": 65.2,
  "light": 450.0,
  "air_quality": 120.5,
  "pressure": 1013.25,
  "timestamp": 1640995200000
}
```

### 6.2 Error Handling và Retry Mechanism
- **Connection timeout**: 30 seconds
- **Retry attempts**: 3 lần
- **Backoff strategy**: Exponential backoff
- **Offline storage**: Buffer dữ liệu khi mất kết nối

## 7. TÍNH NĂNG NÂNG CAO

### 7.1 Watchdog Timer
- Hardware watchdog để reset system khi hang
- Software watchdog cho từng task quan trọng
- Timeout: 60-120 seconds

### 7.2 Power Management
- Deep sleep mode khi không hoạt động
- Dynamic frequency scaling
- Tối ưu power consumption cho battery operation

### 7.3 OTA Updates
- Firmware update qua WiFi
- Rollback mechanism khi update fail
- Secure boot và encrypted firmware

### 7.4 Data Validation và Filtering
- Range check cho giá trị cảm biến
- Moving average filter để giảm noise
- Outlier detection và rejection

## 8. PERFORMANCE OPTIMIZATION

### 8.1 Memory Management
- Static allocation cho critical tasks
- Heap monitoring và leak detection
- Stack overflow protection

### 8.2 CPU Optimization  
- Task affinity cho dual-core ESP32
- Interrupt priority configuration
- DMA usage cho data transfer

### 8.3 Network Optimization
- Connection pooling
- Compression cho large payloads
- Batch sending để giảm overhead

## 9. TESTING STRATEGY

### 9.1 Unit Testing
- Individual sensor reading functions
- Data validation algorithms
- Communication protocols

### 9.2 Integration Testing
- Multi-task coordination
- Network connectivity
- ThingsBoard integration

### 9.3 Stress Testing
- Long-term operation (24h+)
- Network interruption scenarios
- Memory leak detection

## 10. KẾT LUẬN

Hệ thống giám sát môi trường thông minh kết hợp hiệu quả giữa hardware sensing, real-time operating system, và cloud platform để tạo ra một giải pháp IoT hoàn chỉnh. Việc sử dụng FreeRTOS cho phép hệ thống xử lý đa tác vụ hiệu quả, trong khi ThingsBoard cung cấp khả năng visualization và management mạnh mẽ. Thiết kế này có thể mở rộng và tùy chỉnh để phù hợp với nhiều ứng dụng giám sát môi trường khác nhau.
