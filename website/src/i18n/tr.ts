// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0
import type { Dictionary } from './types';

export const tr = {
  siteName: 'Kite',
  meta: {
    home: {
      title: 'Kite: native bir Minecraft başlatıcısı',
      description:
        'Kite, C++ ile yazılmış, arayüzü Qt Quick olan bir Minecraft başlatıcısı. Ayrı kurulumlar; Modrinth, CurseForge, FTB, ATLauncher ve Technic paketleri; Java’yı da kendisi halleder. Ücretsiz ve açık kaynak.',
    },
    download: {
      title: 'Kite’ı indir',
      description:
        'macOS, Windows ve Linux için ücretsiz ve açık kaynak Minecraft başlatıcısı Kite’ı indir.',
    },
    notFound: {
      title: 'Sayfa bulunamadı',
      description: 'Bu sayfa yok.',
    },
  },
  nav: {
    skipToContent: 'İçeriğe geç',
    mainLabel: 'Ana gezinme',
    footerLabel: 'Alt bilgi',
    languageLabel: 'Dil',
    homeLabel: 'Kite ana sayfa',
    features: 'Özellikler',
    compare: 'Karşılaştır',
    download: 'İndir',
    privacy: 'Gizlilik',
    terms: 'Koşullar',
    github: 'GitHub',
  },
  hero: {
    title: 'Native yazılmış bir Minecraft başlatıcısı.',
    lead: 'Kite, bir web sayfasının etrafına sarılmadı; C++ ile yazıldı, arayüzü Qt Quick. Her kurulumun kendi modları, dünyaları ve Java’sı var. Modrinth, CurseForge, FTB, ATLauncher ve Technic paketleri bir arama uzağında.',
    primaryCta: 'Kite’ı indir',
    secondaryCta: 'Diğerlerinden farkı',
    note: 'Ücretsiz ve açık kaynak · Apache 2.0 · macOS, Windows ve Linux',
    screenshotAlt:
      'Gerçek kurulumlarla Kite ana sayfası: oynamaya hazır MC-26.3-Animations, Zombiemania ve üç Minecraft sürümüyle “Oynamaya devam et” listesi, sağda son dünyalar.',
  },
  native: {
    title: 'Native, web sayfası değil',
    lead: 'Bugün çoğu başlatıcı bir pencereye konmuş web uygulaması. Kite’ın arayüzü Qt’nin arayüz dili QML ile yazıldı ve Qt Quick onu ekran kartınla çiziyor: macOS’ta Metal, Windows’ta Direct3D, Linux’ta OpenGL.',
    body: 'Altında MultiMC’den gelen C++ var; Prism Launcher da aynı aileden. Kite 2026’da MeshMC’den ayrıldı ve sıfırdan yazılmış yeni bir arayüz aldı.',
    facts: [
      { label: 'Arayüz', value: 'Qt Quick (QML)' },
      { label: 'Çekirdek', value: 'C++17 ve Qt 6' },
      { label: 'Çizim', value: 'Metal, Direct3D ya da OpenGL' },
      { label: 'Kite için yazılan', value: '216 QML dosyası, 35.000 satır' },
      { label: 'Lisans', value: 'Apache 2.0' },
    ],
    codeCaption: 'Oyna düğmesi, {file} dosyasından',
  },
  features: {
    title: 'Kurulumlar etrafında tasarlandı',
    lead: 'Kurulum, kendi modları, dünyaları, kaynak paketleri ve Java ayarları olan bir Minecraft kopyasıdır. Sade bir 26.3, 437 modluk bir paket ve bir ön sürüm birbirine dokunmadan yan yana durur.',
    rows: {
      library: {
        title: 'Bütün kütüphanen',
        text: 'Kurulumlar mod paketinin logosunu ya da kendi oyununun dokularından kesilmiş bir ikonu taşır. Sırala, grupla, hangisinde ne kadar oynadığını gör.',
        alt: 'Kite kütüphanesi: ATM10, Create: Skies and Seas, DeceasedCraft, Fabulously Optimized, Fresh & Smooth ve Zombiemania dahil on gerçek kurulum; üstte toplam 58 saat oynama süresi.',
      },
      mods: {
        title: '437 mod, tek liste',
        text: 'Bu All the Mods 10. Bir modu kapat, bütün modların güncellemesini tek seferde denetle ya da klasör açmadan Modrinth ve CurseForge’dan yenilerini ekle.',
        alt: 'ATM10 kurulumunun İçerik sekmesi: sürümleri ve açma kapama düğmeleriyle 437 mod; üstte güncelleme denetleme ve içerik ekleme düğmeleri.',
      },
      loaders: {
        title: 'Her yükleyici, her sürüm',
        text: 'Bir Minecraft sürümü seç, sonra Fabric, Quilt, Forge ya da NeoForge. Oyunu, yükleyiciyi ve gereken bütün kütüphaneleri Kite indirir.',
        alt: 'Yeni kurulum penceresi: ad alanı, 26.3’ten 1.21.11’e Minecraft sürümleri ve mod yükleyici satırı: Vanilla, Fabric, Quilt, Forge ve NeoForge.',
      },
      java: {
        title: 'Java hazır',
        text: 'Her Minecraft sürümü başka bir Java ister. Kite doğrusunu seçer, sende yoksa indirir. Bellek bir kaydırıcı.',
        alt: 'Java ve bellek ayarları: 6 GB’a ayarlı en çok bellek kaydırıcısı, başlangıç belleği alanı ve kullanılan Java.',
      },
    },
  },
  compare: {
    title: 'Diğerlerinden farkı',
    lead: 'Her başlatıcının kendi sitesi, kaynak kodu ve gizlilik politikasıyla Ekim 2026’da kontrol edildi.',
    corner: 'Başlatıcı',
    sourcesTitle: 'Kaynaklar',
  },
  closing: {
    title: 'Kite ücretsiz',
    text: 'macOS, Windows ve Linux için. Reklam yok, bize hesap açman gerekmiyor.',
    cta: 'Kite’ı indir',
  },
  download: {
    title: 'Kite’ı indir',
    lead: 'Kite ücretsiz. Sistemini seç.',
    notReleased: 'Kite’ın ilk sürümü henüz çıkmadı. Çıktığında indirme bağlantıları burada olacak.',
    button: 'İndir',
    buttonLabel: '{name} için Kite’ı indir',
    comingSoon: 'Yakında',
    platforms: {
      macos: { name: 'macOS', detail: 'Apple silicon ve Intel, macOS 12 ya da üstü' },
      windows: { name: 'Windows', detail: 'Windows 10 ve 11, x64 ve Arm' },
      linux: { name: 'Linux', detail: 'AppImage, x86-64 ve ARM64' },
    },
    requirementsTitle: 'Gerekenler',
    requirements: [
      'Minecraft: Java Edition’a sahip bir Microsoft hesabı',
      '64 bit bir sistem',
      'Bir kurulum oyunu ilk kez indirirken internet bağlantısı',
    ],
    sourceLink: 'Kaynak kod ve eski sürümler',
  },
  legal: {
    lastUpdated: 'Son güncelleme',
  },
  notFound: {
    title: 'Sayfa bulunamadı',
    text: 'Bu sayfa yok. Taşınmış olabilir.',
    home: 'Ana sayfaya git',
    download: 'Kite’ı indir',
  },
  footer: {
    license: '© 2026 grxtor. Kite, Apache Lisansı 2.0 ile lisanslanmıştır.',
    fork: 'Kite, MultiMC’nin devamı olan Project Tick’in MeshMC projesinden türetilmiştir.',
    disclaimer:
      'NOT AN OFFICIAL MINECRAFT PRODUCT. NOT APPROVED BY OR ASSOCIATED WITH MOJANG OR MICROSOFT.',
    disclaimerLocal:
      'Resmî bir Minecraft ürünü değildir; Mojang veya Microsoft tarafından onaylanmamıştır ve onlarla bağlantılı değildir.',
    trademark: 'Minecraft is a trademark of Mojang Synergies AB.',
  },
} satisfies Dictionary;
