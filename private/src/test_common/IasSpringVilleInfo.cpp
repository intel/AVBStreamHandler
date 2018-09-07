#include "test_common/IasSpringVilleInfo.hpp"
#include <iostream>
#include <sstream>

namespace IasMediaTransportAvb {

const uint16_t IasSpringVilleInfo::cPCISpringvilleCard1 = 0x1533;
const uint16_t IasSpringVilleInfo::cPCISpringvilleCard2 = 0x1531;
const uint16_t IasSpringVilleInfo::cEthernetID = 0x200;
const uint16_t IasSpringVilleInfo::cVendorID = 0x8086; // Intel Corporation

std::string IasSpringVilleInfo::mBusID;
std::string IasSpringVilleInfo::mDeviceID;
std::string IasSpringVilleInfo::mInterfaceName;

uint8_t IasSpringVilleInfo::mBusIdHexValue = 0x00;
uint16_t IasSpringVilleInfo::mDeviceIDHexValue = 0x0000;

bool IasSpringVilleInfo::fetchData(bool force)
{
  bool success = false;
  pci_access* pciAccess = NULL;
  pci_dev* pciDevice = NULL;

  // If we've already been called and all the data is set then just return.
  if (!force && mBusID.size() && mDeviceID.size() && mInterfaceName.size())
  {
    return true;
  }

  pciAccess = pci_alloc();
  pci_init(pciAccess);
  pci_scan_bus(pciAccess);

  pciDevice = pciAccess->devices;
  while (NULL != pciDevice)
  {
    pci_fill_info(pciDevice, PCI_FILL_IDENT | PCI_FILL_BASES | PCI_FILL_CLASS);

    if (cVendorID == pciDevice->vendor_id)
    {
      // check if a springville card is found
      if (cEthernetID == pciDevice->device_class)
      {
          if ((cPCISpringvilleCard1 == pciDevice->device_id) || (cPCISpringvilleCard2 == pciDevice->device_id))
          {
            std::stringstream ss;
            ss     << std::hex << static_cast<int>(pciDevice->bus);
            ss >> mBusID;
            mBusIdHexValue = pciDevice->bus;
            ss.clear();
            ss  << std::hex << static_cast<int>(pciDevice->device_id);
            ss >> mDeviceID;
            mDeviceIDHexValue = pciDevice->device_id;
            success = getInterfaceName(pciDevice, mInterfaceName);

            int maxPath = 255;
            const char* syspath = "/sys/bus/pci/devices/";
            char path[maxPath];
            snprintf(path, maxPath, "%s%04x:%02x:%02x.%d",syspath, pciDevice->domain, pciDevice->bus, pciDevice->dev, pciDevice->func);
            printf("Springville card: %x - path - %s\n", pciDevice->device_id, path);

            break;
          }
      }
    }
    pciDevice = pciDevice->next;
  }
  pci_cleanup(pciAccess);

  printf("FetchData[%s]: mBusID [%s / %#02x], mDeviceID [%s / %#04x], mInterfaceName [%s]\n",
      (success?"SUCCESS":"ERROR"), getBusId(), getBusIdHexValue(), getDeviceId(), getDeviceIdHexValue(), getInterfaceName());

  return success;
}

bool IasSpringVilleInfo::getInterfaceName(pci_dev* pciDevice, std::string& ifname)
{
  bool nameFound = false;
  const uint8_t maxPath = 255;
  char path[maxPath];
  const char* syspath = "/sys/bus/pci/devices/";
  const char* netdir = "/net/";
  snprintf(path, maxPath, "%s%04x:%02x:%02x.%d%s",syspath, pciDevice->domain, pciDevice->bus, pciDevice->dev,
      pciDevice->func, netdir);

  const char* nonDir[] = {".", ".."};

  dirent* dirEntry;
  DIR* directory = opendir(path);

  if (NULL != directory)
  {
    while (NULL != (dirEntry = readdir(directory)))
    {
      if ((0 != strcmp(dirEntry->d_name, nonDir[0])) && (0 != strcmp(dirEntry->d_name, nonDir[1])))
      {
        ifname = dirEntry->d_name;
        nameFound = true;
        break;
      }
    }
    closedir(directory);
  }
  else
  {
    printf("ERROR PCISpringville card directory doesn't exist\n");
  }

  return nameFound;
}

} // namespace
