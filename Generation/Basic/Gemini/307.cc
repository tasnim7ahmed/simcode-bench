#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/udp-echo-helper.h"

// NS_LOG_COMPONENT_DEFINE("WifiTwoNodes80211n"); // Not needed for the final output, but good for debugging

int
main(int argc, char* argv[])
{
    // LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("WifiPhy", LOG_LEVEL_INFO); // Example for debugging Wi-Fi

    // 1. Create two nodes
    ns3::NodeContainer nodes;
    nodes.Create(2);

    // 2. Set node positions using ConstantPositionMobilityModel
    ns3::MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    nodes.Get(0)->GetObject<ns3::ConstantPositionMobilityModel>()->SetPosition(ns3::Vector(0.0, 0.0, 0.0));
    nodes.Get(1)->GetObject<ns3::ConstantPositionMobilityModel>()->SetPosition(ns3::Vector(5.0, 0.0, 0.0)); // 5 meters apart

    // 3. Configure Wi-Fi (802.11n 5GHz)
    ns3::WifiHelper wifi;
    wifi.SetStandard(ns3::WIFI_PHY_STANDARD_80211n_5GHZ);

    ns3::YansWifiPhyHelper wifiPhy;
    wifiPhy.SetPcapDataLinkType(ns3::WifiPhyHelper::DLT_IEEE802_11_RADIO); // Optional: for pcap traces

    // Create and configure the Wi-Fi channel
    ns3::YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationLoss("ns3::FriisPropagationLossModel");
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    wifiPhy.SetChannel(wifiChannel.Create());

    // Set up MAC layer for ad-hoc mode
    ns3::HtWifiMacHelper wifiMac = ns3::HtWifiMacHelper::Default();
    wifiMac.SetType("ns3::AdhocWifiMac");

    // Install Wi-Fi devices on nodes
    ns3::NetDeviceContainer wifiDevices = wifi.Install(wifiPhy, wifiMac, nodes);

    // 4. Install Internet stack
    ns3::InternetStackHelper stack;
    stack.Install(nodes);

    // 5. Assign IP addresses (192.168.1.x)
    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = ipv4.Assign(wifiDevices);

    // 6. Setup UDP Echo Server on node 0
    uint16_t port = 9; // UDP Echo port
    ns3::UdpEchoServerHelper echoServer(port);
    ns3::ApplicationContainer serverApps = echoServer.Install(nodes.Get(0));
    serverApps.Start(ns3::Seconds(1.0));
    serverApps.Stop(ns3::Seconds(10.0));

    // 7. Setup UDP Echo Client on node 1
    ns3::UdpEchoClientHelper echoClient(interfaces.GetAddress(0), port); // Server IP: node 0's IP
    echoClient.SetAttribute("MaxPackets", ns3::UintegerValue(10));
    echoClient.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", ns3::UintegerValue(1024));
    ns3::ApplicationContainer clientApps = echoClient.Install(nodes.Get(1));
    clientApps.Start(ns3::Seconds(2.0));
    clientApps.Stop(ns3::Seconds(10.0));

    // Optional: Enable Pcap tracing
    // wifiPhy.EnablePcap("wifi-two-nodes", wifiDevices);

    // 8. Run simulation
    ns3::Simulator::Stop(ns3::Seconds(10.0));
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}