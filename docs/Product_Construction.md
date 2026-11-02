# Product Construction --- AI Learning Device

> **Tài liệu định hướng sản phẩm và kiến trúc kỹ thuật**
>
> Phiên bản: 0.1\
> Trạng thái: Product concept / Prototype planning\
> Nền tảng phần cứng mục tiêu: Panlee / Wireless-Tag
> ZX3D50CE08S-V15-USRC (mã revision người dùng cung cấp: `230208`)\
> Framework firmware: ESP-IDF\
> Ngôn ngữ sản phẩm giai đoạn đầu: Tiếng Anh\
> Đối tượng: Học sinh nhiều độ tuổi, có phân cấp trình độ\
> Trải nghiệm: Hybrid --- học qua giao diện cảm ứng, trò chơi, âm thanh
> và hội thoại AI

------------------------------------------------------------------------

## 1. Tổng quan sản phẩm

### 1.1. Tên sản phẩm tạm thời

**AI Learning Companion**

Tên này mô tả đúng mục tiêu: một thiết bị học tập tương tác có AI hỗ
trợ, không chỉ là màn hình chạy chatbot.

Tên thương mại có thể thay đổi sau khi xác định nhóm khách hàng, nhận
diện thương hiệu và phạm vi sản phẩm.

### 1.2. Product vision

Xây dựng một thiết bị học tiếng Anh nhỏ gọn, độc lập, có màn hình cảm
ứng và khả năng tương tác bằng giọng nói. Thiết bị đóng vai trò người
bạn đồng hành học tập: hướng dẫn bài học, đặt câu hỏi, lắng nghe câu trả
lời, giải thích lỗi và theo dõi tiến độ.

Thiết bị sử dụng ESP32-S3 để xử lý giao diện, cảm ứng, âm thanh, kết nối
mạng và điều khiển ngoại vi. Các tác vụ AI nặng như nhận dạng tiếng nói,
mô hình ngôn ngữ và tổng hợp giọng nói được thực hiện trên AI Backend.

### 1.3. Giá trị cốt lõi

-   **Accessible:** thao tác đơn giản, phù hợp học sinh ở nhiều độ tuổi.
-   **Interactive:** kết hợp touch, hình ảnh, âm thanh và hội thoại.
-   **Adaptive:** nội dung và độ khó được điều chỉnh theo trình độ.
-   **Measurable:** lưu kết quả và theo dõi tiến bộ.
-   **Safe by design:** nội dung phù hợp lứa tuổi, có giới hạn và kiểm
    soát khi dùng AI.

### 1.4. Phạm vi phiên bản đầu

Sản phẩm tập trung vào tiếng Anh. Không mở rộng ngay sang Toán, Khoa học
hoặc nhiều ngoại ngữ trước khi xác thực trải nghiệm học tiếng Anh.

------------------------------------------------------------------------

## 2. Phần cứng nền tảng

### 2.1. Board chính

**Tên/mã board người dùng cung cấp:** `ZX3D50CE08S-V15-USRC 230208`\
**Module MCU:** WT32-S3-WROVER class, ESP32-S3.

Board thuộc nhóm phần cứng Panlee / Wireless-Tag WT32-SC01 Plus. Tài
liệu công khai của dòng WT32-SC01 Plus mô tả các ngoại vi chính gồm LCD
cảm ứng, microSD, speaker amplifier, RS485 và GPIO mở rộng.

**Lưu ý về revision:** ảnh PCB từng được cung cấp trong trao đổi hiển
thị mã `ZX3D50CE02S-V13-USRC 220511`, khác với mã V15/230208 người dùng
xác nhận đang sở hữu. Vì vậy, pin mapping dưới đây là mapping tham khảo
của nền tảng WT32-SC01 Plus/ZX3D50CE08S. Trước khi khóa cấu hình
firmware cho V15, cần đối chiếu schematic/datasheet đúng revision hoặc
đo kiểm thực tế. Không mặc định mọi revision có cùng BOM hoặc pinout.

### 2.2. Các khối phần cứng

  -----------------------------------------------------------------------
  Khối                    Phần cứng / IC được     Vai trò
                          công bố cho dòng board  
  ----------------------- ----------------------- -----------------------
  MCU                     ESP32-S3 trên           Chạy firmware, UI,
                          WT32-S3-WROVER          network, audio

  LCD                     3.5 inch, 480 × 320,    Hiển thị giao diện
                          ST7796 family           

  Touch                   FT6336U / FT5x06 family Nhận thao tác chạm

  RAM mở rộng             PSRAM trên module; dung Buffer đồ họa/audio
                          lượng cần xác nhận đúng 
                          module                  

  Flash                   Flash trên module; dung Firmware và dữ liệu cấu
                          lượng cần xác nhận      hình

  microSD                 Khe thẻ microSD         Tài nguyên offline,
                                                  log, cache

  Audio output            I2S audio amplifier     Khuếch đại tín hiệu ra
                          tích hợp, công bố       speaker
                          khoảng 2.5 W @ 4 Ω trên 
                          tài liệu dòng board     

  RS485                   Transceiver RS485 tích  Giao tiếp thiết bị công
                          hợp                     nghiệp/ngoại vi

  USB                     USB-C / USB native      Nạp firmware, console,
                          ESP32-S3 tùy thiết kế   cấp nguồn
                          board                   

  GPIO mở rộng            Header mở rộng          Nút nhấn, cảm biến hoặc
                                                  phụ kiện
  -----------------------------------------------------------------------

