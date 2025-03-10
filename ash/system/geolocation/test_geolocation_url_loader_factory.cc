// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/geolocation/test_geolocation_url_loader_factory.h"

#include <memory>

#include "base/json/json_string_value_serializer.h"

namespace ash {

namespace {

// Creates a serialized dictionary string of the geoposition.
std::string CreateResponseBody(const Geoposition& position) {
  base::Value::Dict value;
  if (position.accuracy)
    value.Set("accuracy", position.accuracy);

  if (position.latitude && position.longitude) {
    base::Value location(base::Value::Type::DICTIONARY);
    location.SetDoubleKey("lat", position.latitude);
    location.SetDoubleKey("lng", position.longitude);
    value.Set("location", std::move(location));
  }

  if (position.error_code) {
    base::Value error(base::Value::Type::DICTIONARY);
    error.SetIntKey("error_code", position.error_code);
    value.Set("error", std::move(error));
  }

  std::string serialized_response;
  JSONStringValueSerializer serializer(&serialized_response);
  serializer.Serialize(value);
  return serialized_response;
}

}  // namespace

TestGeolocationUrlLoaderFactory::TestGeolocationUrlLoaderFactory() = default;

void TestGeolocationUrlLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& url_request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  test_url_loader_factory_.CreateLoaderAndStart(
      std::move(receiver), request_id, options, url_request, std::move(client),
      traffic_annotation);
  test_url_loader_factory_.AddResponse(url_request.url.spec(),
                                       CreateResponseBody(position_));
}

void TestGeolocationUrlLoaderFactory::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  NOTREACHED();
}

std::unique_ptr<network::PendingSharedURLLoaderFactory>
TestGeolocationUrlLoaderFactory::Clone() {
  NOTREACHED();
  return nullptr;
}

TestGeolocationUrlLoaderFactory::~TestGeolocationUrlLoaderFactory() = default;

}  // namespace ash
