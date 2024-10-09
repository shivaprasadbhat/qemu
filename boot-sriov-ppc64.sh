gdb --args /home/shiva-user/qemu/build/qemu-system-ppc64 -machine pseries,usb=off,dump-guest-core=off,cap-htm=off,cap-cfpc=broken,cap-ccf-assist=off,cap-fwnmi=off \
-accel kvm -nographic -m size=8g -cpu host -smp 128,sockets=4,dies=1,cores=4,threads=8 -monitor telnet:localhost:1248,server,nowait \
-drive file=/home/shiva-user//images/Fedora-Server-KVM-40-1.14.build-guest-ppc64le.qcow2,format=qcow2,if=none,id=drive-virtio-disk0 \
-device virtio-blk-pci,drive=drive-virtio-disk0,id=virtio-disk0 -serial mon:stdio \
-drive file=build-guest/tmpdisk.qcow2,format=qcow2,if=none,id=drive-virtio-disk2,cache=directsync \
-device virtio-blk-pci,bus=pci.0,addr=0x7,drive=drive-virtio-disk2,id=virtio-disk2,bootindex=3 \
-fsdev local,id=kernel,path=/home/shiva-user//,security_model=passthrough,writeout \
-device virtio-9p-pci,fsdev=kernel,mount_tag=host_bld -object memory-backend-file,id=ram-node0,mem-path=/dev/shm,share=yes,size=8g \
-numa node,nodeid=0,memdev=ram-node0,cpus=0-127 \
-netdev user,id=n -netdev user,id=o -netdev user,id=p -netdev user,id=q \
-netdev user,id=r \
-device spapr-pci-host-bridge,index=1,id=pci.1,pcie-extended-configuration-space=on \
-device igb,mac=DE:AD:BE:EE:04:18,netdev=n,bus=pci.1.0 \
-kernel /home/shiva-user/worktrees/post-final/build-guest/vmlinux -initrd /home/shiva-user/worktrees/post-final/build-guest/initrd.img-6.11.0-rc7-00157-g5a3bd613c389-dirty \
-append "BOOT_IMAGE=(ieee1275/disk,gpt2)/vmlinuz-6.8.5-301.fc40.ppc64le root=/dev/mapper/sysvg-root ro console=tty1 console=ttyS0,115200n8 rd.lvm.lv=sysvg/root selinux=0 udbg-immortal audit=0 movable_node=1 nmi_watchdog=0"

:'
#Multifunction
-device virtio-net-pci,bus=pci.1.0,addr=0x0.0x4,netdev=q,sriov-pf=f  \
-device virtio-net-pci,bus=pci.1.0,addr=0x0.0x3,netdev=p,sriov-pf=f \
-device virtio-net-pci,bus=pci.1.0,addr=0x0.0x2,netdev=o,sriov-pf=f \
-device virtio-net-pci,bus=pci.1.0,addr=0x0.0x1,netdev=r,id=g \
-device virtio-net-pci,bus=pci.1.0,addr=0x0.0x0,netdev=n,id=f,multifunction=on \

device_add virtio-net-pci,bus=pci.1.0,addr=0x0.0x4,netdev=q,sriov-pf=f
device_add virtio-net-pci,bus=pci.1.0,addr=0x0.0x3,netdev=p,sriov-pf=f
device_add virtio-net-pci,bus=pci.1.0,addr=0x0.0x2,netdev=o,sriov-pf=f
device_add virtio-net-pci,bus=pci.1.0,addr=0x0.0x1,netdev=n,id=g
device_add virtio-net-pci,bus=pci.1.0,addr=0x0.0x0,netdev=r,id=f,multifunction=on

#Single function
-device virtio-net-pci,bus=pci.1.0,addr=0x0.0x3,netdev=q,sriov-pf=f  \
-device virtio-net-pci,bus=pci.1.0,addr=0x0.0x2,netdev=p,sriov-pf=f \
-device virtio-net-pci,bus=pci.1.0,addr=0x0.0x1,netdev=o,sriov-pf=f \
-device virtio-net-pci,bus=pci.1.0,addr=0x0.0x0,netdev=n,id=f


device_add virtio-net-pci,bus=pci.1.0,addr=0x0.0x0,netdev=r,id=f
device_add virtio-net-pci,bus=pci.1.0,addr=0x0.0x3,netdev=q,sriov-pf=f
device_add virtio-net-pci,bus=pci.1.0,addr=0x0.0x2,netdev=p,sriov-pf=f
device_add virtio-net-pci,bus=pci.1.0,addr=0x0.0x1,netdev=o,sriov-pf=f
device_add virtio-net-pci,bus=pci.1.0,addr=0x0.0x0,netdev=r,id=f
'
