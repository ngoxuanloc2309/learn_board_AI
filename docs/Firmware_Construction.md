# Firmware Construction — AI Learning Device

> Tài liệu kiến trúc firmware. Bổ sung cho `Product_Construction.md` (định hướng sản phẩm).
> Trạng thái: v0.2 — khung kiến trúc theo mô hình simple_plc, chưa có driver.
> Môi trường: ESP-IDF v5.4.3, target `esp32s3`, ngôn ngữ C.

## 1. Cấu trúc layer

Mô hình theo `simple_plc`: hợp đồng (Layer 1) tách khỏi hiện thực theo chip (Layer 0).

```text
main/                         app_main() -> app_start(). Không chứa logic.
LoganSDK/
  app/                        Layer 4: task chương trình và khởi động
    user/<x>_app/               mỗi chương trình một thư mục (screen, speaker, AI, http, ntp)
  board/                      Layer 4: chân, số cổng, wiring (board_init), sở hữu instance driver
  services/                   Layer 3: http, AI backend/frontend. Không biết phần cứng.
  components/
    modules/                  Layer 2: thiết bị logic: screenHMI, touch, speaker, storage, sdcard
    peripherals/              Layer 1: hợp đồng sx_gpio, sx_i2c, sx_spi, sx_i80 (CHỈ header)
  platforms/
    esp32s3/                  Layer 0: hiện thực hợp đồng bằng ESP-IDF (esp32s3_gpio.c, ...)
  utils/                      Layer U: cqueue, sx_filter, ... (không phụ thuộc layer nào)
  libs/                       (chưa dùng; xác định mục đích trước khi thêm nội dung)
```

Mỗi thư mục có `CMakeLists.txt` riêng và là **một component ESP-IDF**. Root `CMakeLists.txt`
khai báo chúng qua `EXTRA_COMPONENT_DIRS` (đặt trước `include(project.cmake)`).

## 2. Quy tắc phụ thuộc

Chỉ phụ thuộc xuống dưới. Tầng dưới không include tầng trên.

| Component | Được phép `REQUIRES` | Ghi chú |
|---|---|---|
| `main` | `app` | |
| `app` | `board`, `modules`, `services`, `utils`, `esp32s3` | Layer thấy tất cả; `esp32s3` (thư mục `platforms/esp32s3`) có mặt để hiện thực chip được link. Tên component ESP-IDF là tên thư mục, nên đổi chip thì đổi tên này |
| `board` | `peripherals`, `modules` | Không include `esp32s3`; chỉ dùng hợp đồng |
| `services` | `utils` | Thêm `esp_http_client`, `esp-tls`... khi code thật sự dùng |
| `modules` | `peripherals`, `utils` | Nhận cấu hình qua tham số, không include `board.h`. `sdcard` chỉ dùng `sx_spi` |
| `esp32s3` (`platforms/esp32s3`) | `peripherals`; PRIV: `driver`, `esp_lcd` | **Layer duy nhất được include header driver ESP-IDF** |
| `peripherals` | (không) | Chỉ header, không phụ thuộc ESP-IDF |
| `utils` | (không) | |

Hệ quả:
- Đổi chip: thêm `platforms/<chip>/`, không sửa layer trên.
- Đổi board: sửa `board/`.
- Nếu ai include ngược chiều hoặc include ESP-IDF ở layer trên, build báo lỗi thiếu header ngay.

## 3. Quy ước

- Hợp đồng ở `components/peripherals/<x>/sx_<x>.h`; hiện thực ở `platforms/esp32s3/<x>/esp32s3_<x>.c`.
- API: `sx_<periph>_<action>`, ví dụ `sx_gpio_init`, `sx_gpio_read`, `sx_gpio_write`.
- Cấu hình: `sx_<periph>_config_t` chứa số cổng và chân (không chứa handle driver; hiện thực tự tạo handle
  bên trong vì ESP-IDF không có bước khởi tạo sẵn như CubeMX).
- Instance: `sx_<periph>_t` do caller cấp (thường là static trong `board.c`), `board` sở hữu mọi instance.
- Mã trả về: `SX_<P>_OK = 0`, lỗi là số âm (`SX_<P>_ERR_PARAM`, `_ERR_BUS`, `_ERR_TIMEOUT`...).
- Giá trị chân/cổng đến từ `board.h`, dùng ở `board.c` để điền cấu hình; layer dưới không include `board.h`.
- Header công khai chỉ chứa API cần thiết; chi tiết nội bộ để `static` trong `.c`.
- File nguồn mới phải được thêm vào `SRCS` của component tương ứng.

## 4. SOLID trong C (áp dụng vừa đủ)

- **S:** mỗi file một trách nhiệm (`sx_i2c` chỉ lo bus; `touch` chỉ lo bộ điều khiển cảm ứng).
- **O / L:** chỗ nào thật sự có nhiều cách triển khai (ví dụ nhiều chip touch) thì dùng bảng hàm
  (`struct { init; read; }`). Không tạo bảng hàm khi chỉ có một cách.
- **I:** header nhỏ, mỗi module chỉ lộ những gì caller cần.
- **D:** module phụ thuộc hợp đồng `sx_*` và cấu hình truyền vào, không phụ thuộc ESP-IDF hay board cụ thể.

### Quy tắc bộ nhớ

- Mặc định dùng bộ nhớ tĩnh, buffer do caller cấp (ví dụ `cqueue_init_static`).
- Heap chỉ dùng khi có chủ đích: LVGL, network stack, buffer lớn (audio, frame) đặt ở PSRAM.
- Cấp phát chỉ lúc khởi tạo, chỉ rõ kiểu bộ nhớ (`MALLOC_CAP_*`), không cấp phát/giải phóng trong vòng lặp thường trực.
- `utils` không dùng `malloc` trong đường chạy bình thường.

