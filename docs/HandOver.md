# Handover - AI Learning Device

> File này do mọi phiên làm việc (Claude hoặc AI khác) cập nhật ở cuối phiên. Quy tắc: xem `RuleCode.md` mục 3.
> Cập nhật lần cuối: 2026-11-08 (phiên 16).

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
| `board.h`, `board.c` (`board_init()`, chân, instance thiết bị) | Phiên 14: `board.h` khai báo chân bằng số nguyên có nhãn `REF/DESIGN/GIVEN` và `BOARD_PINMAP_VERIFIED = 0` (không include header `board_device`). `board.c` sở hữu instance tĩnh (`screen_hmi_t`, `touch_t`, `speaker_t`, `inmp441_t`, `sdcard_t`) và các cấu hình `const` của chúng, kèm `storage_config_t` (`format_if_mount_failed = false`). API: `board_init()` (chỉ đặt cờ, không chạm phần cứng), `board_init_screen/touch/speaker/mic/storage()` (mỗi hàm một thiết bị, chưa ai gọi), `board_get_screen/touch/speaker/mic()` (NULL tới khi init thành công). `board_init_touch()` trả `BOARD_ERR_STATE` khi màn hình chưa init vì reset GPIO4 dùng chung và do màn hình lái. `board/CMakeLists.txt` thêm `services` vào `REQUIRES`. **Đã biên dịch thử trên máy host** (gcc 13.3, `-fsyntax-only` với `-Wall -Wextra -Wshadow -Wconversion -Wsign-conversion -Wpedantic -Wmissing-prototypes -Wstrict-prototypes -Werror`, sạch) và **đã chạy logic trên host** với module giả lập (ASan, UBSan, trạng thái, thứ tự phụ thuộc, nội dung cấu hình, không trùng GPIO, 13 đột biến đã thử; 2 đột biến đầu sống sót vì thiếu ca test (speaker init lỗi, đảo màu), đã bổ sung test rồi tất cả bị bắt). Chủ dự án báo bằng lời đã `idf.py build` bản phiên 14 **không lỗi** (không gửi log, nên chưa biết có cảnh báo không và kích thước bin). Chưa chạy `board_init` hay `board_init_xxx` trên phần cứng (chưa ai gọi). Các giá trị `DESIGN` xem mục 4 |
| Peripheral `sx_gpio`, `sx_i2c`, `sx_spi`, `sx_i80`, `sx_i2s` | Hợp đồng và hiện thực ESP32-S3 đã viết, **đã compile trong build đầy đủ (phiên 3), chưa chạy trên phần cứng**. Chưa kiểm chứng link vào firmware vì chưa có code nào gọi chúng |
| Peripheral `sx_pwm`, `sx_uart`, ngắt của `sx_gpio` | Viết ở phiên 4. Phiên 5: sửa lỗi compile ở `esp32s3_gpio.c`, **build đầy đủ thành công (0 cảnh báo)**; chưa chạy trên phần cứng |
| Peripheral `sx_time` | Phiên 6: `sx_delay_ms`, `sx_get_time_ms` (`sx_time.h` và `platform/esp32s3/time/esp32s3_time.c`). Đã chạy trên máy host (giả lập FreeRTOS, `esp_timer`) và đã build đầy đủ trong sandbox. Phiên 7: chủ dự án đã đổi tên `esp32c3_time.c` thành `esp32s3_time.c` và push (commit `aa0d00c`), khớp với `platform/esp32s3/CMakeLists.txt`. Chưa chạy trên phần cứng, chưa có ai gọi |
| `sx_i2s` mở rộng (loa TX + mic RX) | Phiên 8: thêm `din_pin` và `sx_i2s_read`; quy ước `-1` (`SX_I2S_PIN_UNUSED`) là không dùng hướng đó; `dout_pin >= 0` tạo kênh TX, `din_pin >= 0` tạo kênh RX, cả hai trên một cổng là song công. Sửa lỗi timeout cũ (xem nhật ký phiên 8). **Đã chạy trên máy host** với driver I2S giả lập (ASan, UBSan, 0 lỗi, 6 đột biến đều bị bắt) và biên dịch sạch cảnh báo nghiêm ngặt trên host. **Chưa build bằng ESP-IDF, chưa chạy trên phần cứng.** Chỉ module `inmp441` gọi `sx_i2s` (phía RX); loa chưa có module |
| Module `inmp441` (`components/modules/micro`) | Phiên 8: mic I2S MEMS INMP441 trên `sx_i2s`, chỉ nhận, mono, slot 32 bit, trả PCM 16 bit có dấu (16 bit cao của mẫu 24 bit). API: `inmp441_init/deinit/start/stop/read`. **Đã chạy trên máy host** (`sx_i2s` giả lập, ASan, UBSan, 0 lỗi) và biên dịch riêng file này sạch cảnh báo nghiêm ngặt, kể cả bằng GCC 14.2 cho ESP32-S3 (422 byte code). **Chưa build trong dự án, chưa chạy trên phần cứng**, được `app/user/wifi_app` gọi trong `app_start()` (cuối phiên 11), chưa build |
| Module `sdcard` (`components/modules/sdcard`) | Phiên 5: block device SD-SPI trên `sx_spi`. **Đã build trên target esp32s3 (0 cảnh báo) và đã chạy thử trên máy host với thẻ SD giả lập** (SDHC, SDSC, MMC, không có thẻ, cấu hình sai); **chưa thử với thẻ thật**. Chưa có hệ thống tệp (FatFs) |
| Module `speaker` (`components/modules/speaker`) | Phiên 10: loa I2S TX trên `sx_i2s`, chỉ phát. Người gọi đưa vào PCM mono 16 bit; module gửi cùng mẫu ra cả slot trái và phải (frame stereo 16 bit) vì chân chọn kênh của amp chưa xác minh. Volume tuyến tính 0 đến 100 phần trăm (mặc định 50, chọn theo thiết kế, chưa đo). API: `speaker_init/deinit/start/stop/set_volume/write`. **Đã chạy trên máy host** (`sx_i2s` giả lập, ASan, UBSan, biên dịch sạch `-Wall -Wextra -Wshadow -Wconversion -Wsign-conversion -Wpedantic -Werror`, 8 đột biến đều bị bắt). **Chưa `idf.py build`, chưa biên dịch bằng GCC cho ESP32-S3, chưa chạy trên phần cứng**, chưa có ai gọi (chưa có `board_init()`). Thư mục `modules/storage` không còn: storage đã chuyển lên `services/` (xem dòng dưới) |
| Module `screenHMI` (`modules/screenHMI`) | Phiên 6: ST7796 qua `sx_i80`, reset qua `sx_gpio`, backlight PWM qua `sx_pwm`. Chỉ gửi lệnh MIPI DCS chuẩn; bảng lệnh riêng của panel do board truyền vào. **Đã chạy trên máy host** (hợp đồng `sx_*` giả lập) và biên dịch sạch bằng GCC 14.2 cho ESP32-S3. Đã build trong dự án ở sandbox (phiên 6, không có cảnh báo biên dịch từ code dự án); **chưa chạy trên phần cứng** |
| Module `touch` (`modules/touch`) | Phiên 6: FT6x36/FT5x06 (FT6336U) qua `sx_i2c`, chỉ đọc thanh ghi, dùng polling (không dùng chân INT). Kiểm chứng giống `screenHMI`. Đã build trong dự án ở sandbox (phiên 6, không có cảnh báo biên dịch từ code dự án); **chưa chạy trên phần cứng**. Phiên 16: `screen_app` có bài thử touch (đọc `touch_read_id` rồi đọc điểm chạm theo chu kỳ, xem nhật ký phiên 16); **chưa build, chưa chạy trên phần cứng**, nên touch vẫn chưa từng được thử với board thật. Cập nhật phiên 16 (log chủ dự án): **touch đã chạy trên phần cứng**, trả lời ở địa chỉ `0x38`, `chip_id=0x64`, `firmware_id=0x11`, `vendor_id=0x11`, đọc điểm chạm được; trục Y bị ngược nên `BOARD_TOUCH_MIRROR_Y` đổi sang `true` (bản sửa chưa build, chưa chạy lại) |
| Peripheral `sx_wifi` (`peripherals/wifi`, `platform/esp32s3/wifi`) | Phiên 11: hợp đồng và hiện thực Wi-Fi cho STA, AP hoặc APSTA cùng lúc. AP có IP tĩnh do config đặt (đã chốt `192.168.4.1`, netmask 255.255.255.0, gateway = IP của AP, DHCP server của chip), STA dùng DHCP, tự thử nối lại sau mỗi lần mất kết nối. API: `sx_wifi_init/deinit/start/stop/wait_ip/get_status` kèm callback sự kiện. **Đã chạy trên máy host** (ESP-IDF giả lập, ASan, UBSan, 121 check, biên dịch sạch `-Wall -Wextra -Wshadow -Wconversion -Wsign-conversion -Wpedantic -Werror`, 8 đột biến đều bị bắt). Sau phiên 11 chủ dự án đã build và flash (suy ra từ log phiên 12, không có log build): board boot, AP `LEARN_BOARD_2309` lên ở `192.168.4.1` với DHCP server, nhưng STA không nối được router, `reason=201` (`NO_AP_FOUND`) lặp mãi. Phiên 12: thêm `sx_wifi_scan`. **Nguyên nhân lỗi đã tìm ra: SSID thật của router là `Log Terminal ` (có một dấu cách ở cuối, 13 ký tự), cấu hình lại thiếu dấu cách.** Sau khi chủ dự án sửa, log cho thấy STA nối kênh 11, WPA2-PSK, nhận IP `192.168.30.102`, AP tự chuyển sang kênh 11. Phần quét đã chạy trên phần cứng và cho kết quả đúng (22 mạng thấy, 16 liệt kê). Code quét đã được chủ dự án build (không có log build gửi lại). Chủ dự án nối điện thoại vào AP `LEARN_BOARD_2309`: board nhận client (`AID=1`), DHCP cấp `192.168.4.2`, `clients=1`, STA vẫn giữ IP. Phiên 13: chủ dự án rút router 20 giây rồi cắm lại, log cho thấy **tự nối lại chạy đúng trên phần cứng** (xem nhật ký phiên 13). Phiên 13 sửa hai điểm hiển thị: `sta_last_reason` về 0 khi có IP, `ap_clients` đọc từ driver (`esp_wifi_ap_get_sta_list`) thay cho bộ đếm sự kiện; hai sửa này **đã build (phiên 13, xem nhật ký), chưa chạy trên phần cứng**. Chưa kiểm tra: router WPA3, AP chuyển tiếp mạng ra router (chưa có, chưa là mục tiêu) |
| Service `http` (`services/http`) | Phiên 11: client HTTP/HTTPS chặn (blocking), một yêu cầu một lần gọi: `http_svc_perform`. Dùng `esp_http_client`; HTTPS luôn kiểm chứng chứng chỉ (CA bundle của firmware, hoặc PEM do người gọi đưa), không có cách tắt. Không theo redirect. **Đã chạy trên máy host** (`esp_http_client` giả lập, ASan, UBSan, 61 check, biên dịch sạch cờ nghiêm ngặt, 6 đột biến đều bị bắt). Phiên 13: thêm `http_app` gọi một GET HTTPS thử. **Đã chạy trên phần cứng (log chủ dự án gửi sau phiên 14, xem bổ sung ở nhật ký phiên 14):** `GET https://example.com/` trả `result=0 (ok)`, `status=200`, thân 577 byte, 2479 ms, chứng chỉ được kiểm chứng bằng CA bundle. Chưa thử HTTP thường, redirect, chứng chỉ sai, mất Wi-Fi giữa lúc gọi, gọi lặp. Chưa có log build đầy đủ của lần build này (suy ra từ việc board chạy được) |
| Service `AI` (`AI_backend`, `AI_frontend`) | Chỉ là file rỗng. Chủ dự án để phần backend AI lại sau (chưa có đặc tả backend: URL, endpoint, JSON, xác thực) |
| Service `storage` (`services/storage`) | Phiên 6: API file trên thẻ SD (mount, open/read/write/seek, list, mkdir, remove, rename, đọc/ghi cả file), nối FatFs vào `sdcard` qua `storage_diskio.c`. **Đã chạy trên máy host** (FatFs thật, thẻ SD giả lập bằng RAM, ASan và UBSan, 0 lỗi) và biên dịch sạch với `-Wall -Wextra -Wshadow -Wconversion -Wsign-conversion -Wpedantic`. Chủ dự án đã chạy `idf.py build` thành công cho esp32s3 (phiên 6). Chưa thử với thẻ thật, chưa chạy trên phần cứng. Lưu ý: `app_init()` chưa được ai gọi nên linker có thể bỏ code storage, kích thước bin chưa phản ánh chi phí thật của FatFs Chưa có `board_init()` nên chưa có code gọi `storage_init` |
| `libs/` (FatFs R0.16 của ChaN, cJSON) | Phiên 6: `libs/CMakeLists.txt` đăng ký component `libs` chỉ build FatFs (`ff.c`, `ffsystem.c`, `ffunicode.c`). cJSON nằm trong thư mục nhưng **chưa build**. Không dùng component `fatfs` của ESP-IDF |
| App tasks | `app_start()` có (gọi `wifi_app_start()`), `app_main()` gọi `app_start()`. `wifi_app` chạy trên phần cứng ở mức Wi-Fi (xem dòng `sx_wifi`), phiên 12 thêm quét chẩn đoán vào `wifi_app`. Phiên 13 thêm `http_app` (task chờ IP rồi gửi một GET thử; đã chạy trên phần cứng, xem dòng `http`). Phiên 15 thêm `screen_app` (task bring-up màn hình, xem nhật ký phiên 15; **chưa build, chưa chạy trên phần cứng**). Các app khác (`AI_app`, `ntp_app`, `speaker_app`, `storage_app`) vẫn là file rỗng hoặc chỉ có header. `board_init()` có từ phiên 14; từ phiên 15 nó được gọi **bên trong task của `screen_app`**, còn `app_start()` tự nó không gọi `board_init()` (`app_start()` gọi `screen_app_start()`). Phiên 16: `screen_app` sau mẫu màn hình gọi tiếp `board_init_touch()` và chạy vòng đọc touch không kết thúc (dòng log `[touch] Tab in <vùng màu> x=.. y=..`); **chưa build, chưa chạy trên phần cứng** |
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
- Giá trị MADCTL cho từng hướng, có đảo màu (INVON) hay không, thứ tự RGB/BGR: chưa xác minh trên board. (Cập nhật phiên 15: đã xác nhận bằng mắt, theo lời chủ dự án, với `MADCTL = MV | BGR`, đảo màu bật và `swap_color_bytes = true`, xem bổ sung 2 của phiên 15. Chưa có log, chưa đo.)
- Bản đồ thanh ghi và ý nghĩa cờ sự kiện của touch (TD_STATUS 0x02, điểm đầu từ 0x03, ID 0xA3/0xA6/0xA8): theo bản đồ FocalTech thông dụng, chưa đối chiếu datasheet của chip trên board. Cực tính và chế độ chân INT chưa xác minh nên chưa dùng.

