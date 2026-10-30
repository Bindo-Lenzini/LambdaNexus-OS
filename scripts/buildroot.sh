#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILDROOT_DIR="$ROOT/.buildroot/source"
OUTPUT_DIR="$ROOT/.buildroot/output"
DOWNLOAD_DIR="$ROOT/.buildroot/downloads"
EXTERNAL_DIR="$ROOT/buildroot-external"
DEFCONFIG="$EXTERNAL_DIR/configs/lambdanexus_qemu_aarch64_defconfig"
BUILDROOT_COMMIT=137a566bf5353831d97c068a4c22e6df5b4d07a0
BUILDROOT_TAG=2025.02.12
QEMU=${QEMU:-qemu-system-aarch64}
RELEASE_URL=https://github.com/Bindo-Lenzini/LambdaNexus-OS/releases/download/buildroot-latest

need_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        printf 'Missing required command: %s\n' "$1" >&2
        exit 1
    fi
}

build_image() {
    if [ "$(uname -s)" != Linux ]; then
        printf 'Buildroot must be built on a Linux host.\n' >&2
        exit 1
    fi

    for command in git make gcc g++ bison flex cpio rsync unzip bc file \
                   python3 perl wget patch; do
        need_command "$command"
    done
    MAKE_VERSION=$(make --version | awk 'NR == 1 { print $3 }')
    if ! printf '%s\n' "$MAKE_VERSION" |
        awk -F. '{ exit !($1 > 4 || ($1 == 4 && $2 >= 0)) }'; then
        printf 'Buildroot requires GNU Make 4.0 or newer (found %s).\n' \
            "$MAKE_VERSION" >&2
        exit 1
    fi

    mkdir -p "$(dirname "$BUILDROOT_DIR")" "$DOWNLOAD_DIR"
    if [ ! -d "$BUILDROOT_DIR/.git" ]; then
        if [ -e "$BUILDROOT_DIR" ]; then
            printf 'Buildroot source path exists but is not a Git checkout: %s\n' \
                "$BUILDROOT_DIR" >&2
            exit 1
        fi
        git init "$BUILDROOT_DIR"
        git -C "$BUILDROOT_DIR" remote add origin \
            https://github.com/buildroot/buildroot.git
        git -C "$BUILDROOT_DIR" fetch --depth 1 origin \
            "refs/tags/$BUILDROOT_TAG"
        git -C "$BUILDROOT_DIR" checkout --detach FETCH_HEAD
    fi

    ACTUAL_COMMIT=$(git -C "$BUILDROOT_DIR" rev-parse HEAD)
    if [ "$ACTUAL_COMMIT" != "$BUILDROOT_COMMIT" ]; then
        printf 'Unexpected Buildroot source revision: %s\n' "$ACTUAL_COMMIT" >&2
        printf 'Expected pinned revision: %s\n' "$BUILDROOT_COMMIT" >&2
        exit 1
    fi

    export BR2_DL_DIR="$DOWNLOAD_DIR"
    make -C "$BUILDROOT_DIR" O="$OUTPUT_DIR" \
        BR2_EXTERNAL="$EXTERNAL_DIR" BR2_DEFCONFIG="$DEFCONFIG" defconfig
    JOBS=${LAMBDA_JOBS:-$(getconf _NPROCESSORS_ONLN)}
    make -C "$BUILDROOT_DIR" O="$OUTPUT_DIR" -j"$JOBS"
}

download_image() {
    IMAGE_DIR="$OUTPUT_DIR/images"
    if [ -s "$IMAGE_DIR/Image" ] && [ -s "$IMAGE_DIR/rootfs.ext4" ]; then
        return
    fi

    if [ "$(uname -s)" != Darwin ]; then
        printf '%s\n' \
            'No Buildroot image is available locally.' \
            'Build it with `make buildroot-image` on a Linux host.' >&2
        exit 1
    fi

    need_command curl
    need_command gzip
    need_command shasum
    mkdir -p "$IMAGE_DIR" "$DOWNLOAD_DIR"
    TEMP_DIR=$(mktemp -d "$DOWNLOAD_DIR/.image.XXXXXX")
    cleanup_download() {
        rm -rf "$TEMP_DIR"
    }
    trap cleanup_download EXIT
    trap 'exit 1' HUP INT TERM

    for asset in SHA256SUMS Image.gz rootfs.ext4.gz; do
        printf 'Downloading verified LambdaNexus image asset: %s\n' "$asset"
        if ! curl -fL --retry 3 --progress-bar \
            "$RELEASE_URL/$asset" -o "$TEMP_DIR/$asset"; then
            printf '%s\n' \
                'The verified LambdaNexus image has not been published yet.' \
                'Check the latest Build LambdaNexus image workflow:' \
                'https://github.com/Bindo-Lenzini/LambdaNexus-OS/actions/workflows/buildroot.yml' >&2
            exit 1
        fi
    done
    (cd "$TEMP_DIR" && shasum -a 256 -c SHA256SUMS)

    gzip -dc "$TEMP_DIR/Image.gz" > "$IMAGE_DIR/Image.tmp"
    gzip -dc "$TEMP_DIR/rootfs.ext4.gz" > "$IMAGE_DIR/rootfs.ext4.tmp"
    mv "$IMAGE_DIR/Image.tmp" "$IMAGE_DIR/Image"
    mv "$IMAGE_DIR/rootfs.ext4.tmp" "$IMAGE_DIR/rootfs.ext4"
    cleanup_download
    trap - EXIT HUP INT TERM
}

run_image() {
    IMAGE_DIR="$OUTPUT_DIR/images"
    if [ ! -s "$IMAGE_DIR/Image" ] || [ ! -s "$IMAGE_DIR/rootfs.ext4" ]; then
        if [ "$(uname -s)" = Linux ]; then
            build_image
        else
            download_image
        fi
    fi

    need_command "$QEMU"
    QEMU=$(command -v "$QEMU")
    case "$(uname -s)" in
        Darwin)
            ACCEL=hvf
            CPU=host
            ;;
        Linux)
            if [ -e /dev/kvm ]; then
                ACCEL=kvm
                CPU=host
            else
                ACCEL=tcg
                CPU=cortex-a72
            fi
            ;;
        *)
            ACCEL=tcg
            CPU=cortex-a72
            ;;
    esac

    exec "$QEMU" \
        -name "LambdaNexus OS" \
        -machine "virt,gic-version=3,accel=$ACCEL" \
        -cpu "$CPU" \
        -smp "${LAMBDA_CPUS:-4}" \
        -m "${LAMBDA_MEMORY:-2048}" \
        -kernel "$IMAGE_DIR/Image" \
        -drive "if=none,id=rootfs,format=raw,file=$IMAGE_DIR/rootfs.ext4" \
        -device virtio-blk-device,drive=rootfs \
        -netdev user,id=net0 \
        -device virtio-net-pci,netdev=net0 \
        -append "root=/dev/vda rw rootwait console=ttyAMA0,115200" \
        -display none \
        -monitor none \
        -serial stdio
}

case "${1:-run}" in
    build)
        build_image
        ;;
    run)
        run_image
        ;;
    *)
        printf 'Usage: %s [build|run]\n' "$0" >&2
        exit 2
        ;;
esac
