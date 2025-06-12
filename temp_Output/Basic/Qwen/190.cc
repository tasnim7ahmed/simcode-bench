#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/wifi-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ManetUdpSimulation");

int main(int argc, char *argv[]) {
    uint32_t numNodes = 3;
    double totalTime = 10.0;
    bool enableFlowMonitor = true;
    bool verbose = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("numNodes", "Number of nodes in the network", numNodes);
    cmd.AddValue("totalTime", "Total simulation time (seconds)", totalTime);
    cmd.AddValue("verbose", "Enable/disable verbose output", verbose);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("ManetUdpSimulation", LOG_LEVEL_INFO);
    }

    NodeContainer nodes;
    nodes.Create(numNodes);

    WifiMacHelper wifiMac;
    WifiPhyHelper wifiPhy;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    wifiMac.SetType("ns3::AdhocWifiMac");
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(channel.Create());

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomDirection2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)),
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=2]"),
                              "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.5]"));
    mobility.Install(nodes);

    InternetStackHelper stack;
    OlsrHelper olsr;
    Ipv4ListRoutingHelper list;
    list.Add(olsr, 10);
    stack.SetRoutingHelper(list);
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 9;
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinks = sinkHelper.Install(nodes.Get(0));
    sinks.Start(Seconds(0.0));
    sinks.Stop(Seconds(totalTime));

    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(0), port));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("100kbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer apps = onoff.Install(nodes.Get(1));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(totalTime));

    AsciiTraceHelper ascii;
    wifiPhy.EnableAsciiAll(ascii.CreateFileStream("manet-udp.tr"));
    wifiPhy.EnablePcapAll("manet-udp");

    AnimationInterface anim("manet-udp.xml");
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        anim.UpdateNodeDescription(nodes.Get(i), "Node-" + std::to_string(i));
        anim.UpdateNodeColor(nodes.Get(i), 255 - (i * 85), i * 85, 0);
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor;
    if (enableFlowMonitor) {
        monitor = flowmon.InstallAll();
    }

    Simulator::Stop(Seconds(totalTime));
    Simulator::Run();

    if (enableFlowMonitor) {
        flowmon.SerializeToXmlFile("manet-udp.flowmon", false, false);
    }

    Simulator::Destroy();

    return 0;
}