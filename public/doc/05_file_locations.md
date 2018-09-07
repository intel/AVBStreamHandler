# File Locations {#file_locations}


#################################################################################
@page file_locations File Locations
#################################################################################

In the following the files and their locations involved in the AVB stack execution are listed.

~~~~~~~~~~{.cpp}
// igb-avb driver
/lib/modules/<kernel-version>-abl/kernel/drivers/net/ethernet/intel/igb/igb-avb.ko

// udev rules file
/etc/udev/rules.d/66-avb.rules

// PTP daemon
/usr/bin/daemon_cl
/lib/systemd/system/gptp.service

// stream handler demo app
/usr/bin/avb_streamhandler_demo

// reference configuration library
/usr/lib/pluginias-media_transport-avb_configuration_reference.so

// environment files
/etc/sysconfig/gptp
/etc/sysconfig/avb
/lib/systemd/system/avbstreamhandler_demo.service

// ALSA plugin
/usr/share/alsa/alsa.conf.d/50-smartx.conf

// example wav file
/opt/audio/setup/Grummelbass_48000Hz_60s.wav
~~~~~~~~~~
