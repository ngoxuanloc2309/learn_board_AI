# Handover - AI Learning Device

> File này do mọi phiên làm việc (Claude hoặc AI khác) cập nhật ở cuối phiên. Quy tắc: xem `RuleCode.md` mục 3.
> Cập nhật lần cuối: 2026-11-08 (phiên 11).

## 1. Đọc trước khi làm

1. `docs/RuleAgent.md` - cách làm việc của AI/agent: quy trình đầu/cuối phiên, nguyên tắc bắt buộc.
2. `docs/RuleCode.md` - quy tắc bàn giao file và viết comment.
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
| Phase 1 trên máy của chủ dự án | Phiên 6: chủ dự án đã `set-target` và `idf.py build` thành công (trước khi có `sx_time`). **Chưa flash.** Chưa build lại trên máy chủ dự án sau commit `aa0d00c` (phiên 7) |
| `board.h` (chân, số cổng) | **Chỉ là stub** (enum `BoardType_t`), chưa có chân nào, chưa có nhãn `REF/DESIGN/GIVEN` hay `BOARD_PINMAP_VERIFIED`. Phiên 1 mô tả đầy đủ hơn nhưng nội dung đó không có trong repo |
| Peripheral `sx_gpio`, `sx_i2c`, `sx_spi`, `sx_i80`, `sx_i2s` | Hợp đồng và hiện thực ESP32-S3 đã viết, **đã compile trong build đầy đủ (phiên 3), chưa chạy trên phần cứng**. Chưa kiểm chứng link vào firmware vì chưa có code nào gọi chúng |
| Peripheral `sx_pwm`, `sx_uart`, ngắt của `sx_gpio` | Viết ở phiên 4. Phiên 5: sửa lỗi compile ở `esp32s3_gpio.c`, **build đầy đủ thành công (0 cảnh báo)**; chưa chạy trên phần cứng |
| Peripheral `sx_time` | Phiên 6: `sx_delay_ms`, `sx_get_time_ms` (`sx_time.h` và `platform/esp32s3/time/esp32s3_time.c`). Đã chạy trên máy host (giả lập FreeRTOS, `esp_timer`) và đã build đầy đủ trong sandbox. Phiên 7: chủ dự án đã đổi tên `esp32c3_time.c` thành `esp32s3_time.c` và push (commit `aa0d00c`), khớp với `platform/esp32s3/CMakeLists.txt`. Chưa chạy trên phần cứng, chưa có ai gọi |
| `sx_i2s` mở rộng (loa TX + mic RX) | Phiên 8: thêm `din_pin` và `sx_i2s_read`; quy ước `-1` (`SX_I2S_PIN_UNUSED`) là không dùng hướng đó; `dout_pin >= 0` tạo kênh TX, `din_pin >= 0` tạo kênh RX, cả hai trên một cổng là song công. Sửa lỗi timeout cũ (xem nhật ký phiên 8). **Đã chạy trên máy host** với driver I2S giả lập (ASan, UBSan, 0 lỗi, 6 đột biến đều bị bắt) và biên dịch sạch cảnh báo nghiêm ngặt trên host. **Chưa build bằng ESP-IDF, chưa chạy trên phần cứng.** Chỉ module `inmp441` gọi `sx_i2s` (phía RX); loa chưa có module |
| Module `inmp441` (`components/modules/micro`) | Phiên 8: mic I2S MEMS INMP441 trên `sx_i2s`, chỉ nhận, mono, slot 32 bit, trả PCM 16 bit có dấu (16 bit cao của mẫu 24 bit). API: `inmp441_init/deinit/start/stop/read`. **Đã chạy trên máy host** (`sx_i2s` giả lập, ASan, UBSan, 0 lỗi) và biên dịch riêng file này sạch cảnh báo nghiêm ngặt, kể cả bằng GCC 14.2 cho ESP32-S3 (422 byte code). **Chưa build trong dự án, chưa chạy trên phần cứng**, được `app/user/wifi_app` gọi trong `app_start()` (cuối phiên 11), chưa build |
| Module `sdcard` (`components/modules/sdcard`) | Phiên 5: block device SD-SPI trên `sx_spi`. **Đã build trên target esp32s3 (0 cảnh báo) và đã chạy thử trên máy host với thẻ SD giả lập** (SDHC, SDSC, MMC, không có thẻ, cấu hình sai); **chưa thử với thẻ thật**. Chưa có hệ thống tệp (FatFs) |
| Module `speaker` (`components/modules/speaker`) | Phiên 10: loa I2S TX trên `sx_i2s`, chỉ phát. Người gọi đưa vào PCM mono 16 bit; module gửi cùng mẫu ra cả slot trái và phải (frame stereo 16 bit) vì chân chọn kênh của amp chưa xác minh. Volume tuyến tính 0 đến 100 phần trăm (mặc định 50, chọn theo thiết kế, chưa đo). API: `speaker_init/deinit/start/stop/set_volume/write`. **Đã chạy trên máy host** (`sx_i2s` giả lập, ASan, UBSan, biên dịch sạch `-Wall -Wextra -Wshadow -Wconversion -Wsign-conversion -Wpedantic -Werror`, 8 đột biến đều bị bắt). **Chưa `idf.py build`, chưa biên dịch bằng GCC cho ESP32-S3, chưa chạy trên phần cứng**, chưa có ai gọi (chưa có `board_init()`). Thư mục `modules/storage` không còn: storage đã chuyển lên `services/` (xem dòng dưới) |
| Module `screenHMI` (`modules/screenHMI`) | Phiên 6: ST7796 qua `sx_i80`, reset qua `sx_gpio`, backlight PWM qua `sx_pwm`. Chỉ gửi lệnh MIPI DCS chuẩn; bảng lệnh riêng của panel do board truyền vào. **Đã chạy trên máy host** (hợp đồng `sx_*` giả lập) và biên dịch sạch bằng GCC 14.2 cho ESP32-S3. Đã build trong dự án ở sandbox (phiên 6, không có cảnh báo biên dịch từ code dự án); **chưa chạy trên phần cứng** |
| Module `touch` (`modules/touch`) | Phiên 6: FT6x36/FT5x06 (FT6336U) qua `sx_i2c`, chỉ đọc thanh ghi, dùng polling (không dùng chân INT). Kiểm chứng giống `screenHMI`. Đã build trong dự án ở sandbox (phiên 6, không có cảnh báo biên dịch từ code dự án); **chưa chạy trên phần cứng** |
| Peripheral `sx_wifi` (`peripherals/wifi`, `platform/esp32s3/wifi`) | Phiên 11: hợp đồng và hiện thực Wi-Fi cho STA, AP hoặc APSTA cùng lúc. AP có IP tĩnh do config đặt (đã chốt `192.168.4.1`, netmask 255.255.255.0, gateway = IP của AP, DHCP server của chip), STA dùng DHCP, tự thử nối lại sau mỗi lần mất kết nối. API: `sx_wifi_init/deinit/start/stop/wait_ip/get_status` kèm callback sự kiện. **Đã chạy trên máy host** (ESP-IDF giả lập, ASan, UBSan, 121 check, biên dịch sạch `-Wall -Wextra -Wshadow -Wconversion -Wsign-conversion -Wpedantic -Werror`, 8 đột biến đều bị bắt). **Chưa `idf.py build`, chưa chạy với router thật, chưa chạy trên phần cứng**, chưa có ai gọi (chưa có `board_init()`) |
| Service `http` (`services/http`) | Phiên 11: client HTTP/HTTPS chặn (blocking), một yêu cầu một lần gọi: `http_svc_perform`. Dùng `esp_http_client`; HTTPS luôn kiểm chứng chứng chỉ (CA bundle của firmware, hoặc PEM do người gọi đưa), không có cách tắt. Không theo redirect. **Đã chạy trên máy host** (`esp_http_client` giả lập, ASan, UBSan, 61 check, biên dịch sạch cờ nghiêm ngặt, 6 đột biến đều bị bắt). **Chưa `idf.py build`, chưa gọi server thật** |
| Service `AI` (`AI_backend`, `AI_frontend`) | Chỉ là file rỗng. Chủ dự án để phần backend AI lại sau (chưa có đặc tả backend: URL, endpoint, JSON, xác thực) |
| Service `storage` (`services/storage`) | Phiên 6: API file trên thẻ SD (mount, open/read/write/seek, list, mkdir, remove, rename, đọc/ghi cả file), nối FatFs vào `sdcard` qua `storage_diskio.c`. **Đã chạy trên máy host** (FatFs thật, thẻ SD giả lập bằng RAM, ASan và UBSan, 0 lỗi) và biên dịch sạch với `-Wall -Wextra -Wshadow -Wconversion -Wsign-conversion -Wpedantic`. Chủ dự án đã chạy `idf.py build` thành công cho esp32s3 (phiên 6). Chưa thử với thẻ thật, chưa chạy trên phần cứng. Lưu ý: `app_init()` chưa được ai gọi nên linker có thể bỏ code storage, kích thước bin chưa phản ánh chi phí thật của FatFs Chưa có `board_init()` nên chưa có code gọi `storage_init` |
| `libs/` (FatFs R0.16 của ChaN, cJSON) | Phiên 6: `libs/CMakeLists.txt` đăng ký component `libs` chỉ build FatFs (`ff.c`, `ffsystem.c`, `ffunicode.c`). cJSON nằm trong thư mục nhưng **chưa build**. Không dùng component `fatfs` của ESP-IDF |
| App tasks | Chỉ là file rỗng. `app_start()` và `board_init()` **chưa tồn tại**; `app_main()` rỗng |
| LVGL, OTA | Chưa bắt đầu |

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
- Các thời gian chờ trong `screenHMI` và `touch` (reset thấp 10 ms, chờ sau reset 120 ms, chờ sau SWRESET và SLPOUT 120 ms, chờ boot touch 300 ms): lấy theo thông lệ, chưa đối chiếu datasheet.
- Bảng lệnh riêng của panel (nguồn, gamma, mở khóa lệnh mở rộng): chưa viết, cần nguồn đã xác minh cho đúng panel của board. Không có bảng này màn hình chạy với thiết lập mặc định của chip.
- Giá trị MADCTL cho từng hướng, có đảo màu (INVON) hay không, thứ tự RGB/BGR: chưa xác minh trên board.
- Bản đồ thanh ghi và ý nghĩa cờ sự kiện của touch (TD_STATUS 0x02, điểm đầu từ 0x03, ID 0xA3/0xA6/0xA8): theo bản đồ FocalTech thông dụng, chưa đối chiếu datasheet của chip trên board. Cực tính và chế độ chân INT chưa xác minh nên chưa dùng.