### 2.3. Giao thức sử dụng

  -----------------------------------------------------------------------
  Ngoại vi                Giao thức               Mục đích
  ----------------------- ----------------------- -----------------------
  LCD ST7796              **8080 Parallel 8-bit   Truyền command và pixel
                          (I80)**                 data

  Touch controller        **I2C**                 Đọc tọa độ và trạng
                                                  thái cảm ứng

  microSD                 **SPI**                 Đọc/ghi file và tài
                                                  nguyên

  Speaker amplifier       **I2S TX**              Truyền PCM audio tới
                                                  amplifier

  RS485 transceiver       **UART + RS485          Truyền nhận Modbus RTU
                          half-duplex**           hoặc protocol riêng

  AI Backend              **Wi-Fi +               API, streaming
                          HTTPS/WebSocket**       audio/text, đồng bộ dữ
                                                  liệu

  Firmware update         HTTPS/OTA               Cập nhật firmware từ
                                                  server
  -----------------------------------------------------------------------

### 2.4. Pin mapping tham khảo của nền tảng WT32-SC01 Plus

Các GPIO dưới đây được tìm thấy trong tài liệu và BSP công khai của dòng
board. Phải xác minh với revision V15/230208 trước khi dùng trong
firmware sản phẩm.

#### LCD --- I80 8-bit

  Tín hiệu          GPIO tham khảo
  --------------- ----------------
  LCD_DB0                        9
  LCD_DB1                       46
  LCD_DB2                        3
  LCD_DB3                        8
  LCD_DB4                       18
  LCD_DB5                       17
  LCD_DB6                       16
  LCD_DB7                       15
  LCD_WR                        47
  LCD_RS / DC                    0
  LCD_RESET                      4
  LCD_TE                        48
  Backlight PWM                 45

LCD truyền dữ liệu pixel qua bus I80 8-bit, không phải I2C hoặc SPI.
Touch dùng I2C riêng.

#### Touch --- I2C

  Tín hiệu     GPIO tham khảo
  ---------- ----------------
  TP_SDA                    6
  TP_SCL                    5
  TP_INT                    7
  TP_RESET                  4

GPIO4 có thể được dùng chung cho reset LCD/touch theo thiết kế tham
khảo. Cần xác minh trình tự reset của đúng revision.

#### microSD --- SPI

  Tín hiệu         GPIO tham khảo
  -------------- ----------------
  SD_CS                        41
  SD_MOSI / DI                 40
  SD_SCLK                      39
  SD_MISO / DO                 38

#### Audio --- I2S TX

  Tín hiệu          GPIO tham khảo
  --------------- ----------------
  I2S_LRCK / WS                 35
  I2S_BCLK                      36
  I2S_DOUT                      37

Board có đầu nối speaker SPK+ / SPK-. Đây là ngõ ra khuếch đại công
suất; không nối trực tiếp chân speaker với GPIO hoặc chân I2S. Cần dùng
speaker có trở kháng/công suất phù hợp thông số amplifier.

**Microphone input chưa được xác nhận là có sẵn.** Để làm hội thoại hai
chiều, cần kiểm tra board có microphone và ADC/codec thu âm hay không.
Nếu không có, bổ sung microphone I2S hoặc codec phù hợp.

#### RS485 --- UART

Tài liệu công khai của nền tảng có mô tả cổng RS485 4 chân: A, B, GND,
+5V. Một số pin mapping BSP/community ghi UART TX/RX/RTS, nhưng cần xác
minh theo đúng V15. Không được mặc định thứ tự chân connector hoặc GPIO
điều khiển DE/RE nếu chưa kiểm tra.

### 2.5. Năng lực và giới hạn phần cứng

ESP32-S3 phù hợp với vai trò **edge controller**: - Render UI LVGL. -
Điều khiển LCD, touch, SD card. - Phát audio I2S và thu audio nếu có
microphone. - Kết nối Wi-Fi, HTTPS, WebSocket. - Quản lý trạng thái học
tập và cache dữ liệu.

Không thiết kế để chạy LLM đa dụng trực tiếp trên MCU. Speech-to-Text,
LLM inference và Text-to-Speech nên chạy trên backend/cloud ở MVP.

Cần xác nhận chính xác: - Mã module ESP32-S3-WROVER đầy đủ. - Dung lượng
Flash và PSRAM. - LCD controller/revision. - Audio amplifier và pin
mapping. - Có hay không microphone tích hợp. - Pinout RS485 và GPIO mở
rộng.

------------------------------------------------------------------------

## 3. Product concept và trải nghiệm học tập

### 3.1. Đối tượng sử dụng

