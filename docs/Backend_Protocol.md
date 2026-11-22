# Backend Protocol - AI Learning Device

> Trạng thái: **v0.1.** Phiên 23 (sau khi chủ dự án trả lời): D7 đến D12 đã chốt theo đề xuất (mục 2). Các mục 3 đến 9 vẫn là DESIGN chờ thử. Chưa có dòng code nào theo tài liệu này.
> Tài liệu này là hợp đồng giữa firmware (ESP32-S3) và backend (server). Cả hai bên cùng tuân theo file này.
> Soạn: phiên 23 (2026-11-22). Bổ sung cho `Product_Construction.md` mục 6 đến 8 và `Firmware_Construction.md`.

Nhãn dùng trong tài liệu (giống tinh thần `board.h`):

- **VERIFIED-HDR**: đã đối chiếu header ESP-IDF v5.4.3 trong phiên 23.
- **HW**: đã chạy trên board (theo log chủ dự án).
- **DESIGN**: do phiên thiết kế chọn, chưa có số đo hoặc nguồn. Chỉ là giá trị khởi điểm để thử.
- **DECIDED**: chủ dự án đã quyết trong phiên 23.
- **OPEN**: chờ chủ dự án quyết. Không được coi là đã chốt.
- **UNVERIFIED**: chưa kiểm, không được coi là sự thật.

## 1. Phạm vi

Trong phạm vi bản v0:

- Một lượt nói: người dùng chạm màn hình, nói, thiết bị gửi âm thanh lên server theo luồng, server trả chữ nhận dạng (STT).
- Khung để mở rộng sang lượt trả lời (LLM, TTS, hành động trên giao diện) mà không đổi hợp đồng.

Ngoài phạm vi v0 (không làm, không thiết kế chi tiết):

- REST cho bài học, quiz, tiến độ (xem `Product_Construction.md` mục 7.6, làm ở giai đoạn riêng).
- Opus, OTA, nhận diện giọng, từ khóa đánh thức, nói chen khi loa đang phát (AEC).
- Backend nhiều người dùng, tài khoản, thanh toán.

## 2. Quyết định đã chốt và quyết định còn mở

| # | Nội dung | Trạng thái |
|---|---|---|
| D1 | Kích hoạt bằng **chạm màn hình để bắt đầu nói** (không từ khóa đánh thức, không VAD luôn nghe) | DECIDED |
| D2 | **Vẫn có nút dừng** (chạm lần nữa). Thêm cơ chế tự nhận biết nói xong làm lưới an toàn để phòng quên bấm, không thay nút | DECIDED |
| D3 | Âm thanh **stream từng khối**, không giữ cả đoạn nói trong RAM | DECIDED |
| D4 | Bán song công: mic bật thì loa tắt, loa phát thì mic không gửi. Hủy lúc loa phát bằng chạm màn hình | DECIDED (hướng), số liệu chi tiết OPEN |
| D5 | Truyền tải: một kết nối WebSocket, JSON cho tin điều khiển, nhị phân cho âm thanh | DESIGN (chờ duyệt) |
| D6 | Định dạng âm thanh v0: PCM 16 bit, mono, 16 kHz, little endian | DESIGN (khớp `inmp441` và `speaker` hiện có) |
| D7 | Khung âm thanh có tiêu đề 4 byte (loại, cờ, số thứ tự) | DECIDED (phiên 23) |
| D8 | Phân tích JSON trên thiết bị: dùng cJSON (`libs/cJSON`, thêm vào `SRCS` của `libs`). Mức RAM thật chưa đo | DECIDED (phiên 23) |
| D9 | Danh sách `action` ban đầu: để trống ở v0, thiết bị bỏ qua `action` lạ | DECIDED (phiên 23) |
| D10 | Riêng tư cho bài thử: chạy trong LAN, STT cục bộ, không gửi âm thanh ra ngoài, không lưu âm thanh lâu dài, chữ chỉ in ra console. Chính sách đầy đủ (lưu bao lâu, ai xóa, phụ huynh) vẫn OPEN | DECIDED cho bài thử, OPEN cho sản phẩm |
| D11 | Bộ vẽ chữ tự viết, có dấu tiếng Việt (xem mục 11). Font cụ thể OPEN | DECIDED (hướng), font OPEN |
| D12 | STT chạy cục bộ trên PC dev. Mô hình cụ thể chọn sau khi đo trên WAV thu từ mic của chủ dự án (xem mục 12) | DECIDED (hướng), mô hình OPEN |

