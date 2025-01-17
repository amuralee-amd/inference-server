// Copyright 2021 Xilinx, Inc.
// Copyright 2022 Advanced Micro Devices, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/**
 * @file
 * @brief Implements the HTTP REST server
 */

#include "amdinfer/servers/http_server.hpp"

#include <drogon/HttpAppFramework.h>  // for HttpAppFramework, app
#include <drogon/HttpRequest.h>       // for HttpRequestPtr, Htt...
#include <json/value.h>               // for Value, arrayValue
#include <trantor/utils/Logger.h>     // for Logger, Logger::Warn

#include <chrono>         // for high_resolution_clock
#include <memory>         // for shared_ptr, __share...
#include <string>         // for allocator, operator+
#include <unordered_set>  // for unordered_set
#include <utility>        // for move
#include <vector>         // for vector

#include "amdinfer/build_options.hpp"          // for AMDINFER_ENABLE_TRACING
#include "amdinfer/clients/http_internal.hpp"  // for propagate, errorHtt...
#include "amdinfer/core/exceptions.hpp"        // for runtime_error, inva...
#include "amdinfer/core/interface.hpp"         // for Interface
#include "amdinfer/core/parameters.hpp"        // for ParameterMapPtr
#include "amdinfer/core/predict_api_internal.hpp"  // for ParameterMapPtr
#include "amdinfer/core/shared_state.hpp"          // for SharedState
#include "amdinfer/observation/logging.hpp"       // for Logger, AMDINFER_LOG...
#include "amdinfer/observation/metrics.hpp"       // for Metrics, MetricCoun...
#include "amdinfer/observation/tracing.hpp"       // for startTrace, Trace
#include "amdinfer/servers/websocket_server.hpp"  // for WebsocketServer
#include "amdinfer/util/string.hpp"               // for toLower

using drogon::HttpRequestPtr;
using drogon::HttpResponse;
using drogon::HttpResponsePtr;
using drogon::HttpStatusCode;

