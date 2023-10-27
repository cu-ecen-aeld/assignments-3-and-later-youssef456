#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.1.10
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
#="$(readlink -f /home/youssef/gcc-arm-10.2-2020.11-x86_64-aarch64-none-linux-gnu/bin/)"
CROSS_COMPILER_PATH=/home/youssef/gcc-arm-10.2-2020.11-x86_64-aarch64-none-linux-gnu/bin/
CROSS_COMPILE="${CROSS_COMPILER_PATH}aarch64-none-linux-gnu-"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    #Clone only if the repository does not exist.
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}
    
    echo "Current working directory: $(pwd)"
    if [ -d CROSS_COMPILER_PATH ]; then
        echo "The directory '$CROSS_COMPILER_PATH' exists."
    else
        echo "The directory '$CROSS_COMPILER_PATH' does not exist."
fi
    # TODO: Add your kernel build steps here
    ##sudo make ARCH=arm64 CROSS_COMPILE="${CROSS_COMPILE}" defconfig
    ##sudo make ARCH=arm64 CROSS_COMPILE="${CROSS_COMPILE}" -j4
    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- mrproper
    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- defconfig
    make -j 4 ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- all
    #build kernel modules 
    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- modules
    #build device tree
    make ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- dtbs
fi

cd ${OUTDIR}/linux-stable
    
echo "Adding the Image in outdir"
# Install the kernel image to "OUTDIR."
cp arch/arm64/boot/Image ${OUTDIR}

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    sudo rm  -rf ${OUTDIR}/rootfs
fi

# TODO: Create necessary base directories
mkdir -p ${OUTDIR}/rootfs/{bin,dev,etc,home,lib,lib64,proc,sbin,sys,usr,var}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]
then
    git clone git://busybox.net/busybox.git ${OUTDIR}/busybox
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    # TODO:  Configure busybox
    make ARCH=arm64 CROSS_COMPILE="${CROSS_COMPILE}" defconfig
else
    cd busybox
fi

# TODO: Make and install busybox
cd ${OUTDIR}/busybox
#make ARCH=arm64 CROSS_COMPILE="${CROSS_COMPILE}" -j4
#make ARCH=arm64 CROSS_COMPILE="${CROSS_COMPILE}" install
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install

cd ${OUTDIR}/rootfs/

echo "Library dependencies"
${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter"
${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library"

# TODO: Add library dependencies to rootfs
export SYSROOT=$(${CROSS_COMPILER_PATH}aarch64-none-linux-gnu-gcc -print-sysroot)
sudo cp -L ${SYSROOT}/lib/ld-linux-aarch64.so.1 ${OUTDIR}/rootfs/lib
sudo cp -L ${SYSROOT}/lib64/libm.so.6 ${OUTDIR}/rootfs/lib64 
sudo cp -L ${SYSROOT}/lib64/libresolv.so.2 ${OUTDIR}/rootfs/lib64
sudo cp -L ${SYSROOT}/lib64/libc.so.6 ${OUTDIR}/rootfs/lib64

# TODO: Make device nodes
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/null c 1 3
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/console c 5 1

# TODO: Clean and build the writer utility
cd ${FINDER_APP_DIR}
make clean
make ARCH=arm64 CROSS_COMPILE=${CROSS_COMPILE}

# TODO: Copy the finder related scripts and executables to the /home directory
# on the target rootfs
sudo cp -L -r ${SCRIPT_DIR}/* "${OUTDIR}/rootfs/home"

# TODO: Chown the root directory
sudo chown -R root:root ${OUTDIR}/rootfs

# TODO: Create initramfs.cpio.gz
cd ${OUTDIR}/rootfs
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio   
gzip -c -9 "${OUTDIR}/initramfs.cpio" > "${OUTDIR}/initramfs.cpio.gz"
