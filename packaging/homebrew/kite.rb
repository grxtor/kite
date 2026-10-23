# Filled in and pushed to grxtor/homebrew-tap by .github/workflows/publish.yml.
cask "kite" do
  arch arm: "ARM", intel: "Intel"

  version "@VERSION@"
  sha256 arm:   "@SHA256:Kite-macOS-ARM-Qt6-@TAG@.zip@",
         intel: "@SHA256:Kite-macOS-Intel-Qt6-@TAG@.zip@"

  url "https://github.com/grxtor/kite/releases/download/v#{version}/Kite-macOS-#{arch}-Qt6-v#{version}.zip"
  name "Kite"
  desc "Minecraft launcher with multi-instance management"
  homepage "https://kiteclient.com/"

  livecheck do
    url :url
    strategy :github_latest
  end

  depends_on macos: :monterey

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