Sản phẩm hướng tới học sinh nhiều độ tuổi: - Tiểu học: nội dung ngắn,
hình ảnh trực quan, từ vựng cơ bản, trò chơi. - THCS: từ vựng theo chủ
đề, ngữ pháp nền tảng, hội thoại tình huống. - THPT: luyện nghe/nói, ngữ
pháp nâng cao, phản hồi và luyện tập theo mục tiêu.

Không dùng một bộ nội dung và một mức độ khó cho tất cả. Khi onboarding,
người học chọn nhóm tuổi/lớp hoặc làm bài đánh giá trình độ ngắn. Hệ
thống sử dụng thông tin đó để cá nhân hóa nội dung.

### 3.2. Ba chế độ trải nghiệm kết hợp

1.  **Learn --- Học có hướng dẫn:** bài học được cấu trúc theo chủ đề và
    cấp độ.
2.  **Play --- Học qua trò chơi:** quiz, ghép từ, nghe và chọn, thử
    thách theo thời gian.
3.  **Talk --- Hội thoại với AI:** luyện nói theo tình huống, hỏi đáp và
    nhận phản hồi.

AI Tutor là lớp tương tác xuyên suốt ba chế độ, không phải một mục
chatbot tách biệt duy nhất.

### 3.3. Các module nội dung

#### A. Vocabulary

-   Từ mới theo chủ đề: family, school, food, animals, travel, daily
    activities...
-   Flashcard gồm từ, nghĩa, hình minh họa, phiên âm và audio.
-   Ví dụ câu ngắn theo cấp độ.
-   Nút nghe phát âm và nút học sinh đọc lại (khi có microphone).
-   Ôn tập từ đã học theo lịch.

#### B. Grammar

-   Bài học ngắn theo chủ điểm.
-   Ví dụ đúng/sai.
-   Bài tập chọn đáp án hoặc sắp xếp từ.
-   AI giải thích vì sao câu trả lời đúng hoặc sai.
-   Gợi ý bài luyện tập tiếp theo.

#### C. Listening

-   Phát đoạn audio ngắn.
-   Câu hỏi kiểm tra nội dung.
-   Chọn đáp án, điền từ hoặc sắp xếp câu.
-   Nghe lại toàn bộ hoặc từng câu.
-   Điều chỉnh tốc độ phát nếu backend cung cấp audio phù hợp.

#### D. Speaking

-   Hội thoại theo tình huống: giới thiệu bản thân, hỏi đường, gọi món,
    ở trường...
-   AI đặt câu hỏi và chờ học sinh trả lời.
-   Speech-to-Text chuyển giọng nói thành văn bản.
-   AI đánh giá nội dung, ngữ pháp và mức độ phù hợp của câu trả lời.
-   Phản hồi ngắn, khuyến khích và có ví dụ sửa lỗi.

Không tuyên bố chấm chính xác accent/phoneme nếu hệ thống chưa có mô
hình đánh giá phát âm chuyên dụng. STT thông thường chỉ cho biết nội
dung nhận dạng, không đủ để kết luận phát âm chuẩn tuyệt đối.

#### E. AI Tutor

-   Giải thích từ vựng và ngữ pháp.
-   Đưa ví dụ theo trình độ.
-   Tạo câu hỏi luyện tập từ chủ đề hiện tại.
-   Gợi ý học lại kiến thức còn yếu.
-   Duy trì ngữ cảnh hội thoại trong phiên học.

#### F. Learning Progress

-   Thời gian học.
-   Bài đã hoàn thành.
-   Từ vựng đã học/đã nhớ/chưa vững.
-   Tỷ lệ đúng của quiz.
-   Chủ đề cần ôn tập.
-   Mục tiêu học tập tuần.

------------------------------------------------------------------------

## 4. MVP --- Phiên bản khả dụng đầu tiên

### 4.1. Mục tiêu MVP

MVP cần chứng minh ba giả thuyết: 1. Học sinh có thể tự thao tác và hoàn
thành một bài học ngắn trên thiết bị. 2. Audio và giao diện cảm ứng tạo
ra trải nghiệm tốt hơn một ứng dụng chỉ có văn bản. 3. AI có thể phản
hồi hữu ích và cá nhân hóa bài luyện tập ở mức cơ bản.

### 4.2. Phạm vi MVP đề xuất

  -----------------------------------------------------------------------
  Nhóm                    Tính năng MVP           Ưu tiên
  ----------------------- ----------------------- -----------------------
  Device                  Boot, Wi-Fi, cấu hình   Must
                          thiết bị                

  UI                      Home, menu, điều hướng  Must
                          touch                   

  Vocabulary              Bộ từ vựng mẫu,         Must
                          flashcard, audio        

  Quiz                    Trắc nghiệm 3--4 lựa    Must
                          chọn, chấm điểm         

  Audio                   Phát audio TTS/recorded Must
                          qua I2S                 

  Backend                 Đăng ký thiết           Must
                          bị/session, API nội     
                          dung                    

  AI Tutor                Hỏi đáp text theo chủ   Should
                          đề                      

  Speaking                Thu microphone và hội   Should, phụ thuộc phần
                          thoại voice             cứng mic

  Progress                Lưu kết quả và lịch sử  Should
                          học cơ bản              

  OTA                     Cập nhật firmware từ xa Later

  Parent dashboard        Báo cáo cho phụ huynh   Later

  Multi-subject           Toán/Khoa học           Out of MVP
  -----------------------------------------------------------------------

