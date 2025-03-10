// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_topics/browsing_topics_page_load_data_tracker.h"

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "components/browsing_topics/test_util.h"
#include "components/history/content/browser/history_context_helper.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/test_history_database.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/browsing_topics_test_util.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "content/test/test_render_view_host.h"

namespace browsing_topics {

class BrowsingTopicsPageLoadDataTrackerTest
    : public content::RenderViewHostTestHarness {
 public:
  BrowsingTopicsPageLoadDataTrackerTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{blink::features::kBrowsingTopics},
        /*disabled_features=*/{});

    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());

    history_service_ = std::make_unique<history::HistoryService>();
    history_service_->Init(
        history::TestHistoryDatabaseParamsForPath(temp_dir_.GetPath()));
  }

  ~BrowsingTopicsPageLoadDataTrackerTest() override = default;

  void TearDown() override {
    DCHECK(history_service_);

    base::RunLoop run_loop;
    history_service_->SetOnBackendDestroyTask(run_loop.QuitClosure());
    history_service_.reset();
    run_loop.Run();

    content::RenderViewHostTestHarness::TearDown();
  }

  void NavigateToPage(const GURL& url,
                      bool publicly_routable,
                      bool browsing_topics_permissions_policy_allowed,
                      bool interest_cohort_permissions_policy_allowed) {
    auto simulator = content::NavigationSimulator::CreateBrowserInitiated(
        url, web_contents());
    simulator->SetTransition(ui::PageTransition::PAGE_TRANSITION_TYPED);

    if (!publicly_routable) {
      net::IPAddress address;
      EXPECT_TRUE(address.AssignFromIPLiteral("0.0.0.0"));
      simulator->SetSocketAddress(net::IPEndPoint(address, /*port=*/0));
    }

    blink::ParsedPermissionsPolicy policy;

    if (!browsing_topics_permissions_policy_allowed) {
      policy.emplace_back(
          blink::mojom::PermissionsPolicyFeature::kBrowsingTopics,
          /*values=*/std::vector<url::Origin>(), /*matches_all_origins=*/false,
          /*matches_opaque_src=*/false);
    }

    if (!interest_cohort_permissions_policy_allowed) {
      policy.emplace_back(blink::mojom::PermissionsPolicyFeature::
                              kBrowsingTopicsBackwardCompatible,
                          /*values=*/std::vector<url::Origin>(),
                          /*matches_all_origins=*/false,
                          /*matches_opaque_src=*/false);
    }

    simulator->SetPermissionsPolicyHeader(std::move(policy));

    simulator->Commit();

    history_service_->AddPage(
        url, base::Time::Now(),
        history::ContextIDForWebContents(web_contents()),
        web_contents()->GetController().GetLastCommittedEntry()->GetUniqueID(),
        /*referrer=*/GURL(),
        /*redirects=*/{}, ui::PageTransition::PAGE_TRANSITION_TYPED,
        history::VisitSource::SOURCE_BROWSED,
        /*did_replace_entry=*/false,
        /*floc_allowed=*/false);
  }

  BrowsingTopicsPageLoadDataTracker* GetBrowsingTopicsPageLoadDataTracker() {
    return BrowsingTopicsPageLoadDataTracker::GetOrCreateForPage(
        web_contents()->GetMainFrame()->GetPage());
  }

  content::BrowsingTopicsSiteDataManager* topics_site_data_manager() {
    return web_contents()
        ->GetMainFrame()
        ->GetProcess()
        ->GetStoragePartition()
        ->GetBrowsingTopicsSiteDataManager();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<history::HistoryService> history_service_;

  base::ScopedTempDir temp_dir_;
};

