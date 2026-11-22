# backend - bước 1 (server echo và thiết bị giả)

Theo `docs/Backend_Protocol.md` mục 13, bước 1. Chưa có nhận dạng giọng nói: `stt_result` là **kết quả giả**
(chữ cố định kèm số liệu của lượt). Mục đích bước này là kiểm giao thức, khung âm thanh và việc ghi WAV.

Môi trường: Python 3.11 trở lên, thư viện `websockets` bản 17.3 (`requirements.txt`).

## Cài (Windows, một lần)

```text
cd E:\prj_canhan\Learning_board\board_hmi_ai\backend
python -m venv .venv
.venv\Scripts\activate
pip install -r requirements.txt
```

## Chạy

Cửa sổ 1 (server, mặc định lắng nghe cổng 8765 trên mọi card mạng):

```text
.venv\Scripts\activate
python server.py
```

Lần đầu Windows có thể hiện hộp thoại Firewall: chọn Allow trên mạng riêng (Private). Địa chỉ cho board sau này:
`ws://192.168.30.101:8765/v1/voice` (IP do DHCP cấp, kiểm lại bằng `ipconfig` nếu đổi).

Cửa sổ 2 (thiết bị giả):

```text
.venv\Scripts\activate
python fake_device.py --tone 3
```

Các kịch bản thử khác (xem đầu file `fake_device.py`): `--skip-frame 10` (server phải báo mất 1 khối),
`--late-frames 5` (khung sau `listen_stop` phải bị bỏ qua), `--abort-after 1` (server xóa lượt),
`--version 9` (server phải trả `bad_version`), `--wav file.wav` (WAV phải là PCM 16 bit, mono, 16000 Hz).

## Kết quả mong đợi

- Server in `turn 1 stopped ... 150 frames, 3.00 s ... missing 0`.
- Thiết bị giả in `stt_result` với chữ tiếng Việt có dấu (kiểm UTF-8 và NFC; nếu console Windows hiển thị
  lỗi dấu thì chạy `chcp 65001` trước, nội dung gửi đi không bị ảnh hưởng).
- Thư mục `recordings\` có file WAV 3.000 s (nghe được bằng trình phát của Windows, là âm 440 Hz).

## Lưu ý

- `recordings\` chứa âm thanh thu được và **không đưa vào git** (`.gitignore`). Đây là file thử, xóa sau khi
  dùng (D10: không lưu âm thanh lâu dài). `--no-save` để tắt việc ghi file.
- Ping của WebSocket tắt mặc định (`--ping-interval`), chờ đọc `transport_ws.c` của ESP-IDF v5.4.3.
- Server chưa có VAD, chưa gửi `listen_end`, chưa có STT.
