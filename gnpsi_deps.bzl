# Copyright 2024 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
"""Dependencies to build gnpsi."""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def gnpsi_deps():
    """Declare the third-party dependencies necessary to build gnpsi"""
    if not native.existing_rule("com_github_grpc_grpc"):
        http_archive(
            name = "com_github_grpc_grpc",
            url = "https://github.com/grpc/grpc/archive/v1.61.0.zip",
            strip_prefix = "grpc-1.61.0",
            sha256 = "ba6c53c3924a1d01c663352010e0f73736bad3d99d72108e0f2b1a6466f9be20",
            patch_args = ["-p1"],
            patches = [
                "@com_github_openconfig_gnpsi//:bazel/patches/grpc-001-fix_file_watcher_race_condition.patch",
            ],
        )
    if not native.existing_rule("com_google_absl"):
        http_archive(
            name = "com_google_absl",
            url = "https://github.com/abseil/abseil-cpp/archive/20230802.0.tar.gz",
            strip_prefix = "abseil-cpp-20230802.0",
            sha256 = "59d2976af9d6ecf001a81a35749a6e551a335b949d34918cfade07737b9d93c5",
        )
    if not native.existing_rule("com_google_googletest"):
        http_archive(
            name = "com_google_googletest",
            urls = ["https://github.com/google/googletest/archive/release-1.11.0.tar.gz"],
            strip_prefix = "googletest-release-1.11.0",
            sha256 = "b4870bf121ff7795ba20d20bcdd8627b8e088f2d1dab299a031c1034eddc93d5",
        )
    if not native.existing_rule("com_google_protobuf"):
        http_archive(
            name = "com_google_protobuf",
            url = "https://github.com/protocolbuffers/protobuf/archive/refs/tags/v25.2.zip",
            strip_prefix = "protobuf-25.2",
            sha256 = "ddd0f5271f31b549efc74eb39061e142132653d5d043071fcec265bd571e73c4",
        )
    if not native.existing_rule("com_google_googleapis"):
        http_archive(
            name = "com_google_googleapis",
            url = "https://github.com/googleapis/googleapis/archive/f405c718d60484124808adb7fb5963974d654bb4.zip",
            strip_prefix = "googleapis-f405c718d60484124808adb7fb5963974d654bb4",
            sha256 = "406b64643eede84ce3e0821a1d01f66eaf6254e79cb9c4f53be9054551935e79",
        )
    if not native.existing_rule("com_github_google_glog"):
        http_archive(
            name = "com_github_google_glog",
            url = "https://github.com/google/glog/archive/v0.6.0.tar.gz",
            strip_prefix = "glog-0.6.0",
            sha256 = "8a83bf982f37bb70825df71a9709fa90ea9f4447fb3c099e1d720a439d88bad6",
        )

    # Needed to make glog happy.
    if not native.existing_rule("com_github_gflags_gflags"):
        http_archive(
            name = "com_github_gflags_gflags",
            url = "https://github.com/gflags/gflags/archive/v2.2.2.tar.gz",
            strip_prefix = "gflags-2.2.2",
            sha256 = "34af2f15cf7367513b352bdcd2493ab14ce43692d2dcd9dfc499492966c64dcf",
        )
    if not native.existing_rule("rules_proto"):
        http_archive(
            name = "rules_proto",
            urls = [
                "https://github.com/bazelbuild/rules_proto/archive/5.3.0-21.7.tar.gz",
            ],
            strip_prefix = "rules_proto-5.3.0-21.7",
            sha256 = "dc3fb206a2cb3441b485eb1e423165b231235a1ea9b031b4433cf7bc1fa460dd",
        )
