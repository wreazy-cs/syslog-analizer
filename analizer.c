/*
 * =============================================================================
 *  Linux Syslog Kayitlarinin Cift Yonlu Bagli Liste ile Gosterilmesi
 * =============================================================================
 *
 *  Aciklama:
 *    Bu program ayni dizindeki "syslog.txt" dosyasini okur. Dosya icindeki
 *    her satir gercek bir Linux syslog kaydini temsil eder. Program bu
 *    satirlari ayristirarak (tarih, sunucu, surec, mesaj) bir CIFT YONLU
 *    BAGLI LISTE yapisina yukler ve cesitli islemler gerceklestirir.
 *
 *  Neden Cift Yonlu Bagli Liste?
 *    - Syslog kayitlari kronolojik sirayla tutulur; hem eskiden yeniye
 *      hem yeniden eskiye gezinmek gerekebilir (ornegin son N kayit).
 *    - Belirli bir dugumu silmek O(1) ile yapilabilir cunku onceki
 *      dugume dogrudan erisim vardir.
 *    - Tarih araligina gore filtreleme yapilirken iki yone de hareket
 *      edebilmek buyuk kolaylik saglar.
 *    - Tek yonlu listede son elemana erismek O(n) iken, cift yonlu
 *      listede "son" isaretcisi sayesinde O(1) ile erisilir.
 *
 *  Derleme (Windows - MinGW) : gcc -o syslog_ll syslog_linked_list.c
 *  Derleme (Linux - GCC)     : gcc -o syslog_ll syslog_linked_list.c
 *  Calistirma                : ./syslog_ll
 *
 *  Not: "syslog.txt" dosyasi programla ayni klasorde bulunmalidir.
 *
 *  Yazar  : Samet
 *  Tarih  : 2026
 * =============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========================== SABITLER ========================== */

#define MAKS_SATIR_UZUNLUGU 1024  /* Bir syslog satirinin okuma tamponu   */
#define DOSYA_ADI "syslog.txt"    /* Okunacak ornek syslog dosyasi        */

/* =============================================================
 *  YAPI TANIMLARI (STRUCT DEFINITIONS)
 * =============================================================*/

/*
 * SyslogKaydi: Tek bir syslog satirinin ayristirilmis halini tutar.
 *
 *   tarih_saat : Logun olusturuldugu zaman  -> "Mar 13 10:05:33"
 *   sunucu_adi : Logu ureten makine adi     -> "ubuntu"
 *   surec_adi  : Surec ve PID bilgisi       -> "sshd[5023]"
 *   mesaj      : Log mesajinin kendisi      -> "Failed password for..."
 *
 * Bu struct, ham metin satirini yapilandirilmis veriye donusturur.
 * Boylece her alana ayri ayri erisilebilir ve islem yapilabilir.
 */
typedef struct SyslogKaydi {
    char tarih_saat[20];
    char sunucu_adi[64];
    char surec_adi[128];
    char mesaj[512];
} SyslogKaydi;

/*
 * Dugum: Cift yonlu bagli listenin temel yapitasidir.
 *
 *   kayit   : Bu dugumun icinde sakladigi syslog verisi
 *   onceki  : Bir ONCEKI dugumun bellek adresi (geri yonde gezinme)
 *   sonraki : Bir SONRAKi dugumun bellek adresi (ileri yonde gezinme)
 *
 * Tek yonlu listeden farki: "onceki" isaretcisinin bulunmasidir.
 * Bu sayede herhangi bir dugumden hem ileri hem geri gidilebilir.
 *
 *   NULL <- [Dugum A] <-> [Dugum B] <-> [Dugum C] -> NULL
 *            onceki=NULL                  sonraki=NULL
 */
typedef struct Dugum {
    SyslogKaydi    kayit;
    struct Dugum  *onceki;
    struct Dugum  *sonraki;
} Dugum;

