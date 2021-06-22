The following is a sample example when evaluating with the R-Car V3U Falcon board.

After starting the Kernel by applying Test_patch-media-vsp1-Change-ARGB8888-YCbCr-422-10bit.patch,
Check that the display destination monitor has a greenish screen.
Connect the Camera device to the Falcon board and execute the following command after starting Target.

Camera device is supported only LI-AR0231-AP0200-GMSL2-xxxH

# media-ctl -d /dev/media0 -l "'rcar_csi2 feaa0000.csi2':1 -> 'VIN0 output':0 [1]"
# media-ctl -d /dev/media0 -l "'rcar_csi2 feaa0000.csi2':2 -> 'VIN1 output':0 [1]"
# media-ctl -d /dev/media0 -l "'rcar_csi2 feaa0000.csi2':3 -> 'VIN2 output':0 [1]"
# media-ctl -d /dev/media0 -l "'rcar_csi2 feaa0000.csi2':4 -> 'VIN3 output':0 [1]"
# media-ctl -d /dev/media0 -V "'rcar_csi2 feaa0000.csi2':1 [fmt:Y10_1X10/1920x1020 field:none]"
# media-ctl -d /dev/media0 -l "'rcar_csi2 fed60000.csi2':1 -> 'VIN16 output':0 [1]"
# media-ctl -d /dev/media0 -l "'rcar_csi2 fed60000.csi2':2 -> 'VIN17 output':0 [1]"
# media-ctl -d /dev/media0 -l "'rcar_csi2 fed60000.csi2':3 -> 'VIN18 output':0 [1]"
# media-ctl -d /dev/media0 -l "'rcar_csi2 fed60000.csi2':4 -> 'VIN19 output':0 [1]"
# media-ctl -d /dev/media0 -V "'rcar_csi2 fed60000.csi2':1 [fmt:Y10_1X10/1920x1020 field:none]"
# media-ctl -d /dev/media0 -l "'rcar_csi2 fed70000.csi2':1 -> 'VIN24 output':0 [1]"
# media-ctl -d /dev/media0 -l "'rcar_csi2 fed70000.csi2':2 -> 'VIN25 output':0 [1]"
# media-ctl -d /dev/media0 -l "'rcar_csi2 fed70000.csi2':3 -> 'VIN26 output':0 [1]"
# media-ctl -d /dev/media0 -l "'rcar_csi2 fed70000.csi2':4 -> 'VIN27 output':0 [1]"
# media-ctl -d /dev/media0 -V "'rcar_csi2 fed70000.csi2':1 [fmt:Y10_1X10/1920x1020 field:none]"

Ex)
In case of 4k monitor
# ./capture -D 12 -F -f raw10 -L 480 -T 180 -W 960 -H 720 -c 10000 -z

In case of Full HD monitor
# ./capture -D 12 -F -f raw10 -L 480 -T 180 -W 480 -H 360 -c 10000 -z


