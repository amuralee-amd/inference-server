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
 * @brief Defines the base batcher implementation
 */

#ifndef GUARD_AMDINFER_BATCHING_BATCHER
#define GUARD_AMDINFER_BATCHING_BATCHER

#include <chrono>   // for system_clock::time_point
#include <cstddef>  // for size_t
#include <memory>   // for unique_ptr, shared_ptr
#include <string>   // for string
#include <thread>   // for thread
#include <vector>   // for vector

#include "amdinfer/build_options.hpp"        // for AMDINFER_ENABLE_LOGGING
#include "amdinfer/core/parameters.hpp"      // for ParameterMap
#include "amdinfer/declarations.hpp"         // for BufferPtrs, InferenceReq...
#include "amdinfer/observation/logging.hpp"  // for LoggerPtr
#include "amdinfer/observation/tracing.hpp"  // for TracePtr
#include "amdinfer/util/queue.hpp"           // for BlockingConcurrentQueue

namespace amdinfer {
class Buffer;
class WorkerInfo;
}  // namespace amdinfer

namespace amdinfer {

enum class BatcherStatus { New, Run, Inactive, Dead };

/**
 * @brief The Batch is what the batcher produces and pushes to the workers. It
 * represents the requests, the buffers associated with the request and other
 * metadata that should be sent to the worker.
 *
 */
class Batch {
 public:
  explicit Batch(const WorkerInfo* worker);
  /// Copy constructor
  Batch(Batch const&) = delete;
  /// Copy assignment constructor
  Batch& operator=(const Batch&) = delete;
  /// Move constructor
  Batch(Batch&& other) = default;
  /// Move assignment constructor
  Batch& operator=(Batch&& other) = default;
  /// Destructor
  ~Batch();

  void addRequest(InferenceRequestPtr request);

  [[nodiscard]] const InferenceRequestPtr& getRequest(size_t index);
  [[nodiscard]] const std::vector<InferenceRequestPtr>& getRequests() const;
  [[nodiscard]] const BufferPtrs& getInputBuffers() const;
  [[nodiscard]] const BufferPtrs& getOutputBuffers() const;
  [[nodiscard]] std::vector<Buffer*> getRawInputBuffers() const;
  [[nodiscard]] std::vector<Buffer*> getRawOutputBuffers() const;

  [[nodiscard]] bool empty() const;
  [[nodiscard]] size_t size() const;
  [[nodiscard]] size_t getInputSize() const;
  [[nodiscard]] size_t getOutputSize() const;

#ifdef AMDINFER_ENABLE_TRACING
  void addTrace(TracePtr trace);
  TracePtr& getTrace(size_t index);
#endif
#ifdef AMDINFER_ENABLE_METRICS
  void addTime(std::chrono::high_resolution_clock::time_point timestamp);
  std::chrono::high_resolution_clock::time_point getTime(size_t index);
#endif

  [[nodiscard]] auto begin() const { return requests_.begin(); }
  [[nodiscard]] auto end() const { return requests_.end(); }

 private:
  const WorkerInfo* worker_;
  std::vector<InferenceRequestPtr> requests_;
  std::vector<BufferPtr> input_buffers_;
  std::vector<BufferPtr> output_buffers_;
#ifdef AMDINFER_ENABLE_TRACING
  std::vector<TracePtr> traces_;
#endif
#ifdef AMDINFER_ENABLE_METRICS
  std::vector<std::chrono::high_resolution_clock::time_point> start_times_;
#endif
};

using BatchPtr = std::unique_ptr<Batch>;
using BatchPtrQueue = BlockingQueue<BatchPtr>;

/**
 * @brief The base batcher implementation defines the basic structure of how
 * batchers behave in amdinfer. The run() method is purely virtual and must be
 * implemented by the child classes.
 *
 */
class Batcher {
 public:
  /// Construct a new Batcher object
  Batcher();
  explicit Batcher(ParameterMap* parameters);
  /**
   * @brief Construct a new Batcher object
   *
   * @param name the endpoint corresponding to the batcher's worker group
   */
  // explicit Batcher(const std::string& name);
  Batcher(const Batcher& batcher);              ///< copy constructor
  Batcher& operator=(const Batcher&) = delete;  ///< Copy assignment constructor
  Batcher(Batcher&& other) = delete;            ///< Move constructor
  Batcher& operator=(Batcher&& other) =
    delete;                      ///< Move assignment constructor
  virtual ~Batcher() = default;  ///< Destructor

  /**
   * @brief Start the batcher
   *
   * @param worker
   */
  void start(WorkerInfo* worker);
  /**
   * @brief Set the batch size for the batcher
   *
   * @param batch_size target batch size
   */
  void setBatchSize(size_t batch_size);
  /**
   * @brief Set the name of the batcher (i.e. the batcher's worker group
   * endpoint)
   *
   * @param name the endpoint
   */
  void setName(const std::string& name);
  /// Get the batcher's worker group name
  [[nodiscard]] std::string getName() const;

  /// Get the batcher's input queue (used to enqueue new requests)
  BlockingQueue<InterfacePtr>* getInputQueue();
  /// Get the batcher's output queue (used to push batches to the worker group)
  BatchPtrQueue* getOutputQueue();

  void run(WorkerInfo* worker);

  BatcherStatus getStatus();

  /**
   * @brief Enqueue a new request to the batcher
   *
   * @param request
   */
  void enqueue(InterfacePtr request);

  /// End the batcher
  void end();

 protected:
#ifdef AMDINFER_ENABLE_LOGGING
  [[nodiscard]] const Logger& getLogger() const;
#endif

  size_t batch_size_ = 1;
  std::shared_ptr<BlockingQueue<InterfacePtr>> input_queue_;
  std::shared_ptr<BatchPtrQueue> output_queue_;
  std::thread thread_;
  std::string model_;
  ParameterMap parameters_;

 private:
  /**
   * @brief The doRun method defines the exact process by which the batcher
   * consumes incoming Interface objects and uses them to create batches.
   *
   * @param worker pointer to this batcher's worker [group]
   */
  virtual void doRun(WorkerInfo* worker) = 0;

  BatcherStatus status_;

#ifdef AMDINFER_ENABLE_LOGGING
  Logger logger_{Loggers::Server};
#endif
};

}  // namespace amdinfer

#endif  // GUARD_AMDINFER_BATCHING_BATCHER
