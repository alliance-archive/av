config_setting(
    name = "darwin",
    constraint_values = ["@bazel_tools//platforms:osx"],
)

filegroup(
    name = "headers",
    srcs = glob([
        "libavcodec/*.h",
        "libavformat/*.h",
        "libavutil/*.h",
        "config.h",
        "libavutil/avconfig.h",
        "libswscale/*.h",
        "**/*.hpp",
    ]),
    visibility = ["//visibility:public"],
)


cc_library(
    name = "avcodec",
    deps = [":avutil", "@x264//:x264", "@x265//:x265"],
    hdrs = glob(["libavcodec/*.h"]) + ["config.h"],
    srcs = ["libavcodec.a"],
    linkopts = select({
        ":darwin": ["-liconv"],
        "//conditions:default": [],
    }),
    includes = ["."],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "avformat",
    deps = [":avcodec"],
    hdrs = glob(["libavformat/*.h"]) + ["config.h"],
    srcs = ["libavformat.a"],
    linkopts = ["-lz"],
    includes = ["."],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "avutil",
    hdrs = glob(["libavutil/*.h"]) + ["config.h", "libavutil/avconfig.h"],
    srcs = ["libavutil.a"],
    linkopts = select({
        ":darwin": ["-framework CoreVideo"],
        "//conditions:default": [],
    }),
    includes = ["."],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "swscale",
    deps = [":avutil"],
    hdrs = glob(["libswscale/*.h"]) + ["config.h"],
    srcs = ["libswscale.a"],
    includes = ["."],
    visibility = ["//visibility:public"],
)

genrule(
    name = "ffmpeg-build",
    srcs = glob(["**/*"], exclude=["bazel-*"])
            + [
                "@nasm//:nasm",
                "@x264//:headers",
                "@x264//:libx264.a",
                "@x265//:include/x265.h",
                "@x265//:include/x265_config.h",
                "@x265//:lib/libx265.a",
                "@x265//:x265.pc",
            ],
    outs = [
        "config.h",
        "libavcodec.a",
        "libavformat.a",
        "libavutil.a",
        "libavutil/avconfig.h",
        "libswscale.a",
    ],
    cmd = """
        FFMPEG_ROOT=$$(pwd)/$$(dirname $(location configure))
        mkdir $$FFMPEG_ROOT/additional_includes
        cp $(locations @x264//:headers) $$FFMPEG_ROOT/additional_includes
        mkdir $$FFMPEG_ROOT/additional_libs
        cp $(location @x264//:libx264.a) $$FFMPEG_ROOT/additional_libs
        ASM=$$(pwd)/$(location @nasm//:nasm)
        X265_ROOT=$$(pwd)/$$(dirname $(location @x265//:x265.pc))
        sed "1 s+.*+prefix=$$X265_ROOT+" $(location @x265//:x265.pc) > $$FFMPEG_ROOT/x265.pc
        pushd $$FFMPEG_ROOT
            export PKG_CONFIG_PATH=$$FFMPEG_ROOT
            ./configure \
                --x86asmexe=$$ASM \
                --disable-everything \
                --disable-programs \
                --disable-doc \
                --disable-swresample \
                --enable-demuxer=mov \
                --enable-demuxer=mpegts \
                --enable-muxer=mpegts \
                --enable-decoder=aac \
                --enable-decoder=h264 \
                --enable-encoder=aac \
                --enable-encoder=libx264 \
                --enable-encoder=libx265 \
                --enable-protocol=file \
                --enable-gpl \
                --enable-libx264 \
                --enable-libx265 \
                --extra-cflags='-I./additional_includes -I.' \
                --extra-ldflags='-L./additional_libs -L.'\
                --pkg-config-flags="--static" \
            && make -j8
        popd
        cp $$FFMPEG_ROOT/config.h $(location config.h)
        cp $$FFMPEG_ROOT/libavcodec/libavcodec.a $(location libavcodec.a)
        cp $$FFMPEG_ROOT/libavformat/libavformat.a $(location libavformat.a)
        cp $$FFMPEG_ROOT/libavutil/libavutil.a $(location libavutil.a)
        cp $$FFMPEG_ROOT/libavutil/avconfig.h $(location libavutil/avconfig.h)
        cp $$FFMPEG_ROOT/libswscale/libswscale.a $(location libswscale.a)
    """,
    visibility = ["//visibility:public"],
)
