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
 * @brief Implements the hard batcher
 */

#include "amdinfer/batching/hard.hpp"

#include <cstddef>  // for size_t
#include <memory>   // for unique_ptr, operator==
#include <string>   // for operator+
#include <utility>  // for move
#include <vector>   // for vector

#include "amdinfer/build_options.hpp"        // for AMDINFER_ENABLE_METRICS
#include "amdinfer/core/exceptions.hpp"      // for invalid_argument
#include "amdinfer/core/interface.hpp"       // for Interface
#include "amdinfer/declarations.hpp"         // for InterfacePtr
#include "amdinfer/observation/metrics.hpp"  // for Metrics, MetricCounterIDs
#include "amdinfer/observation/tracing.hpp"  // for Trace
#include "amdinfer/util/queue.hpp"           // for BlockingConcurrentQueue
#include "amdinfer/util/thread.hpp"          // for setThreadName

// IWYU pragma: no_forward_declare amdinfer::Buffer

namespace amdinfer {

void HardBatcher::doRun(WorkerInfo* worker) {
  auto thread_name = "batch" + this->getName();
  util::setThreadName(thread_name);
  InterfacePtr req;
  bool run = true;

  while (run) {
    auto batch = std::make_unique<Batch>(worker);
    auto input_buffers = batch->getRawInputBuffers();
    auto output_buffers = batch->getRawOutputBuffers();
    std::vector<size_t> input_offset(input_buffers.size(), 0);
    std::vector<size_t> output_offset(output_buffers.size(), 0);
    size_t batch_size = 0;

    do {
      this->input_queue_->wait_dequeue(req);

      if (req == nullptr) {
        run = false;
        break;
      }

#ifdef AMDINFER_ENABLE_TRACING
      auto trace = req->getTrace();
      trace->startSpan("hard_batcher");
#endif

#ifdef AMDINFER_ENABLE_METRICS
      Metrics::getInstance().incrementCounter(
        MetricCounterIDs::PipelineIngressBatcher);
#endif

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

      auto old_input_offset = input_offset;
      auto old_output_offset = output_offset;
      auto new_req = req->getRequest(input_buffers, input_offset,
                                     output_buffers, output_offset);
      if (new_req == nullptr) {
        input_offset = old_input_offset;
        output_offset = old_output_offset;
      } else {
        batch->addRequest(new_req);
        batch_size++;
#ifdef AMDINFER_ENABLE_TRACING
        trace->endSpan();
        batch->addTrace(std::move(trace));
#endif
#ifdef AMDINFER_ENABLE_METRICS
        batch->addTime(req->getTime());
#endif
      }
    } while (batch_size % this->batch_size_ != 0);

    if (!batch->empty()) {
      this->output_queue_->enqueue(std::move(batch));
#ifdef AMDINFER_ENABLE_METRICS
      Metrics::getInstance().incrementCounter(
        MetricCounterIDs::PipelineEgressBatcher);
#endif
    }
  }
}

}  // namespace amdinfer
