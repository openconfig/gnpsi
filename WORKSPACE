# Copyright 2024 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

workspace(name = "com_github_openconfig_gnpsi")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

### Bazel rules for many languages to compile PROTO into gRPC libraries
# Note: any version of this which is less than 4.3.0 requires bazel version 5.4.0 (set in .bazelversion file)
http_archive(
    name = "rules_proto_grpc",
    sha256 = "c0d718f4d892c524025504e67a5bfe83360b3a982e654bc71fed7514eb8ac8ad",
    strip_prefix = "rules_proto_grpc-4.6.0",
    urls = ["https://github.com/rules-proto-grpc/rules_proto_grpc/archive/4.6.0.tar.gz"],
)

# googleapis has not had a release since 2016 - take the master version as of 4-jan-23
http_archive(
    name = "com_google_googleapis",
    sha256 = "9fc03150d86501d7da35eefa989d5553bdd77a95cfe4373cdafe8eee92f6bfb1",
    strip_prefix = "googleapis-870a5ed7e141b4faf70e2a0858854e9b5bb18612",
    urls = ["https://github.com/googleapis/googleapis/archive/870a5ed7e141b4faf70e2a0858854e9b5bb18612.tar.gz"],
)

# -- Load GRPC Dependencies ----------------------------------------------------
load("@com_google_googleapis//:repository_rules.bzl", "switched_rules_by_language")

switched_rules_by_language(
    name = "com_google_googleapis_imports",
    cc = True,
    grpc = True,
    go = True,
)

load(
    "@rules_proto_grpc//:repositories.bzl",
    "bazel_gazelle",
    "io_bazel_rules_go",
    "rules_proto_grpc_repos",
    "rules_proto_grpc_toolchains",
)

rules_proto_grpc_toolchains()
rules_proto_grpc_repos()

load("@rules_proto//proto:repositories.bzl", "rules_proto_dependencies", "rules_proto_toolchains")

rules_proto_dependencies()
rules_proto_toolchains()

### Golang
io_bazel_rules_go()
load("@io_bazel_rules_go//go:deps.bzl", "go_register_toolchains", "go_rules_dependencies")
go_rules_dependencies()
go_register_toolchains(go_version = "1.20")

# gazelle:repo bazel_gazelle
bazel_gazelle()

# -- Load Dependencies ---------------------------------------------------------
load("gnpsi_deps.bzl", "gnpsi_deps")

gnpsi_deps()

load("@rules_proto_grpc//go:repositories.bzl", rules_proto_grpc_go_repos = "go_repos")

rules_proto_grpc_go_repos()

### C++
load("@rules_proto_grpc//cpp:repositories.bzl", rules_proto_grpc_cpp_repos = "cpp_repos")

rules_proto_grpc_cpp_repos()

load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")

grpc_deps()

# Load gazelle_dependencies last, so that the newer version of org_golang_google_grpc is used.
# see https://github.com/rules-proto-grpc/rules_proto_grpc/issues/160
load("@bazel_gazelle//:deps.bzl", "gazelle_dependencies")
gazelle_dependencies()