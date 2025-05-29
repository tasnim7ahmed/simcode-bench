#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VanetSimulation");

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponent::Enable("UdpClient", LOG_LEVEL_INFO);
    LogComponent::Enable("UdpServer", LOG_LEVEL_INFO);

    // Set simulation time
    double simulationTime = 10.0;

    // Set number of vehicles
    uint32_t nVehicles = 5;

    // Create nodes
    NodeContainer vehicles;
    vehicles.Create(nVehicles);

    // Configure channel
    YansWifiChannelHelper wifiChannelHelper = YansWifiChannelHelper::Default();
    YansWifiPhyHelper wifiPhyHelper = YansWifiPhyHelper::Default();
    wifiPhyHelper.SetChannel(wifiChannelHelper.Create());

    // Configure MAC layer
    NqosWaveMacHelper wifiMacHelper = NqosWaveMacHelper::Default();
    Wifi80211pHelper wifi80211pHelper = Wifi80211pHelper::Default();
    NetDeviceContainer devices = wifi80211pHelper.Install(wifiPhyHelper, wifiMacHelper, vehicles);

    // Configure mobility
    MobilityHelper mobilityHelper;
    mobilityHelper.SetPositionAllocator("ns3::GridPositionAllocator",
                                        "MinX", DoubleValue(0.0),
                                        "MinY", DoubleValue(0.0),
                                        "DeltaX", DoubleValue(20.0),
                                        "DeltaY", DoubleValue(0.0),
                                        "GridWidth", UintegerValue(nVehicles),
                                        "LayoutType", StringValue("RowFirst"));
    mobilityHelper.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobilityHelper.Install(vehicles);

    // Set initial velocities
    for (uint32_t i = 0; i < nVehicles; ++i) {
        Ptr<ConstantVelocityMobilityModel> mobility = vehicles.Get(i)->GetObject<ConstantVelocityMobilityModel>();
        mobility->SetVelocity(Vector(10.0 + i * 2, 0.0, 0.0)); // Varying speeds
    }

    // Install internet stack
    InternetStackHelper internet;
    internet.Install(vehicles);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Configure UDP server (on the first vehicle)
    UdpServerHelper server(9);
    ApplicationContainer serverApp = server.Install(vehicles.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(simulationTime));

    // Configure UDP client (on the other vehicles)
    UdpClientHelper client(interfaces.GetAddress(0), 9);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    for (uint32_t i = 1; i < nVehicles; ++i) {
        clientApps.Add(client.Install(vehicles.Get(i)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simulationTime));

    // Create flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Enable NetAnim visualization
    AnimationInterface anim("vanet_animation.xml");
    anim.SetMaxPktsPerTraceFile(10000000);

    // Run simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Analyze flow monitor data
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Packet Delivery Ratio: " << double(i->second.rxPackets) / double(i->second.txPackets) << "\n";
        std::cout << "  Delay Sum: " << i->second.delaySum << "\n";
        std::cout << "  Mean Delay: " << i->second.delaySum / i->second.rxPackets << "\n";
    }

    Simulator::Destroy();

    return 0;
}