# 📋 Linux Syslog Kayıtlarının Çift Yönlü Bağlı Liste ile Gösterilmesi

Linux işletim sistemindeki syslog kayıtlarını dosyadan okuyarak **çift yönlü bağlı liste (doubly linked list)** veri yapısında saklayan ve üzerinde çeşitli analiz işlemleri gerçekleştiren bir C uygulamasıdır.

## 📌 Proje Hakkında

Bu proje, Kırklareli Üniversitesi Yazılım Mühendisliği bölümü **Veri Yapıları** dersi kapsamında hazırlanmıştır.

Program, `syslog.txt` dosyasındaki örnek Linux syslog kayıtlarını satır satır okur, her satırı ayrıştırarak (tarih, sunucu, süreç, mesaj) yapılandırılmış bir biçime dönüştürür ve çift yönlü bağlı listeye yükler. Kullanıcı, interaktif bir menü aracılığıyla kayıtlar üzerinde ileri/geri gezinme, filtreleme ve istatistik görüntüleme işlemleri gerçekleştirebilir.

## 🧠 Teorik Arka Plan

### Syslog Nedir?

Syslog, Linux sistemlerde çekirdek mesajları, servis hataları, kullanıcı girişleri ve ağ olayları gibi sistem olaylarını merkezi ve standart bir biçimde kayıt altına alan günlük mekanizmasıdır. Kayıtlar `/var/log/syslog` dosyasında kronolojik sırayla tutulur ve her satır şu formattadır:

```
Mar 13 10:05:33 ubuntu sshd[5023]: Failed password for admin from 192.168.1.50
│               │      │           └─ Mesaj
│               │      └─ Süreç adı ve PID
│               └─ Sunucu adı (hostname)
└─ Zaman damgası (timestamp)
```

### Neden Çift Yönlü Bağlı Liste?

| Kriter | Dizi | Tek Yönlü Liste | Çift Yönlü Liste |
|--------|------|-----------------|-------------------|
| Dinamik boyut | ✗ (sabit veya realloc) | ✓ | ✓ |
| İleri gezinme | ✓ | ✓ | ✓ |
| Geri gezinme | ✓ (indeks ile) | ✗ | ✓ (`onceki` ile) |
| Son elemana erişim | ✓ O(1) | ✗ O(n) | ✓ O(1) |
| Ortadan silme | ✗ O(n) kaydırma | ✗ O(n) arama | ✓ O(1) |
| Bellek verimliliği | ✓ (en az) | ✓ | △ (fazladan 1 işaretçi) |

Syslog kayıtları kronolojik sıralıdır. Sistem yöneticileri hem eskiden yeniye olayları takip eder hem de en son hatalardan geriye doğru sorun araştırır. Çift yönlü bağlı liste bu iki yönlü gezinmeyi doğal olarak destekler. Ayrıca `son` işaretçisi sayesinde en güncel kayıtlara O(1) ile erişilir.

## 🏗️ Veri Yapısı Tasarımı

```
BagliListe
┌─────────────────────────────────────────────┐
│ bas ──► [Dugum A] ◄──► [Dugum B] ◄──► [Dugum C] ◄── son │
│ boyut: 3                                              │
└─────────────────────────────────────────────┘

Her Dugum:
┌──────────────────────┐
│ SyslogKaydi kayit    │  ← tarih_saat, sunucu_adi, surec_adi, mesaj
│ Dugum *onceki        │  ← bir önceki düğümün adresi
│ Dugum *sonraki       │  ← bir sonraki düğümün adresi
└──────────────────────┘
```

## ⚙️ Özellikler

- **İleri gezinme** — Kayıtları eskiden yeniye kronolojik sırada listeleme
- **Geri gezinme** — Kayıtları yeniden eskiye ters sırada listeleme
- **Son N kayıt** — `son` işaretçisinden geriye giderek en güncel kayıtları gösterme
- **Anahtar kelime filtresi** — Mesaj, süreç ve sunucu alanlarında metin araması
- **Süreç filtresi** — Belirli bir servise (sshd, kernel, CRON vb.) ait kayıtları izole etme
- **İstatistik özeti** — Tek geçişte toplam kayıt, hata sayısı, süreç dağılımı hesaplama
- **Bellek yönetimi** — Çıkışta tüm düğümlerin `free()` ile serbest bırakılması

