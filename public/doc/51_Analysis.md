# Analysis

@section dlt Diagnostic Log and Trace

The Stream Handler supports the DLT mechanism, log messages can be observed using the DLT viewer.
In case you do not want to use the DLT log on a PC, the log messages can be printed to the console by specifying the -c option on the command line (before the word 'setup').
The DLT log level of all of the Stream Handler's DLT contexts can be increased by specifying -v on the command line.

There are 6 different DLT log levels:
    0: off
    1: fatal
    2: error
    3: warning
    4: info
    5: debug
    6: verbose

The default log level of the Streamhandler is 'warning' (3). 
The log level for all contexts can be increased from the default level 'warning' by adding the desired numbers of v in the command line (somewhere before the word 'setup'). For 'info' (4) use -v, for 'debug' (5) use -vv and for 'verbose' (6) use -vvv. The same effect is achieved by setting the level in the DLT viewer. It is not possible to decrease the log level below the default level 'warning' by using -v option. 

@note Be careful not to flood the DLT with thousands of messages per second, because they will overflow when the transport reaches capacity limit. Also, generating huge amount of DLT messages will have an impact on real-time behavior! Filtering out messages in DLT viewer does NOT prevent that. Setting of 'debug' and 'verbose' through -v option will not be available in production mode.

@note Don't get confused with verbosity level which is reported in the journal. Verbosity level corresponds to the number of 'v' specified in the command line. Verbosity level '0' means no 'v' (which corresponds to default DLT log level 'warning'), verbosity level '1' means one 'v' (DLT log level 'info'),  verbosity level '2' means two 'v' (DLT log level 'debug') and verbosity level '3' means three 'v' (DLT log level 'verbose'), 


In order to root cause any audio drop-outs and general misbehavior of the AVB stack, all its DLT messages on error or warning level are activated by default.

The most important messages of type warning are:

* reset due to launch time lag
* TX worker thread slept too long!
* <numEvents> more oversleep events occurred since the last message
* Alsa Engine worker thread slept too long
* writeToAvbPacket] clock reference out of expected interval, resetting."
    the keyword reset is always an indicator that there is an audio drop out.
* deviation out of bounds
    That indicates that the ALSA thread can't lock to the clock information based on the Clock Reference Stream (CRF).
    Root cause could be a missing CRF or missing gPTP information.


###
@subsection common_dlts Common DLT messages

    TX worker slept to long

The worker thread was not activated by the linux scheduler in time. There is same safety margin calculated by `window size(5ms) - window pitch(3ms) = 2ms`.
Only if the safety margin has completely run out, underruns may occur.

If those errors happen often after each other, they will be collected and the count given:

    X more oversleep events occurred since the last message

If the package time is in the past or max transit time can not be guaranteed, the stream gets reset and following error DLT messages appear (numbers will differ):

    stream X reset due to launch time lag: -5452635 now =  22906070290 launch =  22900617655
    Reset due to launchtime lag

###
@subsection alsa_oversleep Alsa Engine worker thread slept too long

    AlsaAlsa Engine worker thread slept too long
    slept for: 8.20166 ms, limit was 4 ms
    AvbAlsaWrk0 overslept more than 4 ms, reinitializing

The worker thread was not activated by the linux scheduler in time. The thread is expected to run at period time interval, which is 4 ms in the above case. If activation time was more than a period time late, it will restart the thread which may cause underruns (playback) or overruns (capture). You may change maximum allowable over sleep time with '-k alsa.clock.threshold.reset=<ns>' key. For example if the key value was 8000000 ns (=8 ms), the thread would recover the delayed periods up to 8 ms when it wakes up. But it may momentarily increase CPU load in order to process pending periods in a rush. The key value should not exceed the max buffer time of alsa stream. That means for a stream with 48kHz freq, 192 period size and 3 number of periods, the maximum key value should be 12000000 ns (=12 ms).

###
@subsection localaudiobuffer Local Audio Buffer (_LAB)

