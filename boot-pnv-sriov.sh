sudo   ~/qemu/build/qemu-system-ppc64 -m 8G -M powernv \
-kernel ./build-pnv/vmlinux -append root=/dev/nvme0n1 \
-device nvme-subsys,id=subsys1,nqn=1 \
-device nvme,bus=pcie.2,addr=0x0,serial=1234,sriov_max_vfs=4,sriov_vq_flexible=8,sriov_vi_flexible=8,sriov_max_vi_per_vf=8,sriov_max_vq_per_vf=8,subsys=subsys1 \
-blockdev '{"driver":"file","filename":"/home/shiva-user/qemu-ppc-boot/buildroot/qemu_ppc64le_powernv8-latest/rootfs.ext2","node-name":"path1"}' \
-blockdev '{"node-name":"drive0","read-only":false,"driver":"raw","file":"path1"}' \
-device nvme-ns,drive=drive0,nsid=1  \
-device e1000e,bus=pcie.1,addr=0x0,netdev=net0 \
-netdev user,id=net0 -serial mon:stdio -nographic \
-snapshot  -bios skiboot.lid