### 4.3. Kịch bản demo MVP

**Kịch bản: học chủ đề Animals** 1. Học sinh mở Vocabulary. 2. Thiết bị
hiển thị flashcard hình con vật. 3. Học sinh chạm nút nghe; speaker phát
từ tiếng Anh. 4. Học sinh xem nghĩa và ví dụ. 5. Học sinh làm quiz nhận
diện từ. 6. Thiết bị gửi kết quả lên backend. 7. Backend lưu tiến độ và
đề xuất ôn tập. 8. Học sinh mở AI Tutor để hỏi về từ vừa học.

Kịch bản này có thể hoàn thành trước khi tích hợp microphone và voice
conversation.

------------------------------------------------------------------------

## 5. UI/UX Concept

### 5.1. Nguyên tắc thiết kế

Màn hình 3.5 inch, 480 × 320 pixel có diện tích hiển thị hạn chế. UI
cần: - Ưu tiên nút lớn, dễ chạm. - Mỗi màn hình chỉ có một mục tiêu
chính. - Hạn chế đoạn văn dài. - Dùng icon và hình minh họa để giảm tải
chữ. - Có phản hồi rõ ràng sau mỗi thao tác. - Không dùng quá nhiều
animation làm ảnh hưởng hiệu năng. - Font dễ đọc, tương phản tốt. - Có
thể chuyển ngôn ngữ hướng dẫn sang tiếng Việt, nhưng nội dung học vẫn là
tiếng Anh.

### 5.2. Information Architecture

``` text
BOOT
 |
 +--> Device Setup
 |      +--> Wi-Fi
 |      +--> Language
 |      +--> Learner Profile
 |
 +--> HOME
        |
        +--> Learn
        |     +--> Vocabulary
        |     +--> Grammar
        |     +--> Listening
        |
        +--> Play
        |     +--> Quick Quiz
        |     +--> Word Match
        |     +--> Listening Challenge
        |
        +--> Talk
        |     +--> Choose Scenario
        |     +--> AI Conversation
        |
        +--> Progress
        |     +--> Daily Summary
        |     +--> Learning History
        |
        +--> Settings
              +--> Wi-Fi
              +--> Volume
              +--> Brightness
              +--> Profile
              +--> Device Info
```

### 5.3. Các màn hình chính

#### Screen 1 --- Home

Hiển thị: - Lời chào và learner profile. - Tiến độ hoặc mục tiêu hôm
nay. - Các entry point Learn, Play, Talk, Progress. - Trạng thái Wi-Fi
và âm lượng ở khu vực nhỏ.

#### Screen 2 --- Learn

Hiển thị các nhóm bài học theo chủ đề và cấp độ. Tránh danh sách quá
dài; dùng category card và phân trang/scroll.

#### Screen 3 --- Vocabulary Flashcard

Bố cục: - Hình minh họa ở vùng trung tâm. - Từ tiếng Anh nổi bật. -
Nghĩa tiếng Việt và phiên âm ở vùng phụ. - Nút nghe, ví dụ, Next. - Có
trạng thái đã biết/chưa chắc/chưa biết.

#### Screen 4 --- Quiz

-   Một câu hỏi tại một thời điểm.
-   3--4 đáp án dạng nút lớn.
-   Thanh tiến độ.
-   Phản hồi đúng/sai ngay sau khi chọn.
-   Cho phép xem giải thích ngắn.

#### Screen 5 --- Listening

-   Nút play/pause lớn.
-   Tiến trình audio.
-   Câu hỏi và lựa chọn.
-   Replay audio.
-   Không tự phát audio khi chưa có hành động hoặc chính sách phù hợp.

#### Screen 6 --- Talk

-   Chọn tình huống trước khi bắt đầu.
-   Hiển thị trạng thái Listening / Processing / Speaking.
-   Nút microphone rõ ràng.
-   Hiển thị transcript để học sinh biết thiết bị đã nhận nội dung gì.
-   Nút dừng/kết thúc phiên.
-   Cho phép phát lại câu trả lời của AI.

#### Screen 7 --- Progress

-   Số bài hoàn thành.
-   Tỷ lệ đúng gần đây.
-   Từ vựng cần ôn.
-   Mục tiêu ngày/tuần.
-   Tránh biểu đồ phức tạp trên màn hình nhỏ; ưu tiên số liệu và thanh
    tiến độ.

#### Screen 8 --- Settings

-   Wi-Fi setup.
-   Brightness.
-   Volume.
-   Learner profile.
-   Thông tin thiết bị.
-   Phiên bản firmware và trạng thái kết nối.

### 5.4. Visual direction

