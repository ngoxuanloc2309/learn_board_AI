# Handover - AI Learning Device

> File này do mọi phiên làm việc (Claude hoặc AI khác) cập nhật ở cuối phiên. Quy tắc: xem `rule_code.md` mục 3.
> Cập nhật lần cuối: 2026-11-05 (phiên 6).

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
| Peripheral `sx_pwm`, `sx_uart`, ngắt của `sx_gpio` | Viết ở phiên 4. Phiên 5: sửa lỗi compile ở `esp32s3_gpio.c`, **build đầy đủ thành công (0 cảnh báo)**; chưa chạy trên phần cứng |
| Module `sdcard` (`components/modules/sdcard`) | Phiên 5: block device SD-SPI trên `sx_spi`. **Đã build trên target esp32s3 (0 cảnh báo) và đã chạy thử trên máy host với thẻ SD giả lập** (SDHC, SDSC, MMC, không có thẻ, cấu hình sai); **chưa thử với thẻ thật**. Chưa có hệ thống tệp (FatFs) |
| Modules (speaker) | Chỉ là file rỗng. Thư mục `modules/storage` không còn: storage đã chuyển lên `services/` (xem dòng dưới) |
| Module `screenHMI` (`modules/screenHMI`) | Phiên 6: ST7796 qua `sx_i80`, reset qua `sx_gpio`, backlight PWM qua `sx_pwm`. Chỉ gửi lệnh MIPI DCS chuẩn; bảng lệnh riêng của panel do board truyền vào. **Đã chạy trên máy host** (hợp đồng `sx_*` giả lập) và biên dịch sạch bằng GCC 14.2 cho ESP32-S3. **Chưa build trong dự án, chưa chạy trên phần cứng** |
| Module `touch` (`modules/touch`) | Phiên 6: FT6x36/FT5x06 (FT6336U) qua `sx_i2c`, chỉ đọc thanh ghi, dùng polling (không dùng chân INT). Kiểm chứng giống `screenHMI`. **Chưa build trong dự án, chưa chạy trên phần cứng** |
| Services (http, AI) | Chỉ là file rỗng |
| Service `storage` (`services/storage`) | Phiên 6: API file trên thẻ SD (mount, open/read/write/seek, list, mkdir, remove, rename, đọc/ghi cả file), nối FatFs vào `sdcard` qua `storage_diskio.c`. **Đã chạy trên máy host** (FatFs thật, thẻ SD giả lập bằng RAM, ASan và UBSan, 0 lỗi) và biên dịch sạch với `-Wall -Wextra -Wshadow -Wconversion -Wsign-conversion -Wpedantic`. Chủ dự án đã chạy `idf.py build` thành công cho esp32s3 (phiên 6). Chưa thử với thẻ thật, chưa chạy trên phần cứng. Lưu ý: `app_init()` chưa được ai gọi nên linker có thể bỏ code storage, kích thước bin chưa phản ánh chi phí thật của FatFs Chưa có `board_init()` nên chưa có code gọi `storage_init` |
| `libs/` (FatFs R0.16 của ChaN, cJSON) | Phiên 6: `libs/CMakeLists.txt` đăng ký component `libs` chỉ build FatFs (`ff.c`, `ffsystem.c`, `ffunicode.c`). cJSON nằm trong thư mục nhưng **chưa build**. Không dùng component `fatfs` của ESP-IDF |
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
- Các thời gian chờ trong `screenHMI` và `touch` (reset thấp 10 ms, chờ sau reset 120 ms, chờ sau SWRESET và SLPOUT 120 ms, chờ boot touch 300 ms): lấy theo thông lệ, chưa đối chiếu datasheet.
- Bảng lệnh riêng của panel (nguồn, gamma, mở khóa lệnh mở rộng): chưa viết, cần nguồn đã xác minh cho đúng panel của board. Không có bảng này màn hình chạy với thiết lập mặc định của chip.
- Giá trị MADCTL cho từng hướng, có đảo màu (INVON) hay không, thứ tự RGB/BGR: chưa xác minh trên board.
- Bản đồ thanh ghi và ý nghĩa cờ sự kiện của touch (TD_STATUS 0x02, điểm đầu từ 0x03, ID 0xA3/0xA6/0xA8): theo bản đồ FocalTech thông dụng, chưa đối chiếu datasheet của chip trên board. Cực tính và chế độ chân INT chưa xác minh nên chưa dùng.

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

