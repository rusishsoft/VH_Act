## Настройка сервера USB Hasp на базе Ubuntu Server 16.04 LTS (xenial)
###### !АХТУНГ! Использовалась версия Ubuntu Server 16.04 LTS x86_64 с версией ядра linux-image-4.4.0-210-generic


###### 1.1 Подготовка. Установка необходимого софта

```sh
sudo apt update
sudo apt install gcc g++ make libjansson-dev libusb-dev git
```

###### 1.2 Подготовка. Установка компонентов ядра


```sh
sudo apt update
sudo apt install linux-headers-`uname -r`
sudo apt install linux-tools-lts-xenial
```

###### 2.1 Загрузка исходных кодов компонентов ядра и их сборка
```sh
cd /usr/src
git clone 
```
