load("@rules_cc//cc:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

SOURCES = [
    "db/builder.cc",
    "db/c.cc",
    "db/dbformat.cc",
    "db/db_impl.cc",
    "db/db_iter.cc",
    "db/dumpfile.cc",
    "db/filename.cc",
    "db/log_reader.cc",
    "db/log_writer.cc",
    "db/memtable.cc",
    "db/repair.cc",
    "db/table_cache.cc",
    "db/version_edit.cc",
    "db/version_set.cc",
    "db/write_batch.cc",
    "table/block_builder.cc",
    "table/block.cc",
    "table/filter_block.cc",
    "table/format.cc",
    "table/iterator.cc",
    "table/merger.cc",
    "table/table_builder.cc",
    "table/table.cc",
    "table/two_level_iterator.cc",
    "util/arena.cc",
    "util/bloom.cc",
    "util/cache.cc",
    "util/coding.cc",
    "util/comparator.cc",
    "util/crc32c.cc",
    "util/env.cc",
    "util/env_posix.cc",
    "util/filter_policy.cc",
    "util/hash.cc",
    "util/histogram.cc",
    "util/logging.cc",
    "util/options.cc",
    "util/status.cc",
    "helpers/memenv/memenv.cc",
]

cc_library(
    name = "leveldb",
    srcs = SOURCES,
    hdrs = glob(
        [
            "helpers/memenv/*.h",
            "util/*.h",
            "port/*.h",
            "port/win/*.h",
            "table/*.h",
            "db/*.h",
            "include/leveldb/*.h",
        ],
        exclude = [
            "**/*test.*",
        ],
    ),
    copts = [
        "-fno-builtin-memcmp",
        "-DLEVELDB_PLATFORM_POSIX=1",
        "-DLEVELDB_ATOMIC_PRESENT",
    ],
    defines = [
        "LEVELDB_PLATFORM_POSIX",
    ] + select({
        "@bazel_tools//src/conditions:darwin": ["OS_MACOSX"],
        "//conditions:default": [],
    }),
    includes = [
        "include/",
    ],
)