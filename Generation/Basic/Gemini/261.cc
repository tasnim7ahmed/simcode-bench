#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/constant-velocity-mobility-model.h"

int main(int argc, char *argv[])
{
    // 1. Create three nodes (vehicles)
    ns3::NodeContainer nodes;
    nodes.Create(3);

    // 2. Configure ConstantVelocityMobilityModel for each vehicle
    ns3::MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(nodes);

    // Set initial positions and velocities for each vehicle
    // Vehicle 0: (0,0,0) at 20 m/s along X-axis
    ns3::Ptr<ns3::ConstantVelocityMobilityModel> mobilityNode0 = nodes.Get(0)->GetObject<ns3::ConstantVelocityMobilityModel>();
    mobilityNode0->SetPosition(ns3::Vector(0.0, 0.0, 0.0));
    mobilityNode0->SetVelocity(ns3::Vector(20.0, 0.0, 0.0));

    // Vehicle 1: (50,0,0) at 15 m/s along X-axis
    ns3::Ptr<ns3::ConstantVelocityMobilityModel> mobilityNode1 = nodes.Get(1)->GetObject<ns3::ConstantVelocityMobilityModel>();
    mobilityNode1->SetPosition(ns3::Vector(50.0, 0.0, 0.0));
    mobilityNode1->SetVelocity(ns3::Vector(15.0, 0.0, 0.0));

    // Vehicle 2: (100,0,0) at 25 m/s along X-axis
    ns3::Ptr<ns3::ConstantVelocityMobilityModel> mobilityNode2 = nodes.Get(2)->GetObject<ns3::ConstantVelocityMobilityModel>();
    mobilityNode2->SetPosition(ns3::Vector(100.0, 0.0, 0.0));
    mobilityNode2->SetVelocity(ns3::Vector(25.0, 0.0, 0.0));

    // 3. Configure IEEE 802.11p (WAVE) Wi-Fi
    ns3::YansWifiPhyHelper wifiPhy;
    ns3::YansWifiChannelHelper wifiChannel = ns3::YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());
    
    // Set 802.11p standard
    ns3::WifiHelper wifi80211p;
    wifi80211p.SetStandard(ns3::WIFI_PHY_STANDARD_80211p);

    ns3::NqosWifiMacHelper wifiMac = ns3::NqosWifiMacHelper::Default();
    wifiMac.SetType("ns3::AdhocWifiMac"); // Ad-hoc mode for VANET

    // Use a constant rate manager for 802.11p data rates (e.g., OfdmRate6Mbps)
    wifi80211p.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                       "DataMode", ns3::StringValue("OfdmRate6Mbps"),
                                       "ControlMode", ns3::StringValue("OfdmRate6Mbps"));

    ns3::NetDeviceContainer devices = wifi80211p.Install(wifiPhy, wifiMac, nodes);

    // 4. Install Internet Stack on all nodes
    ns3::InternetStackHelper internet;
    internet.Install(nodes);

    // 5. Assign IPv4 addresses in the 10.1.1.0/24 subnet
    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // 6. Setup UDP Echo Server on the last vehicle (Node 2)
    uint16_t port = 9; // UDP Echo port
    ns3::UdpEchoServerHelper echoServer(port);
    ns3::ApplicationContainer serverApps = echoServer.Install(nodes.Get(2));
    serverApps.Start(ns3::Seconds(1.0)); // Server starts at 1 second
    serverApps.Stop(ns3::Seconds(10.0)); // Server stops at simulation end

    // 7. Setup UDP Echo Client on the first vehicle (Node 0)
    ns3::Ipv4Address serverAddress = interfaces.GetAddress(2); // IP address of Node 2
    ns3::UdpEchoClientHelper echoClient(serverAddress, port);
    echoClient.SetAttribute("MaxPackets", ns3::UintegerValue(5));      // Send 5 packets
    echoClient.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(1.0))); // Send 1 packet per second
    echoClient.SetAttribute("PacketSize", ns3::UintegerValue(1024));   // Packet size of 1024 bytes

    ns3::ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(ns3::Seconds(2.0)); // Client starts at 2 seconds (after server is up)
    clientApps.Stop(ns3::Seconds(10.0)); // Client stops at simulation end

    // 8. Set simulation duration
    ns3::Simulator::Stop(ns3::Seconds(10.0));

    // 9. Run simulation
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}