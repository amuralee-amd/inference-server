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
 * @brief Implements a fake WorkerInfo class - most methods are empty and the
 * worker always returns a new BufferPtrs when a new buffer is requested.
 */

#include <cstddef>  // for size_t
#include <cstdint>  // for int32_t
#include <memory>   // for unique_ptr, make_unique
#include <string>   // for allocator, string
#include <thread>   // for thread::id, thread

#include "amdinfer/batching/batcher.hpp"  // for Batcher
#include "amdinfer/buffers/buffer.hpp"    // IWYU pragma: keep
#include "amdinfer/core/parameters.hpp"   // for ParameterMap
#include "amdinfer/core/worker_info.hpp"  // for WorkerInfo
#include "amdinfer/declarations.hpp"      // for BufferPtrs
#include "amdinfer/util/queue.hpp"        // for BufferPtrsQueue, BufferP...

namespace amdinfer {

constexpr auto kDefaultBufferNum = 10;

WorkerInfo::WorkerInfo(const std::string& name, ParameterMap* parameters) {
  this->input_buffer_ptr_ = std::make_unique<BufferPtrsQueue>();
  this->output_buffer_ptr_ = std::make_unique<BufferPtrsQueue>();
  this->buffer_num_ = kDefaultBufferNum;
  this->max_buffer_num_ = kDefaultBufferNum;
  this->batch_size_ = 1;

  this->addAndStartWorker(name, parameters);
}

WorkerInfo::~WorkerInfo() = default;

void WorkerInfo::addAndStartWorker(const std::string& name,
                                   ParameterMap* parameters) {
  (void)name;
  if (parameters == nullptr) {
    return;
  }
  if (parameters->has("batch_size")) {
    this->batch_size_ = parameters->get<int32_t>("batch_size");
  }
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
Batcher* WorkerInfo::getBatcher() { return nullptr; }

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
void WorkerInfo::join([[maybe_unused]] std::thread::id id) {}

void WorkerInfo::joinAll() {}

void WorkerInfo::unload() {}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
size_t WorkerInfo::getGroupSize() const { return 1; }

void WorkerInfo::shutdown() {}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
BufferPtrs WorkerInfo::getInputBuffer() const {
  BufferPtrs buffer;
  return buffer;
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
BufferPtrs WorkerInfo::getOutputBuffer() const {
  BufferPtrs buffer;
  return buffer;
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
void WorkerInfo::putInputBuffer([[maybe_unused]] BufferPtrs&& buffer) const {}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
void WorkerInfo::putOutputBuffer([[maybe_unused]] BufferPtrs&& buffer) const {}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
bool WorkerInfo::inputSizeValid([[maybe_unused]] size_t size) const {
  return true;
}

void WorkerInfo::allocate([[maybe_unused]] size_t request_size) {}

}  // namespace amdinfer
