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
 * @brief Implements the VectorBuffer class
 */

#include "amdinfer/buffers/vector_buffer.hpp"

#include <algorithm>  // for max
#include <memory>     // for make_unique
#include <utility>    // for move

#include "amdinfer/declarations.hpp"  // for BufferPtrs

namespace amdinfer {

VectorBuffer::VectorBuffer(int elements, DataType data_type)
  : type_(data_type) {
  std::size_t size = elements * data_type.size();
  this->data_.reserve(size);
  // this->write_counter_ = 0;
}

void* VectorBuffer::data(size_t offset) {
  return static_cast<std::byte*>(this->data_.data()) + offset;
}
void VectorBuffer::reset() {}

// size_t VectorBuffer::write(bool value, size_t offset) {
//   constexpr auto bytes = types::getSize(DataType::Bool);
//   std::memcpy(this->data_.data() + offset, &value, bytes);
//   return offset + bytes;
// }
// size_t VectorBuffer::write(uint8_t value, size_t offset) {
//   constexpr auto bytes = types::getSize(DataType::Uint8);
//   std::memcpy(this->data_.data() + offset, &value, bytes);
//   return offset + bytes;
// }
// size_t VectorBuffer::write(uint16_t value, size_t offset) {
//   constexpr auto bytes = types::getSize(DataType::Uint16);
//   std::memcpy(this->data_.data() + offset, &value, bytes);
//   return offset + bytes;
// }
// size_t VectorBuffer::write(uint32_t value, size_t offset) {
//   constexpr auto bytes = types::getSize(DataType::Uint32);
//   std::memcpy(this->data_.data() + offset, &value, bytes);
//   return offset + bytes;
// }
// size_t VectorBuffer::write(uint64_t value, size_t offset) {
//   constexpr auto bytes = types::getSize(DataType::Uint64);
//   std::memcpy(this->data_.data() + offset, &value, bytes);
//   return offset + bytes;
// }
// size_t VectorBuffer::write(int8_t value, size_t offset) {
//   constexpr auto bytes = types::getSize(DataType::Int8);
//   std::memcpy(this->data_.data() + offset, &value, bytes);
//   return offset + bytes;
// }
// size_t VectorBuffer::write(int16_t value, size_t offset) {
//   constexpr auto bytes = types::getSize(DataType::Int16);
//   std::memcpy(this->data_.data() + offset, &value, bytes);
//   return offset + bytes;
// }
// size_t VectorBuffer::write(int32_t value, size_t offset) {
//   constexpr auto bytes = types::getSize(DataType::Int32);
//   std::memcpy(this->data_.data() + offset, &value, bytes);
//   return offset + bytes;
// }
// size_t VectorBuffer::write(int64_t value, size_t offset) {
//   constexpr auto bytes = types::getSize(DataType::Int64);
//   std::memcpy(this->data_.data() + offset, &value, bytes);
//   return offset + bytes;
// }
// size_t VectorBuffer::write(float value, size_t offset) {
//   constexpr auto bytes = types::getSize(DataType::Fp32);
//   std::memcpy(this->data_.data() + offset, &value, bytes);
//   return offset + bytes;
// }
// size_t VectorBuffer::write(double value, size_t offset) {
//   constexpr auto bytes = types::getSize(DataType::Fp64);
//   std::memcpy(this->data_.data() + offset, &value, bytes);
//   return offset + bytes;
// }
// // not quite sure what the best way to do this is. The commented out code is
// // other attempts. This works but may be non-optimal.
// size_t VectorBuffer::write(std::string value, size_t offset) {
//   // constexpr auto bytes = types::getSize(DataType::String);
//   // std::memcpy(this->data_.data() + offset, &value, bytes);

//   char null_term = '\0';
//   std::copy(value.begin(), value.end(),
//             reinterpret_cast<char*>(this->data_.data() + offset));
//   // strcpy(reinterpret_cast<char*>(this->data_.data() +
//   // this->write_counter_),
//   // value.c_str());
//   std::memcpy(this->data_.data() + offset + value.length(), &null_term, 1);

//   // this->write_counter_ += value.length() + 1;
//   return offset + value.length() + 1;
// }

// Status VectorBuffer::read(int index, bool& value) {
//   value = *reinterpret_cast<bool*>(
//     &this->data_[index * types::getSize(DataType::Bool)]);
//   return Status::kOK;
// }
// Status VectorBuffer::read(int index, uint8_t& value) {
//   value = *reinterpret_cast<uint8_t*>(
//     &this->data_[index * types::getSize(DataType::Uint8)]);
//   return Status::kOK;
// }
// Status VectorBuffer::read(int index, uint16_t& value) {
//   value = *reinterpret_cast<uint16_t*>(
//     &this->data_[index * types::getSize(DataType::Uint16)]);
//   return Status::kOK;
// }
// Status VectorBuffer::read(int index, uint32_t& value) {
//   value = *reinterpret_cast<uint32_t*>(
//     &this->data_[index * types::getSize(DataType::Uint32)]);
//   return Status::kOK;
// }
// Status VectorBuffer::read(int index, uint64_t& value) {
//   value = *reinterpret_cast<uint64_t*>(
//     &this->data_[index * types::getSize(DataType::Uint64)]);
//   return Status::kOK;
// }
// Status VectorBuffer::read(int index, int8_t& value) {
//   value = *reinterpret_cast<int8_t*>(
//     &this->data_[index * types::getSize(DataType::Int8)]);
//   return Status::kOK;
// }
// Status VectorBuffer::read(int index, int16_t& value) {
//   value = *reinterpret_cast<int16_t*>(
//     &this->data_[index * types::getSize(DataType::Int16)]);
//   return Status::kOK;
// }
// Status VectorBuffer::read(int index, int32_t& value) {
//   value = *reinterpret_cast<int32_t*>(
//     &this->data_[index * types::getSize(DataType::Int32)]);
//   return Status::kOK;
// }
// Status VectorBuffer::read(int index, int64_t& value) {
//   value = *reinterpret_cast<int64_t*>(
//     &this->data_[index * types::getSize(DataType::Int64)]);
//   return Status::kOK;
// }
// Status VectorBuffer::read(int index, float& value) {
//   value = *reinterpret_cast<float*>(
//     &this->data_[index * types::getSize(DataType::Fp32)]);
//   return Status::kOK;
// }
// Status VectorBuffer::read(int index, double& value) {
//   value = *reinterpret_cast<double*>(
//     &this->data_[index * types::getSize(DataType::Fp64)]);
//   return Status::kOK;
// }

void VectorBuffer::allocate(BufferPtrsQueue* my_buffer, size_t num,
                            size_t elements, DataType data_type) {
  for (size_t i = 0; i < num; i++) {
    BufferPtrs vec;
    vec.emplace_back(std::make_unique<VectorBuffer>(elements, data_type));
    my_buffer->enqueue(std::move(vec));
  }
}

}  // namespace amdinfer
