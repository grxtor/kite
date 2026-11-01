---
layout: ../../layouts/LegalLayout.astro
title: "Gizlilik Politikası"
description: "Kite’ın ve kiteclient.com’un verilerinle ne yaptığı."
updated: "2026-10-25"
---

Kite, grxtor’un yaptığı bir Minecraft başlatıcısı. Bu politika Kite uygulamasını ve bu siteyi, kiteclient.com’u kapsar. Anlaşılmayan bir yer olursa [contact@kiteclient.com](mailto:contact@kiteclient.com) adresine yaz.

## Kısaca

- Kite’ta analitik, telemetri ya da reklam yok. Kite sana veya bilgisayarına bir kimlik numarası vermez.
- Kite’ın kendine ait bir hesap sistemi yok. Microsoft girişin, ayarların ve kurulumların senin bilgisayarında kalır.
- Arkadaşlar, bizim bir sunucumuzu kullanan tek özellik. Microsoft hesabıyla giriş yaptığında varsayılan olarak açıktır; Ayarlar → Arkadaşlar’dan kapatabilirsin. Açıkken Kite’ın sunucusu Minecraft adını, arkadaşlarını ve ne oynadığını tutar; bunların hepsini Kite’ın içinden silebilirsin.
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
- Arkadaşlar açıkken: Kite’ın arkadaşlık sunucusunun anahtarını tutan `friends.json` ve `cache/friendfaces` içinde arkadaşlarının kafaları