## 📁 Dosya Yapısı

```
syslog-bagli-liste/
├── syslog_linked_list.c   # Ana program kaynak kodu
├── syslog.txt             # Örnek syslog verileri (25 kayıt)
└── README.md              # Bu dosya
```

## 🚀 Derleme ve Çalıştırma

### Gereksinimler

- GCC derleyici (MinGW, GCC veya benzeri)
- C99 veya üzeri standart

### Windows (MinGW)

```bash
gcc -o syslog_ll syslog_linked_list.c
syslog_ll.exe
```

### Linux / macOS

```bash
gcc -o syslog_ll syslog_linked_list.c
./syslog_ll
```

> **Not:** `syslog.txt` dosyası çalıştırılabilir dosya ile aynı dizinde olmalıdır.

## 📸 Örnek Çıktılar

### Program Başlangıcı
```
--------------------------------------------------------------
  SYSLOG DOSYASI YUKLENDI
  Dosya           : syslog.txt
  Okunan satir    : 25
  Basarili kayit  : 25
--------------------------------------------------------------
```

### İstatistik Görünümü
```
--------------------------------------------------------------
  SYSLOG ISTATISTIKLERI
--------------------------------------------------------------
  Toplam kayit sayisi       : 25
  Basarisiz giris denemesi  : 4
  Kernel mesaji             : 4
  CRON gorevi               : 4
  SSH baglantisi            : 7
  Ilk kayit zamani          : Mar 13 08:01:12
  Son kayit zamani          : Mar 13 09:50:33
--------------------------------------------------------------
```

### Anahtar Kelime Filtresi ("Failed")
```
--------------------------------------------------------------
  ANAHTAR KELIME FILTRESI: "Failed"
--------------------------------------------------------------
  Mar 13 08:22:10 | ubuntu   | sshd[3210]           | Failed password for root from 203.0.113.15 port 55123
  Mar 13 08:22:13 | ubuntu   | sshd[3210]           | Failed password for root from 203.0.113.15 port 55123
  Mar 13 08:22:17 | ubuntu   | sshd[3210]           | Failed password for root from 203.0.113.15 port 55123
  Mar 13 09:30:45 | ubuntu   | sshd[3700]           | Failed password for invalid user test from 198.51.100.23 port 22
  Bulunan kayit sayisi: 4
--------------------------------------------------------------
```

## 🔧 Fonksiyon Açıklamaları

| Fonksiyon | Açıklama | Karmaşıklık |
|-----------|----------|-------------|
| `liste_olustur()` | Boş bir çift yönlü bağlı liste oluşturur | O(1) |
| `dugum_olustur()` | Yeni bir düğüm oluşturup veriyi kopyalar | O(1) |
| `liste_sona_ekle()` | Kayıdı listenin sonuna ekler (kronolojik sıra korunur) | O(1) |
| `satir_ayristir()` | Syslog satırını `sscanf` ile parçalara ayırır | O(1) |
| `liste_ileri_yazdir()` | Listeyi baştan sona (eskiden yeniye) yazdırır | O(n) |
| `liste_geri_yazdir()` | Listeyi sondan başa (yeniden eskiye) yazdırır | O(n) |
| `liste_son_n_kayit()` | Son N kaydı `son` işaretçisinden geriye giderek gösterir | O(N) |
| `liste_filtrele()` | Anahtar kelime ile tüm alanlarda arama yapar | O(n) |
| `liste_surece_gore_filtrele()` | Belirli bir sürecin kayıtlarını listeler | O(n) |
| `liste_istatistik()` | Tek geçişte istatistik özeti çıkarır | O(n) |
| `liste_serbest_birak()` | Tüm düğümleri ve listeyi bellekten siler | O(n) |


- **Ad Soyad:** Samet Muhammed Ali ŞAFAK
- **Üniversite:** Kırklareli Üniversitesi
- **Bölüm:** Yazılım Mühendisliği 
- **Ders:** Veri Yapıları