/*
 * BagliListe: Tum listeyi yoneten ust duzey yapidir.
 *
 *   bas   : Listenin ILK dugumunun adresi (en eski kayit)
 *   son   : Listenin SON dugumunun adresi (en yeni kayit)
 *   boyut : Listede kac dugum oldugunu tutar
 *
 * "son" isaretcisi cift yonlu listenin en buyuk avantajlarindan
 * biridir: son kayda O(1) ile erisim saglar. Tek yonlu listede
 * son elemana ulasmak icin bastan sonuna kadar O(n) yurumek gerekir.
 */
typedef struct BagliListe {
    Dugum *bas;
    Dugum *son;
    int    boyut;
} BagliListe;

/* ===================== FONKSIYON PROTOTIPLERI ===================== */

BagliListe *liste_olustur(void);
Dugum      *dugum_olustur(SyslogKaydi kayit);
void        liste_sona_ekle(BagliListe *liste, SyslogKaydi kayit);
void        liste_ileri_yazdir(const BagliListe *liste);
void        liste_geri_yazdir(const BagliListe *liste);
void        liste_son_n_kayit(const BagliListe *liste, int n);
void        liste_filtrele(const BagliListe *liste, const char *anahtar);
void        liste_surece_gore_filtrele(const BagliListe *liste, const char *surec);
void        liste_istatistik(const BagliListe *liste);
void        liste_serbest_birak(BagliListe *liste);
int         satir_ayristir(const char *satir, SyslogKaydi *kayit);
void        ayirici_cizgi_yazdir(void);
void        menu_goster(void);

/* =============================================================
 *  LISTE YONETIM FONKSIYONLARI
 * =============================================================*/

/*
 * liste_olustur:
 *   Bellekte yeni ve bos bir BagliListe yaratir.
 *   Bas ve son isaretcileri NULL olarak baslar (liste bos).
 *   Boyut 0 ile baslatilir.
 *   Basarisiz bellek tahsisinde program hata mesaji ile sonlanir.
 */
BagliListe *liste_olustur(void)
{
    BagliListe *liste = (BagliListe *)malloc(sizeof(BagliListe));
    if (!liste) {
        fprintf(stderr, "[HATA] Bellek ayrilamadi (BagliListe).\n");
        exit(EXIT_FAILURE);
    }
    liste->bas   = NULL;
    liste->son   = NULL;
    liste->boyut = 0;
    return liste;
}

/*
 * dugum_olustur:
 *   Verilen SyslogKaydi'ni icerisine kopyalayarak yeni bir Dugum olusturur.
 *   Olusturulan dugumun onceki ve sonraki isaretcileri NULL'dir;
 *   listeye baglanma islemi ayri bir fonksiyonda (liste_sona_ekle) yapilir.
 */
Dugum *dugum_olustur(SyslogKaydi kayit)
{
    Dugum *yeni = (Dugum *)malloc(sizeof(Dugum));
    if (!yeni) {
        fprintf(stderr, "[HATA] Bellek ayrilamadi (Dugum).\n");
        exit(EXIT_FAILURE);
    }
    yeni->kayit   = kayit;   /* Struct icerigini kopyalar */
    yeni->onceki  = NULL;
    yeni->sonraki = NULL;
    return yeni;
}

/*
 * liste_sona_ekle:
 *   Yeni bir kaydi listenin SONUNA ekler.
 *
 *   Neden sona ekliyoruz?
 *   Syslog dosyasi kronolojik sirayla yazilir (eski satirlar uste,
 *   yeni satirlar alta). Dosyayi bastan sona okuyup her kaydi sona
 *   eklersek, listedeki sira dosyadaki zaman sirasiyla birebir eslesir.
 *
 *   Islem adimlari:
 *     1. Yeni dugum olustur
 *     2. Liste bos mu kontrol et
 *        - Bossa: yeni dugum hem bas hem son olur
 *        - Doluysa: mevcut son dugumun sonraki'sine bagla,
 *                   yeni dugumun onceki'sine eski son'u ata,
 *                   son isaretcisini guncelle
 *     3. Boyutu bir artir
 *
 *   Karmasiklik: O(1) — son isaretcisi sayesinde arama gerekmez
 */
