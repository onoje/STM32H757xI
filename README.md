# STM32H757XI JPEG Decode & Ağ Üzerinden Görüntü Aktarım Sistemi

STM32H757XI mikrodenetleyicisinin donanımsal JPEG çözücüsünü kullanarak SD karttaki görüntüleri LCD ekrana aktaran, bu mimariyi interrupt-driven bir pipeline'a dönüştüren ve son aşamada Ethernet üzerinden standart RTP/UDP protokolüyle gerçek zamanlı video akışına genişleten bir gömülü sistem projesidir.


## Donanım

| Bileşen | Detay |
|---|---|
| MCU | STM32H757XI — Cortex-M7 (bu projede 400 MHz'e çıkarılmış, grafik/JPEG/ağ) + Cortex-M4 |
| Flash | 2 MB |
| Dahili RAM | 1 MB (DTCM dahil, çekirdekler arası paylaşımlı) |
| Harici RAM | 32 MB SDRAM, FMC (Flexible Memory Controller) üzerinden, 8M×32 bit |
| Ekran | 800×480 landscape veya 480×800 portrait, MIPI DSI Host arayüzlü. Panel: KoD KM-040TMP-02-0621 (sürücü IC: Orise Technology OTM8009A), fiziksel çözünürlük dikey 480×800 — **proje şu an native portrait modda çalışıyor** |
| Depolama | SD kart, FatFs dosya sistemi (SD kart demosu ve ilk mimaride; canlı ağ akışında artık kullanılmıyor) |
| Ağ | Ethernet MAC/PHY + lwIP TCP/IP yığını |

## Sistem Mimarisi

STM32H757XI çift çekirdekli bir yapıya sahiptir:

- **Cortex-M7**: ekranın çizilmesi ve yönetilmesinden sorumludur; bu projedeki tüm JPEG decode, DMA2D ve LCD işlemleri bu çekirdek üzerinde yürütülür.
- **Cortex-M4**: dual-core boot senaryosunu göstermek amacıyla arka planda çalıştırılır (Deepsleep/STOP modunda tutulur), bu projede aktif bir görev üstlenmez.

Sistem **bare-metal** olarak çalışmaktadır; **RTOS kullanılmamaktadır.** lwIP `NO_SYS=1` modunda yapılandırılmış olup `MX_LWIP_Process()` fonksiyonu ayrı bir thread yerine `main()` döngüsünden elle çağrılmaktadır.


800×480 (ya da 480×800) çözünürlükte ARGB8888 (32 bit) formatındaki bir görüntünün tek bir framebuffer boyutu ~1,5 MB'tır (JPEG dosyasının kendisi 24 bittir.). Bu boyut MCU'nun 1 MB'lık dahili RAM'ine sığmadığından harici 32 MB SDRAM zorunludur.

## Bellek Mimarisi

| Bellek | Adres | Boyut | Erişim | Not |
|---|---|---|---|---|
| Flash | 0x08000000 | 2 MB | CPU | Program kalıcı deposu |
| DTCM RAM | 0x20000000 | 128 KB | Yalnızca Cortex-M7 + MDMA | Cache'e alınmaz, cache tutarlılığı sorunu oluşmaz; stack/heap ve global handle'lar (`JPEG_Handle`, `DMA2D_Handle`, `SDFatFs`, `DecodeTime_ms[]` vb.) burada tutulur. **Genel Ethernet DMA erişemez** |
| RAM_D1 (AXI SRAM) | 0x24000000 | 512 KB | DMA erişebilir | Ethernet descriptor/RX buffer havuzu burada |
| D2 domain SRAM | 0x30000000 | — | DMA erişebilir | lwIP `LWIP_RAM_HEAP_POINTER` burada |
| SDRAM | 0xD0000000 | 32 MB | CPU + LTDC + DMA2D + MDMA | Görüntü tamponları |

**Cache coherence (önbellek tutarlılığı):** LTDC, DMA2D ve MDMA gibi donanım birimleri SDRAM'e doğrudan erişirken CPU tarafında önbellek etkindir; bu nedenle CPU'nun gördüğü veri ile donanımın okuduğu veri arasında tutarsızlık oluşabilir. Bu projede SDRAM, `MPU_Config()` içinde **cacheable, Write-Through** modda yapılandırılmıştır.

**SDRAM adres haritası:**

| Adres | Tanım |
|---|---|
| `0xD0400000` | `JPEG_RAW_BUFFER_0` — slot 0'ın sıkıştırılmış hâli |
| `0xD0600000` | `JPEG_RAW_BUFFER_1` — slot 1'in sıkıştırılmış hâli |
| `0xD0A00000` | `JPEG_RAW_BUFFER_2` — slot 2'nin sıkıştırılmış hâli (triple buffering ile eklendi) |
| `0xD0200000` | `JPEG_OUTPUT_DATA_BUFFER` — decoder'ın ham YCbCr çıktısı (tekil, slot'lara göre dönmüyor) |
| `0xD0000000` | `LCD_FRAME_BUFFER` — slot 0 ekran tamponu |
| `0xD0800000` | `LCD_FRAME_BUFFER_1` — slot 1 ekran tamponu |
| `0xD0C00000` | `LCD_FRAME_BUFFER_2` — slot 2 ekran tamponu (triple buffering ile eklendi) |