The buffer is set to 50% Fill Level at the beginning.
<!-- will change with time-aware buffers in EC23 release. -->
A stream in class C reads 64 Samples from the buffer `(48000/750 = 64)`
Example:

    8402 2016/08/02 11:00:36.799188 4986.1897 50 ECU1 INAS __LAB log debug verbose 12 [::read] mReadCnt= 1056000 mReadIndex= 1984 mWriteIndex= 2112 distance= 192 state= 1 numread= 64
    8403 2016/08/02 11:00:36.799225 4986.1898 51 ECU1 INAS _LAB log debug verbose 12 [::read] mReadCnt= 1056000 mReadIndex= 1984 mWriteIndex= 2112 distance= 192 state= 1 numread= 64
    8404 2016/08/02 11:00:36.799261 4986.1899 52 ECU1 INAS _LAB log debug verbose 12 [::read] mReadCnt= 1056000 mReadIndex= 1984 mWriteIndex= 2112 distance= 192 state= 1 numread= 64
    8405 2016/08/02 11:00:36.799284 4986.1899 53 ECU1 INAS _LAB log debug verbose 12 [::read] mReadCnt= 1056000 mReadIndex= 1984 mWriteIndex= 2112 distance= 192 state= 1 numread= 64

The value __distance__ shows the fill level and should run around 50%.

###
@subsection deviation Deviation out of bounds

    611 2016/08/02 16:18:52.531051 336.1848 185 ECU1 INAS _AEN log warn verbose 4 [wt 1 ::run] deviation out of bounds: -5.20038e+10

In slave mode, PTP or CRF is not behaving. The deviation of clock values is bigger than the system can manage.


#################################################################################
@section dlt_log_messages DLT Logging Message Description

The application ID for the StreamHandler is *INAS*. In the following tables you will find a description of all important DLT logs
and a more detailed description if the message itself is not sufficient for debugging purposes.

################################################
@subsection log_error DLT Logging Messages for Log Level DLT_LOG_ERROR

@htmlinclude dlt_error.html


################################################
@subsection log_warn DLT Logging Messages for Log Level DLT_LOG_WARN

@htmlinclude dlt_warning.html


################################################
@subsection log_info DLT Logging Messages for Log Level DLT_LOG_INFO

@htmlinclude dlt_info.html


################################################
@subsection log_verbose DLT Logging Messages for Log Level DLT_LOG_VERBOSE

@htmlinclude dlt_verbose.html


################################################
@subsection log_hc DLT Logging Messages not included in above tables

<table class="doxtable">
<tr><th>Context ID <th>DLT Message <th>Description
<tr><td><td>IasAvbStreamHandlerEnvironment::notifySchedulingIssue(1067): TX worker thread slept too long! <td>TX worker thread slept too long.
<tr><td>_AEN<td>IasAvbStreamHandlerEnvironment::notifySchedulingIssue(1067): Alsa Engine worker thread slept too long. <td>Alsa Engine worker thread slept too long.
<tr><td><td>Create Streamhandler *** Version &lt;VERSION&gt; <td>StreamHandler starting point.
<tr><td>_SHM<td>force audio presentation: now / pt / fillLevel / time-aware-mode = &lt;NOW&gt; / &lt;PT&gt; / &lt;FILLLEVEL&gt; / &lt;DESCMODE&gt; <td>Force audio presentation.
</table>


######
@section mirroring Recording of AVB Packages

In order to verify that AVB packets are being sent to the network by the Stream Handler, some additional hardware is required.

It is not possible to monitor the in- and outgoing traffic with tools like Wireshark/tcpdump on the target machine itself,
since the AVB packets bypass all Linux layers and are copied between Stream Handler and Springville NIC directly.

@note Incoming streams at the target can only be captured (with a tool like tcpdump) when Stream Handler is not running. In this case
the flex filters in the I210 are not touched and the traffic is visible to the capture tool.

In order to monitor the live traffic an AVB-compliant switch with port mirroring enabled is needed. By attaching a PC or measurement device to the mirror port of the switch, all packets sent and received by the AVB endpoints can be observed.
Using a command line application like ptrace or the GUI application Wireshark it is possible to analyze the Wireshark/tcpdump traces.

By extracting the audio information out of an AVB packet, saving it in a .wav file and playing that back in an audio wave editor can 
be very helpful. If there are audio drop-outs in those files, then the audio stream is corrupt inside the AVB stack of the sending device. That could be caused by scheduling issues or stream resets.

Is the recorded audio without audio-drop outs (but there are some audible), then the issue might depend on wrong timing conditions.
Wrong time-stamps inside the CRF or the Audio AVB packets or the Precision Time Protocol PTP Daemon does't work correctly.


######
@section clientapp AVB Stream Handler Client Application

For debugging purposes the Stream Handler API can be controlled by a command line application. You can see its parameters with the usual --help.
    usr/bin/avb_streamhandler_client_app --help