void liste_sona_ekle(BagliListe *liste, SyslogKaydi kayit)
{
    Dugum *yeni = dugum_olustur(kayit);

    if (liste->son == NULL) {
        /* Liste bos: ilk eleman hem bas hem son */
        liste->bas = yeni;
        liste->son = yeni;
    } else {
        /* Mevcut son dugumun sagina bagla */
        liste->son->sonraki = yeni;       /* Eski son -> yeni     */
        yeni->onceki        = liste->son; /* Yeni <- eski son     */
        liste->son          = yeni;       /* Liste sonu guncelle  */
    }
    liste->boyut++;
}

/* =============================================================
 *  SYSLOG SATIR AYRISTIRMA (PARSING)
 * =============================================================*/

/*
 * satir_ayristir:
 *   Bir syslog satirini parcalayarak SyslogKaydi struct'ina yazar.
 *
 *   Tipik syslog formati:
 *     "Mar 13 10:05:33 ubuntu sshd[5023]: Failed password for admin"
 *      ^^^^^^^^^^^^^^^^^^^  ^^^^^^ ^^^^^^^^^^^  ^^^^^^^^^^^^^^^^^^^^^^^^^
 *      tarih_saat           sunucu surec_adi    mesaj
 *
 *   Ayristirma adimlari:
 *     1. sscanf ile ilk 5 alani oku (ay, gun, saat, sunucu, surec)
 *     2. Ay+gun+saat'i birlestirerek tarih_saat alanina yaz
 *     3. ": " isaretini bularak mesaj kismini ayikla
 *     4. Satir sonu karakterini temizle
 *
 *   Donus degeri: 1 = basarili ayristirma, 0 = hatali/eksik satir
 */
int satir_ayristir(const char *satir, SyslogKaydi *kayit)
{
    char ay[4];       /* "Mar", "Jan", "Feb" gibi 3 harfli ay kisaltmasi */
    char saat[16];    /* "10:05:33" saat:dakika:saniye                    */
    int  gun;         /* Ayin kacinci gunu                                */

    /* sscanf ile temel alanlari oku.
     * %3s   -> 3 karakterlik ay kisaltmasi
     * %d    -> gun sayisi
     * %15s  -> saat bilgisi
     * %63s  -> sunucu adi (maks 63 karakter)
     * %127[^:] -> ':' karakterine kadar olan surec adi
     */
    int okunan = sscanf(satir, "%3s %d %15s %63s %127[^:]",
                        ay, &gun, saat, kayit->sunucu_adi, kayit->surec_adi);

    if (okunan < 5)
        return 0;  /* Yeterli alan okunamadi, satir formati uygun degil */

    /* Tarih ve saat bilgisini tek bir alanda birlestir */
    snprintf(kayit->tarih_saat, sizeof(kayit->tarih_saat),
             "%s %2d %s", ay, gun, saat);

    /* Mesaj kismini bul: satirda ilk ": " gectikten sonraki kisim */
    const char *mesaj_baslangic = strstr(satir, ": ");
    if (mesaj_baslangic) {
        mesaj_baslangic += 2;  /* ": " ifadesini atla */
        strncpy(kayit->mesaj, mesaj_baslangic, sizeof(kayit->mesaj) - 1);
        kayit->mesaj[sizeof(kayit->mesaj) - 1] = '\0';
    } else {
        strcpy(kayit->mesaj, "(mesaj yok)");
    }

    /* Satir sonu (\n) karakterini temizle */
    kayit->mesaj[strcspn(kayit->mesaj, "\n")] = '\0';

    return 1;  /* Ayristirma basarili */
}

/* =============================================================
 *  LISTELEME VE GORUNTULEME FONKSIYONLARI
 * =============================================================*/

/*
 * ayirici_cizgi_yazdir:
 *   Ekran ciktisinda bolumleri birbirinden ayirmak icin
 *   gorsel bir cizgi basar. Okunabilirligi artirir.
 */
