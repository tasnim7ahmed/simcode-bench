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
    bool enablePcap = true;
    uint32_t numNodes = 10;
    double simulationTime = 30.0;
    double areaSize = 500.0;

    CommandLine cmd;
    cmd.AddValue("enablePcap", "Enable PCAP tracing", enablePcap);
    cmd.AddValue("numNodes", "Number of nodes", numNodes);
    cmd.AddValue("simulationTime", "Simulation time", simulationTime);
    cmd.AddValue("areaSize", "Area size", areaSize);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(numNodes);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
    wifiPhy.SetChannel(wifiChannel.Create());

    NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default();
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    AodvHelper aodv;
    InternetStackHelper stack;
    stack.SetRoutingProtocol(aodv);
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(areaSize) + "]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(areaSize) + "]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                               "Bounds", RectangleValue(Rectangle(0, areaSize, 0, areaSize)),
                               "Speed", StringValue("ns3::ConstantRandomVariable[Constant=10]"));
    mobility.Install(nodes);

    uint16_t port = 9;
    for (uint32_t i = 0; i < numNodes; ++i) {
        for (uint32_t j = 0; j < numNodes; ++j) {
            if (i != j) {
                V4PingHelper ping(interfaces.GetAddress(j));
                ping.SetAttribute("Verbose", BooleanValue(false));
                ping.SetAttribute("Interval", TimeValue(Seconds(0.5)));
                ping.SetAttribute("StartTime", TimeValue(Seconds(1.0 + (double)rand() / RAND_MAX * 5.0)));
                ping.SetAttribute("StopTime", TimeValue(Seconds(simulationTime - 1.0)));
                ApplicationContainer pingApp = ping.Install(nodes.Get(i));
            }
        }
    }


    if (enablePcap) {
        wifiPhy.EnablePcapAll("adhoc_aodv");
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    double totalPacketsSent = 0;
    double totalPacketsReceived = 0;
    double totalDelay = 0;
    int numFlows = 0;

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        totalPacketsSent += i->second.txPackets;
        totalPacketsReceived += i->second.rxPackets;
        totalDelay += i->second.delaySum.GetSeconds();
        numFlows++;
    }

    double packetDeliveryRatio = (totalPacketsSent > 0) ? (totalPacketsReceived / totalPacketsSent) : 0;
    double averageEndToEndDelay = (totalPacketsReceived > 0) ? (totalDelay / totalPacketsReceived) : 0;

    std::cout << "Packet Delivery Ratio: " << packetDeliveryRatio << std::endl;
    std::cout << "Average End-to-End Delay: " << averageEndToEndDelay << " seconds" << std::endl;

    Simulator::Destroy();
    return 0;
}