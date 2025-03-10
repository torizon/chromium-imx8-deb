// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_SYNC_BROWSER_SYNC_SWITCHES_H_
#define COMPONENTS_BROWSER_SYNC_BROWSER_SYNC_SWITCHES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace switches {

// Enabled the local sync backend implemented by the LoopbackServer.
inline constexpr char kEnableLocalSyncBackend[] = "enable-local-sync-backend";

// Specifies the local sync backend directory. The name is chosen to mimic
// user-data-dir etc. This flag only matters if the enable-local-sync-backend
// flag is present.
inline constexpr char kLocalSyncBackendDir[] = "local-sync-backend-dir";

#if BUILDFLAG(IS_ANDROID)
inline constexpr base::Feature kSyncUseSessionsUnregisterDelay{
    "SyncUseSessionsUnregisterDelay", base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_ANDROID)

// Sync invalidation switches.
//
// Enables providing the list of FCM registration tokens in the commit request.
inline constexpr base::Feature kSyncUseFCMRegistrationTokensList{
    "SyncUseFCMRegistrationTokensList", base::FEATURE_ENABLED_BY_DEFAULT};
// Max size of FCM registration tokens list. If the number of active devices
// having FCM registration tokens is higher, then the resulting list will be
// empty meaning unknown FCM registration tokens.
inline constexpr base::FeatureParam<int> kSyncFCMRegistrationTokensListMaxSize{
    &kSyncUseFCMRegistrationTokensList, "SyncFCMRegistrationTokensListMaxSize",
    5};
// Enables filtering out inactive devices which haven't sent DeviceInfo update
// recently (depending on the device's pulse_interval and an additional margin).
inline constexpr base::Feature kSyncFilterOutInactiveDevicesForSingleClient{
    "SyncFilterOutInactiveDevicesForSingleClient",
    base::FEATURE_DISABLED_BY_DEFAULT};
// An additional threshold to consider devices as active. It extends device's
// pulse interval to mitigate possible latency after DeviceInfo commit.
inline constexpr base::FeatureParam<base::TimeDelta> kSyncActiveDeviceMargin{
    &kSyncFilterOutInactiveDevicesForSingleClient, "SyncActiveDeviceMargin",
    base::Minutes(30)};

}  // namespace switches

#endif  // COMPONENTS_BROWSER_SYNC_BROWSER_SYNC_SWITCHES_H_
