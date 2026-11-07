深圳市启明智显科技有限公司 

# **智显智能串口屏** 

## **ZX3D50CE08S-USRC-4832** 

**（SC01 PLUS）** 



<!-- Start of picture text -->
SUT. Ve nn<br>a aS rs Tem<br>wl a \ Q te WYN<br>. \) Ie ,S ee<br>2.Jervnsmn ie nu EN$ =WisIa<br>oe ATAY SHAW EAU<br>= N\\Y WW<br>SANIT. SWCD Ennieariees ve)<br><!-- End of picture text -->

ZX3D50CE08S-USRC-4832 规格书 深圳市启明智显科技有限公司 



### **变更日志：** 

|**日期**|**变更人**|**说明**|
|---|---|---|
|2022/11/16|哈迪斯|创建文档|
|2023/02/06|许灏轩|添加接口封装，排版修饰|



### **命名说明：** 

|**规格型号**|**说明**|**备注**|
|---|---|---|
|ZX3D50CE08S-USRC-P58|整箱58pcs，包含塑胶独立包装||
|ZX3D50CE08S-USRC-4832|独立开发板，不含包装||



- 1 - 

ZX3D50CE08S-USRC-4832 规格书 深圳市启明智显科技有限公司 



<!-- Start of picture text -->
Q p " SMART<br><!-- End of picture text -->

### **产品特点：** 

1. 支持快速原型开发 

2. WT32-S3-WROVER-N4R2、N8R2 或N16R2 

### **Features：** 

1. Support rapid prototyping. 

2. WT32-S3-WROVER-N4R2, N8R2, or N16R2 

### **命名规则Naming Conventions：** 



<!-- Start of picture text -->
Pm Bir ZX 3D50 C £08 S - USRC 4832<br>ARTES<br>ERR TSS<br>SRORE SE BREE<br>ea aco<br>SREMESE, BTRSTEH<br>ENS, SER ROSS) x<br>N | eee<br>c |seems -<br>R | SIBRsEe —<br>Gc | ae use<br>u_| eras 7 | Sania<br>CORED m | ER<br>s | ame<br>S01 | SS0202 1GBit SPINAND R | S45<br>s02 | ss0202 2G8it SPI NAND c |amore ra<br>s03_| ss0212 1GBit SPINAND | season<br>soa | ssp212 2G8it spi NAND<br>£08 | W132-S3-WROVER-NI6R2<br>= erin BREF<br>> samFRAEPS BOREDSQaeEETESSRASTER<br>na | BER RR)<br>3 | mime<br>c | seme<br>ee | stem<br>eam<br><!-- End of picture text -->

Fig.0 Naming Conventions (图0 命名规则) 

### **核心物料列表(Tab.0)：** 

|**序号**|**名称**|**型号**|**备注**|
|---|---|---|---|
|**1**|ESP32-S3 模组|WT32-S3-WROVER-N16R2||
|**2**||||
|**3**||||



- 2 - 

ZX3D50CE08S-USRC-4832 规格书 深圳市启明智显科技有限公司 



<!-- Start of picture text -->
OPantee<br><!-- End of picture text -->

### **硬件及接口Hardware interface：** 

硬件接口图： 



<!-- Start of picture text -->
SE<br>Oranlanlee zxspsoceo2s-vi3usec  22<br>or 220511 =”=<br>c Bie<br>“Be &®— =Cayoofv2f gya"uae"485, oeie 1<br>re ps a. +<br>i oo o@ _cisws w_ pris = TV pe”<br>Ps : Pa Wwe SH) oe<br>CDH) C4 Ss Sa" Ao<br>eee32 winceaeSons se<br>nis s |<br>: : ez<br>B gee eBlog. }<br>a | RS<br>Lie RST Pe menetS SDE EL<br>+--__¥ii 19 DEBUG iS aad eens<br>SO@Anassss— Poasesss— “nash 5<br>rn a ey<br><!-- End of picture text -->



<!-- Start of picture text -->
——[S===___A<br>fe) zxsps0ceo2s-v13 22 Py 5<br>Panlee -usR 220511 2"<br>ee<br>ne me COS ETS =<br>= 2SS =of A 27435,<br>=i ks Bia +a<br>= Na Sew) he<br>= Ff C¥@ cides w_pRis oo<br>=a) oe Sey<br>= (ai ot<br>= as 2 Ala) fi<br>= : ef<br>=Sh52 |= yeat eS al L<br>mob ut pice ea<br>rater) ‘ST Vee CiSe8 oto Bt<br>fe EI DEBUG May 89 PORES<br>CURR AARAAB. Es ieJa a8 8888. aeJ4in asl<br>i = 2<br><!-- End of picture text -->

