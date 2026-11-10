# Handover - AI Learning Device

> File này do mọi phiên làm việc (Claude hoặc AI khác) cập nhật ở cuối phiên. Quy tắc: xem `RuleCode.md` mục 3.
> Cập nhật lần cuối: 2026-11-10 (phiên 19). Đã rút gọn: chỉ giữ trạng thái hiện tại và việc gần. Nhật ký chi tiết các phiên 1 đến 16
> (kết quả test, đột biến, log từng lần chạy) nằm trong lịch sử git: `git show 271fe1c:docs/HandOver.md`.

## 1. Đọc trước khi làm

1. `docs/RuleAgent.md` - cách làm việc của AI/agent: quy trình đầu/cuối phiên, nguyên tắc bắt buộc.
2. `docs/RuleCode.md` - quy tắc bàn giao file và viết comment.
3. `docs/Firmware_Construction.md` - kiến trúc layer, quy tắc phụ thuộc, trạng thái phần cứng.
4. `docs/Product_Construction.md` - đặc tả sản phẩm (đọc khi liên quan).
5. Repo tham khảo phong cách của chủ dự án: https://github.com/ngoxuanloc2309/simple_plc.git (mô hình layer, hợp đồng `sx_*`).

## 2. Thông tin dự án

- Thiết bị học tập AI (Learn / Play / Talk), môn đầu tiên: English. AI nặng (STT, LLM, TTS) chạy ở backend, không chạy trên ESP32-S3.
- MCU: ESP32-S3 (WT32-S3-WROVER), ESP-IDF v5.4.3, ngôn ngữ C, UI dự kiến LVGL. Project name `board_hmi_ai`.
- Repo: https://github.com/ngoxuanloc2309/learn_board_AI.git. Chủ dự án dùng Windows, thư mục `E:\prj_canhan\Learning_board`, cổng nạp `COM9`.

```text
app, board (L4)  ->  services (L3)  ->  modules (L2)  ->  peripherals contracts (L1)  ->  platform/esp32s3 (L0)
utils (không phụ thuộc layer nào)
```

Mỗi thư mục là một component ESP-IDF, tên component = tên thư mục (`LoganSDK/platform/esp32s3` có tên `esp32s3`). Chỉ gọi xuống dưới.
Chỉ layer `platform/esp32s3` được include header driver ESP-IDF. Tầng app được phép gọi hàm công khai của app khác (tài liệu không cấm).

## 3. Trạng thái hiện tại

Nhãn: **HW** = đã chạy trên board (theo log hoặc lời chủ dự án); **host** = chỉ chạy logic trên máy host với giả lập (không phải build ESP-IDF);
**build** = đã nằm trong lần `idf.py build` thành công của chủ dự án (suy ra từ việc build thành công, không có log).

