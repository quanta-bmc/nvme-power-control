# nvme-power-control
This daemon is to generate NVMe SSD power enable/disable DBus path and could be set from host to control SSD power. 

* DBus Service: ```xyz.openbmc_project.Control.Nvme.Power```
* DBus Interface: ```xyz.openbmc_project.Control.Nvme.Power```
* DBus PWR_DIS Path: ```/xyz/openbmc_project/control/nvme/u2_x_pwr_dis```
* DBus PWR_EN Path: ```/xyz/openbmc_project/control/nvme/pwr_u2_x_en```

PWR_DIS and PWR_EN are the GPIO pins from GPIO expanders and they have to be set as gpio-line-names in Kernel DTS file.
* PWR_DIS gpio-line-names: ```u2_x_pwr_dis```
* PWR_EN gpio-line-names: ```pwr_u2_x_en```
