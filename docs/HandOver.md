# Handover - AI Learning Device

> File này do mọi phiên làm việc (Claude hoặc AI khác) cập nhật ở cuối phiên. Quy tắc: xem `rule_code.md` mục 3.
> Cập nhật lần cuối: 2026-11-04 (phiên 3).

## 1. Đọc trước khi làm

1. `docs/RuleAgent.md` - cách làm việc của AI/agent: quy trình đầu/cuối phiên, nguyên tắc bắt buộc.
2. `docs/rule_code.md` - quy tắc bàn giao file và viết comment.
3. `docs/Firmware_Construction.md` - kiến trúc layer, quy tắc phụ thuộc, trạng thái phần cứng.
4. `docs/Product_Construction.md` - đặc tả sản phẩm.
5. Repo tham khảo phong cách của chủ dự án: https://github.com/ngoxuanloc2309/simple_plc.git (mô hình layer, hợp đồng `sx_*`).

## 2. Thông tin dự án (ngắn)

- Thiết bị học tập AI (Learn / Play / Talk), môn đầu tiên: English.
- MCU: ESP32-S3 (WT32-S3-WROVER), ESP-IDF v5.4.3, ngôn ngữ C, UI dự kiến LVGL.
- AI nặng (STT, LLM, TTS) chạy ở backend, không chạy trên ESP32-S3.
- Repo: https://github.com/ngoxuanloc2309/learn_board_AI.git, project name `board_hmi_ai`.
- Chủ dự án dùng Windows, thư mục `E:\prj_canhan\Learning_board`.

## 3. Trạng thái hiện tại

| Hạng mục | Trạng thái |
|---|---|
| Khung project ESP-IDF, CMake nối các layer | Xong ở phiên 3: **đã build thành công** trong sandbox (ESP-IDF v5.4.3, target esp32s3, 0 cảnh báo). Phiên 1 ghi đã build nhưng repo thật lúc đó chưa có các file CMake này |
| Phase 1 trên máy của chủ dự án (`set-target`, build, flash) | **Chưa làm** (chủ dự án sẽ làm sau khi dev tương đối xong) |
| `board.h` (chân, số cổng) | **Chỉ là stub** (enum `BoardType_t`), chưa có chân nào, chưa có nhãn `REF/DESIGN/GIVEN` hay `BOARD_PINMAP_VERIFIED`. Phiên 1 mô tả đầy đủ hơn nhưng nội dung đó không có trong repo |
| Peripheral `sx_gpio`, `sx_i2c`, `sx_spi`, `sx_i80`, `sx_i2s` | Hợp đồng và hiện thực ESP32-S3 đã viết, **đã compile trong build đầy đủ (phiên 3), chưa chạy trên phần cứng**. Chưa kiểm chứng link vào firmware vì chưa có code nào gọi chúng |
| PWM, UART, SD card (`sx_sdcard`) | Chưa có |
| Modules (screenHMI, touch, speaker, storage) | Chỉ là file rỗng |
| Services (http, AI) | Chỉ là file rỗng |
| App tasks | Chỉ là file rỗng. `app_start()` và `board_init()` **chưa tồn tại**; `app_main()` rỗng |
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
- Viết `rule_code.md`, `Firmware_Construction.md`, `handover.md`, `RuleAgent.md`.
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

Phiên 3 đã đổi tên hai file trong bản sao của sandbox. Trên máy của chủ dự án cần chạy (Git trên Windows không nhận đổi hoa/thường):

```powershell
git mv -f LoganSDK/services/CmakeLists.txt LoganSDK/services/CMakeLists.txt
git mv LoganSDK/app/user/screen_app/screem_app.c LoganSDK/app/user/screen_app/screen_app.c
```

Chép đè các `CMakeLists.txt` đã present ở phiên 3 (root, `main`, `app`, `board`, `modules`, `peripherals`, `platform/esp32s3` là file mới, `services`, `utils`).

Dọn tùy chọn, chờ chủ dự án duyệt (các file rỗng này không nằm trong `SRCS`, không ảnh hưởng build):
`LoganSDK/components/peripherals/{gpio/sx_gpio.c,i2c/sx_i2c.c,i2s/sx_i2s.c,i80/sx_i80.c,spi/sx_spi.c}` và `LoganSDK/platform/CMakeLists.txt` (rỗng, không phải component).

Danh sách lệnh đổi tên cũ của phiên 1 (đã lỗi thời, 4 file `esp32s3_*.c` đã ở đúng chỗ):

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

Quyết định còn mở:

