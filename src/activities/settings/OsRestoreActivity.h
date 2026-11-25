#pragma once

#include <GfxRenderer.h>
#include <I18n.h>

#include <string>
#include <vector>

#include "OsBackupService.h"
#include "activities/UiListActivity.h"

class MappedInputManager;

/**
 * OS restore picker (fork feature, lib/OsBackup + src/OsBackupService).
 *
 * Lists the backup generations under /.crosspoint/backup (newest last, so
 * the most recent backup opens at the bottom / latest position). Rows with
 * a missing or corrupt manifest are shown disabled and cannot be selected.
 * Selecting a valid row pushes the run activity (defined in the .cpp):
 * battery guard -> confirmation naming the backup and the restart ->
 * full verification -> flash to the inactive OTA slot -> config restore ->
 * restart into the restored OS.
 */
class OsRestoreActivity final : public UiListActivity {
 public:
  explicit OsRestoreActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;

 private:
  int listCount() const override { return static_cast<int>(rowItems.size()); }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override;

  std::vector<os_backup_service::BackupEntry> entries;
  // ListItem holds const char*, so labels/values need owned storage.
  std::vector<std::string> labelStorage;
  std::vector<std::string> valueStorage;
  std::vector<freeink::ui::ListItem> rowItems;
};
