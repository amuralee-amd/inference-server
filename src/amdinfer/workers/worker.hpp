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
 * @brief Defines the Worker class
 */

#ifndef GUARD_AMDINFER_WORKERS_WORKER
#define GUARD_AMDINFER_WORKERS_WORKER

#include <limits>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "amdinfer/batching/soft.hpp"
#include "amdinfer/buffers/buffer.hpp"
#include "amdinfer/build_options.hpp"
#include "amdinfer/core/predict_api.hpp"
#include "amdinfer/observation/logging.hpp"

namespace amdinfer {

constexpr auto kNumBufferAuto = -1;

namespace workers {

enum class WorkerStatus {
  New,
  Init,
  Allocate,
  Acquire,
  Run,
  Inactive,
  Release,
  Deallocate,
  Destroy,
  Dead
};

/**
 * @brief All workers should extend the Worker class which defines the methods
 * to create, start, and run workers over their lifecycle.
 */
class Worker {
 public:
  /// The constructor only initializes the private class members
  Worker(const std::string& name, const std::string& platform)
    : metadata_(name, platform) {
    this->status_ = WorkerStatus::New;
    this->input_buffers_ = nullptr;
    this->output_buffers_ = nullptr;
    this->max_buffer_num_ = UINT_MAX;
    this->batch_size_ = 1;
  }
  virtual ~Worker() = default;  ///< Destroy the Worker object

  /**
   * @brief Starts the worker's run() method as a separate thread and returns it
   *
   * @param input_queue queue used to send requests to the started worker thread
   * @return std::thread
   */
  virtual std::thread spawn(BatchPtrQueue* input_queue) = 0;

  /// Perform low-cost initialization of the worker
  void init(ParameterMap* parameters) {
    this->status_ = WorkerStatus::Init;
    this->doInit(parameters);
  }
  /// Allocate some buffers that are used to hold input and output data
  size_t allocate(size_t num) {
    this->status_ = WorkerStatus::Allocate;
    return this->doAllocate(num);
  }
  /// Acquire any hardware resources or perform high-cost initialization
  void acquire(ParameterMap* parameters) {
    this->status_ = WorkerStatus::Acquire;
    this->doAcquire(parameters);
    this->metadata_.setReady(true);
  }
  /**
   * @brief The main body of the worker executes the work
   *
   * @param input_queue queue that receives incoming requests
   */
  void run(BatchPtrQueue* input_queue) {
    this->status_ = WorkerStatus::Run;
    this->doRun(input_queue);
    this->status_ = WorkerStatus::Inactive;
  }
  /// Release any hardware resources
  void release() {
    this->status_ = WorkerStatus::Release;
    this->metadata_.setReady(false);
    this->doRelease();
  }
  /// Free the input and output buffers
  void deallocate() {
    this->status_ = WorkerStatus::Deallocate;
    this->doDeallocate();
    this->input_buffers_ = nullptr;
    this->output_buffers_ = nullptr;
  }
  /// Perform any final operations before the worker thread is joined
  void destroy() {
    this->status_ = WorkerStatus::Destroy;
    this->doDestroy();
    this->status_ = WorkerStatus::Dead;
  }

  /**
   * @brief Return buffers to the worker's buffer pool after use. The buffers'
   * reset() method is called prior to returning.
   *
   * @param input_buffers
   * @param output_buffers
   */
  void returnBuffers(std::unique_ptr<std::vector<BufferPtrs>> input_buffers,
                     std::unique_ptr<std::vector<BufferPtrs>> output_buffers) {
    this->input_buffers_->enqueue_bulk(
      std::make_move_iterator(input_buffers->begin()), input_buffers->size());
    this->output_buffers_->enqueue_bulk(
      std::make_move_iterator(output_buffers->begin()), output_buffers->size());
  }

  void setInputBuffers(BufferPtrsQueue* buffers) {
    this->input_buffers_ = buffers;
  }

  void setOutputBuffers(BufferPtrsQueue* buffers) {
    this->output_buffers_ = buffers;
  }

  [[nodiscard]] uint32_t getMaxBufferNum() const {
    return this->max_buffer_num_;
  }

  [[nodiscard]] size_t getBatchSize() const { return this->batch_size_; }
  [[nodiscard]] WorkerStatus getStatus() const { return this->status_; }

  virtual std::vector<std::unique_ptr<Batcher>> makeBatcher(
    int num, ParameterMap* parameters) {
    return this->makeBatcher<SoftBatcher>(num, parameters);
  }

  template <typename T>
  std::vector<std::unique_ptr<Batcher>> makeBatcher(int num,
                                                    ParameterMap* parameters) {
    std::vector<std::unique_ptr<Batcher>> batchers;
    batchers.emplace_back(std::make_unique<T>(parameters));
    for (int i = 1; i < num; i++) {
      batchers.push_back(
        std::make_unique<T>(*dynamic_cast<T*>(batchers.back().get())));
    }
    return batchers;
  }

  ModelMetadata getMetadata() const { return this->metadata_; }

 protected:
#ifdef AMDINFER_ENABLE_LOGGING
  [[nodiscard]] const Logger& getLogger() const { return logger_; };
#endif

  BufferPtrsQueue* input_buffers_;
  BufferPtrsQueue* output_buffers_;
  uint32_t max_buffer_num_;
  size_t batch_size_;
  ModelMetadata metadata_;

 private:
  /// Perform low-cost initialization of the worker
  virtual void doInit(ParameterMap* parameters) = 0;
  /// Allocate some buffers that are used to hold input and output data
  virtual size_t doAllocate(size_t num) = 0;
  /// Acquire any hardware resources or perform high-cost initialization
  virtual void doAcquire(ParameterMap* parameters) = 0;
  /**
   * @brief The main body of the worker executes the work
   *
   * @param input_queue queue that receives incoming requests
   */
  virtual void doRun(BatchPtrQueue* input_queue) = 0;
  /// Release any hardware resources
  virtual void doRelease() = 0;
  /// Free the input and output buffers
  virtual void doDeallocate() = 0;
  /// Perform any final operations before the worker's run thread is joined
  virtual void doDestroy() = 0;

#ifdef AMDINFER_ENABLE_LOGGING
  Logger logger_{Loggers::Server};
#endif

  WorkerStatus status_;
};

}  // namespace workers

}  // namespace amdinfer

#endif  // GUARD_AMDINFER_WORKERS_WORKER
