# STM32H757XI JPEG Decode & Ağ Üzerinden Görüntü Aktarım Sistemi

STM32H757XI mikrodenetleyicisinin donanımsal JPEG çözücüsünü kullanarak SD karttaki görüntüleri LCD ekrana aktaran, bu mimariyi kesme tabanlı (interrupt-driven) bir pipeline'a dönüştüren ve son aşamada Ethernet üzerinden standart RTP/UDP protokolüyle gerçek zamanlı video akışına genişleten bir gömülü sistem projesidir.

**Güncel durum:** Sistem çalışıyor (PC → Ethernet/UDP → kart RTP/JPEG akışı, JPEG donanım kod çözme, DMA2D dönüşümü, LTDC gösterim, üçlü tamponlama) — tek açık sorun, akış sırasında oluşan sabit köşegen ekran yırtılması (bkz. [Köşegen Görüntü Yırtılması](#köşegen-görüntü-yırtılması-diagonal-tearing)).

---

## Donanım

| Bileşen | Detay |
|---|---|
| MCU | STM32H757XI — Cortex-M7 (bu projede 400 MHz'e çıkarılmış, grafik/JPEG/ağ) + Cortex-M4 |
| Flash | 2 MB |
| Dahili RAM | 1 MB (DTCM dahil, çekirdekler arası paylaşımlı) |
| Harici RAM | 32 MB SDRAM, FMC (Flexible Memory Controller) üzerinden, 8M×32 bit |
| Ekran | 800×480, MIPI DSI Host arayüzlü, Video Mode/Burst. Panel: KoD KM-040TMP-02-0621 (sürücü IC: Orise Technology OTM8009A), fiziksel çözünürlük dikey 480×800 |
| Depolama | SD kart, FatFs dosya sistemi (SD kart demosu ve ilk mimaride; canlı ağ akışında artık kullanılmıyor) |
| Ağ | Ethernet MAC/PHY + lwIP TCP/IP yığını |

## Sistem Mimarisi

STM32H757XI çift çekirdekli bir yapıya sahiptir:

- **Cortex-M7**: ekranın çizilmesi ve yönetilmesinden sorumludur; bu projedeki tüm JPEG decode, DMA2D ve LCD işlemleri bu çekirdek üzerinde yürütülür.
- **Cortex-M4**: dual-core boot senaryosunu göstermek amacıyla arka planda çalıştırılır, bu projede aktif bir görev üstlenmez.

Sistem **bare-metal** olarak çalışmaktadır; RTOS kullanılmamaktadır. lwIP `NO_SYS` modunda yapılandırılmış olup `MX_LWIP_Process()` fonksiyonu ayrı bir thread yerine `main()` döngüsünden elle çağrılmaktadır.

800×480 çözünürlükte ARGB8888 (32 bit) formatındaki bir görüntünün tek bir framebuffer boyutu ~1,5 MB'tır (JPEG dosyasının kendisi 24 bittir; framebuffer'a yazım sırasında alpha kanalı eklenerek 32 bit ARGB8888'e dönüştürülür). Bu boyut MCU'nun 1 MB'lık dahili RAM'ine sığmadığından harici 32 MB SDRAM zorunludur.

## Bellek Mimarisi

| Bellek | Adres | Boyut | Erişim | Not |
|---|---|---|---|---|
| Flash | 0x08000000 | 2 MB | CPU | Program kalıcı deposu |
| DTCM RAM | 0x20000000 | 128 KB | Yalnızca Cortex-M7 + MDMA | Cache'e alınmaz, cache tutarlılığı sorunu oluşmaz; stack/heap ve global handle'lar (`JPEG_Handle`, `DMA2D_Handle`, `SDFatFs`, `DecodeTime_ms[]` vb.) burada tutulur |
| RAM_D1 (AXI SRAM) | 0x24000000 | 512 KB | DMA erişebilir | Ethernet descriptor/RX buffer havuzu burada |
| D2 domain SRAM | 0x30000000 | — | DMA erişebilir | lwIP `LWIP_RAM_HEAP_POINTER` burada |
| SDRAM | 0xD0000000 | 32 MB | CPU + LTDC + DMA2D + MDMA | Görüntü tamponları |

**Cache coherence (önbellek tutarlılığı):** LTDC, DMA2D ve MDMA gibi donanım birimleri SDRAM'e doğrudan erişirken CPU tarafında önbellek etkindir; bu nedenle CPU'nun gördüğü veri ile donanımın okuduğu veri arasında tutarsızlık oluşabilir. Bu projede SDRAM, `MPU_Config()` içinde **cacheable, Write-Through** modda yapılandırılmıştır. DMA ile paylaşılan diğer bölgeler (RAM_D1, D2-SRAM) ise tamamen **non-cacheable** yapılmıştır (bkz. [Ethernet Bağlantısı: Karşılaşılan Sorunlar](#ethernet-bağlantısı-karşılaşılan-sorunlar)).

**SDRAM adres haritası (güncel — üçlü tamponlama):**

| Adres | Tanım |
|---|---|
| `0xD0000000` | `LCD_FRAME_BUFFER` — slot 0 ekrana basılan ARGB8888 görüntü |
| `0xD0200000` | `JPEG_OUTPUT_DATA_BUFFER` — decoder'ın ham YCbCr çıktısı (**tekil**, slot'lara göre dönmüyor) |
| `0xD0400000` | `JPEG_RAW_BUFFER_0` — slot 0'ın sıkıştırılmış hâli |
| `0xD0600000` | `JPEG_RAW_BUFFER_1` — slot 1'in sıkıştırılmış hâli |
| `0xD0800000` | `LCD_FRAME_BUFFER_1` — slot 1 ekran tamponu |
| `0xD0A00000` | `JPEG_RAW_BUFFER_2` — slot 2'nin sıkıştırılmış hâli (triple buffering ile eklendi) |
| `0xD0C00000` | `LCD_FRAME_BUFFER_2` — slot 2 ekran tamponu (triple buffering ile eklendi) |

Tamponlar arasında bilinçli olarak 2 MB'lık paylar bırakılmıştır (`JPEG_RAW_BUFFER_MAX_SIZE` ile aynı büyüklükte); amaç bir sonraki tamponun üzerine taşmayı önlemektir. Rotasyondaki slot sayısı, `main.h` içindeki tek bir sabitle (`NB_IMAGES`, şu an 3) kontrol edilir; koddaki hiçbir yerde sabit "2" ya da "3" yazmaz.

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

**Donanım kısıtları:** STM32H7 JPEG birimi yalnızca **baseline, YCbCr, 24 bit** JPEG dosyalarını çözebilir. **CMYK renk uzayı (32 bit)** ve **progressive JPEG** desteklenmez; çözünürlük ekranın kendi çözünürlüğü olan **800×480**'i aşamaz.

> Geliştirme sürecinde CMYK/32 bit kaydedilmiş bir görüntünün ekranda gösterilemediği gözlemlenmiş, görüntü 24 bit olarak yeniden kaydedildiğinde sorun ortadan kalkmıştır. Bu bir yazılım hatası değil, donanım format kısıtıdır.

## Decoder Kontrol Fonksiyonları (HAL_JPEG API)

| Fonksiyon | Rol |
|---|---|
| `HAL_JPEG_Decode_DMA(hjpeg, inBuf, inSize, outBuf, outSize)` | Decode'u DMA ile, non-blocking başlatır |
| `HAL_JPEG_ConfigInputBuffer(hjpeg, addr, size)` | Bir sonraki giriş verisinin konumunu bildirir |
| `HAL_JPEG_ConfigOutputBuffer(hjpeg, addr, size)` | Bir sonraki çıktının yazılacağı adresi bildirir |
| `HAL_JPEG_DataReadyCallback()` | Her 64 KB (`CHUNK_SIZE_OUT`) veri üretildiğinde tetiklenir |
| `HAL_JPEG_GetInfo(&JPEG_Handle, &JPEG_Info)` | Decode bitince genişlik/yükseklik/renk uzayı/chroma subsampling/kalite bilgisini döndürür |

`HAL_JPEG_Pause` / `HAL_JPEG_Resume`, verinin SD karttan parça parça okunduğu ilk mimaride kullanılmış; verinin tamamının önceden RAM'e alındığı mimariye geçişle (bkz. [SD Kart Okuma Mimarisinin Evrimi](#sd-kart-okuma-mimarisinin-evrimi)) kaldırılmıştır.

## Görüntü Verisinin Bellekteki Akışı

```
1. SD kart / Ağ ──▶ RAM'deki ham tampon (sıkıştırılmış JPEG)
2. HAL_JPEG (donanım) ──▶ JPEG_OUTPUT_DATA_BUFFER (0xD0200000, YCbCr)
3. DMA2D (YCbCr→RGB dönüşüm) ──▶ LCD_FRAME_BUFFER[idx] (ARGB8888)
4. LTDC ──▶ sürekli okuma ──▶ LCD
```

## SD Kart Okuma Mimarisinin Evrimi

**İlk mimari:** her decode döngüsünde SD karttan fiziksel okuma yapılır. İki adet 4096 byte'lık giriş tamponu (`JPEG_Data_InBuffer0`/`1`) "ping-pong" (çift tamponlama) şeklinde kullanılır: biri decoder'a veri sağlarken diğerine SD'den yeni veri okunur. `JPEG_InputHandler()` ve `main.c` içindeki `do-while` döngüsü bu akışı yönetir. Decode hızı, SD kartın görece yavaş okuma hızına bağımlı kalır.

**Sonraki mimari — dört değişiklik:**

- Her JPEG dosyası açılışta **bir kez** SD'den okunup SDRAM'de sıkıştırılmış hâliyle saklanır.
- Decode döngüsünde SD'ye hiç erişilmez; decode RAM'deki kopyalardan yapılır (`JPEG_Data_InBuffer0/1` ve `JPEG_InputHandler()` kaldırılmıştır).
- Görüntüler arası gecikme kaldırılmış, decode'lar art arda en kısa sürede yapılır.
- Her decode'un başında/sonunda `HAL_GetTick()` ile zaman damgası alınır.

SD→SDRAM aktarımında CPU veriyi doğrudan okuyup yazar; ara bir geçici depo kullanılmaz.

**Güncel mimaride** bu tamamen ağ akışına devrolmuş durumda: SD hiç kullanılmıyor, ham JPEG baytları doğrudan `network_stream.c` tarafından ağdan `JPEG_RAW_BUFFER_x` tamponlarına yazılıyor (bkz. [RTP/UDP Protokolüne Geçiş](#rtpudp-protokolüne-geçiş)).

## Performans Ölçümü ve Analizi

### İlk decode süresi ölçümü

`DecodeTime_ms[idx] = HAL_GetTick() - tick_start;` — `HAL_GetTick()`, `HAL_Init()` içinde 1ms çözünürlüklü yapılandırılan SysTick'e dayanır.

20ms'lik decode süresi → `1000/20 = 50 FPS`. Bu değer insan gözünün akıcılık eşiğinin (10-15 FPS) ve video standardının (24-30 FPS) belirgin üzerindedir; ancak gösterilen içerik video değil, **iki sabit görüntü arasında saniyede 50 kez yapılan geçiş** olduğundan göz bunu titreşen/karışmış bir görüntü olarak algılar.

### Kopyalama (DMA2D) süresinin ölçülmesi

`CopyTime_ms[idx]`, `DMA2D_CopyBuffer()` çağrısının başı/sonu ile ölçülür. Beklenti, donanımsal ~1,46 MB'lık transferin 1ms'nin altında (ölçülemez) olmasıydı; gerçekte **decode süresine yakın (13-14ms)** çıkmıştır, çünkü DMA2D düz bir `memcpy` değil, her piksel için gerçek hesaplama yapmaktadır:

- YCbCr → ARGB8888 renk uzayı dönüşümü
- Chroma subsampling'in geri açılması (4:2:0'da her Cb/Cr değerinin 4 piksele yayılması)
- Alpha kanalı ekleme

### Decode süresinin görüntü içeriğine bağlılığı

Ölçümlerde decode süresi 19-20ms'den 13-14ms'e kadar değişmiştir. Düz/tek renkli alanlarda yüksek frekans bileşeni (ani renk değişimi) az olduğundan Huffman'ın çözeceği veri ve IDCT'nin işleyeceği katsayı sayısı azalır; içerik ne kadar basitse decode o kadar hızlıdır.

## Framebuffer Swapping Mimarisi

**Tespit edilen sorun:** tek `JPEG_OUTPUT_DATA_BUFFER` ve tek `LCD_FRAME_BUFFER` kullanıldığından her döngüde `DMA2D_CopyBuffer()` ile veri yeniden hesaplanıp aynı adrese yazılmakta, her geçişte 13-14ms harcanmaktaydı.

**Hedef mimari:** her görüntü için ayrı bir sonuç tamponu (ekrana basılmaya hazır ARGB8888), bir kez decode edilip bu tamponlara yazılır; ekran değişiminde veri kopyalanmaz, yalnızca LTDC'nin okuduğu adres değiştirilir.

**Ölçüm sonuçları:**

- `LcdFrameBufferAddr[0]` = `0xD0000000`, `LcdFrameBufferAddr[1]` = `0xD0800000` (o dönemki çift tamponlu mimari; güncel üçlü tampon adres haritası için bkz. [Bellek Mimarisi](#bellek-mimarisi)).
- Eski mimaride geçiş ~27-28ms (saniyede ~35 geçiş).
- Yeni mimaride `BSP_LCD_SetLayerAddress()` tek bir register yazımı; DWT (Data Watchpoint and Trace) cycle counter ile ölçülen süre **7 mikrosaniye** — yaklaşık **5000 kat** hızlanma.
- Yan etki: geçiş o denli hızlanmıştır ki ekranda karıncalanma/statik gürültüye benzer bir görsel bozulma oluşmuştur (iki görüntü ayrı ayrı değil neredeyse aynı anda görünür gibi algılanmıştır). Bu, ileride üçlü tamponlama ve VBLANK senkronizasyonuna duyulan ihtiyacın ilk işaretiydi.

## Beklemesiz Yürütme: DMA ve Busy-Wait Ayrımı

`HAL_JPEG_Decode_DMA()` çağrıldığı anda döner ve CPU'yu meşgul etmez — Huffman/IDCT tamamen donanımda yürütülür. Ancak gelişimin bir aşamasında kodda şu satır tespit edilmişti:

```c
while(Jpeg_HWDecodingEnd == 0) { }
```

Bu döngü DMA'nın parçası değildir; uygulama kodu tarafından eklenmişti çünkü decode bitmeden `HAL_JPEG_GetInfo()` ve `DMA2D_CopyBuffer()` çağrılamaz, ayrıca süre ölçümü için başlangıç/bitiş noktası gerekir. Bu sırada CPU **busy-wait** hâlindeydi.

**Tam callback zincirine geçiş:** `while` kaldırılıp bir sonraki adımlar doğrudan `HAL_JPEG_DecodeCpltCallback()` içinden tetiklenmiştir; kod sıralı yapıdan bir **durum makinesine (state machine)** dönüşmüştür.

`__WFI()` (Wait For Interrupt): busy-wait CPU'yu tam hızda boşuna döndürüp güç tüketirken, `__WFI()` CPU saatini durdurup kesme gelene kadar düşük güçte bekletir:

```c
while(1) { __WFI(); }
```

`HAL_GetTick()` tek başına beklemeye neden olmaz; asıl fark, "bitti mi" bilgisinin **polling** (while döngüsü) ile mi yoksa **kesme/callback** ile mi öğrenildiğidir.

## Sürekli Yeniden Decode Mimarisi

Ağ üzerinden görüntü akışı hedeflendiğinden decode adımı, bir kez yapılan sabit işlemden **sürekli tekrarlanan bir döngüye** dönüştürülmüştür: her görüntü gösterildikten hemen sonra bir sonrakinin decode'u başlatılır, bu sonsuza dek tekrarlanır.

**Adres değiştirmenin (point etme) değişen rolü:** ilk mimaride amaç yeniden hesaplama yapmamaktı (decode döngü dışındaydı); sürekli decode eden yapıda ise amaç **tearing önlemedir**. Ekranda bir tampon gösterilirken arka planda diğerine yeni veri yazılır (ekranda görünmediği için güvenlidir), tampon hazır olunca adres değiştirilir. Bu olmasaydı, DMA2D henüz yazımı tamamlamamışken LTDC yarım/bozuk veriyi ekrana basar ve **tearing** oluşurdu.

**Performans:** ortalama 33ms/frame (13ms decode + 20ms kopyalama/dönüştürme + ihmal edilebilir switch) → `1000/33 ≈ 30,3 FPS`, video standardına (24-30 FPS) yakın.

## Pipeline Kontrol Akışı (Callback Zinciri)

Klasik bir `for`/`while` döngüsü yoktur; her adım tamamlandığında bir sonrakini kendisi tetikler (bayrak yarışı mantığı). Aşağıda, framebuffer swapping'in **ilk eklendiği**, henüz köşegen yırtılma düzeltmeleri (VBLANK reload, onaylı geçiş) uygulanmamış erken sürüm gösteriliyor:

```c
static void JPEG_Pipeline_Start(void)
{
  PipelineIdx  = 0;
  PipelineState = PIPE_DECODING;
  PipelineDecodeTickStart = HAL_GetTick();

  JPEG_Decode_DMA(&JPEG_Handle, (uint8_t *)ImageRawAddr[PipelineIdx],
                  ImageRawSize[PipelineIdx], JPEG_OUTPUT_DATA_BUFFER);
}
```

```c
static void DMA2D_XferCpltCallback(DMA2D_HandleTypeDef *hdma2d)
{
  if(PipelineState == PIPE_FILLING)
  {
    PipelineState = PIPE_COPYING;

    DMA2D_StartCopy((uint32_t *)JPEG_OUTPUT_DATA_BUFFER,
                    (uint32_t *)LcdFrameBufferAddr[PipelineIdx],
                    (uint16_t)PipelineXPos, (uint16_t)PipelineYPos,
                    (uint16_t)JPEG_Info.ImageWidth,
                    (uint16_t)JPEG_Info.ImageHeight,
                    JPEG_Info.ChromaSubsampling);
  }
  else if(PipelineState == PIPE_COPYING)
  {
    uint32_t tick_start, cyc_start;

    CopyTime_ms[PipelineIdx] = HAL_GetTick() - PipelineCopyTickStart;

    tick_start = HAL_GetTick();
    cyc_start  = DWT->CYCCNT;

    BSP_LCD_SetLayerAddress(0, 0, LcdFrameBufferAddr[PipelineIdx]);

    SwitchTime_ms[PipelineIdx] = HAL_GetTick() - tick_start;
    SwitchTime_us[PipelineIdx] = (DWT->CYCCNT - cyc_start)
                                 / (SystemCoreClock / 1000000U);

    /* Sıradaki görüntüye geçilir ve decode hemen başlatılır;
       bu akış sürekli tekrarlanır. */
    PipelineIdx = (PipelineIdx + 1) % NB_IMAGES;

    PipelineState = PIPE_DECODING;
    PipelineDecodeTickStart = HAL_GetTick();
    JPEG_Decode_DMA(&JPEG_Handle, (uint8_t *)ImageRawAddr[PipelineIdx],
                    ImageRawSize[PipelineIdx], JPEG_OUTPUT_DATA_BUFFER);
  }
}
```

**Güncel sürüm** (bkz. [Köşegen Görüntü Yırtılması](#köşegen-görüntü-yırtılması-diagonal-tearing)), yukarıdaki akışın üzerine iki katman daha ekler: `PIPE_COPYING` bitince adres **hemen** değil `BSP_LCD_Relaod(0, BSP_LCD_RELOAD_NONE)` ile sadece "hazırlanır", ardından `HAL_LTDC_Reload(&hlcd_ltdc, LTDC_RELOAD_VERTICAL_BLANKING)` ile bir sonraki dikey boşluğa (VSYNC) ertelenir; yeni bir `PIPE_WAITING_RELOAD` durumu, ekranın bu geçişi gerçekten uyguladığını `HAL_LTDC_ReloadEventCallback()` ile onaylamadan bir sonraki karenin decode'unu başlatmaz. Bu iki katman hâlâ kodda aktif, ama köşegen yırtılmayı tek başına çözmemiştir (ayrıntı için bkz. ilgili bölüm).

## HAL_Delay ve Kesme Bağlamı Kısıtı

Pipeline'ın tamamı kesme bağlamında çalışmaktadır. `HAL_Delay()`, SysTick'in her 1ms'de bir artırdığı `uwTick` sayacına dayanır. Bu fonksiyon `JPEG_IRQn` veya `DMA2D_IRQn` içine eklenirse, o kesme tamamlanmadan SysTick araya giremeyebilir; `uwTick` ilerlemez ve `HAL_Delay()` **sonsuza kadar beklemede kalarak sistemi kilitler**.

Öncelik yapılandırması: `DMA2D_IRQn` = 7, `SysTick` = 15 (düşük numara = yüksek öncelik). SysTick, DMA2D'den düşük öncelikli olduğundan DMA2D işi sürerken çalışamaz. Not: SysTick daha yüksek öncelikli olsaydı kesmeyi preempt edip `HAL_Delay()` tamamlanabilirdi — ancak kesme içinde uzun bekleme genel olarak kötü tasarımdır.

**Çözüm:** delay'i kesme bağlamından çıkarıp `main()`'in normal (non-interrupt) döngüsüne taşımak; bu, pipeline'ın sürekli-decode yapısında ek bir durum (state) yönetimi gerektirir.

## Üçlü Tamponlama

- **Ekran framebuffer'ları:** LTDC, adres değişimini hemen değil bir sonraki VSYNC anında uygulayabilir. DMA2D "eski" tampona hemen yazarsa LTDC hâlâ onu okuyor olabilir → kısa süreli tearing. Üçüncü bir tampon, DMA2D'nin LTDC'nin o an okuduğu tampona hiç dokunmamasını garanti eder.
- **Ham JPEG tamponları:** ağ hızı decode/gösterim hızını aştığında (özellikle büyütülmüş RX tamponlarıyla daha "patlamalı" veri geldiğinde), üçüncü tampon kare atlamadan ek nefes payı sağlar.

Slot sayısı `NB_IMAGES` sabitiyle (şu an 3) kontrol edilir; çift tampona (2) geri dönüş testinde köşegen yırtılmanın **devam ettiği** gözlenmiştir — yani triple buffering'in kendisi yırtılmanın kaynağı değildir (bkz. [Köşegen Görüntü Yırtılması](#köşegen-görüntü-yırtılması-diagonal-tearing)).

## Ağ Alt Yapısı: lwIP

lwIP (Lightweight IP), bellek kısıtlı gömülü sistemler için geliştirilmiş açık kaynaklı bir TCP/IP yığınıdır.

- **Desteklenen protokoller:** IPv4, IPv6, TCP, UDP, ICMP, DHCP, DNS, ARP.
- **Düşük bellek kullanımı:** birkaç kilobayt RAM ile çalışabilecek şekilde optimize edilmiştir.
- **İşletim sistemi desteği:** RTOS ile veya bağımsız (`NO_SYS`) çalışabilir — bu projede `NO_SYS` modu kullanılmaktadır.

HAL Ethernet sürücüsü MAC/PHY donanımıyla, lwIP ise TCP/UDP/IP/DHCP gibi protokol katmanlarıyla ilgilenir. Ethernet, kesme değil **polling** ile okunur: `ethernetif_input()`, DMA'nın alım halkasını doğrudan okur; bunun düzenli çalışması `MX_LWIP_Process()`'in ana döngüde her `__WFI()` uyanışında (1ms'lik SysTick sayesinde en az bu sıklıkta) çağrılmasına bağlıdır.

## RTP Kavramı

RTP (Real-time Transport Protocol), ses/video gibi gerçek zamanlı verilerin taşınması için kullanılır; genellikle **UDP** üzerinde çalışır ve her pakete **sıra numarası**, **zaman damgası** ve **payload type** bilgisi ekler. TCP'nin garantili/sıralı teslimat mekanizması canlı video için dezavantajdır — gecikmiş bir paketi yeniden göndermek genelde anlamsızdır. lwIP, UDP/IP dâhil alt katmanları sağlar; RTP bunun üzerine inşa edilen bir uygulama katmanı protokolüdür.

## Ethernet Bağlantısı: Karşılaşılan Sorunlar

Ethernet eklendiğinde kart PC'nin ARP isteklerine yanıt vermiyor, ping `Destination host unreachable` hatası veriyordu. Sistematik inceleme (derleyici çıktısı, debugger, Wireshark) üç kök neden ortaya çıkarmıştır:

1. **DTCM erişim sorunu:** linker script, Ethernet descriptor/RX tamponlarını yanlışlıkla DTCM'e (0x20000000) yerleştirmişti — bu bölgeye hiçbir DMA erişemez. **Çözüm:** veriler DMA'nın erişebildiği `RAM_D1` (0x24000000, AXI SRAM) bölgesine taşınmıştır.
2. **MPU yapılandırma hatası:** `RAM_D1` önbelleksiz yapılandırılırken "Normal, önbelleksiz" yerine yanlışlıkla çok daha katı olan **"Strongly Ordered"** tipi seçilmiş; bu tip hizasız erişimlere izin vermediğinden lwIP'nin ARP kodu `HardFault` üretiyordu. **Çözüm:** `TEX_LEVEL0` → `TEX_LEVEL1`.
3. **Gizli, ayrı bellek bölgesi:** `LWIP_RAM_HEAP_POINTER`, `0x30000000` (D2 domain SRAM) adresine sabitlenmişti — bu bölge önceki düzeltmelerden bağımsız ve hâlâ önbellekliydi; CPU'nun yazdığı ARP yanıtı yalnızca önbellekte kalıyor, DMA ise fiziksel RAM'i okuyup eski/çöp veri gönderiyordu. **Çözüm:** bu bölge için üçüncü bir MPU alanı tanımlanıp önbelleksiz yapılmıştır.

Doğrulama: PC (192.168.1.10) — Kart (192.168.1.20) arası ping, ortalama 5ms, %0 paket kaybı.

## TCP Tabanlı İlk Aktarım Mimarisi

**Protokol:** ham TCP soketi + her JPEG karesinin önüne eklenen 4 byte'lık uzunluk bilgisi, port **5001**.

**Bağlantı sırası:**

```
Kart: MX_LWIP_Init() → Network_Stream_Init() port 5001 dinlemede
PC:   webcam açılır → connect() → TCP 3-way handshake
Kart: Network_TcpAcceptCallback() → tcp_recv() kaydı
PC:   her kare sock.sendall() ile gönderilir
Kart: Network_TcpRecvCallback() → 4B uzunluk + JPEG verisi ayrıştırılır
```

Port seçimi: 0-1023 well-known portlar (HTTP/FTP/SSH/Telnet) gereksiz karışıklık yaratır; 1024-49151 registered aralığından 5001 seçilmiştir (5000, bazı sistemlerde AirPlay/Control Center ile çakışabilir).

**Bu aşamada RTP kullanılmama gerekçesi:** paket kaybı zaten TCP ile önleniyor; geriden kalma `network_stream.c` içindeki `DropCurrentFrame` mantığıyla (tamponlar doluyken yeni kare atlanır) çözülüyor; tek akışlı sistemde RTP'nin zaman senkronizasyonu özelliğine gerek yok; RTSP/jitter buffer gibi ek karmaşıklık gereksiz görülmüştür.

## RTP/UDP Protokolüne Geçiş

Sonraki aşamada standart **RTP (RFC 3550)** + **RFC 2435** ("RTP Payload Format for JPEG-compressed Video") formatına geçilmiştir — ffmpeg/gstreamer/IP kameraların, herhangi bir standart RTP oynatıcının (VLC gibi) da anlayabileceği endüstri standardı.

**Taşıma katmanı:** TCP → UDP (gecikmiş kareyi beklemek yerine atlayıp devam etme mantığıyla uyumlu).

**Paket formatı:**

| Katman | Boyut | İçerik |
|---|---|---|
| RTP header (RFC 3550) | 12 byte | V/P/X/CC (sabit 0x80), M+PT (PT=26, M=son parçada 1), sequence number, timestamp (90 kHz), SSRC |
| JPEG header (RFC 2435) | 8 byte | type-specific, fragment offset (24-bit), type (0=4:2:2, 1=4:2:0), Q (1-99), width/8, height/8 |

Bir kare (15-40 KB) tek pakete sığmadığından parçalanır (**fragmentation**, ~1400 byte/paket) ve kart tarafında sıra numarasına göre yeniden birleştirilir (**reassembly**). Kayıp paket olursa o kare tamamen düşürülür (retransmit yok) — UDP/RTP'nin doğal, beklenen davranışı.

**Tablosuz aktarım:** Quantization/Huffman tabloları hiç gönderilmez; yalnızca 1-99 arası bir kalite değeri (**Q=80**) gönderilir, her iki taraf (PC: libjpeg, kart: `JFIF_BuildHeader()`) aynı standart formülle kendi tablolarını hesaplar.

**Karşılaşılan yan sorunlar:** CubeIDE'nin ayrı dosyadaki (`jpeg_std_tables.c`) tabloları derlemeye dahil etmemesi (tüm kod `network_stream.c`'de birleştirilerek çözülmüştür); iki proje klasörünün karıştırılması nedeniyle ilk RTP denemesinin yanlış klasörde test edilmesi; `LCD_BriefDisplay()`'in font ayarlamadan yazı yazması nedeniyle her açılışta oluşan `NULL` font çökmesi (ağ yığınının hiç başlamamasına yol açıyordu).

**Asıl mühendislik sorunu:** RTP paketleri doğru gönderiliyordu (Wireshark ile doğrulandı) ancak kart hiçbir kareyi tamamlayamıyordu (`FrameCorrupt` sürekli 1). Sebep: UDP'de TCP'deki gibi akış kontrolü yok — PC bir kare için 20-60 paketi art arda, durmadan gönderiyor, bu paketler 1-3ms içinde karta ulaşıyor, kart ise alım durumunu yalnızca 1ms'lik SysTick aralığıyla kontrol ediyordu. `ETH_RX_DESC_CNT=4` ve `ETH_RX_BUFFER_CNT=12` bu patlamayı karşılayamıyor, paketler donanım seviyesinde kayboluyordu.

**Çözüm:** `ETH_RX_DESC_CNT: 4 → 32`, `ETH_RX_BUFFER_CNT: 12 → 48` (ek bellek ~80 KB, 512 KB'lık `RAM_D1` içinde sorun yaratmıyor).

## Alım Tamponu Boyutlandırması

- **`ETH_RX_DESC_CNT`** — Ethernet DMA'nın kullandığı donanım tanımlayıcı (descriptor) sayısı; her tanımlayıcı gelen bir paketin yazılacağı konumu gösterir. Doluysa yeni paketler donanım tarafından, yazılımın haberi olmadan atılır.
- **`ETH_RX_BUFFER_CNT`** — her tanımlayıcının işaret ettiği gerçek bellek havuzunun (1536 byte'lık birimler) büyüklüğü. Bir paket donanımdan yazılıma geçtikten sonra işlenip serbest bırakılana kadar bir birimi meşgul eder.
- **Buffer sayısı > descriptor sayısı** olmalıdır çünkü bir paket, donanım tanımlayıcısından çıkıp yazılıma geçtikten sonra da (tam olarak işlenip serbest bırakılana kadar) buffer'ı işgal etmeye devam edebilir.

## PC Tarafı Script'leri

`PC_Sender/` klasöründe, aynı RTP/JPEG gönderim mantığını paylaşan iki bağımsız Python script'i bulunur — hangisi terminalde çalıştırılırsa o çalışır, birbirlerinden bağımsızlardır:

| Script | Kaynak | Not |
|---|---|---|
| `send_webcam_stream.py` | PC'nin webcam'i | Yatay ayna çevirme (`cv2.flip`, selfie-önizleme gibi görünsün diye), kırp-doldur (letterbox yok, ekran hep tam dolu), pencereyi X'e basarak kapatmanın da düzgün algılanması |
| `send_video_file_stream.py` | Yerel video dosyası (`video0_800_480.avi`) | Dosyanın kendi FPS'ine göre düzenli aralıklarla gönderim yapar (disk okuma anlık olduğundan bu olmasa dosya son sürat gönderilirdi); video bitince başa sarıp döngüye girer |

**Ortak mantık:**

- `find_scan_data()` — JPEG baytlarında marker segmentlerini teker teker (her segmentin kendi uzunluk alanını kullanarak) atlayarak SOS/EOI arasındaki gerçek entropi verisini çıkarır. Ham `0xFFDA` baytı araması kullanılmaz — başlık segmentlerinin içeriğinde bu baytlar tesadüfen geçebilir.
- `compute_crop()` — kaynağın hangi boyutta gelirse gelsin 800:480 en-boy oranına merkezi kırpma yapıp tam boyuta ölçekler; gerinme veya siyah bant oluşmaz.
- `JPEG_QUALITY = 80` — hem `cv2.imencode`'a hem RTP/JPEG başlığındaki Q baytına aynı değer olarak veriliyor; ikisinin senkron kalması kritik.

## Köşegen Görüntü Yırtılması (Diagonal Tearing)

Triple buffering'e geçtikten sonra, canlı akış sırasında ekranın sol üst köşesinden sağ alt köşesine sabit bir köşegen çizgi boyunca yırtılma oluşuyor — çizginin bir tarafı eski karenin, diğer tarafı yeni karenin içeriğini gösteriyor. Akış durunca (sabit karede) tamamen kayboluyor; akış sırasında görüntü hâlâ güncelleniyor, pipeline kilitlenmiş değil.

Bugüne kadar denenen **10 yöntem**:

1. **VBLANK zamanlı LTDC reload — kodda aktif.** `BSP_LCD_SetLayerAddress()` varsayılan olarak anlık (immediate) reload kullanır; `BSP_LCD_Relaod(0, BSP_LCD_RELOAD_NONE)` + kopyalama bitince açıkça `HAL_LTDC_Reload(..., LTDC_RELOAD_VERTICAL_BLANKING)` ile bir sonraki VSYNC'e ertelendi. Register seviyesinde doğru bağlandığı doğrulandı. **Yırtılma değişmedi.**
2. **`PIPE_WAITING_RELOAD` — onaylı geçiş, kodda aktif.** Bir sonraki kare, önceki geçişin donanımdan onayı (`HAL_LTDC_ReloadEventCallback`) gelmeden başlamıyor. 3 tamponlu döngü matematiksel olarak kanıtlanabilir şekilde güvenli hâle getirildi. **Yırtılma değişmedi** (video akmaya devam ediyor, donunca tertemiz — pipeline kilitlenmiyor, sadece bu mantık sorunu çözmüyor).
3. **Row/Column (MADCTL) teorisi — elendi.** OTM8009A fiziksel olarak dikey (480×800); landscape modda çalışması için MADCTL ile satır/sütun takası yapılır. `BSP_LCD_Init`, `MX_DSIHOST_DSI_Init` ve `MX_LTDC_Init` çağrılarının tümüne tutarlı biçimde 800×480 verildiği doğrulanmış, panel içi takasın sabit karede hatasız çalıştığı görülmüştür. Şekli açıklayabilir ama kaynağı değil.
4. **LTDC FIFO Underrun / Transfer Error sayaçları — sonuçsuz.** DMA2D/JPEG/Ethernet DMA'sı aynı SDRAM'e erişiyor; bant genişliği rekabetinin LTDC FIFO'sunu boşaltıp (underrun) görsel bozulmaya yol açabileceği teorisi test edilecekti (`HAL_LTDC_ErrorCallback()` override edildi). Hiç test edilmeden geri alındı — sonuç hiçbir zaman elde edilemedi.
5. **DMA2D önceliği + eksik `DSI_IRQHandler` düzeltmeleri — gerçek ama ilgisiz hatalar.** Kod incelemesinde `DMA2D_IRQn` önceliğinin `BSP_LCD_Init()` tarafından sessizce ezildiği ve `DSI_IRQn`'in hiç handler'ı olmadığı bulundu; düzeltildi ama test edilmeden geri alındı (`DSI_IRQn`'in `ErrorMsk` register'ı hiç konfigüre edilmediği için bu yol zaten "ölü").
6. **Memcpy/CPU dönüşüm denemesi — sonuçsuz.** DMA2D'nin donanımsal YCbCr→ARGB dönüşümü CPU döngüsüyle değiştirildi (düz `memcpy` mümkün değildi, format dönüşümü gerekiyordu); ~384.000 piksel/kare CPU yükü sistemi kullanılamaz hâle getirdi ve geri alındı.
7. **OTM8009A datasheet derinlemesine inceleme — mimari doğrulandı, kaldıraç bulunamadı.** Panelin gerçek bir dahili GRAM'i (480×864×24bit = 1.244.160 bayt) ve bağımsız bir zamanlama üreteci + osilatörü olduğu doğrulandı. LTDC veriyi ne kadar senkronize gönderse de panelin GRAM'den ekrana basma zamanlaması kısmen kendi dahili osilatörüne bağlı olabilir — gözlemlerle (durunca düzelme, LTDC düzeltmelerinin etkisiz kalması, sabit faz farkı) tutarlı, güncel en güçlü hipotez.
8. **Register denemesi (`C1A1h` RGB_VIDEO_SET: 0x08 → 0x0F) — sonuçsuz.** Datasheet'in önerdiği tam senkron değeri denendi, değişiklik gözlenmedi; bu register'ın yalnızca RGB paralel arayüzüne özgü olabileceği, kullanılan DSI modunu etkilemediği değerlendirilerek geri alındı.
9. **Çift tampona (2) geri dönüş sağlaması — önemli negatif sonuç.** Yırtılmanın gerçekten triple buffering ile mi başladığını doğrulamak için `NB_IMAGES` 2'ye çekildi. **Yırtılma devam etti** — sorun triple buffering'in kendisinden kaynaklanmıyor.
10. **Düzenli (paced) vs. patlamalı (bursty) gönderim karşılaştırması — sonuçsuz.** Webcam gönderimi de video dosyası gibi düzenli 20fps'e sabitlendi. **Yırtılma değişmedi** — sorun gönderim düzensizliğinden kaynaklanmıyor.

**Durum:** kaynağın panelin iç zamanlama davranışı olduğu değerlendirilmekte (7 numaralı bulgu), ama bunu düzeltmeye yönelik somut deneme (8 numara) de işe yaramadı; kesin çözüm henüz uygulanmamıştır. Sonraki adaylar: FIFO underrun sayaçlarının gerçekten test edilmesi, DSI'ye özel bir senkronizasyon register'ının aranması (RM0399 DSI Host bölümü), `DSI_TE` pininin veya panelin gerçek VSYNC zamanlamasının osiloskop/lojik analizörle izlenmesi.

## Bilinen Kısıtlar

- `main.c` içindeki `xPos`/`yPos` hesaplamaları `uint32_t` (işaretsiz) türündedir; görüntü boyutu ekran boyutundan büyükse çıkarma işlemi negatif sonuç üretemediğinden **underflow** oluşur (800×480'de sorun gözlenmemiştir).
- Köşegen görüntü yırtılması (bkz. yukarı) çözülmemiştir.
- Donanım yalnızca baseline/YCbCr/24 bit JPEG'i, 800×480'i aşmayan çözünürlükte destekler.

## Dosya Haritası

| Dosya | Rol |
|---|---|
| `CM7/Src/main.c` | Pipeline durum makinesi, DMA2D/LTDC yönetimi, MPU konfigürasyonu |
| `CM7/Inc/main.h` | Buffer adresleri, `NB_IMAGES` |
| `CM7/Src/network_stream.c` | UDP/RTP alıcısı, RFC 2435 JFIF başlık yeniden inşası |
| `CM7/Src/decode_dma.c` | JPEG donanım çözücü callback'leri |
| `CM7/Src/stm32h7xx_it.c` | Kesme handler'ları (JPEG, DMA2D, LTDC, MDMA, SDMMC) |
| `CM7/Inc/stm32h7xx_hal_conf.h` | `ETH_RX_DESC_CNT` vb. HAL konfigürasyonu |
| `CM7/LWIP/Target/ethernetif.c` | `ETH_RX_BUFFER_CNT`, Ethernet arayüz katmanı |
| `Drivers/BSP/STM32H747I-EVAL/stm32h747i_eval_lcd.c` | LCD/LTDC/DSI BSP katmanı |
| `Drivers/BSP/Components/otm8009a/otm8009a.c` | Panel başlatma dizisi, MADCTL/register yazımları |
| `PC_Sender/send_webcam_stream.py` | Webcam → RTP/JPEG gönderici |
| `PC_Sender/send_video_file_stream.py` | Video dosyası → RTP/JPEG gönderici |

## Referanslar

- RFC 3550 — RTP: A Transport Protocol for Real-Time Applications
- RFC 2435 — RTP Payload Format for JPEG-compressed Video
- lwIP (Lightweight IP) — https://savannah.nongnu.org/projects/lwip/
- OTM8009A Datasheet (DSI panel controller, Orise Technology)
- STM32H757XI Reference Manual (**RM0399** — dual-core H745/H755/H747/H757 ailesi; JPEG kodek, DMA2D, LTDC, MPU, Ethernet DMA bölümleri)