| Hạng mục | Trạng thái |
|---|---|
| Khung project, CMake các layer, `sdkconfig` | Build thành công trên máy chủ dự án (bản phiên 16, bin 0xf3c20). Flash 16 MB, bảng `partitions.csv` (2 slot OTA 3 MB) đã build và chạy |
| `board.h`, `board.c` | `board_init()` chỉ đặt cờ. `board_init_screen/touch/speaker/mic/storage()`, `board_get_*()`. `board_init_touch()` cần màn hình đã init (reset GPIO4 dùng chung, do màn hình lái). Màn hình, touch, loa: HW. Mic: có dữ liệu về nhưng L/R chưa nối GND (xem mục 4). Thẻ SD: chưa chạy. Mọi chân còn nhãn `REF` (mic: `GIVEN`), `BOARD_PINMAP_VERIFIED = 0` |
| `sx_gpio`, `sx_i2c`, `sx_i80`, `sx_pwm`, `sx_time` | HW (qua màn hình, touch, backlight). Ngắt của `sx_gpio` chưa ai dùng |
| `sx_spi`, `sx_uart` | Build. Chưa chạy HW (SD chưa thử; UART/RS485 chưa dùng, chân chưa xác minh) |
| `sx_i2s` (TX loa, RX mic) | HW: TX (loa) chạy, nghe được. RX (mic) nhận được dữ liệu, chưa đánh giá được chất lượng vì L/R chưa nối GND |
| `sx_wifi` | HW: AP `LEARN_BOARD_2309` ở `192.168.4.1` (DHCP), STA nối router WPA2 và nhận IP, quét mạng, tự nối lại sau khi rút router 20 s. Chưa thử router WPA3 |
| `services/http` (`http_svc_perform`, blocking) | HW: `GET https://example.com/` trả 200, chứng chỉ kiểm bằng CA bundle (luôn kiểm, không có cách tắt, không theo redirect). Chưa thử HTTP thường, redirect, chứng chỉ sai, mất Wi-Fi giữa chừng, gọi lặp |
| Module `screenHMI` | HW: ST7796 qua I80 8 bit, màu, đảo màu, hướng đã đúng ở 10 MHz. Chưa có bảng lệnh riêng của panel (gamma, nguồn) |
| Module `touch` (FT6336U, polling, không dùng INT) | HW: địa chỉ `0x38`, `chip_id=0x64`, đọc được điểm chạm, trục đã chỉnh (`BOARD_TOUCH_MIRROR_Y = true`) |
| Module `sdcard` (SD-SPI, khối 512 byte) và `services/storage` (FatFs R0.16 của ChaN trong `libs/`, không dùng component `fatfs` của ESP-IDF) | host (FatFs thật, thẻ giả lập bằng RAM). Chưa thử thẻ thật. `storage_*` không thread-safe: chỉ một task gọi. `sdcard_init` không có thẻ có thể chặn vài giây. `board.c` đặt `format_if_mount_failed = false`. `ffconf.h`: `FF_FS_EXFAT = 0` nên thẻ SDXC 64 GB trở lên (exFAT) không dùng được, chỉ FAT32 |
| Module `speaker` (I2S TX, mono vào, ra hai slot) | HW: chủ dự án nghe được ba tiếng bíp; log `48000 samples queued (3000 ms of audio) in 3105 ms`. Chưa biết độ to, độ méo, công suất tối đa của amp và loa 4 ohm |
| Module `inmp441` (I2S RX, mono, 16 bit, lấy 16 bit trên của mẫu 24 bit) | HW một phần: mẫu về được, `peak`, `rms`, `dc` thay đổi theo thời gian, nhưng **kết quả chưa đáng tin**: L/R chưa nối GND (header của module ghi "L/R must never float"), peak chạm 32767 liên tục trong nhiều khối. Cần chạy lại sau khi nối L/R xuống GND |
| `wifi_app`, `http_app`, `screen_app` | HW. `screen_app` gọi `board_init()`, vẽ mẫu màu rồi chạy vòng đọc touch không kết thúc (log `[touch] Tab in <vùng> x= y=`) |
| `speaker_app`, `mic_app` (phiên 17, thay `audio_app`) | HW (bản `f7ae25e`, log chủ dự án 11-10, thấy cả hai chạy cùng Wi-Fi, màn hình, touch). `speaker_app`: ba tiếng bíp 440/880/1320 Hz ở 20 phần trăm, nghe được; xuất `speaker_app_tone_test_done()`. `mic_app`: chờ cờ đó rồi mới init mic (thứ tự đã đúng trong log: `[mic] level meter` in sau `[speaker] task ends`); đo mức 5 s; thu 2 s và phát lại (`recorded 32000 samples, peak 32623, software gain x1`, `playback done`). Chưa biết phát lại có nghe ra giọng không; do L/R thả nổi nên bản thu nhiều khả năng là nhiễu, chủ dự án chưa nói nghe được gì. **Phiên 19 (host, chưa build ESP-IDF, chưa chạy HW):** `mic_app` thêm bước 3 `run_live_monitor`: sau bài đo mức, đọc mic liên tục (khối 8 ms), in **1 dòng mỗi giây** (`peak`, `ac` = RMS đã bỏ DC, `dc`, mức ba vùng LOW 100-400 Hz, MID 600-1700 Hz, HIGH 2500-7000 Hz kèm thanh `#`), và phát lại thẳng ra loa (đã bỏ DC, nhân `APP_MIC_LIVE_GAIN` = 4, âm lượng `APP_MIC_LIVE_VOLUME` = 15). Có howl guard: mic có peak từ `APP_MIC_LIVE_HOWL_PEAK` (20000) liên tục 300 ms thì loa bị tắt `APP_MIC_LIVE_MUTE_MS` (3000 ms). Chạy mãi mãi theo mặc định (`APP_MIC_LIVE_MS`). Đã đổi: `APP_MIC_RECORD_MS` mặc định từ 2000 xuống 0 (bài thu 2 s rồi phát bị bỏ qua), bài đo mức in 1 dòng mỗi giây thay vì mỗi 100 ms. Ba vùng dùng bộ lọc biquad số nguyên (Butterworth bậc 2 cao thông rồi thấp thông, hệ số Q28, trạng thái Q8) |
| `storage_app` (bài thử microSD) | Có code, chưa có log build, chưa chạy, **chưa được gọi trong `app_start()`** (chủ dự án thêm `#include "storage_app.h"` và `(void)storage_app_start();` khi có thẻ) |
| `AI_app`, `ntp_app`, `services/AI`, `AI_backend`, `AI_frontend` | File rỗng. Chờ đặc tả backend AI |
| `libs/cJSON` | Có trong thư mục, chưa build |
| LVGL, OTA | Chưa bắt đầu (bảng phân vùng đã chừa chỗ OTA, chưa có code) |

