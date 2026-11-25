#include "OsBackupActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <HalPowerManager.h>
#include <I18n.h>
#include <Logging.h>

#include "MappedInputManager.h"
#include "OsBackupService.h"
#include "activities/util/ConfirmationActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
// Same low-battery gate main.cpp applies to power-hungry features: writing
// several MB to SD (and, for restore, to flash) must not start on a nearly
// empty battery. getBatteryPercentage() returns a value on every board
// (fuel gauge or ADC fallback), so the check is always meaningful.
constexpr uint16_t kMinBatteryPercent = 20;
}  // namespace

void OsBackupActivity::onEnter() {
  Activity::onEnter();
  if (powerManager.getBatteryPercentage() < kMinBatteryPercent) {
    LOG_ERR("OSBACKUP", "battery too low: %u%%", powerManager.getBatteryPercentage());
    errorMessage = tr(STR_OS_LOW_BATTERY);
    state = State::FAILED;
    requestUpdate();
    return;
  }
  promptConfirmation();
}

void OsBackupActivity::promptConfirmation() {
  state = State::CONFIRMING;
  startActivityForResult(std::make_unique<ConfirmationActivity>(renderer, mappedInput, tr(STR_OS_BACKUP_PROMPT),
                                                                os_backup::kBackupRootPath),
                         [this](const ActivityResult& result) { onConfirmationResult(result); });
}

void OsBackupActivity::onConfirmationResult(const ActivityResult& result) {
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
  performBackup();
}

void OsBackupActivity::performBackup() {
  auto progressCb = +[](size_t done, size_t total, void* ctx) {
    auto* self = static_cast<OsBackupActivity*>(ctx);
    self->writtenBytes = done;
    self->totalBytes = total;
    self->requestUpdate(true);
  };

  std::string name;
  const auto err = os_backup_service::createBackup(progressCb, this, &name);
  if (err != os_backup_service::Error::Ok) {
    LOG_ERR("OSBACKUP", "backup failed: %s", os_backup_service::errorName(err));
    errorMessage = tr(STR_OS_BACKUP_FAILED);
    RenderLock lock(*this);
    state = State::FAILED;
    requestUpdate();
    return;
  }

  createdName = name;
  RenderLock lock(*this);
  state = State::SUCCESS;
  requestUpdate();
}

void OsBackupActivity::loop() {
  if (state == State::SUCCESS || state == State::FAILED) {
    int x = 0;
    int y = 0;
    if (mappedInput.wasPressed(MappedInputManager::Button::Back) ||
        mappedInput.wasPressed(MappedInputManager::Button::Confirm) || mappedInput.wasScreenTapped(x, y)) {
      finish();
    }
  }
}

void OsBackupActivity::render(RenderLock&&) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_OS_BACKUP));

  const auto lineHeight = renderer.getLineHeight(UI_10_FONT_ID);
  const auto top = (pageHeight - lineHeight) / 2;

  if (state == State::RUNNING) {
    const unsigned int pct = totalBytes > 0 ? static_cast<unsigned int>((writtenBytes * 100) / totalBytes) : 0;
    if (pct == lastRenderedPercent) return;
    lastRenderedPercent = pct;

    renderer.drawCenteredText(UI_10_FONT_ID, top, tr(STR_OS_BACKUP_RUNNING), true, EpdFontFamily::BOLD);
    int y = top + lineHeight + metrics.verticalSpacing;
    GUI.drawProgressBar(renderer,
                        Rect{metrics.contentSidePadding, y, pageWidth - metrics.contentSidePadding * 2,
                             metrics.progressBarHeight},
                        static_cast<int>(pct), 100);
    y += metrics.progressBarHeight + metrics.verticalSpacing + lineHeight + metrics.verticalSpacing;
    renderer.drawCenteredText(UI_10_FONT_ID, y, tr(STR_FIRMWARE_UPDATE_DO_NOT_POWER_OFF));
  } else if (state == State::SUCCESS) {
    renderer.drawCenteredText(UI_10_FONT_ID, top, tr(STR_OS_BACKUP_DONE), true, EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_10_FONT_ID, top + lineHeight + metrics.verticalSpacing, createdName.c_str());
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  } else if (state == State::FAILED) {
    renderer.drawCenteredText(UI_10_FONT_ID, top, tr(STR_OS_BACKUP_FAILED), true, EpdFontFamily::BOLD);
    if (!errorMessage.empty()) {
      renderer.drawCenteredText(UI_10_FONT_ID, top + lineHeight + metrics.verticalSpacing, errorMessage.c_str());
    }
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }
  // CONFIRMING: the ConfirmationActivity is on top, nothing to draw.

  renderer.displayBuffer();
}
