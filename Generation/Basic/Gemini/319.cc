#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

// NS_LOG_COMPONENT_DEFINE ("WifiUdpApplication"); // No log needed as per instructions

int main (int argc, char *argv[])
{
    // Configure default attributes
    ns3::Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", ns3::StringValue ("2200"));
    ns3::Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", ns3::StringValue ("2200"));
    ns3::Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode", ns3::StringValue ("DsssRate1Mbps"));

    // Create nodes
    ns3::NodeContainer apNode;
    apNode.Create (1);
    ns3::NodeContainer staNodes;
    staNodes.Create (3);

    // Set up mobility
    ns3::MobilityHelper mobility;
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (apNode);
    mobility.Install (staNodes);

    // Position AP at (0,0,0)
    ns3::Ptr<ns3::ConstantPositionMobilityModel> apMobility = apNode.Get (0)->GetObject<ns3::ConstantPositionMobilityModel> ();
    apMobility->SetPosition (ns3::Vector (0.0, 0.0, 0.0));

    // Position STAs around AP
    ns3::Ptr<ns3::ConstantPositionMobilityModel> staMobility0 = staNodes.Get (0)->GetObject<ns3::ConstantPositionMobilityModel> ();
    staMobility0->SetPosition (ns3::Vector (10.0, 0.0, 0.0));
    ns3::Ptr<ns3::ConstantPositionMobilityModel> staMobility1 = staNodes.Get (1)->GetObject<ns3::ConstantPositionMobilityModel> ();
    staMobility1->SetPosition (ns3::Vector (0.0, 10.0, 0.0));
    ns3::Ptr<ns3::ConstantPositionMobilityModel> staMobility2 = staNodes.Get (2)->GetObject<ns3::ConstantPositionMobilityModel> ();
    staMobility2->SetPosition (ns3::Vector (-10.0, 0.0, 0.0));

    // Create channel and PHY
    ns3::YansWifiChannelHelper channel = ns3::YansWifiChannelHelper::Default ();
    ns3::Ptr<ns3::YansWifiChannel> wifiChannel = channel.Create ();
    ns3::YansWifiPhyHelper phy;
    phy.SetChannel (wifiChannel);

    // Configure MAC for AP and STAs
    ns3::WifiHelper wifi;
    wifi.SetStandard (ns3::WIFI_STANDARD_80211n);
    wifi.SetRemoteStationManager ("ns3::AarfWifiManager");

    ns3::WifiMacHelper mac;
    ns3::Ssid ssid = ns3::Ssid ("ns3-wifi");

    // AP MAC
    mac.SetType ("ns3::ApWifiMac",
                 "Ssid", ns3::SsidValue (ssid),
                 "BeaconInterval", ns3::TimeValue (ns3::Seconds (0.1)));
    ns3::NetDeviceContainer apDevice;
    apDevice = wifi.Install (phy, mac, apNode);

    // STA MACs
    mac.SetType ("ns3::StaWifiMac",
                 "Ssid", ns3::SsidValue (ssid),
                 "ActiveProbing", ns3::BooleanValue (false));
    ns3::NetDeviceContainer staDevices;
    staDevices = wifi.Install (phy, mac, staNodes);

    // Install internet stack
    ns3::InternetStackHelper stack;
    stack.Install (apNode);
    stack.Install (staNodes);

    // Assign IP addresses
    ns3::Ipv4AddressHelper address;
    address.SetBase ("10.1.3.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer apInterfaces = address.Assign (apDevice);
    ns3::Ipv4InterfaceContainer staInterfaces = address.Assign (staDevices);

    // Set up UDP server (PacketSink) on AP
    uint16_t port = 9;
    ns3::PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory",
                                            ns3::InetSocketAddress (ns3::Ipv4Address::GetAny (), port));
    ns3::ApplicationContainer serverApps = packetSinkHelper.Install (apNode.Get (0));
    serverApps.Start (ns3::Seconds (1.0));
    serverApps.Stop (ns3::Seconds (10.0));

    // Set up UDP clients on STAs
    ns3::Ipv4Address apIpAddress = apInterfaces.GetAddress (0);
    ns3::UdpClientHelper udpClientHelper (apIpAddress, port);
    udpClientHelper.SetAttribute ("MaxPackets", ns3::UintegerValue (1000));
    udpClientHelper.SetAttribute ("Interval", ns3::TimeValue (ns3::Seconds (0.01)));
    udpClientHelper.SetAttribute ("PacketSize", ns3::UintegerValue (1024));

    ns3::ApplicationContainer clientApps = udpClientHelper.Install (staNodes);
    clientApps.Start (ns3::Seconds (2.0));
    clientApps.Stop (ns3::Seconds (12.0)); // Allow clients to send all packets if simulation permits

    // Run simulation
    ns3::Simulator::Stop (ns3::Seconds (10.0));
    ns3::Simulator::Run ();
    ns3::Simulator::Destroy ();

    return 0;
}