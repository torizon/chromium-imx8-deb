// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/virtual_card_enrollment_strike_database.h"

#include "components/autofill/core/browser/proto/strike_data.pb.h"
#include "components/autofill/core/common/autofill_payments_features.h"

namespace autofill {

// Limit the number of cards for which strikes are collected
constexpr size_t kMaxStrikeEntities = 50;

// Once the limit of cards is reached, delete 20 to create a bit of headroom.
constexpr size_t kMaxStrikeEntitiesAfterCleanup = 30;

// The maximum number of strikes before we stop showing virtual card enrollment
// dialogs.
int kCardMaximumStrikes = 3;

// The number of days until strikes expire for virtual card enrollment.
int kDaysUntilCardStrikeExpiry = 180;

VirtualCardEnrollmentStrikeDatabase::VirtualCardEnrollmentStrikeDatabase(
    StrikeDatabaseBase* strike_database)
    : StrikeDatabaseIntegratorBase(strike_database) {
  RemoveExpiredStrikes();
}

VirtualCardEnrollmentStrikeDatabase::~VirtualCardEnrollmentStrikeDatabase() =
    default;

absl::optional<size_t> VirtualCardEnrollmentStrikeDatabase::GetMaximumEntries()
    const {
  return absl::optional<size_t>(kMaxStrikeEntities);
}

absl::optional<size_t>
VirtualCardEnrollmentStrikeDatabase::GetMaximumEntriesAfterCleanup() const {
  return kMaxStrikeEntitiesAfterCleanup;
}

std::string VirtualCardEnrollmentStrikeDatabase::GetProjectPrefix() const {
  return "VirtualCardEnrollment";
}

int VirtualCardEnrollmentStrikeDatabase::GetMaxStrikesLimit() const {
  // The default limit for strikes is 3.
  return kCardMaximumStrikes;
}

absl::optional<base::TimeDelta>
VirtualCardEnrollmentStrikeDatabase::GetExpiryTimeDelta() const {
  // Expiry time is 180 days by default.
  return absl::optional<base::TimeDelta>(
      base::Days(kDaysUntilCardStrikeExpiry));
}

bool VirtualCardEnrollmentStrikeDatabase::UniqueIdsRequired() const {
  return true;
}

}  // namespace autofill
