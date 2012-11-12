#!/bin/bash

DISTRO=""
SRC="$HOME/src/blender-deps"
CWD=$PWD

THREADS=`cat /proc/cpuinfo | grep cores | uniq | sed -e "s/.*: *\(.*\)/\\1/"`

PYTHON_VERSION="3.3.0"
BOOST_VERSION="1_51_0"
OIIO_VERSION="1.1.0"
OCIO_VERSION="1.0.7"
FFMPEG_VERSION="1.0"

HASXVID=false
HASVPX=false
HASMP3LAME=false
HASX264=false

YUM="yum"

ERROR() {
  echo "${@}"
}

INFO() {
  echo "${@}"
}

detect_distro() {
  if [ -f /etc/debian_version ]; then
    DISTRO="DEB"
  elif [ -f /etc/redhat-release ]; then
    if which yum > /dev/null 2>&1; then
      DISTRO="RPM"
      YUM="yum"
    elif which zypper > /dev/null 2>&1; then
      DISTRO="RPM"
      YUM="zypper"
    fi
  fi
}

prepare_opt() {
  INFO "Ensuring /opt/lib exists and vritabele by us"
  sudo mkdir -p /opt/lib
  sudo chown $USER /opt/lib
  sudo chmod 775 /opt/lib
}

compile_Python() {
  if [ ! -d /opt/lib/python-$PYTHON_VERSION ]; then
    INFO "Building Python-$PYTHON_VERSION"

    prepare_opt

    if [ ! -d $SRC/Python-$PYTHON_VERSION ]; then
      mkdir -p $SRC
      wget -c http://python.org/ftp/python/$PYTHON_VERSION/Python-$PYTHON_VERSION.tar.bz2 -P $SRC

      INFO "Unpacking Python-$PYTHON_VERSION"
      tar -C $SRC -xf $SRC/Python-$PYTHON_VERSION.tar.bz2
    fi

    cd $SRC/Python-$PYTHON_VERSION

    ./configure --prefix=/opt/lib/python-$PYTHON_VERSION --enable-ipv6 \
        --enable-loadable-sqlite-extensions --with-dbmliborder=bdb \
        --with-computed-gotos --with-pymalloc

    make -j$THREADS
    make install
    make clean

    rm -f /opt/lib/python-3.3
    ln -s python-$PYTHON_VERSION /opt/lib/python-3.3

    cd $CWD
  fi
}

compile_Boost() {
  INFO "Building boost"

  version_dots=`echo "$BOOST_VERSION" | sed -r 's/_/./g'`

  if [ ! -d /opt/lib/boost-$version_dots ]; then
    INFO "Building Boost-$version_dots"

    prepare_opt

    if [ ! -d $SRC/boost_$BOOST_VERSION ]; then
      INFO "Downloading Boost-$version_dots"
      mkdir -p $SRC
      wget -c http://sourceforge.net/projects/boost/files/boost/$version_dots/boost_$BOOST_VERSION.tar.bz2/download \
        -O $SRC/boost_$BOOST_VERSION.tar.bz2
      tar -C $SRC -xf $SRC/boost_$BOOST_VERSION.tar.bz2
    fi

    cd $SRC/boost_$BOOST_VERSION
    ./bootstrap.sh
    ./b2 install --prefix=/opt/lib/boost-$version_dots
    ./b2 --clean

    rm -f /opt/lib/boost
    ln -s boost-$BOOST_VERSION /opt/lib/boost

    cd $CWD
  fi
}