Fig.1 Hardware Interface (图1 硬件接口图) 

- 3 - 

ZX3D50CE08S-USRC-4832 规格书 深圳市启明智显科技有限公司 



### **接口说明Interface description：** 

#### [1]调试接口debug interface (Tab.1) 

|**Pin**|**描述**|**模组pin**|**电压范围**|**备注**|
|---|---|---|---|---|
|1|+5V|-|5V||
|2|+3.3V|-|3.3V|参考用，不用于功率输入|
|3|ESP_TXD|TXD0|3.3V TTL||
|4|ESP_RXD|RXD0|3.3V TTL||
|5|EN|EN|0-3.3V|芯片使能|
|6|BOOT|GPIO 0|0-3.3V||
|7|GND|GND|0V|接地|



#### [2] 扩展IO 接口Export IO Interface Description (Tab.2) 

|**Pin**|**描述**|**模组pin**|**电压范围**|**备注**|
|---|---|---|---|---|
|1|+5V|-|5V±5%|供电或输出供电|
|2|GND|-|0V|接地|
|3|EXT_IO1|GPIO 10|0-3.3V||
|4|EXT_IO2|GPIO 11|0-3.3V||
|5|EXT_IO3|GPIO 12|0-3.3V||
|6|EXT_IO4|GPIO 13|0-3.3V|扩展IO|
|7|EXT_IO5|GPIO 14|0-3.3V||
|8|EXT_IO6|GPIO 21|0-3.3V||



#### [3] 扬声器接口Speaker connector (Tab.3) 

|**Pin**|**描述**|**备注**|
|---|---|---|
|1|SPK+|扬声器正|
|2|SPK-|扬声器负|



- 4 - 

ZX3D50CE08S-USRC-4832 规格书 深圳市启明智显科技有限公司 



<!-- Start of picture text -->
©)Panlee<br><!-- End of picture text -->

#### [4] SD 卡接口SD card interface (Tab.4) 

|**描述**|**模组pin**|**备注**|
|---|---|---|
|SD_CS|GPIO 41|SD 卡片选|
|SD_DI（MOSI）|GPIO 40|SD 卡数据输入|
|SD_CLK|GPIO 39|SD 卡时钟|
|SD_DO（MISO）|GPIO 38|SD 卡数据输出|



#### [5] LCD 接口LCD Interface Description (Tab.5) 

|**描述**<br>~~a~~<br>|**模组pin**<br>|**备注**<br>|
|---|---|---|
|BL_PWM<br>~~ee~~<br>|GPIO 45<br><br>|背光控制，高有效<br>~~ee~~<br>|
|LCD_RESET<br><br>~~ee~~|GPIO 4<br><br>~~e~~|LCD 复位，与触摸复位复用<br><br>|
|LCD_RS<br><br>~~a~~|GPIO 0<br> <br>|命令/数据选择<br><br>|
|LCD_WR<br><br>|GPIO 47<br><br>|写时钟<br>~~e~~|
|LCD_TE<br>~~ee~~<br>|GPIO 48<br><br>|帧同步<br>|
|LCD_DB0<br><br>**~~|~~**|GPIO 9<br><br>||
|LCD_DB1<br>~~|~~|GPIO 46<br>||
|LCD_DB2<br>**~~|~~**|GPIO 3<br>||
|LCD_DB3<br>**~~|~~**|GPIO 8<br>|<br>|
|LCD_DB4<br>**~~|~~**|GPIO 18<br>|LCD 数据接口，8bit MCU（8080）<br>|
|LCD_DB5<br>**~~|~~**|GPIO 17<br>||
|LCD_DB6<br>~~|~~<br>|GPIO 16<br>~~|~~<br>||
|LCD_DB7<br>~~|~~|GPIO 15<br>||
|TP_INT<br>~~ee~~|GPIO 7<br>~~e~~|触摸中断<br>|
|TP_SDA<br><br>~~a~~|GPIO 6<br> <br>|触摸IIC 数据<br><br>|
|TP_SCL<br><br>|GPIO 5<br><br>|触摸IIC 时钟<br>~~e~~|
|TP_RST<br>~~ee~~|GPIO 4<br>|触摸复位，与LCD 复位复用|



