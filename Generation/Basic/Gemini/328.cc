#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

int main(int argc, char *argv[])
{
    // Set time resolution to nanoseconds
    ns3::Time::SetResolution(ns3::Time::NS);

    // Create two nodes for the Wi-Fi network
    ns3::NodeContainer nodes;
    nodes.Create(2);

    // Install mobility model (ConstantPositionMobilityModel)
    ns3::MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Set positions for the nodes
    ns3::Ptr<ns3::ConstantPositionMobilityModel> mobility0 = nodes.Get(0)->GetObject<ns3::ConstantPositionMobilityModel>();
    mobility0->SetPosition(ns3::Vector(0.0, 0.0, 0.0)); // Node 0 at (0,0,0)

    ns3::Ptr<ns3::ConstantPositionMobilityModel> mobility1 = nodes.Get(1)->GetObject<ns3::ConstantPositionMobilityModel>();
    mobility1->SetPosition(ns3::Vector(10.0, 0.0, 0.0)); // Node 1 at (10,0,0)

    // Configure Wi-Fi for 802.11b
    ns3::WifiHelper wifi;
    wifi.SetStandard(ns3::WIFI_STANDARD_80211b);

    ns3::YansWifiPhyHelper phy;
    // Create a YansWifiChannel with default propagation delay and loss models
    ns3::Ptr<ns3::YansWifiChannel> channel = ns3::CreateObject<ns3::YansWifiChannel>();
    channel->SetPropagationDelayModel(ns3::CreateObject<ns3::ConstantSpeedPropagationDelayModel>());
    channel->SetPropagationLossModel(ns3::CreateObject<ns3::FriisPropagationLossModel>());
    phy.SetChannel(channel);

    ns3::WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac"); // Ad-hoc mode (no Access Point)

    // Install Wi-Fi devices on nodes
    ns3::NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Install Internet stack (TCP/IP) on nodes
    ns3::InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to Wi-Fi devices
    ns3::Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Setup UDP applications: Node 0 (sender) to Node 1 (receiver)

    // Receiver (Node 1)
    uint16_t port = 9; // Discard port
    // Create socket address for the PacketSink (receiver)
    ns3::Address sinkAddress(ns3::InetSocketAddress(interfaces.GetAddress(1), port));
    ns3::PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory",
                                           ns3::InetSocketAddress(ns3::Ipv4Address::GetAny(), port));
    ns3::ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(1)); // Install on Node 1
    sinkApps.Start(ns3::Seconds(1.0)); // Application starts at 1 second
    sinkApps.Stop(ns3::Seconds(10.0)); // Application stops at 10 seconds

    // Sender (Node 0)
    // OnOffHelper to generate constant rate UDP traffic
    ns3::OnOffHelper onoff("ns3::UdpSocketFactory", ns3::Address()); // Remote address set later
    onoff.SetAttribute("OnTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=1]")); // Always On
    onoff.SetAttribute("OffTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=0]")); // Never Off
    onoff.SetAttribute("DataRate", ns3::DataRateValue(ns3::DataRate("500kb/s"))); // Constant rate of 500 kb/s
    onoff.SetAttribute("PacketSize", ns3::UintegerValue(1024)); // Packet size 1024 bytes

    // Set the remote address for the OnOff application to the PacketSink
    onoff.SetAttribute("Remote", ns3::AddressValue(sinkAddress));

    ns3::ApplicationContainer clientApps = onoff.Install(nodes.Get(0)); // Install on Node 0
    clientApps.Start(ns3::Seconds(1.0)); // Application starts at 1 second
    clientApps.Stop(ns3::Seconds(10.0)); // Application stops at 10 seconds

    // Set simulation duration
    ns3::Simulator::Stop(ns3::Seconds(10.0));

    // Run the simulation
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}