compile_OCIO() {
  if [ ! -d /opt/lib/ocio-$OCIO_VERSION ]; then
    INFO "Building OpenColorIO-$OCIO_VERSION"

    prepare_opt

    if [ ! -d $SRC/OpenColorIO-$OCIO_VERSION ]; then
        INFO "Downloading OpenColorIO-$OCIO_VERSION"
        mkdir -p $SRC
        wget -c http://github.com/imageworks/OpenColorIO/tarball/v$OCIO_VERSION \
          -O $SRC/OpenColorIO-$OCIO_VERSION.tar.gz

        INFO "Unpacking OpenColorIO-$OCIO_VERSION"
        tar -C "$SRC" -xf $SRC/OpenColorIO-$OCIO_VERSION.tar.gz
        mv $SRC/imageworks-OpenColorIO* $SRC/OpenColorIO-$OCIO_VERSION
    fi

    cd $SRC/OpenColorIO-$OCIO_VERSION
    mkdir build
    cd build

    if file /bin/cp | grep -q '32-bit'; then
      cflags="-fPIC -m32 -march=i686"
    else
      cflags="-fPIC"
    fi

    cmake -D CMAKE_BUILD_TYPE=Release \
        -D CMAKE_PREFIX_PATH=/opt/lib/ocio-$OCIO_VERSION \
        -D CMAKE_INSTALL_PREFIX=/opt/lib/ocio-$OCIO_VERSION \
        -D CMAKE_CXX_FLAGS="$cflags" \
        -D CMAKE_EXE_LINKER_FLAGS="-lgcc_s -lgcc" \
        ..

    make -j$THREADS
    make install

    # Force linking against sttaic libs
    rm -f /opt/lib/ocio-$OCIO_VERSION/lib/*.so*

    # Additional depencencies
    cp ext/dist/lib/libtinyxml.a /opt/lib/ocio-$OCIO_VERSION/lib
    cp ext/dist/lib/libyaml-cpp.a /opt/lib/ocio-$OCIO_VERSION/lib

    make clean

    rm -f /opt/lib/ocio
    ln -s ocio-$OCIO_VERSION /opt/lib/ocio

    cd $CWD
  fi
}

compile_OIIO() {
  if [ ! -d /opt/lib/oiio-$OIIO_VERSION ]; then
    INFO "Building OpenImageIO-$OIIO_VERSION"

    prepare_opt

    if [ ! -d $SRC/OpenImageIO-$OIIO_VERSION ]; then
      wget -c https://github.com/OpenImageIO/oiio/tarball/Release-$OIIO_VERSION \
          -O "$SRC/OpenImageIO-$OIIO_VERSION.tar.gz"

      INFO "Unpacking OpenImageIO-$OIIO_VERSION"
      tar -C $SRC -xf $SRC/OpenImageIO-$OIIO_VERSION.tar.gz
      mv $SRC/OpenImageIO-oiio* $SRC/OpenImageIO-$OIIO_VERSION
    fi

    cd $SRC/OpenImageIO-$OIIO_VERSION
    mkdir build
    cd build

    if [ -d /opt/lib/boost ]; then
      boost_root="/opt/lib/boost"
    else
      boost_root="/usr"
    fi

    if file /bin/cp | grep -q '32-bit'; then
      cflags="-fPIC -m32 -march=i686"
    else
      cflags="-fPIC"
    fi

    cmake -D CMAKE_BUILD_TYPE=Release \
        -D CMAKE_PREFIX_PATH=/opt/lib/oiio-$OIIO_VERSION \
        -D CMAKE_INSTALL_PREFIX=/opt/lib/oiio-$OIIO_VERSION \
        -D BUILDSTATIC=ON \
        -D CMAKE_CXX_FLAGS="$cflags" \
        -D CMAKE_EXE_LINKER_FLAGS="-lgcc_s -lgcc" \
        -D BOOST_ROOT="$boost_root" \
        ../src

    make -j$THREADS
    make install
    make clean

    rm -f /opt/lib/oiio
    ln -s oiio-$OIIO_VERSION /opt/lib/oiio

    cd $CWD
  fi
}

compile_FFmpeg() {
  if [ ! -d /opt/lib/ffmpeg-$FFMPEG_VERSION ]; then
    INFO "Building FFmpeg-$FFMPEG_VERSION"

    prepare_opt

    if [ ! -d $SRC/ffmpeg-$FFMPEG_VERSION ]; then
      INFO "Downloading FFmpeg-$FFMPEG_VERSION"
      wget -c http://ffmpeg.org/releases/ffmpeg-$FFMPEG_VERSION.tar.bz2 -P $SRC

      INFO "Unpacking FFmpeg-$FFMPEG_VERSION"
      tar -C $SRC -xf $SRC/ffmpeg-$FFMPEG_VERSION.tar.bz2
    fi

    cd $SRC/ffmpeg-$FFMPEG_VERSION

    extra=""

    if $HASXVID; then
      extra="$extra --enable-libxvid"
    fi

    if $HASVPX; then
      extra="$extra --enable-libvpx"
    fi

    if $HASMP3LAME; then
      extra="$extra --enable-libmp3lame"
    fi

    if $HASX264; then
      extra="$extra --enable-libx264"
    fi

    ./configure --cc="gcc -Wl,--as-needed" --extra-ldflags="-pthread -static-libgcc" \
        --prefix=/opt/lib/ffmpeg-$FFMPEG_VERSION --enable-static --enable-avfilter --disable-vdpau \
        --disable-bzlib --disable-libgsm --enable-libschroedinger --disable-libspeex --enable-libtheora \
        --enable-libvorbis --enable-pthreads --enable-zlib --enable-stripping --enable-runtime-cpudetect \
        --disable-vaapi --enable-libopenjpeg --disable-libfaac --disable-nonfree --enable-gpl \
        --disable-postproc --disable-x11grab  --disable-librtmp  --disable-libopencore-amrnb \
        --disable-libopencore-amrwb --disable-libdc1394 --disable-version3  --disable-outdev=sdl \
        --disable-outdev=alsa --disable-indev=sdl --disable-indev=alsa --disable-indev=jack \
        --disable-indev=lavfi $extra

    make -j$THERADS
    make install
    make clean

    rm -f /opt/lib/ffmpeg
    ln -s ffmpeg-$FFMPEG_VERSION /opt/lib/ffmpeg

    cd $CWD
  fi
}

deb_version() {
    dpkg-query -W -f '${Version}' $1 | sed -r 's/^([0-9]\.[0-9]+).*/\1/'
}