- Cấu hình `sdkconfig` đang commit: flash `2MB`, `CONFIG_SPIRAM` chưa bật, `CONFIG_FREERTOS_HZ=100`. Đây là giá trị đang có trong file, **không phải** thông số đã xác minh của board (phiên 7 chỉ đọc file, chưa đối chiếu phần cứng).

- Mic: file `LoganSDK/board/board_device/ZX3D50CE08S-V15-USRC.h` (commit `d134462`) gán `I2S_MIC_BCLK/WS/DIN` vào `EXT_IO1/2/3` (GPIO 10, 11, 12). Đây là lựa chọn của chủ dự án. Chủ dự án chốt dùng mic INMP441, chân L/R nối xuống GND qua trở 10K do chủ dự án tự thêm (GIVEN, theo lời chủ dự án, chưa kiểm tra bằng phần cứng). Theo cách hiểu của phiên 8 từ datasheet INMP441 (chưa đối chiếu lại): L/R mức thấp cho mic ra kênh trái, khớp `slot_mask` trái của mono; mẫu 24 bit trong khung 32 bit nên cần `bits_per_sample = 32`; nguồn 1.8 đến 3.3 V, header mở rộng chỉ có +5V và GND nên cần nguồn 3.3V riêng (**chưa xác minh** cho V15). Nối dây: SCK vào GPIO 10, WS vào GPIO 11, SD (DOUT của mic) vào GPIO 12 (DIN của ESP32). Header này chưa được include ở đâu, dùng macro `GPIO_xx` chưa định nghĩa, chưa có nhãn `REF/DESIGN/GIVEN`, và có ký tự CJK trong comment (trái `RuleCode.md`, chưa sửa vì là code của chủ dự án).
- `docs/ZX3D50CE08S-USRC-4832.md` (commit `17f1a80`) là datasheet dòng SC01 Plus; ảnh PCB trong đó ghi V13 220511, không phải V15. Nó nêu module `WT32-S3-WROVER-N16R2`, trong khi `sdkconfig` đang flash 2MB và chưa bật SPIRAM. Chưa xác minh cho V15.

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

### Phiên 4 (2026-11-04)

Đã làm:

- Thêm hợp đồng và hiện thực ESP32-S3 cho `sx_pwm` bằng LEDC, `sx_sdcard` bằng SPI + FATFS,
  và `sx_uart` cho UART thường hoặc RS485 half-duplex.
- Thêm đăng ký/gỡ callback ISR cho `sx_gpio`, phục vụ chân INT của touch.
- Nối các header và driver mới vào CMake của component `peripherals` và `platform/esp32s3`.
- Cập nhật tài liệu kiến trúc; không đưa pinout V15 vào code vì chưa được xác minh.

Chưa kiểm chứng:

- Máy hiện tại không có `idf.py` hoặc `IDF_PATH`, nên chưa build lại sau các thay đổi phiên 4.
- Chưa chạy trên phần cứng; chân PWM, SD và RS485 vẫn chưa xác minh.
- `screem_app.c` vẫn chưa được đổi tên trong repo thực tế dù handover phiên 3 ghi đã đổi.

Kế hoạch tiếp theo:

- Chạy `idf.py set-target esp32s3` và `idf.py build` trong môi trường ESP-IDF 5.4.3.
- Sau khi build đạt, nối các module `screenHMI`, `storage`, `speaker` và dùng cấu hình board được xác minh.
4. Wi-Fi.
5. Phase 3 (LVGL, màn hình) trở đi theo roadmap trong Product_Construction.md.

### Phiên 5 (2026-11-05)

Đã làm:

- Pull repo, đối chiếu với handover: phát hiện hai lệnh `git mv` của phiên 3 chưa vào git và `esp32s3_gpio.c` (phiên 4) không compile được, vì code ngắt bị chèn vào trong thân `sx_gpio_write`.
- Sửa `esp32s3_gpio.c`: tách `interrupt_type`, `sx_gpio_set_interrupt`, `sx_gpio_remove_interrupt` ra khỏi thân `sx_gpio_write`, không đổi logic.
- Theo yêu cầu của chủ dự án, chuyển thẻ SD từ peripheral lên module: bỏ `sx_sdcard.h` và `esp32s3_sdcard.c`, viết mới `components/modules/sdcard/sdcard.h` và `sdcard.c`.
  Module chỉ dùng `sx_spi`: nhận dạng thẻ ở xung nhịp chậm (400 kHz, tối đa theo chuẩn), đọc dung lượng từ CSD (v1 và v2),
  rồi tháo và thêm lại thiết bị SPI ở xung nhịp dữ liệu (tối đa 25 MHz); đọc/ghi từng khối 512 byte (CMD17, CMD24). Hỗ trợ SDSC, SDHC, SDXC; từ chối MMC.
- Sửa CMake: `peripherals` bỏ thư mục `sdcard`; `platform/esp32s3` bỏ `esp32s3_sdcard.c` và `PRIV_REQUIRES fatfs sdmmc`; `modules` thêm `sdcard/sdcard.c`.
- Sửa `Firmware_Construction.md` (danh sách module, bảng phụ thuộc, bảng trạng thái peripheral, ghi chú thẻ SD không phải peripheral).

Đã kiểm chứng:

- `idf.py set-target esp32s3` và `idf.py build` trên bản sao (ESP-IDF v5.4.3), sau khi áp tạm hai đổi tên ở mục 6: **build thành công, 0 cảnh báo**; `libmodules.a` có đủ symbol `sdcard_*`.
- `sdcard.c` biên dịch sạch với `-Wall -Wextra -Wshadow -Wconversion -Wsign-conversion -Wpedantic` trên host.
- Chạy `sdcard.c` thật trên máy host (ASan và UBSan) với `sx_spi` giả lập và thẻ SD giả lập: khởi tạo SDHC (1024 khối) và SDSC địa chỉ byte (4096 khối), đọc/ghi hai khối rồi đọc lại, từ chối khối ngoài phạm vi, bỏ qua `count == 0`, MMC trả `SDCARD_ERR_UNSUPPORTED`, không có thẻ trả `SDCARD_ERR_NO_CARD`, cấu hình sai trả `SDCARD_ERR_PARAM`, đúng thứ tự xung nhịp 400 kHz rồi 20 MHz.
  Bài test này nằm ngoài repo (không commit).

Chưa kiểm chứng:

- Chưa thử với thẻ SD thật và chưa chạy trên phần cứng. Giả lập chỉ chứng minh logic khớp mô hình thẻ do chính phiên này viết, không thay thế thử trên thẻ thật (nhiều hãng, thẻ cũ có thể khác).
- Timeout đếm theo số lần poll vì module không có nguồn thời gian; thời gian thật phụ thuộc tốc độ gọi SPI. Khi không có thẻ, `sdcard_init` có thể chặn tới vài giây.
- Chỉ ghi/đọc từng khối; chưa dùng CMD18/CMD25 (nhiều khối) nên tốc độ chưa tối ưu.
- Chân SD, host SPI, nối dây (kéo lên MISO, cấp nguồn thẻ) chưa xác minh trên V15 và chưa có trong `board.h`. Chưa thử thẻ dùng chung bus SPI với thiết bị khác.
- Chưa có hệ thống tệp; chưa có `board_init()` tạo instance `sdcard_t`.

Kế hoạch tiếp theo:

- Chủ dự án quyết định có nối FatFs hay không (xem mục 6), rồi viết module `storage` đặt trên `sdcard`.
- Khôi phục `board.h`, `board_init()`, `app_start()` (bước 0b mục 7), khi đó `board.c` sở hữu `sdcard_t`, `sx_spi_config_t` và chân CS của thẻ.
- Bring-up thẻ SD thật khi có xác minh chân.

### Phiên 6 (2026-11-05)

Đã làm:

- Clone repo, đọc `HandOver.md`, `RuleAgent.md`, `RuleCode.md`, `Firmware_Construction.md`, `Product_Construction.md`, đối chiếu với repo. Phát hiện lệch: tên file docs (`HandOver.md`... thay vì `handover.md`...), `libs/` đã có cJSON và FatFs (handover ghi trống), `app.c` có `app_init()` (không phải `app_start()`), `modules/CMakeLists.txt` còn trỏ `storage/storage.c` khi thư mục đã chuyển sang `services/`.
- Hoàn thành service `storage` mà phiên trước viết dở (phiên đó dừng sau khi có `storage.h`, `storage_diskio.h`, `storage_diskio.c`; `storage.c` chưa có). File trong `LoganSDK/services/storage/`:
  - `storage.h` (sửa nhẹ so với bản dở: thêm `STORAGE_ERR_FULL` khi không ghi được byte nào, ghi chú FatFs không có khóa file, ghi chú `card_config` NULL).
  - `storage.c` (mới viết): mount/unmount (tự bring-up thẻ khi `card_config` có giá trị, tùy chọn format khi thẻ chưa có FAT), bảng file handle tĩnh (mặc định 4), ánh xạ `FRESULT` sang mã `STORAGE_ERR_*`, đọc/ghi cả file, list/stat/mkdir/remove/rename, `storage_get_space`.
  - `storage_diskio.h`, `storage_diskio.c`: nối `disk_*` của FatFs xuống `sdcard_*`. Chỉ sửa một dòng ép kiểu (`DSTATUS`) so với bản dở để hết cảnh báo `-Wconversion`.
- Sửa CMake: `libs/CMakeLists.txt` (mới), `services/CMakeLists.txt` (đổi tên đúng hoa/thường, thêm nguồn storage, `REQUIRES modules utils`, `PRIV_REQUIRES libs`), `modules/CMakeLists.txt` (bỏ storage), `app/CMakeLists.txt` (thêm `storage_app`), `CMakeLists.txt` gốc (thêm `libs`).

Đã kiểm chứng:

