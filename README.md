## Настройка сервера USB Hasp на базе Ubuntu Server 16.04 LTS (xenial)
###### !АХТУНГ! Использовалась версия Ubuntu Server 16.04 LTS x86_64 с версией ядра linux-image-4.4.0-210-generic


###### 1.1 Подготовка. Установка необходимого софта

```bash
sudo apt update
sudo apt install gcc g++ make libjansson-dev libusb-dev git
```

###### 1.2 Подготовка. Установка компонентов ядра


```bash
sudo apt update
sudo apt install linux-headers-`uname -r`
sudo apt install linux-tools-lts-xenial
```
