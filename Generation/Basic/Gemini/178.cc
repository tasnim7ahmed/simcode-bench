#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

int main(int argc, char *argv[])
{
    // 1. Create two nodes
    ns3::NodeContainer nodes;
    nodes.Create(2);

    // 2. Configure Wi-Fi for 802.11n in Ad-hoc mode
    // Channel configuration
    ns3::YansWifiChannelHelper channel = ns3::YansWifiChannelHelper::Default();
    ns3::YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    // MAC configuration for Ad-hoc mode
    ns3::WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac"); // Ad-hoc mode

    // Wi-Fi Helper to set the standard (802.11n) and install devices
    ns3::WifiHelper wifi;
    // Set 802.11n standard (e.g., 5GHz band)
    wifi.SetStandard(ns3::WIFI_PHY_STANDARD_80211n_5GHZ);

    // Install Wi-Fi devices on nodes
    ns3::NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // 3. Install Internet stack on nodes
    ns3::InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to the Wi-Fi interfaces
    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // 4. Setup UDP Client-Server Application
    // Server on Node 0
    ns3::UdpEchoServerHelper echoServer(9); // Port 9
    ns3::ApplicationContainer serverApps = echoServer.Install(nodes.Get(0));
    serverApps.Start(ns3::Seconds(1.0));
    serverApps.Stop(ns3::Seconds(10.0));

    // Client on Node 1, sending to Node 0
    ns3::UdpEchoClientHelper echoClient(interfaces.GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", ns3::UintegerValue(5));    // Send 5 packets
    echoClient.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(1.0))); // 1 packet per second
    echoClient.SetAttribute("PacketSize", ns3::UintegerValue(1024)); // 1024 bytes per packet
    ns3::ApplicationContainer clientApps = echoClient.Install(nodes.Get(1));
    clientApps.Start(ns3::Seconds(2.0)); // Client starts after server
    clientApps.Stop(ns3::Seconds(10.0));

    // Configure node positions for NetAnim visualization
    ns3::MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);
    nodes.Get(0)->GetObject<ns3::ConstantPositionMobilityModel>()->SetPosition(ns3::Vector(0.0, 0.0, 0.0));
    nodes.Get(1)->GetObject<ns3::ConstantPositionMobilityModel>()->SetPosition(ns3::Vector(5.0, 0.0, 0.0)); // Nodes 5 meters apart

    // 5. Enable NetAnim visualization
    ns3::NetAnimHelper netanim;
    netanim.EnableAnimation("wifi-ad-hoc-netanim.xml");

    // 6. Set simulation stop time and run
    ns3::Simulator::Stop(ns3::Seconds(10.0));
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}