`app_start()` hiện gọi: `wifi_app_start()`, `http_app_start()`, `screen_app_start()`, `speaker_app_start()`, `mic_app_start()`.

## 4. Thông tin phần cứng

Chủ dự án cung cấp: board ZX3D50CE08S-V15-USRC 230208, ESP32-S3 / WT32-S3-WROVER, LCD 3.5 inch 480x320 ST7796 I80 8 bit, touch FT6336U I2C,
microSD SPI, loa I2S có amp onboard, RS485 onboard. Chân và giá trị cấu hình nằm ở `LoganSDK/board/board.h`.

Nhãn trong `board.h`: `REF` (lấy từ tài liệu dòng WT32-SC01 Plus, chưa xác minh trên V15), `GIVEN` (chủ dự án nói, firmware chưa kiểm), `DESIGN` (phiên thiết kế chọn, không có nguồn).

**Chạy được trên board nhưng vẫn nhãn `REF`** (không thay cho schematic): chân LCD (D0..D7, WR, RS, RESET GPIO 4, backlight GPIO 45), touch (SDA 6, SCL 5, địa chỉ `0x38`, 100 kHz, reset chung GPIO 4), loa I2S (WS 35, BCLK 36, DOUT 37: nghe được tiếng bíp, amp của board chạy mà code không điều khiển chân enable nào). Board vẫn boot và chạy dù LCD dùng chân strapping GPIO 0, 3, 45, 46 (theo lời chủ dự án, chưa có log boot).

**Đọc từ `esptool` (không phải schematic):** chip ESP32-S3 (QFN56) rev v0.2, 40 MHz, flash 16 MB (quad, 3.3 V), `Embedded PSRAM 2MB (AP_3v3)`.

**Giá trị `DESIGN` đã được xác nhận khi chạy:** `pclk_hz` 10 MHz (chưa biết giới hạn trên), MADCTL `MV | BGR` (0x28), đảo màu bật (`BOARD_LCD_INVERT = true`), `swap_color_bytes = true`, touch `swap_xy = true`, `mirror_x = false`, `mirror_y = true`.

**Giá trị `DESIGN` chưa xác nhận:** `max_transfer_bytes` 30720, `queue_depth` 1; backlight LEDC timer 0 channel 0, 5 kHz, 8 bit, active high; I2C port 0 không bật pull-up trong chip (driver cảnh báo, touch vẫn trả lời, board có thể có trở kéo lên ngoài, chưa xác minh); SD dùng `SPI2_HOST` 10 MHz; loa I2S port 0, mic I2S port 1, 16000 Hz; volume mặc định, các thời gian chờ (reset LCD/touch, SWRESET, SLPOUT, boot touch) lấy theo thông lệ, chưa đối chiếu datasheet.