- Chạy trên máy host (ASan và UBSan) `storage.c` + `storage_diskio.c` + FatFs R0.16 thật với `sdcard_*` giả lập bằng RAM (8192 khối): không có thẻ, thẻ trống không format (`STORAGE_ERR_NO_FS`, thẻ được trả lại), format rồi mount, đọc/ghi file nhỏ và file 100 KB nhiều cluster, ghi đè và thu nhỏ file, tên file dài và không phân biệt hoa thường, append, seek, kiểm tra đường dẫn sai, giới hạn 4 file mở, thư mục lồng nhau, list (đếm, dừng giữa chừng), stat, xóa thư mục không rỗng, đổi tên (kể cả đích đã tồn tại), unmount rồi mount lại và dữ liệu còn nguyên (không format lại), thẻ do chủ khác khởi tạo thì storage không giải phóng, thẻ đầy trả `STORAGE_ERR_FULL` và ghi lại được sau khi xóa. 0 lỗi.
- Biên dịch sạch `storage.c`, `storage_diskio.c` với `-Wall -Wextra -Wshadow -Wconversion -Wsign-conversion -Wpedantic` trên host. Bài test nằm ngoài repo (không commit).
- Dựng ESP-IDF v5.4.3 trong sandbox (theo mục 8) và chạy `idf.py set-target esp32s3` trên bản sao: **cấu hình CMake thành công** với bố cục component mới (có `libs`). Cần áp tạm hai đổi tên ở mục 6 để chạy trên Linux.
- Chủ dự án chạy `idf.py build` trên Windows (ESP-IDF v5.4.3, GCC 14.2.0, target esp32s3): **build thành công**. `board_hmi_ai.bin` 0x2e050 byte, phân vùng app nhỏ nhất 0x100000 byte, còn trống 82%; bootloader 0x51c0 byte. Hai lỗi gặp trên đường: (1) `CMakeLists.txt` gốc chưa có `libs` (chưa chép đè), (2) shell có biến môi trường `IDF_TARGET=esp32` nên `set-target esp32s3` báo lệch; lệnh `idf.py set-target esp32-s3` chạy được. Đổi tên `screem_app.c` sang `screen_app.c` đã làm xong.

Chưa làm hoặc chưa kiểm chứng:

- Chưa có log cảnh báo đầy đủ của lần build thành công (chủ dự án chỉ gửi phần cuối), nên chưa biết `ff.c`, `ffunicode.c` có cảnh báo không. Chưa kiểm tra tường minh việc không xung đột với component `fatfs` của ESP-IDF, nhưng link đã thành công. Chưa đo chi phí Flash và RAM của FatFs vì code storage chưa được gọi từ `main`.
- Chưa thử với thẻ SD thật, chưa chạy trên phần cứng. Giả lập chỉ chứng minh logic khớp mô hình do chính phiên này viết.
- `storage` chưa có chủ sở hữu: chưa có `board_init()` tạo `sdcard_t`, `sdcard_config_t`, `storage_config_t` (và cấu hình SPI/chân CS, vẫn chưa xác minh). `storage_app.c` và `app_storage_init()` vẫn rỗng.
- Không thread-safe (FatFs build với `FF_FS_REENTRANT=0`, một buffer tên dùng chung): phải gọi từ một task duy nhất (storage task). Không có RTC (`FF_FS_NORTC=1`) nên mọi file có cùng dấu thời gian cố định. Ghi cả file không atomic (mất điện giữa chừng có thể để lại file dở). Chỉ truy cập từng khối 512 byte (xem hạn chế của `sdcard`), nên tốc độ chưa tối ưu.
- `ffconf.h` do chủ dự án cấu hình (code page 437, `FF_USE_MKFS=1`, `FF_USE_STRFUNC=1`, LFN bật); tên file có ký tự ngoài code page 437 sẽ không dùng được.
- Các điểm lệch tài liệu còn mở: tên file docs (`HandOver.md`... so với `handover.md`...) và `platform/` so với `platforms/`.

Kế hoạch tiếp theo:

- Xem lại cảnh báo của FatFs trong log build đầy đủ, và đo lại kích thước sau khi `app_init()` được gọi từ `main`.
- Khôi phục `board.h`, `board_init()`, `app_start()` (bước 0b mục 7): khi đó `board.c` sở hữu `sdcard_t`, `sdcard_config_t` và `storage_config_t`.
- Viết `storage_app` (task duy nhất gọi `storage_*`), sau khi chủ dự án quyết định mức ưu tiên và stack.
- Bring-up thẻ SD thật khi có xác minh chân.

Phiên 6 (tiếp, cùng ngày): module `screenHMI` và `touch`.

Đã làm:

- `LoganSDK/components/modules/screenHMI/screenHMI.h` và `screenHMI.c` (file mới, trước đó rỗng). API: `screen_hmi_init`, `screen_hmi_deinit`, `screen_hmi_set_display_on`, `screen_hmi_set_backlight` (0 đến 100 phần trăm, hỗ trợ backlight tích cực thấp), `screen_hmi_draw_bitmap` (bất đồng bộ, vùng chữ nhật tọa độ cuối bao gồm, báo hoàn thành qua callback có thể chạy trong ngắt), `screen_hmi_fill_rect` (chặn, dùng để bring-up, buffer tĩnh 1920 pixel). Chuỗi khởi tạo: reset cứng (nếu có), SWRESET, SLPOUT, bảng lệnh của board, COLMOD RGB565, MADCTL, INVON hoặc INVOFF, DISPON. Module giữ bản sao cấu hình bus I80 và thay `on_color_done`, nên `screen_hmi_t` không được di chuyển khi đang dùng.
- `LoganSDK/components/modules/touch/touch.h` và `touch.c` (file mới, trước đó rỗng). API: `touch_init` (tạo bus I2C, reset nếu có, chờ boot, kiểm tra địa chỉ), `touch_deinit`, `touch_read` (tối đa 2 điểm, đổi trục, lật, cắt biên), `touch_read_id`, `touch_recover`. Chỉ đọc thanh ghi, không ghi vào chip.
- Không đưa số GPIO hay thông số board vào module: mọi thứ qua cấu hình của board layer (module không include `board.h`).
- Không sửa CMake: `modules/CMakeLists.txt` đã có sẵn hai nguồn này và `REQUIRES peripherals utils`.

Đã kiểm chứng:

- Chạy trên máy host (ASan và UBSan) với bản giả lập `sx_i80`, `sx_gpio`, `sx_pwm`, `sx_i2c`: thứ tự và dữ liệu lệnh của chuỗi khởi tạo, thời gian chờ, cửa sổ CASET và RASET (kể cả tọa độ trên 255), kiểm tra tham số, callback hoàn thành được chuyển tiếp và không bị chuyển tiếp trong lúc `fill_rect`, `fill_rect` chia khối đúng, timeout, dọn tài nguyên khi khởi tạo lỗi giữa chừng, backlight tích cực cao và thấp, phân tích khung touch (tọa độ 12 bit, ID, sự kiện lift up và none, khung không hợp lệ, giới hạn bộ đệm), các phép biến đổi tọa độ, reset và địa chỉ tùy chọn, lỗi bus. 0 lỗi. Thử đột biến 5 lỗi cố ý, test bắt được cả 5.
- Biên dịch sạch với GCC 14.2 cho ESP32-S3 (`-Wall -Wextra -Wshadow -Wconversion -Wsign-conversion -Wpedantic`). Code: `screenHMI` 1272 byte, `touch` 659 byte, không có dữ liệu tĩnh trong module (`screen_hmi_t` chứa buffer fill khoảng 3,8 KiB, do board cấp phát).
- Bài test và bản giả lập nằm ngoài repo (không commit). Giả lập chỉ chứng minh logic khớp mô hình do chính phiên này viết.

Chưa làm hoặc chưa kiểm chứng:

- Chưa `idf.py build` với hai module mới trong dự án, chưa chạy trên phần cứng. Màn hình và touch chưa từng được thử với board thật.
- Chưa có `board.h`, `board_init()` nên chưa ai tạo cấu hình cho hai module (chân, tốc độ pixel clock, bảng lệnh panel, MADCTL, hướng touch). Mọi giá trị trên chưa xác minh (xem mục 4).
- Module không có nguồn thời gian riêng, nên cần callback `delay_ms` do board cung cấp. Đã giải quyết: chủ dự án thêm hợp đồng `sx_time` (`sx_delay_ms`), board truyền thẳng `sx_delay_ms` làm callback (cùng chữ ký `void (*)(uint32_t)`). Xem mục sx_time bên dưới.
- `screen_hmi_fill_rect` dùng buffer tĩnh nội bộ làm nguồn DMA, chưa kiểm tra yêu cầu căn lề hay vùng nhớ của driver `esp_lcd` trên ESP32-S3 (buffer này nằm trong RAM trong). `screen_hmi_draw_bitmap` yêu cầu buffer do người gọi cấp phát phù hợp DMA.
- Touch dùng polling, nên chưa tận dụng chân INT. Cần xác minh cực tính và chế độ của INT khi bring-up nếu muốn tiết kiệm I2C.
- Chưa có tích hợp LVGL (flush callback, đọc input) và chưa chia sẻ khóa giữa `screen_hmi` và `touch`.

Kế hoạch tiếp theo:

- Khôi phục `board.h`, `board_init()`, `app_start()` và cấu hình cho `screenHMI`, `touch`, `sdcard`, `storage` (truyền `sx_delay_ms` làm `delay_ms`).
- Viết `speaker`.
- Bring-up: hiển thị màu bằng `screen_hmi_fill_rect`, đọc `touch_read_id` và `touch_read`, chỉnh MADCTL, đảo màu, bảng lệnh panel và hướng touch theo màn hình thật.

Phiên 6 (tiếp, cùng ngày): hoàn thành `sx_time`.

Đã làm (chủ dự án đã push `sx_time.h` với hai chữ ký `sx_delay_ms`, `sx_get_time_ms`; file hiện thực còn rỗng và tên nhầm chip):