Định hướng hình ảnh: - Thân thiện, hiện đại, tích cực. - Màu nền sáng
hoặc trung tính, điểm nhấn màu tươi. - Bo góc card vừa phải. - Icon dễ
hiểu. - Minh họa phù hợp độ tuổi nhưng không quá trẻ con, vì sản phẩm
phục vụ cả THPT. - Có thể thay theme theo nhóm tuổi: Junior / Teen.

### 5.5. UX cho nhiều độ tuổi

  -----------------------------------------------------------------------
  Nhóm                                Điều chỉnh UI/nội dung
  ----------------------------------- -----------------------------------
  Tiểu học                            Ít chữ, hình lớn, audio rõ, thao
                                      tác từng bước, phần thưởng đơn giản

  THCS                                Chủ đề gần gũi, quiz, hội thoại
                                      ngắn, giải thích lỗi

  THPT                                UI gọn hơn, nội dung học thuật hơn,
                                      mục tiêu và tiến độ chi tiết
  -----------------------------------------------------------------------

Không cần tạo ba ứng dụng firmware khác nhau. Dùng cùng một UI
framework, profile và content configuration để thay đổi độ khó, cách
trình bày và nội dung.

------------------------------------------------------------------------

## 6. Kiến trúc hệ thống tổng thể

### 6.1. Nguyên tắc phân chia trách nhiệm

**ESP32-S3 là Edge Device**, chịu trách nhiệm: - UI, touch và display. -
Audio I/O. - Kết nối mạng. - Cache nội dung. - Quản lý session. - Lưu
cấu hình và dữ liệu cục bộ.

**AI Backend chịu trách nhiệm:** - Authentication/device registration. -
Learner profile. - Content management. - Speech-to-Text. - LLM
orchestration / AI Tutor. - Text-to-Speech. - Learning progress. -
Safety/filtering và logging.

Không chạy LLM lớn trực tiếp trên ESP32-S3.

### 6.2. Sơ đồ hệ thống

``` text
+------------------------------------------------------+
|                    AI LEARNING DEVICE                |
|                                                      |
|  +------------------- ESP32-S3 -------------------+  |
|  |                                                |  |
|  |  UI Layer: LVGL                                |  |
|  |       |                                        |  |
|  |  Application / Learning State Machine          |  |
|  |       |                                        |  |
|  |  +----+----------+-----------+-------------+  |  |
|  |  |               |           |             |  |  |
|  | Display/Touch   Audio      Network       Storage|  |
|  | I80 + I2C       I2S        Wi-Fi         SPI SD |  |
|  |  |               |           |             |  |  |
|  | LCD             Speaker     HTTPS/WS      Cache |  |
|  +-----------------+-----------+-------------+--+  |
+--------------------------------|-------------------+
                                 |
                            Wi-Fi / Internet
                                 |
+--------------------------------v-------------------+
|                    AI BACKEND                      |
|                                                    |
|  API Gateway / Device & User Authentication       |
|                  |                                 |
|  Learning Service / Content Service               |
|                  |                                 |
|  AI Orchestrator                                  |
|       +----------+-----------+                     |
|       |          |           |                     |
|      STT        LLM         TTS                     |
|       |          |           |                     |
|       +----------+-----------+                     |
|                  |                                 |
|       Database / Object Storage / Logs             |
+----------------------------------------------------+
```

### 6.3. Firmware architecture

Đề xuất chia theo component/module, không để logic UI, network và audio
phụ thuộc trực tiếp lẫn nhau.

``` text
main/
  app_main.c

components/
  board/
    board_init.c
    board_config.h

  display/
    display_manager.c
    lcd_i80.c
    backlight.c

  touch/
    touch_ft6336.c

  audio/
    audio_output_i2s.c
    audio_input.c
    audio_buffer.c
    audio_codec.c

  storage/
    sdcard_spi.c
    asset_manager.c
    nvs_config.c

  network/
    wifi_manager.c
    http_client.c
    websocket_client.c
    network_event.c

  ai_client/
    ai_session.c
    ai_protocol.c
    ai_stream.c

  learning/
    learner_profile.c
    lesson_manager.c
    quiz_engine.c
    progress_manager.c

  ui/
    ui_manager.c
    screens/
      home_screen.c
      learn_screen.c
      vocabulary_screen.c
      quiz_screen.c
      listening_screen.c
      talk_screen.c
      progress_screen.c
      settings_screen.c

  system/
    event_bus.c
    app_state.c
    diagnostics.c
    ota_manager.c
```

Có thể dùng ESP-IDF component manager để quản lý dependency. Không bắt
buộc phải giữ đúng cấu trúc thư mục trên; đây là cấu trúc tham khảo cho
prototype có khả năng mở rộng.

### 6.4. FreeRTOS task model

Các task logic đề xuất:

  -----------------------------------------------------------------------
  Task                    Trách nhiệm             Ghi chú
  ----------------------- ----------------------- -----------------------
  UI/LVGL                 Render UI và xử lý      Tuân thủ cơ chế lock
                          event                   của LVGL

  Touch/Input             Nhận dữ liệu touch      Có thể được gọi qua
                                                  LVGL input driver

  Network                 Wi-Fi, HTTPS, WebSocket Không block UI

  Audio TX                Phát PCM qua I2S        Dùng DMA và ring buffer

  Audio RX                Thu microphone nếu có   Chỉ triển khai sau khi
                                                  xác minh mic

  Storage                 Đọc file SD, cache      Tránh thao tác file dài
                                                  trong UI task

  Learning                Điều phối bài học/quiz  Dùng queue/event

  System                  OTA, diagnostics,       Ưu tiên theo nhu cầu
                          watchdog                
  -----------------------------------------------------------------------

