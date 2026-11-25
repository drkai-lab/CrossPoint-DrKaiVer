#include "OsRestoreActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <HalPowerManager.h>
#include <I18n.h>
#include <Logging.h>

#include "MappedInputManager.h"
#include "activities/Activity.h"
#include "activities/util/ConfirmationActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace fui = freeink::ui;

namespace {

// Same low-battery gate as OsBackupActivity / main.cpp.
constexpr uint16_t kMinBatteryPercent = 20;

// ---------------------------------------------------------------------------
// Run activity: confirmation -> verification -> flash -> restart.
// Only OsRestoreActivity constructs it, so it lives in this TU.
// ---------------------------------------------------------------------------
class OsRestoreRunActivity : public Activity {
 public:
  enum class State { CONFIRMING, RUNNING, SUCCESS, FAILED };

  OsRestoreRunActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string name,
                       std::string summary)
      : Activity("OsRestoreRun", renderer, mappedInput), backupName(std::move(name)), summary(std::move(summary)) {}

  void onEnter() override {
    Activity::onEnter();
    if (powerManager.getBatteryPercentage() < kMinBatteryPercent) {
      LOG_ERR("OSRESTORE", "battery too low: %u%%", powerManager.getBatteryPercentage());
      errorMessage = tr(STR_OS_LOW_BATTERY);
      state = State::FAILED;
      requestUpdate();
      return;
    }
    startActivityForResult(
        std::make_unique<ConfirmationActivity>(renderer, mappedInput, tr(STR_OS_RESTORE_PROMPT), summary),
        [this](const ActivityResult& result) { onConfirmationResult(result); });
  }

  void loop() override {
    if (state == State::FAILED) {
      int x = 0;
      int y = 0;
      if (mappedInput.wasPressed(MappedInputManager::Button::Back) ||
          mappedInput.wasPressed(MappedInputManager::Button::Confirm) || mappedInput.wasScreenTapped(x, y)) {
        finish();
      }
    }
  }

  bool preventAutoSleep() override { return state == State::RUNNING; }
  bool skipLoopDelay() override { return state == State::RUNNING; }

  void render(RenderLock&&) override {
    const auto& metrics = UITheme::getInstance().getMetrics();
    const auto pageWidth = renderer.getScreenWidth();
    const auto pageHeight = renderer.getScreenHeight();

    renderer.clearScreen();
    GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_OS_RESTORE));

    const auto lineHeight = renderer.getLineHeight(UI_10_FONT_ID);
    const auto top = (pageHeight - lineHeight) / 2;

    if (state == State::CONFIRMING) {
      // Behind the confirmation popup: say plainly what confirming means.
      renderer.drawCenteredText(UI_10_FONT_ID, top, tr(STR_OS_RESTORE_WARNING));
    } else if (state == State::RUNNING) {
      const unsigned int pct = totalBytes > 0 ? static_cast<unsigned int>((writtenBytes * 100) / totalBytes) : 0;
      if (pct == lastRenderedPercent) return;
      lastRenderedPercent = pct;

      renderer.drawCenteredText(UI_10_FONT_ID, top, tr(STR_OS_RESTORE_RUNNING), true, EpdFontFamily::BOLD);
      int y = top + lineHeight + metrics.verticalSpacing;
      GUI.drawProgressBar(renderer,
                          Rect{metrics.contentSidePadding, y, pageWidth - metrics.contentSidePadding * 2,
                               metrics.progressBarHeight},
                          static_cast<int>(pct), 100);
      y += metrics.progressBarHeight + metrics.verticalSpacing + lineHeight + metrics.verticalSpacing;
      renderer.drawCenteredText(UI_10_FONT_ID, y, tr(STR_FIRMWARE_UPDATE_DO_NOT_POWER_OFF));
    } else if (state == State::SUCCESS) {
      renderer.drawCenteredText(UI_10_FONT_ID, top, tr(STR_OS_RESTORE_DONE), true, EpdFontFamily::BOLD);
      const int hintY = top + lineHeight + metrics.verticalSpacing;
      const Rect hintBounds{metrics.contentSidePadding, hintY, pageWidth - metrics.contentSidePadding * 2,
                            pageHeight - hintY};
      UITheme::drawCenteredWrappedText(renderer, hintBounds, UI_10_FONT_ID, tr(STR_RESTARTING_HINT), 3, true,
                                       EpdFontFamily::REGULAR, UITheme::TextVerticalAlignment::TOP);
    } else if (state == State::FAILED) {
      renderer.drawCenteredText(UI_10_FONT_ID, top, tr(STR_OS_RESTORE_FAILED), true, EpdFontFamily::BOLD);
      if (!errorMessage.empty()) {
        renderer.drawCenteredText(UI_10_FONT_ID, top + lineHeight + metrics.verticalSpacing, errorMessage.c_str());
      }
      const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
      GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    }

    renderer.displayBuffer();
  }

 private:
  State state = State::CONFIRMING;
  std::string backupName;
  std::string summary;
  size_t writtenBytes = 0;
  size_t totalBytes = 0;
  unsigned int lastRenderedPercent = 101;
  std::string errorMessage;

  void onConfirmationResult(const ActivityResult& result) {
    if (result.isCancelled) {
      finish();
      return;
    }
    {
      RenderLock lock(*this);
      state = State::RUNNING;
      writtenBytes = 0;
      totalBytes = 0;
      lastRenderedPercent = 101;
    }
    requestUpdateAndWait();
    performRestore();
  }

  void performRestore() {
    auto progressCb = +[](size_t done, size_t total, void* ctx) {
      auto* self = static_cast<OsRestoreRunActivity*>(ctx);
      self->writtenBytes = done;
      self->totalBytes = total;
      self->requestUpdate(true);
    };

    os_backup::RestoreCheck failedCheck = os_backup::RestoreCheck::Ok;
    const auto err = os_backup_service::restoreBackup(backupName, progressCb, this, &failedCheck);
    if (err != os_backup_service::Error::Ok && err != os_backup_service::Error::ConfigRestoreFailed) {
      LOG_ERR("OSRESTORE", "restore of %s failed: %s (check=%s)", backupName.c_str(),
              os_backup_service::errorName(err), os_backup::restoreCheckName(failedCheck));
      // A refused verification and a corrupt manifest both mean "this
      // backup cannot be trusted for this device"; other errors are plain
      // failures. The current OS is untouched in every failure branch:
      // the boot partition is only switched after a full successful flash.
      errorMessage = (err == os_backup_service::Error::RestoreCheckFailed ||
                      err == os_backup_service::Error::ManifestInvalid ||
                      err == os_backup_service::Error::ImageInvalid)
                         ? tr(STR_OS_RESTORE_INVALID)
                         : tr(STR_OS_RESTORE_FAILED);
      RenderLock lock(*this);
      state = State::FAILED;
      requestUpdate();
      return;
    }
    if (err == os_backup_service::Error::ConfigRestoreFailed) {
      // The OS itself is already restored and boot-switched; only some
      // config JSONs failed to copy back. Restarting is still correct —
      // the restored OS will read whatever config is on SD.
      LOG_ERR("OSRESTORE", "config restore incomplete for %s; restarting anyway", backupName.c_str());
    }

    LOG_INF("OSRESTORE", "restore of %s complete, restarting", backupName.c_str());
    {
      RenderLock lock(*this);
      state = State::SUCCESS;
    }
    requestUpdateAndWait();
    delay(1500);
    ESP.restart();
  }
};

}  // namespace

