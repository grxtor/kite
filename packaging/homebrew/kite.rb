# Filled in and pushed to grxtor/homebrew-tap by .github/workflows/publish.yml.
cask "kite" do
  version "@VERSION@"

  # The main builds use Qt 6.11, which needs macOS 13. Intel Macs on macOS 12
  # get the Legacy build (Qt 6.9) instead; there is no Legacy build for ARM.
  on_arm do
    sha256 "@SHA256:Kite-macOS-ARM-Qt6-@TAG@.zip@"

    url "https://github.com/grxtor/kite/releases/download/v#{version}/Kite-macOS-ARM-Qt6-v#{version}.zip"

    depends_on macos: :ventura
  end
  on_intel do
    on_monterey :or_older do
      sha256 "@SHA256:Kite-macOS-Intel-Legacy-Qt6-@TAG@.zip@"

      url "https://github.com/grxtor/kite/releases/download/v#{version}/Kite-macOS-Intel-Legacy-Qt6-v#{version}.zip"
    end
    on_ventura :or_newer do
      sha256 "@SHA256:Kite-macOS-Intel-Qt6-@TAG@.zip@"

      url "https://github.com/grxtor/kite/releases/download/v#{version}/Kite-macOS-Intel-Qt6-v#{version}.zip"
    end

    depends_on macos: :monterey
  end

  name "Kite"
  desc "Minecraft launcher with multi-instance management"
  homepage "https://kiteclient.com/"

  livecheck do
    url :url
    strategy :github_latest
  end

  app "Kite.app"

  # Kite is not notarised yet, so Gatekeeper refuses to open a quarantined
  # copy. Drop this once the release builds are signed with a Developer ID.
  postflight_steps do
    run "/usr/bin/xattr", args: ["-dr", "com.apple.quarantine", "{{appdir}}/Kite.app"]
  end

  zap trash: [
    "~/Library/Application Support/Kite",
    "~/Library/Caches/Kite",
    "~/Library/Preferences/io.github.grxtor.Kite.plist",
    "~/Library/Saved Application State/io.github.grxtor.Kite.savedState",
  ]
end
