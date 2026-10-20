// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0
import type { Dictionary } from './en';

export const tr = {
  siteName: 'Kite',
  meta: {
    home: {
      title: 'Kite: kurulumlar etrafında tasarlanmış bir Minecraft başlatıcısı',
      description:
        'Kite, Minecraft’ın her kopyasını kendi modları, dünyaları ve ayarlarıyla ayrı bir kurulumda tutar. macOS, Windows ve Linux için ücretsiz ve açık kaynak.',
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
    download: 'İndir',
    privacy: 'Gizlilik',
    terms: 'Koşullar',
    github: 'GitHub',
  },
  hero: {
    title: 'Her Minecraft kendi yerinde.',
    lead: 'Kite, oyunun her kopyasını kendi kurulumunda tutar; modlar, dünyalar ve ayarlar birbirine karışmaz. Bir sürüm seç, mod paketi ekle, Microsoft ile giriş yap ve oyna.',
    primaryCta: 'Kite’ı indir',
    secondaryCta: 'Neler yaptığına bak',
    note: 'Ücretsiz ve açık kaynak · macOS, Windows ve Linux',
    screenshotAlt:
      'Kite ana sayfası: en son oynanan kurulum ve Oyna düğmesi, devam edilecek kurulumların listesi, sağda son dünyalar ve altta kütüphane.',
  },
  features: {
    title: 'Kurulumlar etrafında tasarlandı',
    intro: {
      title: 'Hiçbiri diğerini bozmaz',
      text: 'Vanilla 1.21, 200 modluk bir paket ve bir ön sürüm yan yana durabilir. Her kurulumun kendi modları, dünyaları, kaynak paketleri ve Java ayarları vardır.',
    },
    rows: {
      mods: {
        title: 'Klasör karıştırmadan modlar',
        text: 'Bir kurulumdaki tüm modları gör, birini kapat, güncelle ya da Kite’tan çıkmadan Modrinth’ten yenilerini ekle.',
        alt: 'Bir Fabric kurulumunun İçerik sekmesi: altı mod, sürümleri ve her biri için açma kapama düğmesi; üstte güncelleme denetleme ve içerik ekleme düğmeleri.',
      },
      loaders: {
        title: 'Her yükleyici, her sürüm',
        text: 'Kurulum oluştururken bir Minecraft sürümü ile Fabric, Quilt, Forge ya da NeoForge seç. Gerisini Kite indirir.',
        alt: 'Yeni kurulum penceresi: ad alanı, Minecraft sürümleri listesi ve mod yükleyici satırı: Vanilla, Fabric, Quilt, Forge ve NeoForge.',
      },
      accounts: {
        title: 'Hesapların ve skin’lerin',
        text: 'Microsoft hesabınla giriş yap, birden fazla hesap tut, skin’ini ve pelerinini başlatıcıdan değiştir.',
        alt: 'Hesaplar ekranındaki skin ve pelerin penceresi: skin önizlemesi, varsayılan skin’lerin ızgarası ve pelerin seçimi.',
      },
      java: {
        title: 'Java hazır',
        text: 'Kite, oynadığın sürüme uyan bir Java bulur ve belleği bir kaydırıcıyla ayarlamanı sağlar.',
        alt: 'Ayarlar’ın Java ve bellek sayfası: en çok bellek için 6 GB’a ayarlı bir kaydırıcı, başlangıç belleği alanı ve Java yolu.',
      },
    },
  },
  highlights: {
    worlds: {
      title: 'Dünyalar ve ekran görüntüleri',
      text: 'Her kurulum kendininkileri listeler.',
    },
    backups: {
      title: 'Başlatmadan önce yedek',
      text: 'İsteğe bağlı, ayarlardan seçersin.',
    },
    imports: {
      title: 'Paketlerini içe aktarır',
      text: 'Modrinth .mrpack, CurseForge, MultiMC ve Prism zip dosyaları.',
    },
    language: {
      title: 'Senin dilin',
      text: 'İngilizce, Türkçe ve daha fazlası.',
    },
    openSource: {
      title: 'Açık kaynak',
      text: 'Apache 2.0, MeshMC ve MultiMC üzerine kurulu.',
    },
    playTime: {
      title: 'Oynama süresi ve son dünyalar',
      text: 'Ana sayfa kaldığın yeri gösterir.',
    },
  },
  closing: {
    title: 'Kite’ı edin',
    text: 'Ücretsiz, yüklemek için hesap gerekmez.',
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
      macos: { name: 'macOS', detail: 'Apple silicon ve Intel' },
      windows: { name: 'Windows', detail: 'Windows 10 ve 11, 64 bit' },
      linux: { name: 'Linux', detail: 'AppImage, 64 bit' },
    },
    requirementsTitle: 'Gerekenler',
    requirements: [
      'Oynamak için bir Minecraft: Java Edition hesabı (Microsoft)',
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
