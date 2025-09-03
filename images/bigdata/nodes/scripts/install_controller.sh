if sudo growpart /dev/sda 1; then
  resize2fs /dev/sda1
else
  echo "Root partition cannot be grown"
fi

hostnamectl set-hostname controller