- (Cập nhật phiên 16) Flash: `esptool.py flash_id` (log chủ dự án) báo `Manufacturer: 5e`, `Device: 4018`, `Detected flash size: 16MB`, kiểu quad, 3.3 V theo eFuse. Đây là số `esptool` đọc ra từ chip, không phải schematic. PSRAM: `esptool` báo `Embedded PSRAM 2MB (AP_3v3)`; loại quad hay octal **chưa kết luận** (suy luận: 3.3 V gợi ý quad, chưa kiểm chứng; cách kiểm chứng là bật `CONFIG_SPIRAM` và xem log boot).
- Cấu hình `sdkconfig` đang commit: flash `2MB`, `CONFIG_SPIRAM` chưa bật, `CONFIG_FREERTOS_HZ=100`. Đây là giá trị đang có trong file, **không phải** thông số đã xác minh của board (phiên 7 chỉ đọc file, chưa đối chiếu phần cứng).

- Mic: file `LoganSDK/board/board_device/ZX3D50CE08S-V15-USRC.h` (commit `d134462`) gán `I2S_MIC_BCLK/WS/DIN` vào `EXT_IO1/2/3` (GPIO 10, 11, 12). Đây là lựa chọn của chủ dự án. Chủ dự án chốt dùng mic INMP441, chân L/R nối xuống GND qua trở 10K do chủ dự án tự thêm (GIVEN, theo lời chủ dự án, chưa kiểm tra bằng phần cứng). Theo cách hiểu của phiên 8 từ datasheet INMP441 (chưa đối chiếu lại): L/R mức thấp cho mic ra kênh trái, khớp `slot_mask` trái của mono; mẫu 24 bit trong khung 32 bit nên cần `bits_per_sample = 32`; nguồn 1.8 đến 3.3 V, header mở rộng chỉ có +5V và GND nên cần nguồn 3.3V riêng (**chưa xác minh** cho V15). Nối dây: SCK vào GPIO 10, WS vào GPIO 11, SD (DOUT của mic) vào GPIO 12 (DIN của ESP32). Header này chưa được include ở đâu, dùng macro `GPIO_xx` chưa định nghĩa, chưa có nhãn `REF/DESIGN/GIVEN`, và có ký tự CJK trong comment (trái `RuleCode.md`, chưa sửa vì là code của chủ dự án).
- `docs/ZX3D50CE08S-USRC-4832.md` (commit `17f1a80`) là datasheet dòng SC01 Plus; ảnh PCB trong đó ghi V13 220511, không phải V15. Nó nêu module `WT32-S3-WROVER-N16R2`, trong khi `sdkconfig` đang flash 2MB và chưa bật SPIRAM. Chưa xác minh cho V15.

Thông tin mạng do chủ dự án cung cấp ở phiên 12 (GIVEN, lấy từ trang thông tin adapter Wi-Fi của laptop Windows, không đo trực tiếp bằng board): router SSID `Log Terminal`, băng 2.4 GHz kênh 11, chuẩn 802.11n, bảo mật WPA2-Personal, mạng `192.168.30.0/24` (gateway `192.168.30.1`), không trùng dải AP `192.168.4.0/24`. Chủ dự án báo thấy mạng `LEARN_BOARD_2309` của board (thiết bị nào thấy thì chưa hỏi), nên phía phát của radio có vẻ chạy. Phần thu hoạt động: quét thấy 22 mạng. SSID thật của router có **dấu cách ở cuối** (`Log Terminal `), nhận biết từ log quét của board (laptop Windows không hiển thị dấu cách này). Quét cũng cho thấy router báo `wpa/wpa2` (chế độ hỗn hợp) và nhiều mạng lân cận trên kênh 1, 4, 5, 6, 11, 13.

Giá trị trong `board.h` do phiên 14 chọn, nhãn `DESIGN` (không có nguồn, chưa xác minh, chỉnh khi bring-up): xung ghi LCD `pclk_hz` 10 MHz; `max_transfer_bytes` 30720; `queue_depth` 1; `swap_color_bytes` true; MADCTL `MV | BGR` (0x28); không đảo màu; không có bảng lệnh panel; backlight LEDC timer 0, channel 0, 5 kHz, 8 bit, active high (duty 0 lúc init, nên **màn hình tối tới khi gọi `screen_hmi_set_backlight`**); touch I2C port 0, 100 kHz, không bật pull-up trong chip, `swap_xy` true, không mirror; SD dùng SPI host 1 (`SPI2_HOST`, đã đối chiếu `spi_types.h` v5.4.3), 10 MHz; speaker I2S port 0, mic I2S port 1, tốc độ lấy mẫu 16000 Hz. Chân LCD, touch, SD, loa vẫn nhãn `REF`; chân mic nhãn `GIVEN`. Ngoài ra: chân CS của LCD giả định nối đất (tham chiếu không có chân CS); chân `TE` (GPIO 48) và `TP_INT` (GPIO 7) chưa dùng nên chưa khai báo; RS485 chưa khai báo vì chưa có nguồn xác minh. Header `board_device/ZX3D50CE08S-V15-USRC.h` còn một ký tự không phải ASCII ở dòng 75 (comment, dấu ngoặc toàn góc), chưa sửa vì là code của chủ dự án.

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

- (Mới, phiên 16) Build bản có bài thử touch (file đổi: `LoganSDK/app/user/screen_app/screen_app.c`, `screen_app.h`, `docs/HandOver.md`), flash, rồi gửi: (a) log build, gồm cảnh báo của `screen_app.c` và dòng kích thước `board_hmi_ai.bin`; (b) toàn bộ log serial từ đầu (cả phần boot) tới sau dòng `[touch] ...`: các dòng `[touch] init: ...`, `controller answers: chip_id=.. firmware_id=.. vendor_id=..` (hoặc mã lỗi), `touch ready ... lowest free stack`; (c) chạm vào tâm bốn khối màu ở bốn góc, ô trắng giữa, viền và vùng đen, rồi gửi các dòng `[touch] Tab in ...` tương ứng (so x, y với vị trí khối ở nhật ký phiên 16 để chỉnh `BOARD_TOUCH_SWAP_XY`, `BOARD_TOUCH_MIRROR_X`, `BOARD_TOUCH_MIRROR_Y`); (d) cho biết màn hình có còn hiển thị đúng mẫu sau khi touch chạy không, và Wi-Fi, HTTP có còn chạy không (touch dùng I2C và chân GPIO 4 chung với reset của LCD, chưa xác minh). Nếu `controller answers` không hiện mà báo `nobody answered`: cần schematic V15 (trang chân SDA, SCL, RESET, INT của touch) hoặc đo chân.
- (Mới, phiên 15) Build bản có `screen_app` (file đổi: `LoganSDK/app/app.c`, `LoganSDK/app/user/screen_app/screen_app.c`, `screen_app.h`, `docs/HandOver.md`), flash, rồi gửi: (a) log build, gồm cảnh báo của hai file `screen_app.c`, `app.c` và dòng kích thước `board_hmi_ai.bin` (bản phiên 13 là 0xb61f0; lần này `board.o`, `screenHMI`, `sx_i80`, `sx_pwm` sẽ vào bin nên bin lớn hơn); (b) log boot và các dòng `[screen] ...`; (c) mô tả bằng mắt từng bước: màn hình có sáng không, mỗi màu tô toàn màn hình trông ra sao (đối chiếu bảng ở nhật ký phiên 15), mẫu cuối có 4 góc đúng vị trí và viền trắng đủ 4 cạnh không, đen có ra đen không (nếu ra trắng thì cần `BOARD_LCD_INVERT = true`), có nhiễu hay sọc không; (d) board có boot đúng không (chân strapping GPIO 0, 3, 45, 46). Sau đó sửa macro `BOARD_LCD_MADCTL`, `BOARD_LCD_INVERT`, `BOARD_LCD_PCLK_HZ` trong `board.h` theo kết quả, hoặc gửi kết quả để phiên sau sửa.
- (Mới, phiên 15, sau lần quan sát 1) Build lại với `BOARD_LCD_INVERT = true` (file đổi: `LoganSDK/board/board.h`, một dòng), flash, rồi gửi: log build và log boot, thứ tự màu ở bước tô (đỏ, xanh lá, xanh dương, trắng, đen), và **vị trí** từng màu ở bốn góc của mẫu cuối (xem bổ sung 1 của phiên 15). Hướng màn hình chưa xác định vì chưa biết vị trí các góc.
- (Cập nhật sau lần quan sát 2) Việc trên đã xong về mặt hình ảnh (xem bổ sung 2 của phiên 15). Còn lại cho chủ dự án: gửi log serial của lần chạy (các dòng `[screen] ...`, đặc biệt thời gian tô và stack còn trống), báo viền trắng 2 px và ô trắng ở giữa có hiện đủ không, và cho biết có nhiễu hay sọc không.
- (Cập nhật sau log lần 2) Đã nhận log serial và chủ dự án xác nhận viền trắng và ô giữa hiện đủ (xem bổ sung 3 của phiên 15). Còn thiếu: log build (cảnh báo, kích thước `board_hmi_ai.bin`) và phần đầu log boot (log dán bị cắt giữa dòng).
- (Chủ dự án báo build phiên 14 không lỗi, chưa có log: còn cần cảnh báo ở `board.c` và dòng kích thước `board_hmi_ai.bin` so với 0xb61f0 nếu muốn xác nhận bin không đổi.) Build sau phiên 14 (file đổi: `LoganSDK/board/board.h`, `board.c`, `CMakeLists.txt`, `docs/Firmware_Construction.md`, `docs/HandOver.md`) và gửi log. Điểm cần xem: có cảnh báo hay lỗi nào ở `board.c`; `board` thêm `REQUIRES services` có tạo vòng phụ thuộc component không (kỳ vọng không, `services` không `REQUIRES board`); kích thước bin so với 0xb61f0 của phiên 13 (kỳ vọng gần như không đổi vì `app` chưa gọi hàm nào của `board`, linker có thể bỏ `board.o`).
- Bring-up từng thiết bị (việc sau, khi bạn muốn): gọi `board_init()` rồi `board_init_screen()`, thử `screen_hmi_set_backlight` và `screen_hmi_fill_rect` với vài màu đã biết để kiểm tra hướng, thứ tự RGB/BGR, đảo màu và `pclk_hz`; sau đó `board_init_touch()`. Khi một giá trị `DESIGN` sai, sửa macro tương ứng trong `board.h`. Cần cho tôi biết chọn thiết bị nào làm trước và gửi log.
- Chân GPIO 0, 3, 45, 46 (strapping) đang bị LCD dùng; nếu board không boot đúng sau khi `board_init_screen()` chạy, báo ngay kèm log boot.
- (Đã xong sau phiên 14) Build, flash và gửi log `[http] ...` của `http_app`: kết quả `result=0 (ok)`, `status=200`. Router `Log Terminal` ra được Internet và phân giải được tên miền (suy ra từ kết quả, không đo riêng).
- (Build phiên 13 đã xong, xem nhật ký.) Chạy lại `idf.py build` sau bản sửa cảnh báo ở `wifi_app.c` (cuối phiên 13), rồi flash và lặp lại thử rút router 20 giây: kỳ vọng sau khi nối lại dòng trạng thái hiện `reason=0` khi `got_ip`, và `clients=` bằng số thiết bị thật đang nối vào AP (kể cả sau lần AP kick client bằng `reason=209`).
- Chạy `idf.py build` trên máy của mình sau commit `aa0d00c` (lần build đầu có `sx_time`, `screenHMI`, `touch` trên máy chủ dự án). Lưu ý đã gặp ở phiên 6: biến môi trường `IDF_TARGET=esp32` trong shell làm `set-target esp32s3` báo lệch; lệnh `idf.py set-target esp32-s3` chạy được.
- Chạy `idf.py build` sau phiên 8 (file đổi: `sx_i2s.h`, `esp32s3_i2s.c`, `modules/micro/inmp441.h`, `inmp441.c`, `modules/CMakeLists.txt`) và báo lỗi nếu có.
- (Đã xong) `DEFAULT_WIFI_SSID` và `DEFAULT_WIFI_PASSWORD` đã điền thật (commit `21c5737`: `Log Terminal`, mật khẩu 10 ký tự). `DEFAULT_STATIC_IP` là `192.168.4.1`. AP và router không cùng dải mạng.
- (Đã xong ở phiên 12) Chủ dự án đã build, flash và gửi log quét; nguyên nhân là dấu cách cuối SSID. Phiên 13 đã xác nhận `DEFAULT_WIFI_SSID` là `"Log Terminal "` trong repo (commit `4dfca21`) và chủ dự án đã quyết định tắt quét lặp (`APP_WIFI_SCAN_PERIOD_MS` = 0).
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

Quyết định thêm ở phiên 10 (phiên 14: bốn điểm dưới đã chốt, xem từng dòng; điểm còn mở là `app_start()` có gọi `app_init()` hay không và loại `service` tiếp theo):

