#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"

int main(int argc, char *argv[])
{
    ns3::Time::SetResolution(ns3::Time::NS);

    ns3::CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create 5 nodes
    ns3::NodeContainer nodes;
    nodes.Create(5);

    // Install AODV routing protocol
    ns3::AodvHelper aodv;
    ns3::InternetStackHelper internetStack;
    internetStack.SetRoutingHelper(aodv);
    internetStack.Install(nodes);

    // Configure Wi-Fi (802.11b, Ad-hoc mode)
    ns3::WifiHelper wifi;
    wifi.SetStandard(ns3::WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    ns3::WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac"); // Ad-hoc mode

    ns3::YansWifiPhyHelper wifiPhy;
    wifiPhy.Set("RxGain", ns3::DoubleValue(-10)); // Set a default RxGain
    wifiPhy.SetPcapDataLinkType(ns3::WifiPhyHelper::DLT_IEEE802_11_RADIO);

    ns3::NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Assign IP addresses
    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Configure mobility (RandomWaypointMobilityModel within 100x100m area)
    ns3::MobilityHelper mobility;

    // Set initial positions randomly within the 100x100m area
    ns3::Ptr<ns3::UniformRandomVariable> x = ns3::CreateObject<ns3::UniformRandomVariable>();
    x->SetAttribute("Min", ns3::DoubleValue(0.0));
    x->SetAttribute("Max", ns3::DoubleValue(100.0));
    ns3::Ptr<ns3::UniformRandomVariable> y = ns3::CreateObject<ns3::UniformRandomVariable>();
    y->SetAttribute("Min", ns3::DoubleValue(0.0));
    y->SetAttribute("Max", ns3::DoubleValue(100.0));
    ns3::Ptr<ns3::UniformRandomVariable> z = ns3::CreateObject<ns3::UniformRandomVariable>();
    z->SetAttribute("Min", ns3::DoubleValue(0.0));
    z->SetAttribute("Max", ns3::DoubleValue(0.0)); // 2D movement

    ns3::Ptr<ns3::ListPositionAllocator> positionAlloc = ns3::CreateObject<ns3::ListPositionAllocator>();
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        positionAlloc->Add(ns3::Vector(x->GetValue(), y->GetValue(), z->GetValue()));
    }
    mobility.SetPositionAllocator(positionAlloc);

    // Set RandomWaypointMobilityModel with bounds for movement
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                              "Speed", ns3::StringValue("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"), // m/s
                              "Pause", ns3::StringValue("ns3::ConstantRandomVariable[Constant=1.0]"), // seconds
                              "Bounds", ns3::RectangleValue(ns3::Rectangle(0, 100, 0, 100))); // 100x100m area
    mobility.Install(nodes);

    // Configure UDP applications
    uint16_t port = 9; // Port for UDP communication

    // Node 4 as UDP server
    ns3::UdpServerHelper udpServer(port);
    ns3::ApplicationContainer serverApps = udpServer.Install(nodes.Get(4));
    serverApps.Start(ns3::Seconds(1.0)); // Start server slightly before client
    serverApps.Stop(ns3::Seconds(10.0));

    // Node 0 as UDP client, sending to Node 4
    ns3::UdpClientHelper udpClient(interfaces.GetAddress(4), port);
    udpClient.SetAttribute("MaxPackets", ns3::UintegerValue(1000)); // Max packets to send
    udpClient.SetAttribute("Interval", ns3::TimeValue(ns3::MilliSeconds(50))); // Send every 50ms
    udpClient.SetAttribute("PacketSize", ns3::UintegerValue(1024)); // 1KB packets

    ns3::ApplicationContainer clientApps = udpClient.Install(nodes.Get(0));
    clientApps.Start(ns3::Seconds(2.0)); // Application starts at 2 seconds
    clientApps.Stop(ns3::Seconds(10.0));

    // Simulation runs for 10 seconds
    ns3::Simulator::Stop(ns3::Seconds(10.0));
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}