## 3. Truyền tải

- Giao thức: WebSocket. Dev: `ws://<ip-may-chu>:<cong>/v1/voice` trong LAN (mạng router `192.168.30.0/24`, theo HandOver). Máy dev (GIVEN, chủ dự án gửi phiên 23): `192.168.30.101` (IPv4 do DHCP cấp, có thể đổi sau khi khởi động lại). Cổng: đề xuất 8765 (DESIGN, chưa duyệt).
- Bản thật: `wss://` có kiểm chứng chỉ (`http_svc` hiện luôn kiểm CA bundle, WebSocket phải theo cùng nguyên tắc). Không có chế độ tắt kiểm chứng ở bản thật.
- Thiết bị là client, server là bên lắng nghe.
- Cách hiện thực trên firmware (đề xuất, cần duyệt): dùng `tcp_transport` có sẵn trong ESP-IDF và API `esp_transport_ws_*`, đặt trong `services/AI`. **VERIFIED-HDR (v5.4.3):** header `esp_transport_ws.h` có `esp_transport_ws_set_path`, `_set_subprotocol`, `_set_headers`, `_set_auth`, `_set_config`, `_send_raw` (chọn opcode TEXT hoặc BINARY), `_get_read_opcode`, `_get_fin_flag`, `_get_read_payload_len`, `_poll_connection_closed`. Cấu hình có cờ `propagate_control_frames`: nếu `false`, khung điều khiển do transport tự xử lý khi đọc.
- Component `esp_websocket_client` **không nằm trong ESP-IDF v5.4.3** (đã kiểm: không có trong cây `components/`). Dùng nó là thêm dependency mới, cần duyệt. Đề xuất không dùng.
- Thêm `tcp_transport` vào `REQUIRES` của `services` là thay đổi CMake, cần chủ dự án duyệt.
- **UNVERIFIED (cần đọc `transport_ws.c` trước khi code):** xử lý khung bị phân mảnh khi nhận, hành vi khi mất kết nối giữa chừng, tự trả lời ping.

## 4. Khung dữ liệu

- Khung **TEXT**: một đối tượng JSON, UTF-8, có trường `type`. Thiết bị và server **bỏ qua** tin có `type` lạ và trường lạ (để mở rộng về sau không phá bản cũ).
- Khung **BINARY**: âm thanh, theo mục 5.
- **Văn bản tiếng Việt (DECIDED phiên 23):** server chuẩn hóa mọi chữ gửi xuống về **NFC** (chữ có dấu là ký tự dựng sẵn, không tách dấu rời) và gửi JSON UTF-8 nguyên, **không escape `\uXXXX`** (Python: `ensure_ascii=False`). Lý do: bộ vẽ chữ trên thiết bị tra bảng theo ký tự dựng sẵn.

### 4.1. Tin điều khiển (JSON)

Thiết bị gửi lên server:

| `type` | Trường | Gửi khi |
|---|---|---|
| `hello` | `protocol_version` (số nguyên, v0 = 0), `device_id`, `fw`, `audio` (`codec`, `rate`, `channels`, `frame_ms`) | Ngay sau khi mở kết nối |
| `listen_start` | `turn` (số nguyên tăng dần do thiết bị đặt), `reason` = `"touch"`, `lang` = `"vi"` hoặc `"en"` (DECIDED phiên 23; thiết bị đặt, server dùng chọn ngôn ngữ STT) | Người dùng chạm bắt đầu nói |
| `listen_stop` | `turn`, `reason` = `"touch"`, `"vad"`, `"timeout"`, `"max_length"` | Thiết bị dừng gửi âm thanh của lượt này |
| `abort` | `turn` | Hủy lượt (chạm hủy lúc chờ hoặc lúc loa đang phát) |

Server gửi xuống thiết bị:

