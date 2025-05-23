#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/animation-interface.h"

int main(int argc, char *argv[])
{
    // 1. Simulation Parameters
    double simulationTime = 10.0; // seconds
    uint32_t packetSize = 1024;   // bytes
    double udpRate = 100000;      // bits/s (100 kbps)
    uint32_t numPackets = 1000000; // Effectively continuous transmission

    // Command line arguments for customization
    ns3::CommandLine cmd(__FILE__);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("packetSize", "UDP packet size in bytes", packetSize);
    cmd.AddValue("udpRate", "UDP data rate in bits/s", udpRate);
    cmd.Parse(argc, argv);

    // Basic validation for simulation parameters
    if (packetSize <= 0) {
        packetSize = 512;
    }
    if (udpRate <= 0) {
        udpRate = 100000;
    }
    if (simulationTime <= 0) {
        simulationTime = 10.0;
    }

    // 2. Create Nodes
    ns3::NodeContainer apNodes;
    apNodes.Create(2); // Two APs
    ns3::NodeContainer staNodes;
    staNodes.Create(4); // Four STAs

    // 3. Mobility Model (ConstantPositionMobilityModel)
    // Place APs and STAs to ensure "nearest" connection by design
    ns3::MobilityHelper mobility;
    ns3::Ptr<ns3::ListPositionAllocator> positionAlloc = ns3::CreateObject<ns3::ListPositionAllocator>();

    // APs positions
    positionAlloc->Add(ns3::Vector(0.0, 0.0, 0.0));   // AP1
    positionAlloc->Add(ns3::Vector(100.0, 0.0, 0.0)); // AP2

    // STAs positions (near respective APs)
    positionAlloc->Add(ns3::Vector(5.0, 0.0, 0.0));  // STA0 (near AP1)
    positionAlloc->Add(ns3::Vector(10.0, 0.0, 0.0)); // STA1 (near AP1)
    positionAlloc->Add(ns3::Vector(90.0, 0.0, 0.0)); // STA2 (near AP2)
    positionAlloc->Add(ns3::Vector(95.0, 0.0, 0.0)); // STA3 (near AP2)

    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNodes);
    mobility.Install(staNodes);

    // 4. Wi-Fi Configuration
    ns3::WifiHelper wifi;
    wifi.SetStandard(ns3::WIFI_STANDARD_80211n); // Use 802.11n standard

    ns3::YansWifiPhyHelper phy;
    phy.SetChannel(ns3::CreateObject<ns3::YansWifiChannel>());
    // Set propagation loss and delay models
    phy.GetChannel()->SetPropagationLossModel(ns3::CreateObject<ns3::LogDistancePropagationLossModel>());
    phy.GetChannel()->SetPropagationDelayModel(ns3::CreateObject<ns3::ConstantSpeedPropagationDelayModel>());
    phy.Set("TxPowerStart", ns3::DoubleValue(5.0)); // dBm
    phy.Set("TxPowerEnd", ns3::DoubleValue(5.0));   // dBm

    // AP MAC configuration
    ns3::WifiMacHelper apMac;
    ns3::Ssid ssidAp1 = ns3::Ssid("ssid-ap1");
    ns3::Ssid ssidAp2 = ns3::Ssid("ssid-ap2");

    apMac.SetType("ns3::ApWifiMac",
                  "Ssid", ns3::SsidValue(ssidAp1),
                  "BeaconInterval", ns3::TimeValue(ns3::MicroSeconds(102400))); // Default beacon interval

    ns3::NetDeviceContainer apDevices;
    apDevices.Add(wifi.Install(phy, apMac, apNodes.Get(0))); // AP1

    apMac.SetType("ns3::ApWifiMac",
                  "Ssid", ns3::SsidValue(ssidAp2),
                  "BeaconInterval", ns3::TimeValue(ns3::MicroSeconds(102400)));
    apDevices.Add(wifi.Install(phy, apMac, apNodes.Get(1))); // AP2

    // STA MAC configuration for AP1 (STA0, STA1)
    ns3::WifiMacHelper staMacAp1;
    staMacAp1.SetType("ns3::NonAssociatedWifiMac",
                      "Ssid", ns3::SsidValue(ssidAp1),
                      "ActiveProbing", ns3::BooleanValue(false)); // Don't actively probe, just use provided SSID

    ns3::NetDeviceContainer staDevicesAp1;
    staDevicesAp1.Add(wifi.Install(phy, staMacAp1, staNodes.Get(0))); // STA0
    staDevicesAp1.Add(wifi.Install(phy, staMacAp1, staNodes.Get(1))); // STA1

    // STA MAC configuration for AP2 (STA2, STA3)
    ns3::WifiMacHelper staMacAp2;
    staMacAp2.SetType("ns3::NonAssociatedWifiMac",
                      "Ssid", ns3::SsidValue(ssidAp2),
                      "ActiveProbing", ns3::BooleanValue(false));

    ns3::NetDeviceContainer staDevicesAp2;
    staDevicesAp2.Add(wifi.Install(phy, staMacAp2, staNodes.Get(2))); // STA2
    staDevicesAp2.Add(wifi.Install(phy, staMacAp2, staNodes.Get(3))); // STA3

    // Consolidate all STA devices for IP addressing
    ns3::NetDeviceContainer allStaDevices;
    allStaDevices.Add(staDevicesAp1);
    allStaDevices.Add(staDevicesAp2);

    // 5. Install Internet Stack
    ns3::InternetStackHelper stack;
    stack.Install(apNodes);
    stack.Install(staNodes);

    // 6. Assign IP Addresses
    ns3::Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer apInterfaces = address.Assign(apDevices);
    ns3::Ipv4InterfaceContainer staInterfaces = address.Assign(allStaDevices);

    // 7. Configure UDP Applications for traffic exchange

    // Flow 1: STA0 (client) -> STA1 (server)
    // Server on STA1 (node index 1 in staNodes, IP index 1 in staInterfaces)
    uint16_t port1 = 9; // Common echo port
    ns3::PacketSinkHelper sinkHelper1("ns3::UdpSocketFactory",
                                      ns3::InetSocketAddress(ns3::Ipv4Address::GetAny(), port1));
    ns3::ApplicationContainer serverApps1 = sinkHelper1.Install(staNodes.Get(1)); // STA1
    serverApps1.Start(ns3::Seconds(1.0));
    serverApps1.Stop(ns3::Seconds(simulationTime));

    // Client on STA0 (node index 0 in staNodes, IP index 0 in staInterfaces)
    ns3::UdpClientHelper clientHelper1(staInterfaces.GetAddress(1), port1); // Target STA1's IP
    clientHelper1.SetAttribute("MaxPackets", ns3::UintegerValue(numPackets));
    clientHelper1.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(packetSize * 8.0 / udpRate))); // Packet rate
    clientHelper1.SetAttribute("PacketSize", ns3::UintegerValue(packetSize));
    ns3::ApplicationContainer clientApps1 = clientHelper1.Install(staNodes.Get(0)); // STA0
    clientApps1.Start(ns3::Seconds(1.0));
    clientApps1.Stop(ns3::Seconds(simulationTime));

    // Flow 2: STA2 (client) -> STA3 (server)
    // Server on STA3 (node index 3 in staNodes, IP index 3 in staInterfaces)
    uint16_t port2 = 10;
    ns3::PacketSinkHelper sinkHelper2("ns3::UdpSocketFactory",
                                      ns3::InetSocketAddress(ns3::Ipv4Address::GetAny(), port2));
    ns3::ApplicationContainer serverApps2 = sinkHelper2.Install(staNodes.Get(3)); // STA3
    serverApps2.Start(ns3::Seconds(1.0));
    serverApps2.Stop(ns3::Seconds(simulationTime));

    // Client on STA2 (node index 2 in staNodes, IP index 2 in staInterfaces)
    ns3::UdpClientHelper clientHelper2(staInterfaces.GetAddress(3), port2); // Target STA3's IP
    clientHelper2.SetAttribute("MaxPackets", ns3::UintegerValue(numPackets));
    clientHelper2.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(packetSize * 8.0 / udpRate)));
    clientHelper2.SetAttribute("PacketSize", ns3::UintegerValue(packetSize));
    ns3::ApplicationContainer clientApps2 = clientHelper2.Install(staNodes.Get(2)); // STA2
    clientApps2.Start(ns3::Seconds(1.0));
    clientApps2.Stop(ns3::Seconds(simulationTime));

    // 8. Enable NetAnim Visualization
    ns3::AnimationInterface anim("wifi-netanim.xml");

    // Explicitly set constant positions for NetAnim for clarity (though MobilityHelper already does it)
    anim.SetConstantPosition(apNodes.Get(0), 0.0, 0.0);
    anim.SetConstantPosition(apNodes.Get(1), 100.0, 0.0);
    anim.SetConstantPosition(staNodes.Get(0), 5.0, 0.0);
    anim.SetConstantPosition(staNodes.Get(1), 10.0, 0.0);
    anim.SetConstantPosition(staNodes.Get(2), 90.0, 0.0);
    anim.SetConstantPosition(staNodes.Get(3), 95.0, 0.0);

    // Set node descriptions for NetAnim
    anim.UpdateNodeDescription(apNodes.Get(0), "AP1");
    anim.UpdateNodeDescription(apNodes.Get(1), "AP2");
    anim.UpdateNodeDescription(staNodes.Get(0), "STA0");
    anim.UpdateNodeDescription(staNodes.Get(1), "STA1");
    anim.UpdateNodeDescription(staNodes.Get(2), "STA2");
    anim.UpdateNodeDescription(staNodes.Get(3), "STA3");

    // Set node colors and shapes for better visualization
    anim.SetNodeColor(apNodes.Get(0), 255, 0, 0); // Red for AP1
    anim.SetNodeColor(apNodes.Get(1), 0, 0, 255); // Blue for AP2
    anim.SetNodeColor(staNodes.Get(0), 0, 255, 0); // Green for STA0
    anim.SetNodeColor(staNodes.Get(1), 0, 255, 0); // Green for STA1
    anim.SetNodeColor(staNodes.Get(2), 255, 255, 0); // Yellow for STA2
    anim.SetNodeColor(staNodes.Get(3), 255, 255, 0); // Yellow for STA3

    anim.SetNodeShape(apNodes.Get(0), ns3::AnimationInterface::SHAPE_SQUARE);
    anim.SetNodeShape(apNodes.Get(1), ns3::AnimationInterface::SHAPE_SQUARE);
    anim.SetNodeShape(staNodes.Get(0), ns3::AnimationInterface::SHAPE_CIRCLE);
    anim.SetNodeShape(staNodes.Get(1), ns3::AnimationInterface::SHAPE_CIRCLE);
    anim.SetNodeShape(staNodes.Get(2), ns3::AnimationInterface::SHAPE_CIRCLE);
    anim.SetNodeShape(staNodes.Get(3), ns3::AnimationInterface::SHAPE_CIRCLE);

    // 9. Run Simulation
    ns3::Simulator::Stop(ns3::Seconds(simulationTime));
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}