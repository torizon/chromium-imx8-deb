// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/accessibility/ui/accessibility_confirmation_dialog.h"
#include "ash/shell.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/dictation_bubble_test_helper.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/test/browser_test.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/ui_base_features.h"

namespace ash {

using ContextType = ::extensions::ExtensionBrowserTest::ContextType;

class AccessibilityPrivateApiTest
    : public extensions::ExtensionApiTest,
      public testing::WithParamInterface<ContextType> {
 public:
  AccessibilityPrivateApiTest() : ExtensionApiTest(GetParam()) {}
  ~AccessibilityPrivateApiTest() override = default;
  AccessibilityPrivateApiTest& operator=(const AccessibilityPrivateApiTest&) =
      delete;
  AccessibilityPrivateApiTest(const AccessibilityPrivateApiTest&) = delete;

 protected:
  // ExtensionApiTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    scoped_feature_list_.InitAndEnableFeature(
        ::features::kExperimentalAccessibilityDictationCommands);
  }

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    dictation_bubble_test_helper_ =
        std::make_unique<DictationBubbleTestHelper>();
  }

  [[nodiscard]] bool RunSubtest(const char* subtest) {
    return RunExtensionTest("accessibility_private", {.custom_arg = subtest});
  }

  DictationBubbleTestHelper* dictation_bubble_test_helper() {
    return dictation_bubble_test_helper_.get();
  }

 private:
  std::unique_ptr<DictationBubbleTestHelper> dictation_bubble_test_helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(AccessibilityPrivateApiTest, SendSyntheticKeyEvent) {
  ASSERT_TRUE(RunSubtest("testSendSyntheticKeyEvent")) << message_;
}

IN_PROC_BROWSER_TEST_P(AccessibilityPrivateApiTest,
                       GetDisplayNameForLocaleTest) {
  ASSERT_TRUE(RunSubtest("testGetDisplayNameForLocale")) << message_;
}

