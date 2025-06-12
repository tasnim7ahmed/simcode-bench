#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VANET-Dsdv");

int main(int argc, char *argv[])
{
    uint32_t numVehicles = 10;
    double simulationTime = 60.0;
    double distance = 40.0; // meters apart between vehicles
    double roadLength = (numVehicles - 1) * distance + 1;
    double nodeSpeed = 20.0; // m/s (~72km/h)
    uint32_t packetSize = 512; // bytes
    uint32_t dataRate = 512; // bytes/s
    uint16_t port = 8080;

    CommandLine cmd(__FILE__);
    cmd.AddValue("numVehicles", "Number of vehicles in the VANET", numVehicles);
    cmd.AddValue("simulationTime", "Duration of simulation [s]", simulationTime);
    cmd.Parse(argc, argv);

    NodeContainer vehicles;
    vehicles.Create(numVehicles);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211p);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    NqosWaveMacHelper mac = NqosWaveMacHelper::Default();
    Wifi80211pHelper wifi80211p = Wifi80211pHelper::Default();
    wifi80211p.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                       "DataMode", StringValue("OfdmRate6MbpsBW10MHz"),
                                       "ControlMode", StringValue("OfdmRate6MbpsBW10MHz"));

    NetDeviceContainer devices = wifi80211p.Install(phy, mac, vehicles);

    // Mobility: vehicles along a straight road with set speed
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.SetPositionAllocator("ns3::LinePositionAllocator",
        "MinX", DoubleValue(0.0),
        "MinY", DoubleValue(0.0),
        "DeltaX", DoubleValue(distance),
        "DeltaY", DoubleValue(0.0),
        "GridWidth", UintegerValue(numVehicles),
        "LayoutType", StringValue("RowFirst"));
    mobility.Install(vehicles);
    for (uint32_t i = 0; i < vehicles.GetN(); ++i)
    {
        Ptr<ConstantVelocityMobilityModel> mob = vehicles.Get(i)->GetObject<ConstantVelocityMobilityModel>();
        mob->SetVelocity(Vector(nodeSpeed, 0.0, 0.0));
    }

    InternetStackHelper stack;
    DsdvHelper dsdv;
    stack.SetRoutingHelper(dsdv);
    stack.Install(vehicles);

    Ipv4AddressHelper address;
    address.SetBase("10.1.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Application: UDP traffic from car 0 to car N-1
    uint32_t srcIndex = 0;
    uint32_t dstIndex = numVehicles - 1;

    OnOffHelper onoff("ns3::UdpSocketFactory",
                      InetSocketAddress(interfaces.GetAddress(dstIndex), port));
    onoff.SetConstantRate(DataRate(dataRate * 8), packetSize);
    onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    onoff.SetAttribute("StopTime", TimeValue(Seconds(simulationTime)));

    ApplicationContainer senderApps = onoff.Install(vehicles.Get(srcIndex));

    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory",
                                InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = sinkHelper.Install(vehicles.Get(dstIndex));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(simulationTime));

    // Tracing and logging
    phy.EnablePcap("vanet-dsdv", devices);

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    uint64_t totalTxPackets = 0;
    uint64_t totalRxPackets = 0;
    double avgDelay = 0;

    for (auto const &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        if (t.sourceAddress == interfaces.GetAddress(srcIndex) &&
            t.destinationAddress == interfaces.GetAddress(dstIndex) &&
            t.destinationPort == port)
        {
            totalTxPackets += flow.second.txPackets;
            totalRxPackets += flow.second.rxPackets;
            if (flow.second.rxPackets > 0)
            {
                avgDelay += (flow.second.delaySum.GetSeconds() / flow.second.rxPackets);
            }
        }
    }

    std::cout << "VANET DSDV Simulation Results\n";
    std::cout << "Number of vehicles: " << numVehicles << "\n";
    std::cout << "Tx Packets: " << totalTxPackets << "\n";
    std::cout << "Rx Packets: " << totalRxPackets << "\n";
    std::cout << "Packet Delivery Ratio: "
              << (totalTxPackets > 0 ? double(totalRxPackets) / totalTxPackets * 100.0 : 0.0)
              << " %\n";
    if (totalRxPackets > 0)
        std::cout << "Average End-to-End Delay: " << avgDelay << " s\n";
    else
        std::cout << "Average End-to-End Delay: N/A\n";

    Simulator::Destroy();
    return 0;
}