Không cần tạo task cho mọi module ngay từ đầu. Bắt đầu tối giản, đo tải
CPU/RAM rồi mới tách task. Không mặc định pin cứng task vào core nếu
chưa có số liệu profiling.

------------------------------------------------------------------------

## 7. Kiến trúc giao tiếp và luồng dữ liệu

### 7.1. LCD --- 8080 Parallel 8-bit

-   LCD controller: ST7796 family theo tài liệu dòng board.
-   ESP-IDF: `esp_lcd` I80 panel IO.
-   Pixel format: RGB565.
-   Truyền dữ liệu bằng I80 bus và DMA.
-   LVGL render vùng cần cập nhật; driver chuyển buffer tới LCD.
-   Dùng partial buffer trước; cân nhắc full/double framebuffer sau khi
    đo PSRAM và hiệu năng.

Với 480 × 320 × 2 byte/pixel, một framebuffer RGB565 toàn màn hình cần
307,200 bytes (\~300 KiB). Hai framebuffer cần khoảng 600 KiB, chưa tính
buffer khác. Phải kiểm tra PSRAM thực tế và bộ nhớ DMA-capable internal
RAM.

### 7.2. Touch --- I2C

-   Đọc tọa độ từ FT6336U/FT5x06 family.
-   Có thể dùng interrupt GPIO nếu driver và board hỗ trợ.
-   Touch driver cung cấp tọa độ/trạng thái cho LVGL input device.
-   Kiểm tra địa chỉ I2C, reset timing và orientation trên board thực.

### 7.3. microSD --- SPI

Dùng cho: - Audio lesson đã tải trước. - Hình ảnh/asset. - Cache bài
học. - Log chẩn đoán. - Nội dung offline được cấp phép.

Không nên liên tục ghi log nhỏ đồng bộ trong UI task. Dùng buffer và
storage task.

### 7.4. Speaker --- I2S TX

-   ESP32-S3 phát PCM qua I2S.
-   Onboard amplifier nhận tín hiệu I2S theo thiết kế board.
-   Dùng DMA/ring buffer để phát liên tục.
-   Backend TTS có thể trả audio PCM hoặc codec nén; nếu codec nén,
    firmware phải giải mã trước khi đưa PCM vào I2S.
-   Chọn sample rate, bit depth và channel theo audio path đã xác minh.

Đầu SPK+/SPK- là đầu ra amplifier; không nối speaker trực tiếp vào GPIO.

### 7.5. Microphone --- Audio input

Voice conversation yêu cầu đường thu âm. Trước tiên xác minh board V15
có microphone tích hợp hay không.

Nếu không có: - Bổ sung microphone I2S phù hợp, hoặc - Dùng audio codec
có ADC và giao tiếp I2S.

Cần thiết kế gain, sampling rate, noise handling, VAD và cơ chế
push-to-talk. Không khẳng định có voice input chỉ dựa vào việc board có
I2S speaker output.

### 7.6. AI Backend --- Wi-Fi, HTTPS và WebSocket

Đề xuất: - HTTPS REST cho login, lấy lesson, submit quiz, progress và
cấu hình. - WebSocket cho hội thoại tương tác hoặc streaming hai
chiều. - Audio upload theo chunk khi voice input được triển khai. -
Audio response streaming hoặc tải file theo URL có xác thực. - TLS
certificate validation; không bỏ qua xác thực chứng chỉ trong bản
production.

MVP text-only có thể dùng HTTPS request/response trước. Chỉ thêm
WebSocket streaming khi trải nghiệm cần phản hồi liên tục.

### 7.7. RS485 --- UART

RS485 không phải thành phần bắt buộc của AI Learning MVP. Giữ lại như
giao diện mở rộng cho tương lai, ví dụ kết nối thiết bị ngoại vi trong
lớp học hoặc hệ thống điều khiển. Không đưa Modbus vào phạm vi MVP nếu
chưa có use case cụ thể.

------------------------------------------------------------------------

## 8. AI Backend và dữ liệu

### 8.1. Các service logic

  Service                Trách nhiệm
  ---------------------- -------------------------------------------------
  Device Service         Đăng ký thiết bị, token, trạng thái
  User/Learner Service   Hồ sơ người học và cấp độ
  Content Service        Lesson, vocabulary, quiz, audio
  AI Orchestrator        Điều phối prompt, context và model
  STT Service            Audio thành text
  TTS Service            Text thành audio
  Progress Service       Kết quả, lịch sử, mục tiêu
  Safety Layer           Kiểm soát nội dung, giới hạn chủ đề và phản hồi

