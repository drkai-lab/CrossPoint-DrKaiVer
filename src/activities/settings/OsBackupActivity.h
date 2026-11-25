#pragma once

#include <string>

#include "activities/Activity.h"

/**
 * OS backup activity (fork feature, lib/OsBackup + src/OsBackupService).
 *
 * Flow:
 *  1) onEnter -> battery guard, then ConfirmationActivity
 *     ("Back up the current OS to SD?").
 *  2) On confirm: stream the running app partition to
 *     /.crosspoint/backup/<name>/firmware.bin with a progress bar
 *     (plus nvs.bin and the /.crosspoint/*.json config stores), verify by
 *     read-back hash, and write manifest.json last.
 *  3) SUCCESS shows the generation name; FAILED keeps the current OS
 *     untouched (a backup never writes flash).
 */
class OsBackupActivity : public Activity {
 public:
  enum class State { CONFIRMING, RUNNING, SUCCESS, FAILED };

  explicit OsBackupActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("OsBackup", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return state == State::RUNNING; }
  bool skipLoopDelay() override { return state == State::RUNNING; }

 private:
  State state = State::CONFIRMING;
  size_t writtenBytes = 0;
  size_t totalBytes = 0;
  unsigned int lastRenderedPercent = 101;
  std::string createdName;
  std::string errorMessage;

  void promptConfirmation();
  void onConfirmationResult(const ActivityResult& result);
  void performBackup();
};