| `type` | Trường | Gửi khi |
|---|---|---|
| `hello_ack` | `protocol_version`, `session_id` | Trả lời `hello`. Không khớp phiên bản thì gửi `error` rồi đóng |
| `listen_end` | `turn`, `reason` = `"server_vad"` | Server đã đủ dữ liệu và yêu cầu thiết bị ngừng gửi (lưới an toàn thứ hai) |
| `stt_result` | `turn`, `text`, `final` (bool) | Có chữ nhận dạng. v0 chỉ dùng `final: true`. `final: false` (từng phần) để dành, chưa dùng |
| `error` | `turn` (có thể thiếu), `code`, `message` | Lỗi (xem mục 8) |

Dành sẵn cho giai đoạn trả lời (chưa dùng ở v0, chưa chốt trường): `say` (chữ trả lời), `tts_start`, `tts_end`, `action` (hành động trên giao diện, thuộc danh sách D9).

`device_id`: đề xuất lấy từ MAC. **VERIFIED-HDR:** `esp_mac.h` có `esp_read_mac()` (kiểu `ESP_MAC_WIFI_STA`) và `esp_efuse_mac_get_default()`. Quy định chuỗi định dạng (ví dụ 12 ký tự hex): DESIGN. Chỉ layer `platform/esp32s3` được include header ESP-IDF, nên việc đọc MAC phải đi qua hợp đồng `sx_*` (cần thêm API, cần duyệt).

Ví dụ (minh họa, không phải log thật):

```text
device -> server  {"type":"hello","protocol_version":0,"device_id":"AABBCCDDEEFF","fw":"e92909b","audio":{"codec":"pcm_s16le","rate":16000,"channels":1,"frame_ms":20}}
server -> device  {"type":"hello_ack","protocol_version":0,"session_id":"s1"}
device -> server  {"type":"listen_start","turn":1,"reason":"touch"}
device -> server  <khung nhi phan am thanh> ... <khung nhi phan am thanh>
device -> server  {"type":"listen_stop","turn":1,"reason":"touch"}
server -> device  {"type":"stt_result","turn":1,"text":"hello world","final":true}
```

## 5. Âm thanh

- Hướng lên (mic): PCM signed 16 bit little endian, mono, 16 kHz. **HW:** đây là định dạng `inmp441` đang xuất (lấy 16 bit trên của mẫu 24 bit, tại 16000 Hz) và `speaker` đang nhận. Ngoài ra nên bỏ DC và lọc cao thông 100 Hz trước khi gửi (như `mic_app` đã làm cho bản phát lại), việc này làm ở firmware.
- Khối: 20 ms = 320 mẫu = 640 byte (DESIGN). Khối lớn hơn (40 ms) giảm số gói nhưng tăng độ trễ.
- Lưu lượng: 16000 mẫu/s x 2 byte = 32 KB/s mỗi chiều, khoảng 256 kbps (tính, chưa đo trên board).
- Hướng xuống (TTS, giai đoạn sau): cùng định dạng, server tự đổi tần số lấy mẫu cho khớp. **Quy tắc (DECIDED phiên 23):** server gửi âm thanh về đúng nhịp thực (20 ms mỗi 20 ms), không gửi dồn, vì bộ đệm phát trên thiết bị có hạn. Một dự án bên thứ ba ghi bộ đệm phát của họ là 1,5 s; đó là số của họ, không phải của board này, bộ đệm của board này chưa quyết.
- Trường `codec` trong `hello` dành cho Opus về sau. v0 chỉ có `pcm_s16le`.

Khung nhị phân (D7, DECIDED phiên 23: có tiêu đề): 4 byte tiêu đề rồi tới PCM.

| Byte | Nội dung |
|---|---|
| 0 | Loại: `0x01` = mic lên server, `0x02` = TTS xuống thiết bị |
| 1 | Cờ (bit 0 = khối đầu của lượt, bit 1 = khối cuối). Để 0 nếu chưa dùng |
| 2-3 | Số thứ tự khối, uint16, little endian. Dùng để phát hiện mất hoặc đảo khối |

Nếu chủ dự án không muốn tiêu đề: bỏ 4 byte, khung chỉ là PCM thô và biên lượt dựa hoàn toàn vào tin JSON.

## 6. Lượt nói và máy trạng thái của thiết bị

