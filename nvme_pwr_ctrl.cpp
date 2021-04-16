#include <boost/asio/io_service.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <gpiod.hpp>
#include <string.h>
#include <iostream>
#include <ctype.h>
#include <sdbusplus/asio/object_server.hpp>
#include <variant>

#define MAX_SSD_NUMS 16

namespace nvme_pwr_ctrl
{
static boost::asio::io_service io;
static std::shared_ptr<sdbusplus::asio::connection> conn;
static std::shared_ptr<sdbusplus::asio::dbus_interface> pwrDisIface[MAX_SSD_NUMS];
static std::shared_ptr<sdbusplus::asio::dbus_interface> pwrEnIface[MAX_SSD_NUMS];

static constexpr const char* nvmePowerService = "xyz.openbmc_project.Control.Nvme.Power";
static constexpr const char* nvmePowerIface = "xyz.openbmc_project.Control.Nvme.Power";
static std::string nvmePowerPath = "/xyz/openbmc_project/control/nvme/";
static std::string pwrDisTemplate = "u2_x_pwr_dis";
static std::string pwrEnTemplate = "pwr_u2_x_en";
static std::string pwrDisPath[MAX_SSD_NUMS];
static std::string pwrEnPath[MAX_SSD_NUMS];
static std::string pwrDisOut[MAX_SSD_NUMS];
static std::string pwrEnOut[MAX_SSD_NUMS];

static bool setGPIOOutput(const std::string& name, const int value,
                          gpiod::line& gpioLine)
{
    // Find the GPIO line
    gpioLine = gpiod::find_line(name);
    if (!gpioLine)
    {
        std::cerr << "Failed to find the " << name << " line.\n";
        return false;
    }

    // Request GPIO output to specified value
    try
    {
        gpioLine.request({__FUNCTION__, gpiod::line_request::DIRECTION_OUTPUT},
                         value);
    }
    catch (std::exception&)
    {
        std::cerr << "Failed to request " << name << " output\n";
        return false;
    }

    std::cerr << name << " set to " << std::to_string(value) << "\n";
    return true;
}

static void PowerStateMonitor()
{
    static auto match = sdbusplus::bus::match::match(
        *conn,
        "type='signal',member='PropertiesChanged', "
        "interface='org.freedesktop.DBus.Properties', "
        "arg0namespace=xyz.openbmc_project.Control.Nvme.Power",
        [](sdbusplus::message::message& m) {
            std::string intfName;
            boost::container::flat_map<std::string,
                                           std::variant<bool, std::string>>
                    propertiesChanged;

            m.read(intfName, propertiesChanged);
            std::string obj_path;
            obj_path = m.get_path();

            std::string line_name;
            size_t pos = 0;
            std::string token;
            std::string delimiter = "/";
            while((pos = obj_path.find(delimiter)) != std::string::npos) {
                 token = obj_path.substr(0, pos);
                 obj_path.erase(0, pos + delimiter.length());
            }
            line_name.assign(obj_path);
            std::cerr << "line_name: " << line_name << "\n";

            try
            {
                auto state = std::get<std::string>(propertiesChanged.begin()->second);
                std::cerr << "state: " << state << "\n";
                gpiod::line line;
                int value;
                if ( state == "xyz.openbmc_project.Control.Nvme.Power.SlotDisabled" \
                     || state == "xyz.openbmc_project.Control.Nvme.Power.On")
                    value = 1;
                else
                    value = 0;
                setGPIOOutput(line_name, value, line);
                // Release line
                line.reset();
            }
            catch (std::exception& e)
            {
                std::cerr << "Unable to read property\n";
                return;
            }
        });
}
}

int main(int argc, char* argv[])
{
    std::cerr << "Start NVMe power control service...\n";
    nvme_pwr_ctrl::conn =
        std::make_shared<sdbusplus::asio::connection>(nvme_pwr_ctrl::io);

    nvme_pwr_ctrl::conn->request_name(nvme_pwr_ctrl::nvmePowerService);
    sdbusplus::asio::object_server server =
        sdbusplus::asio::object_server(nvme_pwr_ctrl::conn);

    std::string sensor_name;
    int i;

    for (i=0; i<MAX_SSD_NUMS; i++) {
        sensor_name.clear();
        sensor_name.assign(nvme_pwr_ctrl::pwrDisTemplate);
        sensor_name.replace(sensor_name.find("x"), 1, std::to_string(i));
        nvme_pwr_ctrl::pwrDisPath[i] = 
            nvme_pwr_ctrl::nvmePowerPath + sensor_name;
        nvme_pwr_ctrl::pwrDisOut[i].assign(sensor_name);
        nvme_pwr_ctrl::pwrDisIface[i] = 
            server.add_interface(
                 nvme_pwr_ctrl::pwrDisPath[i], nvme_pwr_ctrl::nvmePowerIface);
        nvme_pwr_ctrl::pwrDisIface[i]->register_property("Asserted",
            std::string("xyz.openbmc_project.Control.Nvme.Power.SlotEnabled"),
            sdbusplus::asio::PropertyPermission::readWrite);
        nvme_pwr_ctrl::pwrDisIface[i]->initialize();

        sensor_name.clear();
        sensor_name.assign(nvme_pwr_ctrl::pwrEnTemplate);
        sensor_name.replace(sensor_name.find("x"), 1, std::to_string(i));
        nvme_pwr_ctrl::pwrEnPath[i] = 
            nvme_pwr_ctrl::nvmePowerPath + sensor_name;
        nvme_pwr_ctrl::pwrEnOut[i].assign(sensor_name);
        nvme_pwr_ctrl::pwrEnIface[i] = 
            server.add_interface(
                nvme_pwr_ctrl::pwrEnPath[i], nvme_pwr_ctrl::nvmePowerIface);
        nvme_pwr_ctrl::pwrEnIface[i]->register_property("Asserted",
            std::string("xyz.openbmc_project.Control.Nvme.Power.On"),
            sdbusplus::asio::PropertyPermission::readWrite);
        nvme_pwr_ctrl::pwrEnIface[i]->initialize();
    }

    // Initialize PWR_DIS and PWR_EN GPIOs
    gpiod::line gpioLine;
    for (i=0; i<MAX_SSD_NUMS; i++) {
        if (!nvme_pwr_ctrl::setGPIOOutput(
                nvme_pwr_ctrl::pwrDisOut[i], 0, gpioLine))
        {
            return -1;
        }

        if (!nvme_pwr_ctrl::setGPIOOutput(
                nvme_pwr_ctrl::pwrEnOut[i], 1, gpioLine))
        {
            return -1;
        }
    }
    // Release gpioLine
    gpioLine.reset();

    nvme_pwr_ctrl::PowerStateMonitor();

    nvme_pwr_ctrl::io.run();

    return 0;
}