TEST_F(BrowsingTopicsPageLoadDataTrackerTest, OneUsage) {
  GURL url("https://foo.com");
  NavigateToPage(url, /*publicly_routable=*/true,
                 /*browsing_topics_permissions_policy_allowed=*/true,
                 /*interest_cohort_permissions_policy_allowed=*/true);

  EXPECT_FALSE(BrowsingTopicsEligibleForURLVisit(history_service_.get(), url));
  EXPECT_TRUE(
      content::GetBrowsingTopicsApiUsage(topics_site_data_manager()).empty());

  GetBrowsingTopicsPageLoadDataTracker()->OnBrowsingTopicsApiUsed(
      HashedDomain(123), history_service_.get());

  EXPECT_TRUE(BrowsingTopicsEligibleForURLVisit(history_service_.get(), url));

  std::vector<ApiUsageContext> api_usage_contexts =
      content::GetBrowsingTopicsApiUsage(topics_site_data_manager());
  EXPECT_EQ(api_usage_contexts.size(), 1u);
  EXPECT_EQ(api_usage_contexts[0].hashed_main_frame_host,
            HashMainFrameHostForStorage("foo.com"));
  EXPECT_EQ(api_usage_contexts[0].hashed_context_domain, HashedDomain(123));
}

TEST_F(BrowsingTopicsPageLoadDataTrackerTest, TwoUsages) {
  GURL url("https://foo.com");
  NavigateToPage(url, /*publicly_routable=*/true,
                 /*browsing_topics_permissions_policy_allowed=*/true,
                 /*interest_cohort_permissions_policy_allowed=*/true);

  GetBrowsingTopicsPageLoadDataTracker()->OnBrowsingTopicsApiUsed(
      HashedDomain(123), history_service_.get());
  GetBrowsingTopicsPageLoadDataTracker()->OnBrowsingTopicsApiUsed(
      HashedDomain(456), history_service_.get());

  EXPECT_TRUE(BrowsingTopicsEligibleForURLVisit(history_service_.get(), url));

  std::vector<ApiUsageContext> api_usage_contexts =
      content::GetBrowsingTopicsApiUsage(topics_site_data_manager());
  EXPECT_EQ(api_usage_contexts.size(), 2u);
  EXPECT_EQ(api_usage_contexts[0].hashed_main_frame_host,
            HashMainFrameHostForStorage("foo.com"));
  EXPECT_EQ(api_usage_contexts[0].hashed_context_domain, HashedDomain(123));
  EXPECT_EQ(api_usage_contexts[1].hashed_main_frame_host,
            HashMainFrameHostForStorage("foo.com"));
  EXPECT_EQ(api_usage_contexts[1].hashed_context_domain, HashedDomain(456));
}

TEST_F(BrowsingTopicsPageLoadDataTrackerTest, DuplicateDomains) {
  GURL url("https://foo.com");
  NavigateToPage(url, /*publicly_routable=*/true,
                 /*browsing_topics_permissions_policy_allowed=*/true,
                 /*interest_cohort_permissions_policy_allowed=*/true);

  GetBrowsingTopicsPageLoadDataTracker()->OnBrowsingTopicsApiUsed(
      HashedDomain(123), history_service_.get());
  GetBrowsingTopicsPageLoadDataTracker()->OnBrowsingTopicsApiUsed(
      HashedDomain(456), history_service_.get());
  GetBrowsingTopicsPageLoadDataTracker()->OnBrowsingTopicsApiUsed(
      HashedDomain(123), history_service_.get());

  EXPECT_TRUE(BrowsingTopicsEligibleForURLVisit(history_service_.get(), url));

  std::vector<ApiUsageContext> api_usage_contexts =
      content::GetBrowsingTopicsApiUsage(topics_site_data_manager());
  EXPECT_EQ(api_usage_contexts.size(), 2u);
  EXPECT_EQ(api_usage_contexts[0].hashed_main_frame_host,
            HashMainFrameHostForStorage("foo.com"));
  EXPECT_EQ(api_usage_contexts[0].hashed_context_domain, HashedDomain(123));
  EXPECT_EQ(api_usage_contexts[1].hashed_main_frame_host,
            HashMainFrameHostForStorage("foo.com"));
  EXPECT_EQ(api_usage_contexts[1].hashed_context_domain, HashedDomain(456));

  // The second HashedDomain(123) shouldn't update the database. Verify this by
  // verifying that the timestamp for HashedDomain(123) is no greater than the
  // timestamp for HashedDomain(456).
  EXPECT_LE(api_usage_contexts[0].time, api_usage_contexts[1].time);
}

