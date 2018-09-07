/**
 *  @file IasSpringVilleInfo.hpp
 *  @date 29/05/2013
 */

#ifndef IASSPRINGVILLEINFO_HPP_
#define IASSPRINGVILLEINFO_HPP_

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>

extern "C"{
#include <pci/pci.h>
}
#include <dirent.h>

namespace IasMediaTransportAvb
{

class IasSpringVilleInfo
{
  public:
    static bool fetchData(bool force = false);
    static const char* getBusId();
    static uint8_t getBusIdHexValue();
    static const char* getDeviceId();
    static uint16_t getDeviceIdHexValue();
    static const char* getInterfaceName();
    static void printDebugInfo();

  private:
    IasSpringVilleInfo();
    ~IasSpringVilleInfo();

    /**
     * @brief Copy constructor, private unimplemented to prevent misuse.
     */
    IasSpringVilleInfo(IasSpringVilleInfo const &other);

    /**
     * @brief Helper function to get the interface name.
     */
    static bool getInterfaceName(pci_dev* pciDevice, std::string& ifname);

    static const uint16_t cPCISpringvilleCard1;
    static const uint16_t cPCISpringvilleCard2;
    static const uint16_t cEthernetID;
    static const uint16_t cVendorID; // Intel Corporation

    static std::string mBusID;
    static uint8_t mBusIdHexValue;
    static std::string mDeviceID;
    static uint16_t mDeviceIDHexValue;
    static std::string mInterfaceName;

};

inline const char* IasSpringVilleInfo::getBusId()
{
  return mBusID.c_str();
}

inline uint8_t IasSpringVilleInfo::getBusIdHexValue()
{
  return mBusIdHexValue;
}

inline const char* IasSpringVilleInfo::getDeviceId()
{
  return mDeviceID.c_str();
}

inline uint16_t IasSpringVilleInfo::getDeviceIdHexValue()
{
  return mDeviceIDHexValue;
}

inline const char* IasSpringVilleInfo::getInterfaceName()
{
  return mInterfaceName.c_str();
}

inline void IasSpringVilleInfo::printDebugInfo()
{
  printf("SpringVille Found [%s]: Interface name[%s], deviceID[%s], busID[%s]\n",
      (mInterfaceName.size() && mBusID.size() && mDeviceID.size() ? "TRUE" : "FALSE"),
      getInterfaceName(), getDeviceId(), getBusId());
}

} // namespace

#endif /* IASSPRINGVILLEINFO_HPP_ */
