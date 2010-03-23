// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "app/l10n_util.h"
#include "base/i18n/rtl.h"
#include "base/sys_info.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/app_modal_dialog.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_init.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/js_modal_dialog.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tabs/pinned_tab_codec.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/page_transition_types.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "net/base/mock_host_resolver.h"

const std::string BEFORE_UNLOAD_HTML =
    "<html><head><title>beforeunload</title></head><body>"
    "<script>window.onbeforeunload=function(e){return 'foo'}</script>"
    "</body></html>";

const std::wstring OPEN_NEW_BEFOREUNLOAD_PAGE =
    L"w=window.open(); w.onbeforeunload=function(e){return 'foo'};";

namespace {

// Given a page title, returns the expected window caption string.
std::wstring WindowCaptionFromPageTitle(std::wstring page_title) {
#if defined(OS_MACOSX) || defined(OS_CHROMEOS)
  // On Mac or ChromeOS, we don't want to suffix the page title with
  // the application name.
  if (page_title.empty())
    return l10n_util::GetString(IDS_BROWSER_WINDOW_MAC_TAB_UNTITLED);
  return page_title;
#else
  if (page_title.empty())
    return l10n_util::GetString(IDS_PRODUCT_NAME);

  return l10n_util::GetStringF(IDS_BROWSER_WINDOW_TITLE_FORMAT, page_title);
#endif
}

// Returns the number of active RenderProcessHosts.
int CountRenderProcessHosts() {
  int result = 0;
  for (RenderProcessHost::iterator i(RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance())
    ++result;
  return result;
}

class MockTabStripModelObserver : public TabStripModelObserver {
 public:
  MockTabStripModelObserver() : closing_count_(0) {}

  virtual void TabClosingAt(TabContents* contents, int index) {
    closing_count_++;
  }

  int closing_count() const { return closing_count_; }

 private:
  int closing_count_;

  DISALLOW_COPY_AND_ASSIGN(MockTabStripModelObserver);
};

}  // namespace

class BrowserTest : public ExtensionBrowserTest {
 public:
  // Used by phantom tab tests. Creates two tabs, pins the first and makes it
  // a phantom tab (by closing it).
  void PhantomTabTest() {
    HTTPTestServer* server = StartHTTPServer();
    ASSERT_TRUE(server);
    host_resolver()->AddRule("www.example.com", "127.0.0.1");
    GURL url(server->TestServerPage("empty.html"));
    TabStripModel* model = browser()->tabstrip_model();

    ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("app/")));

    Extension* app_extension = GetExtension();

    ui_test_utils::NavigateToURL(browser(), url);

    TabContents* app_contents = new TabContents(browser()->profile(), NULL,
                                                MSG_ROUTING_NONE, NULL);
    app_contents->SetAppExtension(app_extension);

    model->AddTabContents(app_contents, 0, false, 0, false);
    model->SetTabPinned(0, true);
    ui_test_utils::NavigateToURL(browser(), url);

    // Close the first, which should make it a phantom.
    model->CloseTabContentsAt(0);

    // There should still be two tabs.
    ASSERT_EQ(2, browser()->tab_count());
    // The first tab should be a phantom.
    EXPECT_TRUE(model->IsPhantomTab(0));
    // And the tab contents of the first tab should have changed.
    EXPECT_TRUE(model->GetTabContentsAt(0) != app_contents);
  }

 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) {
    ExtensionBrowserTest::SetUpCommandLine(command_line);

    // Needed for phantom tab tests.
    command_line->AppendSwitch(switches::kEnableExtensionApps);
  }

  // In RTL locales wrap the page title with RTL embedding characters so that it
  // matches the value returned by GetWindowTitle().
  std::wstring LocaleWindowCaptionFromPageTitle(
      const std::wstring& expected_title) {
    std::wstring page_title = WindowCaptionFromPageTitle(expected_title);
#if defined(OS_WIN)
    std::string locale = g_browser_process->GetApplicationLocale();
    if (base::i18n::GetTextDirectionForLocale(locale.c_str()) ==
        base::i18n::RIGHT_TO_LEFT) {
      base::i18n::WrapStringWithLTRFormatting(&page_title);
    }

    return page_title;
#else
    // Do we need to use the above code on POSIX as well?
    return page_title;
#endif
  }

  // Returns the app extension installed by PhantomTabTest.
  Extension* GetExtension() {
    const ExtensionList* extensions =
        browser()->profile()->GetExtensionsService()->extensions();
    for (size_t i = 0; i < extensions->size(); ++i) {
      if ((*extensions)[i]->name() == "App Test")
        return (*extensions)[i];
    }
    NOTREACHED();
    return NULL;
  }
};

