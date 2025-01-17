# Copyright 2021 Xilinx, Inc.
# Copyright 2022 Advanced Micro Devices, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os
import pathlib

import setuptools
import skbuild

version_path = os.getenv("AMDINFER_ROOT")
if version_path:
    version_path += "/VERSION"
    with open(version_path, "r") as f:
        version = f.read()
else:
    version = "0.0.0"

package = pathlib.Path("src/amdinfer/bindings/python/src")

skbuild.setup(
    name="amdinfer",
    version=version,
    license="Apache 2.0",
    packages=setuptools.find_packages(str(package)),
    install_requires=[
        "numpy",
        "opencv-python-headless",
    ],
    python_requires=">=3.6",
    package_dir={"": str(package)},
    include_package_data=True,
    cmake_install_dir=str(package / "amdinfer"),
    package_data={
        "": [
            "*.pyi",
        ]
    },
    zip_safe=False,  # required for mypy
)
