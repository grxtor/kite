# Kite Türkçe çeviri sözlüğü

Bu sözlük Kite'ın Türkçe arayüz metinleri için. Terimler her yerde aynı kalmalı.

## Üslup
- Kısa ve sade yaz. Komutlar emir kipinde ve senli olsun: "Kurulum oluştur", "Giriş yap". "Oluşturunuz" gibi resmi biçimler kullanma.
- Butonlar ve menüler kısa olsun, açıklamalar tam cümle.
- Ürün ve marka adları çevrilmez: Kite, Minecraft, Microsoft, Xbox, Mojang, Modrinth, CurseForge, FTB, Technic, ATLauncher, Fabric, Forge, NeoForge, Quilt, LiteLoader, Java, JVM, Discord.
- Minecraft'ın resmî Türkçe terimleri varsa onları kullan.

## Terimler
| İngilizce | Türkçe |
|---|---|
| instance | kurulum (çoğulu: kurulumlar) |
| New instance | Yeni kurulum |
| launcher | başlatıcı |
| mod / mods | mod / modlar |
| modpack | mod paketi |
| resource pack | kaynak paketi |
| shader pack | gölgelendirici paketi |
| data pack | veri paketi |
| world | dünya |
| server | sunucu |
| screenshot | ekran görüntüsü |
| backup | yedek |
| log | günlük |
| console | konsol |
| content | içerik |
| loader / mod loader | yükleyici / mod yükleyici |
| version | sürüm |
| snapshot | ön sürüm (snapshot) |
| account | hesap |
| sign in / sign out | giriş yap / çıkış yap |
| offline account | çevrimdışı hesap |
| default account | varsayılan hesap |
| skin | skin |
| cape | pelerin |
| Play | Oyna |
| Stop | Durdur |
| Launch | Başlat |
| Kill | Zorla kapat |
| Home | Ana sayfa |
| Library | Kütüphane |
| Discover | Keşfet |
| Settings | Ayarlar |
| Accounts | Hesaplar |
| Java & memory | Java ve bellek |
| memory | bellek |
| Appearance | Görünüm |
| Group instances | Kurulumları grupla |
| Sort / Name / Last played / Time played | Sırala / Ad / Son oynanma / Oynama süresi |
| Create / Import / Export | Oluştur / İçe aktar / Dışa aktar |
| Duplicate | Kopyala |
| Delete / Rename / Remove | Sil / Yeniden adlandır / Kaldır |
| Add / Edit / Save | Ekle / Düzenle / Kaydet |
| Cancel / Close / Continue / Back | İptal / Kapat / Devam / Geri |
| Browse | Gözat |
| Download / Install / Update | İndir / Yükle / Güncelle |
| Open folder | Klasörü aç |
| Search… | Ara… |
| Yesterday / Today | Dün / Bugün |
| %n days ago | %n gün önce |
| Survival / Creative / Adventure / Spectator / Hardcore | Hayatta Kalma / Yaratıcı / Macera / İzleyici / Zorlu |
| Recommended | Önerilen |
| Enabled / Disabled | Açık / Kapalı (ayarlarda) |

## Teknik kurallar
- Yer tutucular aynen kalır: `%1` … `%9`, `%L1`, `%n`. Sırası Türkçeye göre değişebilir. Her yer tutucu çeviride tam bir kez geçmeli.
- HTML etiketleri (`<b>`, `<a href="…">`, `<br/>` vb.) ve bağlantılar aynen kalır, sadece aradaki metin çevrilir.
- `&` kısayol işareti (ör. `&File`) varsa çeviride de bir harften önce kalsın.
- Baştaki ve sondaki boşluklar, `:` ve `…` gibi son ekler korunur.
- Türkçe çoğulda tek biçim var. `numerus` metinlerde çeviri tek elemanlı bir listedir: `["%n yedek"]`. Sayıdan sonra isim tekil kalır: "3 mod", "5 dünya".
- Ek getiren yer tutuculardan kaçın. "%1'e" yerine cümleyi eksiz kur, ör. "Hedef: %1".
- Dosya yolu, komut, JSON anahtarı, ortam değişkeni ve teknik kimlikler çevrilmez.