void ayirici_cizgi_yazdir(void)
{
    printf("--------------------------------------------------------------\n");
}

/*
 * liste_ileri_yazdir:
 *   Listeyi BASTAN SONA (eskiden yeniye) yazdirir.
 *
 *   Gezinme yonu: bas -> sonraki -> sonraki -> ... -> NULL
 *
 *   Kullanim amaci: Olaylarin kronolojik gelisimini takip etmek.
 *   Ornegin bir servisin baslatilma, calisma ve hata verme
 *   surecini zaman sirasina gore gormek icin kullanilir.
 */
void liste_ileri_yazdir(const BagliListe *liste)
{
    printf("\n");
    ayirici_cizgi_yazdir();
    printf("  SYSLOG KAYITLARI (Eskiden Yeniye - Ileri Gezinme)\n");
    printf("  Toplam kayit sayisi: %d\n", liste->boyut);
    ayirici_cizgi_yazdir();

    int sira = 1;
    const Dugum *gecici = liste->bas;  /* Bastan basla */

    while (gecici != NULL) {
        printf("  [%2d] %s | %-8s | %-20s | %s\n",
               sira++,
               gecici->kayit.tarih_saat,
               gecici->kayit.sunucu_adi,
               gecici->kayit.surec_adi,
               gecici->kayit.mesaj);
        gecici = gecici->sonraki;  /* Bir sonraki dugume ilerle */
    }

    ayirici_cizgi_yazdir();
}

/*
 * liste_geri_yazdir:
 *   Listeyi SONDAN BASA (yeniden eskiye) yazdirir.
 *
 *   Gezinme yonu: son -> onceki -> onceki -> ... -> NULL
 *
 *   Bu fonksiyon CIFT YONLU bagli listenin temel avantajini gosterir.
 *   Tek yonlu listede bu islemi yapmak icin ya yigin (stack) kullanmak
 *   ya da listeyi ters cevirmek gerekir ki ikisi de ekstra maliyet getirir.
 *   Cift yonlu listede ise "son" isaretcisinden baslayip "onceki" ile
 *   geri gitmek yeterlidir.
 *
 *   Kullanim amaci: En guncel olaylardan baslayarak geriye dogru
 *   sorun arastirmasi yapmak. Sistem yoneticileri genellikle
 *   "son neler oldu?" diye bakmak ister.
 */
void liste_geri_yazdir(const BagliListe *liste)
{
    printf("\n");
    ayirici_cizgi_yazdir();
    printf("  SYSLOG KAYITLARI (Yeniden Eskiye - Geri Gezinme)\n");
    printf("  Toplam kayit sayisi: %d\n", liste->boyut);
    ayirici_cizgi_yazdir();

    int sira = liste->boyut;
    const Dugum *gecici = liste->son;  /* SONDAN basla */

    while (gecici != NULL) {
        printf("  [%2d] %s | %-8s | %-20s | %s\n",
               sira--,
               gecici->kayit.tarih_saat,
               gecici->kayit.sunucu_adi,
               gecici->kayit.surec_adi,
               gecici->kayit.mesaj);
        gecici = gecici->onceki;  /* Bir ONCEKI dugume don */
    }

    ayirici_cizgi_yazdir();
}

/*
 * liste_son_n_kayit:
 *   Listenin SONUNDAN baslayarak sadece son N kaydi gosterir.
 *
 *   Bu fonksiyon cift yonlu listenin pratik faydasini gosterir:
 *   - "son" isaretcisinden baslayip "onceki" ile N adim geri gidilir
 *   - Tek yonlu listede ayni isi yapmak icin once listenin boyutunu
 *     bulmak, sonra (boyut - N). elemana kadar ileri yurunek gerekir
 *
 *   Karmasiklik: O(N) — sadece N dugum ziyaret edilir, tum liste degil
 */