**Chưa xác minh (không được coi là sự thật):**

- Chân thẻ SD (CS 41, MOSI 40, SCLK 39, MISO 38): `REF`, chưa thử. Chân loa đã chạy (xem trên) nhưng vẫn `REF`; amp có cần chân enable hay không: có vẻ không (đã nghe được), chưa xác minh bằng schematic.
- Mic INMP441: chân SCK GPIO 10, WS 11, SD (DOUT của mic) 12, `GIVEN` theo lời chủ dự án. **L/R:** chủ dự án nói (phiên 19, GIVEN) đã nối xuống GND; log chạy sau khi nối chưa gửi cho phiên này, và chủ dự án cho rằng đỉnh cao trong log là do loa kêu, đỉnh thấp là mic lúc loa tắt (chưa kiểm chứng). Trước đó (phiên 18) L/R thả nổi. Với L/R thả nổi mic có thể xuất ở khe trái hoặc phải một cách không xác định, `inmp441.c` chỉ đọc khe trái. Dữ liệu vẫn về được (xem mục 3), nhưng không đáng tin. Nguồn 3.3 V cho mic chưa xác minh (header mở rộng chỉ có +5V và GND).
- Loại PSRAM (quad hay octal): chưa kết luận; `CONFIG_SPIRAM` chưa bật. **Nếu octal thì GPIO 35 đến 37 (chân loa trong `board.h`) không dùng được.** Cách kiểm chứng: bật `CONFIG_SPIRAM` và xem log boot.
- RS485 (chưa khai báo trong `board.h`), `TE` (GPIO 48) và `TP_INT` (GPIO 7) chưa dùng nên chưa khai báo. Chân CS của LCD giả định nối đất.
- Bản đồ thanh ghi touch theo mô hình FocalTech thông dụng, chưa đối chiếu datasheet.
- Bảng lệnh riêng của panel (nguồn, gamma, mở khóa lệnh mở rộng): chưa viết, cần nguồn đã xác minh.
- Header `board/board_device/ZX3D50CE08S-V15-USRC.h` của chủ dự án: không ai include, dùng macro `GPIO_xx` chưa định nghĩa, có ký tự CJK và một ký tự không phải ASCII trong comment (trái `RuleCode.md`, chưa sửa vì là code của chủ dự án). `docs/ZX3D50CE08S-USRC-4832.md` là datasheet dòng SC01 Plus, ảnh PCB ghi V13, không phải V15.

**Mạng (GIVEN, lấy từ laptop Windows):** router SSID `Log Terminal ` (**có một dấu cách ở cuối**, 13 ký tự; `DEFAULT_WIFI_SSID` trong repo đã đúng), 2.4 GHz, WPA2, mạng `192.168.30.0/24`; router tự đổi kênh, AP của board theo kênh router. `DEFAULT_STATIC_IP` của AP là `192.168.4.1`. Mật khẩu nằm trong code, không ghi ở tài liệu.

## 5. Số đo thật đã có (cho phiên sau tham chiếu)