- Tên thư mục chip: repo dùng `LoganSDK/platform/esp32s3` (số ít) nhưng `Firmware_Construction.md` và một số tài liệu ghi `platforms/`. Phiên 3 giữ theo repo (code). Đề xuất sửa tài liệu cho khớp, chưa sửa vì chưa được duyệt.
- Tên file tài liệu: repo là `HandOver.md`, `RuleCode.md`, `RuleAgent.md`; các tài liệu vẫn nhắc `handover.md`, `rule_code.md`.
- Khôi phục `board.h` đầy đủ (nhãn `REF/DESIGN/GIVEN`, `BOARD_PINMAP_VERIFIED=0`), `board_init()` và `app_start()`; chưa làm vì chưa được yêu cầu.

## 7. Kế hoạch tiếp theo

Thứ tự đề xuất (mỗi bước phải build được trước khi sang bước sau). Bước 0 đã xong ở phiên 3 (CMake nối layer, build OK).

0. (Xong) Nối CMake và build.
0b. Khôi phục `board.h`, `board.c` (`board_init()`), `app.c` (`app_start()`), nối từ `app_main()`.

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

### Phiên 2 (2026-11-04)

Đã làm:

- Hoàn thiện hợp đồng `sx_i2s` cho speaker I2S TX: init, start, stop, write và deinit.
- Hoàn thiện hiện thực ESP32-S3 bằng I2S standard driver của ESP-IDF: Philips slot, mono/stereo,
  8/16/24/32 bit, timeout và ánh xạ lỗi.
- Chạy lệnh build ESP-IDF thành công cho cấu hình hiện tại.

Chưa xác minh:

- Cấu hình build hiện tại chưa đăng ký các component LoganSDK, nên log build không biên dịch
  `esp32s3_i2s.c`; driver chưa được compile/link thực tế.
- Chưa chạy trên phần cứng; chân I2S vẫn chưa xác minh trên revision V15.

Kế hoạch tiếp theo:

- Đăng ký đúng component peripheral/platform trong CMake, build lại để bắt lỗi compile của I2S,
  sau đó mới nối module speaker.

### Phiên 3 (2026-11-04)

Đã làm:

- Clone repo, đọc `HandOver.md`, `RuleCode.md`, `RuleAgent.md`, `Firmware_Construction.md`, `Product_Construction.md`; đối chiếu với repo thật và báo các điểm lệch (xem mục 3 và 6).
- Nối CMake theo bảng phụ thuộc trong `Firmware_Construction.md`: root `CMakeLists.txt` khai báo `EXTRA_COMPONENT_DIRS` cho 7 component (`app`, `board`, `modules`, `peripherals`, `esp32s3`, `services`, `utils`; không gồm `libs/`); viết `CMakeLists.txt` cho `board`, `modules`, `peripherals` (chỉ header), `platform/esp32s3` (mới, `PRIV_REQUIRES driver esp_lcd`), `services`, `utils`; `main` thêm `REQUIRES app`; sửa comment trong `app/CMakeLists.txt` cho đúng đường dẫn `platform/esp32s3`.
- Đổi tên `services/CmakeLists.txt` thành `CMakeLists.txt` và `screem_app.c` thành `screen_app.c` (cần thiết để build; chỉ đổi tên, các file rỗng).
- Dựng ESP-IDF v5.4.3 trong sandbox theo mục 8, chạy `idf.py set-target esp32s3` và `idf.py build` trên bản sao: **build thành công, 0 cảnh báo**. Cả 5 file `esp32s3_*.c` (gồm `esp32s3_i2s.c`), `cqueue.c`, `sx_filter.c`, các file rỗng của modules/services/app đều được biên dịch; `libesp32s3.a` có đủ symbol `sx_gpio_*`, `sx_i2c_*`, `sx_i2s_*`, `sx_i80_*`, `sx_spi_*`.

Chưa làm hoặc chưa kiểm chứng:

- Chưa chạy trên phần cứng, chưa flash. Chưa `set-target` trong repo thật của chủ dự án.
- Chưa kiểm chứng các hàm driver được link vào ELF cuối (chưa có code gọi chúng, linker bỏ qua object không được tham chiếu).
- Chưa thử kiểm tra quy tắc phụ thuộc bằng cách cố ý include ngược chiều.
- Chưa sửa `board.h`, `board.c`, `app.c`, tài liệu (ngoài `HandOver.md`).

Điểm cần lưu ý:

- `peripherals` là component chỉ có header nên không sinh thư viện `.a`; điều này là đúng.