Tamponlar arasında bilinçli olarak 2 MB'lık paylar bırakılmıştır (`JPEG_RAW_BUFFER_MAX_SIZE` ile aynı büyüklükte); amaç bir sonraki tamponun üzerine taşmayı önlemektir.

## Görüntü Dosya Formatı Seçimi

SD karttan ekrana aktarım için FatFs kullanılmaktadır. LTDC ve DMA2D gibi birimler yalnızca RGB565/ARGB8888 gibi ham piksel verisiyle çalıştığından dosya formatının bu forma dönüştürülmesi gerekir:

| Format | Not |
|---|---|
| BMP | Sıkıştırılmamış; başlık atlandıktan sonra piksel verisi doğrudan framebuffer'a kopyalanabilir. |
| JPEG | Ek decode gerektirir; STM32H7'de donanımsal kodek mevcut olduğundan CPU'suz çözülebilir. |
| PNG | Ek decode gerektirir; donanımsal destek yoktur, yazılımsal kütüphane gerekir. |

Donanım hızından yararlanmak amacıyla JPEG formatı tercih edilmiştir.

## JPEG Donanım Decoder

Decoder, sıkıştırılmış veriyi sırasıyla şu aşamalardan geçirir:

1. **Huffman kod çözme** — değişken uzunluklu kodlar orijinal biçimine dönüştürülür.
2. **Ters kuantalama (inverse quantization)** — sıkıştırma sırasında kuantalama tablolarına bölünerek küçültülen değerler aynı tablolarla yeniden büyütülür. Yuvarlama nedeniyle bilgi kalıcı olarak kaybolur; bu JPEG'i **lossy (kayıplı)** bir format yapar.
3. **Ters ayrık kosinüs dönüşümü (IDCT)** — 8×8 piksellik bloklar hâlinde matematiksel olarak ifade edilmiş veriler yeniden piksel değerlerine dönüştürülür.

Çıktı **YCbCr** formatındadır (Y: parlaklık, Cb: mavi bileşen, Cr: kırmızı bileşen). İnsan gözü parlaklığa renkten daha duyarlı olduğundan renk bilgisi **chroma subsampling** ile sıkıştırılır:

| Mod | Saklanan renk bilgisi |
|---|---|
| 4:4:4 | %100 (kayıpsız) |
| 4:2:2 | %50 |
| 4:2:0 | %25 (JPEG dosyalarının büyük kısmı, bu proje dahil) |

