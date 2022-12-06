## Настройка сервера USB Hasp на базе Ubuntu Server 16.04 LTS (xenial)
###### !АХТУНГ! Использовалась версия Ubuntu Server 16.04 LTS x86_64 с версией ядра linux-image-4.4.0-210-generic

## 1 Подготовка 
###### 1.1 Установка необходимого софта
```sh
sudo apt update
sudo apt install gcc g++ make libjansson-dev libusb-dev libc6-i386 git
```

###### 1.2 Установка компонентов ядра
```sh
sudo apt update
sudo apt install linux-headers-`uname -r`
sudo apt install linux-tools-lts-xenial
```


## 2 Загрузка исходных кодов компонентов ядра и их сборка
```sh
cd /usr/src
git clone https://github.com/rusishsoft/VH_Act.git
```

###### 2.1. Сборка и установка VHCI HCD
```sh
KVER=`uname -r`
cd vhci-hcd
sudo mkdir -p linux/${KVER}/drivers/usb/core
sudo cp /usr/src/linux-headers-4.4.0-210-generic/include/linux/usb/hcd.h linux/${KVER}/drivers/usb/core
sudo sed -i 's/#define DEBUG/\/\/#define DEBUG/' usb-vhci-hcd.c
sudo sed -i 's/#define DEBUG/\/\/#define DEBUG/' usb-vhci-iocifc.c
sudo make KVERSION=${KVER}

sudo make install

sudo tee -a /etc/modules <<< "usb_vhci_hcd"
sudo modprobe usb_vhci_hcd

sudo tee -a /etc/modules <<< "usb_vhci_iocifc"
sudo modprobe usb_vhci_iocifc
```

###### 2.2. Сборка и установка LibUSB VHCI
```sh
cd ../libusb_vhci
sudo ./configure
sudo make -s

sudo make install

sudo tee /etc/ld.so.conf.d/libusb_vhci.conf <<< "/usr/local/lib"
sudo ldconfig
```

###### 2.3. Сборка и установка UsbHaspEmul
```sh
cd ../UsbHasp
sudo make -s

sudo cp dist/Release/GNU-Linux/usbhasp /usr/local/sbin
sudo mkdir /etc/usbhaspkey/
```

Создадим unit usbhaspemul.service
```sh
sudo nano /etc/systemd/system/usbhaspemul.service
```
и добавим в него следующее содержимое
```sh
[Unit]
Description=Emulation HASP key for 1C
Requires=haspd.service
After=haspd.service
[Service]
Type=simple
ExecStart=/usr/bin/sh -c 'find /etc/usbhaspkey -name "*.json" | xargs /usr/local/sbin/usbhasp'
Restart=always
[Install]
WantedBy=multi-user.target
```
