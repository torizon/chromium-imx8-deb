// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_DEFAULT_MODEL_MANAGER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_DEFAULT_MODEL_MANAGER_H_

#include <deque>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "components/segmentation_platform/internal/proto/model_metadata.pb.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/public/model_provider.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using optimization_guide::proto::OptimizationTarget;

namespace segmentation_platform {
class SegmentInfoDatabase;

// DefaultModelManager provides support to query all default models available.
// It also provides useful methods to combine results from both the database and
// the default model.
class DefaultModelManager {
 public:
  DefaultModelManager(ModelProviderFactory* model_provider_factory,
                      const std::vector<OptimizationTarget>& segment_ids);
  virtual ~DefaultModelManager();

  // Disallow copy/assign.
  DefaultModelManager(const DefaultModelManager&) = delete;
  DefaultModelManager& operator=(const DefaultModelManager&) = delete;

  // Callback for returning a list of segment infos associated with IDs.
  // The same segment ID can be repeated multiple times.
  using SegmentInfoList =
      std::vector<std::pair<OptimizationTarget, proto::SegmentInfo>>;
  using MultipleSegmentInfoCallback =
      base::OnceCallback<void(std::unique_ptr<SegmentInfoList>)>;

  // Utility function to get the segment info from both the database and the
  // default model for a given set of segment IDs. The result can contain
  // the same segment ID multiple times.
  virtual void GetAllSegmentInfoFromBothModels(
      const std::vector<OptimizationTarget>& segment_ids,
      SegmentInfoDatabase* segment_database,
      MultipleSegmentInfoCallback callback);

  // Called to get the segment info from the default model for a given set of
  // segment IDs.
  virtual void GetAllSegmentInfoFromDefaultModel(
      const std::vector<OptimizationTarget>& segment_ids,
      MultipleSegmentInfoCallback callback);

  // Returns the default provider or `nulllptr` when unavailable.
  ModelProvider* GetDefaultProvider(OptimizationTarget segment_id);

  void SetDefaultProvidersForTesting(
      std::map<OptimizationTarget, std::unique_ptr<ModelProvider>>&& providers);

 private:
  void GetNextSegmentInfoFromDefaultModel(
      std::unique_ptr<SegmentInfoList> result,
      std::deque<OptimizationTarget> remaining_segment_ids,
      MultipleSegmentInfoCallback callback);

  void OnFetchDefaultModel(std::unique_ptr<SegmentInfoList> result,
                           std::deque<OptimizationTarget> remaining_segment_ids,
                           MultipleSegmentInfoCallback callback,
                           OptimizationTarget segment_id,
                           proto::SegmentationModelMetadata metadata,
                           int64_t model_version);

  void OnGetAllSegmentInfoFromDatabase(
      const std::vector<OptimizationTarget>& segment_ids,
      MultipleSegmentInfoCallback callback,
      std::unique_ptr<SegmentInfoList> segment_infos);

  void OnGetAllSegmentInfoFromDefaultModel(
      MultipleSegmentInfoCallback callback,
      std::unique_ptr<SegmentInfoList> segment_infos_from_db,
      std::unique_ptr<SegmentInfoList> segment_infos_from_default_model);

  // Default model providers.
  std::map<OptimizationTarget, std::unique_ptr<ModelProvider>>
      default_model_providers_;
  const raw_ptr<ModelProviderFactory> model_provider_factory_;

  base::WeakPtrFactory<DefaultModelManager> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_EXECUTION_DEFAULT_MODEL_MANAGER_H_
