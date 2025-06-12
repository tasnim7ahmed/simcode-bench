#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdHocAodvSimulation");

int main(int argc, char *argv[]) {
    uint32_t numNodes = 10;
    double duration = 60.0;
    double echoInterval = 4.0;
    bool enableFlowMonitor = true;
    bool printRoutingTables = false;
    double routingTablePrintInterval = 10.0;
    std::string pcapFileName = "aodv_simulation.pcap";
    std::string flowMonitorFileName = "aodv_simulation.flow";

    CommandLine cmd;
    cmd.AddValue("duration", "Simulation duration in seconds", duration);
    cmd.AddValue("echoInterval", "Echo request interval (seconds)", echoInterval);
    cmd.AddValue("enableFlowMonitor", "Enable Flow Monitor", enableFlowMonitor);
    cmd.AddValue("printRoutingTables", "Print routing tables at intervals", printRoutingTables);
    cmd.AddValue("routingTablePrintInterval", "Interval for printing routing tables", routingTablePrintInterval);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(numNodes);

    YansWifiPhyHelper wifiPhy;
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiHelper wifi;
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, mac, nodes);

    AodvHelper aodv;
    Ipv4ListRoutingHelper list;
    list.Add(aodv, 100);

    InternetStackHelper internet;
    internet.SetRoutingHelper(list);
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(20.0),
                                  "DeltaY", DoubleValue(20.0),
                                  "GridWidth", UintegerValue(5),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
    mobility.Install(nodes);

    AnimationInterface anim("aodv_simulation.xml");
    anim.EnablePacketMetadata(true);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps;
    for (uint32_t i = 0; i < numNodes; ++i) {
        serverApps.Add(echoServer.Install(nodes.Get(i)));
    }
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(duration));

    for (uint32_t i = 0; i < numNodes; ++i) {
        uint32_t j = (i + 1) % numNodes;
        Ipv4Address destAddr = interfaces.GetAddress(j, 0);
        UdpEchoClientHelper echoClient(destAddr, 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(echoInterval)));
        echoClient.SetAttribute("PacketSize", UintegerValue(512));
        ApplicationContainer clientApps = echoClient.Install(nodes.Get(i));
        clientApps.Start(Seconds(1.0));
        clientApps.Stop(Seconds(duration - 0.1));
    }

    if (printRoutingTables) {
        Ptr<OutputStreamWrapper> routingStream = CreateFileStreamWrapper("aodv_routing_tables.txt", std::ios::out);
        aodv.PrintRoutingTableAllAt(Seconds(routingTablePrintInterval), routingStream);
    }

    wifiPhy.EnablePcap(pcapFileName, devices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    if (enableFlowMonitor) {
        FlowMonitorHelper flowmonHelper;
        Ptr<FlowMonitor> flowmon = flowmonHelper.InstallAll();
        flowmon->SetStartTime(Seconds(0));
        flowmon->SetEndTime(Seconds(duration));
        Simulator::Stop(Seconds(duration));
        Simulator::Run();
        flowmon->SerializeToXmlFile(flowMonitorFileName, false, false);
    } else {
        Simulator::Stop(Seconds(duration));
        Simulator::Run();
    }

    Simulator::Destroy();
    return 0;
}