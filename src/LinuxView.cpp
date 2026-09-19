// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

// License: GPL-3.0

#include "LinuxView.h"

#include <orbis/libkernel.h> // sceKernelGetProcessTime

#include <cstdio>
#include <string>
#include <vector>

#include "libLog.h"
#include "notifi.h"

#include "App.h"
#include "Controller.h"
#include "Graphics.h"
#include "Language.h"
#include "Resource.h"
#include "Utility.h"

LinuxView::LinuxView(Application *p_App) {
  m_App = p_App;
  if (!m_App) {
    logKernel(LL_Debug, "%s", "App is null!");
    return;
  }

  // Setup variables
  m_Launched = false;
  m_OnError = false;
  m_LastScanTime = 0;
  m_ScanAttempts = 0;

  // Setup color (dark-on-light to match the mockup and the Tux background)
  m_BackgroundColor = {0xFF, 0xFF, 0xFF, 0xFF}; // Fallback if the background image fails to load
  m_TextColor = {0x00, 0x00, 0x00, 0xFF};
  m_DividerColor = {0x00, 0x00, 0x00, 0xFF};

  logKernel(LL_Debug, "%s", "LinuxView: End of constructor");
}

LinuxView::~LinuxView() {
}

std::string LinuxView::ResolvePayloadPath(const std::string &p_Filename) {
  // Prefer the console's internal storage, then fall back to any USB device.
  std::vector<std::string> s_Candidates;
  s_Candidates.push_back(std::string("/data/payloads/") + p_Filename);

  for (int i = 0; i < 8; i++) {
    std::string s_UsbPath;
    if (Utility::IsJailbroken()) {
      Utility::StrSprintf(s_UsbPath, "/mnt/usb%i/payloads/%s", i, p_Filename.c_str());
    } else {
      Utility::StrSprintf(s_UsbPath, "/usb%i/payloads/%s", i, p_Filename.c_str());
    }
    s_Candidates.push_back(s_UsbPath);
  }

  for (size_t i = 0; i < s_Candidates.size(); i++) {
    FILE *s_FilePointer = std::fopen(s_Candidates[i].c_str(), "r");
    if (s_FilePointer) {
      std::fclose(s_FilePointer);
      return s_Candidates[i];
    }
  }

  return "";
}

bool LinuxView::IsFileNonEmpty(const std::string &p_Path) {
  FILE *s_FilePointer = std::fopen(p_Path.c_str(), "r");
  if (!s_FilePointer) {
    return false;
  }

  bool s_Result = true;
  if (std::fseek(s_FilePointer, 0, SEEK_END) != 0) {
    s_Result = false;
  } else if (std::ftell(s_FilePointer) <= 0) {
    s_Result = false;
  }

  std::fclose(s_FilePointer);
  return s_Result;
}

bool LinuxView::TryLaunch(const char *p_Filename) {
  std::string s_Path = ResolvePayloadPath(p_Filename);
  if (s_Path.empty() || !IsFileNonEmpty(s_Path)) {
    return false;
  }

  logKernel(LL_Debug, "Payload found: %s", s_Path.c_str());
  notifi(NULL, "Loading Linux payload:\n%s", s_Path.c_str());

  LaunchPayload(s_Path);
  return true;
}

void LinuxView::LaunchPayload(const std::string &p_Path) {
  logKernel(LL_Debug, "Loading Linux: %s", p_Path.c_str());

  if (!Utility::IsPS5()) {
    if (
      !Utility::SendPayloadSocket(m_App, "127.0.0.1", 9021, p_Path) && // Send to Mira loader
      !Utility::SendPayloadSocket(m_App, "127.0.0.1", 9090, p_Path)    // Send to GoldHEN loader
    ) {
      Utility::LaunchShellcode(m_App, p_Path); // Launch here
    }
  } else {
    if (
      !Utility::SendPayloadSocket(m_App, "127.0.0.1", 9020, p_Path) &&                // Send to default loader
      !Utility::SendPayloadSocket(m_App, "127.0.0.1", 9021, p_Path) &&                // Send to elfldr
      !Utility::SendPayloadPost(m_App, "http://127.0.0.1:8080/elfldr", p_Path, false) // Send to websrv elfldr
    ) {
      notifi(NULL, "Could not send payload to loader");
    }
  }

  m_Launched = true;
}