- `app_start()` có gọi `app_init()` khi năm hàm `*_init` của app còn chưa có thân hàm không (đề xuất: chưa gọi, không động vào các file rỗng; hoặc thêm stub).
- (Đã chốt phiên 14: phương án A) Thêm `services` vào `REQUIRES` của `board` để `board.c` sở hữu `storage_config_t`. Bảng ở `Firmware_Construction.md` mục 2 đã sửa. Cũ: hay để `storage_app` sở hữu.
- (Đã chốt phiên 14) `board.h` tự khai báo số chân có nhãn `REF/DESIGN/GIVEN` và không include header `board_device`.
- (Đã áp phiên 14, theo mặc định tôi nêu và chủ dự án không phản đối) Giá trị chưa có nguồn xác minh (`pclk_hz`, MADCTL, đảo màu, hướng touch, LEDC timer và channel, SPI host, I2C port) dùng giá trị bảo thủ nhãn `DESIGN` (danh sách ở mục 4). Bảng lệnh panel **không viết** (chưa có nguồn): màn hình chạy với thiết lập mặc định của chip, có thể sai gamma, nguồn hoặc màu cho tới khi có nguồn đã xác minh.
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
0b. (Phiên 14: đã viết `board.h`, `board.c`, `board_init()` và `board_init_xxx` từng thiết bị, chờ build của chủ dự án.) Còn lại: `app_start()` chưa gọi `board_init()` (quyết định của chủ dự án, để bring-up từng thiết bị); chưa có schematic V15 nên mọi chân vẫn `REF`, `BOARD_PINMAP_VERIFIED = 0`.
0c. (Phiên 8, chờ chủ dự án build) `sx_i2s` hỗ trợ RX (mic) và sửa timeout. Phần `board.h` và cấu hình `speaker`, `inmp441` ở `board.c` đã làm ở phiên 14 (không nối header `board_device`, khai báo số chân trực tiếp).
1. (Xong ở phiên 5: build OK) `sx_pwm`, `sx_uart`, callback ngắt của `sx_gpio`.
2. Modules: `speaker` đã viết ở phiên 10 (chờ build của chủ dự án). `screenHMI` và `touch` đã viết ở phiên 6; `storage` đã viết ở phiên 6 và nằm ở `services/storage`.
2a. Theo chỉ đạo của chủ dự án (phiên 10): sau `speaker` là viết phần service, rồi mới viết app. Chưa chốt service nào (xem mục 6). `board.h`, `board_init()`, `app_start()` vẫn cần có trước khi bất kỳ instance nào (speaker, mic, màn hình, touch, SD) được tạo và chạy.
2b. `storage_app` (task duy nhất gọi `storage_*`). Phiên 14: `board.c` đã có `sdcard_t`, `sdcard_config_t`, `storage_config_t` và `board_init_storage()`; `storage_app` cần gọi `storage_mount()` (chạy trong task riêng, có thể chặn vài giây khi không có thẻ).
3. Bring-up trên phần cứng (chủ dự án chạy build và flash): xác minh Flash/PSRAM, chân, LCD hiển thị, touch đọc được, SD mount, speaker ra tiếng, thử mic. Phiên 15: đã viết `screen_app` cho bước LCD (chờ build, flash và kết quả quan sát của chủ dự án). Bước tiếp theo sau khi LCD đúng hướng, đúng màu: `board_init_touch()` và đọc `touch_read_id` / `touch_read`, rồi mới tới LVGL. Phiên 16: đã viết bài thử touch trong `screen_app` (chờ build, flash và log của chủ dự án).
4. Wi-Fi (`sx_wifi`) và `services/http` đã viết ở phiên 11; chạy trên phần cứng (HTTPS GET thật thành công, xem phiên 14). Còn lại: LVGL (flush callback, đọc input từ touch), OTA, và phần AI (chờ đặc tả backend). Sau đó theo roadmap trong `Product_Construction.md`.
4b. (Phiên 12: nguyên nhân `reason=201` đã rõ. Phiên 13: thử rút router 20 giây xong, tự nối lại đạt; quét lặp đã tắt.) Còn lại cho Wi-Fi: chủ dự án build và thử lại hai sửa của phiên 13; thử router WPA3 nếu có điều kiện; sau đó sang bước tiếp theo của roadmap (xem 0b, 2a, 4a).
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

### Phiên 12 (2026-11-08)

Đầu phiên: clone repo (HEAD `21c5737`), đọc `HandOver.md`, `RuleCode.md`, `RuleAgent.md`, `Firmware_Construction.md` và đầu `Product_Construction.md`, đối chiếu repo. Nhiệm vụ: tìm hiểu vì sao STA không nối được Wi-Fi (log chủ dự án gửi: `sta=connecting`, `reason=201` lặp lại, kèm hai dòng `Haven't to connect to a suitable AP now!` ở khoảng 123 giây). Chỉ kiểm tra logic rồi present file, chủ dự án tự build.

Lệch phát hiện khi đối chiếu:

- Handover còn ghi `DEFAULT_WIFI_SSID` là `YourSSID` và ghi `sx_wifi`, `wifi_app` chưa chạy trên phần cứng. Thực tế commit `21c5737` đã điền SSID và mật khẩu thật và đã thêm dòng log độ dài SSID/mật khẩu (`wifi_app.c`); log của chủ dự án là lần chạy phần cứng đầu tiên của Wi-Fi. Các dòng này đã sửa ở mục 3 và 6.
- Dòng "App tasks" ở mục 3 còn ghi `app_start()` chưa tồn tại (đã có từ phiên 11). Đã sửa.
- Tên file docs vẫn là `HandOver.md`, `RuleCode.md` (không phải chữ thường như prompt khởi động ghi); điểm lệch này đã có từ phiên 6, chưa sửa.

Phân tích (không phải kết luận):

- `reason=201` là `WIFI_REASON_NO_AP_FOUND` (đối chiếu `esp_wifi_types_generic.h` v5.4.3): lần quét khi nối không thấy mạng khớp SSID. Lỗi dừng ở bước quét nên mật khẩu sai không gây ra nó. Sai sót của phiên này: đã đọc dòng log `(12 chars)` và kết luận "không có khoảng trắng thừa", trong khi con số đó chỉ đo chuỗi trong code, không đo SSID thật của router. Nguyên nhân thật là do cấu hình (xem "Kết quả" bên dưới).
- Chủ dự án cung cấp thông tin router (mục 4): 2.4 GHz, kênh 11, WPA2-Personal, nên băng 5 GHz, kênh 14 và WPA3 thuần đều không phải nguyên nhân. Đọc `esp32s3_wifi.c` không thấy lỗi cấu hình STA. Các khả năng còn lại, **chưa xác minh**: sóng yếu hoặc anten/RF của board, router ẩn SSID, quét bị ảnh hưởng bởi AP chạy cùng lúc trên kênh 1 (APSTA), hoặc lỗi chưa thấy trong code. Các giả thuyết này đều sai: nguyên nhân thật là dấu cách cuối SSID (xem "Kết quả" bên dưới).
- Dòng `sta=linked` kèm `rssi=0` và hai dòng `Haven't to connect to a suitable AP now!` ở khoảng 123 giây chưa giải thích được; thư viện Wi-Fi đóng nguồn, log không đủ.

Đã làm (chủ dự án chọn thêm quét):

- `LoganSDK/components/peripherals/wifi/sx_wifi.h` (sửa): thêm `SX_WIFI_SCAN_MAX_ENTRIES` (16, chọn theo thiết kế), `sx_wifi_auth_t`, `sx_wifi_scan_entry_t` (ssid, kênh, rssi, auth) và hàm `sx_wifi_scan(entries, max_entries, count, total)`: chặn khoảng 2 giây, quét mọi kênh gồm cả mạng ẩn (ssid rỗng), kết quả xếp theo tín hiệu mạnh trước, ký tự ssid ngoài ASCII in được đổi thành `?`. Ghi vào hợp đồng: không gọi chồng với lệnh vòng đời.
- `LoganSDK/platform/esp32s3/wifi/esp32s3_wifi.c` (sửa): hiện thực `sx_wifi_scan`. Đối chiếu header v5.4.3: `esp_wifi_scan_start` trả `ESP_ERR_WIFI_STATE` khi STA đang nối, và `esp_wifi_scan_get_ap_records` giải phóng cả danh sách quét. Nên khi STA đang `connecting` hoặc chưa nhận sự kiện start, hàm dừng timer thử lại, gọi `esp_wifi_disconnect`, đặt cờ `scan_paused`; trong thời gian đó sự kiện ngắt kết nối bị bỏ qua (nên `last_reason` 201 không bị ghi đè bằng 8), sự kiện `STA_START` và timer thử lại không tự nối, rồi nối lại sau khi quét xong. STA đã có link thì quét mà không ngắt. Quét gặp `ESP_ERR_WIFI_STATE` thì thử lại tối đa 5 lần cách 100 ms (chọn theo thiết kế). Lỗi sau khi quét thì gọi `esp_wifi_clear_ap_list`. `map_error` thêm `ESP_ERR_WIFI_TIMEOUT` và `ESP_ERR_WIFI_STATE`. Buffer kết quả là mảng tĩnh (16 bản ghi), không dùng heap của module.
- `LoganSDK/app/user/wifi_app/wifi_app.c`, `wifi_app.h` (sửa): quét một lần ngay sau khi khởi động, rồi lặp mỗi `APP_WIFI_SCAN_PERIOD_MS` (30000, chọn theo thiết kế, đặt 0 để chỉ quét một lần) khi STA chưa có IP. In `[wifi] scan: N networks seen, M listed`, từng mạng `[wifi] scan #i ch=.. rssi=.. auth=.. ssid=".."`, và một dòng nói có thấy `DEFAULT_WIFI_SSID` hay không.
- `docs/Firmware_Construction.md` (sửa): cập nhật dòng `sx_wifi` ở bảng mục 8.
- Không sửa: `board.h`, CMake (không thêm nguồn hay dependency mới; `freertos/task.h` đã có trong `PRIV_REQUIRES freertos`).

Đã kiểm chứng:

- Chạy trên máy host (ASan và UBSan, `-fno-sanitize-recover=all`) `esp32s3_wifi.c` thật với ESP-IDF giả lập viết trong phiên này: 65 check, 0 lỗi. Gồm: tham số và trạng thái, quét ngay sau start với sự kiện `STA_START` và ngắt kết nối đến giữa chừng, giữ `last_reason` 201 khi có sự kiện 8 trong lúc quét và coi sự kiện 8 là bình thường sau đó, timer thử lại bắn trong lúc quét không nối, thử lại khi `ESP_ERR_WIFI_STATE` (thành công ở lần 3, và thất bại sau 5 lần vẫn nối lại), ánh xạ timeout, STA đã có IP không bị ngắt, nhiều mạng hơn số chỗ (tổng được báo đúng, số yêu cầu bị chặn ở 16), làm sạch ssid, mạng ẩn, ssid dài 32 ký tự, lỗi ở các lệnh lấy kết quả có gọi dọn danh sách, chế độ AP-only và driver đã stop trả `SX_WIFI_ERR_STATE`. Biên dịch sạch `-Wall -Wextra -Wshadow -Wconversion -Wsign-conversion -Wpedantic -Wmissing-prototypes -Wstrict-prototypes`. 11 đột biến cố ý vào code production (bỏ bỏ-qua-sự-kiện, bỏ nối lại, chỉ tạm dừng khi `CONNECTING`, bỏ chặn `STA_START`, bỏ làm sạch ssid, bỏ dọn danh sách, bỏ ánh xạ timeout, bỏ dừng timer, chỉ thử một lần, bỏ giới hạn số mục, bỏ chặn timer thử lại): lần đầu 1 đột biến sống sót (chặn timer thử lại, do thiếu ca test), đã bổ sung ca test và cả 11 đều bị bắt.
- Chạy `wifi_task` thật của `wifi_app.c` trên máy host với `sx_wifi_*` giả lập: quét một lần lúc đầu, lặp mỗi 30 giây khi chưa có IP (4 lần trong 120 giây), không quét sau khi có IP, dòng "seen" và "not in the N listed" đúng. Biên dịch sạch cờ nghiêm ngặt. Bản giả lập và test nằm ngoài repo (không commit); giả lập chỉ chứng minh logic khớp mô hình do chính phiên này viết.
- Lỗi tự bắt được khi viết: `sx_get_time_ms` ghi qua con trỏ chứ không trả giá trị (đã sửa bằng hàm `now_ms` trong `wifi_app.c`), và một race: sự kiện `STA_START` đến giữa lúc quét ngay sau `sx_wifi_start` sẽ tự gọi `esp_wifi_connect` và hủy quét (đã xử lý bằng cờ `scan_paused` và coi trạng thái `STOPPED` như đang nối).

Chưa làm hoặc chưa kiểm chứng:

- **Không chạy `idf.py build`** (chủ dự án tự build). Code chưa biên dịch với header ESP-IDF thật; chỉ đối chiếu `esp_wifi.h` và `esp_wifi_types_generic.h` v5.4.3 bằng cách đọc. `wifi_ap_record_t` tĩnh 16 bản ghi: kích thước thật chưa đo. `pdMS_TO_TICKS` và `vTaskDelay` dựa vào hiểu biết, chưa đọc header FreeRTOS của v5.4.3.
- Chưa chạy trên phần cứng: chưa biết quét có thấy `Log Terminal` hay không. Hành vi thật của `esp_wifi_disconnect` khi đang nối (sự kiện sinh ra, lý do mã nào) chưa kiểm chứng; code giả định sự kiện đến với reason 8 và bỏ qua mọi sự kiện ngắt kết nối khi đang tạm dừng, nên nếu sự kiện đến trễ sau khi quét xong thì nó được xử lý như lỗi thường (ghi `last_reason`, hẹn nối lại); hậu quả nhẹ nhưng chưa quan sát.
- Quét mất khoảng 2 giây và rời kênh AP trong lúc đó (chế độ APSTA), nên client của AP có thể rớt thoáng qua; chưa đo thời gian quét thật. Quét lặp mỗi 30 giây cũng hủy lần nối đang chạy, chỉ dùng để chẩn đoán.
- Nếu quét thấy hơn 16 mạng, chỉ 16 mạng mạnh nhất được liệt kê; router yếu có thể không có trong danh sách (log sẽ ghi "not in the N listed of M seen").
- Chưa kiểm tra quốc gia và dải kênh mặc định của driver (không đọc `WIFI_INIT_CONFIG_DEFAULT` hay cấu hình country). Không còn là nghi vấn cho lỗi này: router ở kênh 11 và log cho thấy nối được; quét thấy cả mạng kênh 13.

Kết quả (log chủ dự án gửi sau khi build và flash bản quét; mình không có log build, chỉ có log chạy):

