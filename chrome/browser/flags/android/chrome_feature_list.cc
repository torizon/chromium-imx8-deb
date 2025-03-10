// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/flags/android/chrome_feature_list.h"

#include <stddef.h>

#include <string>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/feature_guide/notifications/feature_notification_guide_service.h"
#include "chrome/browser/flags/android/chrome_session_state.h"
#include "chrome/browser/flags/jni_headers/ChromeFeatureList_jni.h"
#include "chrome/browser/notifications/chime/android/features.h"
#include "chrome/browser/performance_hints/performance_hints_features.h"
#include "chrome/browser/push_messaging/push_messaging_features.h"
#include "chrome/browser/share/share_features.h"
#include "chrome/browser/sharing/shared_clipboard/feature_flags.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/video_tutorials/switches.h"
#include "chrome/common/chrome_features.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/browser_ui/photo_picker/android/features.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/content_creation/notes/core/note_features.h"
#include "components/content_creation/reactions/core/reactions_features.h"
#include "components/content_settings/core/common/features.h"
#include "components/download/public/common/download_features.h"
#include "components/embedder_support/android/util/cdn_utils.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/feed/feed_feature_list.h"
#include "components/history_clusters/core/features.h"
#include "components/invalidation/impl/invalidation_switches.h"
#include "components/language/core/common/language_experiments.h"
#include "components/messages/android/messages_feature.h"
#include "components/ntp_tiles/features.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/page_info/core/features.h"
#include "components/paint_preview/features/features.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/permissions/features.h"
#include "components/policy/core/common/features.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/query_tiles/switches.h"
#include "components/reading_list/features/reading_list_switches.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/security_state/core/features.h"
#include "components/send_tab_to_self/features.h"
#include "components/shared_highlighting/core/common/shared_highlighting_features.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/sync/base/features.h"
#include "components/webapps/browser/android/features.h"
#include "content/public/common/content_features.h"
#include "device/fido/features.h"
#include "media/base/media_switches.h"
#include "services/device/public/cpp/device_features.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/ui_base_features.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace chrome {
namespace android {

namespace {

// Array of features exposed through the Java ChromeFeatureList API. Entries in
// this array may either refer to features defined in the header of this file or
// in other locations in the code base (e.g. chrome/, components/, etc).
const base::Feature* const kFeaturesExposedToJava[] = {
    &autofill::features::kAutofillAddressProfileSavePromptNicknameSupport,
    &autofill::features::kAutofillCreditCardAuthentication,
    &autofill::features::kAutofillEnableManualFallbackForVirtualCards,
    &autofill::features::kAutofillKeyboardAccessory,
    &autofill::features::kAutofillManualFallbackAndroid,
    &autofill::features::kAutofillRefreshStyleAndroid,
    &autofill::features::kAutofillEnableGoogleIssuedCard,
    &autofill::features::kAutofillEnableSupportForHonorificPrefixes,
    &autofill::features::kAutofillEnableSupportForMoreStructureInAddresses,
    &autofill::features::kAutofillEnableSupportForMoreStructureInNames,
    &autofill::features::kAutofillEnableUpdateVirtualCardEnrollment,
    &blink::features::kPrerender2,
    &blink::features::kForceWebContentsDarkMode,
    &commerce::kCommerceMerchantViewer,
    &commerce::kCommercePriceTracking,
    &commerce::kShoppingList,
    &commerce::kShoppingPDPMetrics,
    &content_creation::kLightweightReactions,
    &content_settings::kDarkenWebsitesCheckboxInThemesSetting,
    &device::kWebAuthPhoneSupport,
    &download::features::kDownloadAutoResumptionNative,
    &download::features::kDownloadLater,
    &download::features::kIncognitoDownloadsWarning,
    &download::features::kSmartSuggestionForLargeDownloads,
    &download::features::kUseDownloadOfflineContentProvider,
    &embedder_support::kShowTrustedPublisherURL,
    &features::kAnonymousUpdateChecks,
    &features::kContinuousSearch,
    &features::kEarlyLibraryLoad,
    &features::kGenericSensorExtraClasses,
    &features::kHttpsOnlyMode,
    &features::kMetricsSettingsAndroid,
    &features::kNetworkServiceInProcess,
    &features::kPredictivePrefetchingAllowedOnAllConnectionTypes,
    &shared_highlighting::kPreemptiveLinkToTextGeneration,
    &shared_highlighting::kSharedHighlightingV2,
    &shared_highlighting::kSharedHighlightingAmp,
    &features::kElasticOverscroll,
    &features::kElidePrioritizationOfPreNativeBootstrapTasks,
    &features::kElideTabPreloadAtStartup,
    &features::kGiveJavaUiThreadDefaultTaskTraitsUserBlockingPriority,
    &features::kPrivacyGuide,
    &features::kPushMessagingDisallowSenderIDs,
    &features::kPwaUpdateDialogForIcon,
    &features::kPwaUpdateDialogForName,
    &features::kQuietNotificationPrompts,
    &features::kRequestDesktopSiteForTablets,
    &features::kToolbarUseHardwareBitmapDraw,
    &features::kWebNfc,
    &features::kIncognitoNtpRevamp,
    &feature_engagement::kEnableAutomaticSnooze,
    &feature_engagement::kIPHHomepagePromoCardFeature,
    &feature_engagement::kIPHNewTabPageHomeButtonFeature,
    &feature_engagement::kIPHSnooze,
    &feature_engagement::kIPHTabSwitcherButtonFeature,
    &feature_engagement::kUseClientConfigIPH,
    &feature_guide::features::kFeatureNotificationGuide,
    &feature_guide::features::kSkipCheckForLowEngagedUsers,
    &feed::kFeedBackToTop,
    &feed::kFeedClearImageMemoryCache,
    &feed::kFeedImageMemoryCacheSizePercentage,
    &feed::kFeedInteractiveRefresh,
    &feed::kFeedLoadingPlaceholder,
    &feed::kInterestFeedContentSuggestions,
    &feed::kInterestFeedSpinnerAlwaysAnimate,
    &feed::kInterestFeedV1ClicksAndViewsConditionalUpload,
    &feed::kInterestFeedV2,
    &feed::kInterestFeedV2Autoplay,
    &feed::kInterestFeedV2Hearts,
    &feed::kReliabilityLogging,
    &feed::kWebFeed,
    &feed::kWebFeedOnboarding,
    &feed::kWebFeedSort,
    &feed::kXsurfaceMetricsReporting,
    &history_clusters::internal::kJourneys,
    &kAdaptiveButtonInTopToolbar,
    &kAdaptiveButtonInTopToolbarCustomizationV2,
    &kAddToHomescreenIPH,
    &kAllowNewIncognitoTabIntents,
    &kAndroidLayoutChangeTabReparenting,
    &kAndroidSearchEngineChoiceNotification,
    &kAssistantConsentModal,
    &kAssistantConsentSimplifiedText,
    &kAssistantConsentV2,
    &kAssistantIntentExperimentId,
    &kAssistantIntentPageUrl,
    &kAssistantIntentTranslateInfo,
    &kAppLaunchpad,
    &kAppMenuMobileSiteOption,
    &kAppToWebAttribution,
    &kBackgroundThreadPool,
    &kBookmarkBottomSheet,
    &kCastDeviceFilter,
    &kCloseAllTabsModalDialog,
    &kCloseTabSuggestions,
    &kCriticalPersistedTabData,
    &kCCTBackgroundTab,
    &kCCTClientDataHeader,
    &kCCTExternalLinkHandling,
    &kCCTIncognito,
    &kCCTIncognitoAvailableToThirdParty,
    &kCCTNewDownloadTab,
    &kCCTPostMessageAPI,
    &kCCTRedirectPreconnect,
    &kCCTRemoveRemoteViewIds,
    &kCCTReportParallelRequestStatus,
    &kCCTResizable90MaximumHeight,
    &kCCTResizableAllowResizeByUserGesture,
    &kCCTResizableForFirstParties,
    &kCCTResizableForThirdParties,
    &kCCTResourcePrefetch,
    &kDontAutoHideBrowserControls,
    &kChromeNewDownloadTab,
    &kChromeShareLongScreenshot,
    &kChromeSharingHub,
    &kChromeSharingHubLaunchAdjacent,
    &kChromeSurveyNextAndroid,
    &kCommandLineOnNonRooted,
    &kConditionalTabStripAndroid,
    &kContextMenuEnableLensShoppingAllowlist,
    &kContextMenuGoogleLensChip,
    &kContextMenuSearchWithGoogleLens,
    &kContextMenuShopWithGoogleLens,
    &kContextMenuSearchAndShopWithGoogleLens,
    &kContextMenuTranslateWithGoogleLens,
    &kContextMenuPopupStyle,
    &kContextualSearchDebug,
    &kContextualSearchDelayedIntelligence,
    &kContextualSearchForceCaption,
    &kContextualSearchLongpressResolve,
    &kContextualSearchMlTapSuppression,
    &KContextualSearchNewSettings,
    &kContextualSearchTapDisableOverride,
    &kContextualSearchThinWebViewImplementation,
    &kContextualSearchTranslations,
    &kContextualTriggersSelectionHandles,
    &kContextualTriggersSelectionMenu,
    &kContextualTriggersSelectionSize,
    &kDirectActions,
    &kDisableCompositedProgressBar,
    &kDownloadFileProvider,
    &kDownloadNotificationBadge,
    &kDownloadRename,
    &kDuetTabStripIntegrationAndroid,
    &kDynamicColorAndroid,
    &kDynamicColorButtonsAndroid,
    &kEnableDangerousDownloadDialog,
    &kEnableDuplicateDownloadDialog,
    &kExperimentsForAgsa,
    &kExploreSites,
    &kFixedUmaSessionResumeOrder,
    &kFocusOmniboxInIncognitoTabIntents,
    &kGoogleLensSdkIntent,
    &kGridTabSwitcherForTablets,
    &kHandleMediaIntents,
    &kHomepagePromoCard,
    &kImmersiveUiMode,
    &kIncognitoReauthenticationForAndroid,
    &kIncognitoScreenshot,
    &kInstanceSwitcher,
    &kInstantStart,
    &kKitKatSupported,
    &kLensCameraAssistedSearch,
    &kLocationBarModelOptimizations,
    &kNewWindowAppMenu,
    &kNotificationPermissionVariant,
    &kOfflineIndicatorV2,
    &kPageAnnotationsService,
    &kBookmarksImprovedSaveFlow,
    &kBookmarksRefresh,
    &kProbabilisticCryptidRenderer,
    &kQuickActionSearchWidgetAndroid,
    &kReachedCodeProfiler,
    &kImproveReaderModePrompt,
    &kReaderModeInCCT,
    &kReengagementNotification,
    &kRelatedSearches,
    &kRelatedSearchesAlternateUx,
    &kRelatedSearchesInBar,
    &kRelatedSearchesSimplifiedUx,
    &kRelatedSearchesUi,
    &kSearchEnginePromoExistingDevice,
    &kSearchEnginePromoExistingDeviceV2,
    &kSearchEnginePromoNewDevice,
    &kSearchEnginePromoNewDeviceV2,
    &kServiceManagerForBackgroundPrefetch,
    &kServiceManagerForDownload,
    &kShareButtonInTopToolbar,
    &kSharedClipboardUI,
    &kShowScrollableMVTOnNTPAndroid,
    &kSpannableInlineAutocomplete,
    &kSpecialLocaleWrapper,
    &kSpecialUserDecision,
    &kSuppressToolbarCaptures,
    &kStoreHoursAndroid,
    &kSwapPixelFormatToFixConvertFromTranslucent,
    &kTabEngagementReportingAndroid,
    &kTabGroupsAndroid,
    &kTabGroupsContinuationAndroid,
    &kTabGroupsUiImprovementsAndroid,
    &kTabGroupsForTablets,
    &kTabGridLayoutAndroid,
    &kTabReparenting,
    &kTabStripImprovements,
    &kTabSwitcherOnReturn,
    &kTabToGTSAnimation,
    &kTestDefaultDisabled,
    &kTestDefaultEnabled,
    &kThemeRefactorAndroid,
    &kToolbarIphAndroid,
    &kToolbarMicIphAndroid,
    &kTrustedWebActivityLocationDelegation,
    &kTrustedWebActivityNewDisclosure,
    &kTrustedWebActivityPostMessage,
    &kTrustedWebActivityQualityEnforcement,
    &kTrustedWebActivityQualityEnforcementForced,
    &kTrustedWebActivityQualityEnforcementWarning,
    &kShowExtendedPreloadingSetting,
    &kStartSurfaceAndroid,
    &kUmaBackgroundSessions,
    &kUpdateHistoryEntryPointsInIncognito,
    &kUpdateNotificationScheduleServiceImmediateShowOption,
    &kVoiceSearchAudioCapturePolicy,
    &kVoiceButtonInTopToolbar,
    &kVrBrowsingFeedback,
    &kWebOtpCrossDeviceSimpleString,
    &content_creation::kWebNotesStylizeEnabled,
    &kWebApkInstallCompleteNotification,
    &kWebApkTrampolineOnInitialIntent,
    &features::kDnsOverHttps,
    &notifications::features::kUseChimeAndroidSdk,
    &paint_preview::kPaintPreviewDemo,
    &paint_preview::kPaintPreviewShowOnStartup,
    &language::kAppLanguagePrompt,
    &language::kDetailedLanguageSettings,
    &language::kExplicitLanguageAsk,
    &language::kForceAppLanguagePrompt,
    &language::kTranslateAssistContent,
    &language::kTranslateIntent,
    &messages::kMessagesForAndroidChromeSurvey,
    &messages::kMessagesForAndroidInfrastructure,
    &messages::kMessagesForAndroidInstantApps,
    &messages::kMessagesForAndroidReaderMode,
    &messages::kMessagesForAndroidReduceLayoutChanges,
    &messages::kMessagesForAndroidSaveCard,
    &messages::kMessagesForAndroidSyncError,
    &offline_pages::kOfflineIndicatorFeature,
    &offline_pages::kOfflinePagesCTFeature,    // See crbug.com/620421.
    &offline_pages::kOfflinePagesCTV2Feature,  // See crbug.com/734753.
    &offline_pages::kOfflinePagesDescriptiveFailStatusFeature,
    &offline_pages::kOfflinePagesDescriptivePendingStatusFeature,
    &offline_pages::kOfflinePagesLivePageSharingFeature,
    &offline_pages::kPrefetchingOfflinePagesFeature,
    &omnibox::kAdaptiveSuggestionsCount,
    &omnibox::kMostVisitedTiles,
    &omnibox::kOmniboxAssistantVoiceSearch,
    &omnibox::kOmniboxSpareRenderer,
    &omnibox::kUpdatedConnectionSecurityIndicators,
    &optimization_guide::features::kPushNotifications,
    &page_info::kPageInfoAboutThisSite,
    &page_info::kPageInfoDiscoverability,
    &password_manager::features::kBiometricTouchToFill,
    &password_manager::features::kLeakDetectionUnauthenticated,
    &password_manager::features::kPasswordChange,
    &password_manager::features::kPasswordScriptsFetching,
    &password_manager::features::kRecoverFromNeverSaveAndroid,
    &password_manager::features::kTouchToFillPasswordSubmission,
    &password_manager::features::kUnifiedCredentialManagerDryRun,
    &password_manager::features::kUnifiedPasswordManagerAndroid,
    &password_manager::features::kUnifiedPasswordManagerMigration,
    &password_manager::features::kUnifiedPasswordManagerShadowAndroid,
    &password_manager::features::
        kUnifiedPasswordManagerShadowWriteOperationsAndroid,
    &performance_hints::features::kContextMenuPerformanceInfo,
    &policy::features::kChromeManagementPageAndroid,
    &privacy_sandbox::kPrivacySandboxSettings3,
    &query_tiles::features::kQueryTiles,
    &query_tiles::features::kQueryTilesInNTP,
    &query_tiles::features::kQueryTilesInOmnibox,
    &query_tiles::features::kQueryTilesLocalOrdering,
    &query_tiles::features::kQueryTilesSegmentation,
    &reading_list::switches::kReadLater,
    &send_tab_to_self::kSendTabToSelfSigninPromo,
    &send_tab_to_self::kSendTabToSelfV2,
    &share::kPersistShareHubOnAppSwitch,
    &share::kUpcomingSharingFeatures,
    &signin::kMobileIdentityConsistencyPromos,
    &switches::kAllowSyncOffForChildAccounts,
    &switches::kForceStartupSigninPromo,
    &switches::kForceDisableExtendedSyncPromos,
    &syncer::kSyncTrustedVaultPassphraseRecovery,
    &syncer::kSyncAndroidPromosWithSingleButton,
    &switches::kSyncUseSessionsUnregisterDelay,
    &subresource_filter::kSafeBrowsingSubresourceFilter,
    &video_tutorials::features::kVideoTutorials,
    &webapps::features::kInstallableAmbientBadgeInfoBar,
    &webapps::features::kInstallableAmbientBadgeMessage,
};

const base::Feature* FindFeatureExposedToJava(const std::string& feature_name) {
  for (const auto* feature : kFeaturesExposedToJava) {
    if (feature->name == feature_name)
      return feature;
  }
  NOTREACHED() << "Queried feature cannot be found in ChromeFeatureList: "
               << feature_name;
  return nullptr;
}

}  // namespace

// Alphabetical:

const base::Feature kAdaptiveButtonInTopToolbar{
    "AdaptiveButtonInTopToolbar", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAdaptiveButtonInTopToolbarCustomizationV2{
    "AdaptiveButtonInTopToolbarCustomizationV2",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAddToHomescreenIPH{"AddToHomescreenIPH",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAndroidLayoutChangeTabReparenting{
    "AndroidLayoutChangeTabReparenting", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kAllowNewIncognitoTabIntents{
    "AllowNewIncognitoTabIntents", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kFocusOmniboxInIncognitoTabIntents{
    "FocusOmniboxInIncognitoTabIntents", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kAndroidSearchEngineChoiceNotification{
    "AndroidSearchEngineChoiceNotification", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kAssistantConsentModal{"AssistantConsentModal",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAssistantConsentSimplifiedText{
    "AssistantConsentSimplifiedText", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAssistantConsentV2{"AssistantConsentV2",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAssistantIntentExperimentId{
    "AssistantIntentExperimentId", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAssistantIntentPageUrl{"AssistantIntentPageUrl",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAssistantIntentTranslateInfo{
    "AssistantIntentTranslateInfo", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAppLaunchpad{"AppLaunchpad",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAppMenuMobileSiteOption{"AppMenuMobileSiteOption",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAppToWebAttribution{"AppToWebAttribution",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kBackgroundThreadPool{"BackgroundThreadPool",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kBookmarkBottomSheet{"BookmarkBottomSheet",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kConditionalTabStripAndroid{
    "ConditionalTabStripAndroid", base::FEATURE_DISABLED_BY_DEFAULT};

// Used in downstream code.
const base::Feature kCastDeviceFilter{"CastDeviceFilter",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kCloseAllTabsModalDialog{"CloseAllTabsModalDialog",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kCloseTabSuggestions{"CloseTabSuggestions",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kCriticalPersistedTabData{
    "CriticalPersistedTabData", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kCCTBackgroundTab{"CCTBackgroundTab",
                                      base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kCCTClientDataHeader{"CCTClientDataHeader",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kCCTNewDownloadTab{"CCTNewDownloadTab",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kCCTExternalLinkHandling{"CCTExternalLinkHandling",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kCCTIncognito{"CCTIncognito",
                                  base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kCCTIncognitoAvailableToThirdParty{
    "CCTIncognitoAvailableToThirdParty", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kCCTPostMessageAPI{"CCTPostMessageAPI",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kCCTRedirectPreconnect{"CCTRedirectPreconnect",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kCCTRemoveRemoteViewIds{"CCTRemoveRemoteViewIds",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kCCTReportParallelRequestStatus{
    "CCTReportParallelRequestStatus", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kCCTResizable90MaximumHeight{
    "CCTResizable90MaximumHeight", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kCCTResizableAllowResizeByUserGesture{
    "CCTResizableAllowResizeByUserGesture", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kCCTResizableForFirstParties{
    "CCTResizableForFirstParties", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kCCTResizableForThirdParties{
    "CCTResizableForThirdParties", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kCCTResourcePrefetch{"CCTResourcePrefetch",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kDontAutoHideBrowserControls{
    "DontAutoHideBrowserControls", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kChromeNewDownloadTab{"ChromeNewDownloadTab",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kChromeShareLongScreenshot{
    "ChromeShareLongScreenshot", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kChromeSharingHub{"ChromeSharingHub",
                                      base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kChromeSharingHubLaunchAdjacent{
    "ChromeSharingHubLaunchAdjacent", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kChromeSurveyNextAndroid{"ChromeSurveyNextAndroid",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kCommandLineOnNonRooted{"CommandLineOnNonRooted",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kContextMenuEnableLensShoppingAllowlist{
    "ContextMenuEnableLensShoppingAllowlist",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kContextMenuGoogleLensChip{
    "ContextMenuGoogleLensChip", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kContextMenuPopupStyle{"ContextMenuPopupStyle",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kContextMenuSearchWithGoogleLens{
    "ContextMenuSearchWithGoogleLens", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kContextMenuShopWithGoogleLens{
    "ContextMenuShopWithGoogleLens", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kContextMenuSearchAndShopWithGoogleLens{
    "ContextMenuSearchAndShopWithGoogleLens",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kContextMenuTranslateWithGoogleLens{
    "ContextMenuTranslateWithGoogleLens", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kGoogleLensSdkIntent{"GoogleLensSdkIntent",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kLensCameraAssistedSearch{"LensCameraAssistedSearch",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kContextualSearchDebug{"ContextualSearchDebug",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kContextualSearchDelayedIntelligence{
    "ContextualSearchDelayedIntelligence", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kContextualSearchForceCaption{
    "ContextualSearchForceCaption", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kContextualSearchLongpressResolve{
    "ContextualSearchLongpressResolve", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kContextualSearchMlTapSuppression{
    "ContextualSearchMlTapSuppression", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature KContextualSearchNewSettings{
    "ContextualSearchNewSettings", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kContextualSearchTapDisableOverride{
    "ContextualSearchTapDisableOverride", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kContextualSearchThinWebViewImplementation{
    "ContextualSearchThinWebViewImplementation",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kContextualSearchTranslations{
    "ContextualSearchTranslations", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kContextualTriggersSelectionHandles{
    "ContextualTriggersSelectionHandles", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kContextualTriggersSelectionMenu{
    "ContextualTriggersSelectionMenu", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kContextualTriggersSelectionSize{
    "ContextualTriggersSelectionSize", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kDirectActions{"DirectActions",
                                   base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kDisableCompositedProgressBar{
    "DisableCompositedProgressBar", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kDownloadAutoResumptionThrottling{
    "DownloadAutoResumptionThrottling", base::FEATURE_ENABLED_BY_DEFAULT};

extern const base::Feature kDownloadHomeForExternalApp{
    "DownloadHomeForExternalApp", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kDownloadFileProvider{"DownloadFileProvider",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kDownloadNotificationBadge{
    "DownloadNotificationBadge", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kDownloadRename{"DownloadRename",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kDuetTabStripIntegrationAndroid{
    "DuetTabStripIntegrationAndroid", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kDynamicColorAndroid{"DynamicColorAndroid",
                                         base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kDynamicColorButtonsAndroid{
    "DynamicColorButtonsAndroid", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableDangerousDownloadDialog{
    "EnableDangerousDownloadDialog", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kEnableDuplicateDownloadDialog{
    "EnableDuplicateDownloadDialog", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kEnableMixedContentDownloadDialog{
    "EnableMixedContentDownloadDialog", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kExperimentsForAgsa{"ExperimentsForAgsa",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kExploreSites{"ExploreSites",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kGridTabSwitcherForTablets{
    "GridTabSwitcherForTablets", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kHandleMediaIntents{"HandleMediaIntents",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kHomepagePromoCard{"HomepagePromoCard",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kImmersiveUiMode{"ImmersiveUiMode",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kImproveReaderModePrompt{"ImproveReaderModePrompt",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kIncognitoReauthenticationForAndroid{
    "IncognitoReauthenticationForAndroid", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kIncognitoScreenshot{"IncognitoScreenshot",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kInstantStart{"InstantStart",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kKitKatSupported{"KitKatSupported",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kLocationBarModelOptimizations{
    "LocationBarModelOptimizations", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSearchEnginePromoExistingDevice{
    "SearchEnginePromo.ExistingDevice", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kSearchEnginePromoExistingDeviceV2{
    "SearchEnginePromo.ExistingDeviceVer2", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSearchEnginePromoNewDevice{
    "SearchEnginePromo.NewDevice", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kSearchEnginePromoNewDeviceV2{
    "SearchEnginePromo.NewDeviceVer2", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kNewWindowAppMenu{"NewWindowAppMenu",
                                      base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kNotificationPermissionVariant{
    "NotificationPermissionVariant", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kInstanceSwitcher{"InstanceSwitcher",
                                      base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kOfflineIndicatorV2{"OfflineIndicatorV2",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kPageAnnotationsService{"PageAnnotationsService",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kBookmarksImprovedSaveFlow{
    "BookmarksImprovedSaveFlow", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kBookmarksRefresh{"BookmarksRefresh",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kProbabilisticCryptidRenderer{
    "ProbabilisticCryptidRenderer", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kQuickActionSearchWidgetAndroid{
    "QuickActionSearchWidgetAndroid", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kReachedCodeProfiler{"ReachedCodeProfiler",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kReaderModeInCCT{"ReaderModeInCCT",
                                     base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kReengagementNotification{
    "ReengagementNotification", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kRelatedSearches{"RelatedSearches",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kRelatedSearchesAlternateUx{
    "RelatedSearchesAlternateUx", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kRelatedSearchesInBar{"RelatedSearchesInBar",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kRelatedSearchesSimplifiedUx{
    "RelatedSearchesSimplifiedUx", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kRelatedSearchesUi{"RelatedSearchesUi",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kServiceManagerForBackgroundPrefetch{
    "ServiceManagerForBackgroundPrefetch", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kServiceManagerForDownload{
    "ServiceManagerForDownload", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kShareButtonInTopToolbar{"ShareButtonInTopToolbar",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kShowScrollableMVTOnNTPAndroid{
    "ShowScrollableMVTOnNTPAndroid", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSpannableInlineAutocomplete{
    "SpannableInlineAutocomplete", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kSpecialLocaleWrapper{"SpecialLocaleWrapper",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kSpecialUserDecision{"SpecialUserDecision",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kStoreHoursAndroid{"StoreHoursAndroid",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSuppressToolbarCaptures{"SuppressToolbarCaptures",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSwapPixelFormatToFixConvertFromTranslucent{
    "SwapPixelFormatToFixConvertFromTranslucent",
    base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kTabEngagementReportingAndroid{
    "TabEngagementReportingAndroid", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kTabGroupsAndroid{"TabGroupsAndroid",
                                      base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kTabGroupsContinuationAndroid{
    "TabGroupsContinuationAndroid", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kTabGroupsUiImprovementsAndroid{
    "TabGroupsUiImprovementsAndroid", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kTabGroupsForTablets{"TabGroupsForTablets",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kTabGridLayoutAndroid{"TabGridLayoutAndroid",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kTabReparenting{"TabReparenting",
                                    base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kTabStripImprovements{"TabStripImprovements",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kTabSwitcherOnReturn{"TabSwitcherOnReturn",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kTabToGTSAnimation{"TabToGTSAnimation",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kTestDefaultDisabled{"TestDefaultDisabled",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kTestDefaultEnabled{"TestDefaultEnabled",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kThemeRefactorAndroid{"ThemeRefactorAndroid",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kToolbarIphAndroid{"ToolbarIphAndroid",
                                       base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kToolbarMicIphAndroid{"ToolbarMicIphAndroid",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kTrustedWebActivityLocationDelegation{
    "TrustedWebActivityLocationDelegation", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kTrustedWebActivityNewDisclosure{
    "TrustedWebActivityNewDisclosure", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kTrustedWebActivityPostMessage{
    "TrustedWebActivityPostMessage", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kTrustedWebActivityQualityEnforcement{
    "TrustedWebActivityQualityEnforcement", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kTrustedWebActivityQualityEnforcementForced{
    "TrustedWebActivityQualityEnforcementForced",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kTrustedWebActivityQualityEnforcementWarning{
    "TrustedWebActivityQualityEnforcementWarning",
    base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kShowExtendedPreloadingSetting{
    "ShowExtendedPreloadingSetting", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kStartSurfaceAndroid{"StartSurfaceAndroid",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, keep logging and reporting UMA while chrome is backgrounded.
const base::Feature kUmaBackgroundSessions{"UMABackgroundSessions",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kUpdateHistoryEntryPointsInIncognito{
    "UpdateHistoryEntryPointsInIncognito", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kUpdateNotificationScheduleServiceImmediateShowOption{
    "UpdateNotificationScheduleServiceImmediateShowOption",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kUserMediaScreenCapturing{
    "UserMediaScreenCapturing", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kVoiceSearchAudioCapturePolicy{
    "VoiceSearchAudioCapturePolicy", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kVoiceButtonInTopToolbar{"VoiceButtonInTopToolbar",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kVrBrowsingFeedback{"VrBrowsingFeedback",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// Shows only the remote device name on the Android notification instead of
// a descriptive text.
const base::Feature kWebOtpCrossDeviceSimpleString{
    "WebOtpCrossDeviceSimpleString", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kWebApkInstallCompleteNotification{
    "WebApkInstallCompleteNotification", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kWebApkTrampolineOnInitialIntent{
    "WebApkTrampolineOnInitialIntent", base::FEATURE_ENABLED_BY_DEFAULT};

static jboolean JNI_ChromeFeatureList_IsEnabled(
    JNIEnv* env,
    const JavaParamRef<jstring>& jfeature_name) {
  const base::Feature* feature =
      FindFeatureExposedToJava(ConvertJavaStringToUTF8(env, jfeature_name));
  return base::FeatureList::IsEnabled(*feature);
}

static ScopedJavaLocalRef<jstring>
JNI_ChromeFeatureList_GetFieldTrialParamByFeature(
    JNIEnv* env,
    const JavaParamRef<jstring>& jfeature_name,
    const JavaParamRef<jstring>& jparam_name) {
  const base::Feature* feature =
      FindFeatureExposedToJava(ConvertJavaStringToUTF8(env, jfeature_name));
  const std::string& param_name = ConvertJavaStringToUTF8(env, jparam_name);
  const std::string& param_value =
      base::GetFieldTrialParamValueByFeature(*feature, param_name);
  return ConvertUTF8ToJavaString(env, param_value);
}

static jint JNI_ChromeFeatureList_GetFieldTrialParamByFeatureAsInt(
    JNIEnv* env,
    const JavaParamRef<jstring>& jfeature_name,
    const JavaParamRef<jstring>& jparam_name,
    const jint jdefault_value) {
  const base::Feature* feature =
      FindFeatureExposedToJava(ConvertJavaStringToUTF8(env, jfeature_name));
  const std::string& param_name = ConvertJavaStringToUTF8(env, jparam_name);
  return base::GetFieldTrialParamByFeatureAsInt(*feature, param_name,
                                                jdefault_value);
}

static jdouble JNI_ChromeFeatureList_GetFieldTrialParamByFeatureAsDouble(
    JNIEnv* env,
    const JavaParamRef<jstring>& jfeature_name,
    const JavaParamRef<jstring>& jparam_name,
    const jdouble jdefault_value) {
  const base::Feature* feature =
      FindFeatureExposedToJava(ConvertJavaStringToUTF8(env, jfeature_name));
  const std::string& param_name = ConvertJavaStringToUTF8(env, jparam_name);
  return base::GetFieldTrialParamByFeatureAsDouble(*feature, param_name,
                                                   jdefault_value);
}

static jboolean JNI_ChromeFeatureList_GetFieldTrialParamByFeatureAsBoolean(
    JNIEnv* env,
    const JavaParamRef<jstring>& jfeature_name,
    const JavaParamRef<jstring>& jparam_name,
    const jboolean jdefault_value) {
  const base::Feature* feature =
      FindFeatureExposedToJava(ConvertJavaStringToUTF8(env, jfeature_name));
  const std::string& param_name = ConvertJavaStringToUTF8(env, jparam_name);
  return base::GetFieldTrialParamByFeatureAsBool(*feature, param_name,
                                                 jdefault_value);
}

static ScopedJavaLocalRef<jobjectArray>
JNI_ChromeFeatureList_GetFlattedFieldTrialParamsForFeature(
    JNIEnv* env,
    const JavaParamRef<jstring>& feature_name) {
  base::FieldTrialParams params;
  std::vector<std::string> keys_and_values;
  const base::Feature* feature =
      FindFeatureExposedToJava(ConvertJavaStringToUTF8(env, feature_name));
  if (feature && base::GetFieldTrialParamsByFeature(*feature, &params)) {
    for (const auto& param_pair : params) {
      keys_and_values.push_back(param_pair.first);
      keys_and_values.push_back(param_pair.second);
    }
  }
  return base::android::ToJavaArrayOfStrings(env, keys_and_values);
}

}  // namespace android
}  // namespace chrome