**Donanım kısıtları:** STM32H7 JPEG birimi yalnızca **baseline, YCbCr, 24 bit** JPEG dosyalarını çözebilir. **CMYK renk uzayı (32 bit)** ve **progressive JPEG** desteklenmez; çözünürlük ekranın kendi çözünürlüğünü aşamaz.

> Geliştirme sürecinde CMYK/32 bit kaydedilmiş bir görüntünün ekranda gösterilemediği gözlemlenmiş, görüntü 24 bit olarak yeniden kaydedildiğinde sorun ortadan kalkmıştır. Bu bir yazılım hatası değil, donanım format kısıtıdır.

## Decoder Kontrol Fonksiyonları (HAL_JPEG API)

| Fonksiyon | Rol |
|---|---|
| `HAL_JPEG_Decode_DMA(hjpeg, inBuf, inSize, outBuf, outSize)` | Decode'u DMA ile, non-blocking başlatır |
| `HAL_JPEG_ConfigInputBuffer(hjpeg, addr, size)` | Bir sonraki giriş verisinin konumunu bildirir |
| `HAL_JPEG_ConfigOutputBuffer(hjpeg, addr, size)` | Bir sonraki çıktının yazılacağı adresi bildirir |
| `HAL_JPEG_DataReadyCallback()` | Her 64 KB (`CHUNK_SIZE_OUT`) veri üretildiğinde tetiklenir |
| `HAL_JPEG_GetInfo(&JPEG_Handle, &JPEG_Info)` | Decode bitince genişlik/yükseklik/renk uzayı/chroma subsampling/kalite bilgisini döndürür |

`HAL_JPEG_Pause` / `HAL_JPEG_Resume`, verinin SD karttan parça parça okunduğu ilk mimaride kullanılmış; verinin tamamının önceden RAM'e alındığı mimariye geçişle kaldırılmıştır.

## Görüntü Verisinin Bellekteki Akışı

```
1. SD kart / Ağ ──▶ RAM'deki ham tampon (sıkıştırılmış JPEG)
2. HAL_JPEG (donanım) ──▶ JPEG_OUTPUT_DATA_BUFFER (YCbCr)
3. DMA2D (YCbCr→RGB dönüşüm) ──▶ LCD_FRAME_BUFFER[idx] (ARGB8888)
4. LTDC ──▶ sürekli okuma ──▶ LCD
```

