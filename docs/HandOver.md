# Handover - AI Learning Device

> File này do mọi phiên làm việc (Claude hoặc AI khác) cập nhật ở cuối phiên. Quy tắc: xem `rule_code.md` mục 3.
> Cập nhật lần cuối: 2026-11-04 (phiên 1).

## 1. Đọc trước khi làm

1. `docs/rule_code.md` - quy tắc bàn giao file và viết comment.
2. `docs/Firmware_Construction.md` - kiến trúc layer, quy tắc phụ thuộc, trạng thái phần cứng.
3. `docs/Product_Construction.md` - đặc tả sản phẩm.
4. Repo tham khảo phong cách của chủ dự án: https://github.com/ngoxuanloc2309/simple_plc.git (mô hình layer, hợp đồng `sx_*`).

## 2. Thông tin dự án (ngắn)

- Thiết bị học tập AI (Learn / Play / Talk), môn đầu tiên: English.
- MCU: ESP32-S3 (WT32-S3-WROVER), ESP-IDF v5.4.3, ngôn ngữ C, UI dự kiến LVGL.
- AI nặng (STT, LLM, TTS) chạy ở backend, không chạy trên ESP32-S3.
- Repo: https://github.com/ngoxuanloc2309/learn_board_AI.git, project name `board_hmi_ai`.
- Chủ dự án dùng Windows, thư mục `E:\prj_canhan\Learning_board`.

## 3. Trạng thái hiện tại

| Hạng mục | Trạng thái |
|---|---|
| Khung project ESP-IDF, CMake nối các layer | Xong, **đã build thành công** trong sandbox (ESP-IDF v5.4.3) |
| Phase 1 trên máy của chủ dự án (`set-target`, build, flash) | **Chưa làm** (chủ dự án sẽ làm sau khi dev tương đối xong) |
| `board.h` (chân, số cổng) | Có, **chân chưa xác minh trên V15** |
| Peripheral `sx_gpio`, `sx_i2c`, `sx_spi`, `sx_i80` | Hợp đồng và hiện thực ESP32-S3 đã viết, **compile/link OK, chưa chạy trên phần cứng** |
| I2S, PWM, UART, SD card (`sx_sdcard`) | Chưa có |
| Modules (screenHMI, touch, speaker, storage) | Chỉ là file rỗng |
| Services (http, AI) | Chỉ là file rỗng |
| App tasks | Chỉ là file rỗng; `app_start()` log tên/phiên bản và gọi `board_init()` (rỗng) |
| LVGL, Wi-Fi, OTA | Chưa bắt đầu |

### Kiến trúc (tóm tắt, chi tiết ở Firmware_Construction.md)

```text
app, board (L4)  ->  services (L3)  ->  modules (L2)  ->  peripherals contracts (L1)  ->  platforms/esp32s3 (L0)
utils (không phụ thuộc layer nào)
```

Mỗi thư mục là một component ESP-IDF, tên component = tên thư mục (ví dụ `platforms/esp32s3` có tên `esp32s3`).

## 4. Thông tin phần cứng

Đã được chủ dự án cung cấp: board ZX3D50CE08S-V15-USRC 230208, ESP32-S3 / WT32-S3-WROVER, LCD 3.5 inch 480x320 ST7796 I80 8-bit,
touch FT6336U/FT5x06 I2C, microSD SPI, speaker I2S có amp onboard, RS485 onboard.

**Chưa xác minh (không được coi là sự thật):**

- GPIO của LCD, touch, SD, I2S trong `board.h` (lấy tham khảo từ WT32-SC01 Plus).
- Dung lượng Flash/PSRAM và loại PSRAM (quad hay octal). Nếu octal thì GPIO35-37 (I2S) không dùng được.
- Có microphone tích hợp hay không.
- Chân RS485.
- Trình tự reset chung LCD/touch (GPIO4).
- Chân strapping bị LCD dùng: GPIO 0, 3, 45, 46.
- Địa chỉ I2C của touch, hướng màn hình, tốc độ pixel clock tối đa của ST7796 trên board này.

Cách xác minh: schematic đúng revision V15, hoặc đo thực tế, hoặc `idf.py flash monitor` để xem thông số Flash/PSRAM khi boot.

## 5. Nhật ký các phiên

### Phiên 1 (2026-11-04)

Đã làm:

- Clone và đọc repo `learn_board_AI` (khung rỗng) và `simple_plc` (tham khảo kiến trúc).
- Chuyển kiến trúc sang mô hình simple_plc: hợp đồng `sx_*` ở `components/peripherals`, hiện thực chip ở `platforms/esp32s3`.
- Viết CMake cho từng layer; root `CMakeLists.txt` khai báo `EXTRA_COMPONENT_DIRS`.
- Viết `board.h` (nhãn REF/DESIGN/GIVEN cho từng chân), `board_init()` rỗng, `app_start()`.
- Viết hợp đồng và hiện thực `sx_gpio`, `sx_i2c`, `sx_spi`, `sx_i80`.
- Viết `rule_code.md`, `Firmware_Construction.md`, `handover.md`.
- Dựng ESP-IDF v5.4.3 + toolchain trong sandbox và build thành công. Build bắt được một lỗi: `REQUIRES platforms` sai, phải là `esp32s3`; đã sửa.