check_package_DEB() {
  r=`apt-cache policy $1 | grep -c 'Candidate:'`

  if [ $r -ge 1 ]; then
    return 0
  else
    return 1
  fi
}

install_DEB() {
  INFO "Installing dependencies for DEB-based distributive"

  sudo apt-get update
  sudo apt-get -y upgrade

  sudo apt-get install -y cmake scons gcc g++ libjpeg-dev libpng-dev libtiff-dev \
    libfreetype6-dev libx11-dev libxi-dev wget libsqlite3-dev libbz2-dev libncurses5-dev \
    libssl-dev liblzma-dev libreadline-dev libopenjpeg-dev libopenexr-dev libopenal-dev \
    libglew-dev yasm libschroedinger-dev libtheora-dev libvorbis-dev libsdl1.2-dev \
    libfftw3-dev libjack-dev python-dev patch

  check_package_DEB libxvidcore4-dev
  if [ $? -eq 0 ]; then
    sudo apt-get install -y libxvidcore4-dev
    HASXVID=true
  fi

  check_package_DEB libmp3lame-dev
  if [ $? -eq 0 ]; then
    sudo apt-get install -y libmp3lame-dev
    HASMP3LAME=true
  fi

  check_package_DEB libx264-dev
  if [ $? -eq 0 ]; then
    sudo apt-get install -y libx264-dev
    HASX264=true
  fi

  check_package_DEB libvpx-dev
  if [ $? -eq 0 ]; then
    sudo apt-get install -y libvpx-dev
    vpx_version=`deb_version libvpx-dev`
    if [ ! -z "$vpx_version" ]; then
      if  dpkg --compare-versions $vpx_version gt 0.9.7; then
        HASVPX=true
      fi
    fi
  fi

  check_package_DEB libspnav-dev
  if [ $? -eq 0 ]; then
    sudo apt-get install -y libspnav-dev
  fi

  check_package_DEB python3.3-dev
  if [ $? -eq 0 ]; then
    sudo apt-get install -y python3.3-dev
  else
    compile_Python
  fi

  check_package_DEB libboost-dev
  if [ $? -eq 0 ]; then
    sudo apt-get install -y libboost-dev

    boost_version=`deb_version libboost-dev`

    check_package_DEB libboost-locale$boost_version-dev
    if [ $? -eq 0 ]; then
      sudo apt-get install -y libboost-locale$boost_version-dev libboost-filesystem$boost_version-dev \
        libboost-regex$boost_version-dev libboost-system$boost_version-dev libboost-thread$boost_version-dev
    else
      compile_Boost
    fi
  else
    compile_Boost
  fi

  check_package_DEB libopencolorio-dev
  if [ $? -eq 0 ]; then
    sudo apt-get install -y libopencolorio-dev
  else
    compile_OCIO
  fi

  check_package_DEB libopenimageio-dev
  if [ $? -eq 0 ]; then
    sudo apt-get install -y libopenimageio-dev
  else
    compile_OIIO
  fi

  check_package_DEB ffmpeg
  if [ $? -eq 0 ]; then
    sudo apt-get install -y ffmpeg
    ffmpeg_version=`deb_version ffmpeg`
    if [ ! -z "$ffmpeg_version" ]; then
      if  dpkg --compare-versions $ffmpeg_version gt 0.7.2; then
        sudo apt-get install -y libavfilter-dev libavcodec-dev libavdevice-dev libavformat-dev libavutil-dev libswscale-dev
      else
        compile_FFmpeg
      fi
    fi
  fi
}