It offers following commands:

    CreateTransmitAvbAudioStream   - Creates an AVB transmit stream.
    CreateReceiveAudioStream       - Creates an AVB receive stream.
    DestroyStream                  - Destroys a previously created AVB stream.
    SetStreamActive                - Sets an AVB transmit stream to active or inactive.
    CreateAlsaStream               - Creates a local audio stream that connects to a virtual ALSA device.
    DestroyLocalStream             - Destroys a ALSA stream.
    ConnectStreams                 - Connects an AVB stream and a local audio stream.
    DisconnectStreams              - Disconnects an already connected AVB stream from the local audio stream.
    SetChannelLayout               - Sets the layout of the audio data within the stream.
    Monitor                        - Waits for events to receive until aborted by Ctrl-C.
    CreateTransmitAvbVideoStream   - Creates an AVB video transmit stream.
    CreateReceiveVideoStream       - Creates an AVB video receive stream.
    GetAvbStreamInfo               - Retrieves information about all AVB streams currently created.
    CreateLocalVideoStream         - Creates a local video stream that connects to applications.
    CreateTestToneStream           - Creates a local stream that produces test tones on its audio channels.
    SetTestToneParams              - Changes parameters of test tone generators.

See the help of each command for the further information:

    avb_streamhandler_client_app <command> --help

Following options are possible:

    -h, --help        help
    -q, --srclass     stream reservation class (H = high, L = low) (default H)
    -c, --channels    number of channels (default 2)
    -r, --rate        sample frequency (default 48000)
    -f, --format      format of the audio/video (default audio:SAF16=1/video:RTP=1)
    -C, --clock       clockId Id of the clock domain (default cIasAvbPtpClockDomainId=0x00)
    -M, --mode        assignMode controls the definition of streamId and destination MAC (default static=0)
    -n, --network_id  network Audio Stream ID (default none)
    -l, --local_id    local Audio Stream ID (default none)
    -m, --dmac        MAC address (default none)
    -a, --active      activate Network Stream immediately (default 0)
    -d, --direction   whether a transmit (0) or receive (1) stream (default none)
    -D, --device      name of ALSA device (default none)
    -L, --ch_layout   application specific value indicating layout of audio data within the channel (default 0)
    -s, --has_sidech  use last audio channel for channel info (default 0)
    -p, --period_size ALSA period size (default 256)
    -x, --ch_index    index of the channel where the renaming shall begin (default 0)
    -I, --instance    specify the instance name used for communication
    -F, --signal_freq frequency of the tone to be generated in Hz (<=sampleFreq/2) (default 0)
    -A, --amplitude   level/amplitude of the tone in dBFS (0 = full scale, -6 = half, etc.) (default 0)
    -w, --wave_form   wave form selection (default sine=0)
    -u, --user_param  additional param to modify wave generation, depending on mode (default 0)
    -R, --packet_rate maximum video packet rate (default 4000)
    -S, --packet_size maximum video packet size (default 1460)
    -t, --timeout     timeout for command execution (default 5000 ms)
    -v, --verbose     verbose mode, use with --help

Arguments can be passed either in fixed order or arbitrary order with the options.

    Fixed order : avb_streamhandler_client_app CreateTransmitAvbAudioStream H 2 48000 1 0x00 0 0x91E0F000FE000001 0x91E0F0000001 1
    With option names: avb_streamhandler_client_app CreateTransmitAvbAudioStream -c 2 -r 48000 -f 1 -C 0x00 -m 0 -n 0x91E0F000FE000001 -M 0x91E0F0000001 -a 1

You can omit options that have a default value:

    avb_streamhandler_client_app CreateTransmitAvbAudioStream -m 0 -n 0x91E0F000FE000001 -M 0x91E0F0000001 -a

### invocation

the avb_streamhandler_client_app has to run using the same group as the streamhandler (ias_avb).
The easiest way to do this on the command line is to open a shell for user e.g. ias_avb:

    su -s /bin/sh - ias_avb
    export COMMONAPI_CONFIG=/opt/ias/etc/avb_streamhandler_commonapi.ini
    export COMMONAPI_DBUS_CONFIG=/opt/ias/etc/avb_streamhandler_dbus.ini
    avb_streamhandler_client_app_dbus GetAvbStreamInfo

####
@section diag_counter Diagnostic Counters
The validation by AVnu requires special packages with diagnostic information to be sent. this can be enabled by creating a configuration profile

    testing.profile.enable=1 // 0=off, 1=on (default: off)