- `LoganSDK/components/peripherals/time/sx_time.h`: giữ nguyên hai chữ ký. Sửa include guard (đang là `SX_SPI_H`, trùng với `sx_spi.h`, include cả hai header sẽ làm một header bị bỏ qua mà không báo lỗi) thành `SX_TIME_H`. Thêm mô tả hợp đồng: không gọi trong ngắt, chờ ít nhất ms mili giây và dài hơn dưới hai tick, bộ đếm quay vòng sau khoảng 49,7 ngày (đo khoảng bằng phép trừ số không dấu), con trỏ NULL thì không làm gì.
- `LoganSDK/platform/esp32s3/time/esp32s3_time.c`: đổi tên từ `esp32c3_time.c` (`git mv`, nhầm chip) và viết nội dung. `sx_delay_ms`: chờ ngắn hơn hai tick (20 ms ở FreeRTOS 100 Hz của dự án), hoặc khi scheduler chưa chạy, dùng busy wait `esp_rom_delay_us` (chính xác, chia bước tối đa 1 giây); chờ dài dùng `vTaskDelay(ms / tick + 2)` để không bao giờ ngắn hơn ms. `sx_get_time_ms`: `esp_timer_get_time()` chia 1000, giữ 32 bit thấp.
- `LoganSDK/components/peripherals/CMakeLists.txt`: thêm `time` vào `INCLUDE_DIRS`.
- `LoganSDK/platform/esp32s3/CMakeLists.txt`: thêm `time/esp32s3_time.c` vào nguồn, thêm `esp_timer esp_rom freertos` vào `PRIV_REQUIRES`.
- `docs/Firmware_Construction.md`: thêm dòng `sx_time` vào bảng trạng thái peripheral (mục 8) và vào sơ đồ layer.

Đã kiểm chứng:

- Chạy trên máy host (ASan và UBSan) với bản giả lập FreeRTOS, `esp_timer`, `esp_rom_delay_us`, ở tick 100 Hz và 1000 Hz: các nhánh chờ ngắn, chờ dài, chưa có scheduler, chia bước, đảm bảo "không ngắn hơn ms và dài hơn dưới hai tick" cho mọi ms từ 0 đến 20000, bộ đếm quay vòng và đo khoảng qua điểm quay vòng, con trỏ NULL. 0 lỗi, đột biến cố ý (bỏ phần cộng 2) bị bắt. Test này bắt được một lỗi thật: thiếu `#include <stddef.h>` cho `NULL`, đã sửa.
- `idf.py build` đầy đủ trong sandbox (ESP-IDF v5.4.3, GCC 14.2.0, target esp32s3) trên bản sao của repo (commit `01e23fd` cộng thay đổi của phiên này): **build thành công**, không có cảnh báo biên dịch từ code dự án. Đây là lần build đầu tiên có cả `screenHMI`, `touch`, `storage` và `sx_time`. `esp32s3_time.c.obj` có được biên dịch.

Chưa làm hoặc chưa kiểm chứng:

- Chưa chạy trên phần cứng, chưa đo độ chính xác thật của `sx_delay_ms` và `sx_get_time_ms`.
- Chưa có ai gọi `sx_time`, nên linker có thể loại bỏ nó khỏi bin; kích thước bin chưa phản ánh chi phí thật.
- Busy wait giữ CPU: `screen_hmi_fill_rect` chờ theo bước 1 ms bằng busy wait. Chấp nhận được cho bring-up, không dành cho đường chạy chính.
- `sdcard` vẫn đếm timeout theo số lần poll (không dùng `sx_time`); có thể đổi sang thời gian thật sau, chưa làm.

### Phiên 7 (2026-11-06)

Đã làm:

- Clone repo (HEAD `e4597ee`), đọc `HandOver.md`, `RuleCode.md`, `RuleAgent.md`, `Firmware_Construction.md`, `Product_Construction.md`, đối chiếu với repo.
- Phát hiện lệch: `esp32c3_time.c` chưa được đổi tên trong git trong khi `platform/esp32s3/CMakeLists.txt` trỏ tới `time/esp32s3_time.c` (theo cách đọc CMake, build sẽ lỗi thiếu file; chưa kiểm chứng bằng build). Tên file docs (`HandOver.md`, `RuleCode.md`) khác với tên mà các tài liệu nhắc (`handover.md`, `rule_code.md`). Mục 6 của handover còn nhiều lệnh đã lỗi thời.
- Chủ dự án tự đổi tên `esp32c3_time.c` thành `esp32s3_time.c` và push (commit `aa0d00c`). Đã `git pull` và xác nhận: đổi tên 100% (nội dung giữ nguyên), tên file khớp với CMake.
- Cập nhật `docs/HandOver.md` (file duy nhất đổi trong phiên này, chưa có file code nào đổi):
  - Mục 6: xóa các lệnh `git mv` / `git rm` và các mục chờ quyết định đã xong (đã đối chiếu với repo ở commit `aa0d00c`: `services/CMakeLists.txt`, `screen_app.c`, `esp32s3_time.c` đúng tên, `sx_sdcard.h` và `esp32s3_sdcard.c` đã xóa, `libs` đã nối, `storage_app` đã nằm trong CMake của `app`).
  - Mục 3: thêm dòng `sx_time`, sửa trạng thái Phase 1, `screenHMI` và `touch` cho khớp nhật ký phiên 6 (đã build trong sandbox).
  - Mục 4: ghi lại giá trị `sdkconfig` đang có (chưa xác minh).
  - Mục 1 và đầu file: sửa tên file docs cho khớp repo.
  - Mục 5: gom nhật ký phiên 1 đến 7 về đúng một chỗ theo thứ tự. Trước đó phiên 4 nằm trong mục 7 và phiên 2, 3, 5, 6 nằm trong mục 8. Nội dung các phiên cũ giữ nguyên, không sửa.

Đã kiểm chứng:

- Chỉ đọc file và kiểm tra git (`git pull`, danh sách file, đọc CMake). **Không build, không chạy** trong phiên này (sandbox không có ESP-IDF).

Chưa làm hoặc chưa kiểm chứng:

- Chưa build lại sau commit `aa0d00c`, chưa chạy trên phần cứng.
- Phiên này chưa có nhiệm vụ code nào được giao khi cập nhật handover; nếu có việc làm thêm sau mục này, phiên sau cần xem commit mới.

Kế hoạch tiếp theo: xem mục 7.

### Phiên 8 (2026-11-07)

Đã làm:

- Clone repo (HEAD `d134462`), đọc `HandOver.md`, `RuleCode.md`, `RuleAgent.md`, `Firmware_Construction.md`, `Product_Construction.md`, đối chiếu repo. Lệch phát hiện: 3 commit sau phiên 7 (`837bead` thêm `board_device/ZX3D50CE08S-V15-USRC.h`, `17f1a80` thêm datasheet SC01 Plus, `d134462` thêm chân mic I2S) chưa có trong handover; đã ghi ở mục 4.
- Một log của phiên khác (đính kèm bởi chủ dự án) mô tả việc mở rộng `sx_i2s`, nhưng các thay đổi đó **không có trong repo** (`sx_i2s.h` chưa có `din_pin`, `esp32s3_i2s.c` chưa có kênh RX). Đã viết lại từ file thật theo thiết kế trong log, không dùng lại code trong log.
- `LoganSDK/components/peripherals/i2s/sx_i2s.h` (sửa): thêm `din_pin`, `SX_I2S_PIN_UNUSED` (-1), `sx_i2s_read`, mô tả hợp đồng. Thêm trường `din_pin` vào struct: ai khởi tạo struct phải đặt `-1` cho hướng không dùng (hiện chưa có nơi nào gọi `sx_i2s`).
- `LoganSDK/platform/esp32s3/i2s/esp32s3_i2s.c` (sửa, viết lại): tạo kênh TX khi `dout_pin >= 0`, kênh RX khi `din_pin >= 0` (một lần gọi `i2s_new_channel`); hai khóa riêng cho TX và RX để đọc và ghi không chặn nhau, các lệnh `init/deinit/start/stop` lấy cả hai khóa (TX trước, RX sau); khóa tạo một lần ở lần init đầu của cổng; kiểm tra `used` sau khi lấy khóa; `start` lỗi ở RX thì tắt lại TX; `stop` và `deinit` thử cả hai hướng, kênh đã tắt (`ESP_ERR_INVALID_STATE`) coi như đã dừng nên gọi lại được sau lỗi một phần; handle chỉ xóa khi `i2s_del_channel` thành công.
- Sửa lỗi cũ: code phiên 2 đổi `timeout_ms` sang tick rồi truyền cho `i2s_channel_write`, trong khi driver ESP-IDF v5.4.3 nhận mili giây và tự gọi `pdMS_TO_TICKS` (đối chiếu `i2s_common.h` và `i2s_common.c` của v5.4.3). Hệ quả cũ: timeout ngắn đi 10 lần ở tick 100 Hz. Nay truyền thẳng ms. `UINT32_MAX` không còn là `portMAX_DELAY` mà thành khoảng 49 ngày (coi như chờ vô hạn).

Đã kiểm chứng:

- Chạy trên máy host (ASan và UBSan, `-fno-sanitize-recover`) với driver I2S, FreeRTOS semaphore giả lập: kiểm tra cấu hình, loa chỉ TX, mic chỉ RX trên cổng 1, song công (cấu hình clock và slot giống nhau ở hai kênh, đọc trong lúc ghi và ngược lại không chờ nhau), thứ tự bật tắt, lỗi khởi tạo ở từng bước không để lại kênh, `start` lỗi ở RX, `stop` và `deinit` lỗi một phần rồi thử lại, ánh xạ mã lỗi, mọi đường lỗi đều nhả khóa. 0 lỗi. 6 đột biến cố ý (quy đổi timeout lại, luôn tạo RX, đọc và ghi dùng chung khóa, bỏ dung sai kênh đã tắt, bỏ rollback TX, đặt `started` sai) đều bị test bắt. Bài test và bản giả lập nằm ngoài repo (không commit); giả lập chỉ chứng minh logic khớp mô hình do chính phiên này viết.
- Biên dịch sạch `esp32s3_i2s.c` với `-Wall -Wextra -Wshadow -Wconversion -Wsign-conversion -Wpedantic` trên host (với header giả lập).

Chưa làm hoặc chưa kiểm chứng:

- **Chưa chạy `idf.py build`** (chủ dự án tự build). ESP-IDF v5.4.3 đã dựng trong sandbox nhưng phiên này không build. Header driver I2S thật chưa được dùng để biên dịch file này; chỉ đối chiếu `i2s_common.h`, `i2s_common.c`, `i2s_std.h` bằng cách đọc.
- Chưa chạy trên phần cứng. Mic chưa xác minh loại, độ rộng mẫu, kênh trái hay phải (xem mục 4). `sx_i2s` vẫn chưa có người gọi, nên linker có thể bỏ nó.
- Chưa kiểm chứng hành vi song công thật của driver ESP-IDF (hai kênh trên một cổng với cùng cấu hình chân) và hành vi của `i2s_del_channel` ở mọi trạng thái; chỉ dựa vào giả lập.
- Không sửa `board.h`, `board.c`, header board mới của chủ dự án, `speaker`.

