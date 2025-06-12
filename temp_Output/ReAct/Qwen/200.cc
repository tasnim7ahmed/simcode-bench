#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AodvWirelessSimulation");

int main(int argc, char *argv[]) {
    uint32_t numNodes = 4;
    double simulationTime = 10.0;
    bool enableFlowMonitor = true;
    std::string phyMode("DsssRate1Mbps");

    CommandLine cmd(__FILE__);
    cmd.AddValue("numNodes", "Number of nodes in the network.", numNodes);
    cmd.AddValue("simulationTime", "Total simulation time (seconds).", simulationTime);
    cmd.AddValue("enableFlowMonitor", "Enable FlowMonitor to analyze performance metrics.", enableFlowMonitor);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue(phyMode));

    NodeContainer nodes;
    nodes.Create(numNodes);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue(phyMode), "ControlMode", StringValue(phyMode));

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
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobility.Install(nodes);

    PacketSinkHelper packetSink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9));
    ApplicationContainer sinkApps;
    for (uint32_t i = 0; i < numNodes; ++i) {
        sinkApps.Add(packetSink.Install(nodes.Get(i)));
    }
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(simulationTime));

    OnOffHelper onoff("ns3::UdpSocketFactory", Address());
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("100kbps")));

    ApplicationContainer sourceApps;
    for (uint32_t i = 0; i < numNodes; ++i) {
        for (uint32_t j = 0; j < numNodes; ++j) {
            if (i != j) {
                AddressValue remoteAddress(InetSocketAddress(interfaces.GetAddress(j), 9));
                onoff.SetAttribute("Remote", remoteAddress);
                sourceApps.Add(onoff.Install(nodes.Get(i)));
            }
        }
    }
    sourceApps.Start(Seconds(1.0));
    sourceApps.Stop(Seconds(simulationTime - 0.1));

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("aodv-wireless.tr");
    wifiPhy.EnableAsciiAll(stream);

    AnimationInterface anim("aodv-wireless.xml");
    anim.EnablePacketMetadata(true);
    anim.EnableIpv4RouteTracking("aodv-wireless-route.xml", Seconds(0), Seconds(simulationTime), Seconds(0.5));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowMonitorHelper;
    if (enableFlowMonitor) {
        flowMonitor = flowMonitorHelper.InstallAll();
    }

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    if (enableFlowMonitor) {
        flowMonitor->SerializeToXmlFile("aodv-wireless-flow.xml", true, true);
    }

    Simulator::Destroy();
    return 0;
}