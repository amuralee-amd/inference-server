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
 * @brief Implements the objects used to hold incoming inference requests
 */

#include "amdinfer/core/predict_api.hpp"

#include <algorithm>  // for copy
#include <cstring>    // for memcpy
#include <iterator>   // for back_insert_iterator, back_inse...
#include <utility>    // for pair, make_pair, move

#include "amdinfer/build_options.hpp"    // for AMDINFER_ENABLE_TRACING
#include "amdinfer/util/containers.hpp"  // for containerProduct
#include "amdinfer/util/memory.hpp"      // for copy

namespace amdinfer {

void InferenceRequest::setCallback(Callback &&callback) {
  callback_ = std::move(callback);
}

void InferenceRequest::runCallbackOnce(const InferenceResponse &response) {
  if (this->callback_ != nullptr) {
    (this->callback_)(response);
    this->callback_ = nullptr;
  }
}

void InferenceRequest::runCallback(const InferenceResponse &response) {
  (this->callback_)(response);
}

void InferenceRequest::runCallbackError(std::string_view error_msg) {
  this->runCallback(InferenceResponse(std::string{error_msg}));
}

void InferenceRequest::addInputTensor(void *data,
                                      const std::vector<uint64_t> &shape,
                                      DataType data_type,
                                      const std::string &name) {
  this->inputs_.emplace_back(data, shape, data_type, name);
}

void InferenceRequest::addInputTensor(InferenceRequestInput input) {
  this->inputs_.push_back(std::move(input));
}

const std::vector<InferenceRequestInput> &InferenceRequest::getInputs() const {
  return this->inputs_;
}

size_t InferenceRequest::getInputSize() const { return this->inputs_.size(); }

const std::vector<InferenceRequestOutput> &InferenceRequest::getOutputs()
  const {
  return this->outputs_;
}

void InferenceRequest::addOutputTensor(const InferenceRequestOutput &output) {
  this->outputs_.push_back(output);
}

InferenceRequestInput::InferenceRequestInput(void *data,
                                             std::vector<uint64_t> shape,
                                             DataType data_type,
                                             std::string name)
  : data_type_(data_type) {
  this->data_ = data;
  this->shape_ = std::move(shape);
  this->name_ = std::move(name);
  this->parameters_ = std::make_unique<ParameterMap>();
}

InferenceRequestInput::InferenceRequestInput() : data_type_(DataType::Uint32) {
  this->data_ = nullptr;
  this->name_ = "";
  this->parameters_ = std::make_unique<ParameterMap>();
}

void InferenceRequestInput::setName(std::string name) {
  this->name_ = std::move(name);
}

void InferenceRequestInput::setDatatype(DataType type) {
  this->data_type_ = type;
}

void InferenceRequestInput::setData(void *buffer) { this->data_ = buffer; }

void InferenceRequestInput::setData(std::vector<std::byte> &&buffer) {
  this->shared_data_ = std::move(buffer);
}

bool InferenceRequestInput::sharedData() const {
  return this->shared_data_.empty();
}

size_t InferenceRequestInput::getSize() const {
  return util::containerProduct(shape_);
}

void *InferenceRequestInput::getData() const {
  if (!this->shared_data_.empty()) {
    return (void *)shared_data_.data();  // NOLINT(google-readability-casting)
  }
  return this->data_;
}

struct InferenceRequestInputSizes {
  size_t name;
  size_t shape;
  size_t data_type;
  size_t parameters;
  size_t data;
  size_t shared_data;
};

size_t InferenceRequestInput::serializeSize() const {
  auto size = sizeof(InferenceRequestInputSizes);
  size += name_.length();
  size += shape_.size() * sizeof(uint64_t);
  size += sizeof(uint8_t);
  size += parameters_->serializeSize();
  if (!shared_data_.empty()) {
    size += shared_data_.size();
  } else {
    size += sizeof(data_);
  }
  return size;
}

void InferenceRequestInput::serialize(std::byte *data_out) const {
  InferenceRequestInputSizes metadata{name_.length(),
                                      shape_.size() * sizeof(uint64_t),
                                      sizeof(uint8_t),
                                      parameters_->serializeSize(),
                                      0,
                                      0};
  if (!shared_data_.empty()) {
    metadata.shared_data = this->getSize() * data_type_.size();
  } else {
    metadata.data = sizeof(data_);
  }
  data_out = copy(metadata, data_out, sizeof(InferenceRequestInputSizes));
  data_out = copy(name_.c_str(), data_out, metadata.name);
  data_out = copy(shape_.data(), data_out, metadata.shape);
  data_out =
    copy(static_cast<uint8_t>(data_type_), data_out, metadata.data_type);
  parameters_->serialize(data_out);
  data_out += metadata.parameters;
  if (!shared_data_.empty()) {
    amdinfer::copy(shared_data_.data(), data_out, metadata.shared_data);
  } else {
    std::memcpy(data_out, &data_, metadata.data);
  }
}

void InferenceRequestInput::deserialize(const std::byte *data_in) {
  const auto metadata =
    *reinterpret_cast<const InferenceRequestInputSizes *>(data_in);
  data_in += sizeof(InferenceRequestInputSizes);

  name_.resize(metadata.name);
  std::memcpy(name_.data(), data_in, metadata.name);
  data_in += metadata.name;

  shape_.resize(metadata.shape / sizeof(uint64_t));
  std::memcpy(shape_.data(), data_in, metadata.shape);
  data_in += metadata.shape;

  uint8_t type = 0;
  std::memcpy(&type, data_in, metadata.data_type);
  data_in += metadata.data_type;
  data_type_ = static_cast<DataType::Value>(type);

  parameters_ = std::make_shared<ParameterMap>();
  parameters_->deserialize(data_in);
  data_in += parameters_->serializeSize();

  if (metadata.shared_data != 0) {
    shared_data_.resize(metadata.shared_data);
    std::memcpy(shared_data_.data(), data_in, metadata.shared_data);
  } else {
    std::memcpy(&data_, data_in, metadata.data);
  }
}

InferenceRequestOutput::InferenceRequestOutput() {
  this->data_ = nullptr;
  this->name_ = "";
  this->parameters_ = std::make_unique<ParameterMap>();
}

void InferenceRequestOutput::setName(const std::string &name) {
  this->name_ = name;
}

InferenceResponse::InferenceResponse() {
  this->parameters_ = std::make_unique<ParameterMap>();
}

InferenceResponse::InferenceResponse(const std::string &error_msg) {
  this->parameters_ = nullptr;
  this->error_msg_ = error_msg;
}

void InferenceResponse::setID(const std::string &id) { this->id_ = id; }

void InferenceResponse::setModel(const std::string &model) {
  this->model_ = model;
}

std::string InferenceResponse::getModel() { return this->model_; }

bool InferenceResponse::isError() const { return !this->error_msg_.empty(); }

std::string InferenceResponse::getError() const { return this->error_msg_; }

void InferenceResponse::addOutput(const InferenceResponseOutput &output) {
  this->outputs_.push_back(output);
}

std::vector<InferenceResponseOutput> InferenceResponse::getOutputs() const {
  return this->outputs_;
}

#ifdef AMDINFER_ENABLE_TRACING
void InferenceResponse::setContext(StringMap &&context) {
  this->context_ = std::move(context);
}

const StringMap &InferenceResponse::getContext() const {
  return this->context_;
}
#endif

ModelMetadataTensor::ModelMetadataTensor(const std::string &name,
                                         DataType datatype,
                                         std::vector<uint64_t> shape)
  : datatype_(datatype) {
  this->name_ = name;
  this->shape_ = std::move(shape);
}

const std::string &ModelMetadataTensor::getName() const { return this->name_; }

const DataType &ModelMetadataTensor::getDataType() const {
  return this->datatype_;
}
const std::vector<uint64_t> &ModelMetadataTensor::getShape() const {
  return this->shape_;
}

ModelMetadata::ModelMetadata(const std::string &name,
                             const std::string &platform) {
  this->name_ = name;
  this->platform_ = platform;
  this->ready_ = false;
}

void ModelMetadata::addInputTensor(const std::string &name, DataType datatype,
                                   std::initializer_list<uint64_t> shape) {
  this->inputs_.emplace_back(name, datatype, shape);
}

void ModelMetadata::addInputTensor(const std::string &name, DataType datatype,
                                   std::vector<int> shape) {
  std::vector<uint64_t> new_shape;
  std::copy(shape.begin(), shape.end(), std::back_inserter(new_shape));
  this->inputs_.emplace_back(name, datatype, new_shape);
}

void ModelMetadata::addOutputTensor(const std::string &name, DataType datatype,
                                    std::initializer_list<uint64_t> shape) {
  this->outputs_.emplace_back(name, datatype, shape);
}

void ModelMetadata::addOutputTensor(const std::string &name, DataType datatype,
                                    std::vector<int> shape) {
  std::vector<uint64_t> new_shape;
  std::copy(shape.begin(), shape.end(), std::back_inserter(new_shape));
  this->outputs_.emplace_back(name, datatype, new_shape);
}

const std::string &ModelMetadata::getName() const { return this->name_; }
void ModelMetadata::setName(const std::string &name) { this->name_ = name; }

void ModelMetadata::setReady(bool ready) { this->ready_ = ready; }

bool ModelMetadata::isReady() const { return this->ready_; }

const std::string &ModelMetadata::getPlatform() const {
  return this->platform_;
}

const std::vector<ModelMetadataTensor> &ModelMetadata::getInputs() const {
  return this->inputs_;
}

const std::vector<ModelMetadataTensor> &ModelMetadata::getOutputs() const {
  return this->outputs_;
}

}  // namespace amdinfer
