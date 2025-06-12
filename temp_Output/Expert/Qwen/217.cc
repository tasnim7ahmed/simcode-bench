#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AodvManetSimulation");

int main(int argc, char *argv[]) {
    uint32_t numNodes = 5;
    double totalTime = 100.0;
    std::string phyMode("DsssRate1Mbps");
    double distance = 500; // m

    CommandLine cmd(__FILE__);
    cmd.AddValue("totalTime", "Total simulation time (seconds)", totalTime);
    cmd.AddValue("distance", "Distance between nodes (meters)", distance);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue(phyMode));

    NodeContainer nodes;
    nodes.Create(numNodes);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    YansWifiPhyHelper wifiPhy;
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(distance),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 1000, 0, 1000)));
    mobility.Install(nodes);

    AodvHelper aodv;
    Ipv4ListRoutingHelper listRouting;
    listRouting.Add(aodv, 10);

    InternetStackHelper internet;
    internet.SetRoutingHelper(listRouting);
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    ApplicationContainer apps;

    for (uint32_t i = 0; i < numNodes; ++i) {
        for (uint32_t j = 0; j < numNodes; ++j) {
            if (i != j) {
                uint16_t port = 9;
                InetSocketAddress sinkAddr(interfaces.GetAddress(j), port);
                OnOffHelper onoff(tid, sinkAddr);
                onoff.SetAttribute("DataRate", DataRateValue(DataRate("100kbps")));
                onoff.SetAttribute("PacketSize", UintegerValue(512));
                onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
                onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

                ApplicationContainer app = onoff.Install(nodes.Get(i));
                app.Start(Seconds(1.0));
                app.Stop(Seconds(totalTime - 1.0));
            }
        }
    }

    AsciiTraceHelper ascii;
    mobility.EnableAsciiAll(ascii.CreateFileStream("aodv-manet.mobility.tr"));
    wifiPhy.EnablePcapAll("aodv-manet");

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(totalTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
        std::cout << "Flow ID: " << iter->first << " Src Addr: " << t.sourceAddress << " Dst Addr: " << t.destinationAddress << std::endl;
        std::cout << "  Tx Packets: " << iter->second.txPackets << std::endl;
        std::cout << "  Rx Packets: " << iter->second.rxPackets << std::endl;
        std::cout << "  Lost Packets: " << iter->second.lostPackets << std::endl;
        std::cout << "  Throughput: " << iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds()) / 1000 << " Kbps" << std::endl;
    }

    Simulator::Destroy();
    return 0;
}