| Số đo | Giá trị | Ghi chú |
|---|---|---|
| `board_hmi_ai.bin` | 0xf3c20 byte, slot 3 MB còn 68% | Bản phiên 16, chưa có `audio_app`, `mic_app`, `storage_app`. Bootloader 0x51c0 byte |
| Tô toàn màn hình 480x320 | khoảng 85 ms | Bị giới hạn bởi bước chờ 1 ms mỗi khối trong `fill_rect` (80 khối), không phải bởi pclk. Truyền thuần trên dây khoảng 31 ms là phép tính |
| Stack `screen_app` (6144) | còn trống thấp nhất 3860 | Gồm cả phần touch |
| Stack `http_app` (8192) | còn trống thấp nhất 4572 | Một lần gọi tới `example.com` |
| HTTPS GET `example.com` | khoảng 2.5 s, thân 577 byte | Gồm DNS, TCP, TLS |
| Heap trống (có màn hình) | khoảng 250 KB trước HTTPS, thấp nhất 208 KB sau | Đỉnh RAM của một lần HTTPS khoảng 42 KB. Một mẫu, chưa biết có rò rỉ (giảm khoảng 1 KB) hay không |
| Wi-Fi | nối đầu thường `reason=205` rồi lần thử kế tiếp nối được (khoảng 12 s sau boot) | Tự thử lại chạy đúng. Nguyên nhân 205 chưa xác minh |
| `boot` (bản `f7ae25e`) | bảng phân vùng do bootloader in: `nvs` 0x9000 (0x6000), `otadata` 0xf000, `phy_init` 0x11000, `ota_0` 0x20000 (0x300000), `ota_1` 0x320000 (0x300000); `SPI Flash Size : 16MB`; `Loaded app from partition at offset 0x20000` | Boot bình thường dù LCD dùng GPIO 0, 3, 45, 46 (`boot:0xa SPI_FAST_FLASH_BOOT`). Heap lúc khởi động: 264 + 21 + 32 KiB (DRAM) + 7 KiB RTC |
| Loa, 3 tiếng bíp | 48000 mẫu (3000 ms) xếp hàng trong 3105 ms | Nhịp do I2S giữ đúng, chênh khoảng 105 ms. Task `speaker_app` còn trống thấp nhất 4164 / 6144 byte |
| Stack `mic_app` (6144) | còn trống thấp nhất 4116 | Gồm bài đo mức và thu rồi phát |
| Stack `screen_app` sau khi thêm các task âm thanh | còn trống thấp nhất 3832 (log in lúc `touch ready`) | Trước đó 3860; chênh nhỏ |
| Mic, đo mức 5 s, L/R thả nổi | peak 1760 đến 32767, rms 712 đến 19054, dc từ -6111 đến +9467 | Khoảng 25 trong 50 khối chạm peak 32600 trở lên. Không giống tiếng nói thường (đã đọc `inmp441.c`: lấy `raw >> 16`, đúng cho mẫu 24 bit căn trái). **Không dùng làm số đo mức ồn của mic**; đo lại sau khi nối L/R GND |

## 6. Việc đang chờ chủ dự án

1. **(Phiên 19) Build và chạy bản `mic_app` mới** (live monitor + phát lại, mục 3). Cẩn thận tiếng hú: loa gần mic, bắt đầu với âm lượng thấp. Gửi log `[mic]` (mỗi giây một dòng) và cho biết: im lặng thì ba vùng có thấp không; ngân "mmm" thì LOW có lên không; kéo dài "aaa" thì MID; xì "sss" thì HIGH; vỗ tay thì cả ba; phát lại có nghe ra giọng không, có hú không, độ trễ; có dòng `howl guard` không; log build (kích thước `board_hmi_ai.bin`, cảnh báo của `mic_app.c`). Phần cũ bên dưới vẫn đúng khi cần: **nối chân L/R của mic xuống GND, rồi chạy lại** (bản `f7ae25e` hoặc mới hơn). Trước khi chạy: nếu muốn tránh tiếng ồn lớn khi phát lại, hạ `APP_MIC_PLAY_VOLUME` (hiện 40) hoặc nghe thử với âm lượng thấp, vì khi L/R thả nổi bản thu gần như toàn nhiễu mức cao (peak 32623) và `mic_app` đã phát lại nó ở 40 phần trăm. Gửi:
   (a) log serial từ dòng `[mic] level meter` tới `[mic] task ends` (cả 50 dòng đo mức, kèm dòng `verdict`, `recorded ... peak ... gain`);
   (b) bằng tai: lúc im lặng và lúc nói, vỗ tay thì thanh `#` có khác nhau rõ không; phát lại có nghe ra giọng mình không, có méo hay có nhiễu không;
   (c) cho biết ba tiếng bíp to hay nhỏ, có méo không (có cần chỉnh `APP_SPEAKER_TONE_VOLUME`), và lần phát lại ở âm lượng 40 phần trăm có quá to không;
   (d) log build mới nhất: dòng kích thước `board_hmi_ai.bin` (so với 0xf3c20), cảnh báo của `speaker_app.c`, `mic_app.c`, `app.c`.
   Nếu sau khi nối GND mà vẫn toàn không: kiểm tra dây SD vào GPIO 12, WS vào 11, SCK vào 10, nguồn 3.3 V; nếu vẫn thấy peak chạm 32767 khi im lặng: có thể nhiễu từ dây dài hoặc thiếu nguồn ổn định, gửi log để phân tích.