// Launch the app on a page with no title, check that the app title was set
// correctly.
IN_PROC_BROWSER_TEST_F(BrowserTest, NoTitle) {
  ui_test_utils::NavigateToURL(browser(),
                               ui_test_utils::GetTestUrl(L".", L"title1.html"));
  EXPECT_EQ(LocaleWindowCaptionFromPageTitle(L"title1.html"),
            UTF16ToWideHack(browser()->GetWindowTitleForCurrentTab()));
  string16 tab_title;
  ASSERT_TRUE(ui_test_utils::GetCurrentTabTitle(browser(), &tab_title));
  EXPECT_EQ(ASCIIToUTF16("title1.html"), tab_title);
}

// Launch the app, navigate to a page with a title, check that the app title
// was set correctly.
IN_PROC_BROWSER_TEST_F(BrowserTest, Title) {
  ui_test_utils::NavigateToURL(browser(),
                               ui_test_utils::GetTestUrl(L".", L"title2.html"));
  const std::wstring test_title(L"Title Of Awesomeness");
  EXPECT_EQ(LocaleWindowCaptionFromPageTitle(test_title),
            UTF16ToWideHack(browser()->GetWindowTitleForCurrentTab()));
  string16 tab_title;
  ASSERT_TRUE(ui_test_utils::GetCurrentTabTitle(browser(), &tab_title));
  EXPECT_EQ(WideToUTF16(test_title), tab_title);
}

#if defined(OS_MACOSX)
// Test is crashing on Mac, see http://crbug.com/29424.
#define MAYBE_JavascriptAlertActivatesTab DISABLED_JavascriptAlertActivatesTab
#else
#define MAYBE_JavascriptAlertActivatesTab JavascriptAlertActivatesTab
#endif

IN_PROC_BROWSER_TEST_F(BrowserTest, MAYBE_JavascriptAlertActivatesTab) {
  GURL url(ui_test_utils::GetTestUrl(L".", L"title1.html"));
  ui_test_utils::NavigateToURL(browser(), url);
  browser()->AddTabWithURL(url, GURL(), PageTransition::TYPED,
                           true, 0, false, NULL);
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_EQ(0, browser()->selected_index());
  TabContents* second_tab = browser()->GetTabContentsAt(1);
  ASSERT_TRUE(second_tab);
  second_tab->render_view_host()->ExecuteJavascriptInWebFrame(L"",
      L"alert('Activate!');");
  AppModalDialog* alert = ui_test_utils::WaitForAppModalDialog();
  alert->CloseModalDialog();
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_EQ(1, browser()->selected_index());
}

// Create 34 tabs and verify that a lot of processes have been created. The
// exact number of processes depends on the amount of memory. Previously we
// had a hard limit of 31 processes and this test is mainly directed at
// verifying that we don't crash when we pass this limit.
IN_PROC_BROWSER_TEST_F(BrowserTest, ThirtyFourTabs) {
  GURL url(ui_test_utils::GetTestUrl(L".", L"title2.html"));

  // There is one initial tab.
  for (int ix = 0; ix != 33; ++ix) {
    browser()->AddTabWithURL(url, GURL(), PageTransition::TYPED,
                             true, 0, false, NULL);
  }
  EXPECT_EQ(34, browser()->tab_count());

  // See browser\renderer_host\render_process_host.cc for the algorithm to
  // decide how many processes to create.
  if (base::SysInfo::AmountOfPhysicalMemoryMB() >= 2048) {
    EXPECT_GE(CountRenderProcessHosts(), 24);
  } else {
    EXPECT_LE(CountRenderProcessHosts(), 23);
  }
}

