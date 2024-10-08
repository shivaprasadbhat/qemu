gdb --args /home/sbhat/code/qemu/build/x86_64-softmmu/qemu-system-x86_64 \
-name guest=something,debug-threads=on -machine q35,cxl=on,usb=off,dump-guest-core=off,nvdimm=on,kernel-irqchip=on \
-accel kvm -m size=12G -cpu Icelake-Server -smp 8,sockets=1,dies=1,cores=4,threads=2 \
-monitor telnet:localhost:1248,server,nowait -overcommit mem-lock=off -nographic -rtc base=utc,driftfix=none \
-global kvm-pit.lost_tick_policy=delay -global PIIX4_PM.disable_s3=1 -global PIIX4_PM.disable_s4=1 \
-boot strict=on -device ich9-usb-ehci1,id=usb,bus=pcie.0,addr=0x4.0x7 \
-device ich9-usb-uhci1,masterbus=usb.0,firstport=0,bus=pcie.0,multifunction=on,addr=0x4 \
-device ich9-usb-uhci2,masterbus=usb.0,firstport=2,bus=pcie.0,addr=0x4.0x1 \
-device ich9-usb-uhci3,masterbus=usb.0,firstport=4,bus=pcie.0,addr=0x4.0x2 \
-msg timestamp=on -serial mon:stdio -device virtio-blk-pci,bus=pcie.0,addr=0x5,drive=drive-virtio-disk0,id=virtio-disk0,bootindex=1 \
-drive file=/home/sbhat//images/Fedora-Cloud-Base-Generic.build-linux-x86_64-40-1.14.qcow2,format=qcow2,if=none,id=drive-virtio-disk0 \
-device virtio-balloon-pci,id=balloon0,bus=pcie.0 \
-netdev user,id=n -netdev user,id=o \
-netdev user,id=p -netdev user,id=q \
-netdev user,id=r \
-device pcie-root-port,id=b \

Multifunction
-device virtio-net-pci,bus=b,addr=0x0.0x4,netdev=q,sriov-pf=f \
-device virtio-net-pci,bus=b,addr=0x0.0x3,netdev=p,sriov-pf=f \
-device virtio-net-pci,bus=b,addr=0x0.0x2,netdev=o,sriov-pf=f \
-device virtio-net-pci,bus=b,addr=0x0.0x1,netdev=r,id=g \
-device virtio-net-pci,bus=b,addr=0x0.0x0,netdev=n,id=f,multifunction=on

device_add virtio-net-pci,bus=b,addr=0x0.0x4,netdev=q,sriov-pf=f
device_add virtio-net-pci,bus=b,addr=0x0.0x3,netdev=p,sriov-pf=f
device_add virtio-net-pci,bus=b,addr=0x0.0x2,netdev=o,sriov-pf=f
device_add virtio-net-pci,bus=b,addr=0x0.0x1,netdev=r,id=g
device_add virtio-net-pci,bus=b,addr=0x0.0x0,netdev=n,id=f,multifunction=on

Single function
-device virtio-net-pci,bus=b,addr=0x0.0x0,netdev=n,id=f
-device virtio-net-pci,bus=b,addr=0x0.0x3,netdev=q,sriov-pf=f \
-device virtio-net-pci,bus=b,addr=0x0.0x2,netdev=p,sriov-pf=f \
-device virtio-net-pci,bus=b,addr=0x0.0x1,netdev=o,sriov-pf=f \

device_add virtio-net-pci,bus=b,addr=0x0.0x0,netdev=n,id=f
device_add virtio-net-pci,bus=b,addr=0x0.0x3,netdev=q,sriov-pf=f
device_add virtio-net-pci,bus=b,addr=0x0.0x2,netdev=p,sriov-pf=f
device_add virtio-net-pci,bus=b,addr=0x0.0x1,netdev=o,sriov-pf=f