- Quét thấy 22 mạng, liệt kê 16. Mạng đầu danh sách: `ch=11 rssi=-47 auth=wpa/wpa2 ssid="Log Terminal "` với dấu cách trước dấu ngoặc đóng. Dòng tổng kết của app báo `target "Log Terminal" not in the 16 listed of 22 seen` đúng như thiết kế, vì chuỗi cấu hình thiếu dấu cách.
- Nguyên nhân: SSID thật của router là `Log Terminal ` (13 ký tự), cấu hình là `Log Terminal` (12 ký tự). Chủ dự án sửa lại, log sau đó: `ap channel adjust o:1,1 n:11,2`, `auth -> assoc -> run`, `connected with Log Terminal , channel 11, bssid 74:c3:30:1f:b0:ac`, `security: WPA2-PSK`, `sta ip: 192.168.30.102`, và dòng trạng thái `sta=got_ip ip=192.168.30.102 rssi=-40`. Hai dòng `Haven't to connect to a suitable AP now!` trong log cũ vẫn chưa giải thích, nhưng gần như chắc chắn là hệ quả của việc không thấy mạng khớp.
- Điều đã được chạy trên phần cứng nhờ log này: `sx_wifi_init/start`, kết nối STA WPA2 với DHCP, AP chuyển kênh theo router, `sx_wifi_scan`, sự kiện nối và lấy IP, `sx_wifi_get_status`. Bổ sung sau đó (log chủ dự án): điện thoại nối vào AP, nhận IP `192.168.4.2` từ DHCP của board, `clients=1` trong lúc STA vẫn có IP (client vào AP không làm rớt STA ở lần thử này). Chưa chạy: mất kết nối rồi tự nối lại, router WPA3, `sx_wifi_wait_ip`, mọi thứ ở `http`, truy cập dịch vụ nào đó trên board từ điện thoại (chưa có server nào chạy trên board).
- Ký tự ngoài ASCII trong tên mạng bị thay bằng `?` như thiết kế (`T??m T??t`, `Ho??i nam` trong log); đây là hành vi của hàm làm sạch, không phải lỗi.
- Bài học đưa vào tài liệu: dấu cách cuối SSID rất khó thấy; nên in SSID kèm dấu ngoặc kép (đã có trong log của `wifi_app`).

Kế hoạch tiếp theo: xem mục 7 (4b) và mục 6.

### Phiên 13 (2026-11-08)

Đầu phiên: clone repo (HEAD `69fc042`), đọc `HandOver.md`, `RuleCode.md`, `RuleAgent.md`, `Firmware_Construction.md` và đầu `Product_Construction.md`, đối chiếu repo. Nhiệm vụ: phân tích log khi chủ dự án để board đang nối Wi-Fi rồi rút router khoảng 20 giây rồi cắm lại. Chỉ kiểm tra logic rồi present file, chủ dự án tự build.

Lệch phát hiện khi đối chiếu: tên file docs thật là `HandOver.md`, `RuleCode.md` (không phải chữ thường như prompt ghi). Repo khớp handover ở các điểm còn lại: `DEFAULT_WIFI_SSID` đã là `"Log Terminal "` (commit `4dfca21`), `board.h` vẫn là stub, chưa có `board_init()`, chưa có log build cho phiên 10 đến 12.

Phân tích log (đọc từ log chủ dự án gửi, không đo trực tiếp):

- Mất router: `bcn_timeout`, `ap_probe_send over`, `state: run -> init`, trạng thái `connecting` với `reason=200` (`WIFI_REASON_BEACON_TIMEOUT`). Trong lúc mất router `reason=201` (`NO_AP_FOUND`) lặp lại. Mã 200, 201, 209 đã đối chiếu `esp_wifi_types_generic.h` v5.4.3.
- Router cắm lại: STA tự nối lại (`auth -> assoc -> run`), router lúc này ở **kênh 6** (trước đó kênh 11), AP của board tự chuyển theo sang kênh 6, STA nhận IP `192.168.30.100` (trước đó `.102`, do DHCP của router cấp lại). Kết luận: tự nối lại đạt trên phần cứng ở lần thử này. Thời gian từ lúc cắm router đến lúc nối lại không đo được vì log không có thời điểm cắm.
- Quét chẩn đoán chạy ngay khi mất kết nối (vì quy tắc quét mỗi 30 giây khi chưa có IP) và báo `Log Terminal ` không có trong 22 mạng, đúng với lúc router tắt.
- Sau khi AP đổi kênh, AP gửi SA Query cho điện thoại, điện thoại không trả lời 6 lần, AP ngắt nó với `reason=209` (`SA_QUERY_TIMEOUT`), điện thoại nối lại ngay sau đó. Đây là hệ quả của AP đổi kênh theo router (đã ghi ở phiên 11), không sửa.

Hai điểm sai hiển thị tìm ra từ log (chủ dự án đã duyệt cách sửa):

1. `sta_last_reason` còn `201` dù đã `got_ip` (lần nối đầu là `0`). Nguyên nhân: giá trị chỉ được ghi khi ngắt kết nối và không bao giờ xóa. Cách A đã chọn: xóa về 0 khi nhận IP.
2. `ap_clients` đếm thiếu: sau lần điện thoại bị kick (`reason=209`) log hiện `clients=0` dù còn một thiết bị, và cuối cùng điện thoại đã nối lại mà vẫn `clients=0`. Bộ đếm cộng trừ theo sự kiện bị lệch; **sự kiện nào bị lặp hoặc mất thì log không cho biết (chưa xác minh)**. Cách sửa: bỏ bộ đếm, đọc số thật từ driver.

Đã làm (sửa trong repo clone, present từng file):

- `LoganSDK/platform/esp32s3/wifi/esp32s3_wifi.c` (sửa): `handle_got_ip` đặt `last_reason = 0`. Bỏ trường `ap_clients` và mọi chỗ cộng, trừ, xóa nó (`handle_ap_client`, `WIFI_EVENT_AP_STOP`, `sx_wifi_start`, `sx_wifi_stop`). Thêm hàm `count_ap_clients` gọi `esp_wifi_ap_get_sta_list`, trả 0 khi chưa chạy, mode không có AP, hoặc driver báo lỗi; `sx_wifi_get_status` dùng hàm này. Callback sự kiện `AP_CLIENT_JOINED/LEFT` vẫn được gọi như cũ.
- `LoganSDK/components/peripherals/wifi/sx_wifi.h` (sửa, chỉ đổi comment của `sta_last_reason` và `ap_clients`).
- `LoganSDK/app/user/wifi_app/wifi_app.h` (sửa): `APP_WIFI_SCAN_PERIOD_MS` từ 30000 xuống 0 theo quyết định của chủ dự án (chỉ quét một lần lúc khởi động).
- `docs/Firmware_Construction.md` (sửa): cập nhật dòng `sx_wifi` ở bảng mục 8.
- Không sửa `wifi_app.c`, `board.h`, CMake. Không thêm dependency.

Đã kiểm chứng:

- Đọc header ESP-IDF v5.4.3 (tải từ github): `esp_wifi.h` có `esp_wifi_ap_get_sta_list(wifi_sta_list_t *sta)` và mã lỗi của nó; `esp_wifi_types_native.h` (thư mục `include/local`) định nghĩa `wifi_sta_list_t` gồm `sta[ESP_WIFI_MAX_CONN_NUM]` (15 trên ESP32-S3) và `int num`. Hàm `esp_wifi_ap_get_sta_list` nằm trong `esp_wifi.h`, nên không cần include mới.
- Review logic bằng mắt trên diff: không còn chỗ nào dùng `ap_clients` ngoài `sx_wifi_status_t` và `count_ap_clients`; comment chỉ có ASCII (đã kiểm tra bằng lệnh).

Chưa làm hoặc chưa kiểm chứng:

- **Không chạy `idf.py build`, không biên dịch bằng bất kỳ compiler nào, không chạy giả lập host** (theo yêu cầu phiên này). Chưa chạy trên phần cứng.
- `wifi_sta_list_t` nằm trên stack (khoảng 250 byte, chưa đo `sizeof`). Nếu `sx_wifi_get_status` bị gọi từ callback trong task sự kiện hệ thống (stack 2304 byte) thì cần để ý.
- Chưa biết driver cập nhật danh sách client trước hay sau khi phát sự kiện `AP_STADISCONNECTED`; nếu người gọi đọc `ap_clients` ngay trong callback thì có thể thấy số cũ (chưa xác minh).
- Khi STA đã `linked` nhưng chưa có IP, dòng trạng thái vẫn có thể hiện `reason` cũ trong khoảng ngắn (chỉ xóa khi nhận IP).
- Mã nguồn `wifi_app.c` không đổi, nhưng với `APP_WIFI_SCAN_PERIOD_MS = 0` nhánh quét lặp trở thành mã không bao giờ chạy; chưa xem cảnh báo của compiler về việc này.
- Chưa thử router WPA3; chưa đo thời gian nối lại sau khi cắm router.

Kế hoạch tiếp theo: xem mục 6 và mục 7 (4b).

Bổ sung cuối phiên 13 (sau khi chủ dự án build):

- Chủ dự án build bản sửa (gửi phần cuối log, không phải toàn bộ): **build thành công**. `board_hmi_ai.bin` 0xb61f0 byte, phân vùng app nhỏ nhất 0x100000 byte, còn trống 0x49e10 byte (29%). Bootloader 0x51c0 byte. Bin lớn hơn nhiều so với 0x2e050 ở phiên 6 vì đã có Wi-Fi, lwIP, NVS; `http_svc_perform` vẫn chưa ai gọi nên chi phí HTTP và TLS chưa hiện trong con số này.
- Log chỉ có một cảnh báo thấy được: `wifi_app.c:147: comparison of unsigned expression in '>= 0' is always true [-Wtype-limits]`. Nguyên nhân là `APP_WIFI_SCAN_PERIOD_MS` bằng 0 (đúng như dự đoán ở mục "Chưa làm"). Vì chỉ có phần cuối log, chưa biết còn cảnh báo nào khác trong các file khác hay không, kể cả `esp32s3_wifi.c`.
- Đã sửa: `LoganSDK/app/user/wifi_app/wifi_app.c` (sửa): khối quét lặp, biến `last_scan_ms` và hàm `now_ms` (chỉ dùng cho quét lặp, nếu để lại sẽ báo unused function) nằm trong `#if APP_WIFI_SCAN_PERIOD_MS != 0U`, nên khi đặt 0 thì đoạn so sánh không còn được biên dịch. Bản sửa này **chưa build, chưa chạy**; chưa kiểm tra bằng mắt nào ngoài đọc lại diff.

#### Việc thứ hai trong phiên 13: http_app và làm rõ board_device

Repo: trước khi bắt đầu, pull thêm hai commit của chủ dự án (`d79a133` và `1c70566`, chính là các file phiên 13 đã present, gồm cả bản sửa cảnh báo của `wifi_app.c`). Không có lệch nào khác.

Xác nhận từ chủ dự án (GIVEN, theo lời chủ dự án): các chân trong `LoganSDK/board/board_device/ZX3D50CE08S-V15-USRC.h` lấy từ **tài liệu chung của dòng WT32-SC01 Plus, không phải schematic V15**. Chủ dự án nhận định phần cứng giống nhau giữa các bản; đó là nhận định chưa kiểm chứng, nên các chân vẫn mang nhãn `REF` (chưa xác minh trên V15), cờ `BOARD_PINMAP_VERIFIED` (khi có) giữ 0. Hiện trạng đọc từ repo: header đó định nghĩa đủ chân LCD, touch, SD, loa, RS485 và 3 chân mic (GPIO 10, 11, 12), nhưng **không file nào include nó**, nó dùng macro `GPIO_10`, `GPIO_11`... chưa định nghĩa ở đâu, và `board.h` vẫn chỉ có enum. Việc còn lại của mục 0b là nối chân vào code (`board.h`, `board_init()`), không phải thiếu chân.

Đã làm (theo thỏa thuận "bắt đầu việc 1: thử HTTP/HTTPS thật"; mọi file nằm trong `LoganSDK/app/`):

- `user/http_app/http_app.h` (sửa file có sẵn): bỏ khai báo `http_app_init` (file `.c` rỗng, không ai gọi), thêm `http_app_start()` và các macro cấu hình: tên task, stack 8192 byte, ưu tiên 2, `APP_HTTP_TEST_URL` mặc định `https://example.com/`, bộ đệm thân 2048 byte, chu kỳ chờ IP 1000 ms. Stack, ưu tiên và URL do phiên này chọn, chưa đo. URL là lựa chọn của phiên này cho việc thử, không phải backend sản phẩm.
- `user/http_app/http_app.c` (viết vào file rỗng): task chờ `sx_wifi_get_status` báo `GOT_IP` (hỏi mỗi giây, nên thứ tự gọi với `wifi_app_start` không quan trọng), gửi một GET qua `http_svc_perform`, in `result`, `status`, số byte thân, thời gian (ms), heap trống trước và sau, mức heap trống thấp nhất từ lúc boot trước và sau (để thấy đỉnh RAM do TLS), stack còn trống thấp nhất, rồi 60 ký tự đầu thân (ký tự không in được đổi thành dấu chấm), sau đó tự kết thúc. Buffer thân là mảng tĩnh. Chỉ include FreeRTOS, `sx_wifi.h`, `sx_time.h`, `http.h` (không include header driver ESP-IDF).
- `app.c` (sửa): `app_start()` gọi thêm `http_app_start()` sau `wifi_app_start()`. `app.h` (sửa): chỉ đổi comment.
- Không đổi CMake (`http_app.c` và thư mục đã có trong `app/CMakeLists.txt`, `services` đã có `esp_http_client` và `esp-tls`). Không thêm dependency.

Đã kiểm chứng:

- Đọc ESP-IDF v5.4.3 (tải từ github): `portable.h` khai báo `xPortGetFreeHeapSize`, `xPortGetMinimumEverFreeHeapSize`; `FreeRTOSConfig.h` có `INCLUDE_uxTaskGetStackHighWaterMark 1`; `StackType_t` của cổng Xtensa là `uint8_t` và `idf_freertos` ghi kích thước stack tính bằng byte, nên số stack in ra là byte.
- Chạy trên máy host (ASan, UBSan, biên dịch sạch `-Wall -Wextra -Wshadow -Wconversion -Wsign-conversion -Wpedantic -Werror -Wmissing-prototypes -Wstrict-prototypes`) với FreeRTOS, `sx_wifi` và `http_svc_perform` giả lập: chờ qua trạng thái lỗi init, `connecting`, `linked` rồi mới gửi (6 lần hỏi, 5 lần chờ); yêu cầu đúng GET, đúng URL, buffer 2048, không thân, không header, timeout 0; lỗi mạng không in thân; tràn bộ đệm vẫn in phần thân đầu; thân dài bị cắt còn 60 ký tự; ký tự không in được đổi thành dấu chấm; tạo task thất bại trả -1 và không gọi HTTP. Giả lập chỉ chứng minh logic khớp mô hình do chính phiên này viết. Bài test nằm ngoài repo (không commit).