// Test for crbug.com/22004.  Reloading a page with a before unload handler and
// then canceling the dialog should not leave the throbber spinning.
IN_PROC_BROWSER_TEST_F(BrowserTest, ReloadThenCancelBeforeUnload) {
  GURL url("data:text/html," + BEFORE_UNLOAD_HTML);
  ui_test_utils::NavigateToURL(browser(), url);

  // Navigate to another page, but click cancel in the dialog.  Make sure that
  // the throbber stops spinning.
  browser()->Reload();
  AppModalDialog* alert = ui_test_utils::WaitForAppModalDialog();
  alert->CloseModalDialog();
  EXPECT_FALSE(browser()->GetSelectedTabContents()->is_loading());

  // Clear the beforeunload handler so the test can easily exit.
  browser()->GetSelectedTabContents()->render_view_host()->
      ExecuteJavascriptInWebFrame(L"", L"onbeforeunload=null;");
}

// Test for crbug.com/11647.  A page closed with window.close() should not have
// two beforeunload dialogs shown.
// Flaky: see http://crbug.com/27039
IN_PROC_BROWSER_TEST_F(BrowserTest, FLAKY_SingleBeforeUnloadAfterWindowClose) {
  browser()->GetSelectedTabContents()->render_view_host()->
      ExecuteJavascriptInWebFrame(L"", OPEN_NEW_BEFOREUNLOAD_PAGE);

  // Close the new window with JavaScript, which should show a single
  // beforeunload dialog.  Then show another alert, to make it easy to verify
  // that a second beforeunload dialog isn't shown.
  browser()->GetTabContentsAt(0)->render_view_host()->
      ExecuteJavascriptInWebFrame(L"", L"w.close(); alert('bar');");
  AppModalDialog* alert = ui_test_utils::WaitForAppModalDialog();
  alert->AcceptWindow();

  alert = ui_test_utils::WaitForAppModalDialog();
  EXPECT_FALSE(static_cast<JavaScriptAppModalDialog*>(alert)->
                   is_before_unload_dialog());
  alert->AcceptWindow();
}

// Test that get_process_idle_time() returns reasonable values when compared
// with time deltas measured locally.
IN_PROC_BROWSER_TEST_F(BrowserTest, RenderIdleTime) {
  base::TimeTicks start = base::TimeTicks::Now();
  ui_test_utils::NavigateToURL(browser(),
                               ui_test_utils::GetTestUrl(L".", L"title1.html"));
  RenderProcessHost::iterator it(RenderProcessHost::AllHostsIterator());
  for (; !it.IsAtEnd(); it.Advance()) {
    base::TimeDelta renderer_td =
        it.GetCurrentValue()->get_child_process_idle_time();
    base::TimeDelta browser_td = base::TimeTicks::Now() - start;
    EXPECT_TRUE(browser_td >= renderer_td);
  }
}

// Test IDC_CREATE_SHORTCUTS command is enabled for url scheme file, ftp, http
// and https and disabled for chrome://, about:// etc.
// TODO(pinkerton): Disable app-mode in the model until we implement it
// on the Mac. http://crbug.com/13148
#if !defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(BrowserTest, CommandCreateAppShortcut) {
  static const wchar_t kDocRoot[] = L"chrome/test/data";

  CommandUpdater* command_updater = browser()->command_updater();

  // Urls that are okay to have shortcuts.
  GURL file_url(ui_test_utils::GetTestUrl(L".", L"empty.html"));
  ASSERT_TRUE(file_url.SchemeIs(chrome::kFileScheme));
  ui_test_utils::NavigateToURL(browser(), file_url);
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_CREATE_SHORTCUTS));

  scoped_refptr<FTPTestServer> ftp_server(
        FTPTestServer::CreateServer(kDocRoot));
  ASSERT_TRUE(NULL != ftp_server.get());
  GURL ftp_url(ftp_server->TestServerPage(""));
  ASSERT_TRUE(ftp_url.SchemeIs(chrome::kFtpScheme));
  ui_test_utils::NavigateToURL(browser(), ftp_url);
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_CREATE_SHORTCUTS));

  scoped_refptr<HTTPTestServer> http_server(
        HTTPTestServer::CreateServer(kDocRoot, NULL));
  ASSERT_TRUE(NULL != http_server.get());
  GURL http_url(http_server->TestServerPage(""));
  ASSERT_TRUE(http_url.SchemeIs(chrome::kHttpScheme));
  ui_test_utils::NavigateToURL(browser(), http_url);
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_CREATE_SHORTCUTS));

  scoped_refptr<HTTPSTestServer> https_server(
        HTTPSTestServer::CreateGoodServer(kDocRoot));
  ASSERT_TRUE(NULL != https_server.get());
  GURL https_url(https_server->TestServerPage("/"));
  ASSERT_TRUE(https_url.SchemeIs(chrome::kHttpsScheme));
  ui_test_utils::NavigateToURL(browser(), https_url);
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_CREATE_SHORTCUTS));

  // Urls that should not have shortcuts.
  GURL new_tab_url(chrome::kChromeUINewTabURL);
  ui_test_utils::NavigateToURL(browser(), new_tab_url);
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_CREATE_SHORTCUTS));

  GURL history_url(chrome::kChromeUIHistoryURL);
  ui_test_utils::NavigateToURL(browser(), history_url);
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_CREATE_SHORTCUTS));

  GURL downloads_url(chrome::kChromeUIDownloadsURL);
  ui_test_utils::NavigateToURL(browser(), downloads_url);
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_CREATE_SHORTCUTS));

  GURL blank_url(chrome::kAboutBlankURL);
  ui_test_utils::NavigateToURL(browser(), blank_url);
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_CREATE_SHORTCUTS));
}
#endif

