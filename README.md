## Настройка сервера USB Hasp на базе Ubuntu Server 16.04 LTS (xenial)
###### !АХТУНГ! Использовалась версия Ubuntu Server 16.04 LTS x86_64 с версией ядра linux-image-4.4.0-210-generic

## 1. Подготовка 
###### 1.1. Установка необходимого софта
```sh
sudo apt update
sudo apt install gcc g++ make libjansson-dev libusb-dev libc6-i386 git
```

###### 1.2. Установка компонентов ядра
```sh
sudo apt update
sudo apt install linux-headers-`uname -r`
sudo apt install linux-tools-lts-xenial
```


## 2. Загрузка исходных кодов компонентов ядра и их сборка
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
```unit
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

###### 2.4. Добавление в автозагрузку демона UsbHaspEmul
```sh
sudo systemctl daemon-reload
sudo systemctl enable usbhaspemul
```

###### 2.5. Загрузка дампов ключей в директорию /etc/usbhaspkeys
```sh
cd ../dumps
sudo cp ./1c_server_x64.json /etc/usbhaspkey/
sudo cp ./50user.json /etc/usbhaspkey/
```

###### 2.6. Запуск UsbHaspEmul
```sh
sudo systemctl start usbhaspemul
sudo systemctl status usbhaspemul
```

## 3. Установка и активация VirtualHere
###### !АХТУНГ №2! Несмотря на разрядность ОС Ubuntu, VirtualHere устанавливается в 32-битном режиме

###### 3.1. Установка Linux-based серверной части
```sh
cd ../VirtualHere
sudo chmod +x ./install_server
sudo ./install_server
```

###### 3.2. Установка Windows-based клиентской части
Для этого в данном репозитории в папке "WindowsClient" найдите файл, соответствующий вашей аритектуре:
* Windows (x86) - vhui32.exe
* Windows (x64) - vhui64.exe
* Windows (ARM64) - vhuiarm64.exe

Скачайте и запустите файл.
В открышемся окне выберите обнаруженный хаб, щекните ПКМ и в меню щелните пункт "License"
В открывшемся окне выделите и скопируте значение серийного номера
Desktop Hub,`#0969DA`s/n=FE17189D-5211-C848-A448-788475CB15C8,20 devices
<font color="red">This is some text!</font>