int LinuxView::Update() {
  if (m_OnError) {
    if (m_App->Ctrl->GetButtonPressed(ORBIS_PAD_BUTTON_CROSS)) {
      m_OnError = false;
    }
    return 0;
  }

  if (m_Launched) {
    return 0;
  }

  // Throttle re-scans while waiting for devices to appear
  if (m_ScanAttempts > 0) {
    if (m_LastScanTime + (2 * 1000000) > sceKernelGetProcessTime()) {
      return 0;
    }
  }

  // Search the well-known payload names in order and launch the first one found
  // PS4 file systems are case-sensitive, so try both capitalizations
  const char *s_PayloadNames[] = {"Linux-1gb.bin", "linux-1gb.bin", "Linux.bin", "linux.bin"};

  for (size_t i = 0; i < sizeof(s_PayloadNames) / sizeof(s_PayloadNames[0]); i++) {
    if (TryLaunch(s_PayloadNames[i])) {
      return 0;
    }
  }

  // Payload may live on USB that hasn't mounted yet; re-scan every 2s for ~30s
  m_ScanAttempts++;
  if (m_ScanAttempts <= 15) {
    m_LastScanTime = sceKernelGetProcessTime();
    return 0;
  }

  notifi(NULL, "No Linux payload found\n(/data/payloads/Linux-1gb.bin)");
  ShowError(std::string(m_App->Lang->Get("errorMetadataMissingPayloadFile") + std::string("\n\n") + m_App->Lang->Get("errorPathOutput")), "/data/payloads/Linux-1gb.bin");
  logKernel(LL_Debug, "%s", "No Linux payload found");

  return 0;
}

int LinuxView::Render() {
  int s_ScreenHeight = m_App->Graph->GetScreenHeight();
  int s_ScreenWidth = m_App->Graph->GetScreenWidth();

  // Draw plain background color
  m_App->Graph->DrawRectangle(0, 0, s_ScreenWidth, s_ScreenHeight, m_BackgroundColor);

  // Draw centered title
  std::string s_TitleName = m_App->Lang->Get("mainTitle");
  FontSize s_TitleSize;
  m_App->Graph->SetFontSize(m_App->Res->m_Typefaces, HEADER_FONT_SIZE);
  m_App->Graph->GetTextSize(s_TitleName, m_App->Res->m_Typefaces, &s_TitleSize);
  int s_TitleY = (HEADER_SIZE / 2) - (s_TitleSize.height / 2);
  m_App->Graph->DrawText(s_TitleName, m_App->Res->m_Typefaces, (s_ScreenWidth / 2) - (s_TitleSize.width / 2), s_TitleY, m_TextColor, m_TextColor);

  m_App->Graph->SetFontSize(m_App->Res->m_Typefaces, DEFAULT_FONT_SIZE);

  if (m_OnError) {
    FontSize s_ErrorSize;
    m_App->Graph->GetTextSize(m_ErrorMessage, m_App->Res->m_Typefaces, &s_ErrorSize);
    m_App->Graph->DrawText(m_ErrorMessage, m_App->Res->m_Typefaces, ((s_ScreenWidth / 2) - (s_ErrorSize.width / 2)), ((s_ScreenHeight / 2) - (s_ErrorSize.height / 2)), m_TextColor, m_TextColor);
  } else {
    std::string s_Status = m_App->Lang->Get(m_Launched ? "loadingTick3" : "loadingTick1");
    FontSize s_StatusSize;
    m_App->Graph->GetTextSize(s_Status, m_App->Res->m_Typefaces, &s_StatusSize);
    m_App->Graph->DrawText(s_Status, m_App->Res->m_Typefaces, (s_ScreenWidth / 2) - (s_StatusSize.width / 2), (s_ScreenHeight / 2) - (s_StatusSize.height / 2), m_TextColor, m_TextColor);
  }

  // Draw footer divider
  m_App->Graph->DrawRectangle(BORDER_X, s_ScreenHeight - FOOTER_SIZE + 5, s_ScreenWidth - (BORDER_X * 2), 5, m_DividerColor);

  m_App->Graph->SetFontSize(m_App->Res->m_Typefaces, FOOTER_FONT_SIZE);

  if (m_OnError) {
    std::string s_Ok = m_App->Lang->Get("ok");
    FontSize s_OkSize;
    m_App->Graph->GetTextSize(s_Ok, m_App->Res->m_Typefaces, &s_OkSize);

    int s_IconWidth = s_OkSize.width + FOOTER_ICON_SIZE + FOOTER_TEXT_BORDER;
    int s_IconX = (s_ScreenWidth - BORDER_X) - s_IconWidth;
    int s_IconY = (s_ScreenHeight - FOOTER_SIZE + 5) + FOOTER_BORDER_Y;
    int s_TextY = s_IconY + ((FOOTER_ICON_SIZE / 2) - (s_OkSize.height / 2));

    m_App->Graph->DrawSizedPNG(&m_App->Res->m_Cross, s_IconX, s_IconY, FOOTER_ICON_SIZE, FOOTER_ICON_SIZE);
    s_IconX += FOOTER_ICON_SIZE + FOOTER_TEXT_BORDER;
    m_App->Graph->DrawText(s_Ok, m_App->Res->m_Typefaces, s_IconX, s_TextY, m_TextColor, m_TextColor);
  }

  m_App->Graph->SetFontSize(m_App->Res->m_Typefaces, DEFAULT_FONT_SIZE);

  return 0;
}