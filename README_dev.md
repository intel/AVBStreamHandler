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

### Prerequisites

Ensure that the following libraries are already installed
* pci-devel/pciutils, dbus/dbus-devel, gtest-dev, cap-dev
* sndfile, boost, intel-tbb, doxygen, graphviz

### Build and Install

Note that for development we recommend installing AVB Stream Handler
dependencies on a separate directory - indicated by `AVB_DEPS` environment
variable throughout the instructions. This should keep the development
environment "clean". However, this will prevent capabilities from working,
as Linux ignores them when `LD_LIBRARY_PATH` environment variable is used
to modify where libraries should be searched on, causing the need to run
AVB Stream Handler with root privileges - see below.
On production environment, all dependencies can be installed on default
system directories, such as `/usr/lib` or `/usr/bin`.

```


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

(for alsa lib v.1.1.6 or before)
	$ cp deps/audio/common/public/res/50-smartx.conf /usr/share/alsa/alsa.conf.d/51-smartx.conf
(for alsa lib v1.1.7 or later, conf location is fixed to /etc/alsa/conf.d/)
	$ cp deps/audio/common/public/res/50-smartx.conf /etc/alsa/conf.d/51-smartx.conf

$ cp build/deps/audio/common/libasound_module_* /usr/lib/alsa-lib/
- Modify 51-smartx.conf to include:
	pcm_type.smartx {
		lib "<path to lib>/alsa-lib/libasound_module_pcm_smartx.so"
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

# If the module igb_avb is loaded by default on boot, reinsert the module: #
$ rmmod igb_avb
$ sudo insmod /lib/modules/<kernel-ver>/kernel/drivers/staging/igb_avb/igb_avb.ko tx_size=1024

# Start the DLT daemon (for logging and tracing) #

$ cd $AVB_DEPS/bin
$ ./dlt-daemon -d (to daemonize)

# Following is necessary on development environment. If AVB Stream Handler
# dependencies were installed on system default directories, it is not
# necessary. Note that when using `AVB_DEPS` directory on
# `LD_LIBRARY_PATH`, capabilities won't work, so `daemon_cl` and
# `avb_streamhandler_demo` will need to be run with root powers.

$ export LD_LIBRARY_PATH=$AVB_DEPS
```

* Master/slave specific bits
```
# Start the PTP daemon #

- On Master
$ sudo setcap cap_net_admin,cap_net_raw,cap_sys_nice+ep deps/gptp/linux/build/obj/daemon_cl

# Run with `sudo` if AVB Stream Handler dependencies are installed on `AVB_DEPS`
$ deps/gptp/linux/build/obj/daemon_cl <I210 interface name> -G ias_avb -R 200 -GM &> /tmp/daemon_cl.log &

- On Slave
$ sudo setcap cap_net_admin,cap_net_raw,cap_sys_nice+ep deps/gptp/linux/build/obj/daemon_cl

# Run with `sudo` if AVB Stream Handler dependencies are installed on `AVB_DEPS`
$ deps/gptp/linux/build/obj/daemon_cl <I210 interface name> -G ias_avb -R 200 -S &> /tmp/daemon_cl.log &

# Run the AVBSH demo #

- On Master

# Run with `sudo` if AVB Stream Handler dependencies are installed on `AVB_DEPS`
$ build/avb_streamhandler_demo -c -s
  pluginias-media_transport-avb_configuration_reference.so setup --target GrMrb
  -p MRB_Master_Audio -n <I210 interface name> &> /tmp/avbsh.log &

- On Slave

# Run with `sudo` if AVB Stream Handler dependencies are installed on `AVB_DEPS`
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

### Testing video stream

Load igb_avb driver and run gPTP deamon following instructions above. Then,
run AVBSH demo:

- On Master
$ build/avb_streamhandler_demo -c -s
  pluginias-media_transport-avb_configuration_reference.so setup --target GrMrb
  -p <video-profile-master> -n <I210 interface name> &> /tmp/avbsh.log &

- On Slave
$ build/avb_streamhandler_demo -c -s
  pluginias-media_transport-avb_configuration_reference.so setup --target GrMrb
  -p <video-profile-slave> -n <I210 interface name> &> /tmp/avbsh.log &

Note that <video-profile-master> can be `Video_POC_Master` for H.264 or
`Video_POC_MpegTs_Master` for MPEG-TS, and `Video_POC_Slave` for H.264 or
`Video_POC_MpegTs_Slave` for MPEG-TS on slave.

Then, run the `avb_video_debug_app`:

- On Master (for H.264)
$ sudo LD_LIBRARY_PATH=$AVB_DEPS/lib build/avb_video_debug_app -h -s
or (for MPEG-TS)
$ sudo LD_LIBRARY_PATH=$AVB_DEPS/lib build/avb_video_debug_app -m -s

- On Slave (for H.264)
$ sudo LD_LIBRARY_PATH=$AVB_DEPS/lib build/avb_video_debug_app -h -r
or (for MPEG-TS)
$ sudo LD_LIBRARY_PATH=$AVB_DEPS/lib build/avb_video_debug_app -m -r

Output should contain messages about sending (or receiving) packets with
success.

### Running the Systemd Watchdog

AVB streamhandler watchdog is built as a static library. Currently, 3
AVB streamhandler threads - Transmit sequencer, Alsa Worker and Receive
engine,  register themselves with the watchdog manager. These can be
enabled by passing "-k watchdog.enable=1" to avb_streamhandler_demo.

Since we're relying on Systemd watchdog to reset AVB streamhandler
if any of the three threads stop resetting the watchdog timer,the
AVB streamhandler needs to be run as a Systemd service. A sample
Systemd service file is described below.

* Systemd service file. Place this at /etc/systemd/system/avb.service
```
[Unit]
Description=AVBSH for watchdog testing

[Service]
Type=simple
Environment="LD_LIBRARY_PATH=$AVB_DEPS/lib/"
ExecStart=<path to your AVB SH build dir>/avb_streamhandler_demo -c -v -s
pluginias-media_transport-avb_configuration_reference.so setup --target
GrMrb -p MRB_Slave_Audio -n enp4s0 -k watchdog.enable=1
WatchdogSec=30
Restart=always
```

* Start/stop or check the status of the AVB streamhandler service with:
```
 - sudo systemctl start avb.service
 - sudo systemctl status avb.service
 - sudo systemctl stop avb.service
```

* To view watchdog specific logs:
```
 - sudo journalctl -fu avb.service | grep watchdog
```

## Security issues

Please report any security issues with this code to https://www.intel.com/security