Phiên 8 (tiếp, cùng ngày): module `inmp441`.

Đã làm (chủ dự án đã push `sx_i2s` mở rộng ở commit `f2eef0d`, đúng bằng bản phiên 8, chỉ khác dòng cuối không có newline; đã tạo sẵn `micro/inmp441.c` và `.h` rỗng):

- `LoganSDK/components/modules/micro/inmp441.h` và `inmp441.c` (viết vào hai file rỗng). Module chỉ dùng hợp đồng `sx_i2s`, không include `board.h` hay ESP-IDF. Cấu hình `inmp441_config_t`: `port`, `bclk_pin` (SCK), `ws_pin` (WS), `din_pin` (SD), `sample_rate_hz`. Module tự dựng cấu hình `sx_i2s`: `dout_pin = -1`, mono, 32 bit. `inmp441_read` đọc từng khối tối đa 128 slot (buffer 512 byte nằm trong `inmp441_t`, do board cấp phát tĩnh, không dùng heap), đổi sang `int16_t` bằng `raw >> 16`. Khi lỗi hoặc timeout, `*count` là số mẫu hợp lệ đã đọc. Phần slot lẻ byte trả `INMP441_ERR_BUS`. Không có phần chọn kênh phải: chỉ hỗ trợ L/R mức thấp (kênh trái).
- `LoganSDK/components/modules/CMakeLists.txt` (sửa): thêm `micro/inmp441.c` và thư mục `micro` vào `INCLUDE_DIRS`; sửa comment đầu file.
- `docs/Firmware_Construction.md` (sửa): thêm `inmp441` vào danh sách module, cập nhật dòng `sx_i2s` ở bảng trạng thái peripheral.

Đã kiểm chứng:

- Chạy trên máy host (ASan và UBSan) với `sx_i2s` giả lập: kiểm tra tham số, cấu hình `sx_i2s` mà module tạo ra, máy trạng thái init/start/stop/deinit và các đường lỗi (init lỗi rồi thử lại, deinit lỗi rồi thử lại), đổi mẫu (cực trị, số âm, thứ tự qua 3 khối), không ghi quá vùng yêu cầu, timeout giữa chừng giữ lại dữ liệu đã đọc, driver trả OK mà không có dữ liệu không gây vòng lặp vô hạn, ánh xạ mã lỗi. 0 lỗi. Không chạy thử đột biến cho module này.
- Biên dịch `inmp441.c` sạch với `-Wall -Wextra -Wshadow -Wconversion -Wsign-conversion -Wpedantic` trên host, và bằng `xtensa-esp32s3-elf-gcc` 14.2.0 (`-Os`, cùng cờ), 422 byte code, không dữ liệu tĩnh. Đây là biên dịch một file, **không phải `idf.py build`**. Bài test và bản giả lập nằm ngoài repo (không commit); giả lập chỉ chứng minh logic khớp mô hình do chính phiên này viết.

Chưa làm hoặc chưa kiểm chứng:

- Chưa `idf.py build`, chưa chạy trên phần cứng, chưa nghe thử dữ liệu mic thật. Chưa kiểm tra mức tín hiệu thực tế: `raw >> 16` chỉ lấy 16 bit cao nên tiếng nhỏ có thể rất thấp biên độ; việc khuếch đại hoặc chuẩn hóa để gửi STT chưa làm (có thể xử lý ở app hoặc backend).
- Thời gian khởi động của mic sau khi có clock (mẫu đầu không hợp lệ), dải tần số lấy mẫu hỗ trợ, độ lệch DC: theo datasheet thông dụng, chưa đối chiếu. Doc trong `inmp441.h` yêu cầu caller bỏ các mẫu đầu sau `inmp441_start`.
- Chưa có `board_init()`: chưa ai tạo `inmp441_t` và `inmp441_config_t`. Dự kiến cấu hình: `port = 1`, `bclk_pin = 10`, `ws_pin = 11`, `din_pin = 12`, `sample_rate_hz = 16000` (theo gán chân của chủ dự án, chưa xác minh trên V15). Cổng 1 chọn để tách khỏi loa trên cổng 0, chưa có `speaker` nên chưa kiểm tra cùng lúc.
- Module không thread-safe (như `touch`).

## 6. Việc đang chờ chủ dự án

Đã xong và đã gỡ khỏi mục này ở phiên 7 (đối chiếu với repo ở commit `aa0d00c`): các lệnh `git mv` của `CmakeLists.txt`, `screem_app.c` và `esp32c3_time.c`; `git rm` của `sx_sdcard.h` và `esp32s3_sdcard.c`; chép đè các `CMakeLists.txt`; nối FatFs và mục đích của `libs/`. Các thay đổi CMake ở phiên 6 (`services` `REQUIRES modules utils`, `PRIV_REQUIRES libs`; `libs` trong `EXTRA_COMPONENT_DIRS`; `storage_app` trong `app`; bỏ `storage` khỏi `modules`) đã nằm trong repo.

Đã chốt ở phiên 6: thứ tự layer là app > services > modules > peripherals > platform (chỉ gọi xuống dưới); `utils` là logic thuần nên mọi layer dùng được.

Cần chủ dự án làm:

- Chạy `idf.py build` trên máy của mình sau commit `aa0d00c` (lần build đầu có `sx_time`, `screenHMI`, `touch` trên máy chủ dự án). Lưu ý đã gặp ở phiên 6: biến môi trường `IDF_TARGET=esp32` trong shell làm `set-target esp32s3` báo lệch; lệnh `idf.py set-target esp32-s3` chạy được.
- Chạy `idf.py build` sau phiên 8 (file đổi: `sx_i2s.h`, `esp32s3_i2s.c`, `modules/micro/inmp441.h`, `inmp441.c`, `modules/CMakeLists.txt`) và báo lỗi nếu có.
- Điền `DEFAULT_WIFI_SSID` và `DEFAULT_WIFI_PASSWORD` thật trong `LoganSDK/app/app_config.h` trước khi flash (hiện vẫn là `YourSSID`, `YourPassword`, nên STA sẽ không nối được router). `DEFAULT_STATIC_IP` đã được đổi sang `192.168.4.1` ở phiên 11. AP và router không được cùng dải mạng.
- Chạy `idf.py build` sau phiên 11 (file đổi: `peripherals/wifi/sx_wifi.h`, `platform/esp32s3/wifi/esp32s3_wifi.c`, `services/http/http.h`, `http.c`, `peripherals/CMakeLists.txt`, `platform/esp32s3/CMakeLists.txt`, `services/CMakeLists.txt`) và gửi log. Cần xem kích thước bin so với phân vùng app 1 MB (xem rủi ro ở nhật ký phiên 11). Với `wifi_app` đã gọi `sx_wifi_*`, kích thước bin lần build này sẽ phản ánh Wi-Fi, lwIP và NVS thật (còn `http_svc_perform` vẫn chưa ai gọi nên chi phí HTTP và TLS chưa hiện trong bin).
- Chạy `idf.py build` sau phiên 10 (file đổi: `modules/speaker/speaker.h`, `speaker.c`, `peripherals/i2s/sx_i2s.h`, `platform/esp32s3/i2s/esp32s3_i2s.c`) và báo lỗi nếu có. Sau đó có thể bỏ `speaker.c` khỏi nhóm file rỗng cần dọn.
- Xác nhận nguồn 3.3V cho INMP441 (header mở rộng không có 3.3V; chân +3.3V ở header debug ghi là chỉ tham khảo).
- Cung cấp schematic V15 hoặc kết quả xác minh: chân LCD, touch, SD, I2S, RS485, loại PSRAM và dung lượng Flash, có microphone hay không (xem mục 4).

Dọn tùy chọn, chờ chủ dự án duyệt (không nằm trong `SRCS`, không ảnh hưởng build):

- `LoganSDK/components/peripherals/{gpio/sx_gpio.c,i2c/sx_i2c.c,i2s/sx_i2s.c,i80/sx_i80.c,spi/sx_spi.c}` (file rỗng).
- `LoganSDK/platform/CMakeLists.txt` (cũ, không phải component) và `LoganSDK/CMakeLists.txt` (0 byte).
- `LoganSDK/platform/esp32s3/{gpio/esp32s3_gpio.h,i2c/esp32s3_i2c.h,i80/esp32s3_i80.h,spi/esp32s3_spi.h}` (file rỗng, nằm trong thư mục có trong `INCLUDE_DIRS`).

Quyết định còn mở (thêm ở phiên 11):

- Đặc tả backend AI (URL, endpoint, định dạng JSON hoặc audio, xác thực, TLS, STT/TTS) để viết `AI_backend`. `AI_backend` và `AI_frontend` khác nhau thế nào cũng chưa được trả lời.
- Trang web cấu hình Wi-Fi qua AP (cần thêm `esp_http_server` và chỗ lưu SSID, mật khẩu như NVS hoặc thẻ SD): chủ dự án nói chưa cần ở giai đoạn này.
- `sx_wifi_init` xóa cả phân vùng NVS khi `nvs_flash_init` báo đầy hoặc sai phiên bản (mẫu chuẩn của ESP-IDF). Nếu sau này code khác lưu dữ liệu trong NVS thì cần đổi cách xử lý.
- Nếu `idf.py build` báo vượt phân vùng app 1 MB: cần chủ dự án quyết định đổi partition (phiên 11 không tự đổi).
- CA bundle đang là `CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_FULL=y` (đọc từ `sdkconfig`), tốn flash. Có thể đổi sang bộ nhỏ hơn hoặc chỉ dùng PEM của backend (`ca_cert_pem`) nếu cần tiết kiệm; chưa đo.

Quyết định còn mở (thêm ở phiên 10, chờ chủ dự án trả lời trước khi khôi phục `board.h`):

- `app_start()` có gọi `app_init()` khi năm hàm `*_init` của app còn chưa có thân hàm không (đề xuất: chưa gọi, không động vào các file rỗng; hoặc thêm stub).
- Thêm `services` vào `REQUIRES` của `board` để `board.c` sở hữu `storage_config_t` (hợp lệ theo sơ đồ layer, khác bảng ở `Firmware_Construction.md` mục 2, sẽ phải cập nhật bảng), hay để `storage_app` sở hữu.
- `board.h` tự khai báo số chân có nhãn `REF/DESIGN/GIVEN` và không include header `board_device` của chủ dự án (đề xuất), hay sửa header đó để dùng.
- Giá trị chưa có nguồn xác minh khi viết cấu hình board: `pclk_hz`, MADCTL, đảo màu, bảng lệnh panel, hướng touch, LEDC timer và channel, SPI host, I2C port. Đề xuất giá trị bảo thủ nhãn `DESIGN`, hoặc tra cấu hình công khai của WT32-SC01 Plus làm nhãn `REF`.
- Loại `service` cần viết sau `speaker` (xem mục 7).