Giriş anahtarları şifrelenmez, ama `accounts.json` ve `friends.json` dosyalarını yalnızca kendi kullanıcı hesabın okuyabilir. Kite’ta bir hesabı kaldırınca onun anahtarları da silinir. Kite’ı kaldırıp veri klasörünü silersen her şey gider.

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
| Kite’ın arkadaşlık sunucusu (`api.kiteclient.com`) | Yalnızca Arkadaşlar açıkken: giriş yaparken, sonra Kite açık olduğu sürece dakikada bir | Aşağıdaki [Arkadaşlar](#arkadaşlar) bölümüne bak |
| Mojang’ın oturum sunucusu (`sessionserver.mojang.com`) ve `textures.minecraft.net` | Yalnızca Arkadaşlar açıkken: giriş yaparken ve arkadaşlarının kafalarını göstermek için | Girişte, Minecraft anahtarınla bir katılma isteği; oyunun bir sunucuya girerken gönderdiğinin aynısı. Kafalar için arkadaş listendeki, isteklerindeki ve engellediklerindeki oyuncuların UUID’leri. |
| paste.ee | Bir log için Yükle’ye bastığında. Kite’ın kendisi çökerse, çökme bildiricisi Kite’ın kendi logunu hemen yükler ve sana bağlantısını gösterir. | Log metni; herkese açık bir bağlantı olur. Kite önce giriş anahtarlarını, UUID’leri ve ev klasörünün yolunu çıkarır. Oyuncu adın, kurulum adların ve mod listen logda kalır. |

Oynarken Minecraft’ın kendisi Mojang ve Microsoft’a, bir de girdiğin sunuculara bağlanır. Bu, Kite’ta değil oyunun içinde olur.

### Eklentiler

Kite eklenti yükleyebilir. Başkalarının yazdığı eklentiler kendi yazılımlarıdır ve veriyi her yere gönderebilir. Yalnızca güvendiğin eklentileri kur.

## Arkadaşlar

Arkadaşlar, arkadaşlarının hangi sunucuda oynadığını gösterir. Sohbet yok. Microsoft hesabıyla giriş yaptığında varsayılan olarak açıktır ve Kite ilk seferde arkadaşlarının seni görebildiğini söyleyen bir not gösterir. Ayarlar → Arkadaşlar’daki **Arkadaşları kullan** ile kapatabilirsin. Kapalıyken Kite bu özellik için hiçbir şey göndermez.

### Giriş

Arkadaşlar, Kite’ın kendi sunucusuna (`api.kiteclient.com`) bir Minecraft sunucusunun oyuna giren oyuncuyu doğruladığı yolla giriş yapar: Kite, Mojang’ın oturum sunucusuna hesabının katıldığını söyler, Kite’ın sunucusu da bunu Mojang’a doğrulatır. Microsoft ve Minecraft anahtarların yalnızca Mojang’a gider, Kite’ın sunucusuna hiç gitmez.

Ardından Kite’ın sunucusu Kite’a kendi anahtarını verir. Bu anahtar arkadaş listesinden başka hiçbir şeyi açmaz ve 30 gün geçerlidir. Arkadaşlar için Microsoft hesabı gerekir; çevrimdışı hesaplarla kullanılamaz.

### Kite’ın gönderdikleri

Kite açık olduğu sürece dakikada bir sunucuya şunları bildirir:

- Kite’ın açık olduğunu ya da oyunun çalıştığını
- oyun çalışırken: kurulumun adını, Minecraft sürümünü ve nerede olduğunu. Kite bunu oyunun logundan okur: girdiğin sunucunun adresi ve portu ya da tek oyunculu.

Kite’ı kapattığında sunucuya çevrimdışı olduğunu bildirir.

### Sunucunun tuttukları

- Minecraft UUID’in ve adın, bir de en son ne zaman görüldüğün
- Arkadaşlar ayarların
- arkadaşların, bekleyen isteklerin ve engellediğin oyuncular
- şu anki durumun. Kite’ın son bildiriminden üç dakika sonra süresi dolar ve silinir.
- giriş anahtarların; yalnızca SHA-256 özeti olarak saklanır, yani geri okunamaz

Arkadaşlık sunucusu, bu site gibi Cloudflare’in arkasında kendi makinemizde çalışır ve istek kaydı tutmaz. Cloudflare ve sunucu IP adresini görür. Sunucu onu yalnızca bellekte, bir adresin ne kadar istek yapabileceğini sınırlamak için tutar.

### Kim neyi görür

- **Seni kimler görebilir** varsayılan olarak Arkadaşlar’dır: durumunu ve en son ne zaman çevrimiçi olduğunu yalnızca arkadaşların görür.
- Herkes seçiliyse giriş yapmış her Kite kullanıcısı da adınla durumuna bakabilir.
- Görünmez seni her zaman çevrimdışı gösterir, son görülme zamanı da göstermez; sunucu senin için hiçbir durum tutmaz.
- **Hangi sunucuda olduğumu gizle** açıksa arkadaşların oynadığını görür, ama nerede oynadığını görmez. Sunucu adresi de tutmaz.
- Ham bir IP adresiyle girdiğin ya da ev ağındaki bir sunucu “bir sunucu” olarak görünür. **IP adresiyle girdiğim sunucuları göster**’i açmadıkça adresi ne gösterilir ne tutulur.
- **Arkadaşlarım bulunduğum sunucuya katılabilsin** varsayılan olarak açıktır. Arkadaşlarına, bulunduğun sunucu için bir Katıl düğmesi verir. Bu düğme yalnızca sunucunu görebildiklerinde çıkar: sunucuyu gizlersen ya da paylaşmadığın ham IP’li veya ev ağındaki bir sunucudaysan Katıl düğmesi olmaz.
- Minecraft adını bilen her Kite kullanıcısı sana arkadaşlık isteği gönderebilir. Birini engellersen arkadaşlarından çıkar; artık sana istek gönderemez ve durumunu göremez.

### Arkadaşlarının kafaları

Kite arkadaşlarının kafalarını Kite’ın sunucusu üzerinden değil, doğrudan Mojang’dan (`sessionserver.mojang.com` ve `textures.minecraft.net`) indirir. Onları veri klasöründe tutar ve bir gün sonra yeniden indirir.

### Kapatmak ve verilerini silmek

Arkadaşlar’ı kapatınca Kite sunucuya çevrimdışı olduğunu bildirir, oturumu kapatır, anahtarı ve kafaları bilgisayarından siler. Arkadaşların sunucuda kalır, böylece sonra yeniden açabilirsin. Hesabın sen silene kadar orada durur.

Ayarlar → Arkadaşlar’daki **Arkadaşlık verilerimi sil**, Kite’ın sunucusundaki hesabını ve onunla saklanan her şeyi siler: anahtarlar, durum, ayarlar, arkadaşlıklar, iki yöndeki istekler ve senin koyduğun engeller. Başka oyuncuların seni engellemesi kalır, çünkü o kayıtlar onlarındır. Arkadaşlar da kapanır.

## Çocuklar

Kite ve bu site, çocuklar dahil hiç kimseden kişisel veri istemez. Arkadaşlar açıkken Kite’ın sunucusu yalnızca [Arkadaşlar](#arkadaşlar) bölümünde yazanları tutar. Küçük oyuncuların girişi Microsoft’un kendi aile ayarlarıyla yönetilir.

## Hakların

Arkadaşlar’ı kullanmıyorsan senin hakkında bir hesap ya da profil tutmuyoruz. Bu yüzden çoğu durumda bakabileceğimiz, düzeltebileceğimiz ya da silebileceğimiz bir şey yok. Verilerin bilgisayarındaki Kite klasöründe durur ve onları istediğin zaman silebilirsin. Arkadaşlar’ı kullanıyorsan **Arkadaşlık verilerimi sil**, Kite’ın sunucusunun senin hakkında tuttuklarını siler. Bize e-posta attıysan o e-postaları silmemizi isteyebilirsin. Türkiye’de, AB’de ya da Birleşik Krallık’taysan veri koruma kurumuna da başvurabilirsin (Türkiye’de KVKK).

## Değişiklikler

Bu politikayı değiştirirsek bu sayfayı ve en üstteki tarihi güncelleriz. Değişiklik uygulamanın gönderdiklerini etkiliyorsa Kite’ın sürüm notlarında da yazar.

## İletişim

grxtor, [contact@kiteclient.com](mailto:contact@kiteclient.com)