2. Thiếu log từ các phiên trước (không gấp): cảnh báo build của `screen_app.c` và `storage_app.c`. (Đã có phần boot: bảng phân vùng 16 MB do bootloader in ra, xem mục 5.)
3. Khi có thẻ microSD 4 GB (SDHC, FAT32 sẵn; thẻ mới chưa nhận): thêm `storage_app_start()` vào `app_start()` (xem mục 3), cắm thẻ trước khi cấp nguồn, gửi log `[storage]`.
4. Cung cấp schematic V15 hoặc kết quả đo: chân SD, chân enable của amp loa (nếu có), RS485, loại PSRAM, nguồn 3.3 V cho mic.

Quyết định còn mở:

- Bật PSRAM (2 MB nhúng): cần biết quad hay octal trước, vì octal chiếm GPIO 35 đến 37 (chân loa). Việc của bước LVGL.
- CA bundle đang `CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_FULL=y` (tốn flash); có thể thu nhỏ hoặc chỉ dùng PEM của backend. Chưa đo.
- Đặc tả backend AI (URL, endpoint, JSON hoặc audio, xác thực, TLS, STT/TTS; `AI_backend` khác `AI_frontend` thế nào chưa trả lời). Đề xuất cũ của phiên 11 (chưa được xác nhận): REST text-only `POST /v1/chat` JSON `{session_id,text}` trả `{reply}`; cần thêm cJSON vào `libs/CMakeLists.txt`.
- `sx_wifi_init` xóa cả phân vùng NVS khi `nvs_flash_init` báo đầy hoặc sai phiên bản (mẫu chuẩn ESP-IDF); nếu code khác lưu dữ liệu trong NVS thì cần đổi cách xử lý. Trang web cấu hình Wi-Fi qua AP: chủ dự án nói chưa cần.
- Giữ `sdkconfig` và `sdkconfig.old` trong git hay không (hiện đang commit cả hai; `.gitignore` chỉ có `build/`).
- Ưu tiên và stack của các app bring-up (hiện 6144 và 2). Đã đo: `screen_app` còn trống 3832, `speaker_app` 4164, `mic_app` 4116; có thể hạ nếu cần RAM, chưa có lý do gấp.
- Dọn file rỗng, không nằm trong `SRCS` nên không ảnh hưởng build: `components/peripherals/{gpio,i2c,i2s,i80,spi}/sx_*.c`, `platform/esp32s3/{gpio,i2c,i80,spi}/esp32s3_*.h`, `board/board_device/ZX3D50CE08S-V15-USRC.c`, `LoganSDK/CMakeLists.txt` (0 byte), `LoganSDK/platform/CMakeLists.txt` (cũ).
- Chỉnh `utils/cqueue` (code của chủ dự án, chưa sửa): macro `QUEUE_DEBUG_ENABLE` và `CQUEUE_DEBUG_ENABLE` lệch tên; header guard `__QUEUE_H` là tên dành riêng; `cqueue_init` dùng `malloc` mà không có hàm giải phóng; cộng số học trên `void*`. `cqueue` không thread-safe.

Đề xuất sửa tài liệu (chờ chủ dự án duyệt, phiên 17 chưa sửa):

