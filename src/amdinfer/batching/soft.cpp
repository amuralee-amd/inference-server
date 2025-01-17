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
 * @brief Implements the soft batcher
 */

#include "amdinfer/batching/soft.hpp"

#include <algorithm>  // for max
#include <cstddef>    // for size_t
#include <cstdint>    // for int32_t
#include <memory>     // for unique_ptr, allocator
#include <ratio>      // for ratio
#include <string>     // for operator+, char_traits
#include <utility>    // for move
#include <vector>     // for vector

#include "amdinfer/build_options.hpp"        // for AMDINFER_ENABLE_METRICS
#include "amdinfer/core/exceptions.hpp"      // for invalid_argument
#include "amdinfer/core/interface.hpp"       // for Interface
#include "amdinfer/core/parameters.hpp"      // for ParameterMap
#include "amdinfer/declarations.hpp"         // for InterfacePtr
#include "amdinfer/observation/logging.hpp"  // for Logger, AMDINFER_LOG_DEBUG
#include "amdinfer/observation/metrics.hpp"  // for Metrics, MetricCounterIDs
#include "amdinfer/observation/tracing.hpp"  // for Trace
#include "amdinfer/util/queue.hpp"           // for BlockingConcurrentQueue
#include "amdinfer/util/thread.hpp"          // for setThreadName
#include "amdinfer/util/timer.hpp"           // for Timer

// default batcher timeout in milliseconds
constexpr auto kDefaultTimeout = 100;

namespace amdinfer {

void SoftBatcher::doRun(WorkerInfo* worker) {
  auto thread_name = "batch" + this->getName();
  util::setThreadName(thread_name);
#ifdef AMDINFER_ENABLE_LOGGING
  const auto& logger = this->getLogger();
#endif

  bool run = true;

  auto timeout = kDefaultTimeout;
  if (this->parameters_.has("timeout")) {
    timeout = this->parameters_.get<int32_t>("timeout");
  }

  while (run) {
    auto batch = std::make_unique<Batch>(worker);
    auto input_buffers = batch->getRawInputBuffers();
    auto output_buffers = batch->getRawOutputBuffers();
    std::vector<size_t> input_offset(input_buffers.size(), 0);
    std::vector<size_t> output_offset(output_buffers.size(), 0);
    size_t batch_size = 0;

#ifdef AMDINFER_ENABLE_METRICS
    Metrics::getInstance().setGauge(
      MetricGaugeIDs::QueuesBatcherInput,
      static_cast<double>(input_queue_->size_approx()));
    Metrics::getInstance().setGauge(
      MetricGaugeIDs::QueuesBatcherOutput,
      static_cast<double>(output_queue_->size_approx()));
#endif

    bool first_request = true;
    util::Timer timer{true};

    do {
      InterfacePtr req;
      if (first_request) {
        // wait for the first request
        this->input_queue_->wait_dequeue(req);
        timer.add("start");
        AMDINFER_LOG_DEBUG(logger,
                           "Got request of a new batch for " + this->model_);
      } else {
        timer.stop();

        auto remaining_time = timeout - timer.count<std::milli, int>();
        auto duration = std::max(remaining_time, 0);
        bool valid = this->input_queue_->wait_dequeue_timed(req, duration);
        if (!valid) {
          break;
        }
      }

      if (req == nullptr) {
        run = false;
        break;
      }

      auto input_size = req->getInputSize();
      if (input_size != input_buffers.size()) {
        auto e =
          invalid_argument("Number of input tensors do not match worker");
        req->errorHandler(e);
        continue;
      }
      if (input_size == 0) {
        auto error = invalid_argument("Input size is zero");
        req->errorHandler(error);
        continue;
      }

#ifdef AMDINFER_ENABLE_TRACING
      auto trace = req->getTrace();
      trace->startSpan("soft_batcher");
#endif

#ifdef AMDINFER_ENABLE_METRICS
      Metrics::getInstance().incrementCounter(
        MetricCounterIDs::PipelineIngressBatcher);
#endif

      auto old_input_offset = input_offset;
      auto old_output_offset = output_offset;
      auto new_req = req->getRequest(input_buffers, input_offset,
                                     output_buffers, output_offset);
      if (new_req == nullptr) {
        AMDINFER_LOG_DEBUG(logger, "Making request for " + this->model_ +
                                     " failed. Reverting buffers.");
        input_offset = old_input_offset;
        output_offset = old_output_offset;
      } else {
        batch->addRequest(new_req);
        batch_size++;
        if (first_request) {
          first_request = false;
        }
#ifdef AMDINFER_ENABLE_TRACING
        trace->endSpan();
        batch->addTrace(std::move(trace));
#endif
#ifdef AMDINFER_ENABLE_METRICS
        batch->addTime(req->getTime());
#endif
      }
    } while (batch_size % this->batch_size_ != 0 && run);

    if (!batch->empty()) {
      AMDINFER_LOG_DEBUG(logger, "Enqueuing batch for " + this->model_ +
                                   " of size " + std::to_string(batch_size));
      this->output_queue_->enqueue(std::move(batch));
#ifdef AMDINFER_ENABLE_METRICS
      Metrics::getInstance().incrementCounter(
        MetricCounterIDs::PipelineEgressBatcher);
#endif
    }
  }
}

}  // namespace amdinfer
