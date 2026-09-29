#!/bin/bash
# hugepage.sh – 1-GB-Hugepag

PAGE_DIR="/sys/kernel/mm/hugepages/hugepages-1048576kB/nr_hugepages"
MOUNT_DIR="/mnt/huge"

echo ">>> Reserve 1 GB Hugepage"
echo 1 | sudo tee ${PAGE_DIR}
echo "Active: $(cat ${PAGE_DIR})"

echo ">>> Mountpoint setup"
sudo mkdir -p ${MOUNT_DIR}

if mountpoint -q ${MOUNT_DIR}; then
    echo "Already mountet at ${MOUNT_DIR}"
else
    sudo mount -t hugetlbfs -o pagesize=1G none ${MOUNT_DIR}
    sudo chown ${USER}:${USER} ${MOUNT_DIR}
    echo "Mountet at ${MOUNT_DIR}"
fi
