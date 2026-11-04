# Quy tắc làm việc và viết code

Áp dụng cho mọi code và file được tạo hoặc sửa trong dự án này. Bổ sung cho
`Firmware_Construction.md` (kiến trúc) và các Engineering rules trong handover.

## 1. Bàn giao file

- Khi viết hoặc sửa code xong, **present từng file code** (từng file riêng lẻ, đúng nội dung cuối cùng).
- **Không present file patch** (`.patch`, `.diff`).
- File có cùng tên ở nhiều thư mục (ví dụ `CMakeLists.txt`) phải được present kèm đường dẫn
  tương đối trong repo, để không bị nhầm lẫn.
- Mỗi lần bàn giao ghi rõ: file nào mới, file nào sửa, file nào chỉ đổi tên.

## 2. Comment trong code

- Chỉ viết comment bằng **tiếng Anh**. Không dùng tiếng Việt trong comment.
- Comment chỉ dùng ký tự ASCII thông thường: chữ, số, khoảng trắng và dấu câu cơ bản
  (`. , : ; - _ ( ) / =`).
- **Không dùng ký tự đặc biệt** trong comment, gồm:
  - ký tự ngoài ASCII (mũi tên, dấu gạch dài, `§`, ký hiệu toán học, emoji, chữ có dấu);
  - ký tự trang trí hoặc đường kẻ phân cách (ví dụ dòng toàn `=====` hoặc `-----`, khung ký tự, mũi tên `->` `=>`);
  - tag trong ngoặc vuông kiểu `[REF]`; thay bằng chữ thường, ví dụ `REF:`.
- Ngoại lệ: thẻ Doxygen (`@brief`, `@param`, `@return`) được giữ nguyên.
- Quy tắc áp dụng cho cả comment trong `CMakeLists.txt`, header và source.
- Tên chuỗi log và message hiển thị cho người dùng thuộc sản phẩm (ví dụ nội dung UI) không phải comment
  và theo quy định riêng của sản phẩm.

## 3. Handover giữa các phiên

Dự án được làm qua nhiều phiên bởi nhiều Claude và AI khác nhau, nên:

- **Đầu phiên**: làm theo `docs/RuleAgent.md`; đọc `docs/handover.md`, `docs/rule_code.md`, `docs/Firmware_Construction.md` trước khi làm gì.
- **Cuối phiên**: cập nhật `docs/handover.md` và present file này cùng các file code đã đổi. Gồm:
  - trạng thái hiện tại của project;
  - tóm tắt việc đã làm trong phiên (thêm một mục mới vào nhật ký, không xóa mục cũ);
  - kế hoạch tiếp theo;
  - việc đang chờ chủ dự án, câu hỏi còn mở, thông tin phần cứng đã/chưa xác minh.
- Chỉ ghi điều đã thực hiện và kiểm tra thật. Ghi rõ điều gì đã build, điều gì đã chạy trên phần cứng, điều gì chưa.
- Không ghi suy đoán phần cứng như sự thật; dùng nhãn `chưa xác minh`.

## 4. Ghi chú

- File tài liệu trong `docs/` có thể viết bằng tiếng Việt; quy tắc ở mục 2 chỉ áp dụng cho code.
- Code có sẵn trước khi có quy tắc này sẽ không tự động bị sửa; chỉ sửa khi chủ dự án yêu cầu.