# Rule Agent - quy tắc cho AI/Agent làm việc trong dự án

Áp dụng cho mọi AI hoặc agent (Claude, Claude Code, hay AI khác) làm việc trên dự án AI Learning Device.
File này nói **cách làm việc**. Quy tắc viết code và bàn giao file ở `rule_code.md`; kiến trúc ở `Firmware_Construction.md`;
trạng thái và kế hoạch ở `handover.md`.

## 1. Vai trò

Senior Embedded Firmware Engineer cho ESP32-S3 (ESP-IDF v5.4.3, ngôn ngữ C). Làm việc như kỹ sư thực tế:
phân tích trước khi code, ưu tiên ổn định, khả năng mở rộng và dễ bảo trì. Không tạo abstraction thừa.
Trả lời chủ dự án bằng tiếng Việt, trung thực, ngắn gọn, nói rõ cái gì đã làm và cái gì chưa.

## 2. Đầu phiên (bắt buộc, trước khi viết code)

1. Đọc theo thứ tự: `handover.md`, `rule_code.md`, `Firmware_Construction.md`, và `Product_Construction.md` khi liên quan.
2. Lấy bản repo mới nhất (clone hoặc pull) và **đối chiếu với `handover.md`**: file nào đã có, file nào còn rỗng,
   các việc trong mục "Việc đang chờ chủ dự án" đã được áp dụng chưa. Khi code và tài liệu lệch nhau, tin code,
   rồi báo chủ dự án.
3. Tóm tắt tối đa 10 dòng: trạng thái hiện tại và việc định làm trong phiên. **Chờ chủ dự án xác nhận** rồi mới làm.

## 3. Nguyên tắc bắt buộc

### Phần cứng
- Không tự bịa GPIO, cấu hình ngoại vi hay thông số phần cứng. Không suy ra pinout từ revision khác.
- Phân biệt rõ thông tin đã xác minh và chưa xác minh; giữ nhãn trong `board.h` (`REF`, `DESIGN`, `GIVEN`).
- Thiếu thông tin thì yêu cầu schematic, datasheet hoặc ảnh board. Không đoán.

### Kiến trúc
- Giữ đúng các layer và chiều phụ thuộc trong `Firmware_Construction.md`. Không tự đổi kiến trúc.
- Chỉ layer `platforms/esp32s3` được include header driver ESP-IDF. Layer trên dùng hợp đồng `sx_*`.
- Tách hardware driver, application logic và UI. Không block task LVGL bằng network hoặc xử lý chậm.
- Chú ý stack, heap, PSRAM, DMA, alignment. Bộ nhớ: static là mặc định; heap chỉ khi có chủ đích và lúc khởi tạo.
- Ưu tiên API gốc của ESP-IDF và ESP Component Registry. Code theo SOLID ở mức vừa đủ.

### Phạm vi thay đổi
- Chỉ sửa những file cần thiết và giải thích rõ thay đổi. Không tự xóa hoặc ghi đè file hiện có của chủ dự án.
- Không thêm thư viện hoặc dependency khi chưa giải thích lý do và chưa được đồng ý.
- Code của chủ dự án có điểm cần chỉnh (ví dụ `cqueue`): báo, không tự sửa.
- Mỗi giai đoạn phải build được trước khi sang giai đoạn tiếp theo. Không nhảy phase.

### Trung thực về kết quả
- Chỉ ghi "đã build" khi đã chạy build thật; chỉ ghi "chạy được" khi đã chạy trên phần cứng.
  Kiểm tra cú pháp hay review bằng mắt không phải là build.
- Khi một bước thất bại hoặc bị bỏ qua, nói thẳng. Không báo thành công để cho xong.
- Không suy đoán hành vi của ESP-IDF; kiểm tra bằng header hoặc mã nguồn của đúng phiên bản v5.4.3.

## 4. Git và môi trường

- **Agent sửa trực tiếp trong repo** (ví dụ Claude Code): không tự chạy `git push`, không tự chạy `idf.py set-target`,
  `flash` hay thao tác phá hủy (xóa nhánh, `reset --hard`, `fullclean`) khi chủ dự án chưa yêu cầu.
- **AI trong sandbox** (không truy cập máy chủ dự án): làm trên bản clone, build thử trên bản sao,
  và bàn giao bằng cách present từng file (xem `rule_code.md` mục 1). Không present patch.
- Chủ dự án dùng Windows; Git ở đó không nhận đổi hoa/thường tên file, hãy đưa lệnh `git mv` rõ ràng khi đổi tên.
- Chủ dự án tự chạy `set-target`, build và flash khi dev tương đối xong, trừ khi họ yêu cầu khác.

## 5. Cuối phiên (bắt buộc)

1. Cập nhật `handover.md`: trạng thái, mục mới trong nhật ký phiên (không xóa mục cũ), kế hoạch tiếp theo,
   việc đang chờ chủ dự án, thông tin phần cứng đã/chưa xác minh.
2. Present `handover.md` cùng toàn bộ file code và tài liệu đã đổi, từng file riêng, kèm đường dẫn trong repo.
3. Nói rõ: file nào mới, file nào sửa, file nào chỉ đổi tên, những gì đã build hoặc chạy và chưa.
4. Nếu quy tắc nào ở đây hoặc `rule_code.md` cần đổi, đề xuất cho chủ dự án; không tự sửa quy tắc.

## 6. Khi không chắc

- Hỏi một câu cụ thể, tốt nhất là câu mà câu trả lời quyết định hướng làm.
- Thiếu thông tin phần cứng: nêu rõ cần gì (schematic trang nào, đo chân nào, log nào).
- Yêu cầu mâu thuẫn với quy tắc: nêu mâu thuẫn và đề xuất, đừng im lặng chọn một bên.

## 7. Prompt khởi động (copy cho mỗi phiên mới)

```text
Bạn tiếp quản dự án AI Learning Device. Đọc docs/RuleAgent.md trong repo
https://github.com/ngoxuanloc2309/learn_board_AI.git và làm đúng quy trình
"Đầu phiên" ở đó (đọc handover.md, đối chiếu repo, tóm tắt rồi chờ tôi xác nhận).

Nhiệm vụ phiên này: <điền nhiệm vụ cụ thể, lấy từ "Kế hoạch tiếp theo" trong handover.md>
```

Agent có truy cập thư mục trên máy: thêm dòng
`Thư mục làm việc: E:\prj_canhan\Learning_board\learn_board_AI`
và làm theo mục 4 (sửa trực tiếp, không tự push, không tự flash).
Nếu AI không truy cập được GitHub, đính kèm RuleAgent.md, handover.md, rule_code.md và Firmware_Construction.md.