*(Not: bu akışta eskiden decode ile DMA2D dönüşümü arasında bir "beyaz dolgu" adımı vardı — kaldırıldı, bkz. [Köşegen Yırtılma ve Kasma — Tam Çözüm Öyküsü](#köşegen-yırtılma-ve-kasma--tam-çözüm-öyküsü).)*

## SD Kart Okuma Mimarisinin Evrimi

**İlk mimari:** her decode döngüsünde SD karttan fiziksel okuma yapılır. İki adet 4096 byte'lık giriş tamponu (`JPEG_Data_InBuffer0`/`1`) "ping-pong" (çift tamponlama) şeklinde kullanılır: biri decoder'a veri sağlarken diğerine SD'den yeni veri okunur. Decode hızı, SD kartın görece yavaş okuma hızına bağımlı kalır.

**Sonraki mimari — dört değişiklik:**

- Her JPEG dosyası açılışta **bir kez** SD'den okunup SDRAM'de sıkıştırılmış hâliyle saklanır.
- Decode döngüsünde SD'ye hiç erişilmez; decode RAM'deki kopyalardan yapılır.
- Görüntüler arası gecikme kaldırılmış, decode'lar art arda en kısa sürede yapılır.
- Her decode'un başında/sonunda `HAL_GetTick()` ile zaman damgası alınır.

**Güncel mimaride** bu tamamen ağ akışına devrolmuş durumda: SD hiç kullanılmıyor, ham JPEG baytları doğrudan `network_stream.c` tarafından ağdan `JPEG_RAW_BUFFER_x` tamponlarına yazılıyor.


## Performans Ölçümü ve Analizi

### İlk decode süresi ölçümü

`DecodeTime_ms[idx] = HAL_GetTick() - tick_start;` — `HAL_GetTick()`, `HAL_Init()` içinde 1ms çözünürlüklü yapılandırılan SysTick'e dayanır.

20ms'lik decode süresi → `1000/20 = 50 FPS`. Bu değer insan gözünün akıcılık eşiğinin (10-15 FPS) ve video standardının (24-30 FPS) belirgin üzerindedir; ancak gösterilen içerik video değil, iki sabit görüntü arasında saniyede 50 kez yapılan geçiş olduğundan göz bunu titreşen/karışmış bir görüntü olarak algılar.

### Kopyalama (DMA2D) süresinin ölçülmesi

`CopyTime_ms[idx]`, `DMA2D_CopyBuffer()` çağrısının başı/sonu ile ölçülür. Beklenti, donanımsal ~1,46 MB'lık transferin 1ms'nin altında (ölçülemez) olmasıydı; gerçekte decode süresine yakın (13-14ms) çıkmıştır, çünkü DMA2D düz bir `memcpy` değil, her piksel için gerçek hesaplama yapmaktadır: YCbCr → ARGB8888 renk uzayı dönüşümü, chroma subsampling'in geri açılması, alpha kanalı ekleme.

### Decode süresinin görüntü içeriğine bağlılığı

Düz/tek renkli alanlarda yüksek frekans bileşeni (ani renk değişimi) az olduğundan Huffman'ın çözeceği veri ve IDCT'nin işleyeceği katsayı sayısı azalır; içerik ne kadar basitse decode o kadar hızlıdır.

## Framebuffer Swapping Mimarisi

**Tespit edilen sorun:** tek `JPEG_OUTPUT_DATA_BUFFER` ve tek `LCD_FRAME_BUFFER` kullanıldığında her döngüde veri yeniden hesaplanıp aynı adrese yazılıyor, her geçişte 13-14ms harcanıyordu.

**Hedef mimari:** her görüntü için ayrı bir sonuç tamponu, bir kez decode edilip bu tamponlara yazılır; ekran değişiminde veri kopyalanmaz, yalnızca LTDC'nin okuduğu adres değiştirilir.

**Ölçüm sonuçları:**

- Eski mimaride geçiş ~27-28ms (saniyede ~35 geçiş).
- Yeni mimaride `BSP_LCD_SetLayerAddress()` tek bir register yazımı; DWT (Data Watchpoint and Trace) cycle counter ile ölçülen süre **~7 mikrosaniye** — yaklaşık **5000 kat** hızlanma.
- Yan etki: geçiş o denli hızlanmıştı ki ekranda karıncalanma/statik gürültüye benzer bir görsel bozulma oluşmuştu — bu, ileride triple buffering ve VBLANK senkronizasyonuna duyulan ihtiyacın ilk işaretiydi.

## Sürekli Yeniden Decode Mimarisi

Ağ üzerinden görüntü akışı hedeflendiğinden decode adımı, bir kez yapılan sabit işlemden **sürekli tekrarlanan bir döngüye** dönüştürülmüştür: her görüntü gösterildikten hemen sonra bir sonrakinin decode'u başlatılır, bu sonsuza dek tekrarlanır.

**Adres değiştirmenin (point etme) değişen rolü:** ilk mimaride amaç yeniden hesaplama yapmamaktı; sürekli decode eden yapıda ise amaç **tearing önlemedir**. Ekranda bir tampon gösterilirken arka planda diğerine yeni veri yazılır, tampon hazır olunca adres değiştirilir.

## Üçlü Tamponlama

- **Ekran framebuffer'ları (`LCD_FRAME_BUFFER`/`_1`/`_2`):** LTDC (ekran denetleyicisi) donanımı, `BSP_LCD_SetLayerAddress()` çağrıldığında adresi hemen değil, bir sonraki ekran tazeleme anında (VSYNC) değiştirebiliyor. DMA2D "eski" tampona hemen yazmaya başlarsa LTDC hâlâ o buffer'ı okuyor olabilir, bu da ekranda kısa süreli bir yırtılmaya (tearing) sebep olabilir. Üçüncü bir buffer eklersek, DMA2D hiçbir zaman LTDC'nin o an okuduğu buffer'a dokunmaz.
- **Ham JPEG tamponları (`JPEG_RAW_BUFFER_0`/`_1`/`_2`):** ağ, decode/gösterim hızını geçtiğinde (özellikle büyütülmüş RX tamponlarıyla ağdan daha "patlamalı" veri gelebildiğinden), üçüncü bir buffer kartın kare atmadan biraz daha nefes almasını sağlar.

## Ağ Alt Yapısı: lwIP

lwIP (Lightweight IP), özellikle bellek (RAM) kısıtlaması olan gömülü sistemler ve mikrodenetleyiciler için geliştirilmiş, açık kaynaklı bir TCP/IP ağ protokolü yığınıdır. Amacı, büyük bilgisayarlardaki karmaşık ağ yığınlarını küçülterek küçük çiplere internet yeteneği kazandırmaktır.

**Temel özellikler:**
- **Desteklenen protokoller:** IPv4, IPv6, TCP, UDP, ICMP, DHCP, DNS ve ARP.
- **Düşük bellek kullanımı:** kod boyutu küçük tutulmuş, birkaç kilobayt RAM ile çalışabilecek şekilde optimize edilmiştir.
- **İşletim sistemi desteği:** ister bir RTOS ile, ister hiçbir işletim sistemi olmadan (bağımsız/raw, bu projedeki gibi) çalışabilir.

Önemli bir netleştirme: **lwIP, UDP/IP'nin kendisi değildir** — UDP, IP, TCP, DHCP, ICMP gibi protokolleri uygulayan lightweight TCP/IP stack'tir; bu protokoller lwIP'nin İÇİNDE bulunan katmanlardır.

Neden "lightweight" (hafif): normal bir bilgisayardaki TCP/IP yığını (Windows/Linux'unki gibi) bol RAM ve işlemci gücü varsayarak tasarlanmıştır. lwIP ise birkaç kilobayt RAM ile bile çalışabilecek şekilde küçültülmüş, STM32 gibi sınırlı kaynaklı çiplere uygun bir versiyondur.

*(Geliştirme sürecinde, STM32 donanımından tamamen bağımsız olarak TCP/IP'nin ve ağ programlamanın nasıl işlediğini görmek için lwIP'nin Unix simülasyonu (`contrib/ports/unix/proj/unixsim`, WSL üzerinde) ile de denemeler yapıldı — `simhost` çalıştırılıp `ping` ile ve `ffplay -protocol_whitelist file,udp,rtp` ile bir `.sdp` dosyası üzerinden gerçek bir RTP akışının oynatılabildiği doğrulandı (VLC değil ffplay çalıştı). Bu, "sadece paket gidiyor mu" sorusundan "bu paketlerden gerçek bir video çıkarabilir miyiz" sorusuna geçişin ilk adımıydı, donanımdan bağımsız bir öğrenme/doğrulama egzersiziydi.)*

## RTP Kavramı

RTP = **Real-time Transport Protocol**, yani Gerçek Zamanlı Taşıma Protokolü. Özellikle ses, video ve canlı görüntü gibi gerçek zamanlı verileri ağ üzerinden taşımak için kullanılır.

**RTP'nin önemli özellikleri:**
- Gerçek zamanlı veri taşımaya odaklanır.
- Paketlerin sırasını takip etmek için **sequence number** kullanır.
- Paketlerin hangi zamana ait olduğunu anlamak için **timestamp** taşır.
- Hangi medya akışının olduğunu belirtmek için **payload type** bilgisi içerir.
- Genellikle UDP üzerinde çalışır; TCP'nin "paket kesinlikle ulaşacak, ulaşmazsa tekrar gönder" yaklaşımını kullanmaz.

**RTP nerede devreye giriyor:** görüntü verisini doğrudan UDP'ye vermek yerine, önce onun üzerine bir RTP başlığı ekleniyor. Bu başlık alıcıya "bu paket hangi video verisinin parçası, sırası ne, hangi zamana ait ve hangi akışa ait?" bilgisini veriyor.

- **RTP** = gönderilecek verinin nasıl paketleneceğini belirleyen medya protokolü.
- **lwIP** = bu paketlerin IP ağı üzerinden nasıl taşınacağını sağlayan ağ altyapısı.

RTP, UDP'nin içine oturuyor; UDP ise IP'nin üzerinde çalışıyor; lwIP ise bunların ağ tarafındaki implementasyonunu sağlıyor.

## Ethernet Bağlantısı: Karşılaşılan Sorunlar

Ethernet eklendiğinde kart PC'nin ARP isteklerine yanıt vermiyor, ping `Destination host unreachable` hatası veriyordu — PC, kartın MAC adresini bile öğrenemiyordu. Sistematik inceleme (derleyici çıktısı, debugger ile canlı değişken takibi, Wireshark ile ağ paketlerinin ham baytlarını inceleme) üç kök neden ortaya çıkarmıştır. 

1. **DTCM erişim sorunu:** STM32H7'de DTCM (0x20000000) diye bir bellek bölgesi var, bu sadece CPU'ya özel, hiçbir DMA denetleyicisi (Ethernet dahil) oraya erişemiyor. Projenin linker script'i (derleyicinin değişkenleri nereye yerleştireceğini belirleyen dosya), Ethernet descriptor/RX tamponlarını yanlışlıkla bu DTCM bölgesine yerleştirmişti — Ethernet donanımı kendi çalışması için gereken veriye fiziksel olarak ulaşamıyordu. **Çözüm:** veriler DMA'nın erişebildiği `RAM_D1` (0x24000000, AXI SRAM) bölgesine taşındı.
2. **MPU yapılandırma hatası:** `RAM_D1`'i önbelleksiz yaparken "Normal, önbelleksiz" yerine yanlışlıkla çok daha katı olan **"Strongly Ordered"** tipi seçilmişti; bu tip hizasız (adres 4'e bölünmeyen) bellek erişimlerine hiç izin vermediğinden lwIP'nin ARP kodundaki basit bir kopyalama işlemi kartı anında çökertiyordu (HardFault). **Çözüm:** `TEX_LEVEL0` → `TEX_LEVEL1`.
3. **Gizli, ayrı bir bellek bölgesi:** Kart artık çökmüyordu ama gönderdiği ARP cevabı Wireshark'ta anlamsız baytlar içeriyordu. `lwipopts.h` dosyasında `LWIP_RAM_HEAP_POINTER`'ın (lwIP'nin tüm giden paket belleği) sabit bir adrese, `0x30000000`'a (D2 domain SRAM, önceki düzeltmelerden tamamen bağımsız bir bölge) bağlandığı bulundu — bu bölge hâlâ önbellekliydi. CPU'nun yazdığı ARP yanıtı sadece önbellekte kalıyor, DMA ise önbelleği atlayıp doğrudan fiziksel RAM'i okuyunca eski/çöp veri gönderiyordu. **Çözüm:** bu bölge için üçüncü bir MPU alanı tanımlanıp önbelleksiz yapıldı.

**Doğrulama:** PC (192.168.1.10) — Kart (192.168.1.20) arası ping, ortalama 5ms, %0 paket kaybı:

## TCP Tabanlı İlk Aktarım Mimarisi

**Protokol:** ham TCP soketi + her JPEG karesinin önüne eklenen 4 byte'lık uzunluk bilgisi, port **5001**.

**Bağlantı anının sırası:**

1. Kart açılır, `MX_LWIP_Init()` ağı başlatır, `Network_Stream_Init()` port 5001'i dinlemeye alır. Kart artık pasif bekliyor, hiçbir şey göndermiyor.
2. `python send_webcam_stream.py` çalıştırılır. Script webcam'i açar, sonra `connect()` çağrılır.
3. Bu `connect()` çağrısı arka planda standart bir TCP üçlü el sıkışma (**three-way handshake**) başlatır: PC'den karta SYN paketi gider, kart SYN-ACK ile cevap verir, PC ACK ile onaylar. Bu, `tcp_accept` callback'inin (kart tarafında) ve Python'daki `connect()`'in (PC tarafında) başarıyla dönmesiyle sonuçlanır.
4. Bağlantı kurulunca kart tarafında `Network_TcpAcceptCallback()` çalışır, `tcp_recv()` ile "veri gelince bu fonksiyonu çağır" diye kaydolur.
5. Artık her iki taraf da hazır: PC her kareyi `sock.sendall()` ile gönderiyor, kart tarafında her veri geldiğinde `Network_TcpRecvCallback()` otomatik tetikleniyor, 4-byte uzunluk + JPEG verisini parçalayıp okuyor.

Özetle: kart pasif bir sunucu gibi bekliyor, PC script'i aktif olarak ona bağlanıyor. Bağlantı bir kere kurulduktan sonra tek yönlü (PC → kart) sürekli veri akıyor.

**Port seçimi:** 0-1023 arası portlar ("well-known ports") HTTP (80), FTP (21), SSH (22), Telnet (23) gibi standart servislere ayrılmış, bunları kullanmak (Windows'ta yönetici izni gerektirebileceği için de) gereksiz karışıklık yaratabilir. 1024-49151 arası ("registered ports") bazıları belirli uygulamalara kayıtlı olsa da çoğu boşta ve serbestçe kullanılabilir. 5001 seçildi çünkü yaygın bilinen bir servisle çakışma ihtimali düşük.

## RTP/UDP Protokolüne Geçiş

### Getirdiği temel değişiklikler

- **Taşıma katmanı:** TCP yerine UDP — bağlantısız, güvenilir teslimat garantisi yok, ama gerçek zamanlı video için tercih edilen yöntem çünkü kayıp bir kareyi beklemek yerine atlayıp devam edilebiliyor.
- **Paket formatı:** her UDP paketi artık standart bir RTP başlığı (12 byte: versiyon, sequence number, timestamp, SSRC) ve RFC 2435'in tanımladığı JPEG'e özel bir başlık (8 byte: fragment offset, chroma subsampling tipi, kalite, genişlik/yükseklik) taşıyor.
- **Akıllı basitleştirme:** Quantization ve Huffman tablolarını hiç ağdan göndermiyoruz. RFC 2435'in izin verdiği bir yöntemle, sadece bir "kalite" sayısı (Q=80) gönderiliyor; hem PC hem kart bu sayıdan aynı standart formülle aynı tabloları kendi başına hesaplıyor. Kart tarafında bu, `network_stream.c` içindeki `JFIF_BuildHeader()` fonksiyonunda oluyor.
- **Parça birleştirme (fragmentation/reassembly):** bir JPEG karesi tek pakete sığmadığı için (genelde 15-40 KB), kart gelen parçaları sıra numarasına göre birleştirip tek bir geçerli JPEG dosyasına dönüştürüyor.

### Yol boyunca bulunan ve çözülen sorunlar

- **Asıl RTP hatası:** RTP paketleri doğru gönderiliyordu (Wireshark ile tek tek doğrulandı) ama kart hiçbir kareyi tamamlayamıyordu (`FrameCorrupt` sürekli 1). Sebep kod hatası değil, donanım kapasitesiydi: TCP'nin kendi hız kontrolü var, UDP'de böyle bir mekanizma yok — PC bir kare için 20-60 paketi art arda, hiç durmadan gönderiyor, bu paketler ~2-3ms içinde kartın kapısına dayanıyor ama kart bunları en fazla 1ms'de bir kontrol edebiliyordu. `ETH_RX_DESC_CNT=4` ve `ETH_RX_BUFFER_CNT=12` bu patlamayı karşılayamıyordu (bkz. Alım Tamponu Boyutlandırması).

*"TCP'nin kendiliğinden yaptığı hız ayarlamasını UDP'de kendimiz donanım tarafında (daha büyük bir tampon ayırarak) telafi etmemiz gerekti, protokolün kendisi zaten baştan doğruydu."*

## PC Tarafı Script'leri

`PC_Sender/` klasöründe, aynı RTP/JPEG gönderim mantığını paylaşan iki bağımsız Python script'i bulunur — hangisi terminalde çalıştırılırsa o çalışır, birbirlerinden bağımsızlardır:

```bash
python "C:\Users\Desktop\HAVELSAN_STM32\JPEG_DecodingUsingFs_DMA\PC_Sender\send_webcam_stream.py"
python "C:\Users\Desktop\HAVELSAN_STM32\JPEG_DecodingUsingFs_DMA\PC_Sender\send_video_file_stream.py"
```

| Script | Kaynak | Not |
|---|---|---|
| `send_webcam_stream.py` | PC'nin webcam'i | Yatay ayna çevirme, kırp-doldur (letterbox yok), temiz pencere kapatma. Kartın güncel **portrait (480×800)** yönüne uydurulmuştur. |
| `send_video_file_stream.py` | Yerel video dosyası (`video0_480_800.avi`) | Dosyanın kendi FPS'ine göre düzenli aralıklarla gönderim yapar; video bitince başa sarıp döngüye girer. |

## Kart Tarafı Hattı

| Dosya | Sorumluluk |
|---|---|
| `network_stream.c` | UDP/RTP alımı, JFIF başlık yeniden inşası, ham JPEG baytlarını tampona yazma, `FrameReady[]`/`FrameSeqNum[]` bayrağını ayarlama |
| `decode_dma.c` | JPEG donanım kod çözücüsünü RAM'den besleyen callback'ler |
| `main.c` | Pipeline durum makinesi, DMA2D/LTDC yönetimi, MPU/saat konfigürasyonu |

## Köşegen Yırtılma ve Kasma — Tam Çözüm Öyküsü

### Sorun neydi

Ekranda sabit, köşegen bir görüntü yırtılması vardı — akış sırasında ekranın bir kısmı eski kareyi, diğer kısmı yeni kareyi gösteriyordu. Triple buffering'e geçtikten sonra ortaya çıkmıştı.

### Kesin çözüm: Native Portrait Moda Geçiş

- `main.c`: `BSP_LCD_Init(LANDSCAPE)` yerine `BSP_LCD_InitEx(0, LCD_ORIENTATION_PORTRAIT, ..., 480, 800)` kullanıldı — bu, hem MADCTL takas komutunu tamamen devre dışı bırakıyor hem LTDC/DSI Host'u gerçekten 480×800 olarak başlatıyor (basit `BSP_LCD_Init` sarmalayıcısı orientasyondan bağımsız hep 800×480 geçtiği için doğrudan `InitEx` kullanmak zorunluydu).
- PC script'leri (`send_video_file_stream.py`, `send_webcam_stream.py`): 480×800 boyutlarına uyduruldu.

**Sonuç: Köşegen yırtılma sorunu tamamen çözüldü.**

**Bağımsız doğrulama:** Aynı panel ailesinde (OTM8009A/NT35510) landscape modda tam olarak bu köşegen yırtılma sorununu ST'nin resmi desteğinin de doğruladığı bulundu: *"panel dahili olarak dikey tarıyor, landscape modda çalışacak şekilde tasarlanmamış."* Bu, native portrait'e geçme çözümünün doğru olduğunu bağımsız bir kaynaktan teyit etti.