check_package_RPM() {
  r=`$YUM info $1 | grep -c 'Summary'`

  if [ $r -ge 1 ]; then
    return 0
  else
    return 1
  fi
}

check_package_version_RPM() {
  v=`yum info $1 | grep Version | tail -n 1 | sed -r 's/.*:\s+(([0-9]+\.?)+)/\1/'`

  # for now major and minor versions only (as if x.y, not x.y.z)
  r=`echo $v | grep -c $2`

  if [ $r -ge 1 ]; then
    return 0
  else
    return 1
  fi
}

install_RPM() {
  INFO "Installing dependencies for RPM-based distributive"

  sudo $YUM -y update

  sudo $YUM -y install gcc gcc-c++ cmake scons libpng-devel libtiff-devel \
    freetype-devel libX11-devel libXi-devel wget libsqlite3x-devel ncurses-devel \
    readline-devel openjpeg-devel openexr-devel openal-soft-devel \
    glew-devel yasm schroedinger-devel libtheora-devel libvorbis-devel SDL-devel \
    fftw-devel lame-libs jack-audio-connection-kit-devel x264-devel libspnav-devel \
    libjpeg-devel patch python-devel

  check_package_version_RPM python-devel 3.3
  if [ $? -eq 0 ]; then
    sudo $YUM install -y python-devel
  else
    compile_Python
  fi

  check_package_RPM boost-devel
  if [ $? -eq 0 ]; then
    sudo $YUM install -y boost-devel
  else
    compile_Boost
  fi

  check_package_RPM OpenColorIO-devel
  if [ $? -eq 0 ]; then
    sudo $YUM install -y OpenColorIO-devel
  else
    compile_OCIO
  fi

  check_package_RPM OpenImageIO-devel
  if [ $? -eq 0 ]; then
    sudo $YUM install -y OpenImageIO-devel
  else
    compile_OIIO
  fi

  # Always for now, not sure which packages should be installed
  compile_FFmpeg
}

print_info() {
  INFO ""
  INFO "If you're using CMake add this to your configuration flags:"

  if [ -d /opt/lib/boost ]; then
    INFO "  -D BOOST_ROOT=/opt/lib/boost"
    INFO "  -D Boost_NO_SYSTEM_PATHS=ON"
  fi

  if [ -d /opt/lib/ffmpeg ]; then
    INFO "  -D FFMPEG=/opt/lib/ffmpeg"
  fi

  INFO ""
  INFO "If you're using SCons add this to your user-config:"

  if [ -d /opt/lib/python3.3 ]; then
    INFO "BF_PYTHON='/opt/lib/puthon-3.3'"
    INFO "BF_PYTHON_ABI_FLAGS='m'"
  fi

  if [ -d /opt/lib/ocio ]; then
    INFO "BF_OCIO='/opt/lib/ocio'"
  fi

  if [ -d /opt/lib/oiio ]; then
    INFO "BF_OCIO='/opt/lib/oiio'"
  fi

  if [ -d /opt/lib/boost ]; then
    INFO "BF_BOOST='/opt/lib/boost'"
  fi

  if [ -d /opt/lib/ffmpeg ]; then
    INFO "BF_FFMPEG='/opt/lib/ffmpeg'"
  fi
}

# Detect distributive type used on this machine
detect_distro

if [ -z "$DISTRO" ]; then
  ERROR "Failed to detect distribytive type"
  exit 1
elif [ "$DISTRO" = "DEB" ]; then
  install_DEB
elif [ "$DISTRO" = "RPM" ]; then
  install_RPM
fi

print_info