void liste_son_n_kayit(const BagliListe *liste, int n)
{
    printf("\n");
    ayirici_cizgi_yazdir();
    printf("  SON %d KAYIT (En Guncel Olaylar)\n", n);
    ayirici_cizgi_yazdir();

    if (n > liste->boyut) {
        n = liste->boyut;  /* Listeden fazla istenirse tamamini goster */
    }

    /* Sondan n adim geriye git: baslangic noktasini bul */
    const Dugum *baslangic = liste->son;
    for (int i = 1; i < n && baslangic->onceki != NULL; i++) {
        baslangic = baslangic->onceki;
    }

    /* Baslangic noktasindan itibaren ileri dogru yazdir */
    const Dugum *gecici = baslangic;
    int sira = 1;
    while (gecici != NULL) {
        printf("  [%2d] %s | %-8s | %-20s | %s\n",
               sira++,
               gecici->kayit.tarih_saat,
               gecici->kayit.sunucu_adi,
               gecici->kayit.surec_adi,
               gecici->kayit.mesaj);
        gecici = gecici->sonraki;
    }

    ayirici_cizgi_yazdir();
}

/* =============================================================
 *  FILTRELEME FONKSIYONLARI
 * =============================================================*/

/*
 * liste_filtrele:
 *   Verilen anahtar kelimeyi mesaj, surec veya sunucu alanlarinda
 *   arar ve eslesen kayitlari ekrana yazdirir.
 *
 *   strstr() fonksiyonu ile alt-dize aramasi yapilir.
 *   Ornegin "error" arandiginda mesajinda "error" gecen tum
 *   kayitlar listelenir.
 *
 *   Kullanim amaci: Binlerce log arasinda belirli bir olayi
 *   veya hatayi hizlica bulmak.
 */
void liste_filtrele(const BagliListe *liste, const char *anahtar)
{
    printf("\n");
    ayirici_cizgi_yazdir();
    printf("  ANAHTAR KELIME FILTRESI: \"%s\"\n", anahtar);
    ayirici_cizgi_yazdir();

    int bulunan = 0;
    const Dugum *gecici = liste->bas;

    while (gecici != NULL) {
        /* Uc alanda da arama yap: mesaj, surec adi, sunucu adi */
        if (strstr(gecici->kayit.mesaj,       anahtar) ||
            strstr(gecici->kayit.surec_adi,   anahtar) ||
            strstr(gecici->kayit.sunucu_adi,  anahtar))
        {
            printf("  %s | %-8s | %-20s | %s\n",
                   gecici->kayit.tarih_saat,
                   gecici->kayit.sunucu_adi,
                   gecici->kayit.surec_adi,
                   gecici->kayit.mesaj);
            bulunan++;
        }
        gecici = gecici->sonraki;
    }

    printf("  Bulunan kayit sayisi: %d\n", bulunan);
    ayirici_cizgi_yazdir();
}

/*
 * liste_surece_gore_filtrele:
 *   Sadece belirtilen surec adina ait kayitlari gosterir.
 *   Ornegin "sshd" verildiginde tum SSH baglantilarini listeler.
 *
 *   Kullanim amaci: Belirli bir servisin davranisini izole ederek
 *   incelemek. SSH saldiri denemeleri, CRON gorevleri veya
 *   kernel mesajlari gibi kategorik analizler icin idealdir.
 */
void liste_surece_gore_filtrele(const BagliListe *liste, const char *surec)
{
    printf("\n");
    ayirici_cizgi_yazdir();
    printf("  SUREC FILTRESI: \"%s\"\n", surec);
    ayirici_cizgi_yazdir();

    int bulunan = 0;
    const Dugum *gecici = liste->bas;

    while (gecici != NULL) {
        if (strstr(gecici->kayit.surec_adi, surec)) {
            printf("  %s | %-20s | %s\n",
                   gecici->kayit.tarih_saat,
                   gecici->kayit.surec_adi,
                   gecici->kayit.mesaj);
            bulunan++;
        }
        gecici = gecici->sonraki;
    }

    printf("  Bulunan kayit sayisi: %d\n", bulunan);
    ayirici_cizgi_yazdir();
}