Chưa làm hoặc chưa kiểm chứng:

- **Chưa `idf.py build`** với `http_app` mới (host dùng header giả của FreeRTOS), chưa flash, chưa gọi server thật. Nhánh in dòng `still waiting` (sau 10 lần hỏi) chưa được test.
- Chưa biết stack 8192 byte có đủ cho TLS (task in số còn trống để chỉnh), chưa biết RAM nội cần bao nhiêu, chưa biết kích thước bin sau khi `http_svc_perform` được gọi (chi phí HTTP và TLS lần đầu hiện trong bin), chưa biết bundle CA FULL có gây vấn đề không.
- `example.com` có trả `Content-Length` hoặc chunked không là điều chưa xác minh; nếu không, `http_svc_perform` báo `HTTP_SVC_ERR_NET` (cố ý, xem `http.h`). Chưa biết router `Log Terminal` có ra Internet không.
- Chưa có thử HTTP thường, thử redirect, thử chứng chỉ sai, thử mất Wi-Fi giữa lúc đang gọi.
- Chỉ gọi một lần sau khi có IP: nếu mất Wi-Fi rồi nối lại thì không gọi lại; chưa đo rò rỉ heap qua nhiều lần gọi.

### Phiên 14 (2026-11-08)

Đầu phiên: clone repo (HEAD `c0d663b`), đọc `HandOver.md`, `RuleCode.md`, `RuleAgent.md`, `Firmware_Construction.md` và đầu `Product_Construction.md`, đối chiếu repo. Nhiệm vụ: bước 0b (`board.h`, `board.c`, `board_init()`). Theo yêu cầu phiên này: chỉ kiểm tra logic rồi present file, chủ dự án tự build.

Lệch phát hiện khi đối chiếu: tên file docs thật là `HandOver.md`, `RuleCode.md` (không phải chữ thường như prompt ghi); thư mục chip là `platform/esp32s3` (số ít) trong khi `Firmware_Construction.md` ghi `platforms/` (đã ghi từ trước, chưa sửa). Repo khớp handover ở các điểm còn lại: `http_app` đã có, `app_start()` gọi `wifi_app_start()` rồi `http_app_start()`, `board.h` còn là stub, `board.c` chỉ có `#include`, `ZX3D50CE08S-V15-USRC.c` rỗng, header `board_device` không ai include và dùng macro `GPIO_xx` chưa định nghĩa. Chưa có log build và log chạy của `http_app`.

Quyết định đã chốt với chủ dự án đầu phiên:

1. `board.h` tự khai báo số chân bằng số nguyên với nhãn `REF/DESIGN/GIVEN`, không include và không sửa header `board_device`.
2. `board_init()` chỉ chuẩn bị cấu hình, không init phần cứng; `app_start()` chưa gọi nó.
3. Phương án A: `board` có `REQUIRES services` để `board.c` giữ `storage_config_t` (hợp lệ theo sơ đồ layer vì `board` ở L4 gọi xuống L3).
4. Giá trị chưa có nguồn dùng mặc định bảo thủ nhãn `DESIGN` (tôi nêu mặc định, chủ dự án không phản đối); không đổi `sdkconfig`.

Đã làm (present từng file, đường dẫn trong repo):

- `LoganSDK/board/board.h` (sửa, giữ nguyên enum `BoardType_t` của chủ dự án): macro chân và giá trị cấu hình có nhãn, `BOARD_PINMAP_VERIFIED = 0`, mã trả về `BOARD_OK` và `BOARD_ERR_STATE` (-100, các mã âm khác là mã của module lỗi, giữ nguyên), khai báo `board_init`, `board_init_screen/touch/speaker/mic/storage`, `board_get_screen/touch/speaker/mic`.
- `LoganSDK/board/board.c` (sửa, trước đó chỉ có `#include`): instance tĩnh, cấu hình `const` cho LCD (bus I80 8 bit, reset, backlight PWM), touch (I2C, reset dùng chung nên `reset = NULL`), thẻ SD (SPI host 1, CS mềm, dummy 0xFF, mode 0), `storage_config_t` (không format thẻ), speaker (I2S port 0), mic (I2S port 1); các hàm init và getter. `board_init_touch()` trả `BOARD_ERR_STATE` nếu màn hình chưa init. Khi init một thiết bị lỗi, cờ giữ false và lỗi của module được trả nguyên, nên có thể gọi lại.
- `LoganSDK/board/CMakeLists.txt` (sửa): `REQUIRES peripherals modules services`, cập nhật comment.
- `docs/Firmware_Construction.md` (sửa một dòng): bảng mục 2, dòng `board` thêm `services`.
- `docs/HandOver.md` (sửa): bản này.
- Không sửa `app.c` (không gọi `board_init`), `sdkconfig`, `ZX3D50CE08S-V15-USRC.h`, các module, `libs`. Không thêm dependency ngoài `services` đã nêu.

Đã kiểm chứng:

- Đọc header ESP-IDF v5.4.3 (tải từ github): `spi_types.h` có `SPI2_HOST=1`, `SPI3_HOST=2`; `soc_caps.h` của ESP32-S3 có `SOC_I2C_NUM=2`, `SOC_I2S_NUM=2`, `SOC_LEDC_TIMER_NUM=4`, `SOC_LEDC_CHANNEL_NUM=8`. Đọc code platform trong repo: `esp32s3_spi.c` chỉ nhận host từ `SPI2_HOST` trở lên; `esp32s3_i80.c` bắt buộc `pclk_hz`, `max_transfer_bytes`, `queue_depth` khác 0 và chỉ đọc `bus_width` chân dữ liệu đầu; `screenHMI.c` cần `max_transfer_bytes` đủ cho một hàng (480 x 2 byte).
- Biên dịch thử trên máy host (gcc 13.3, **không phải** GCC cho ESP32-S3, **không phải** `idf.py build`): `board.c` chạy `-fsyntax-only` với `-Wall -Wextra -Wshadow -Wconversion -Wsign-conversion -Wpedantic -Wmissing-prototypes -Wstrict-prototypes -Werror`, sạch. `board.h` include riêng được, cả từ C++.
- Chạy `board.c` thật trên host (ASan, UBSan) với module giả lập: trước `board_init` mọi init trả `BOARD_ERR_STATE` và không chạm thiết bị; `board_init` không gọi thiết bị nào; touch bị từ chối khi màn hình chưa up; lỗi module được trả nguyên và cờ không lên; gọi lại sau lỗi chạy được; gọi hai lần bị từ chối; getter trả NULL tới khi init thành công; nội dung cấu hình khớp macro (thứ tự chân dữ liệu, CS LCD = -1, backlight duty 0, MADCTL 0x28, touch `reset = NULL`, SD dummy 0xFF mode 0 CS 41, `format_if_mount_failed = false`, I2S hai port khác nhau); không có GPIO nào bị dùng hai lần trong 24 chân đã khai báo. 13 đột biến đã thử: 2 đột biến đầu (cờ lên khi speaker init lỗi, đảo màu) sống sót vì thiếu ca test, đã bổ sung test và tất cả bị bắt. Giả lập chỉ chứng minh logic của `board.c`, không chứng minh module hay phần cứng.
- Kiểm tra bằng lệnh: `board.h`, `board.c`, `CMakeLists.txt` của board chỉ có ASCII; không có ký tự trang trí hay thẻ ngoặc vuông trong comment.

Chưa làm hoặc chưa kiểm chứng:

- **Không chạy `idf.py build`, không biên dịch bằng GCC cho ESP32-S3, không chạy trên phần cứng.** Chưa biết `board` thêm `REQUIRES services` có gây vấn đề ở cấu hình CMake của ESP-IDF không (không có vòng phụ thuộc theo `CMakeLists.txt` hiện tại, chưa xác minh bằng build).
- Mọi giá trị `DESIGN` (mục 4) chưa kiểm chứng, đặc biệt: `pclk_hz` 10 MHz, MADCTL, đảo màu, `swap_xy` của touch, không bật pull-up I2C, SPI host và xung của SD. Chân vẫn `REF`, `BOARD_PINMAP_VERIFIED = 0`. Trình tự reset chung LCD/touch ở GPIO4 chưa xác minh.
- Chưa đo RAM tĩnh thêm do instance trong `board.c` (`screen_hmi_t` riêng đã có buffer fill 1920 pixel, khoảng 3.8 KB), chưa biết `max_transfer_bytes` 30720 có làm driver I80 cấp thêm RAM DMA nhiều không (chưa đọc kỹ code ESP-IDF phần này). Vì `app` chưa gọi hàm nào của `board`, bin có thể không đổi (chưa kiểm).
- `board_init_screen()` để backlight tắt: màn hình sẽ tối cho tới khi gọi `screen_hmi_set_backlight`. Chưa có app nào gọi.
- Chưa có app nào gọi `board_init_xxx`; `storage_app`, `screen_app`, `speaker_app` vẫn rỗng. Không có ai gọi `storage_mount`.
- Việc cũ vẫn chờ: log build và chạy của `http_app` (phiên 13), build của các phiên 8 đến 11 còn lại nếu chưa gửi log.

Kế hoạch tiếp theo: xem mục 6 và mục 7. Gợi ý thứ tự: (1) chủ dự án build và gửi log phiên 14; (2) gửi log `[http] ...` để chốt `http_app`; (3) bring-up màn hình qua `board_init_screen()` và `screen_hmi_fill_rect`, từng thiết bị một, mỗi bước build trước khi sang bước sau.

Bổ sung phiên 14 (chủ dự án gửi log chạy `http_app` của bản phiên 13, **không phải** bản có `board.h` mới của phiên 14; log không có thời điểm tuyệt đối, chỉ có tick của ESP-IDF):

- Đọc từ log (không đo trực tiếp bằng thiết bị khác): quét thấy `Log Terminal ` ở kênh 1, rssi -36, `wpa/wpa2`; STA nối kênh 1, WPA2-PSK, nhận IP `192.168.30.100`, AP của board theo router sang kênh 1. Router đã đổi kênh qua các lần ghi nhận (11 ở phiên 12, 6 và 11 ở phiên 13, 1 ở lần này), nhất quán với việc router tự chọn kênh (chưa xác minh cài đặt của router).
- Lần nối đầu có `reason=205` (`WIFI_REASON_CONNECTION_FAIL`, đã đối chiếu `esp_wifi_types_generic.h` v5.4.3) trong trạng thái `connecting`, rồi lần thử kế tiếp nối được (`auth -> assoc -> run`, `connected ... channel 1`). Tự thử lại đã hoạt động. Nguyên nhân của `205` lần này chưa xác minh (log không nói).
- `http_app`: in `still waiting for a Wi-Fi IP` đúng một lần trước khi có IP (nhánh này đã chạy thật, trước đó chỉ test trên host). Sau khi có IP: `GET https://example.com/` cho `result=0 (ok)`, `status=200`, thân 577 byte, **2479 ms** (gồm tra tên miền, nối TCP và bắt tay TLS; chưa tách được từng phần), có dòng `Certificate validated` của `esp-x509-crt-bundle` nên bundle CA đang hoạt động. Thân bắt đầu bằng `<!doctype html>`, nên `example.com` trả được thân có độ dài xác định hoặc chunked (không biết kiểu nào).
- Số đo của task: heap trống trước 257980 byte, sau 256984 byte; mức heap trống thấp nhất từ lúc boot là 255272 trước và 215620 sau. Suy ra đỉnh RAM của một lần gọi HTTPS khoảng 42 KB so với lúc trước gọi (257980 trừ 215620 bằng 42360 byte, hàm đo của FreeRTOS trả tổng heap, không tách RAM nội và PSRAM). Stack còn trống thấp nhất 4620 trên 8192 byte, tức đã dùng tối đa khoảng 3.5 KB trong lần gọi này.
- Chưa kết luận được: heap giảm 996 byte sau một lần gọi có phải rò rỉ hay chỉ là bộ nhớ đệm một lần, vì chỉ có một mẫu. Cần gọi lặp nhiều lần để biết. Stack 8192 còn dư khoảng 4.6 KB cho đúng một yêu cầu tới `example.com`; chưa đổi vì backend thật có thể cần nhiều hơn (chứng chỉ và chuỗi dài hơn).
- Việc tên SSID `Ho??i nam` hiện dấu hỏi là do log in byte không phải ASCII thành `?` (hành vi thiết kế của phần in), không phải lỗi quét.
- Chưa có log build đầy đủ cho bản này; kích thước bin sau khi `http_svc_perform` được gọi (chi phí HTTP và TLS) vẫn chưa biết.
- Việc kế tiếp theo kế hoạch: build bản phiên 14 (`board`) và gửi log; quyết định có gọi `http` lặp để đo heap hay chuyển sang bring-up màn hình. `AI_backend` vẫn chờ đặc tả backend.
- Cuối phiên 14: chủ dự án báo bằng lời đã build bản có `board.h` và `board.c` mới và **không có lỗi**. Không có log, nên không ghi được số cảnh báo hay kích thước bin. Chưa flash bản này (chưa biết), `app_start()` không gọi `board`, nên hành vi chạy kỳ vọng như bản phiên 13.

### Phiên 15 (2026-11-08)

Đầu phiên: clone repo (HEAD `f3c7bdc`), đọc `HandOver.md`, `RuleAgent.md`, `RuleCode.md`, `Firmware_Construction.md` và phần liên quan của `Product_Construction.md`, đối chiếu repo. Nhiệm vụ: bring-up màn hình bằng `screen_app`. Theo yêu cầu phiên này: chỉ kiểm tra logic rồi present file, chủ dự án tự build.

Lệch phát hiện khi đối chiếu: không có lệch về code. Hai commit sau `c0d663b` (`d30f4e8`, `f3c7bdc`) đúng là các file phiên 14 (`board.h`, `board.c`, `board/CMakeLists.txt`, `Firmware_Construction.md`, `HandOver.md`). Chỉ còn lệch tên file docs đã biết (`HandOver.md`, `RuleCode.md`). `screen_app.c` và `screen_app.h` là file 0 byte và đã có sẵn trong `app/CMakeLists.txt` (SRCS và INCLUDE_DIRS), nên không cần sửa CMake. Chưa có log build của phiên 14 (chỉ có lời báo của chủ dự án).