- Tên thư mục chip: repo là `LoganSDK/platform/esp32s3` (số ít), `Firmware_Construction.md` ghi `platforms/`. Tên file: repo là `HandOver.md`, `RuleCode.md`, một số chỗ còn viết `handover.md`, `rule_code.md`.
- `Firmware_Construction.md` mục 1 liệt kê app `screen, speaker, AI, http, ntp`: thiếu `wifi`, `storage`, `mic`.
- `app.h` còn comment lỗi thời ("for now it only starts Wi-Fi").
- `RuleCode.md` mục 4 nhắc "Engineering rules trong handover" nhưng handover không có mục đó (quy tắc nằm ở `RuleAgent.md`).
- `RuleAgent.md` mục 5 và `RuleCode.md` mục 3 ghi "không xóa mục cũ" trong nhật ký; handover đã rút gọn theo yêu cầu của chủ dự án (phiên 17), nên nên đổi thành: handover chỉ giữ hiện tại và tương lai gần, lịch sử để ở git.

## 7. Kế hoạch tiếp theo

Mỗi bước phải build được trước khi sang bước sau.

1. Mic: chạy bản live monitor của phiên 19 (mục 6, việc 1), đánh giá chất lượng thu theo ba vùng; chỉnh `APP_MIC_LIVE_GAIN`, `APP_MIC_LIVE_VOLUME`, ngưỡng howl guard theo kết quả. Chưa biết hai cổng I2S (mic cổng 1, loa cổng 0) có lệch nhịp khi chạy vòng đọc-ghi liên tục hay không; nếu nghe rè hoặc thấy lỗi đọc thì phải xem lại. Loa đã nghe được; chỉnh `APP_SPEAKER_TONE_VOLUME` theo độ to chủ dự án báo. Cân nhắc thêm vào `mic_app` một kết luận "gần như toàn bộ khối chạm full scale: dữ liệu nhiều khả năng không phải âm thanh" (hiện code chỉ biết "toàn không", "không đổi", "có biến thiên", nên đã báo ok với dữ liệu nhiễu). Chưa làm, chờ chủ dự án duyệt.
2. SD và storage khi có thẻ (mục 6, việc 3).
3. Quyết định PSRAM (kiểm loại bằng `CONFIG_SPIRAM` và log boot), rồi LVGL: flush callback của LVGL nên chờ bằng callback hoàn tất của bus I80 thay cho bước chờ 1 ms của `fill_rect`; đọc input từ touch; không block task LVGL bằng network.
4. Thử `BOARD_LCD_PCLK_HZ` cao hơn (quan sát nhiễu, sọc; số ms trong log sẽ không đổi nhiều, xem mục 5).
5. AI backend khi có đặc tả; sau đó OTA (bật rollback `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`, viết service OTA) và các phase theo `Product_Construction.md`.
6. Phần còn lại cho Wi-Fi: thử router WPA3 nếu có điều kiện; đo heap khi gọi HTTPS lặp nhiều lần.

## 8. Ghi chú kỹ thuật cần nhớ

- Màn hình tối sau `board_init_screen()` cho tới khi gọi `screen_hmi_set_backlight` (`screen_app` đã gọi). `screen_hmi_fill_rect` chờ bằng busy wait theo bước 1 ms.
- `sx_i2c`: lệnh đọc/ghi không phân biệt NACK với lỗi bus; dùng `sx_i2c_probe`. `sx_spi`: `timeout_ms` không có hiệu lực trên ESP32-S3.
- `sx_delay_ms`: ở `CONFIG_FREERTOS_HZ=100` (tick 10 ms), chờ dưới 20 ms là busy wait, từ 20 ms nhường CPU. `sx_get_time_ms` quay vòng sau khoảng 49,7 ngày: luôn so thời gian bằng phép trừ số không dấu.
- `sx_i2s`: `-1` (`SX_I2S_PIN_UNUSED`) là không dùng hướng đó; loa và mic ở hai cổng I2S khác nhau (0 và 1). Loa xuất mẫu 0 khi hết dữ liệu.
- `sx_wifi`: AP IP tĩnh `192.168.4.1`, STA dùng DHCP, tự thử nối lại; `sx_wifi_scan` để chẩn đoán. In SSID kèm dấu ngoặc kép khi debug (lỗi từng gặp: dấu cách cuối SSID làm `reason=201` lặp mãi).
- Bộ nhớ: static là mặc định; heap chỉ cho LVGL, network, buffer lớn, cấp phát lúc khởi tạo. Bài thu của `mic_app` lấy 64000 byte heap trong RAM trong, một lần.
- Windows: Git không nhận đổi hoa/thường tên file, hãy dùng lệnh `git mv` rõ ràng. Biến môi trường `IDF_TARGET=esp32` trong shell làm `set-target esp32s3` báo lệch; lệnh `idf.py set-target esp32-s3` chạy được. Chủ dự án tự quản `sdkconfig`, tự build và flash.
- Sandbox của AI (không truy cập máy chủ dự án): làm trên bản clone, present từng file, không present patch. Mạng sandbox chặn `dl.espressif.com`; ESP-IDF cài qua github.com (clone `v5.4.3 --recursive`, `install.sh esp32s3`) với `IDF_PYTHON_CHECK_CONSTRAINTS=no`, Python requirements từ PyPI, `cmake==3.30.5` và `ninja` bằng pip. Build thử trên bản sao, không `set-target` trong repo thật. Bài test host nằm ngoài repo, chỉ chứng minh logic khớp mô hình giả lập do phiên viết.