```text
IDLE
  | chạm nút NÓI
  v
LISTENING   (mic gửi khối; loa tắt)
  | chạm nút DỪNG        -> reason "touch"
  | VAD thấy nói xong     -> reason "vad"      (lưới an toàn, mục 7)
  | không ai nói quá lâu  -> reason "timeout"
  | quá thời gian tối đa  -> reason "max_length"
  | nhận listen_end       -> (server đã đủ)
  v
WAITING     (đã gửi listen_stop; chờ stt_result; hiện "đang xử lý")
  | stt_result final      -> hiển thị chữ
  | error hoặc quá hạn chờ -> hiển thị lỗi
  v
IDLE
```

Giai đoạn sau thêm `SPEAKING` giữa `WAITING` và `IDLE` (loa phát, mic không gửi, chạm để `abort`).

Quy tắc:

- Mỗi lượt có số `turn` riêng. Thiết bị bỏ qua mọi tin của `turn` cũ.
- Thiết bị chỉ mở một lượt tại một thời điểm.
- **Mic và I2S RX chạy liên tục, chỉ khóa việc gửi theo trạng thái** (đề xuất, DESIGN). Lý do (từ số đo phiên 21 đến 22 trong HandOver): sau mỗi `inmp441_start` có xung DC lớn tắt dần trong khoảng 2 đến 4 s. Nếu bật và tắt mic mỗi lượt, người dùng sẽ nói trúng giai đoạn xung này. Giữ mic chạy mà chỉ đổi trạng thái gửi tránh được việc đó. Đổi lại mic chạy liên tục tốn một ít điện và CPU; chưa đo.
- Nên giữ một vòng đệm ngắn các khối vừa đọc để khi chạm có thể gửi kèm vài chục ms trước đó (tương tự pre-roll 60 ms `mic_app` đang có), tránh mất đầu từ.

## 7. Tự nhận biết nói xong (lưới an toàn, không thay nút dừng)

Nút dừng (D2) là đường chính. Cơ chế tự động chỉ để phòng quên bấm, gồm ba lớp độc lập, lớp nào xảy ra trước thì kết thúc lượt:

1. **VAD năng lượng trên thiết bị.** Đo năng lượng khung ngắn sau khi bỏ DC và lọc cao thông, theo dõi mức nền ồn thích nghi. Máy trạng thái: chờ tiếng nói, đang nói, rồi im lặng đủ lâu (khoảng giữ) thì phát `listen_stop` với `reason: "vad"`. Chỉ tính là "đang nói" khi năng lượng vượt nền liên tục đủ lâu, để tiếng động ngắn (đồ rơi) không bị coi là tiếng nói.
2. **VAD ở server.** Server tự phán đoán độc lập và có quyền gửi `listen_end` sớm hơn. Đây là phán quyết cuối vì server dùng được mô hình mạnh hơn.
3. **Giới hạn thời gian.** Tối đa cho một lượt, và hủy nếu bấm NÓI mà không có tiếng nói trong một khoảng.

Giá trị khởi điểm để thử (DESIGN, **không có số đo nào**, phải chỉnh bằng tai và log):

| Tham số | Khởi điểm | Ghi chú |
|---|---|---|
| Khoảng im lặng để kết thúc | 1500 ms | Ngắn thì cắt nhầm khi bé ngừng suy nghĩ, dài thì phản hồi chậm |
| Chờ tiếng nói đầu tiên | 6000 ms | Hết hạn thì `reason: "timeout"` |
| Độ dài tối đa một lượt | 15000 ms | Hết hạn thì `reason: "max_length"` |
| Chờ `stt_result` sau `listen_stop` | 10000 ms | Cùng cỡ `HTTP_SVC_TIMEOUT_MS_DEFAULT`, cũng là giá trị theo thiết kế, chưa đo |

Số tham chiếu có sẵn trong HandOver (từ log chạy của chủ dự án, không phải kết quả của tài liệu này): sau lọc cao thông 100 Hz, nền ồn khi im lặng khoảng 8 đến 19, khoảng 70 đến 85 khi có tiếng tủ lạnh ở xa, tiếng nói ở cỡ 30 cm có đỉnh khoảng 1000 đến 4500. Chênh lệch lớn nên VAD năng lượng khả thi, nhưng ngưỡng cụ thể cần đo.

