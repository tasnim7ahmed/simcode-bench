#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HybridNetworkSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create wired nodes
    NodeContainer wiredNodes;
    wiredNodes.Create(4);

    // Create wireless nodes
    NodeContainer wirelessNodes;
    wirelessNodes.Create(3);

    // Setup point-to-point links between wired nodes (chain topology)
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer wiredDevices[3];
    for (uint32_t i = 0; i < 3; ++i) {
        wiredDevices[i] = p2p.Install(wiredNodes.Get(i), wiredNodes.Get(i + 1));
    }

    // Setup Wi-Fi network for wireless nodes
    WifiMacHelper wifiMac;
    WifiPhyHelper wifiPhy;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    wifiMac.SetType("ns3::AdhocWifiMac");
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(channel.Create());

    NetDeviceContainer wirelessDevices = wifi.Install(wifiPhy, wifiMac, wirelessNodes);

    // Mobility for wireless nodes
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wirelessNodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(wiredNodes);
    stack.Install(wirelessNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    Ipv4InterfaceContainer wiredInterfaces[3];
    for (uint32_t i = 0; i < 3; ++i) {
        std::ostringstream subnet;
        subnet << "10.1." << i + 1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        wiredInterfaces[i] = address.Assign(wiredDevices[i]);
    }

    address.SetBase("10.2.1.0", "255.255.255.0");
    Ipv4InterfaceContainer wirelessInterfaces = address.Assign(wirelessDevices);

    // Set up routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Connect wireless gateway to wired network: wireless node 0 connects to wired node 0
    // Create bridge between wireless and wired network
    NodeContainer gatewayNodes = NodeContainer(wiredNodes.Get(0), wirelessNodes.Get(0));
    NetDeviceContainer gatewayDevices = p2p.Install(gatewayNodes);
    Ipv4InterfaceContainer gatewayInterfaces;
    address.SetBase("10.3.1.0", "255.255.255.0");
    gatewayInterfaces = address.Assign(gatewayDevices);

    // Enable pcap tracing
    wifiPhy.EnablePcap("wireless_node0", wirelessDevices.Get(0), true, true);
    p2p.EnablePcapAll("wired_link");

    // Set up TCP traffic from wireless nodes to wired nodes
    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(wiredInterfaces[2].GetAddress(1), sinkPort));

    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(wiredNodes.Get(3));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    OnOffHelper onoff("ns3::TcpSocketFactory", Address());
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer sourceApps;
    for (uint32_t i = 0; i < wirelessNodes.GetN(); ++i) {
        onoff.SetAttribute("Remote", AddressValue(sinkAddress));
        sourceApps.Add(onoff.Install(wirelessNodes.Get(i)));
    }
    sourceApps.Start(Seconds(1.0));
    sourceApps.Stop(Seconds(9.0));

    // Flow monitor setup
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> flowMonitor = flowmonHelper.InstallAll();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    flowMonitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    double totalThroughput = 0.0;
    uint32_t flowCount = 0;

    std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator iter = stats.begin(); iter != stats.end(); ++iter) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
        if (t.destinationPort == sinkPort) {
            double duration = iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds();
            double throughput = iter->second.rxBytes * 8.0 / duration / 1024 / 1024; // Mbps
            totalThroughput += throughput;
            flowCount++;
        }
    }

    if (flowCount > 0) {
        std::cout << "Average Throughput: " << (totalThroughput / flowCount) << " Mbps" << std::endl;
    }

    Simulator::Destroy();
    return 0;
}