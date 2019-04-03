# AVB Stream Handler for Clear Linux OS in 64bit


# ALSA-LIB #
$ ./configure --lib64=/usr/lib64 --with-pythonlibs="-lpthread -lm -ldl -lpython2.7" --with-pythonincludes=-I/usr/include/python2.7
$ make
$ make install

# DLT-DAEMON #
$ cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib64 -DWITH_DLT_CXX11_EXT=ON
$ cmake .. -DWITH_DLT_CXX11_EXT=ON
$ make
$ make install

# KERNEL SOURCES #

$ swupd bundle-list
$ swupd bundle-add linux-dev

# AVB StreamHandler #
$ mkdir build && cd build
$ cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib64  -DIAS_IS_HOST_BUILD=1 ../
$ make
$ sudo make install



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

# alsa configuration, and libasound module files setup ( This instruction is just for your information as 'make install'command in AVB StreamHandler section above already takes care of it)
	(for alsa lib v.1.1.6 or before)
	$ cp deps/audio/common/public/res/50-smartx.conf /usr/share/alsa/alsa.conf.d/51-smartx.conf
	(for alsa lib v1.1.7 or later, conf location is fixed to /etc/alsa/conf.d/)
	$ cp deps/audio/common/public/res/50-smartx.conf /etc/alsa/conf.d/51-smartx.conf
	
	$ cp build/deps/audio/common/libasound_module_* /usr/lib64/alsa-lib/

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

$ dlt-daemon -d (to daemonize)


# Start the PTP daemon #

- On Master
$ daemon_cl <I210 interface name> -G ias_avb -R 200

- On Slave
$ daemon_cl <I210 interface name> -G ias_avb -R 200


# Run the AVBSH demo #

- On Master
for default lib path
$ avb_streamhandler_demo -c -s pluginias-media_transport-avb_configuration_reference.so setup --target GrMrb -p MRB_Master_Audio -n <I210 interface name>

- On Slave
for default lib path
$ avb_streamhandler_demo -c -s pluginias-media_transport-avb_configuration_reference.so setup --target GrMrb -p MRB_Slave_Audio -n <I210 interface name>

# Finally, transfer an audio stream w/ aplay #

- On Master
$ aplay -D avb:stereo_0 <wav file>
	
- On Slave
$ aplay -C -f dat -D avb:stereo_0 record.wav
	  (quit with ^C once the master is done playing)

### Testing video stream

Load igb_avb driver and run gPTP deamon following instructions above. Then,
run AVBSH demo:

- On Master
$ avb_streamhandler_demo -c -s
  pluginias-media_transport-avb_configuration_reference.so setup --target GrMrb
  -p <video-profile-master> -n <I210 interface name> &> /tmp/avbsh.log &

- On Slave
$ avb_streamhandler_demo -c -s
  pluginias-media_transport-avb_configuration_reference.so setup --target GrMrb
  -p <video-profile-slave> -n <I210 interface name> &> /tmp/avbsh.log &

Note that <video-profile-master> can be `Video_POC_Master` for H.264 or
`Video_POC_MpegTs_Master` for MPEG-TS, and `Video_POC_Slave` for H.264 or
`Video_POC_MpegTs_Slave` for MPEG-TS on slave.

Then, run the `avb_video_debug_app`:

- On Master (for H.264)
	$ sudo build/avb_video_debug_app -h -s
	or (for MPEG-TS)
	$ sudo build/avb_video_debug_app -m -s


- On Slave (for H.264)
	$ sudo build/avb_video_debug_app -h -r
	or (for MPEG-TS)
	$ sudo build/avb_video_debug_app -m -r

Output should contain messages about sending (or receiving) packets with
success.

## Security issues

Please report any security issues with this code to https://www.intel.com/security
