// Copyright 2022 Xilinx Inc.
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
 * @brief Implements the Python bindings for the websocket client
 */

#include "proteus/clients/websocket.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <sstream>

#include "docstrings.hpp"

namespace py = pybind11;

using proteus::types::DataType;

void wrapWebSocketClient(py::module_ &m) {
  using proteus::WebSocketClient;

  py::class_<WebSocketClient, proteus::Client>(m, "WebSocketClient")
    .def(py::init<const std::string &, const std::string &>(),
         py::arg("ws_address"), py::arg("http_address"))
    .def("serverMetadata", &WebSocketClient::serverMetadata)
    .def("serverLive", &WebSocketClient::serverLive)
    .def("serverReady", &WebSocketClient::serverReady)
    .def("modelReady", &WebSocketClient::modelReady, py::arg("model"))
    .def("modelLoad", &WebSocketClient::modelLoad, py::arg("model"),
         py::arg("parameters") = proteus::RequestParameters())
    .def("modelUnload", &WebSocketClient::modelUnload, py::arg("model"))
    .def("modelInfer", &WebSocketClient::modelInfer, py::arg("model"),
         py::arg("request"))
    .def("modelInferAsync", &WebSocketClient::modelInferAsync, py::arg("model"),
         py::arg("request"))
    .def("modelRecv", &WebSocketClient::modelRecv)
    .def("modelList", &WebSocketClient::modelList)
    .def("close", &WebSocketClient::close);
}