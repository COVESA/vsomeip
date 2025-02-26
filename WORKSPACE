workspace(name = "vsomeip")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

BOOST_VERSION = "1.82.0"

BOOST_VERSION_us = BOOST_VERSION.replace(".", "_")

http_archive(
    name = "boost",
    build_file = "@//:boost.BUILD",
    sha256 = "66a469b6e608a51f8347236f4912e27dc5c60c60d7d53ae9bfe4683316c6f04c",
    strip_prefix = "boost_{version_underscore}".format(version_underscore = BOOST_VERSION_us),
    urls = ["https://mirror.bazel.build/boostorg.jfrog.io/artifactory/main/release/{version}/source/boost_{version_underscore}.tar.gz".format(
        version = BOOST_VERSION,
        version_underscore = BOOST_VERSION_us,
    )],
)

GOOGLETEST_VERSION = "1.11.0"

http_archive(
    name = "googletest",
    sha256 = "353571c2440176ded91c2de6d6cd88ddd41401d14692ec1f99e35d013feda55a",
    strip_prefix = "googletest-release-{version}".format(version = GOOGLETEST_VERSION),
    urls = ["https://github.com/google/googletest/archive/refs/tags/release-{version}.zip".format(version = GOOGLETEST_VERSION)],
)

# GOOGLETEST dependencies, taken from: https://raw.githubusercontent.com/google/googletest/release-1.11.0/WORKSPACE
http_archive(
    name = "com_google_absl",
    sha256 = "aeba534f7307e36fe084b452299e49b97420667a8d28102cf9a0daeed340b859",
    strip_prefix = "abseil-cpp-7971fb358ae376e016d2d4fc9327aad95659b25e",
    urls = ["https://github.com/abseil/abseil-cpp/archive/7971fb358ae376e016d2d4fc9327aad95659b25e.zip"],  # 2021-05-20T02:59:16Z
)

http_archive(
    name = "rules_cc",
    sha256 = "1e19e9a3bc3d4ee91d7fcad00653485ee6c798efbbf9588d40b34cbfbded143d",
    strip_prefix = "rules_cc-68cb652a71e7e7e2858c50593e5a9e3b94e5b9a9",
    urls = ["https://github.com/bazelbuild/rules_cc/archive/68cb652a71e7e7e2858c50593e5a9e3b94e5b9a9.zip"],  # 2021-05-14T14:51:14Z
)

http_archive(
    name = "rules_python",
    sha256 = "98b3c592faea9636ac8444bfd9de7f3fb4c60590932d6e6ac5946e3f8dbd5ff6",
    strip_prefix = "rules_python-ed6cc8f2c3692a6a7f013ff8bc185ba77eb9b4d2",
    urls = ["https://github.com/bazelbuild/rules_python/archive/ed6cc8f2c3692a6a7f013ff8bc185ba77eb9b4d2.zip"],  # 2021-05-17T00:24:16Z
)
