#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

// Do not use the ns3 namespace to avoid conflicts with other libraries.
// Instead, explicitly qualify ns3 types and functions.

int main(int argc, char *argv[])
{
    // Set default time resolution to nanoseconds.
    ns3::Time::SetResolution(ns3::NanoSeconds);

    // Enable logging for relevant components.
    ns3::LogComponentEnable("UdpEchoClientApplication", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("UdpEchoServerApplication", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("WifiPhy", ns3::LOG_LEVEL_INFO); // Optional, for detailed PHY events.

    // 1. Create two nodes: one AP and one STA.
    ns3::NodeContainer nodes;
    nodes.Create(2);

    ns3::Ptr<ns3::Node> apNode = nodes.Get(0);
    ns3::Ptr<ns3::Node> staNode = nodes.Get(1);

    // 2. Install mobility model (static positions).
    ns3::MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", ns3::DoubleValue(0.0),
                                  "MinY", ns3::DoubleValue(0.0),
                                  "DeltaX", ns3::DoubleValue(5.0), // AP at (0,0), STA at (5,0)
                                  "DeltaY", ns3::DoubleValue(0.0),
                                  "GridWidth", ns3::UintegerValue(2),
                                  "LayoutType", ns3::StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // 3. Configure Wi-Fi.
    ns3::WifiHelper wifi;
    // Default Wi-Fi data rate and models are used as no specific standard or rate manager is set.

    ns3::YansWifiPhyHelper phy;
    ns3::YansWifiChannelHelper channel;
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss("ns3::FriisPropagationLossModel");
    phy.SetChannel(channel.Create());

    ns3::NqosWifiMacHelper mac = ns3::NqosWifiMacHelper::Default();
    ns3::Ssid ssid = ns3::Ssid("my-wifi-network");

    // Configure AP MAC.
    mac.SetType("ns3::ApWifiMac",
                "Ssid", ns3::SsidValue(ssid),
                "BeaconInterval", ns3::TimeValue(ns3::MicroSeconds(102400)));
    ns3::NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, apNode);

    // Configure STA MAC.
    mac.SetType("ns3::StaWifiMac",
                "Ssid", ns3::SsidValue(ssid),
                "ActiveProbing", ns3::BooleanValue(false)); // STA doesn't need to probe explicitly in this simple setup
    ns3::NetDeviceContainer staDevice;
    staDevice = wifi.Install(phy, mac, staNode);

    // 4. Install Internet Stack.
    ns3::InternetStackHelper stack;
    stack.Install(nodes);

    // 5. Assign IP Addresses.
    ns3::Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer apInterfaces;
    apInterfaces = address.Assign(apDevice);
    ns3::Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevice);

    // 6. Configure Applications (UDP Echo Server and Client).

    // UDP Echo Server on AP (Node 0).
    uint16_t port = 9; // Echo port
    ns3::UdpEchoServerHelper echoServer(port);
    ns3::ApplicationContainer serverApps = echoServer.Install(apNode);
    serverApps.Start(ns3::Seconds(1.0));
    serverApps.Stop(ns3::Seconds(10.0));

    // UDP Echo Client on STA (Node 1).
    ns3::UdpEchoClientHelper echoClient(apInterfaces.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", ns3::UintegerValue(1000)); // Sufficiently large number of packets
    echoClient.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(0.1))); // 0.1s interval
    echoClient.SetAttribute("PacketSize", ns3::UintegerValue(1024)); // Default packet size

    ns3::ApplicationContainer clientApps = echoClient.Install(staNode);
    clientApps.Start(ns3::Seconds(2.0));
    clientApps.Stop(ns3::Seconds(10.0));

    // 7. Run Simulation.
    ns3::Simulator::Stop(ns3::Seconds(10.0));
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}