IN_PROC_BROWSER_TEST_P(AccessibilityPrivateApiTest, OpenSettingsSubpage) {
  Profile* profile = AccessibilityManager::Get()->profile();

  // Install the Settings App.
  web_app::WebAppProvider::GetForTest(profile)
      ->system_web_app_manager()
      .InstallSystemAppsForTesting();

  ASSERT_TRUE(RunSubtest("testOpenSettingsSubpage")) << message_;

  chrome::SettingsWindowManager* settings_manager =
      chrome::SettingsWindowManager::GetInstance();

  Browser* settings_browser = settings_manager->FindBrowserForProfile(profile);
  EXPECT_NE(nullptr, settings_browser);

  content::WebContents* web_contents =
      settings_browser->tab_strip_model()->GetWebContentsAt(0);

  EXPECT_TRUE(WaitForLoadStop(web_contents));

  EXPECT_EQ(GURL(chrome::GetOSSettingsUrl("manageAccessibility/tts")),
            web_contents->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_P(AccessibilityPrivateApiTest,
                       OpenSettingsSubpage_InvalidSubpage) {
  Profile* profile = AccessibilityManager::Get()->profile();

  // Install the Settings App.
  web_app::WebAppProvider::GetForTest(profile)
      ->system_web_app_manager()
      .InstallSystemAppsForTesting();

  ASSERT_TRUE(RunSubtest("testOpenSettingsSubpageInvalidSubpage")) << message_;

  chrome::SettingsWindowManager* settings_manager =
      chrome::SettingsWindowManager::GetInstance();

  // Invalid subpage should not open settings window.
  Browser* settings_browser = settings_manager->FindBrowserForProfile(profile);
  EXPECT_EQ(nullptr, settings_browser);
}

template <bool enabled>
class AccessibilityPrivateApiFeatureTest : public AccessibilityPrivateApiTest {
 public:
  AccessibilityPrivateApiFeatureTest() = default;
  ~AccessibilityPrivateApiFeatureTest() override = default;
  AccessibilityPrivateApiFeatureTest& operator=(
      const AccessibilityPrivateApiFeatureTest&) = delete;
  AccessibilityPrivateApiFeatureTest(
      const AccessibilityPrivateApiFeatureTest&) = delete;

  // AccessibilityPrivateApiTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    AccessibilityPrivateApiTest::SetUpCommandLine(command_line);
    if (enabled) {
      scoped_feature_list_.InitAndEnableFeature(
          ::features::kEnhancedNetworkVoices);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          ::features::kEnhancedNetworkVoices);
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

using AccessibilityPrivateApiFeatureDisabledTest =
    AccessibilityPrivateApiFeatureTest<false>;
using AccessibilityPrivateApiFeatureEnabledTest =
    AccessibilityPrivateApiFeatureTest<true>;

IN_PROC_BROWSER_TEST_P(AccessibilityPrivateApiFeatureDisabledTest,
                       IsFeatureEnabled_FeatureDisabled) {
  ASSERT_TRUE(RunSubtest("testFeatureDisabled")) << message_;
}

IN_PROC_BROWSER_TEST_P(AccessibilityPrivateApiFeatureEnabledTest,
                       IsFeatureEnabled_FeatureEnabled) {
  ASSERT_TRUE(RunSubtest("testFeatureEnabled")) << message_;
}

IN_PROC_BROWSER_TEST_P(AccessibilityPrivateApiTest, IsFeatureUnknown) {
  ASSERT_TRUE(RunSubtest("testFeatureUnknown")) << message_;
}

IN_PROC_BROWSER_TEST_P(AccessibilityPrivateApiTest, AcceptConfirmationDialog) {
  ASSERT_TRUE(RunSubtest("testAcceptConfirmationDialog")) << message_;

  // The test has requested to open the confirmation dialog. Check that
  // it was created, then confirm it.
  AccessibilityConfirmationDialog* dialog_ =
      Shell::Get()->accessibility_controller()->GetConfirmationDialogForTest();
  ASSERT_NE(dialog_, nullptr);

  EXPECT_EQ(dialog_->GetWindowTitle(), u"Confirm me! 🐶");

  // Accept the dialog and wait for the JS test to get the confirmation.
  extensions::ResultCatcher catcher;
  dialog_->Accept();
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_P(AccessibilityPrivateApiTest, CancelConfirmationDialog) {
  ASSERT_TRUE(RunSubtest("testCancelConfirmationDialog")) << message_;

  // The test has requested to open the confirmation dialog. Check that
  // it was created, then cancel it.
  AccessibilityConfirmationDialog* dialog_ =
      Shell::Get()->accessibility_controller()->GetConfirmationDialogForTest();
  ASSERT_NE(dialog_, nullptr);

  EXPECT_EQ(dialog_->GetWindowTitle(), u"Cancel me!");

  // Cancel the dialog and wait for the JS test to get the callback.
  extensions::ResultCatcher catcher;
  dialog_->Cancel();
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_P(AccessibilityPrivateApiTest, CloseConfirmationDialog) {
  ASSERT_TRUE(RunSubtest("testCancelConfirmationDialog")) << message_;

  // The test has requested to open the confirmation dialog. Check that
  // it was created, then close it.
  AccessibilityConfirmationDialog* dialog_ =
      Shell::Get()->accessibility_controller()->GetConfirmationDialogForTest();
  ASSERT_TRUE(dialog_ != nullptr);

  EXPECT_EQ(dialog_->GetWindowTitle(), u"Cancel me!");

  // Close the dialog and wait for the JS test to get the callback.
  extensions::ResultCatcher catcher;
  dialog_->Close();
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_P(AccessibilityPrivateApiTest, UpdateDictationBubble) {
  // Enable Dictation to allow the API to work.
  Shell::Get()->accessibility_controller()->dictation().SetEnabled(true);

  // This test requires some back and forth communication between C++ and JS.
  // Use message listeners to force the synchronicity of this test.
  ExtensionTestMessageListener standby_listener("Standby", /*will_reply=*/true);
  ExtensionTestMessageListener show_text_listener("Show text",
                                                  /*will_reply=*/true);
  ExtensionTestMessageListener macro_success_listener("Show macro success",
                                                      /*will_reply=*/true);
  ExtensionTestMessageListener reset_listener("Reset", /*will_reply=*/true);
  ExtensionTestMessageListener hide_listener("Hide", /*will_reply=*/false);

  extensions::ResultCatcher result_catcher;
  ASSERT_TRUE(RunSubtest("testUpdateDictationBubble")) << message_;

  ASSERT_TRUE(standby_listener.WaitUntilSatisfied());
  EXPECT_TRUE(dictation_bubble_test_helper()->IsVisible());
  EXPECT_EQ(std::u16string(), dictation_bubble_test_helper()->GetText());
  EXPECT_EQ(DictationBubbleIconType::kStandby,
            dictation_bubble_test_helper()->GetVisibleIcon());
  standby_listener.Reply("Continue");

  ASSERT_TRUE(show_text_listener.WaitUntilSatisfied());
  EXPECT_TRUE(dictation_bubble_test_helper()->IsVisible());
  EXPECT_EQ(u"Hello", dictation_bubble_test_helper()->GetText());
  EXPECT_EQ(DictationBubbleIconType::kHidden,
            dictation_bubble_test_helper()->GetVisibleIcon());
  show_text_listener.Reply("Continue");

  ASSERT_TRUE(macro_success_listener.WaitUntilSatisfied());
  EXPECT_TRUE(dictation_bubble_test_helper()->IsVisible());
  EXPECT_EQ(u"Hello", dictation_bubble_test_helper()->GetText());
  EXPECT_EQ(DictationBubbleIconType::kMacroSuccess,
            dictation_bubble_test_helper()->GetVisibleIcon());
  macro_success_listener.Reply("Continue");

  ASSERT_TRUE(reset_listener.WaitUntilSatisfied());
  EXPECT_TRUE(dictation_bubble_test_helper()->IsVisible());
  EXPECT_EQ(std::u16string(), dictation_bubble_test_helper()->GetText());
  EXPECT_EQ(DictationBubbleIconType::kStandby,
            dictation_bubble_test_helper()->GetVisibleIcon());
  reset_listener.Reply("Continue");

  ASSERT_TRUE(hide_listener.WaitUntilSatisfied());
  EXPECT_FALSE(dictation_bubble_test_helper()->IsVisible());
  EXPECT_EQ(std::u16string(), dictation_bubble_test_helper()->GetText());
  EXPECT_EQ(DictationBubbleIconType::kHidden,
            dictation_bubble_test_helper()->GetVisibleIcon());

  ASSERT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

IN_PROC_BROWSER_TEST_P(AccessibilityPrivateApiTest,
                       UpdateDictationBubbleWithHints) {
  Shell::Get()->accessibility_controller()->dictation().SetEnabled(true);
  ExtensionTestMessageListener show_listener("Some hints", /*will_reply=*/true);
  ExtensionTestMessageListener no_hints_listener("No hints",
                                                 /*will_reply=*/false);
  extensions::ResultCatcher result_catcher;
  ASSERT_TRUE(RunSubtest("testUpdateDictationBubbleWithHints")) << message_;

  ASSERT_TRUE(show_listener.WaitUntilSatisfied());
  EXPECT_TRUE(dictation_bubble_test_helper()->IsVisible());
  EXPECT_TRUE(dictation_bubble_test_helper()->HasVisibleHints(
      std::vector<std::u16string>{u"Try saying:", u"\"Type [word / phrase]\"",
                                  u"\"Help\""}));
  show_listener.Reply("Continue");

  ASSERT_TRUE(no_hints_listener.WaitUntilSatisfied());
  EXPECT_TRUE(dictation_bubble_test_helper()->IsVisible());
  EXPECT_TRUE(dictation_bubble_test_helper()->HasVisibleHints(
      std::vector<std::u16string>()));

  ASSERT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         AccessibilityPrivateApiTest,
                         ::testing::Values(ContextType::kPersistentBackground));
INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         AccessibilityPrivateApiFeatureDisabledTest,
                         ::testing::Values(ContextType::kPersistentBackground));
INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         AccessibilityPrivateApiFeatureEnabledTest,
                         ::testing::Values(ContextType::kPersistentBackground));
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         AccessibilityPrivateApiTest,
                         ::testing::Values(ContextType::kServiceWorker));
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         AccessibilityPrivateApiFeatureDisabledTest,
                         ::testing::Values(ContextType::kServiceWorker));
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         AccessibilityPrivateApiFeatureEnabledTest,
                         ::testing::Values(ContextType::kServiceWorker));

}  // namespace ash