// Test RenderView correctly send back favicon url for web page that redirects
// to an anchor in javascript body.onload handler.
IN_PROC_BROWSER_TEST_F(BrowserTest, FaviconOfOnloadRedirectToAnchorPage) {
  static const wchar_t kDocRoot[] = L"chrome/test/data";
  scoped_refptr<HTTPTestServer> server(
        HTTPTestServer::CreateServer(kDocRoot, NULL));
  ASSERT_TRUE(NULL != server.get());
  GURL url(server->TestServerPage("files/onload_redirect_to_anchor.html"));
  GURL expected_favicon_url(server->TestServerPage("files/test.png"));

  ui_test_utils::NavigateToURL(browser(), url);

  NavigationEntry* entry = browser()->GetSelectedTabContents()->
      controller().GetActiveEntry();
  EXPECT_EQ(expected_favicon_url.spec(), entry->favicon().url().spec());
}

// TODO(sky): get these to run on a Mac.
#if !defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(BrowserTest, PhantomTab) {
  PhantomTabTest();
}

IN_PROC_BROWSER_TEST_F(BrowserTest, RevivePhantomTab) {
  PhantomTabTest();

  if (HasFatalFailure())
    return;

  TabStripModel* model = browser()->tabstrip_model();

  // Revive the phantom tab by selecting it.
  browser()->SelectTabContentsAt(0, true);

  // There should still be two tabs.
  ASSERT_EQ(2, browser()->tab_count());
  // The first tab should no longer be a phantom.
  EXPECT_FALSE(model->IsPhantomTab(0));
}

// Makes sure TabClosing is sent when uninstalling an extension that is an app
// tab.
IN_PROC_BROWSER_TEST_F(BrowserTest, TabClosingWhenRemovingExtension) {
  HTTPTestServer* server = StartHTTPServer();
  ASSERT_TRUE(server);
  host_resolver()->AddRule("www.example.com", "127.0.0.1");
  GURL url(server->TestServerPage("empty.html"));
  TabStripModel* model = browser()->tabstrip_model();

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("app/")));

  Extension* app_extension = GetExtension();

  ui_test_utils::NavigateToURL(browser(), url);

  TabContents* app_contents = new TabContents(browser()->profile(), NULL,
                                              MSG_ROUTING_NONE, NULL);
  app_contents->SetAppExtension(app_extension);

  model->AddTabContents(app_contents, 0, false, 0, false);
  model->SetTabPinned(0, true);
  ui_test_utils::NavigateToURL(browser(), url);

  MockTabStripModelObserver observer;
  model->AddObserver(&observer);

  // Uninstall the extension and make sure TabClosing is sent.
  ExtensionsService* service = browser()->profile()->GetExtensionsService();
  service->UninstallExtension(GetExtension()->id(), false);
  EXPECT_EQ(1, observer.closing_count());

  model->RemoveObserver(&observer);

  // There should only be one tab now.
  ASSERT_EQ(1, browser()->tab_count());
}

IN_PROC_BROWSER_TEST_F(BrowserTest, AppTabRemovedWhenExtensionUninstalled) {
  PhantomTabTest();

  Extension* extension = GetExtension();
  UninstallExtension(extension->id());

  // The uninstall should have removed the tab.
  ASSERT_EQ(1, browser()->tab_count());
}