Đã chốt với chủ dự án đầu phiên: mỗi màu giữ 1,5 s, backlight 100%, mẫu cố định ở cuối rồi giữ nguyên trên màn hình, task tự kết thúc.

Đã làm (present từng file, đường dẫn trong repo):

- `LoganSDK/app/user/screen_app/screen_app.h` (viết vào file rỗng): macro cấu hình `APP_SCREEN_TASK_NAME`, `APP_SCREEN_STACK_SIZE` (6144 byte), `APP_SCREEN_TASK_PRIORITY` (2), `APP_SCREEN_BACKLIGHT_PERCENT` (100), `APP_SCREEN_HOLD_MS` (1500), khai báo `screen_app_start()`. Stack, ưu tiên, độ sáng, thời gian giữ đều do phiên này chọn, chưa đo.
- `LoganSDK/app/user/screen_app/screen_app.c` (viết vào file rỗng): task `app_screen_task`. Trình tự: `board_init()`, `board_init_screen()`, `board_get_screen()`, in `size`, `madctl`, `invert`, `pclk` lấy từ macro của `board.h`, `screen_hmi_set_backlight(100)`, tô toàn màn hình lần lượt đỏ (0xF800), xanh lá (0x07E0), xanh dương (0x001F), trắng (0xFFFF), đen (0x0000), mỗi màu giữ 1,5 s, rồi vẽ mẫu cố định gồm nền đen, viền trắng 2 px, khối 120x80 đỏ ở góc trên trái, xanh lá ở trên phải, xanh dương ở dưới trái, vàng (0xFFE0) ở dưới phải, ô trắng 40x40 ở giữa (kích thước màn hình lấy từ `BOARD_LCD_WIDTH` và `BOARD_LCD_HEIGHT`). Mỗi lần tô in tọa độ, giá trị RGB565 và thời gian (ms, đo bằng `sx_get_time_ms`). Cuối cùng in stack còn trống thấp nhất (`uxTaskGetStackHighWaterMark`) rồi `vTaskDelete`. Mọi lỗi (init, backlight, tô) in mã rồi dừng task. Không sửa `board.h`: chủ dự án chỉnh các macro `DESIGN` khi có kết quả.
- `LoganSDK/app/app.c` (sửa): thêm `#include "screen_app.h"` và gọi `screen_app_start()` sau `http_app_start()`. `wifi_app` và `http_app` giữ nguyên. Chưa sửa comment cũ trong `app.h` ("for now it only starts Wi-Fi", đã lỗi thời), vì chưa cần thiết.
- Không đổi CMake, không thêm dependency, không đụng `board.h`, `board.c`, `screenHMI`.

Bảng đối chiếu màu khi quan sát (suy từ phép tính RGB565 và thứ tự byte của panel, **chưa kiểm chứng trên board**; giả định panel đọc từ ghép byte cao trước):

| Thấy lần lượt khi tô đỏ, xanh lá, xanh dương | Nghĩa là |
|---|---|
| đỏ, xanh lá, xanh dương | thứ tự byte và thứ tự màu đúng |
| xanh dương, xanh lá, đỏ | bit BGR của MADCTL sai (đổi `SCREEN_HMI_MADCTL_BGR` trong `BOARD_LCD_MADCTL`) |
| xanh dương, đỏ, xanh lá | thứ tự byte sai (`swap_color_bytes` trong `board.c`) |
| đỏ, xanh dương, xanh lá | sai cả hai |

Hướng màn hình: so 4 góc của mẫu cuối với danh sách trên (trên trái đỏ, trên phải xanh lá, dưới trái xanh dương, dưới phải vàng). Lật hay xoay thì đổi bit `MX`, `MY`, `MV` trong `BOARD_LCD_MADCTL`. Đảo màu: đen phải ra đen, trắng phải ra trắng, nếu ngược lại thì đặt `BOARD_LCD_INVERT = true`. `pclk_hz`: sửa `BOARD_LCD_PCLK_HZ` rồi build lại, quan sát nhiễu, sọc hoặc lỗi trả về, so thời gian tô ghi trong log.

Đã kiểm chứng:

- Biên dịch thử trên máy host (gcc 13.3, **không phải** GCC cho ESP32-S3, **không phải** `idf.py build`) với `-Wall -Wextra -Wshadow -Wconversion -Wsign-conversion -Wpedantic -Wmissing-prototypes -Wstrict-prototypes -Werror`: `screen_app.c` và `app.c` sạch, dùng header thật của dự án (`board.h`, `screenHMI.h` và các header `sx_*`) và header FreeRTOS giả.
- Chạy logic trên host (ASan, UBSan) với `board_*`, `screen_hmi_*`, `sx_time`, FreeRTOS giả lập, trong đó `fill_rect` giả lập kiểm tra cùng quy tắc vùng hợp lệ với module thật: tham số task (stack 6144, ưu tiên 2), tạo task lỗi trả -1; đúng thứ tự gọi `board_init`, `board_init_screen`, backlight 100, năm màu đúng giá trị và đúng thứ tự, tổng thời gian giữ 7500 ms, 10 hình chữ nhật của mẫu; mọi pixel của khung hình cuối được ghi và các góc, mép khối, ô giữa đúng vị trí; các đường lỗi (init lỗi, `board_init_screen` lỗi, getter trả NULL, backlight lỗi, tô lỗi ở màu thứ ba, tô lỗi giữa mẫu) đều dừng đúng chỗ, không gọi các bước sau và có kết thúc task. 7 đột biến cố ý vào code production (đổi đỏ thành xanh dương, đổi bề rộng viền, đổi độ sáng, bỏ kiểm tra NULL, bỏ thời gian giữ, bỏ mẫu cuối, lệch khối trên phải) đều bị bắt. Bài test và bản giả lập nằm ngoài repo (không commit); giả lập chỉ chứng minh logic khớp mô hình do chính phiên này viết.
- Kiểm tra bằng lệnh: ba file code chỉ có ký tự ASCII.
- Đọc code platform trong repo: `esp32s3_i80.c` truyền `swap_color_bytes` vào `flags.swap_color_bytes` của `esp_lcd_panel_io_i80_config_t`; không tìm hiểu thêm hành vi phần cứng của cờ này (phiên này không mở header ESP-IDF, `screen_app` không include header driver).

Chưa làm hoặc chưa kiểm chứng:

- **Không chạy `idf.py build`, không biên dịch bằng GCC cho ESP32-S3, không chạy trên phần cứng.** `uxTaskGetStackHighWaterMark` và `vTaskDelete` đã được dùng ở `http_app` và `wifi_app` (đã build và chạy), nhưng `screen_app.c` chưa biên dịch với header FreeRTOS thật.
- `board_init_screen()` chưa từng chạy trên phần cứng: chưa biết bus I80 lên được không, chưa biết GPIO strapping 0, 3, 45, 46 có làm board boot sai không, chưa biết `max_transfer_bytes` 30720 làm driver I80 cấp thêm bao nhiêu RAM DMA (chưa đo), chưa biết stack 6144 có đủ cho khởi tạo bus I80 và chuỗi init panel (task in số còn trống để chỉnh).
- Không có bảng lệnh riêng của panel (nguồn, gamma, mở khóa lệnh mở rộng): màn hình chạy với thiết lập mặc định của chip nên màu có thể nhạt hoặc sai gamma dù đã đúng thứ tự RGB. Trình tự reset chung LCD/touch (GPIO4) chưa xác minh.
- `screen_hmi_fill_rect` chờ theo bước 1 ms bằng busy wait (giữ CPU); mỗi lần tô toàn màn hình gồm khoảng 80 khối 4 hàng. Giữa các màu có `sx_delay_ms(1500)` nhường CPU nên task wifi và http không bị đói, nhưng chưa quan sát thực tế khi cả ba task chạy cùng lúc. Chưa đo thời gian tô thật (log sẽ cho số này).
- Backlight bật trước khi tô màu đầu tiên, nên vài trăm ms đầu màn hình có thể hiện nội dung bộ nhớ khung chưa xác định (nhiễu, trắng, hay đen); đây cũng là thông tin chẩn đoán (panel đã được cấp nguồn hay chưa), nên giữ nguyên thứ tự này.
- Touch, loa, mic, SD chưa được gọi, `board_init_touch()` chưa chạy.

Kế hoạch tiếp theo: xem mục 6 (việc đầu tiên của chủ dự án) và mục 7 (bước 3).

### Phiên 15, bổ sung 1 (2026-11-08): kết quả quan sát lần 1 và sửa `BOARD_LCD_INVERT`

Chủ dự án báo bằng lời (chưa có log serial, chưa có log build) là đã build, nạp bản có `screen_app` và màn hình sáng. Quan sát được:

- Các lần tô toàn màn hình, theo thứ tự thời gian: xanh dương, tím, vàng, đen, trắng. Code tô theo thứ tự: đỏ, xanh lá, xanh dương, trắng, đen.
- Mẫu cuối: nền trắng, bốn khối góc có màu xanh dương đậm (navy), xanh dương nhạt (cyan), vàng, tím (chưa báo vị trí từng khối), ô giữa màu đen. Chưa báo về viền 2 px.

Kết luận từ quan sát (suy luận, chưa chạy lại trên board):

- Màn hình đang hiện **đảo màu**. Trắng ra đen và đen ra trắng ở cả hai phần (các lần tô và mẫu cuối: nền đen thành trắng, ô giữa trắng thành đen). Trắng và đen không phụ thuộc thứ tự byte hay thứ tự màu, nên kết luận này không phụ thuộc vào các giả định khác.
- Đã đọc code: `screenHMI.c` dòng 161 gửi `CMD_INVOFF` (0x20) khi `invert = false` và `CMD_INVON` (0x21) khi `invert = true`. Lúc quan sát `BOARD_LCD_INVERT = false`, tức panel hiện đảo màu dù đã gửi lệnh tắt đảo, nên cần gửi lệnh bật đảo.
- Giả thuyết "đảo màu, thứ tự byte và thứ tự màu đều đúng" cho kết quả: đỏ ra lục lam (cyan), xanh lá ra tím đỏ (magenta), xanh dương ra vàng, trắng ra đen, đen ra trắng. Bốn khối góc: trên trái cyan, trên phải magenta, dưới trái vàng, dưới phải xanh dương thuần. Bốn khối góc quan sát (navy, cyan, vàng, tím) đúng là tập màu này (xanh dương thuần có thể được mô tả là navy). Ba trong bốn lần tô đầu (xanh lá, xanh dương, và hai màu trắng và đen) khớp đúng. Lần tô đầu (đỏ) được báo "xanh dương" trong khi giả thuyết cho cyan: **chưa rõ**, có thể là cyan được gọi là xanh dương. Lần chạy sau sẽ cho thấy.
- Các giả thuyết khác (có đảo màu và thêm sai thứ tự byte, sai bit BGR hoặc sai cả hai) tính ra tập màu không khớp quan sát. Các phép tính này dùng RGB565, chưa kiểm chứng trên board.

Đã sửa: `LoganSDK/board/board.h` dòng `BOARD_LCD_INVERT` từ `false` sang `true` (kèm comment ngắn). Không sửa file nào khác. **Chưa build, chưa chạy lại.**

Dự kiến thấy sau khi sửa (nếu giả thuyết đúng): các lần tô là đỏ, xanh lá, xanh dương, trắng, đen; mẫu cuối nền đen, viền trắng, trên trái đỏ, trên phải xanh lá, dưới trái xanh dương, dưới phải vàng, ô giữa trắng. Nếu vị trí các góc khác danh sách này thì đối chiếu bảng hướng ở nhật ký phiên 15 để chọn bit `MX`, `MY`, `MV` trong `BOARD_LCD_MADCTL`.

Cũng từ quan sát này: màn hình hiện màu theo đúng trình tự nên bus I80 ở 10 MHz và chuỗi khởi tạo đang chạy (theo lời chủ dự án). Chưa biết có nhiễu hay sọc, thời gian tô, stack còn trống, và việc board boot với các chân strapping (chưa có log).

### Phiên 15, bổ sung 2 (2026-11-08): kết quả quan sát lần 2, màn hình đúng màu và đúng hướng

Sau khi nạp bản có `BOARD_LCD_INVERT = true`, chủ dự án báo bằng lời (chưa có log serial, chưa có log build):

- Các lần tô toàn màn hình theo thứ tự: đỏ, xanh lá, xanh dương, trắng, đen.
- Mẫu cuối: nền đen, bốn góc đỏ (trên trái), xanh lá (trên phải), xanh dương (dưới trái), vàng (dưới phải). Chưa báo về viền trắng 2 px và ô trắng 40x40 ở giữa, nên hai phần này **chưa xác nhận**.

Kết luận (quan sát bằng mắt, theo lời chủ dự án):

- Khớp đúng dự kiến của bổ sung 1, nên giả thuyết đảo màu được xác nhận: `BOARD_LCD_INVERT = true` là đúng cho panel này.
- Thứ tự màu (bit BGR) và thứ tự byte (`swap_color_bytes = true`) đúng: đỏ, xanh lá, xanh dương hiện đúng màu theo bảng ở nhật ký phiên 15.
- Hướng đúng: bốn góc trùng vị trí dự kiến, nên `BOARD_LCD_MADCTL = SCREEN_HMI_MADCTL_MV | SCREEN_HMI_MADCTL_BGR` đúng cho hướng ngang với màn hình 480x320 và cạnh dài nằm ngang. Chưa kiểm cạnh nào là phía trên của board vật lý ngoài việc chủ dự án đang nhìn theo hướng chữ ngang.
- Bus I80 ở 10 MHz chạy được và hiển thị đúng màu. Chưa thử tốc độ cao hơn nên chưa biết giới hạn `pclk_hz`.
- Board boot được và chạy task `screen_app` dù LCD dùng các chân strapping GPIO 0, 3, 45, 46 (theo lời chủ dự án, chưa có log boot).