Có thể triển khai ban đầu dưới dạng một backend application modular
monolith thay vì nhiều microservice. Tách service khi tải và nhu cầu vận
hành thực tế yêu cầu.

### 8.2. Data model khái niệm

Các entity chính: - `Device`: device_id, firmware_version, status,
last_seen. - `Learner`: learner_id, age_group, proficiency_level,
preferences. - `Lesson`: lesson_id, topic, level, content_type. -
`VocabularyItem`: word, meaning, phonetic, example, audio_asset. -
`Quiz`: quiz_id, lesson_id, questions, answer_key. - `LearningSession`:
session_id, learner_id, started_at, ended_at. - `AnswerRecord`:
question_id, answer, correctness, timestamp. - `Progress`: learner_id,
skill, score, mastery_state. - `AIConversation`: conversation_id,
learner_id, messages, safety_metadata.

Không lưu dữ liệu cá nhân nhạy cảm của trẻ em nếu không cần thiết. Thiết
kế consent, retention, quyền xóa dữ liệu và kiểm soát phụ huynh trước
khi phát hành thực tế.

### 8.3. AI Tutor behavior

AI Tutor cần nhận context có cấu trúc: - Nhóm tuổi/cấp độ đã được cấu
hình. - Chủ đề/bài học hiện tại. - Từ vựng đã học. - Lỗi gần đây. - Mục
tiêu phiên học.

AI phải: - Dùng ngôn ngữ phù hợp độ tuổi. - Giải thích ngắn, rõ. - Không
bịa đáp án hoặc quy tắc. - Khuyến khích học sinh tự suy nghĩ. - Không
tạo nội dung không phù hợp. - Chuyển về nội dung học an toàn khi người
dùng hỏi ngoài phạm vi.

------------------------------------------------------------------------

## 9. Quy trình phát triển

### Phase 0 --- Hardware validation

1.  Xác nhận chính xác revision V15/230208.
2.  Xác nhận module ESP32-S3 đầy đủ và dung lượng Flash/PSRAM.
3.  Xác nhận LCD controller, resolution, bus và GPIO.
4.  Xác nhận touch controller và I2C.
5.  Xác nhận microSD SPI.
6.  Xác nhận amplifier, I2S pinout và speaker specification.
7.  Kiểm tra microphone tích hợp hoặc quyết định phần cứng bổ sung.
8.  Xác nhận RS485 pinout và transceiver.

**Deliverable:** Hardware validation report và `board_config.h` được
kiểm chứng.

### Phase 1 --- Board bring-up

1.  Tạo project ESP-IDF.
2.  Build/flash qua USB.
3.  Kiểm tra log và reset reason.
4.  Khởi tạo LCD I80 và hiển thị màu/test pattern.
5.  Khởi tạo touch và kiểm tra tọa độ.
6.  Khởi tạo backlight.
7.  Khởi tạo SD card SPI.
8.  Phát audio test qua I2S.

**Deliverable:** Board test application ổn định.

### Phase 2 --- UI MVP

1.  Tích hợp LVGL.
2.  Xây dựng Home, Learn, Vocabulary, Quiz, Settings.
3.  Tích hợp asset từ SD hoặc flash.
4.  Kiểm tra memory, frame rate, touch latency.
5.  Tối ưu partial rendering và DMA.

**Deliverable:** Offline English learning demo.

### Phase 3 --- Backend integration

1.  Wi-Fi provisioning.
2.  Device registration.
3.  API lấy lesson.
4.  Submit quiz result.
5.  Lưu learner progress.
6.  AI Tutor text interaction.
7.  TTS playback.

**Deliverable:** Online learning prototype.

### Phase 4 --- Voice interaction

1.  Xác nhận/bổ sung microphone.
2.  I2S RX hoặc audio codec input.
3.  Push-to-talk.
4.  Audio buffering và upload.
5.  STT.
6.  AI response.
7.  TTS streaming/playback.
8.  Timeout, retry và xử lý mất mạng.

**Deliverable:** Voice AI Tutor prototype.

### Phase 5 --- Product hardening

-   OTA có xác minh image và rollback strategy.
-   Secure device identity.
-   TLS và quản lý token.
-   Watchdog, crash log và diagnostics.
-   Power/thermal testing.
-   Kiểm thử mất Wi-Fi và backend unavailable.
-   Kiểm thử độ bền audio/display.
-   Privacy và child-safety review.

------------------------------------------------------------------------

## 10. Rủi ro và điểm cần xác minh

  -----------------------------------------------------------------------
  Rủi ro / câu hỏi        Tác động                Hành động
  ----------------------- ----------------------- -----------------------
  V15 pinout khác         Firmware có thể cấu     Xác minh tài liệu đúng
  V13/V1.x                hình sai GPIO           revision/đo thực tế

  PSRAM/Flash chưa xác    Ảnh hưởng framebuffer   Đọc module marking và
  nhận                    và cache                kiểm tra runtime

  Không có microphone     Không thể voice         Kiểm tra BOM; bổ sung
                          conversation hai chiều  mic nếu cần

  Audio format không phù  Không phát được hoặc âm Xác minh I2S format,
  hợp amplifier path      thanh sai               sample rate, channel

  UI và audio tranh chấp  Drop frame/drop audio   Dùng buffer có giới
  RAM                                             hạn, profiling

  Wi-Fi chập chờn         AI phản hồi chậm/mất    Timeout, retry, offline
                          phiên                   lesson

  AI trả lời không phù    Rủi ro sản phẩm giáo    Safety layer và kiểm
  hợp lứa tuổi            dục trẻ em              soát nội dung

  Màn hình nhỏ            Khó hiển thị nội dung   Thiết kế một nhiệm
                          dài                     vụ/màn hình
  -----------------------------------------------------------------------