TEST_F(BrowsingTopicsPageLoadDataTrackerTest, NumberOfDomainsExceedsLimit) {
  GURL url("https://foo.com");
  NavigateToPage(url, /*publicly_routable=*/true,
                 /*browsing_topics_permissions_policy_allowed=*/true,
                 /*interest_cohort_permissions_policy_allowed=*/true);

  for (int i = 0; i < 31; ++i) {
    GetBrowsingTopicsPageLoadDataTracker()->OnBrowsingTopicsApiUsed(
        HashedDomain(i), history_service_.get());
  }

  EXPECT_TRUE(BrowsingTopicsEligibleForURLVisit(history_service_.get(), url));

  std::vector<ApiUsageContext> api_usage_contexts =
      content::GetBrowsingTopicsApiUsage(topics_site_data_manager());

  EXPECT_EQ(api_usage_contexts.size(), 30u);

  for (int i = 0; i < 30; ++i) {
    EXPECT_EQ(api_usage_contexts[i].hashed_main_frame_host,
              HashMainFrameHostForStorage("foo.com"));
    EXPECT_EQ(api_usage_contexts[i].hashed_context_domain, HashedDomain(i));
  }
}

TEST_F(BrowsingTopicsPageLoadDataTrackerTest, NotPubliclyRoutable) {
  GURL url("https://foo.com");
  NavigateToPage(url, /*publicly_routable=*/false,
                 /*browsing_topics_permissions_policy_allowed=*/true,
                 /*interest_cohort_permissions_policy_allowed=*/true);

  GetBrowsingTopicsPageLoadDataTracker()->OnBrowsingTopicsApiUsed(
      HashedDomain(123), history_service_.get());

  EXPECT_FALSE(BrowsingTopicsEligibleForURLVisit(history_service_.get(), url));
  EXPECT_TRUE(
      content::GetBrowsingTopicsApiUsage(topics_site_data_manager()).empty());
}

TEST_F(BrowsingTopicsPageLoadDataTrackerTest,
       BrowsingTopicsPermissionsPolicyNotAllowed) {
  GURL url("https://foo.com");
  NavigateToPage(url, /*publicly_routable=*/true,
                 /*browsing_topics_permissions_policy_allowed=*/false,
                 /*interest_cohort_permissions_policy_allowed=*/true);

  GetBrowsingTopicsPageLoadDataTracker()->OnBrowsingTopicsApiUsed(
      HashedDomain(123), history_service_.get());

  EXPECT_FALSE(BrowsingTopicsEligibleForURLVisit(history_service_.get(), url));
  EXPECT_TRUE(
      content::GetBrowsingTopicsApiUsage(topics_site_data_manager()).empty());
}

TEST_F(BrowsingTopicsPageLoadDataTrackerTest,
       InterestCohortPermissionsPolicyNotAllowed) {
  GURL url("https://foo.com");
  NavigateToPage(url, /*publicly_routable=*/true,
                 /*browsing_topics_permissions_policy_allowed=*/true,
                 /*interest_cohort_permissions_policy_allowed=*/false);

  GetBrowsingTopicsPageLoadDataTracker()->OnBrowsingTopicsApiUsed(
      HashedDomain(123), history_service_.get());

  EXPECT_FALSE(BrowsingTopicsEligibleForURLVisit(history_service_.get(), url));
  EXPECT_TRUE(
      content::GetBrowsingTopicsApiUsage(topics_site_data_manager()).empty());
}

}  // namespace browsing_topics