- 5 - 

ZX3D50CE08S-USRC-4832 规格书 深圳市启明智显科技有限公司 



<!-- Start of picture text -->
OPantee<br><!-- End of picture text -->

#### [6] RS485 接口RS485 interface (Tab.6) 

|**Pin**|**描述**|**备注**|
|---|---|---|
|1<br>2|RS485-A<br>RS485-B|RS485 总线|
|3|GND|接地|
|4|+5V|供电或输出供电|



### **接口封装Interface Footprint ：** 

|**接口描述**|**接口封**|**装**|**备注**|
|---|---|---|---|
|调试接口|Molex <br>(7Pin)<br>WAFER <br>贴|SD-53398-0771<br> MX1.25 7P 立|MX1.25|
|扩展IO 接口|Molex <br>(8Pin)<br>WAFER <br>贴|SD-53398-0871<br> MX1.25 8P 立|MX1.25|
|扬声器接口|Molex <br>(2Pin)<br>WAFER <br>贴|SD-53398-0271<br> MX1.25 2P 立|MX1.25|
|RS485 接口|Molex <br>(4Pin)<br>WAFER <br>贴|SD-53261-0471<br> MX1.25 4P 卧|MX1.25|



- 6 - 

ZX3D50CE08S-USRC-4832 规格书 深圳市启明智显科技有限公司 



### **硬件外设Hardware peripherals：** 

|**外设名称**|**描述**|
|---|---|
|音频功放|使用IIS的功放，2.5W @4R|
|LCD带触摸|3.5寸电容触摸全贴合屏幕，分辨率480*320，8080接口|
|SD卡|SPI接口SD卡|
|RS485|带流控IO的串口|



[1] 音频功放（IIS 接口）Audio amplifier (IIS interface) (Tab.7) 

|**描述**|**模组pin**|
|---|---|
|LRCK|GPIO 35|
|BCLK|GPIO 36|
|DOUT|GPIO 37|



#### [2] 差分通信RS485 (Tab.8) 

|**描述**|**模组pin**|
|---|---|
|RXD|GPIO 1|
|RTS|GPIO 2|
|TXD|GPIO 42|





- 7 - 

ZX3D50CE08S-USRC-4832 规格书 深圳市启明智显科技有限公司 



### **电气参数Electrical parameters (Tab.9)** 

|**参数**|**测试条件**|**最小值**|**典型值**|**最大值**|**单位**|
|---|---|---|---|---|---|
|工作电压||||||
|Operating|**-**|4.7|5.0|5.5|V|
|Voltage||||||
|工作电流<br>Operating<br>current|USB提供5V，背光亮<br>度最大|170|175|190|mA|



- 8 - 

深圳市启明智显科技有限公司 

### **原理图Schematic：** 



<!-- Start of picture text -->
earn ee .<br>j espsz—ss | | reir, | exec H<br>| TE abbas i did3|) ™ Ee" me Ts |<br>i ele TT BBD |<br><!-- End of picture text -->

深圳市启明智显科技有限公司 

### **规格参数Specification parameters：** 

#### [1] LCD 参数LCD parameters (Tab.10) 

|**显示屏类**<br>**Display type**|LCD|
|---|---|
|**驱动IC 型号**<br>**Drive IC model**|ST7796UI|
|**视角**<br>**Visual angle**|FULL|
|**分辨率**<br>**Resolution**|480*320|
|**接口**<br>**Interface**|MCU8080 8Bit|
|**色彩**<br>**Color**|RGB565|
|**背光模式**<br>**Backlit mode**||
|**背光亮度**<br>**Backlight brightness**||



#### [2] 触摸参数Touch parameters (Tab.11) 

|**触摸屏类型**<br>**Touchscreen type**|电容式触摸|
|---|---|
|**驱动IC 型号**<br>**Drive IC model**|FT6336U|
|**接口**<br>**Interface**|I2C|
|**触摸屏结构**<br>**Touchscreen structure**|G+F|
|**触摸方式**<br>**Touch mode**|单点触摸|
|**表面硬度**<br>**Surface hardness**|＞6H|
|**透光率**||
|**Light transmittance**||



ZX3D50CE08S-USRC-4832 规格书 深圳市启明智显科技有限公司 



