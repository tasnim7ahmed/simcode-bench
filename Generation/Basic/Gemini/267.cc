#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"

int
main()
{
    ns3::Time::SetResolution(ns3::Time::NS);

    // Create nodes
    ns3::NodeContainer nodes;
    nodes.Create(3);

    // Set up mobility (linear layout with 10-meter separation)
    ns3::MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", ns3::DoubleValue(0.0),
                                  "MinY", ns3::DoubleValue(0.0),
                                  "DeltaX", ns3::DoubleValue(10.0),
                                  "DeltaY", ns3::DoubleValue(0.0),
                                  "GridWidth", ns3::UintegerValue(3),
                                  "LayoutType", ns3::StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Set up Wi-Fi (802.11b ad-hoc)
    ns3::WifiHelper wifi;
    wifi.SetStandard(ns3::WIFI_PHY_STANDARD_80211b);

    ns3::WifiPhyHelper wifiPhy;
    wifiPhy.Set("TxPowerStart", ns3::DoubleValue(16.02)); // 16.02 dBm = 40 mW
    wifiPhy.Set("TxPowerEnd", ns3::DoubleValue(16.02));
    wifiPhy.Set("TxPowerLevels", ns3::UintegerValue(1));
    wifiPhy.Set("TxGain", ns3::DoubleValue(0));
    wifiPhy.Set("RxGain", ns3::DoubleValue(0));
    wifiPhy.Set("EnergyDetectionThreshold", ns3::DoubleValue(-96));
    wifiPhy.Set("CcaMode1Threshold", ns3::DoubleValue(-99));

    ns3::YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationLossModel("ns3::FriisPropagationLossModel");
    wifiChannel.SetPcapDataLinkType(ns3::WifiPhyHelper::DLT_IEEE802_11_RADIO);
    wifiPhy.SetChannel(wifiChannel.Create());

    ns3::WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac", "QosSupported", ns3::BooleanValue(false));

    ns3::NetDeviceContainer wifiDevices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Set up AODV routing
    ns3::AodvHelper aodv;
    ns3::InternetStackHelper internetStack;
    internetStack.SetRoutingHelper(aodv); // Set AODV as the routing protocol
    internetStack.Install(nodes);

    // Assign IP addresses (192.168.1.0/24 subnet)
    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = ipv4.Assign(wifiDevices);

    // Setup UDP Echo Server on the last node (Node 2)
    ns3::UdpEchoServerHelper echoServer(9); // Port 9
    ns3::ApplicationContainer serverApps = echoServer.Install(nodes.Get(2));
    serverApps.Start(ns3::Seconds(1.0));
    serverApps.Stop(ns3::Seconds(10.0));

    // Setup UDP Echo Client on the first node (Node 0)
    ns3::UdpEchoClientHelper echoClient(interfaces.GetAddress(2), 9);
    echoClient.SetAttribute("MaxPackets", ns3::UintegerValue(5));
    echoClient.SetAttribute("PacketSize", ns3::UintegerValue(512));
    echoClient.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(1.0))); // Send one packet per second
    ns3::ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(ns3::Seconds(2.0)); // Start client after server is up
    clientApps.Stop(ns3::Seconds(10.0));

    // Run simulation
    ns3::Simulator::Stop(ns3::Seconds(10.0));
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}