Phiên 5 kiểm tra lại sau `git pull`: hai lệnh `git mv` ở trên **vẫn chưa vào git** (repo còn `CmakeLists.txt` và `screem_app.c`), nên build trên Linux lỗi cho tới khi chạy chúng.

Phiên 6 kiểm tra lại: hai lệnh `git mv` (`CmakeLists.txt`, `screem_app.c`) **vẫn chưa vào git**; hai file `sx_sdcard.h` và `esp32s3_sdcard.c` đã được xóa (phần đó xong).

Phiên 6, chủ dự án đã xác nhận thứ tự layer là app > services > modules > peripherals > platform (chỉ gọi xuống dưới), và `utils` là logic thuần nên mọi layer dùng được. `Firmware_Construction.md` (mục 1, 2, 7) đã được sửa cho khớp. Các thay đổi sau đã được chấp nhận:

- `services` giờ `REQUIRES modules utils` và `PRIV_REQUIRES libs`. Lý do: `storage.h` include `sdcard.h` của `modules`, và `storage.c` dùng FatFs ở `libs`.
- `libs` được thêm vào `EXTRA_COMPONENT_DIRS` ở `CMakeLists.txt` gốc (trước đó ghi "không liệt kê").
- `app/CMakeLists.txt`: thêm `user/storage_app/storage_app.c` và include dir `user/storage_app`, vì `app.c` có `#include "storage_app.h"` mà trước đó thư mục này không có trong `INCLUDE_DIRS`.
- `modules/CMakeLists.txt`: bỏ `storage/storage.c` và include dir `storage` (thư mục này không còn, đây là lỗi cấu hình CMake thật gặp ở phiên trước).

Cần chạy trên máy chủ dự án (Git trên Windows). Phiên 6 đã sửa nội dung `services/CMakeLists.txt` nên chỉ cần đổi tên rồi chép đè file đã present:

```powershell
git mv -f LoganSDK/services/CmakeLists.txt LoganSDK/services/CMakeLists.txt
git mv LoganSDK/app/user/screen_app/screem_app.c LoganSDK/app/user/screen_app/screen_app.c
```

Phiên 5 chuyển thẻ SD khỏi peripheral. Cần chạy trên máy của chủ dự án để xóa hai file cũ:

```powershell
git rm LoganSDK/components/peripherals/sdcard/sx_sdcard.h LoganSDK/platform/esp32s3/sdcard/esp32s3_sdcard.c
```

(Đã xử lý ở phiên 6, xem Phiên 6) Cần chủ dự án quyết định: nối FatFs cho `sdcard`. `ff.h` và `diskio_impl.h` thuộc component `fatfs` của ESP-IDF nên cần thêm `fatfs` vào `REQUIRES` của `modules`; quy tắc hiện tại chỉ cho `modules` dùng `peripherals` và `utils`. Đề xuất đặt phần nối này trong module `storage` (một file riêng), không đụng vào `sdcard`.

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

1. (Xong ở phiên 5: build OK) `sx_pwm`, `sx_uart`, callback ngắt của `sx_gpio`. Cần review API trước khi dùng. Module `sdcard` đã viết, xem Phiên 5.
2. Modules: `speaker`. (`screenHMI` và `touch` đã viết ở phiên 6, xem nhật ký.) (`storage` đã viết ở phiên 6, nằm ở `services/storage`.)
3. Bring-up trên phần cứng (chủ dự án chạy `set-target`, build, flash): xác minh Flash/PSRAM, chân, LCD hiển thị, touch đọc được, SD mount, speaker ra tiếng, thử mic.

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
- `sdkconfig` đang được commit vào git (`.gitignore` chỉ có `build/`). Cần chủ dự án quyết định có giữ không.