OsRestoreActivity::OsRestoreActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiListActivity("OsRestore", renderer, mappedInput) {}

void OsRestoreActivity::onEnter() {
  UiListActivity::onEnter();

  entries = os_backup_service::listBackups();
  labelStorage.clear();
  valueStorage.clear();
  rowItems.clear();

  if (entries.empty()) {
    labelStorage.emplace_back(tr(STR_OS_NO_BACKUPS));
    valueStorage.emplace_back("");
  } else {
    for (const auto& entry : entries) {
      labelStorage.push_back(entry.name);
      if (entry.valid) {
        std::string value = entry.manifest.appVersion;
        if (!entry.manifest.createdUtc.empty()) {
          if (!value.empty()) value += " · ";
          value += entry.manifest.createdUtc;
        }
        valueStorage.push_back(value);
      } else {
        valueStorage.emplace_back(tr(STR_OS_RESTORE_INVALID));
      }
    }
  }

  const size_t count = labelStorage.size();
  rowItems.resize(count);
  for (size_t i = 0; i < count; i++) {
    fui::ListItem item;
    item.label = labelStorage[i].c_str();
    item.value = valueStorage[i].empty() ? nullptr : valueStorage[i].c_str();
    item.actionValue = static_cast<int16_t>(i);
    item.enabled = !entries.empty() && entries[i].valid;
    rowItems[i] = item;
  }
  // Open on the newest generation (list is ascending by name).
  if (!entries.empty()) nav.selected = static_cast<int>(entries.size() - 1);
}

const char* OsRestoreActivity::headerTitle() const { return tr(STR_OS_RESTORE); }

void OsRestoreActivity::activateIndex(const int index) {
  app.clearTapFlash();
  if (entries.empty() || index < 0 || index >= static_cast<int>(entries.size())) return;
  const auto& entry = entries[index];
  if (!entry.valid) return;
  nav.selected = index;

  std::string summary = entry.name;
  if (!entry.manifest.appVersion.empty()) summary += " (" + entry.manifest.appVersion + ")";
  startActivityForResult(std::make_unique<OsRestoreRunActivity>(renderer, mappedInput, entry.name, summary),
                         [this](const ActivityResult&) { /* failed restore returns here; list is unchanged */ });
}

void OsRestoreActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  screen.setContentMarginFromScreen(fui::Insets{
      static_cast<int16_t>(safe.y + metrics.topPadding + metrics.headerHeight),
      static_cast<int16_t>(renderer.getScreenWidth() - (safe.x + safe.width)),
      static_cast<int16_t>(renderer.getScreenHeight() - (safe.y + safe.height)), static_cast<int16_t>(safe.x)});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  fui::ListProps props;
  props.items = rowItems.data();
  props.count = static_cast<uint16_t>(rowItems.size());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  props.labelText = screen.theme().smallText;
  props.labelText.maxLines = 2;
  syncListViewport(screen, props);
  screen.list(props);
}