Trạng thái nhãn: các chân LCD (D0..D7, WR, RS, RST, backlight) đã hoạt động thực tế với giá trị hiện tại, nhưng chúng vẫn mang nhãn `REF` (lấy từ tài liệu dòng WT32-SC01 Plus, không phải schematic V15) và `BOARD_PINMAP_VERIFIED` vẫn là 0. Việc hoạt động được là quan sát, không thay cho xác nhận bằng schematic. Không đổi nhãn nào trong code ở bổ sung này.

Chưa biết: viền và ô giữa của mẫu cuối; thời gian mỗi lần tô và stack còn trống (cần log); có nhiễu, sọc hay màu nhạt do thiếu bảng lệnh panel không; giới hạn `pclk_hz`; hành vi khi wifi và http chạy cùng lúc với tô màn hình; touch, loa, mic, SD chưa gọi.

Việc tiếp theo đề xuất (chờ chủ dự án chọn): (a) thử tăng `BOARD_LCD_PCLK_HZ` (ví dụ 20 MHz, giá trị do phiên này chọn, không có nguồn) và xem có nhiễu, sọc hay lỗi tô không, hoặc (b) sang bring-up touch (`board_init_touch()`, `touch_read_id`, `touch_read`). Nên có log của lần chạy này trước khi bắt đầu.

### Phiên 15, bổ sung 3 (2026-11-08): log serial của lần chạy với `BOARD_LCD_INVERT = true`

Chủ dự án xác nhận bằng lời: viền trắng và ô trắng ở giữa **có hiện** trên màn hình thật. Cùng với bổ sung 2, toàn bộ mẫu cuối đã được xác nhận bằng mắt.

Số liệu đọc từ log (đây là số đo thật của lần chạy, không phải suy đoán):

- Dòng `panel up`: `size=480x320 madctl=0x28 invert=on pclk=10000000 Hz`. Không có dòng lỗi nào của `[screen]`: mọi lần tô trả OK, `set_backlight` OK.
- Thời gian tô toàn màn hình (480x320): 84, 84, 86, 85, 85 ms cho năm màu, 85 ms cho nền của mẫu. Khối 120x80: 5 đến 6 ms. Viền: 1 đến 2 ms. Ô giữa 40x40: 1 ms.
- Đọc code `screenHMI.c`: `fill_rect` chia khối theo hàng (`rows_per_chunk = 1920 / width`) và chờ mỗi khối bằng vòng `delay_ms(1U)` (dòng 311). Toàn màn hình có 80 khối, khối 120x80 có 5 khối, các phần nhỏ có 1 khối. Thời gian đo được xấp xỉ 1 ms mỗi khối ở cả ba trường hợp, nên **thời gian tô bị giới hạn bởi bước chờ 1 ms, không phải bởi pclk**. Hệ quả: tăng `BOARD_LCD_PCLK_HZ` sẽ không làm số ms trong log giảm đáng kể; kiểm tra giới hạn pclk phải dựa vào quan sát nhiễu hoặc sọc trên màn hình. Thời gian truyền thuần trên dây (bus 8 bit, 10 MHz, 307200 byte) khoảng 31 ms là phép tính, chưa đo. Đây không phải lỗi của bring-up; khi làm LVGL nên chờ bằng callback hoàn tất của bus thay vì chờ từng 1 ms (việc sau, chưa làm).
- Stack của `screen_app`: còn trống thấp nhất 3884 trên 6144 byte, tức đỉnh dùng khoảng 2260 byte. Giữ nguyên 6144, không chỉnh.
- Wi-Fi: quét thấy 20 mạng, lần nối đầu báo `reason=205` rồi lần thử kế tiếp nối được (khoảng 12,5 s sau khi boot), `sta ip: 192.168.30.100`. Hiện tượng `reason=205` ở lần nối đầu đã được ghi ở phần trước của HandOver, không phải do `screen_app`.
- HTTP: `GET https://example.com/` trả `result=0`, `status=200`, 577 byte, 2549 ms; stack http còn trống thấp nhất 4572 trên 8192 byte. Wi-Fi, HTTP và `screen_app` chạy đồng thời, không thấy xung đột trong log này.
- Heap: trước HTTPS 250176 byte trống, sau 248972; thấp nhất từ lúc boot 241692 trước và 208460 sau. So với số đo trước khi có màn hình (257980, 256984, 255272, 215620) thì thấp hơn khoảng 7 KB. Nhiều khả năng do bus I80, bộ mô tả DMA và `board_init()`, nhưng chưa tách riêng nên chưa kết luận.

Log dán lên bị cắt: dòng đầu là `esp_image: segment 4 ...` dính vào dòng `[screen] panel up` (mất đoạn boot ở giữa), và chưa có log build. Vì vậy **chưa biết** cảnh báo khi build, kích thước bin, lý do reset, và các dòng boot liên quan tới chân strapping. Việc board boot rồi chạy tới `screen_app` là bằng chứng gián tiếp là boot không hỏng.

Trạng thái bring-up màn hình: màu, thứ tự màu, đảo màu, hướng, viền, ô giữa đã xác nhận bằng mắt (theo lời chủ dự án) ở 10 MHz, và log không có lỗi. Chưa xác nhận: giới hạn `pclk_hz`, có nhiễu hay sọc không (chủ dự án chưa nhắc), độ đúng của gamma (không có bảng lệnh panel). Nhãn chân vẫn `REF`, `BOARD_PINMAP_VERIFIED = 0`.

Đề xuất bước tiếp theo: bring-up touch (`board_init_touch()`, `touch_read_id`, `touch_read`). Các chân touch (I2C, ngắt, reset dùng chung GPIO 4 với LCD) cũng là `REF` từ tài liệu dòng WT32-SC01 Plus. Các chân LCD lấy từ cùng tài liệu đã chạy đúng, nhưng điều đó không bảo đảm chân touch đúng trên bản V15. Cần chủ dự án xác nhận trước khi bắt đầu.

### Phiên 16 (2026-11-08): bài thử touch trong screen_app

Đầu phiên: clone repo (HEAD `4cfde3d`), đọc `HandOver.md`, `RuleAgent.md`, `RuleCode.md`, `Firmware_Construction.md` và đầu `Product_Construction.md`, đối chiếu repo. Phiên này kế thừa một cuộc trò chuyện Claude trước đó (chủ dự án dán lại làm tài liệu): cuộc đó đã thống nhất thiết kế bài thử touch và viết code trong sandbox của nó, nhưng bị dừng (hết hạn mức sử dụng) trước khi present file, nên **không có file nào của nó trong repo**. Phiên này viết lại hai file từ repo thật theo đúng thiết kế đó, không dùng lại code đã chạy trong sandbox cũ (không có cách kiểm chứng sandbox đó).

Lệch phát hiện khi đối chiếu (đã báo chủ dự án đầu phiên): prompt ghi phiên trước là phiên 14, nhưng handover đã có phiên 15 và ba bổ sung (bring-up màn hình xong về mặt hình ảnh: đảo màu bật, MADCTL `0x28`, 10 MHz). Từ đó `app_start()` không gọi `board_init()` trực tiếp mà `screen_app` gọi trong task của nó; màn hình không còn tối vì `screen_app` bật backlight. Lệch cũ còn nguyên: `platform/` so với `platforms/` trong tài liệu, tên file docs, comment lỗi thời trong `app.h`.

Thiết kế đã chốt với chủ dự án (theo hội thoại được dán): bài thử touch nằm trong `screen_app` (không tạo task mới), chạy ngay sau khi mẫu màn hình vẽ xong; chạm vào các vùng màu của mẫu thì in dòng `Tab in <màu>` (giữ nguyên chữ `Tab`, theo yêu cầu của chủ dự án) kèm tọa độ.

Đã làm (present từng file, đường dẫn trong repo):

- `LoganSDK/app/user/screen_app/screen_app.h` (sửa): thêm `APP_SCREEN_TOUCH_POLL_MS` (20 ms) và `APP_SCREEN_HEARTBEAT_MS` (30000 ms), cả hai do phiên này chọn, chưa đo; cập nhật comment của stack và mô tả bài thử. Stack giữ 6144 byte (số đo 3884 byte còn trống ở phiên 15 chỉ cho phần màn hình; phần touch chưa đo, task in số còn trống để chỉnh).
- `LoganSDK/app/user/screen_app/screen_app.c` (sửa): `pattern_rect_t` thêm trường `label` (nhãn dùng cho log touch: `black`, `white border`, `red`, `green`, `blue`, `yellow`, `white centre`). Sau `run_pattern` thành công, task gọi `init_touch()`: in cấu hình touch lấy từ macro của `board.h` (SDA, SCL, tốc độ I2C, địa chỉ mặc định, pull-up, swap, mirror), gọi `board_init_touch()`, `board_get_touch()`, `touch_read_id()`, in `chip_id`, `firmware_id`, `vendor_id`. Lỗi ở bất kỳ bước nào: in mã (kèm gợi ý khi `TOUCH_ERR_NO_DEVICE`), task kết thúc như trước, mẫu màn hình giữ nguyên. Khi chạy được, `run_touch_loop()` không bao giờ trả về: đọc `touch_read` mỗi `APP_SCREEN_TOUCH_POLL_MS`; in `[touch] Tab in <vùng> x=<x> y=<y> id=<id> points=<số điểm>` khi bắt đầu chạm và khi ngón tay chuyển sang vùng khác (giữ yên trong cùng một vùng thì không in lại; nhấc tay rồi chạm lại thì in lại). Chỉ điểm đầu được tra vùng, điểm thứ hai chỉ được đếm. Tọa độ ngoài mọi vùng in `outside the panel`. Tra vùng duyệt ngược bảng `s_pattern` (phần vẽ sau nằm trên). Lỗi đọc: in lần đầu và mỗi 50 lỗi liên tiếp, kèm `touch_recover` ở mỗi 50 lỗi, in dòng khi đọc lại được. Mỗi 30 s in một dòng `alive` kèm stack còn trống. Thời gian so sánh bằng phép trừ số không dấu nên đúng khi bộ đếm quay vòng.
- Không sửa CMake, `board.h`, `board.c`, `touch.c`, `app.c`. Không thêm dependency. Mọi tên dùng trong code (`BOARD_PIN_TOUCH_SDA/SCL`, `BOARD_TOUCH_*`, `TOUCH_DEFAULT_ADDR`, `touch_id_t`, `TOUCH_ERR_NO_DEVICE`, `TOUCH_MAX_POINTS`) đã đối chiếu với `board.h` và `touch.h` trong repo. `board.h` đã include `touch.h` nên `screen_app.c` thấy được `touch_t`.

Vị trí các vùng (tọa độ màn hình 480x320, tính từ code, để so với x, y của touch): khối đỏ x 2..121, y 2..81; xanh lá x 358..477, y 2..81; xanh dương x 2..121, y 238..317; vàng x 358..477, y 238..317; ô trắng giữa x 220..259, y 140..179; viền 2 px quanh mép. Tâm các khối: đỏ khoảng (62, 42), xanh lá (418, 42), xanh dương (62, 278), vàng (418, 278), ô giữa (240, 160). Nếu x, y đọc được lệch so với bảng này thì chỉnh `BOARD_TOUCH_SWAP_XY`, `BOARD_TOUCH_MIRROR_X`, `BOARD_TOUCH_MIRROR_Y` trong `board.h` (swap rồi giới hạn rồi mirror, theo `touch.h`).

Đã kiểm chứng:

- Biên dịch thử trên máy host (gcc 13.3, **không phải** GCC cho ESP32-S3, **không phải** `idf.py build`) với `-Wall -Wextra -Wshadow -Wconversion -Wsign-conversion -Wpedantic -Wmissing-prototypes -Wstrict-prototypes -Werror`: `screen_app.c` sạch, dùng header thật của dự án (`board.h`, `touch.h`, `screenHMI.h`, các `sx_*`) và header FreeRTOS giả.
- Chạy logic trên host (ASan, UBSan, `-fno-sanitize-recover=all`) với `board_*`, `screen_hmi_*`, `touch_*`, `sx_time` giả lập: tham số task (stack 6144, ưu tiên 2) và tạo task lỗi trả -1; đúng dòng log cho từng vùng (đỏ, xanh lá, xanh dương, vàng, ô giữa, viền trái, viền trên, đen, ngoài panel) và các biên của khối (điểm cuối cùng của khối vàng vẫn là vàng, điểm ngay ngoài khối đỏ là đen); giữ nguyên vùng thì không in lại, chuyển vùng khi vẫn chạm thì in, nhấc tay rồi chạm lại thì in, hai ngón thì in `points=2` theo ngón đầu; lỗi đọc xen giữa lúc đang chạm thì in lại sau khi đọc lại được; 120 lỗi liên tiếp thì in ở lần 1, 50, 100 và gọi `touch_recover` đúng 2 lần; một dòng `alive` sau 34 s với bộ đếm thời gian quay vòng ngay trong vòng đọc; init lỗi (`NO_DEVICE`, mã khác), `board_get_touch` trả NULL, `touch_read_id` lỗi đều kết thúc task mà không đọc touch; mọi `sx_delay_ms` trong vòng đọc đúng bằng 20 ms. 0 lỗi. 10 đột biến cố ý vào code production (tra vùng duyệt xuôi, bỏ in khi chuyển vùng, recover mỗi lần lỗi, bỏ đặt lại trạng thái khi lỗi (cả hai dòng), heartbeat không bao giờ in, heartbeat không dùng phép trừ, đổi nhãn vùng, bỏ kiểm tra `touch != NULL`, bỏ gợi ý `NO_DEVICE`, đổi chu kỳ đọc sang 5 ms) đều bị bắt. Lần chạy đầu của bộ đột biến có 2 đột biến sống sót do thiếu ca test (lỗi đọc xen giữa lúc đang chạm, mốc quay vòng nằm ngoài vòng đọc); đã bổ sung ca test, đột biến heartbeat bị bắt; hai đột biến xóa riêng từng dòng `down = false` hoặc `last_region = NULL` là đột biến tương đương (mỗi dòng đủ để in lại), xóa cả hai thì bị bắt. Bài test và bản giả lập nằm ngoài repo (không commit); giả lập chỉ chứng minh logic khớp mô hình do chính phiên này viết.
- Đọc `platform/esp32s3/time/esp32s3_time.c`: ở `CONFIG_FREERTOS_HZ=100` (`sdkconfig`), `TICK_MS = 10`, ngưỡng busy wait là dưới 20 ms. `sx_delay_ms(20)` không nhỏ hơn ngưỡng nên đi nhánh `vTaskDelay(20 / 10 + 2 = 4 tick)`: vòng đọc nhường CPU, nhưng chu kỳ đọc thật khoảng 30 đến 40 ms (cộng thời gian một lần đọc I2C), **không phải 20 ms**. Dùng 20 ms vì đó là giá trị nhỏ nhất không bị busy wait; không cần đổi.
- Kiểm tra bằng lệnh: hai file code chỉ có ASCII; comment không có mũi tên, đường kẻ hay thẻ ngoặc vuông (chuỗi log `[touch]` nằm trong chuỗi `printf`, không phải comment).