Hạn chế đã biết (không tự giải quyết được ở thiết bị): hai người khác nói gần nhau cùng lúc. Mic mono chỉ cho một luồng trộn. Với trường hợp này cổng tay (chạm hoặc giữ) là phương án chắc nhất; lớp VAD chỉ là lưới an toàn.

## 8. Lỗi, thời gian chờ, nối lại

Mã lỗi (`error.code`, DESIGN, đề xuất):

| `code` | Ý nghĩa |
|---|---|
| `bad_version` | `protocol_version` không khớp |
| `bad_frame` | Khung sai định dạng (JSON hỏng, khung âm thanh sai kích thước) |
| `stt_failed` | Dịch vụ nhận dạng lỗi |
| `no_speech` | Không nhận ra tiếng nói (chữ rỗng) |
| `busy` | Server đang bận lượt khác |
| `internal` | Lỗi khác của server |

Hành vi:

- Thiết bị hiển thị lỗi rồi về `IDLE`. Không tự thử lại lượt đã mất (người dùng nói lại).
- Mất kết nối: thiết bị tự nối lại (chiến lược chờ tăng dần, số cụ thể OPEN), gửi lại `hello`. Lượt đang dở bị bỏ. Wi-Fi đã có cơ chế tự nối lại (`sx_wifi`, HW), phần này chỉ lo kết nối WebSocket.
- Server phải chịu được thiết bị gửi khối sau khi đã gửi `listen_stop` (do độ trễ). Bỏ qua khối của lượt đã đóng.

## 9. Xác thực và định danh

- Dev trong LAN: không xác thực. Ghi rõ đây chỉ là chế độ phát triển.
- Bản thật: `wss://` kèm token riêng của thiết bị trong tiêu đề kết nối (`esp_transport_ws_set_auth` hoặc `_set_headers`, VERIFIED-HDR). Cách cấp phát và lưu token (NVS?) OPEN.
- Khóa API của STT, LLM, TTS **chỉ nằm ở server**, không bao giờ trong firmware hay repo.
- Ghi chú: mật khẩu Wi-Fi hiện nằm trong code (HandOver mục 4). Đây là việc cần dọn trước khi phát hành, ngoài phạm vi tài liệu này.

## 10. Riêng tư và an toàn cho trẻ em

Bài thử (D10, DECIDED phiên 23): chạy trong LAN, STT cục bộ, âm thanh không đi ra ngoài, không lưu lâu dài, chữ chỉ in ra console. Chính sách cho sản phẩm vẫn OPEN. Điểm xuất phát đề xuất, chờ chủ dự án đặt:

- Mặc định server không lưu âm thanh.
- Chữ nhận dạng lưu hay không, bao lâu, ai xóa, kiểm soát phụ huynh thế nào: chủ dự án quyết.
- Khi dùng dịch vụ bên thứ ba cho STT, LLM, TTS, âm thanh và nội dung của bé sẽ đi tới họ: phải được chủ dự án chấp nhận rõ ràng trước khi dùng (xem `Product_Construction.md` mục 8.2 và 8.3). Điều này gồm cả server chính thức xiaozhi.me: firmware xiaozhi nối mặc định tới đó (theo nguồn web, chưa đọc mã nguồn), nên dùng nó nghĩa là âm thanh của bé đi tới dịch vụ ngoài.
- Lớp an toàn nội dung nằm ở server (giai đoạn LLM). Thiết bị chỉ thực hiện `action` thuộc danh sách cho phép (D9), `action` lạ bị bỏ qua.

## 11. Hiển thị chữ trên màn hình (D11)

**Phát hiện từ repo (phiên 23):** `screenHMI` chỉ có `screen_hmi_fill_rect` và `screen_hmi_draw_bitmap`. Repo không có font hay hàm vẽ chữ.

**Cập nhật phiên 23 (sau khi chủ dự án trả lời): bài thử phải in được tiếng Việt có dấu.** Bản v0 ghi hướng ASCII là không đủ. Hướng chọn: bộ vẽ chữ tự viết (hướng A) có dấu tiếng Việt. LVGL (hướng B) vẫn là hướng lâu dài cho giai đoạn UI, không làm ở bài thử.

