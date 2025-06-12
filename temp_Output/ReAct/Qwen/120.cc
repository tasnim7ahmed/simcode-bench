#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AodvGridSimulation");

int main(int argc, char *argv[]) {
    uint32_t numNodes = 10;
    double duration = 50.0;
    bool enableFlowMonitor = true;
    bool printRoutingTables = true;
    std::string pcapPrefix = "aodv_grid_simulation";
    std::string animFile = "aodv_grid_simulation.xml";

    CommandLine cmd;
    cmd.AddValue("duration", "Duration of simulation in seconds", duration);
    cmd.AddValue("printRoutingTables", "Print routing tables at intervals", printRoutingTables);
    cmd.AddValue("pcapPrefix", "PCAP file prefix", pcapPrefix);
    cmd.AddValue("enableFlowMonitor", "Enable Flow Monitor", enableFlowMonitor);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(numNodes);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiHelper wifi;
    wifi.SetRemoteStationManager("ns3::ArfRateControl");

    NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, mac, nodes);

    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv);
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
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

    for (uint32_t i = 0; i < numNodes; ++i) {
        UdpEchoServerHelper server(9);
        ApplicationContainer serverApp = server.Install(nodes.Get(i));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(duration));

        uint32_t nextNode = (i + 1) % numNodes;
        UdpEchoClientHelper client(interfaces.GetAddress(nextNode), 9);
        client.SetAttribute("MaxPackets", UintegerValue(1000));
        client.SetAttribute("Interval", TimeValue(Seconds(4.0)));
        client.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApp = client.Install(nodes.Get(i));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(duration));
    }

    if (printRoutingTables) {
        aodv.PrintRoutingTableAllAt(Seconds(10.0), nodes);
        aodv.PrintRoutingTableAllEvery(Seconds(10.0), nodes, Seconds(duration - 10.0));
    }

    wifiPhy.EnablePcapAll(pcapPrefix);

    AnimationInterface anim(animFile);
    for (uint32_t i = 0; i < numNodes; ++i) {
        anim.UpdateNodeDescription(nodes.Get(i), "Node-" + std::to_string(i));
        anim.UpdateNodeColor(nodes.Get(i), 255, 0, 0);
    }

    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    if (enableFlowMonitor) {
        flowMonitor = flowHelper.InstallAll();
    }

    Simulator::Stop(Seconds(duration));
    Simulator::Run();

    if (enableFlowMonitor) {
        flowMonitor->CheckForLostPackets();
        flowMonitor->SerializeToXmlFile("aodv_flow_monitor.xml", true, true);
    }

    Simulator::Destroy();
    return 0;
}