#endif

// Tests that the CLD (Compact Language Detection) works properly.
IN_PROC_BROWSER_TEST_F(BrowserTest, PageLanguageDetection) {
  static const wchar_t kDocRoot[] = L"chrome/test/data";
  scoped_refptr<HTTPTestServer> server(
        HTTPTestServer::CreateServer(kDocRoot, NULL));
  ASSERT_TRUE(NULL != server.get());

  TabContents* current_tab = browser()->GetSelectedTabContents();

  // Navigate to a page in English.
  ui_test_utils::NavigateToURL(
      browser(), GURL(server->TestServerPage("files/english_page.html")));
  EXPECT_TRUE(current_tab->language_state().original_language().empty());
  std::string lang = ui_test_utils::WaitForLanguageDetection(current_tab);
  EXPECT_EQ("en", lang);
  EXPECT_EQ("en", current_tab->language_state().original_language());

  // Now navigate to a page in French.
  ui_test_utils::NavigateToURL(
      browser(), GURL(server->TestServerPage("files/french_page.html")));
  EXPECT_TRUE(current_tab->language_state().original_language().empty());
  lang = ui_test_utils::WaitForLanguageDetection(current_tab);
  EXPECT_EQ("fr", lang);
  EXPECT_EQ("fr", current_tab->language_state().original_language());
}

// Chromeos defaults to restoring the last session, so this test isn't
// applicable.
#if !defined(OS_CHROMEOS)
#if defined(OS_MACOSX)
// http://crbug.com/38522
#define RestorePinnedTabs FLAKY_RestorePinnedTabs
#endif
// Makes sure pinned tabs are restored correctly on start.
IN_PROC_BROWSER_TEST_F(BrowserTest, RestorePinnedTabs) {
  HTTPTestServer* server = StartHTTPServer();
  ASSERT_TRUE(server);

  // Add an pinned app tab.
  host_resolver()->AddRule("www.example.com", "127.0.0.1");
  GURL url(server->TestServerPage("empty.html"));
  TabStripModel* model = browser()->tabstrip_model();
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("app/")));
  Extension* app_extension = GetExtension();
  ui_test_utils::NavigateToURL(browser(), url);
  TabContents* app_contents = new TabContents(browser()->profile(), NULL,
                                              MSG_ROUTING_NONE, NULL);
  app_contents->SetAppExtension(app_extension);
  model->AddTabContents(app_contents, 0, false, 0, false);
  model->SetTabPinned(0, true);
  ui_test_utils::NavigateToURL(browser(), url);

  // Add a non pinned tab.
  browser()->NewTab();

  // Add a pinned non-app tab.
  browser()->NewTab();
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  model->SetTabPinned(2, true);

  // Write out the pinned tabs.
  PinnedTabCodec::WritePinnedTabs(browser()->profile());

  // Simulate launching again.
  CommandLine dummy(CommandLine::ARGUMENTS_ONLY);
  BrowserInit::LaunchWithProfile launch(std::wstring(), dummy);
  launch.profile_ = browser()->profile();
  launch.OpenStartupURLs(std::vector<GURL>());

  // The launch should have created a new browser.
  ASSERT_EQ(2u, BrowserList::GetBrowserCount(browser()->profile()));

  // Find the new browser.
  Browser* new_browser = NULL;
  for (BrowserList::const_iterator i = BrowserList::begin();
       i != BrowserList::end() && !new_browser; ++i) {
    if (*i != browser())
      new_browser = *i;
  }
  ASSERT_TRUE(new_browser);
  ASSERT_TRUE(new_browser != browser());

  // We should get back an additional tab for the app.
  ASSERT_EQ(2, new_browser->tab_count());

  // Make sure the state matches.
  TabStripModel* new_model = new_browser->tabstrip_model();
  EXPECT_TRUE(new_model->IsAppTab(0));
  EXPECT_FALSE(new_model->IsAppTab(1));

  EXPECT_TRUE(new_model->IsTabPinned(0));
  EXPECT_TRUE(new_model->IsTabPinned(1));

  EXPECT_TRUE(new_model->GetTabContentsAt(0)->app_extension() ==
              app_extension);
}
#endif