## 5. Quy tắc runtime

- Không block task LVGL bằng network hoặc xử lý chậm; giao tiếp qua queue/event.
- Tạo task khi có nhu cầu, không tạo sẵn cho mọi module. Đo CPU/heap trước khi tách thêm.
- DMA buffer phải ở bộ nhớ DMA-capable, đúng alignment. Cân nhắc PSRAM cho buffer lớn sau khi xác minh PSRAM thật.
- `cqueue` **không thread-safe**. Dùng giữa nhiều task/ISR phải bọc mutex/critical section hoặc dùng FreeRTOS queue.

## 6. Trạng thái xác minh phần cứng

| Hạng mục | Trạng thái |
|---|---|
| Board `ZX3D50CE08S-V15-USRC 230208`, ESP32-S3 / WT32-S3-WROVER | Đã cung cấp bởi chủ dự án |
| LCD 480x320, ST7796 family, I80 8-bit | Đã cung cấp bởi chủ dự án |
| GPIO LCD / touch / SD / I2S | **Chưa xác minh trên V15** (mapping tham khảo WT32-SC01 Plus) |
| Flash / PSRAM (dung lượng, loại quad/octal) | **Chưa xác minh** |
| Microphone | **Chưa xác minh** |
| RS485 pinout | **Chưa xác minh**, chưa đưa vào `board.h` |
| Trình tự reset chung LCD/touch (GPIO4) | **Chưa xác minh** |

`board.h` đánh dấu từng nhóm bằng `REF` / `DESIGN` / `GIVEN` và có cờ
`BOARD_PINMAP_VERIFIED` (hiện = 0). Chỉ đặt 1 sau khi đối chiếu schematic V15 hoặc đo thực tế.

**Cảnh báo:** GPIO 35–37 (I2S) không dùng được nếu module hóa ra có PSRAM octal. GPIO 0, 3, 45, 46
(LCD) là chân strapping của ESP32-S3; cần kiểm tra khi bring-up để không ảnh hưởng boot.

## 7. Lộ trình

Giữ nguyên theo handover: Phase 1 khởi tạo, Phase 2 hardware bring-up, Phase 3 UI (LVGL),
Phase 4 learning features, Phase 5 AI integration. Mỗi phase phải build được trước khi sang phase sau.
Phase 1 hiện ở trạng thái: khung đã nối build; **chưa chạy `set-target`/build** theo quyết định của chủ dự án.

## 8. Trạng thái peripheral (Phase 2)

| Hợp đồng | Hiện thực ESP32-S3 | Ghi chú |
|---|---|---|
| `sx_gpio` | Có | Chưa có ngắt (cần cho chân INT của touch nếu không poll) |
| `sx_i2c` | Có (driver I2C master mới) | Lệnh đọc/ghi không phân biệt được NACK với lỗi bus; dùng `sx_i2c_probe` |
| `sx_spi` | Có | CS mềm qua `sx_gpio`; `timeout_ms` không có hiệu lực trên nền này |
| `sx_i80` | Có | Chỉ mức bus và panel IO; chuỗi khởi tạo ST7796 thuộc module `screenHMI` |
| `sx_i2s` | Có | Speaker I2S TX; chân vẫn chưa xác minh |
| `sx_pwm` | Có | LEDC đèn nền; timer, channel và chân do board cung cấp |
| `sx_uart` | Có | UART thường hoặc RS485 half-duplex; chân RS485 vẫn chưa xác minh |

Các hợp đồng trên đã được nối vào component CMake và đã build thành công trong sandbox (phiên 5, ESP-IDF v5.4.3, target esp32s3).
**Chưa chạy trên phần cứng.**

**Thẻ SD không phải peripheral.** Thẻ là thiết bị nằm trên bus SPI nên thuộc Layer 2: module `sdcard` (`components/modules/sdcard`)
nói giao thức SD-SPI qua `sx_spi` và phơi API khối 512 byte (`sdcard_init`, `sdcard_read_blocks`, `sdcard_write_blocks`).
Module không include ESP-IDF và không biết hệ thống tệp. Nối FatFs (mount, `f_open`...) chưa làm vì cần thêm dependency `fatfs`
vào layer `modules`; chờ chủ dự án quyết định (xem `HandOver.md` mục 6).

`sx_gpio` có thêm đăng ký và gỡ callback ngắt ISR. Callback phải ngắn, không block và chỉ gọi API ISR-safe.

## 9. Việc còn mở

- Mục đích của `libs/` (hiện không có nội dung).
- Bảng chọn platform/board bằng CMake (như `splc_select.cmake` của simple_plc): chỉ làm khi có chip hoặc board thứ hai.
- `utils/cqueue` cần chỉnh nhỏ (chưa sửa, chờ chủ dự án duyệt):
  - `cqueue.h` dùng `#if QUEUE_DEBUG_ENABLE` nhưng macro định nghĩa là `CQUEUE_DEBUG_ENABLE` (đang chạy đúng do macro chưa định nghĩa = 0).
  - Header guard `__QUEUE_H` là tên dành riêng của compiler và dễ trùng; nên đổi thành `CQUEUE_H`.
  - `cqueue_init` dùng `malloc` nhưng chưa có hàm giải phóng.
  - Con trỏ `void*` cộng số học (`buff + n`) là extension của GCC; nên ép sang `uint8_t*`.