Chưa làm hoặc chưa kiểm chứng:

- **Không chạy `idf.py build`, không biên dịch bằng GCC cho ESP32-S3, không chạy trên phần cứng.** `uxTaskGetStackHighWaterMark` đã dùng ở các file khác (đã build), nhưng `screen_app.c` mới chưa biên dịch với header FreeRTOS và ESP-IDF thật.
- **Touch chưa từng chạy trên phần cứng.** Chưa biết chân SDA 6, SCL 5 có đúng trên V15 không (nhãn `REF`, từ tài liệu dòng WT32-SC01 Plus), chưa biết địa chỉ `0x38` đúng không, chưa biết I2C không bật pull-up trong chip (`BOARD_TOUCH_I2C_PULLUP = false`) có đủ không (board có thể có trở kéo lên ngoài, chưa xác minh), chưa biết 100 kHz có ổn không. Cực tính `swap_xy = true` và không mirror là giá trị `DESIGN` do phiên 14 chọn; hướng đúng chỉ biết được sau khi chạm thử.
- Reset của touch dùng chung GPIO 4 với LCD và do màn hình lái (`reset = NULL` trong `board.c`): chưa xác minh touch có thực sự nhận được xung reset đó, và `touch_init` chờ 300 ms (thông lệ, chưa đối chiếu datasheet) sau khi màn hình reset. Nếu touch không trả lời, một khả năng là thứ tự reset (chưa xác minh), không nhất thiết là sai chân.
- Bản đồ thanh ghi của touch theo mô hình FocalTech thông dụng, chưa đối chiếu datasheet của chip trên board (đã ghi ở mục 4). `chip_id` thật chỉ biết khi có log.
- Stack: phần `board_init_touch` (tạo bus I2C) chạy trong task 6144 byte chưa đo; dòng `touch ready ... lowest free stack` và `alive` sẽ cho số thật. `printf` với nhiều tham số dòng `[touch] init:` cũng tốn stack.
- Vòng đọc không bao giờ kết thúc và không có chỗ dừng; dòng `alive` mỗi 30 s là toàn bộ tín hiệu còn sống. Chưa đo tác động lên Wi-Fi và HTTP khi đọc I2C mỗi 30 đến 40 ms (kỳ vọng nhẹ, chưa quan sát).
- Touch dùng polling, chân INT (GPIO 7) vẫn chưa dùng và chưa xác minh.
- Chưa có LVGL: dòng `Tab in ...` chỉ là bài thử bring-up, không phải giao diện.

Kế hoạch tiếp theo: xem mục 6 (việc đầu tiên của chủ dự án) và mục 7 (bước 3). Sau khi có log touch: chỉnh `BOARD_TOUCH_SWAP_XY`, `BOARD_TOUCH_MIRROR_X/Y` theo kết quả, rồi quyết định giữa thử tăng `BOARD_LCD_PCLK_HZ`, bring-up SD, loa hoặc mic, hoặc bắt đầu LVGL.

### Phiên 16, bổ sung 1 (2026-11-08): log chạy bài thử touch, sửa trục Y

Chủ dự án build, flash và gửi log serial (phần boot và log build bị cắt, chỉ có kết quả của `esptool` và đoạn log chạy). Đọc từ log:

- Build: `board_hmi_ai.bin` 0xf3c20 byte, phân vùng app nhỏ nhất 0x100000 byte, còn trống 0xc3e0 byte (5%), ESP-IDF cảnh báo phân vùng gần đầy. Bootloader 0x51c0 byte. Bản trước (phiên 13) là 0xb61f0; bin tăng vì `board`, `screenHMI`, `touch`, `sx_i80`, `sx_pwm`, `sx_i2c` đã vào bin. **Chưa có log cảnh báo biên dịch** của `screen_app.c`. Rủi ro dung lượng: thêm LVGL, OTA hay TLS lớn hơn sẽ không còn chỗ trong phân vùng 1 MB, cần chủ dự án quyết định về partition (xem quyết định mở ở mục 6).
- `esptool` báo: chip ESP32-S3 (QFN56) revision v0.2, `Embedded PSRAM 2MB (AP_3v3)`, crystal 40 MHz. Đây là thông tin do `esptool` đọc, **chưa phải xác minh của board V15**: gợi ý chip có PSRAM 2 MB nhúng (loại PSRAM quad hay octal vẫn chưa kết luận từ log này, `sdkconfig` vẫn chưa bật `CONFIG_SPIRAM`). Dung lượng Flash thật chưa biết (lệnh flash dùng `--flash_size 2MB` theo `sdkconfig`, `esptool` không in dung lượng Flash trong log này).
- Màn hình: mẫu vẽ xong như phiên 15 (nền 85 ms, các khối 5 đến 6 ms, ô giữa 2 ms).
- Touch: `[touch] init: sda=6 scl=5 i2c=100000 Hz addr=0x38 internal_pullup=off ...`, driver I2C ESP-IDF in cảnh báo `Please check pull-up resistances whether be connected properly` (do `BOARD_TOUCH_I2C_PULLUP = false`; touch vẫn trả lời, nên có vẻ board có trở kéo lên ngoài, **chưa xác minh**, cảnh báo của driver không chứng minh trở có hay không). `controller answers: chip_id=0x64 firmware_id=0x11 vendor_id=0x11`. `0x64` khớp với FT6336U theo hiểu biết của phiên này (chưa đối chiếu datasheet). Chân SDA 6, SCL 5, địa chỉ `0x38`, 100 kHz, reset chung GPIO 4 do màn hình lái: **hoạt động thực tế**, nhưng vẫn nhãn `REF` (chưa có schematic V15), `BOARD_PINMAP_VERIFIED` vẫn 0.
- Stack: `touch ready ... lowest free stack 3860 of 6144 bytes` và dòng `alive` cũng 3860, nên phần touch không dùng thêm stack so với phần màn hình (3884 ở phiên 15, chênh nhỏ do `printf`). Giữ 6144.
- Wi-Fi và HTTP chạy cùng lúc, không thấy xung đột trong đoạn log này (`sta=got_ip`, `clients=0`, `[http] body starts` có mặt). Chưa có đo định lượng.
- Hướng chạm: chủ dự án báo bằng lời chạm vào khối đỏ (trên trái) thì log báo `blue`, chạm xanh dương (dưới trái) thì báo `red`, chạm xanh lá (trên phải) báo `yellow`, chạm vàng (dưới phải) báo `green`. Log khớp: ví dụ `blue x=80 y=290`, `red x=74 y=48`, `yellow x=390 y=287`, `green x=411 y=38`. Phiên này đối chiếu bốn cặp với hình học khối: lật trục Y (y thành 319 trừ y) đổi đúng các cặp đỏ và xanh dương, xanh lá và vàng; lật cả trục X sẽ đổi chéo (đỏ và vàng), không khớp báo cáo; x của bốn lần chạm nằm đúng cột (80 và 74 trong cột trái, 390 và 411 trong cột phải). Kết luận: `swap_xy = true` đúng, X đúng, **Y cần mirror**. Ô giữa đúng ở cả hai lần chạm (y 146 và 155 sau khi lật thành 173 và 164, vẫn trong ô 140..179). Đây là suy luận từ log và lời chủ dự án, vị trí ngón tay thật không đo.
- Hai dòng `white border x=409 y=319` và `black x=260 y=61`, `black x=295 y=245` là các lần chạm ở viền và vùng đen, không mâu thuẫn (y=319 là mép dưới trước khi lật, tức mép trên sau khi lật).

Đã sửa (present từng file, đường dẫn trong repo):

- `LoganSDK/board/board.h` (sửa): `BOARD_TOUCH_MIRROR_Y` từ `false` sang `true`, cập nhật comment của khối touch. Không sửa file nào khác. Không đổi `swap_xy` và `mirror_x`.

Đã kiểm chứng: chỉ đối chiếu số liệu log với hình học khối bằng phép tính trên host (không phải build, không phải chạy trên board). Kiểm tra bằng lệnh: `board.h` chỉ có ASCII. **Chưa build, chưa chạy lại.**

Dự kiến thấy sau khi sửa: chạm khối đỏ in `Tab in red`, xanh lá in `green`, xanh dương in `blue`, vàng in `yellow`, ô giữa `white centre`, tọa độ y của khối trên khoảng 2..81 và của khối dưới khoảng 238..317. Nếu vẫn lệch thì gửi bốn dòng log của bốn góc.

Chưa biết: độ chính xác tọa độ (lệch giữa điểm chạm và khối, vùng gần mép, hiệu chuẩn), chạm nhiều ngón, độ trễ, độ ổn định khi chạy lâu, khả năng nhiễu I2C khi Wi-Fi phát mạnh.

Kế hoạch tiếp theo: chủ dự án build, flash, xác nhận bốn góc; sau đó chọn giữa bring-up loa hoặc mic, SD và storage, thử `BOARD_LCD_PCLK_HZ` cao hơn, hoặc LVGL. Trước LVGL cần quyết định về dung lượng phân vùng app (5% trống) và về PSRAM (xem trên).

### Phiên 16, bổ sung 2 (2026-11-08): flash 16 MB và bảng phân vùng riêng

Bối cảnh: bin 0xf3c20 byte chỉ còn 5% trong phân vùng app 1 MB; `sdkconfig` đang khai báo flash `2MB` và bảng `partitions_singleapp.csv`. Chủ dự án chạy `esptool.py -p COM9 flash_id`: flash thật là 16 MB (xem mục 4). Chủ dự án chọn phương án C (bảng phân vùng riêng, hai slot OTA).

Đã làm (present từng file, đường dẫn trong repo):

- `partitions.csv` (mới, ở thư mục gốc project, cạnh `sdkconfig`): `nvs` 0x9000 (24 KB, như bảng single app có sẵn của ESP-IDF), `otadata` 0xF000 (8 KB), `phy_init` 0x11000 (4 KB), `ota_0` 0x20000 (3 MB), `ota_1` 0x320000 (3 MB). Không có `factory` (bootloader chạy `ota_0` khi `otadata` trống). Còn trống từ 0x620000 đến hết 16 MB, khoảng 9,88 MB, **chưa cấp cho phân vùng dữ liệu nào** (không thêm phân vùng chưa có code dùng; thêm sau khi biết nhu cầu, ví dụ tài nguyên âm thanh hay bài học). Bin hiện tại 0xf3c20 byte chiếm khoảng 32% một slot 3 MB. Bố cục nvs, otadata, phy_init, ota_0 ở 0x20000 giống bảng `partitions_two_ota_large.csv` có sẵn của v5.4.3 (đọc từ repo Espressif), chỉ khác kích thước slot (3 MB thay vì 1700 KB) và offset ghi tường minh.
- Không sửa `sdkconfig`, CMake hay code. Không thêm dependency.

Việc chủ dự án cần làm để áp dụng (chủ dự án tự quản `sdkconfig`):

1. `idf.py menuconfig`, mục `Serial flasher config`, `Flash size`: chọn `16 MB`.
2. Mục `Partition Table`, `Partition Table`: chọn `Custom partition table CSV`; `Custom partition CSV file`: `partitions.csv` (đường dẫn tính từ thư mục gốc project, theo `Kconfig.projbuild` của v5.4.3). Lưu, thoát.
3. `idf.py build`, rồi `idf.py -p COM9 flash monitor`. Lần flash này ghi cả bootloader, bảng phân vùng và app (app nằm ở 0x20000, không còn ở 0x10000). Gửi lại: log build (dòng kích thước bin, phải còn trống khoảng 68% của slot 3 MB), log `esptool` (địa chỉ ghi app, `--flash_size 16MB`), phần boot đầu tiên (bảng phân vùng được bootloader in ra).
4. Cập nhật sau đó: commit cả `sdkconfig` (nếu chủ dự án vẫn giữ `sdkconfig` trong git) và `partitions.csv`.

Đã kiểm chứng:

- Chạy `gen_esp32part.py` của ESP-IDF v5.4.3 (tải từ github) với `--flash-size 16MB` trên `partitions.csv`: tạo được bin, đọc ngược ra đúng 5 phân vùng với offset và kích thước như trên, không báo lỗi chồng lấn hay lệch biên 64 KB. Bootloader hiện 0x51c0 byte nằm dưới 0x8000 (log phiên 16). Đây là kiểm tra công cụ của IDF, **không phải `idf.py build`, không phải chạy trên board**. Chỉ ASCII trong file.

Chưa làm hoặc chưa kiểm chứng:

- Chưa build với bảng mới, chưa flash. Chưa biết build có phàn nàn gì về `sdkconfig` cũ (ví dụ giá trị `CONFIG_ESPTOOLPY_FLASHSIZE` còn 2MB sẽ khiến `esptool` ghi `--flash_size 2MB` và bảng ngoài 2 MB không hợp lệ; do đó bước 1 là bắt buộc).
- OTA chưa có code (chưa có service OTA, chưa bật rollback `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`). Bảng chỉ chuẩn bị chỗ.
- Hành vi của `idf.py flash` với `otadata` trống (khởi động `ota_0`) theo hiểu biết về bootloader ESP-IDF, chưa quan sát trên board này.
- Việc đổi bảng phân vùng có thể xóa NVS cũ; Wi-Fi của dự án lấy cấu hình từ code (`WIFI_STORAGE_RAM`) nên không mất dữ liệu đã biết, nhưng chưa kiểm tra trên board.
- PSRAM chưa bật, nên mọi buffer lớn vẫn nằm trong RAM trong. Bật PSRAM (quad hay octal) vẫn là việc của bước LVGL.

Kế hoạch tiếp theo: sau khi build và flash bảng mới, chọn giữa bring-up loa hoặc mic, SD và storage, bật PSRAM, hoặc LVGL. Quyết định về CA bundle FULL và OTA vẫn còn mở nhưng không còn gấp vì đã có chỗ.