/*
 * liste_istatistik:
 *   Liste uzerinde gezerek genel bir ozet/istatistik cikarir.
 *   Toplam kayit sayisi, basarisiz giris denemesi sayisi ve
 *   surec bazli dagilimi hesaplar.
 *
 *   Bu fonksiyon bagli listenin tum elemanlari uzerinde tek bir
 *   gecisle (O(n)) birden fazla istatistik toplayabildigini gosterir.
 */
void liste_istatistik(const BagliListe *liste)
{
    printf("\n");
    ayirici_cizgi_yazdir();
    printf("  SYSLOG ISTATISTIKLERI\n");
    ayirici_cizgi_yazdir();

    int hata_sayisi    = 0;   /* "Failed" iceren kayitlar     */
    int kernel_sayisi  = 0;   /* Kernel mesajlari             */
    int cron_sayisi    = 0;   /* CRON gorev kayitlari         */
    int ssh_sayisi     = 0;   /* SSH baglanti kayitlari       */

    const Dugum *gecici = liste->bas;

    while (gecici != NULL) {
        if (strstr(gecici->kayit.mesaj, "Failed"))
            hata_sayisi++;
        if (strstr(gecici->kayit.surec_adi, "kernel"))
            kernel_sayisi++;
        if (strstr(gecici->kayit.surec_adi, "CRON"))
            cron_sayisi++;
        if (strstr(gecici->kayit.surec_adi, "sshd"))
            ssh_sayisi++;

        gecici = gecici->sonraki;
    }

    printf("  Toplam kayit sayisi       : %d\n", liste->boyut);
    printf("  Basarisiz giris denemesi  : %d\n", hata_sayisi);
    printf("  Kernel mesaji             : %d\n", kernel_sayisi);
    printf("  CRON gorevi               : %d\n", cron_sayisi);
    printf("  SSH baglantisi            : %d\n", ssh_sayisi);

    /* Ilk ve son kaydin zamanini goster — "son" isaretcisinin faydasi */
    if (liste->bas && liste->son) {
        printf("  Ilk kayit zamani          : %s\n", liste->bas->kayit.tarih_saat);
        printf("  Son kayit zamani          : %s\n", liste->son->kayit.tarih_saat);
    }

    ayirici_cizgi_yazdir();
}

/* =============================================================
 *  BELLEK YONETIMI
 * =============================================================*/

/*
 * liste_serbest_birak:
 *   Listedeki TUM dugumleri tek tek free() ile serbest birakir,
 *   ardindan BagliListe yapisinin kendisini de siler.
 *
 *   Islem: bas'tan baslayarak sonraki ile ilerle, her dugumu sil.
 *   Bu fonksiyon cagirilmadan program sonlanirsa BELLEK SIZINTISI
 *   (memory leak) olusur.
 *
 *   Karmasiklik: O(n) — her dugum bir kez ziyaret edilir
 */
void liste_serbest_birak(BagliListe *liste)
{
    Dugum *gecici = liste->bas;

    while (gecici != NULL) {
        Dugum *silinecek = gecici;       /* Silinecek dugumu sakla */
        gecici = gecici->sonraki;        /* Bir sonrakine ilerle   */
        free(silinecek);                 /* Eski dugumu sil        */
    }

    free(liste);  /* Son olarak liste yapisinin kendisini sil */
    printf("\n[BILGI] Tum bellek basariyla serbest birakildi.\n");
}

/* =============================================================
 *  INTERAKTIF MENU
 * =============================================================*/

/*
 * menu_goster:
 *   Kullaniciya secenekleri gosteren basit bir konsol menusu.
 */