Quyết định đã chốt với chủ dự án:

- `LoganSDK/` là khung kiến trúc, dùng mô hình giống simple_plc (hợp đồng tách khỏi hiện thực).
- Đã bỏ `mqtt`. `http` giữ lại cho Phase 5.
- Code theo SOLID ở mức vừa đủ, tránh abstraction thừa.
- Mỗi lần giao: present từng file code, không present patch.
- Comment trong code chỉ tiếng Anh, ASCII, không ký tự đặc biệt.
- Bộ nhớ: static là mặc định; heap chỉ cho LVGL, network, buffer lớn, cấp phát lúc khởi tạo.

Điểm cần lưu ý:

- Các file đã present ở dạng file riêng lẻ; **chưa chắc đã được chép vào repo của chủ dự án** (remote chỉ có 2 commit gốc tại thời điểm kiểm tra). Phiên sau cần pull và kiểm tra trước.
- API `sx_i2c` và `sx_spi` cố ý giữ hình dạng giống simple_plc để module dùng lại được; khác biệt: cấu hình chứa số cổng/chân thay vì `pDriver`, trả `const` config, thêm `deinit`.
- `sx_i2c`: lệnh đọc/ghi không phân biệt được NACK với lỗi bus (driver ESP-IDF trả mã chung); dùng `sx_i2c_probe` để phân biệt.
- `sx_spi`: `timeout_ms` không có hiệu lực trên ESP32-S3 (driver không hỗ trợ).
- `utils/cqueue` có vài chỗ cần chỉnh (xem Firmware_Construction.md mục 9), chưa sửa vì là code của chủ dự án.

## 6. Việc đang chờ chủ dự án

Các lệnh đổi tên (Git trên Windows không nhận đổi hoa/thường, và các file rỗng chưa được di chuyển):

```powershell
git mv LoganSDK/components/peripherals/gpio/sx_gpio.c LoganSDK/platforms/esp32s3/gpio/esp32s3_gpio.c
git mv LoganSDK/components/peripherals/i2c/sx_i2c.c   LoganSDK/platforms/esp32s3/i2c/esp32s3_i2c.c
git mv LoganSDK/components/peripherals/spi/sx_spi.c   LoganSDK/platforms/esp32s3/spi/esp32s3_spi.c
git mv LoganSDK/components/peripherals/i80/sx_i80.c   LoganSDK/platforms/esp32s3/i80/esp32s3_i80.c
git mv -f LoganSDK/services/CmakeLists.txt LoganSDK/services/CMakeLists.txt
git mv LoganSDK/app/user/screen_app/screem_app.c LoganSDK/app/user/screen_app/screen_app.c
```

(Nếu các file `esp32s3_*.c` đã được chép vào đúng chỗ thì bỏ 4 lệnh đầu và xóa các file `sx_*.c` rỗng cũ.)

Cần quyết định hoặc cung cấp:

- Mục đích của thư mục `libs/` (đang trống, chưa nối vào build).
- Schematic V15 hoặc kết quả xác minh chân, loại PSRAM, microphone, RS485.

## 7. Kế hoạch tiếp theo

Thứ tự đề xuất (mỗi bước phải build được trước khi sang bước sau):

1. Hợp đồng và hiện thực còn thiếu cho Phase 2: `sx_i2s` (speaker), `sx_pwm` (đèn nền), `sx_sdcard` (SD + FATFS), `sx_uart` (RS485 - chờ xác minh chân). Cân nhắc ngắt cho `sx_gpio`.
2. Modules: `screenHMI` (chuỗi khởi tạo ST7796 qua `sx_i80`, reset/backlight), `touch` (FT6336U qua `sx_i2c`), `storage`, `speaker`.
3. Bring-up trên phần cứng (chủ dự án chạy `set-target`, build, flash): xác minh Flash/PSRAM, chân, LCD hiển thị, touch đọc được, SD mount, speaker ra tiếng, thử mic.
4. Wi-Fi.
5. Phase 3 (LVGL, màn hình) trở đi theo roadmap trong Product_Construction.md.

## 8. Ghi chú môi trường (cho phiên chạy trong sandbox)

- Sandbox Linux, **không truy cập được `E:\...` của chủ dự án**; làm việc bằng cách clone repo và present file.
- Mạng sandbox chặn `dl.espressif.com`. ESP-IDF cài được qua github.com (clone `v5.4.3 --recursive`, `install.sh esp32s3`), nhưng:
  - cần đặt `IDF_PYTHON_CHECK_CONSTRAINTS=no`;
  - cài Python requirements trực tiếp từ PyPI (`pip install --index-url https://pypi.org/simple -r tools/requirements/requirements.core.txt`);
  - cài `cmake==3.30.5` và `ninja` bằng pip (CMake 4.x chưa dùng cho IDF 5.4).
- Build thử trên bản sao của repo, không `set-target` trong repo thật.