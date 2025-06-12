#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdHocAodvSimulation");

int main(int argc, char *argv[]) {
    uint32_t numNodes = 10;
    double areaSize = 500.0;
    double simDuration = 30.0;

    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(numNodes);

    YansWifiPhyHelper wifiPhy;
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiHelper wifi;
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    AodvHelper aodv;
    InternetStackHelper stack;
    stack.SetRoutingHelper(aodv);
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, areaSize, 0, areaSize)),
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"));
    mobility.Install(nodes);

    uint16_t port = 9;
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("100kbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer apps;

    for (uint32_t i = 0; i < numNodes; ++i) {
        uint32_t j = i;
        while (j == i) {
            j = rand() % numNodes;
        }
        Ipv4Address destAddr = interfaces.GetAddress(j, 0);
        onoff.SetAttribute("Remote", AddressValue(InetSocketAddress(destAddr, port)));
        apps = onoff.Install(nodes.Get(i));
        double startTime = Seconds(Seconds(1).GetValue() * (rand() / (double)RAND_MAX) * simDuration);
        apps.Start(Seconds(startTime));
        apps.Stop(Seconds(simDuration - 0.001));
    }

    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("aodv-wireless.tr");
    wifiPhy.EnableAsciiAll(stream);
    wifiPhy.EnablePcapAll("aodv-wireless");

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simDuration));
    Simulator::Run();

    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    double totalRx = 0;
    double totalTx = 0;
    Time totalDelay = Seconds(0);

    for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
        FlowId id = iter->first;
        FlowMonitor::FlowStats s = iter->second;
        totalRx += s.rxPackets;
        totalTx += s.txPackets;
        if (s.rxPackets > 0) {
            totalDelay += s.delaySum;
        }
    }

    double pdr = (totalTx > 0) ? (totalRx / totalTx) : 0.0;
    double avgDelay = (totalRx > 0) ? (totalDelay.GetSeconds() / totalRx) : 0.0;

    std::cout << "Packet Delivery Ratio: " << pdr << std::endl;
    std::cout << "Average End-to-End Delay: " << avgDelay << " seconds" << std::endl;

    Simulator::Destroy();
    return 0;
}