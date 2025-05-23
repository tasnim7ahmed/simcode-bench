#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/aodv-helper.h"
#include "ns3/netanim-module.h"

int main(int argc, char *argv[])
{
    // Set default parameters for simulation
    ns3::Time::SetResolution(ns3::Time::NS);
    ns3::LogComponentEnable("UdpEchoClientApplication", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("UdpEchoServerApplication", ns3::LOG_LEVEL_INFO);
    // Log AODV routing protocol activities (optional, for debugging AODV)
    ns3::LogComponentEnable("ns3::aodv::RoutingProtocol", ns3::LOG_LEVEL_DEBUG);

    // 1. Create 3 nodes for the ad-hoc network
    ns3::NodeContainer nodes;
    nodes.Create(3);

    // 2. Set up mobility for the nodes
    // Using ConstantPositionMobilityModel to keep nodes static for a stable topology
    ns3::MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Position the nodes strategically to force Node 1 to act as a relay
    // Node 0 (Source): (0,0,0)
    // Node 1 (Relay): (50,0,0)
    // Node 2 (Destination): (100,0,0)
    // With a WiFi range of 60 meters, Node 0 can reach Node 1, Node 1 can reach Node 2,
    // but Node 0 cannot directly reach Node 2 (distance 100m > 60m range).
    nodes.Get(0)->GetObject<ns3::ConstantPositionMobilityModel>()->SetPosition(ns3::Vector(0.0, 0.0, 0.0));
    nodes.Get(1)->GetObject<ns3::ConstantPositionMobilityModel>()->SetPosition(ns3::Vector(50.0, 0.0, 0.0));
    nodes.Get(2)->GetObject<ns3::ConstantPositionMobilityModel>()->SetPosition(ns3::Vector(100.0, 0.0, 0.0));

    // 3. Configure the WiFi channel and physical layer
    ns3::WifiHelper wifi;
    wifi.SetStandard(ns3::WIFI_STANDARD_80211n); // Using 802.11n standard
    wifi.SetRemoteStationManager("ns3::AarfWifiManager"); // Automatic Rate Fallback

    ns3::YansWifiPhyHelper wifiPhy;
    // Set up a channel with a RangePropagationLossModel to clearly define link connectivity
    ns3::YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationLossModel("ns3::RangePropagationLossModel", "Range", ns3::DoubleValue(60.0)); // Max range of 60 meters
    wifiChannel.SetPropagationDelayModel("ns3::ConstantSpeedPropagationDelayModel");
    wifiPhy.SetChannel(wifiChannel.Create());

    // 4. Configure the MAC layer for ad-hoc mode
    ns3::WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac"); // Ad-hoc mode (no Access Point)

    // Install WiFi devices on all nodes
    ns3::NetDeviceContainer devices;
    devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // 5. Install AODV routing protocol on all nodes
    ns3::AodvHelper aodv;
    ns3::InternetStackHelper internet;
    internet.SetRoutingHelper(aodv); // Set AODV as the routing protocol
    internet.Install(nodes);

    // 6. Assign IP addresses to the network devices
    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0"); // Network 10.1.1.0/24
    ns3::Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // 7. Set up UDP communication applications
    // A UDP Echo Server on Node 2 (destination)
    ns3::UdpEchoServerHelper echoServer(9); // Listen on port 9
    ns3::ApplicationContainer serverApps = echoServer.Install(nodes.Get(2));
    serverApps.Start(ns3::Seconds(1.0));  // Start server at 1 second
    serverApps.Stop(ns3::Seconds(10.0)); // Stop server at 10 seconds

    // A UDP Echo Client on Node 0 (source), sending to Node 2
    ns3::UdpEchoClientHelper echoClient(interfaces.GetAddress(2), 9); // Send to Node 2's IP, port 9
    echoClient.SetAttribute("MaxPackets", ns3::UintegerValue(10));    // Send 10 packets
    echoClient.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(1.0))); // Send every 1 second
    echoClient.SetAttribute("PacketSize", ns3::UintegerValue(1024)); // Packet size 1024 bytes
    ns3::ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(ns3::Seconds(2.0)); // Start client at 2 seconds (after server is up)
    clientApps.Stop(ns3::Seconds(9.0));  // Stop client at 9 seconds

    // 8. Enable NetAnim visualization
    ns3::NetAnimHelper netanim;
    // Trace all events to a NetAnim XML file
    netanim.TraceAll("aodv-ad-hoc-netanim.xml");

    // 9. Run the simulation
    ns3::Simulator::Stop(ns3::Seconds(11.0)); // Stop simulation slightly after applications finish
    ns3::Simulator::Run();
    ns3::Simulator::Destroy(); // Clean up simulation resources

    return 0;
}