Việc cần có:

1. **Font có dấu.** 95 ký tự ASCII cộng 134 ký tự ngoài ASCII. Cách tính 134: 12 nguyên âm (a, ă, â, e, ê, i, o, ô, ơ, u, ư, y) x 6 dạng (không dấu và 5 dấu thanh) x 2 kiểu hoa thường = 144, trừ 12 ký tự đã là ASCII, cộng đ và Đ. Tổng 229 ký tự x 48 byte (ô 16x24, 1 bit) = 10992 byte, khoảng 11 KB flash (**tính, chưa đo**). Chuyển TTF thành mảng C bằng script Python **ngoài firmware**. Font cụ thể chưa chọn. **Phải kiểm giấy phép của font trước khi đưa vào repo** (chưa kiểm). Rủi ro chưa kiểm: dấu chồng (ệ, ữ, Ấ) có thể nhìn rối ở ô 16x24 1 bit.
2. **Bộ vẽ chữ.** Giải mã UTF-8, tra bảng, vẽ qua `screen_hmi_draw_bitmap`. Theo bảng phụ thuộc, `modules` không được `REQUIRES` module khác, nên bộ vẽ chữ nhận hàm vẽ bitmap qua con trỏ hàm (cùng kiểu `screen_hmi_delay_fn_t`) thay vì phụ thuộc `screenHMI`. **Là thành phần mới, cần chủ dự án duyệt trước khi viết.** Màn hình 480x320 với ô 16x24 cho 30 cột x 13 dòng (tính từ kích thước).
3. **Server** chuẩn hóa NFC và gửi JSON UTF-8 nguyên (mục 4).
4. **`lang`** trong `listen_start` (mục 4.1).

## 12. Server

- Ngôn ngữ đề xuất: Python (dễ đọc, nhiều thư viện âm thanh). Đặt ở thư mục mới ngoài `LoganSDK` (ví dụ `backend/`), **cần chủ dự án duyệt** trước khi tạo.
- Cấu trúc: mỗi khâu (VAD, STT, sau này LLM và TTS) là một module cùng hợp đồng vào ra, nguồn (cục bộ hay API) chọn bằng cấu hình. Cách chia này lấy ý từ server cộng đồng của xiaozhi (theo tìm hiểu phiên 22 và 23, chưa tự đọc mã nguồn của họ).
- STT cho bài thử (D12, DECIDED hướng phiên 23): chạy cục bộ trên PC dev, âm thanh không ra khỏi LAN. Mô hình cụ thể **OPEN**, chọn sau khi đo trên WAV thu từ chính mic của chủ dự án.
- **Ngôn ngữ chính là tiếng Việt** (phiên 23). Thông tin tra được (nguồn web, **chưa kiểm chứng, chưa đo**):
  - README server xiaozhi cộng đồng chỉ liệt kê FunASR hỗ trợ Quan Thoại, Quảng Đông, Anh, Nhật, Hàn. Chủ dự án thấy xiaozhi nhận được tiếng Việt, nên đó là mô hình mặc định của server cộng đồng chứ không phải giới hạn của xiaozhi.
  - Firmware xiaozhi nối mặc định tới server chính thức xiaozhi.me, người dùng cá nhân dùng mô hình Qwen thời gian thực miễn phí. Mô hình nào chạy cho tiếng Việt và ai trả chi phí: **chưa có nguồn**.
  - Một dự án bên thứ ba cho thiết bị tiếng Việt dùng Qwen3-ASR và VieNeu TTS.
  - Ứng viên khác: Whisper tinh chỉnh cho tiếng Việt (PhoWhisper, EraX-WoW-Turbo; README ghi huấn luyện trên giọng người lớn, có thể khó với giọng trẻ nhỏ), Zipformer-vi qua sherpa-onnx (khoảng 75 MB, chạy CPU).
  - Mọi số WER là do tác giả tự báo, chưa đo trên mic và giọng của chủ dự án, chưa có số cho giọng trẻ em.
