# AVB Stream Handler

The AVB Stream Handler acts as a link between the AVB-capable Ethernet
interface and audio/video applications local to the device. Audio
applications can receive and transmit audio data through ALSA interfaces.
Video applications can receive and transmit either H.264 encoded video or
MPEG-TS streams. The Stream Handler ensures proper AVB packet generation
following the IEEE1722 standard and observes the subset of IEEE802.1Q
deemed relevant for automotive applications.

## Getting started

Clone the following:
* avb_sh (run git submodule update --init after cloning)
* alsa-lib
* dlt-daemon

### Prerequisistes

Ensure that the following libraries are already installed
* pci-devel/pciutils, dbus/dbus-devel, gtest-dev, cap-dev
* sndfile, boost, intel-tbb, doxygen, graphviz

### Build and Install

```
$ export AVB_DEPS=~/opt/libs
$ mkdir -p $AVB_DEPS


# ALSA-LIB #

$ autoreconf -i
$ ./configure --prefix=$AVB_DEPS --with-pythonlibs="-lpthread -lm -ldl
    -lpython2.7" --with-pythonincludes=-I/usr/include/python2.7
$ make
$ make install


# DLT-DAEMON #

$ mkdir build && cd build
$ cmake .. -DCMAKE_INSTALL_PREFIX=$AVB_DEPS -DCMAKE_INSTALL_LIBDIR=lib
	   -DWITH_DLT_CXX11_EXT=ON
$ make
$ make install


# KERNEL SOURCES #

- install your distro's kernel source package.
- if it isn't provided, then check your version with `uname -r`
- go to https://mirrors.edge.kernel.org/pub/linux/kernel/ and download the
  right .tar.gz file for your version and do the following:
$ sudo mkdir -p /usr/src
$ sudo mv linux-tarball-FOO.tar.gz /usr/src
$ cd /usr/src
$ sudo tar xf linux-tarball-FOO.tar.gz
$ sudo mv linux-FOO linux-`uname -r`
$ cd linux-VERSION
$ zcat /proc/config.gz | sudo tee .config
$ sudo -E make modules_prepare


# AVB StreamHandler #

$ mkdir build && cd build
$ env PKG_CONFIG_PATH=$AVB_DEPS/lib/pkgconfig cmake -DIAS_IS_HOST_BUILD=1 ../
$ make setcap_tool
$ install setcap_tool $AVB_DEPS/bin/
$ sudo setcap cap_setfcap=pe $AVB_DEPS/bin/setcap_tool
$ AVB_SETCAP_TOOL=$AVB_DEPS/bin/setcap_tool make
```

### Send an Audio stream w/ AVBSH demo

* This setup consists of two systems, both equipped with an I210 card that
  are connected over ethernet, where one acts as a Master and the other as
  a slave.
* The following steps need to be replicated on both the systems.
```
# Initial prep #

- Create user groups ias_audio and ias_avb and add yourself
$ sudo groupadd ias_avb
$ sudo groupadd ias_audio
$ sudo usermod -a -G ias_avb <username>
$ sudo usermod -a -G ias_audio <username>
- Restart the session.

$ export LD_LIBRARY_PATH=$AVB_DEPS
$ cp deps/audio/common/public/res/50-smartx.conf $AVB_DEPS/share/alsa/alsa.conf.d
     /51-smartx.conf
$ cp build/deps/audio/common/libasound_module_* $AVB_DEPS/lib/alsa-lib/
- Modify $AVB_DEPS/share/alsa/alsa.conf.d/51-smartx.conf to include:
	pcm_type.smartx {
		lib "<$AVB_DEPS>/lib/alsa-lib/libasound_module_pcm_smartx.so"
	}
  After the pcm.smartx block.
- This is key to ensure that ALSA and AVBSH are able to communicate with each
  other.


# Create shared memory for the smartx plugin #

$ sudo mkdir /run/smartx
$ sudo chown <username>:ias_audio /run/smartx
$ sudo chmod 2770 /run/smartx
- This will be lost on a reboot.


# Insert igb_avb kernel module #

$ sudo modprobe -r igb (If upstream igb driver exists)
$ sudo insmod deps/igb_avb/kmod/igb_avb.ko tx_size=1024
$ sudo chmod 660 /dev/igb_avb
$ sudo chgrp ias_avb /dev/igb_avb
$ sudo chmod 660 /dev/ptp*
$ sudo chgrp ias_avb /dev/ptp*


# Start the DLT daemon (for logging and tracing) #

$ cd $AVB_DEPS/bin
$ ./dlt-daemon -d (to daemonize)
```

* Master/slave specific bits
```
# Start the PTP daemon #

- On Master
$ sudo setcap cap_net_admin,cap_net_raw,cap_sys_nice+ep deps/gptp/linux/build/obj/daemon_cl
$ deps/gptp/linux/build/obj/daemon_cl <I210 interface name> -G ias_avb -R 200 -GM &> /tmp/daemon_cl.log &

- On Slave
$ sudo setcap cap_net_admin,cap_net_raw,cap_sys_nice+ep deps/gptp/linux/build/obj/daemon_cl
$ deps/gptp/linux/build/obj/daemon_cl <I210 interface name> -G ias_avb -R 200 -S &> /tmp/daemon_cl.log &


# Run the AVBSH demo #

- On Master
$ build/avb_streamhandler_demo -c -s
  pluginias-media_transport-avb_configuration_reference.so setup --target GrMrb
  -p MRB_Master_Audio -n <I210 interface name> &> /tmp/avbsh.log &

- On Slave
$ build/avb_streamhandler_demo -c -s
  pluginias-media_transport-avb_configuration_reference.so setup --target GrMrb
  -p MRB_Slave_Audio -n <I210 interface name> &> /tmp/avbsh.log &


# Finally, transfer an audio stream w/ aplay #

- On Master
$ sudo LD_LIBRARY_PATH=$AVB_DEPS/lib aplay -D avb:stereo_0 <wav file>

- On Slave
$ sudo LD_LIBRARY_PATH=$AVB_DEPS/lib aplay -C -f dat -D avb:stereo_0 record.wav
  (quit with ^C once the master is done playing)
```