------------------------------------------------------------------------

## 11. Tiêu chí nghiệm thu MVP

### Hardware/Firmware

-   [ ] Boot ổn định và có log chẩn đoán.
-   [ ] LCD hiển thị đúng orientation và màu.
-   [ ] Touch nhận đúng tọa độ.
-   [ ] SD card đọc được asset.
-   [ ] Speaker phát được audio test qua I2S.
-   [ ] Wi-Fi kết nối và tự phục hồi sau mất kết nối.
-   [ ] UI không bị block khi network request chạy.
-   [ ] Không tràn heap trong kịch bản học liên tục.

### Learning experience

-   [ ] Người học vào được bài Vocabulary.
-   [ ] Nghe được phát âm.
-   [ ] Hoàn thành được quiz.
-   [ ] Nhận phản hồi đúng/sai.
-   [ ] Kết quả được lưu.
-   [ ] AI Tutor trả lời theo chủ đề và cấp độ.

### Voice (nếu có microphone)

-   [ ] Thu âm rõ trong môi trường sử dụng mục tiêu.
-   [ ] Có trạng thái listening/processing/speaking.
-   [ ] Có transcript để kiểm tra STT.
-   [ ] Xử lý được timeout và mất mạng.
-   [ ] Không tự ghi âm nền khi chưa có consent/interaction phù hợp.

------------------------------------------------------------------------

## 12. Công nghệ đề xuất

  -----------------------------------------------------------------------
  Layer                               Công nghệ
  ----------------------------------- -----------------------------------
  Firmware                            ESP-IDF

  GUI                                 LVGL

  LCD                                 `esp_lcd` I80

  Touch                               I2C driver cho FT6336U/FT5x06

  SD card                             SPI + FATFS

  Audio output                        ESP-IDF I2S TX

  Audio input                         I2S RX hoặc codec, tùy microphone

  Concurrency                         FreeRTOS

  Network                             ESP Wi-Fi, esp_http_client,
                                      WebSocket client

  Local config                        NVS

  Backend API                         HTTPS REST

  Real-time session                   WebSocket

  AI                                  STT + LLM + TTS qua backend

  Database                            Relational DB cho learner/progress;
                                      object storage cho audio/assets

  OTA                                 ESP-IDF OTA
  -----------------------------------------------------------------------

### Tài liệu kỹ thuật tham khảo

1.  Wireless-Tag --- WT32-SC01 PLUS Datasheet:\
    https://img01.71360.com/w3/77q677/20241031/bf724a7a164740edb1896e09c5ea50d5.pdf

2.  ESP-IDF LCD Peripheral API:\
    https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/lcd/index.html

3.  ESP-IDF I2S API:\
    https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/i2s.html

4.  ESP-IDF SPI Master:\
    https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/spi_master.html

5.  ESP-IDF I2C:\
    https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/i2c.html

6.  Smart Panlee SC01 Plus ESP-IDF BSP:\
    https://components.espressif.com/components/espp/smartpanlee-sc01-plus

7.  WT32-SC01 Plus ESP-IDF/LVGL reference implementation:\
    https://github.com/xventus/LGVL_WT32_SC01_Plus

------------------------------------------------------------------------

## 13. Quyết định sản phẩm hiện tại

  Hạng mục             Quyết định
  -------------------- ------------------------------------------------------
  Product              AI Learning Companion
  Target               Học sinh nhiều độ tuổi
  Initial subject      English
  Experience           Hybrid: Learn + Play + Talk
  Main MCU             ESP32-S3 / WT32-S3-WROVER
  Display              3.5-inch 480 × 320, ST7796 family
  LCD bus              8080 Parallel 8-bit
  Touch                I2C
  SD card              SPI
  Speaker              I2S TX + onboard amplifier
  RS485                UART, để dành cho mở rộng
  Firmware             ESP-IDF + LVGL
  AI processing        Cloud/Backend
  MVP priority         Vocabulary + Quiz + Audio + Progress + Text AI Tutor
  Voice conversation   Giai đoạn tiếp theo sau khi xác minh microphone

**Kết luận:** Ưu tiên đầu tiên không phải tích hợp AI ngay, mà là xác
minh phần cứng đúng revision và hoàn thành board bring-up. Sau đó xây
dựng một trải nghiệm học Vocabulary/Quiz chạy offline, rồi mới kết nối
backend. Cách này giảm rủi ro, giúp phân biệt lỗi phần cứng, firmware và
AI trong quá trình phát triển.