void menu_goster(void)
{
    printf("\n");
    printf("  ============ SYSLOG ANALIZ MENUSU ============\n");
    printf("  1 - Tum kayitlari goster (eskiden yeniye)\n");
    printf("  2 - Tum kayitlari goster (yeniden eskiye)\n");
    printf("  3 - Son N kaydi goster\n");
    printf("  4 - Anahtar kelime ile filtrele\n");
    printf("  5 - Surec adina gore filtrele\n");
    printf("  6 - Istatistikleri goster\n");
    printf("  0 - Cikis\n");
    printf("  =============================================\n");
    printf("  Seciminiz: ");
}

/* =============================================================
 *  ANA FONKSIYON (MAIN)
 * =============================================================*/

/*
 * main:
 *   Programin giris noktasidir. Su adimlari sirayla gerceklestirir:
 *
 *     1. syslog.txt dosyasini acar
 *     2. Her satiri okur ve satir_ayristir ile parcalar
 *     3. Basarili ayristirilan kayitlari bagli listeye ekler
 *     4. Interaktif menu ile kullanicidan islem secimi alir
 *     5. Cikista tum bellegi serbest birakir
 */
int main(void)
{
    /* ---- Adim 1: Dosyayi ac ---- */
    FILE *dosya = fopen(DOSYA_ADI, "r");
    if (!dosya) {
        fprintf(stderr, "[HATA] '%s' dosyasi acilamadi!\n", DOSYA_ADI);
        fprintf(stderr, "       Dosyanin programla ayni klasorde oldugunu kontrol edin.\n");
        return EXIT_FAILURE;
    }

    /* ---- Adim 2: Bos listeyi olustur ---- */
    BagliListe *syslog_listesi = liste_olustur();

    /* ---- Adim 3: Dosyayi satir satir oku ve listeye yukle ---- */
    char satir[MAKS_SATIR_UZUNLUGU];
    int  okunan_satir  = 0;  /* Dosyadan okunan toplam satir */
    int  eklenen_satir = 0;  /* Basariyla ayristirilan satir */

    while (fgets(satir, sizeof(satir), dosya) != NULL) {
        okunan_satir++;

        SyslogKaydi kayit;
        memset(&kayit, 0, sizeof(kayit));  /* Struct'i sifirla */

        if (satir_ayristir(satir, &kayit)) {
            liste_sona_ekle(syslog_listesi, kayit);
            eklenen_satir++;
        }
    }
    fclose(dosya);

    /* Yukleme ozeti */
    printf("\n");
    ayirici_cizgi_yazdir();
    printf("  SYSLOG DOSYASI YUKLENDI\n");
    printf("  Dosya           : %s\n", DOSYA_ADI);
    printf("  Okunan satir    : %d\n", okunan_satir);
    printf("  Basarili kayit  : %d\n", eklenen_satir);
    ayirici_cizgi_yazdir();

    /* ---- Adim 4: Interaktif menu dongusu ---- */
    int secim;
    int n;
    char arama[128];

    do {
        menu_goster();
        scanf("%d", &secim);

        switch (secim) {
            case 1:
                liste_ileri_yazdir(syslog_listesi);
                break;

            case 2:
                liste_geri_yazdir(syslog_listesi);
                break;

            case 3:
                printf("  Kac kayit gosterilsin? ");
                scanf("%d", &n);
                liste_son_n_kayit(syslog_listesi, n);
                break;

            case 4:
                printf("  Aranacak kelime: ");
                scanf("%s", arama);
                liste_filtrele(syslog_listesi, arama);
                break;

            case 5:
                printf("  Surec adi (orn: sshd, kernel, CRON): ");
                scanf("%s", arama);
                liste_surece_gore_filtrele(syslog_listesi, arama);
                break;

            case 6:
                liste_istatistik(syslog_listesi);
                break;

            case 0:
                printf("\n  Program sonlandiriliyor...\n");
                break;

            default:
                printf("  [UYARI] Gecersiz secim. Tekrar deneyin.\n");
        }
    } while (secim != 0);

    /* ---- Adim 5: Bellek temizligi ---- */
    liste_serbest_birak(syslog_listesi);

    return EXIT_SUCCESS;
}