## 9. Nhật ký rút gọn (chi tiết: lịch sử git)

- Phiên 1 (11-04): khung kiến trúc theo simple_plc; `sx_gpio`, `sx_i2c`, `sx_spi`, `sx_i80`; các file docs.
- Phiên 2: `sx_i2s` cho loa TX. Phiên 3: nối CMake các layer, build đầu thành công trong sandbox.
- Phiên 4: `sx_pwm`, `sx_uart`, ngắt `sx_gpio`. Phiên 5: sửa `esp32s3_gpio.c`; module `sdcard` (thẻ SD chuyển từ peripheral lên module).
- Phiên 6 (11-05): `screenHMI`, `touch`, `services/storage` với FatFs, `sx_time`; chủ dự án build thành công lần đầu. Phiên 7: đối chiếu repo, dọn mục chờ.
- Phiên 8 (11-07): `sx_i2s` thêm RX và sửa lỗi timeout, module `inmp441`. Phiên 9: kiểm chứng, build sandbox thành công. Phiên 10: module `speaker`.
- Phiên 11 (11-08): `sx_wifi`, `services/http`, `wifi_app`. Phiên 12: tìm ra lỗi dấu cách cuối SSID, thêm `sx_wifi_scan`. Phiên 13: tự nối lại sau khi rút router, `http_app`.
- Phiên 14: `board.h`, `board.c`, `board_init_xxx`; HTTPS GET chạy trên board.
- Phiên 15: `screen_app`; màn hình đúng màu, đúng hướng (đảo màu bật). Phiên 16: bài thử touch (chạy được, sửa trục Y), flash 16 MB và `partitions.csv` 2 slot OTA, `storage_app` (chưa gọi).
- Phiên 17 (11-09): đối chiếu repo (HandOver chưa có `audio_app` của commit `271fe1c`); tách `audio_app` thành `speaker_app` và `mic_app` (host, chưa build); rút gọn HandOver.
- Phiên 19 (11-10): `mic_app` thêm live monitor (1 dòng mỗi giây, ba vùng tần số bằng biquad số nguyên, phát lại thẳng ra loa kèm howl guard), `APP_MIC_RECORD_MS` mặc định 0. Làm lại trên bản clone (bản dán từ một phiên khác không có trong repo; hệ số biquad được tính lại và trùng). Kiểm tra: biên dịch host `-Wall -Wextra -Wshadow -Wconversion -Wpedantic -Werror` bốn cấu hình; test logic host dưới ASan/UBSan (bộ lọc khớp đáp ứng giải tích trong 4 phần trăm cộng 2 đơn vị, DC bị loại, howl guard, chế độ không phát). **Chưa build ESP-IDF, chưa chạy trên board.**
- Phiên 18 (11-10): nhận log chạy `speaker_app` và `mic_app` trên board (bản `f7ae25e`): loa nghe được, bảng phân vùng 16 MB do bootloader in đúng, mic có dữ liệu nhưng L/R thả nổi nên chưa đáng tin; chưa sửa code.