Quyết định còn mở (cũ):

- Tên thư mục chip: repo dùng `LoganSDK/platform/esp32s3` (số ít) nhưng `Firmware_Construction.md` và một số tài liệu ghi `platforms/`. Đề xuất sửa tài liệu cho khớp, chưa sửa vì chưa được duyệt.
- Tên file tài liệu: repo là `HandOver.md`, `RuleCode.md`, `RuleAgent.md`; một số tài liệu vẫn nhắc `handover.md`, `rule_code.md`. Chưa sửa vì chưa được duyệt.
- Khôi phục `board.h` đầy đủ (nhãn `REF/DESIGN/GIVEN`, `BOARD_PINMAP_VERIFIED=0`), `board_init()` và hàm khởi động app. Hiện `app.c` có `app_init()` chứ không phải `app_start()`; cần chốt tên và nối từ `app_main()`. Chưa làm vì chưa được yêu cầu.
- `sdkconfig` đang được commit (`.gitignore` chỉ có `build/`): giữ hay không.
- Có build cJSON trong `libs/` hay không, và dùng ở layer nào.
- Mức ưu tiên và stack của `storage_app` (hiện 4096 và 4, ghi trong `storage_app.h` là chưa chắc).
- Các điểm cần chỉnh của `utils/cqueue` (xem `Firmware_Construction.md` mục 9): chưa sửa vì là code của chủ dự án.

## 7. Kế hoạch tiếp theo

Thứ tự đề xuất (mỗi bước phải build được trước khi sang bước sau).

0. (Xong) Nối CMake và build.
0a. Chủ dự án build lại trên máy sau commit `aa0d00c` (xem mục 6).
0b. Khôi phục `board.h`, `board.c` (`board_init()`), hàm khởi động trong `app.c`, nối từ `app_main()`. Cần chủ dự án chốt tên hàm và cần schematic cho chân (giữ nhãn `chưa xác minh`).
0c. (Phiên 8, chờ chủ dự án build) `sx_i2s` hỗ trợ RX (mic) và sửa timeout. Sau đó: khôi phục `board.h` dùng nhãn `REF/DESIGN/GIVEN` và nối header `board_device/ZX3D50CE08S-V15-USRC.h` (cần định nghĩa hoặc thay macro `GPIO_xx`), rồi module `speaker` và cấu hình `inmp441` trong `board_init()`.
1. (Xong ở phiên 5: build OK) `sx_pwm`, `sx_uart`, callback ngắt của `sx_gpio`.
2. Modules: `speaker` đã viết ở phiên 10 (chờ build của chủ dự án). `screenHMI` và `touch` đã viết ở phiên 6; `storage` đã viết ở phiên 6 và nằm ở `services/storage`.
2a. Theo chỉ đạo của chủ dự án (phiên 10): sau `speaker` là viết phần service, rồi mới viết app. Chưa chốt service nào (xem mục 6). `board.h`, `board_init()`, `app_start()` vẫn cần có trước khi bất kỳ instance nào (speaker, mic, màn hình, touch, SD) được tạo và chạy.
2b. `storage_app` (task duy nhất gọi `storage_*`), sau khi có `board_init()` tạo `sdcard_t`, `sdcard_config_t`, `storage_config_t`.
3. Bring-up trên phần cứng (chủ dự án chạy build và flash): xác minh Flash/PSRAM, chân, LCD hiển thị, touch đọc được, SD mount, speaker ra tiếng, thử mic.
4. Wi-Fi (`sx_wifi`) và `services/http` đã viết ở phiên 11 (chờ build của chủ dự án). Còn lại: LVGL (flush callback, đọc input từ touch), OTA, và phần AI (chờ đặc tả backend). Sau đó theo roadmap trong `Product_Construction.md`.
4a. Phiên sau (khi có log flash Wi-Fi của chủ dự án): `AI_backend` kiểu REST text-only (`POST /v1/chat`, JSON `{session_id,text}` trả `{reply}`, đề xuất của phiên 11, chưa được chủ dự án xác nhận là đặc tả backend) kèm backend Python mẫu; cần thêm cJSON vào `libs/CMakeLists.txt`. Streaming âm thanh qua WebSocket để sau, khi có đặc tả backend và biết loại PSRAM.

## 8. Ghi chú môi trường (cho phiên chạy trong sandbox)

- Sandbox Linux, **không truy cập được `E:\...` của chủ dự án**; làm việc bằng cách clone repo và present file.
- Mạng sandbox chặn `dl.espressif.com`. ESP-IDF cài được qua github.com (clone `v5.4.3 --recursive`, `install.sh esp32s3`), nhưng:
  - cần đặt `IDF_PYTHON_CHECK_CONSTRAINTS=no`;
  - cài Python requirements trực tiếp từ PyPI (`pip install --index-url https://pypi.org/simple -r tools/requirements/requirements.core.txt`);
  - cài `cmake==3.30.5` và `ninja` bằng pip (CMake 4.x chưa dùng cho IDF 5.4).
- Build thử trên bản sao của repo, không `set-target` trong repo thật.

### Phiên 9 (2026-11-07)

Phiên này chỉ kiểm chứng, không đổi code trong repo.

Đã làm:

- Pull về commit `2147009` (đã có `sx_i2s` mở rộng, `inmp441`, header board và datasheet). Phiên này đã viết song song một bản mở rộng `sx_i2s` có thiết kế gần như giống hệt bản đã có trong repo, nên bản đó bị bỏ, không đưa vào repo (không có gì để trộn).
- Chạy bộ test host của phiên này lên chính `esp32s3_i2s.c` trong repo (driver I2S và semaphore giả lập, ASan và UBSan): loa chỉ TX, mic chỉ RX trên cổng 1, song công (đọc trong lúc ghi và ngược lại không chờ nhau), thứ tự bật tắt, lỗi khởi tạo ở từng bước không để lại kênh, `start` lỗi ở RX, `stop` và `deinit` lỗi một phần rồi thử lại, timeout truyền nguyên ms, ánh xạ mã lỗi, mọi đường lỗi nhả khóa. **0 lỗi**, không cảnh báo `-Wall -Wextra -Wshadow -Wconversion -Wsign-conversion -Wpedantic`. Thử 5 đột biến cố ý trên bản của phiên này (đổi đơn vị timeout, bỏ việc coi kênh đã tắt là đã dừng, dùng chung một khóa cho đọc và ghi, quên nhả khóa, luôn tạo kênh RX), test bắt được cả 5.
- Chạy `idf.py build` đầy đủ trên commit `2147009` trong sandbox (ESP-IDF v5.4.3, GCC 14.2.0, target esp32s3): **thành công** (rc=0), không có cảnh báo biên dịch từ code dự án. `board_hmi_ai.bin` 0x2e050 byte, còn trống 82% phân vùng app 1 MB. Kích thước không đổi so với trước vì chưa có code nào gọi `sx_i2s`, `inmp441`, `screenHMI`, `touch`, `storage`, `sx_time`, nên linker bỏ chúng. Đây là lần build đầu tiên có header driver I2S thật của ESP-IDF trong `esp32s3_i2s.c`.

Chưa làm hoặc chưa kiểm chứng:

- Không chạy lại test host của `inmp441` (phiên 8 đã làm), chưa chạy trên phần cứng, chưa kiểm chứng hành vi song công thật của driver ESP-IDF (chỉ có giả lập), chưa kiểm tra mic và loa chạy cùng lúc (chưa có `speaker`).
- Các điểm lệch tài liệu, nguồn 3,3 V cho INMP441, và việc khôi phục `board.h`, `board_init()`, `app_start()` vẫn như mục 7.

### Phiên 10 (2026-11-07)

Đầu phiên: clone repo (HEAD `ae9e5f5`, tức `2147009` cộng cập nhật handover phiên 9), đọc `HandOver.md`, `RuleCode.md`, `RuleAgent.md`, `Firmware_Construction.md`, `Product_Construction.md`, đối chiếu repo. Nhiệm vụ ban đầu là khôi phục `board.h`, `board_init()`, `app_start()`; chủ dự án đổi hướng sang: viết driver `speaker`, rồi service, rồi app. Việc khôi phục board chưa làm (xem mục 6, các điểm cần chốt).

Lệch phát hiện khi đối chiếu (chưa có trong handover trước):

- `app_init()` trong `app.c` gọi `ai_app_init`, `http_app_init`, `ntp_app_init`, `speaker_app_init`, `app_storage_init`. Cả năm chỉ có khai báo trong header, các file `.c` tương ứng rỗng. Hiện build được vì chưa ai gọi `app_init()`; nếu gọi sẽ lỗi link.
- `board/CMakeLists.txt` chỉ `REQUIRES peripherals modules`, trong khi `storage_config_t` (handover muốn `board.c` sở hữu) nằm ở `services`.
- Header `board_device/ZX3D50CE08S-V15-USRC.h` dùng macro `GPIO_xx` chưa định nghĩa, thư mục không nằm trong include path của `board`, comment có ký tự CJK. Header này cũng có chân RS485 (RXD 1, RTS 2, TXD 42), chưa xác minh.

Đã làm:

- `LoganSDK/components/modules/speaker/speaker.h` và `speaker.c` (viết vào hai file rỗng; CMake của `modules` đã có sẵn nguồn và thư mục `speaker`, không sửa). Cấu hình `speaker_config_t`: `port`, `bclk_pin`, `ws_pin`, `dout_pin`, `sample_rate_hz`. Module tự dựng cấu hình `sx_i2s`: `din_pin = -1`, stereo, 16 bit. `speaker_write` nhận mono `int16_t`, đổi từng khối 128 mẫu thành frame trái phải giống nhau (buffer 512 byte trong `speaker_t`, không dùng heap), nhân gain, rồi gọi `sx_i2s_write`. Khi lỗi hoặc timeout, `*written` là số mẫu đã đưa vào hàng đợi. Ghi dở một frame trả `SPEAKER_ERR_BUS` (trái phải có thể bị đảo, cần stop rồi start lại). `speaker_set_volume` được phép gọi từ task khác trong lúc `speaker_write` chạy (`volume` là `volatile uint8_t`); mọi hàm khác không thread-safe.
- Lý do gửi cả hai slot: đã đối chiếu `i2s_std.h` của ESP-IDF v5.4.3, kênh mono chỉ phát slot trái (`I2S_STD_SLOT_LEFT`); amp trên board chưa xác minh nghe slot nào.
- `LoganSDK/platform/esp32s3/i2s/esp32s3_i2s.c` (sửa 2 dòng) và `LoganSDK/components/peripherals/i2s/sx_i2s.h` (thêm 2 dòng comment hợp đồng): đặt `channel_config.auto_clear_after_cb = true` và ghi vào hợp đồng rằng hướng phát xuất mẫu 0 khi không có dữ liệu, không lặp dữ liệu cũ. Lý do: đối chiếu `i2s_common.h` và `i2s_common.c` của v5.4.3, `I2S_CHANNEL_DEFAULT_CONFIG` để `auto_clear_after_cb = false`, buffer DMA không bị xóa sau khi phát nên khi ngừng cấp dữ liệu mà kênh còn chạy (ví dụ TTS streaming bị đứt) driver phát lại buffer cũ. Trường `auto_clear_after_cb` có trong `i2s_chan_config_t` của v5.4.3. Không ảnh hưởng hướng thu (chỉ gán cho kênh TX).
- `docs/Firmware_Construction.md`: cập nhật dòng `sx_i2s` ở bảng trạng thái peripheral (mục 8).

Đã kiểm chứng:

- Chạy trên máy host (ASan và UBSan, `-fno-sanitize-recover=all`) `speaker.c` thật với `sx_i2s` giả lập: kiểm tra tham số và từng trường cấu hình, máy trạng thái init/start/stop/deinit và đường lỗi (init lỗi rồi thử lại, stop và deinit lỗi rồi thử lại), chia khối (300 mẫu thành 3 lần ghi 512, 512, 176 byte), mẫu trái và phải giống nhau, gain 100, 50, 0 phần trăm kể cả số âm và cực trị (-32768, 32767), ghi dở (timeout, ghi ngắn không báo lỗi, ghi dở một frame), không lặp vô hạn khi driver không nhận byte nào, ánh xạ mã lỗi. 0 lỗi. 8 đột biến cố ý (bỏ nhân đôi slot, sai mẫu số gain, bỏ kiểm tra frame dở, bỏ kiểm tra ghi ngắn, sai kích thước frame, không cập nhật `*written`, đổi sang mono, bỏ chân `dout`) đều bị bắt; đột biến bỏ kiểm tra ghi ngắn gây vòng lặp vô hạn và bị chặn bằng timeout 10 giây. Bài test và bản giả lập nằm ngoài repo (không commit); giả lập chỉ chứng minh logic khớp mô hình do chính phiên này viết.

Chưa làm hoặc chưa kiểm chứng:

- **Không chạy `idf.py build`** (chủ dự án tự build). Hai dòng sửa trong `esp32s3_i2s.c` chưa được biên dịch với header ESP-IDF thật (đã đối chiếu tên trường bằng cách đọc `i2s_common.h` v5.4.3). `speaker.c` chưa biên dịch bằng GCC cho ESP32-S3.
- Chưa chạy trên phần cứng, chưa nghe thử. Amp trên board chưa xác minh (loại, chân enable hoặc shutdown, gain, slot nghe). Chân I2S loa (35, 36, 37) vẫn chưa xác minh và loại PSRAM chưa biết (nếu octal thì không dùng được).
- Hành vi thật của `auto_clear_after_cb` trên phần cứng chưa kiểm tra. `speaker_stop` cắt phần âm còn trong DMA (theo mặc định driver là 6 buffer x 240 frame, khoảng 90 ms ở 16 kHz; con số này lấy từ `I2S_CHANNEL_DEFAULT_CONFIG` của v5.4.3, không do module kiểm soát).
- Volume tuyến tính, chưa có đường cong theo độ to cảm nhận. Chưa có phát hiện clip hay giới hạn công suất.
- Chưa kiểm tra loa và mic chạy cùng lúc (loa dự kiến cổng 0, mic cổng 1).
- `board.h`, `board_init()`, `app_start()`, service, app: chưa làm.


### Phiên 11 (2026-11-08)

Đầu phiên: clone repo (HEAD `c02d469`), đọc `HandOver.md`, `RuleCode.md`, `RuleAgent.md`, `Firmware_Construction.md` và các phần liên quan của `Product_Construction.md`, đối chiếu repo. Nhiệm vụ: viết xong driver Wi-Fi, sau đó services http và AI; chỉ kiểm tra logic rồi present file, chủ dự án tự build và gửi log.

Lệch phát hiện khi đối chiếu:

- Có 4 commit sau `ae9e5f5` chưa nằm trong handover: `9ecb73b`, `41ad749` ("default wifi & static IP"), `2d5db6e` ("add wifi"), `c02d469` ("add exoskeleton wifi"). Phần Wi-Fi mới chỉ là khung: `sx_wifi.h` rỗng, `esp32s3_wifi.c` chỉ có một dòng `#include "sx_wifi.h"`, cả hai chưa được đăng ký trong CMake; `app_config.h` có `DEFAULT_WIFI_SSID/PASSWORD`, `DEFAULT_AP_SSID/PASSWORD`, `DEFAULT_STATIC_IP "192.168.1.1"`. Handover phiên 10 và `speaker` đã nằm trong repo.
- `board.h` vẫn là stub, chưa có `board_init()` và `app_start()`. `http.c`, `http.h`, `AI_backend.c`, `AI_frontend.*` rỗng, `AI_backend.h` chỉ có guard. Chưa có kết quả build sau phiên 10.
- Tên file tài liệu thật là `HandOver.md`, `RuleCode.md` (không phải `handover.md`, `rule_code.md`).

Đã chốt với chủ dự án:

- Backend AI để sau (phiên này không viết `AI_backend`, `AI_frontend`).
- Chế độ APSTA (STA và AP cùng lúc). AP tĩnh `192.168.4.1`, STA dùng DHCP. Đổi từ `192.168.1.1` vì dải đó dễ trùng router nhà. Chưa làm trang web cấu hình.
- Chấp nhận thêm component có sẵn của ESP-IDF: `esp_wifi`, `esp_netif`, `esp_event`, `nvs_flash` (cho `platform/esp32s3`), `esp_http_client`, `esp-tls` (cho `services`). Chấp nhận rủi ro dung lượng flash (xem dưới).

Đã làm:

- `LoganSDK/components/peripherals/wifi/sx_wifi.h` (viết vào file rỗng): hợp đồng. Cấu hình `sx_wifi_config_t`: `mode` (STA, AP, APSTA), `sta_ssid`, `sta_password`, `sta_retry_ms` (0 là 5000, chọn theo thiết kế, chưa đo), `ap_ssid`, `ap_password`, `ap_ip` (chuỗi), `ap_max_clients` (0 là 4, tối đa 8), `callback`, `callback_context`. Chuỗi được sao chép khi init. Một instance duy nhất. Mật khẩu rỗng hoặc NULL là mạng mở, ngược lại 8 đến 63 ký tự. IP của AP phải có octet cuối từ 1 đến 254, octet đầu khác 0, 127 và từ 224 trở lên. Không có tham số kênh AP: AP bắt đầu ở kênh 1 và theo kênh router khi STA nối (clients của AP có thể rớt thoáng qua). Callback chạy trong task sự kiện hệ thống, không được chặn và không được gọi `wait_ip/start/stop/deinit` từ đó.
- `LoganSDK/platform/esp32s3/wifi/esp32s3_wifi.c` (viết vào file đã có một dòng): hiện thực. Init: kiểm tra config, `nvs_flash_init` (xóa và init lại khi đầy hoặc sai phiên bản), `esp_netif_init`, vòng lặp sự kiện mặc định (đã tồn tại thì bỏ qua lỗi), tạo netif mặc định cho STA và AP theo mode, đặt IP tĩnh AP (dừng DHCP server, `esp_netif_set_ip_info`, bật lại), `esp_wifi_init`, `esp_wifi_set_storage(WIFI_STORAGE_RAM)` (cấu hình lấy từ config mỗi lần khởi động nên không ghi flash), đặt mode và cấu hình, đăng ký handler cho `WIFI_EVENT` và `IP_EVENT`, tạo timer thử nối lại. Mọi lỗi giữa chừng đều hoàn tác (`release_all`). `start` đặt cờ `running` trước `esp_wifi_start` vì sự kiện start có thể đến trước khi hàm trả về. Mất kết nối: ghi mã lý do, xóa IP và cờ `wait_ip`, gọi callback, hẹn nối lại bằng `esp_timer`; không hẹn khi `running` đã tắt. `wait_ip` làm tròn lên số tick. Định dạng IP viết tay (không `snprintf`) vì task sự kiện hệ thống chỉ có stack 2304 byte (`CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE` trong `sdkconfig`). Tên hàm và logic dùng trạng thái tĩnh, nên chỉ một instance.
- `LoganSDK/services/http/http.h`, `http.c` (viết vào file rỗng): `http_svc_perform` (một yêu cầu chặn, không giữ trạng thái giữa các lần gọi). Phương thức GET, POST, PUT, DELETE; header tùy ý; thân yêu cầu chỉ cho POST và PUT; buffer phản hồi do người gọi cấp (NULL thì bỏ qua thân); có byte 0 kết thúc nếu còn chỗ; thân quá lớn trả `HTTP_SVC_ERR_OVERFLOW` kèm status và phần đầu; status HTTP bất kỳ vẫn là `HTTP_SVC_OK` của lần gọi (người gọi kiểm tra `status`); không theo redirect; timeout mỗi bước mặc định 10000 ms (chọn theo thiết kế, chưa đo). Dùng luồng `esp_http_client_open`, `fetch_headers`, `read`, nên phản hồi phải có Content-Length hoặc chunked (thân kết thúc bằng đóng kết nối được báo là `HTTP_SVC_ERR_NET`). HTTPS luôn kiểm chứng chứng chỉ; không có tùy chọn bỏ kiểm chứng. Không đụng đến Wi-Fi: chưa có mạng thì trả `HTTP_SVC_ERR_NET`.
- CMake: `peripherals` thêm thư mục `wifi`; `platform/esp32s3` thêm `wifi/esp32s3_wifi.c` và `PRIV_REQUIRES esp_wifi esp_netif esp_event nvs_flash`; `services` thêm `PRIV_REQUIRES esp_http_client esp-tls` (`mbedtls` và `esp_crt_bundle.h` đi qua `esp-tls`, dựa vào `REQUIRES mbedtls` của `esp-tls` trong v5.4.3).
- `docs/Firmware_Construction.md`: thêm dòng `sx_wifi` vào bảng mục 8.
- Không sửa `board.h`, `AI_backend`, `AI_frontend`. Phần bring-up (app, `main`, `app_config.h`) ghi ở mục "Bổ sung cuối phiên 11" bên dưới.

