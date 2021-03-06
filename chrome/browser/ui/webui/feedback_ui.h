// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_FEEDBACK_UI_H_
#define CHROME_BROWSER_UI_WEBUI_FEEDBACK_UI_H_

#include <string>

#include "chrome/browser/ui/webui/web_dialog_ui.h"

class Browser;

namespace browser {
void ShowWebFeedbackView(Browser* browser,
                         const std::string& description_template,
                         const std::string& category_tag);
}  // namespace browser

class FeedbackUI : public WebDialogUI {
 public:
  explicit FeedbackUI(content::WebUI* web_ui);

#if defined(OS_CHROMEOS)
  static void GetMostRecentScreenshots(
      const FilePath& filepath,
      std::vector<std::string>* saved_screenshots,
      size_t max_saved);
#endif

 private:
  DISALLOW_COPY_AND_ASSIGN(FeedbackUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_FEEDBACK_UI_H_
