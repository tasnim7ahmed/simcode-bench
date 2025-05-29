#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdhocAodvSimulation");

int main(int argc, char *argv[]) {
    // Set simulation parameters
    uint32_t numNodes = 10;
    double simTime = 30.0;
    double areaSize = 500.0;

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Configure Wifi Ad-hoc network
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Set mobility model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                 "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"),
                                 "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"));
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                             "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=20.0]"),
                             "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.5]"),
                             "PositionAllocator", PointerValue(mobility.GetPositionAllocator()));
    mobility.Install(nodes);

    // Install internet stack and AODV
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv);
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Install UDP traffic: For example, half as source, half as sink
    uint16_t port = 4000;
    ApplicationContainer appContainer;
    Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();

    for (uint32_t i = 0; i < numNodes / 2; ++i) {
        // Sink
        UdpServerHelper server(port + i);
        ApplicationContainer serverApp = server.Install(nodes.Get(i));
        serverApp.Start(Seconds(0.0));
        serverApp.Stop(Seconds(simTime));

        // Source
        uint32_t dstIndex = i;
        do {
            dstIndex = uv->GetInteger(numNodes / 2, numNodes - 1);
        } while (dstIndex == i);

        UdpClientHelper client(interfaces.GetAddress(dstIndex), port + i);
        client.SetAttribute("MaxPackets", UintegerValue(1000));
        client.SetAttribute("Interval", TimeValue(Seconds(0.05)));
        client.SetAttribute("PacketSize", UintegerValue(512));
        ApplicationContainer clientApp = client.Install(nodes.Get(i));
        double startTime = uv->GetValue(1.0, 5.0);
        clientApp.Start(Seconds(startTime));
        clientApp.Stop(Seconds(simTime));
        appContainer.Add(serverApp);
        appContainer.Add(clientApp);
    }

    // Enable tracing
    wifiPhy.EnablePcapAll("aodv-adhoc");

    // Install Flow Monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // Calculate PDR and average delay
    double totalReceived = 0;
    double totalSent = 0;
    double totalDelay = 0;
    uint32_t deliveredPackets = 0;

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
        totalSent += iter->second.txPackets;
        totalReceived += iter->second.rxPackets;
        deliveredPackets += iter->second.rxPackets;
        if (iter->second.rxPackets > 0) {
            totalDelay += iter->second.delaySum.GetSeconds();
        }
    }

    double pdr = (totalSent > 0) ? (totalReceived / totalSent) * 100.0 : 0.0;
    double avgDelay = (deliveredPackets > 0) ? (totalDelay / deliveredPackets) : 0.0;

    std::cout << "------ Simulation Results -----\n";
    std::cout << "Total Sent Packets: " << totalSent << std::endl;
    std::cout << "Total Received Packets: " << totalReceived << std::endl;
    std::cout << "Packet Delivery Ratio (PDR): " << pdr << " %" << std::endl;
    std::cout << "Average End-to-End Delay: " << avgDelay << " s" << std::endl;

    Simulator::Destroy();
    return 0;
}