namespace amdinfer {

namespace http {

void start(SharedState *state, uint16_t port) {
  auto controller = std::make_shared<HttpServer>(state);
  auto ws_controller = std::make_shared<WebsocketServer>(state);

  auto &app = drogon::app();
  app.registerController(controller);
  app.registerController(ws_controller);
#ifdef AMDINFER_ENABLE_LOGGING
  auto dir = getLogDirectory();
  app.setLogLevel(trantor::Logger::kWarn).setLogPath(dir);
#else
  app.setLogLevel(trantor::Logger::kFatal).setLogPath(".");
#endif

  app.addListener("0.0.0.0", port)
    .setThreadNum(kDefaultDrogonThreads)
    .registerPostHandlingAdvice([](const drogon::HttpRequestPtr &req,
                                   const drogon::HttpResponsePtr &resp) {
      (void)req;  // suppress unused variable warning
      resp->addHeader("Access-Control-Allow-Origin", "*");
    })
    .setClientMaxBodySize(kMaxClientBodySize)
    // .enableRunAsDaemon()
    .run();
}

void stop() { drogon::app().quit(); }

}  // namespace http

HttpServer::HttpServer(SharedState *state) : state_(state) {
  AMDINFER_LOG_DEBUG(logger_, "Constructed HttpServer");
}

#ifdef AMDINFER_ENABLE_REST

void HttpServer::getServerLive(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback) const {
  AMDINFER_LOG_INFO(logger_, "Received getServerLive request");
#ifdef AMDINFER_ENABLE_METRICS
  Metrics::getInstance().incrementCounter(MetricCounterIDs::RestGet);
#endif
#ifdef AMDINFER_ENABLE_TRACING
  auto trace = startTrace(&(__func__[0]), req->getHeaders());
#else
  (void)req;  // suppress unused variable warning
#endif

  auto resp = HttpResponse::newHttpResponse();
#ifdef AMDINFER_ENABLE_TRACING
  auto context = trace->propagate();
  propagate(resp.get(), context);
#endif
  callback(resp);
}

void HttpServer::getServerReady(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback) const {
  AMDINFER_LOG_INFO(logger_, "Received getServerReady request");
#ifdef AMDINFER_ENABLE_METRICS
  Metrics::getInstance().incrementCounter(MetricCounterIDs::RestGet);
#endif
  (void)req;  // suppress unused variable warning

  // for now, assuming that server is always ready (assumes that user has loaded
  // all the required models).

  auto resp = HttpResponse::newHttpResponse();
  callback(resp);
}

void HttpServer::getModelReady(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback,
  std::string const &model) const {
  AMDINFER_LOG_INFO(logger_, "Received getModelReady request");
#ifdef AMDINFER_ENABLE_METRICS
  Metrics::getInstance().incrementCounter(MetricCounterIDs::RestGet);
#endif
  (void)req;  // suppress unused variable warning

  auto resp = HttpResponse::newHttpResponse();
  try {
    if (!state_->modelReady(model)) {
      resp->setStatusCode(HttpStatusCode::k503ServiceUnavailable);
    }
  } catch (const invalid_argument &e) {
    resp->setStatusCode(HttpStatusCode::k400BadRequest);
    resp->setBody(e.what());
  }
  callback(resp);
}

void HttpServer::getServerMetadata(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback) const {
  AMDINFER_LOG_INFO(logger_, "Received getServerMetadata request");
#ifdef AMDINFER_ENABLE_METRICS
  Metrics::getInstance().incrementCounter(MetricCounterIDs::RestGet);
#endif
  (void)req;  // suppress unused variable warning

  auto metadata = SharedState::serverMetadata();

  Json::Value ret;
  ret["name"] = metadata.name;
  ret["version"] = metadata.version;
  ret["extensions"] = Json::arrayValue;
  for (const auto &extension : metadata.extensions) {
    ret["extensions"].append(extension);
  }
  auto resp = HttpResponse::newHttpJsonResponse(ret);
  callback(resp);
}

void HttpServer::getModelMetadata(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback,
  const std::string &model) const {
  AMDINFER_LOG_INFO(logger_, "Received getModelMetadata request");
#ifdef AMDINFER_ENABLE_METRICS
  Metrics::getInstance().incrementCounter(MetricCounterIDs::RestGet);
#endif
  (void)req;  // suppress unused variable warning

  Json::Value ret;
  bool error = false;
  try {
    auto metadata = state_->modelMetadata(model);
    ret = modelMetadataToJson(metadata);
  } catch (const runtime_error &e) {
    ret["error"] = e.what();
    error = true;
  }

  auto resp = HttpResponse::newHttpJsonResponse(ret);
  if (error) {
    resp->setStatusCode(HttpStatusCode::k400BadRequest);
  }
  callback(resp);
}

void HttpServer::modelList(
  const drogon::HttpRequestPtr &req,
  std::function<void(const drogon::HttpResponsePtr &)> &&callback) const {
  AMDINFER_LOG_INFO(logger_, "Received modelList request");
  (void)req;  // suppress unused variable warning
  const auto models = state_->modelList();

  Json::Value json;
  json["models"] = Json::arrayValue;
  for (const auto &model : models) {
    json["models"].append(model);
  }

  auto resp = HttpResponse::newHttpJsonResponse(json);
  callback(resp);
}

void HttpServer::hasHardware(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback) const {
  AMDINFER_LOG_INFO(logger_, "Received hasHardware request");
#ifdef AMDINFER_ENABLE_METRICS
  Metrics::getInstance().incrementCounter(MetricCounterIDs::RestGet);
#endif
  const auto &json = req->jsonObject();

  if (json == nullptr) {
    auto resp = errorHttpResponse("No JSON body in hasHardware request",
                                  HttpStatusCode::k400BadRequest);
    callback(resp);
    return;
  }

  auto found = SharedState::hasHardware(json->get("name", "").asString(),
                                        json->get("num", 1).asInt());

  auto resp = HttpResponse::newHttpResponse();
  if (!found) {
    resp->setStatusCode(drogon::k404NotFound);
  }
  callback(resp);
}

void HttpServer::modelInfer(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback,
  std::string const &model) const {
#ifdef AMDINFER_ENABLE_TRACING
  auto trace = startTrace(&(__func__[0]), req->getHeaders());
  trace->setAttribute("model", model);
#endif

  AMDINFER_LOG_INFO(logger_, "Received modelInfer request for " + model);
#ifdef AMDINFER_ENABLE_METRICS
  auto now = std::chrono::high_resolution_clock::now();
  Metrics::getInstance().incrementCounter(MetricCounterIDs::RestPost);
#endif

#ifdef AMDINFER_ENABLE_TRACING
  trace->startSpan("request_handler");
#endif

  try {
    auto request = std::make_unique<DrogonHttp>(req, std::move(callback));
#ifdef AMDINFER_ENABLE_METRICS
    request->setTime(now);
#endif
#ifdef AMDINFER_ENABLE_TRACING
    trace->endSpan();
    request->setTrace(std::move(trace));
#endif
    state_->modelInfer(model, std::move(request));
  } catch (const invalid_argument &e) {
    AMDINFER_LOG_INFO(logger_, e.what());
    auto resp = errorHttpResponse(e.what(), HttpStatusCode::k400BadRequest);
#ifdef AMDINFER_ENABLE_TRACING
    auto context = trace->propagate();
    propagate(resp.get(), context);
#endif
    callback(resp);
  }
}

void HttpServer::modelLoad(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback,
  const std::string &model) const {
  auto model_lower = util::toLower(model);
#ifdef AMDINFER_ENABLE_TRACING
  auto trace = startTrace(&(__func__[0]), req->getHeaders());
  trace->setAttribute("model", model_lower);
#endif
  AMDINFER_LOG_INFO(logger_, "Received modelLoad request for " + model_lower);

  auto json = req->getJsonObject();
  ParameterMapPtr parameters = nullptr;
  if (json != nullptr) {
    parameters = mapJsonToParameters(*json);
  } else {
    parameters = std::make_unique<ParameterMap>();
  }
#ifdef AMDINFER_ENABLE_TRACING
  trace->setAttributes(parameters.get());
#endif

  try {
    state_->modelLoad(model_lower, parameters.get());
  } catch (const runtime_error &e) {
    AMDINFER_LOG_ERROR(logger_, e.what());
    auto resp = errorHttpResponse(e.what(), HttpStatusCode::k400BadRequest);
#ifdef AMDINFER_ENABLE_TRACING
    auto context = trace->propagate();
    propagate(resp.get(), context);
#endif
    callback(resp);
  }

  auto resp = HttpResponse::newHttpResponse();
#ifdef AMDINFER_ENABLE_TRACING
  auto context = trace->propagate();
  propagate(resp.get(), context);
#endif
  callback(resp);
}

void HttpServer::modelUnload(
  [[maybe_unused]] const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback,
  const std::string &model) const {
  AMDINFER_LOG_INFO(logger_, "Received modelUnload request");
#ifdef AMDINFER_ENABLE_TRACING
  auto trace = startTrace(&(__func__[0]), req->getHeaders());
#endif

  auto model_lower = util::toLower(model);

#ifdef AMDINFER_ENABLE_TRACING
  trace->setAttribute("model", model_lower);
#endif

  state_->modelUnload(model_lower);

  auto resp = HttpResponse::newHttpResponse();
#ifdef AMDINFER_ENABLE_TRACING
  auto context = trace->propagate();
  propagate(resp.get(), context);
#endif
  callback(resp);
}

void HttpServer::workerLoad(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback,
  const std::string &worker) const {
  AMDINFER_LOG_INFO(logger_, "Received load request");
#ifdef AMDINFER_ENABLE_TRACING
  auto trace = startTrace(&(__func__[0]), req->getHeaders());
#endif

  auto json = req->getJsonObject();
  ParameterMapPtr parameters = nullptr;
  if (json != nullptr) {
    parameters = mapJsonToParameters(*json);
  } else {
    parameters = std::make_unique<ParameterMap>();
  }

  auto worker_lower = util::toLower(worker);

#ifdef AMDINFER_ENABLE_TRACING
  trace->setAttribute("model", worker_lower);
#endif
  AMDINFER_LOG_INFO(logger_, "Received load request is for " + worker_lower);

#ifdef AMDINFER_ENABLE_TRACING
  trace->setAttributes(parameters.get());
#endif
  HttpResponsePtr resp;
  try {
    auto endpoint = state_->workerLoad(worker_lower, parameters.get());
    resp = HttpResponse::newHttpResponse();
    resp->setBody(endpoint);
  } catch (const runtime_error &e) {
    AMDINFER_LOG_ERROR(logger_, e.what());
    resp = errorHttpResponse(e.what(), HttpStatusCode::k400BadRequest);
  }

#ifdef AMDINFER_ENABLE_TRACING
  auto context = trace->propagate();
  propagate(resp.get(), context);
#endif
  callback(resp);
}

void HttpServer::workerUnload(
  [[maybe_unused]] const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback,
  const std::string &worker) const {
#ifdef AMDINFER_ENABLE_TRACING
  auto trace = startTrace(&(__func__[0]), req->getHeaders());
#endif

  auto worker_lower = util::toLower(worker);

  AMDINFER_LOG_INFO(logger_, "Received unload request is for " + worker_lower);

#ifdef AMDINFER_ENABLE_TRACING
  trace->setAttribute("model", worker_lower);
#endif

  state_->workerUnload(worker_lower);

  auto resp = HttpResponse::newHttpResponse();
#ifdef AMDINFER_ENABLE_TRACING
  auto context = trace->propagate();
  propagate(resp.get(), context);
#endif
  callback(resp);
}

#endif  // AMDINFER_ENABLE_REST

#ifdef AMDINFER_ENABLE_METRICS
void HttpServer::metrics(
  const HttpRequestPtr &req,
  std::function<void(const HttpResponsePtr &)> &&callback) const {
  (void)req;  // suppress unused variable warning
  AMDINFER_LOG_INFO(logger_, "Received metrics request");
  std::string body = Metrics::getInstance().getMetrics();
  auto resp = drogon::HttpResponse::newHttpResponse();
  resp->setBody(body);
  resp->setContentTypeCode(drogon::ContentType::CT_TEXT_PLAIN);
  callback(resp);
}
#endif

}  // namespace amdinfer
