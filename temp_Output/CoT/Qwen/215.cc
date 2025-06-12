#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ManetAodvExample");

int main(int argc, char *argv[]) {
    uint32_t totalNodes = 4;
    double simulationTime = 60.0;
    double distance = 500.0; // meters
    std::string phyMode("DsssRate1Mbps");
    double txp = 5; // dBm
    uint32_t packetSize = 1024;
    uint32_t numPackets = 1000;
    Time interPacketInterval = MilliSeconds(100);

    CommandLine cmd(__FILE__);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("distance", "Distance between nodes in meters", distance);
    cmd.AddValue("txp", "Transmit power in dBm", txp);
    cmd.AddValue("phyMode", "Phy mode", phyMode);
    cmd.AddValue("packetSize", "size of application packet sent", packetSize);
    cmd.AddValue("numPackets", "number of packets generated", numPackets);
    cmd.AddValue("interval", "interval (ms) between packets", interPacketInterval);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue("2200"));
    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("2200"));
    Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue(phyMode));

    NodeContainer nodes;
    nodes.Create(totalNodes);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue(phyMode), "ControlMode", StringValue(phyMode));
    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    MobilityHelper mobility;
    ObjectFactory positionAllocator;
    positionAllocator.SetTypeId("ns3::RandomRectanglePositionAllocator");
    positionAllocator.Set("X", StringValue("ns3::UniformRandomVariable[Min=0.0,Max=300.0]"));
    positionAllocator.Set("Y", StringValue("ns3::UniformRandomVariable[Min=0.0,Max=300.0]"));

    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"),
                              "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.2]"),
                              "PositionAllocator", PointerValue(positionAllocator.Create()));
    mobility.Install(nodes);

    AodvHelper aodv;
    Ipv4ListRoutingHelper list;
    list.Add(aodv, 100);
    InternetStackHelper internet;
    internet.SetRoutingHelper(list);
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(numPackets));
    echoClient.SetAttribute("Interval", TimeValue(interPacketInterval));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simulationTime));

    AsciiTraceHelper ascii;
    wifiPhy.EnableAsciiAll(ascii.CreateFileStream("manet-aodv.tr"));
    wifiPhy.EnablePcapAll("manet-aodv");

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        if (t.sourceAddress == interfaces.GetAddress(0) && t.destinationAddress == interfaces.GetAddress(1)) {
            std::cout << "Flow ID: " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")" << std::endl;
            std::cout << "  Tx Packets: " << i->second.txPackets << std::endl;
            std::cout << "  Rx Packets: " << i->second.rxPackets << std::endl;
            std::cout << "  Lost Packets: " << i->second.lostPackets << std::endl;
            std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / simulationTime / 1000 / 1000 << " Mbps" << std::endl;
        }
    }

    Simulator::Destroy();
    return 0;
}