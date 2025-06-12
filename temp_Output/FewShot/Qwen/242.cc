#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Set default values for IEEE 802.15.4 and 6LoWPAN
    Config::SetDefault("ns3::LrWpanPhy::ChannelPage", UintegerValue(0));
    Config::SetDefault("ns3::LrWpanPhy::Channel", UintegerValue(11));
    Config::SetDefault("ns3::LrWpanCsmaCa::Slotted", BooleanValue(false));

    // Enable logging if needed (optional)
    // LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create nodes: multiple IoT devices + one central server
    NodeContainer devices;
    NodeContainer serverNode;
    devices.Create(5);              // 5 IoT devices
    serverNode.Create(1);           // Central server

    NodeContainer allNodes = NodeContainer(devices, serverNode);

    // Create LR-WPAN helper to install the MAC and PHY layers
    LrWpanHelper lrWpanHelper;
    NetDeviceContainer deviceNetDevices;
    NetDeviceContainer serverNetDevice;

    // Install LR-WPAN devices on all nodes
    deviceNetDevices = lrWpanHelper.Install(devices);
    serverNetDevice = lrWpanHelper.Install(serverNode);

    // Install 6LoWPAN header compression
    SixLowPanHelper sixlowpan;
    NetDeviceContainer sixlowpanDevices;
    sixlowpanDevices.Add(sixlowpan.Install(deviceNetDevices));
    sixlowpanDevices.Add(sixlowpan.Install(serverNetDevice));

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(allNodes);

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6;
    ipv6.SetBase("2001:db8::", Ipv6Prefix(64));
    Ipv6InterfaceContainer deviceInterfaces;
    Ipv6InterfaceContainer serverInterface;

    deviceInterfaces = ipv6.Assign(sixlowpanDevices, devices.GetN() + serverNode.GetN());
    serverInterface = ipv6.Assign(serverNetDevice);

    // Server listens on UDP port 9
    uint16_t udpPort = 9;
    UdpServerHelper server(udpPort);
    ApplicationContainer serverApp = server.Install(serverNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(20.0));

    // Configure each IoT device as a UDP client sending periodically
    for (uint32_t i = 0; i < devices.GetN(); ++i) {
        Ipv6Address serverAddr = serverInterface.GetAddress(0, 1); // Get server's IPv6 address

        UdpClientHelper client(serverAddr, udpPort);
        client.SetAttribute("MaxPackets", UintegerValue(0)); // Infinite packets
        client.SetAttribute("Interval", TimeValue(Seconds(2.0)));
        client.SetAttribute("PacketSize", UintegerValue(64));

        ApplicationContainer clientApp = client.Install(devices.Get(i));
        clientApp.Start(Seconds(2.0 + i)); // Staggered start times
        clientApp.Stop(Seconds(20.0));
    }

    // Setup static routing so devices can reach the server
    for (uint32_t i = 0; i < devices.GetN(); ++i) {
        Ptr<Ipv6StaticRouting> routing = Ipv6RoutingHelper::GetRouting<ns3::Ipv6StaticRouting>(
            devices.Get(i)->GetObject<Ipv6>()->GetRoutingProtocol());
        routing->AddNetworkRouteTo("2001:db8::/64", Ipv6Address::GetZero(), 1);
    }

    // Enable ASCII trace
    AsciiTraceHelper asciiTraceHelper;
    lrWpanHelper.EnableAsciiAll(asciiTraceHelper.CreateFileStream("sixlowpan-udp-server-client.tr"));

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}