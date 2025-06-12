#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    uint32_t numVehicles = 10;
    double simulationTime = 30.0;
    double distance = 50.0;
    double nodeSpeed = 20.0; // meters/second

    CommandLine cmd;
    cmd.AddValue("numVehicles", "Number of vehicles", numVehicles);
    cmd.AddValue("simulationTime", "Duration of Simulation (s)", simulationTime);
    cmd.AddValue("distance", "Spacing between vehicles (m)", distance);
    cmd.AddValue("nodeSpeed", "Vehicle speed (m/s)", nodeSpeed);
    cmd.Parse(argc, argv);

    NodeContainer vehicles;
    vehicles.Create(numVehicles);

    // WiFi configuration
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    WifiMacHelper mac;
    Ssid ssid = Ssid("vanet-ssid");
    mac.SetType("ns3::AdhocWifiMac", "Ssid", SsidValue(ssid));

    NetDeviceContainer devices = wifi.Install(phy, mac, vehicles);

    // Mobility: vehicles on straight road, constant velocity
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    for (uint32_t i = 0; i < numVehicles; ++i)
    {
        positionAlloc->Add(Vector(i * distance, 0.0, 0.0));
    }
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(vehicles);

    // Assign initial speeds
    for (uint32_t i = 0; i < vehicles.GetN(); ++i)
    {
        Ptr<ConstantVelocityMobilityModel> cvmm = vehicles.Get(i)->GetObject<ConstantVelocityMobilityModel>();
        cvmm->SetVelocity(Vector(nodeSpeed, 0.0, 0.0));
    }

    // Internet stack with DSDV
    DsdvHelper dsdv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(dsdv);
    internet.Install(vehicles);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Application: UDP traffic from first to last vehicle
    uint16_t port = 5000;
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(numVehicles - 1), port));
    onoff.SetAttribute("DataRate", StringValue("1Mbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(512));
    onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    onoff.SetAttribute("StopTime", TimeValue(Seconds(simulationTime)));

    ApplicationContainer senderApp = onoff.Install(vehicles.Get(0));

    // UDP packet sink at the last vehicle
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(vehicles.Get(numVehicles - 1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simulationTime + 1.0));

    // Enable flow monitor to track performance
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Enable PCAP tracing
    phy.EnablePcapAll("vanet-dsdv");

    Simulator::Stop(Seconds(simulationTime + 1.0));
    Simulator::Run();

    // Print flow monitor statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    for (const auto &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        std::cout << "Flow from " << t.sourceAddress << " to " << t.destinationAddress << std::endl;
        std::cout << "Tx Packets: " << flow.second.txPackets << std::endl;
        std::cout << "Rx Packets: " << flow.second.rxPackets << std::endl;
        std::cout << "Throughput: "
                  << (flow.second.rxBytes * 8.0 / (simulationTime * 1000000.0)) << " Mbps" << std::endl;
    }

    Simulator::Destroy();
    return 0;
}