Đã kiểm chứng:

- Đối chiếu header và mã nguồn ESP-IDF v5.4.3 (tải từ github): `esp_wifi.h`, `esp_wifi_types_generic.h`, `esp_wifi_default.h`, `esp_netif.h`, `esp_netif_types.h`, `esp_netif_ip_addr.h`, `esp_netif_lwip.c`, `esp_event.h`, `esp_timer.h`, `nvs_flash.h`, `nvs.h`, `esp_http_client.h`, `esp_crt_bundle.h`, `CMakeLists.txt` của `esp_wifi`, `esp_http_client`, `esp-tls`. Điểm đáng chú ý: `esp_netif_create_default_wifi_sta/ap` abort khi lỗi chứ không trả mã lỗi; DHCP server của AP: dừng, `set_ip_info` (chỉ cho khi đã dừng), bật lại, và khi AP chưa chạy thì server thật sự khởi động lúc AP start với địa chỉ mới (đọc `esp_netif_lwip.c`); `esp_http_client_read` trả -`ESP_ERR_HTTP_EAGAIN` khi timeout, và `esp_http_client_is_complete_data_received` dùng để phân biệt hết dữ liệu với bị cắt.
- Chạy trên máy host (ASan và UBSan, `-fno-sanitize-recover=all`), ESP-IDF giả lập; mô hình trạng thái DHCP server giả lập sao theo `esp_netif_lwip.c` (nên chuỗi sai thứ tự bị bắt). `sx_wifi`: 121 check, 0 lỗi: kiểm tra tham số (kể cả IP biên), cấu hình đưa cho driver (SSID, mật khẩu, auth, kênh, số client, PMF), thứ tự gọi DHCP, luồng STA (nối, có IP, mất IP, mất kết nối, hẹn lại, timer lỡ gọi, nối lại thất bại tức thì), dừng không hẹn lại, đếm client AP không xuống dưới 0, rollback ở từng bước lỗi (NVS đầy, vòng lặp sự kiện, đặt IP, `esp_wifi_init`, đăng ký handler, `start`), `deinit` rồi `init` lại, làm tròn tick, định dạng IP ở các biên (255, 100, 101). Thử 8 đột biến (bỏ `dhcps_stop`, bỏ hẹn lại, quên tắt `running` khi stop, không làm tròn tick, đếm client xuống âm, bỏ hoàn tác netif, không bật cờ có IP, đặt `running` sau `start`): cả 8 bị bắt. `http`: 61 check, 0 lỗi: tham số, GET, POST ghi từng mẩu nhỏ, thân vừa khít bộ đệm, tràn, đọc từng mẩu nhỏ, status 404 và 302 không phải lỗi, HTTPS dùng bundle, HTTPS dùng PEM riêng, HTTP không dùng bundle, timeout mặc định và tùy chỉnh, mọi đường lỗi đều gọi `close` và `cleanup` đúng một lần, ghi 0 byte không lặp vô hạn, server đóng sớm. 6 đột biến (bỏ cleanup, bỏ thăm dò tràn, bỏ kiểm tra ghi 0 byte (treo, bị timeout chặn), tắt bundle, chấp nhận đóng sớm, bỏ byte 0) đều bị bắt. Bài test và bản giả lập nằm ngoài repo (không commit); giả lập chỉ chứng minh logic khớp mô hình do chính phiên này viết. Lần chạy đầu của test http có 1 lỗi nằm ở bản giả lập (không mô phỏng được ghi 0 byte), đã sửa bản giả lập, code production không đổi.
- Biên dịch sạch hai file production trên host với `-Wall -Wextra -Wshadow -Wconversion -Wsign-conversion -Wpedantic -Werror -Wmissing-prototypes -Wstrict-prototypes` (header giả lập).

Chưa làm hoặc chưa kiểm chứng:

- **Không chạy `idf.py build`** (chủ dự án tự build). Hai file chưa biên dịch với header ESP-IDF thật hay bằng GCC cho ESP32-S3; chỉ đối chiếu tên hàm, tên trường và kiểu bằng cách đọc header v5.4.3. Cách dùng `portENTER_CRITICAL(&mux)` với `portMUX_TYPE` và `xEventGroupCreateStatic` trong v5.4.3 chỉ dựa trên hiểu biết, chưa đọc header.
- Chưa chạy trên phần cứng, chưa nối router thật, chưa thử cấp IP cho client qua AP, chưa kiểm tra STA với router WPA3 thuần hoặc WEP (mật khẩu phải 8 đến 63 ký tự, ngưỡng auth của STA để mặc định của driver). Hành vi AP đổi kênh theo router và việc client rớt khi đó chưa kiểm tra thực tế. Chưa kiểm tra chế độ tiết kiệm điện mặc định của Wi-Fi (chưa gọi `esp_wifi_set_ps`).
- `esp_netif_create_default_wifi_*` abort khi lỗi (thường chỉ khi hết bộ nhớ); `sx_wifi_init` không bắt được trường hợp đó. Stack của task sự kiện hệ thống 2304 byte dùng chung với handler mặc định của ESP-IDF và callback của người dùng: nếu tràn stack thì tăng `CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE`.
- Kích thước bin: Wi-Fi, lwIP, TLS và CA bundle FULL sẽ làm bin to lên đáng kể so với 0x2e050 byte hiện tại; chưa đo. Phân vùng app 1 MB, flash 2 MB (theo `sdkconfig`). RAM trong khi chưa bật PSRAM: TLS và APSTA đều tốn RAM nội, chưa đo.
- `http`: chưa gọi server thật, chưa thử chứng chỉ thật, chưa thử response không có Content-Length và không chunked (cố ý báo lỗi), chưa có API truyền thân lớn theo dòng (ví dụ audio TTS) mà phần AI sau này có thể cần. Mỗi lần gọi tạo client mới, kết nối không dùng lại.
- `http_svc_perform` chưa có ai gọi. `sx_wifi` được `wifi_app` gọi (xem mục bổ sung bên dưới). Không thread-safe cho các lệnh vòng đời (init, deinit, start, stop), ghi trong hợp đồng.
- Phần AI chưa làm (để sau theo chủ dự án).

#### Bổ sung cuối phiên 11: bring-up Wi-Fi

Theo thỏa thuận "bước 1: bring-up Wi-Fi" với chủ dự án (các mặc định mình đề xuất, chủ dự án đồng ý bằng "ok"): chưa đụng `board.h`.

- `LoganSDK/app/user/wifi_app/wifi_app.h`, `wifi_app.c` (mới): `wifi_app_start()` tạo task `app_wifi_task` (stack 6144 byte, ưu tiên 3, đều là giá trị chọn theo thiết kế, chưa đo). Trong task: dựng `sx_wifi_config_t` APSTA từ `DEFAULT_*` của `app_config.h`, gọi `sx_wifi_init`, `sx_wifi_start`, rồi in trạng thái mỗi 2000 ms bằng `printf` (`[wifi] sta=... ip=... rssi=... reason=... | ap=... clients=...`). Lỗi init hoặc start được in rồi task kết thúc (start lỗi thì `sx_wifi_deinit`). Không in mật khẩu. Init chạy trong task thay vì `app_main` vì task chính chỉ có 3584 byte stack (`CONFIG_ESP_MAIN_TASK_STACK_SIZE`).
- `LoganSDK/app/app.c` và `app.h`: `app_start()` chỉ gọi `wifi_app_start()`, nằm thẳng trong `app.c` (chủ dự án quyết định không dùng file `app_start.c` riêng; phiên 11 đã đưa file đó và lần build đầu của chủ dự án lỗi vì file không có trong commit). `app_init()` cũ (gọi các `*_init` của app, phần lớn chưa có thân) đã bị xóa khỏi `app.c` ở commit `a6977f4`; bản cũ nằm ở commit `6a59be4`. Vì `app.c` không còn tham chiếu hàm nào chưa có thân nên link không bị kéo lỗi. `app.h` chỉ còn `app_start`. Tên `app_start` lấy từ cách gọi trong handover phiên 10.
- `main/board_hmi_ai.c`: `app_main` gọi `app_start()`.
- `LoganSDK/app/CMakeLists.txt`: thêm `user/wifi_app/wifi_app.c` và thư mục `user/wifi_app`.
- `LoganSDK/app/app_config.h`: `DEFAULT_STATIC_IP` đổi `192.168.1.1` thành `192.168.4.1` (quyết định của chủ dự án, mình sửa một dòng).
- Đã chạy trên máy host (FreeRTOS và `sx_wifi` giả lập, ASan, UBSan): 11 check, biên dịch sạch cờ nghiêm ngặt, cả `app.c` mới; 4 đột biến (bỏ `deinit` khi start lỗi, sai nguồn IP của AP, in mật khẩu, init chạy ở hàm gọi) đều bị bắt. Đã duyệt mọi `idf_component_register` trong repo: chỉ `LoganSDK/platform/CMakeLists.txt` (file cũ còn sót, không nằm trong `EXTRA_COMPONENT_DIRS` nên không build) liệt kê file không tồn tại. Chưa `idf.py build` sau lần sửa này, chưa flash.
- Cần để ý khi flash: nếu console báo tràn stack của task `app_wifi_task` thì tăng `APP_WIFI_STACK_SIZE`; nếu báo tràn stack của task sự kiện hệ thống thì tăng `CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE`. Việc `sx_wifi_init` xóa NVS khi cần (xem quyết định mở) chưa kiểm chứng trên phần cứng. Khi `DEFAULT_WIFI_SSID` vẫn là `YourSSID`, log sẽ hiển thị `sta=connecting` mãi cùng `reason` khác 0; AP vẫn phải lên và có thể nối được.