// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0
import type { Dictionary } from './types';

export const tr = {
  siteName: 'Kite',
  meta: {
    home: {
      title: 'Kite: native bir Minecraft başlatıcısı',
      description:
        'Kite, C++ ve Qt Quick ile yazılmış ücretsiz, açık kaynak bir Minecraft başlatıcısı. Ayrı kurulumlar, Modrinth ve CurseForge paketleri; Java’yı kendisi halleder.',
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
    faq: 'Sorular',
    download: 'İndir',
    privacy: 'Gizlilik',
    terms: 'Koşullar',
    github: 'GitHub',
  },
  hero: {
    eyebrow: 'Bir Minecraft başlatıcısı',
    title: ['Hafif ama', 'her şeyi taşır.'],
    titleAccent: 'Hafif',
    lead: 'C++ ve Qt Quick ile yazıldı; pencereye konmuş bir web sayfası değil. Modrinth, CurseForge, FTB, ATLauncher ve Technic paketleri, her biri kendi kurulumunda.',
    explore: 'İçine göz at',
    cta: 'Kite’ı indir',
    ctaFor: {
      macos: 'macOS için indir',
      windows: 'Windows için indir',
      linux: 'Linux için indir',
    },
    status: 'İlk sürüm yakında',
    minimum: {
      macos: 'macOS 12 ya da üstü',
      windows: 'Windows 10 ve 11',
      linux: 'AppImage, x86-64 ve ARM64',
    },
    otherSystems: 'Diğer sistemler',
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
  faq: {
    title: 'Sorular',
    items: [
      {
        question: 'Minecraft’ı satın almam gerekiyor mu?',
        answer:
          'Evet. Kite, Minecraft: Java Edition’a sahip Microsoft hesabınla giriş yapar. Hesap yoksa Kite yalnızca oyunun demosunu açabilir.',
      },
      {
        question: 'Kite gerçekten ücretsiz mi?',
        answer:
          'Evet. Reklam yok, ücretli paket yok, bize hesap açman da gerekmiyor. Kite Apache Lisansı 2.0 ile lisanslı; kaynak kodu GitHub’da herkese açık.',
      },
      {
        question: 'Prism ya da MultiMC’deki kurulumlarımı getirebilir miyim?',
        answer:
          'Eski başlatıcında kurulumu zip olarak dışa aktar, o zip’i Kite’ta içe aktar. Modrinth .mrpack dosyaları, CurseForge paket zip’leri ve Technic paketleri de aynı şekilde gelir.',
      },
      {
        question: 'Minecraft daha hızlı çalışır mı?',
        answer:
          'Kite bir başlatıcı; FPS’in oyundan ve kurduğun modlardan (Sodium gibi) gelir. Kite her sürüm için doğru Java’yı seçer ve oyuna ayarladığın belleği verir.',
      },
      {
        question: 'Microsoft hesabım güvende mi?',
        answer:
          'Girişi tarayıcında, Microsoft’un kendi sayfasında yaparsın; Kite şifreni hiç görmez. Giriş anahtarları bilgisayarında kalır. Kite’ın neyi nereye gönderdiği gizlilik politikasında tek tek yazıyor.',
      },
      {
        question: 'Hangi sistemlerde çalışır?',
        answer:
          'Apple silicon ve Intel’de macOS 12 ya da üstü; x64 ve Arm’da Windows 10 ve 11; Linux’ta x86-64 ve ARM64 için AppImage.',
      },
    ],
  },
  platformsBand: {
    title: 'Kite’ı edin',
    lead: 'macOS, Windows ve Linux için ücretsiz.',
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
    trademark: 'Minecraft, Mojang Synergies AB’nin ticari markasıdır.',
  },
} satisfies Dictionary;
