// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

// License: GPL-3.0

#ifndef SOURCES_LINUX_VIEW_H
#define SOURCES_LINUX_VIEW_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "App.h"
#include "Graphics.h"
#include "Utility.h"
#include "View.h"

class Application;

class LinuxView : public View {
public:
  LinuxView(Application *p_App);
  virtual ~LinuxView();

  int Update(void);
  int Render(void);

private:
  Application *m_App;

  // Whether the payload has already been handed off to a loader
  bool m_Launched;

  // Periodic re-scan while waiting for (e.g.) USB drives to mount
  uint64_t m_LastScanTime;
  int m_ScanAttempts;

  std::string m_ErrorMessage;
  bool m_OnError;

  // Colors
  Color m_BackgroundColor;
  Color m_TextColor;
  Color m_DividerColor;

  template <typename... Args>
  void ShowError(const std::string &p_Format, Args... p_Args) {
    Utility::StrSprintf(m_ErrorMessage, p_Format, p_Args...);
    m_OnError = true;
  }

  static bool IsFileNonEmpty(const std::string &p_Path);
  std::string ResolvePayloadPath(const std::string &p_Filename);
  bool TryLaunch(const char *p_Filename);
  void LaunchPayload(const std::string &p_Path);
};

#endif