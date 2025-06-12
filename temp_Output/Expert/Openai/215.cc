#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/aodv-module.h"
#include "ns3/wifi-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    uint32_t numNodes = 4;
    double simTime = 30.0;
    double distance = 100.0;
    std::string phyMode("DsssRate11Mbps");

    NodeContainer nodes;
    nodes.Create(numNodes);

    // WiFi configuration
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());
    wifiPhy.Set("TxPowerStart", DoubleValue(16.0));
    wifiPhy.Set("TxPowerEnd", DoubleValue(16.0));

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Mobility: RandomWaypoint within 200x200m box
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                             "Speed", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=5.0]"),
                             "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.5]"),
                             "PositionAllocator", PointerValue(
                                    CreateObjectWithAttributes<RandomRectanglePositionAllocator>(
                                        "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=200.0]"),
                                        "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=200.0]")
                                    )
                                )
                             );
    mobility.Install(nodes);

    // Internet stack and AODV
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv);
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // UDP: Node 0 (client) --> Node 3 (server)
    uint16_t port = 4000;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApps = udpServer.Install(nodes.Get(3));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simTime - 1));

    UdpClientHelper udpClient(interfaces.GetAddress(3), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(10000));
    udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(50))); // 20 pkts/sec
    udpClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApps = udpClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simTime - 1));

    // Enable pcap tracing
    wifiPhy.EnablePcapAll("manet-aodv");

    // FlowMonitor for throughput, loss
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // Throughput & Packet Loss statistics
    double totalReceived = 0;
    double totalLost = 0;
    double totalTxBytes = 0;
    double totalRxBytes = 0;

    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (auto const &flow : stats) {
        Ipv4FlowClassifier::FiveTuple t = flowmon.GetClassifier()->FindFlow(flow.first);
        if (t.destinationPort == port) {
            totalTxBytes += flow.second.txBytes;
            totalRxBytes += flow.second.rxBytes;
            totalReceived += flow.second.rxPackets;
            totalLost += (flow.second.txPackets - flow.second.rxPackets);
            double throughput = (flow.second.rxBytes * 8.0) / (simTime - 2.0 - 1.0) / 1e3; // in Kbps
            std::cout << "Flow " << flow.first << " (" << t.sourceAddress << " --> " << t.destinationAddress << ")\n";
            std::cout << "  TxPackets: " << flow.second.txPackets << ", RxPackets: " << flow.second.rxPackets << "\n";
            std::cout << "  Lost Packets: " << flow.second.lostPackets << "\n";
            std::cout << "  Throughput: " << throughput << " Kbps\n";
        }
    }
    std::cout << "Total Rx Packets: " << totalReceived << "\n";
    std::cout << "Total Lost Packets: " << totalLost << "\n";
    std::cout << "Overall Throughput: " << (totalRxBytes * 8.0) / (simTime - 2.0 - 1.0) / 1e3 << " Kbps\n";

    Simulator::Destroy();
    return 0;
}