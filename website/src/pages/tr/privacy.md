---
layout: ../../layouts/LegalLayout.astro
title: "Gizlilik Politikası"
description: "Kite’ın ve kiteclient.com’un verilerinle ne yaptığı."
updated: "2026-10-20"
---

Kite, grxtor’un yaptığı bir Minecraft başlatıcısı. Bu politika Kite uygulamasını ve bu siteyi, kiteclient.com’u kapsar. Anlaşılmayan bir yer olursa [contact@kiteclient.com](mailto:contact@kiteclient.com) adresine yaz.

## Kısaca

- Kite’ta analitik, telemetri ya da reklam yok. Kite sana veya bilgisayarına bir kimlik numarası vermez.
- Bizim bir hesap sistemimiz yok. Microsoft girişin, ayarların ve kurulumların senin bilgisayarında kalır.
- Kite, çalışmak için gereken servislerle konuşur. Seni giriş yaptırmak ve oyunu indirmek için Microsoft ve Mojang’la, mod ararken ya da güncellerken Modrinth gibi mod siteleriyle bağlantı kurar. Bu servislerin her biri IP adresini görür ve verini kendi gizlilik politikasına göre işler.

## Site

Bu site sabit sayfalardan oluşur. Çerez kullanmaz, betik çalıştırmaz, başka şirketlerden bir şey yüklemez. Yazı tipleri de bu sitenin kendisinden gelir.

Site, Cloudflare’in arkasında kendi sunucumuzda çalışır. Cloudflare ve sunucumuz IP adresini, istediğin sayfayı, zamanı ve tarayıcının kimlik bilgisini (user agent) görür. Bunları yalnızca siteyi sunmak ve kötüye kullanıma karşı korumak için kullanırız. Ayrıca erişim kaydı tutmayız.

## Kite uygulaması

### Bilgisayarında kalanlar

Kite her şeyi bilgisayarındaki veri klasöründe tutar:

- macOS: `~/Library/Application Support/Kite/Kite`
- Windows: `%APPDATA%\Kite\Kite`
- Linux: `~/.local/share/Kite`

Bu klasörde şunlar vardır:

- ayarların
- kurulumların, modları, dünyaları, ekran görüntüleri ve yedekleriyle birlikte
- Kite’ın kendi kayıtları (log)
- `accounts.json`: Minecraft profilin (ad, UUID, skin ve pelerinler) ve Kite’ın oturumunu açık tutmak için gereken giriş anahtarları (token)

Giriş anahtarları şifrelenmez, ama bu dosyayı yalnızca kendi kullanıcı hesabın okuyabilir. Kite’ta bir hesabı kaldırınca onun anahtarları da silinir. Kite’ı kaldırıp veri klasörünü silersen her şey gider.

### Microsoft ile giriş

Giriş yaparken Kite, Microsoft’un kendi giriş sayfasını tarayıcında açar. Kite şifreni hiçbir zaman görmez. Onay verdikten sonra Microsoft, kendi bilgisayarındaki kısa ömürlü bir adres (`127.0.0.1`) üzerinden Kite’a bir anahtar gönderir. Kite bu anahtarı şu servislerle kullanır:

- **Xbox Live:** Minecraft’ın istediği anahtarları almak için. Giriş sırasında Kite, Xbox Live’dan Xbox profil ayarlarını da ister, ama bunları saklamaz ve hiçbir yere göndermez.
- **Minecraft servisleri** (`api.minecraftservices.com`): Minecraft: Java Edition’a sahip olduğunu doğrulamak, profilini, skinini ve pelerinlerini okumak için.

Kite açıkken bu anahtarların süresi dolunca onları yeniler. Skin ve pelerin editörünü kullanırsan seçtiğin skin Minecraft profiline yüklenir.

Çevrimdışı hesaplar ancak oyuna sahip bir Microsoft hesabı ekledikten sonra açılır. Çevrimdışı hesap yalnızca bir ad saklar.

### Kite’ın bağlandığı servisler

Kite normal kullanımda şu servislere bağlanır. Her isteğe, o istek için gerekenden fazla kişisel veri eklemez.

| Servis | Ne zaman | Ne gider |
|---|---|---|
| Mojang (`minecraft.net` sunucuları) | Bir kurulumun oyun dosyalarına, kütüphanelere ya da varlıklara ihtiyacı olduğunda | İndirme istekleri |
| Meta sunucusu | Kurulum oluştururken, sürüm değiştirirken ya da Kite’ın bir Java’ya ihtiyacı olduğunda | Sürüm listesi istekleri. Bugün bu sunucu `meta.projecttick.org`; Kite’ın çatallandığı proje olan Project Tick tarafından işletiliyor. Kite, kiteclient.com’daki kendi sunucusuna geçecek. |
| Java sağlayıcıları: Mojang, Eclipse Adoptium, Azul | Bir kurulum sende olmayan bir Java sürümü istediğinde ve otomatik Java indirme açıkken (varsayılan olarak açık) | İndirme istekleri |
| Modrinth, CurseForge, FTB, ATLauncher, Technic | Mod paketlerine ve modlara bakarken, kurarken ya da güncellerken | Arama sözcüklerin. Güncelleme denetiminde, servis eşleştirebilsin diye kurulu modlarının dosya özetleri (hash). |
| Paket yazarlarının seçtiği görsel sunucuları | Bir mod paketinin açıklamasını açtığında | O açıklamadaki görsellerin istekleri |
| GitHub | Sürüm derlemelerinde günde yaklaşık bir kez, Kite’ın yeni sürümü var mı diye bakmak için. Ayrıca `git` kuruluysa açılışta, Kite’ın çevrimdışı wiki kopyasını güncellemek için. | İndirme istekleri |
| paste.ee | Bir log için Yükle’ye bastığında. Kite’ın kendisi çökerse, çökme bildiricisi Kite’ın kendi logunu hemen yükler ve sana bağlantısını gösterir. | Log metni; herkese açık bir bağlantı olur. Kite önce giriş anahtarlarını, UUID’leri ve ev klasörünün yolunu çıkarır. Oyuncu adın, kurulum adların ve mod listen logda kalır. |

Oynarken Minecraft’ın kendisi Mojang ve Microsoft’a, bir de girdiğin sunuculara bağlanır. Bu, Kite’ta değil oyunun içinde olur.

### Eklentiler

Kite eklenti yükleyebilir. Başkalarının yazdığı eklentiler kendi yazılımlarıdır ve veriyi her yere gönderebilir. Yalnızca güvendiğin eklentileri kur.

## Çocuklar

Kite ve bu site, çocuklar dahil hiç kimseden kişisel veri istemez ya da toplamaz. Küçük oyuncuların girişi Microsoft’un kendi aile ayarlarıyla yönetilir.

## Hakların

Senin hakkında bir hesap ya da profil tutmuyoruz. Bu yüzden çoğu durumda bakabileceğimiz, düzeltebileceğimiz ya da silebileceğimiz bir şey yok. Verilerin bilgisayarındaki Kite klasöründe durur ve onları istediğin zaman silebilirsin. Bize e-posta attıysan o e-postaları silmemizi isteyebilirsin. Türkiye’de, AB’de ya da Birleşik Krallık’taysan veri koruma kurumuna da başvurabilirsin (Türkiye’de KVKK).

## Değişiklikler

Bu politikayı değiştirirsek bu sayfayı ve en üstteki tarihi güncelleriz. Değişiklik uygulamanın gönderdiklerini etkiliyorsa Kite’ın sürüm notlarında da yazar.

## İletişim

grxtor, [contact@kiteclient.com](mailto:contact@kiteclient.com)