<!-- Start of picture text -->
Q PanleeSMART<br><!-- End of picture text -->

### **外观结构图(Fig.2)** 



<!-- Start of picture text -->
° ° : :<br>| |e ij<br>ral<br>z<br>co<br>[= 60<br>° | | 7z<br>" 5 of<br>~ i — s|<br>i | | |<br>oyhb -. J><br>‘|<br>w rt “<br>| cy fo =i. —_—<br>i> |<br>o et lL ssl | o<br>— ko<br>_————)" ©<br>8 ee - io<br>i SI<br>® Fa a BL |<br>on = |[Pe a Ol x<br>4 Qt tompf] I lite. 22<br>: Bo} imal<br>lg! (el ey rr 51.39 | fay:<br>[=| n ml7| 2 lS i}<br>¥ | @ Sls(81sislo| |Z > I<br>3| B 81318 & Ie Ey co<br>: ‘ ePlefe lela SS<br>a :<br><!-- End of picture text -->

- 1 - 

ZX3D50CE08S-USRC-4832 规格书 深圳市启明智显科技有限公司 



<!-- Start of picture text -->
(©)Pp L =t=}<br><!-- End of picture text -->

### **固件烧录Firmware burning：** 

1. 通过USB-Type C 连接下载器（ZXACC-ESPDB），用MX1.25-7P 数据线把ZX3D50CE08S 板卡与下载器（ZXACC-ESPDB）连接起来。由于下载器（ZXACC-ESPDB）做有数据流 控自动处理，所以接下来固件通过ESP32 Flash Download Tools 即可自动下载。 



<!-- Start of picture text -->
~ C)<br><!-- End of picture text -->

图3（Fig.3） 

2. 如右图4（Fig.4）所示：在1 处选择要烧录的固件路径， 地址通常为0X00，设置好后记得在前面打上勾勾；在2 处选择系统时钟为40MHz；在3 处选择Flash 的大小 为32Mbit；在4 处选择SPI MODE 为DIO 模式；在5 处选择当前板卡在电脑所识别的端口号；在6 处选择串 口波特率（值越大下载固件的速率越快，最大支持 1152000bps）； 

3. 完成前面的配置后，在7 处单击便可开始烧录固件。 

4. 完成上面两个步骤后，按下开发板后面的复位按钮即可开 始运行刚才烧录的固件。 



<!-- Start of picture text -->
{| sroowoed | ToMDowioad | Weonkg [GnOCony MA<br>[=) m= @ |0x1000<br>OD me [000<br>rs) «| oxioon<br>iz) -~@<br>r)OL j-¢°<br>OoSpiranhCong lle “<br>ystelfreg ‘Combinesin | "ASH SIZE CSpiautoser<br>lfm =]2 Telok | OOM COON<br>TSE SBIMODE Ove<br>Sm Cao sTECTED INFO<br>Orsmmis flour OssMbit<br>Ozone Oreste<br>Csoue —Ov0uT<br>Domiond Pee!<br>DLE<br>ef 5<br>stor ease<br># 6<br><!-- End of picture text -->

图4（Fig.4） 

- 2 - 

ZX3D50CE08S-USRC-4832 规格书 深圳市启明智显科技有限公司 



### **软件参考Software reference：** 

URL：https://www.espressif.com.cn/en/support/documents/technical-documents 

SDK： 

1. https://github.com/smartpanle/QMSD-ESP32-BSP 

2. <u>https://gitee.com/qiming-zhixian/qmsd-esp32-bsp/tree/master</u> 

在线平台使用手册： <u>https://doc.panel-tag.cn/</u> LVGL 官网链接：https://lvgl.io/ LVGL 推介使用版本为v8.3.1。推荐UI 开发工具为NXP- GUI Guider GUI Guider 下 载 链 接 ： <u>https://www.nxp.com/design/design-center/software/development-software/gui-guider:GU</u> 

<u>I-GUIDER</u> 

- 3 - 

ZX3D50CE08S-USRC-4832 规格书 深圳市启明智显科技有限公司 



<!-- Start of picture text -->
OPantée<br><!-- End of picture text -->

### **联系方式** 

网址：http://www. panel-tag.cn/ 联系邮箱：panlee@smartpanle.com 样品购买地址：https://shop212317088.taobao.com/ 

微信公众号：启明智显 



<!-- Start of picture text -->
Ro rr<br><!-- End of picture text -->

- 4 - 