- PC dev (GIVEN, chủ dự án gửi): Ryzen 5 6600H, RAM 16 GB (15,2 GB dùng được), NVIDIA RTX 3050 Ti Laptop 4 GB VRAM, Windows, Python đã cài (phiên bản chưa báo), IPv4 `192.168.30.101` (DHCP, có thể đổi). Dùng để chọn mô hình: từng ứng viên phải được kiểm kích thước trước khi cài.
- Thiết bị giả bằng Python (phát một file WAV lên server) để kiểm server khi chưa có board.

## 13. Bài thử đầu tiên (đề xuất, chờ duyệt)

Mục tiêu theo yêu cầu của chủ dự án: có nút trên màn hình để nói, gửi lên server, sau khi dừng thì in đoạn chữ đã nói lên màn hình.

Chia nhỏ, mỗi bước có tiêu chí qua và chủ dự án tự build, gửi log:

| Bước | Việc | Tiêu chí qua |
|---|---|---|
| 0 | Duyệt tài liệu này và các mục OPEN | **Xong phiên 23:** chủ dự án chốt D7 đến D12 theo đề xuất |
| 1 | Server Python echo cộng thiết bị giả (WAV) | Server nhận khung, ghi ra file WAV nghe được, trả `stt_result` giả |
| 2 | Firmware: nút chạm trên màn hình, máy trạng thái, VAD, vẽ chữ. **Chưa có mạng**, in log độ dài lượt và lý do kết thúc | Chạm bắt đầu, chạm dừng, VAD cắt đúng ở 30 cm trong phòng yên (tiêu chí số: chủ dự án đặt) |
| 3 | Firmware nối server echo, stream khối PCM | Nghe lại bản server lưu: sạch, đủ, không mất khối (số thứ tự liên tục); đo độ trễ và mất gói |
| 4a | Thử vài mô hình STT offline trên PC bằng WAV thu từ mic board (từng mô hình một, theo dõi RAM và VRAM) | Chọn được mô hình theo số đo trên giọng của chủ dự án; ghi thời gian xử lý |
| 4 | Server thêm STT thật (mô hình đã chọn ở 4a) | `stt_result` đúng với câu nói tiếng Việt (có `lang`) |
| 5 | Màn hình in `text` nhận được, tiếng Việt có dấu | Đoạn nói hiện đúng, đủ dấu, trên màn hình sau khi dừng |

Cần chủ dự án duyệt trước khi bước 2 và 3 bắt đầu (tổng hợp): `tcp_transport` vào `REQUIRES` của `services`, API đọc MAC trong `sx_*`, cách phân tích JSON (D8), bộ đệm giữa mic và mạng (StreamBuffer của FreeRTOS hoặc vòng đệm ở PSRAM), bộ vẽ chữ (D11), thư mục `backend/`.

## 14. Câu hỏi còn mở (tổng hợp)

Cập nhật phiên 23: câu 1 (IP máy dev: `192.168.30.101`, cùng LAN), câu 2 đến 5 và câu 6 (D7 đến D11), câu 7 (D12: cục bộ) đã trả lời ở mục 2. Còn mở: font (D11), mô hình STT (D12), chính sách riêng tư cho sản phẩm (D10), cổng của server (đề xuất 8765, DESIGN), chế độ truy cập PSRAM (đề xuất `CONFIG_SPIRAM_USE_CAPS_ALLOC` kèm `heap_caps_malloc(MALLOC_CAP_SPIRAM)`, chủ dự án chưa quyết), và các câu 8, 9, 10 dưới đây. Danh sách gốc của bản v0:


1. IP và cổng của máy dev; máy dev là PC Windows của chủ dự án trong cùng LAN với board?
2. D7: có tiêu đề 4 byte cho khung âm thanh không?
3. D8: bật cJSON hay định dạng tối giản?
4. D9: danh sách `action` ban đầu.
5. D10: chính sách riêng tư.
6. D11: hướng vẽ chữ (A hay B).
7. D12: STT cục bộ hay API.
8. Tiêu chí số cho "VAD cắt đúng" ở bước 2.
9. Bộ đệm giữa mic và mạng: cỡ bao nhiêu, đặt ở RAM trong hay PSRAM (đo ở bước 3 mới biết jitter).
10. `board.h` chưa khai báo chân/cổng cho gì mới. Tài liệu này không đòi thay đổi chân nào.