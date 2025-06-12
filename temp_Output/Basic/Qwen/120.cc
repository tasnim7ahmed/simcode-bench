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

NS_LOG_COMPONENT_DEFINE("AODVAdHocNetwork");

int main(int argc, char *argv[]) {
    uint32_t numNodes = 10;
    double duration = 60.0;
    bool enableFlowMonitor = true;
    bool printRoutingTables = false;
    double routingTablePrintInterval = 10.0;
    std::string pcapFileName = "aodv_simulation";
    std::string flowMonitorOutputFile = "aodv_simulation.flowmon";
    std::string animFile = "aodv_simulation.xml";

    CommandLine cmd(__FILE__);
    cmd.AddValue("numNodes", "Number of nodes in the grid.", numNodes);
    cmd.AddValue("duration", "Duration of simulation in seconds", duration);
    cmd.AddValue("printRoutingTables", "Print routing tables at regular intervals", printRoutingTables);
    cmd.AddValue("routingTablePrintInterval", "Interval (seconds) for printing routing tables", routingTablePrintInterval);
    cmd.AddValue("pcapFileName", "Basename for PCAP files", pcapFileName);
    cmd.AddValue("enableFlowMonitor", "Enable Flow Monitor to collect stats", enableFlowMonitor);
    cmd.AddValue("flowMonitorOutputFile", "Filename for Flow Monitor XML output", flowMonitorOutputFile);
    cmd.AddValue("animFile", "Animation file name", animFile);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(numNodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(50.0),
                                  "DeltaY", DoubleValue(50.0),
                                  "GridWidth", UintegerValue(5),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 1000, 0, 1000)));
    mobility.Install(nodes);

    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv);
    internet.Install(nodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    for (uint32_t i = 0; i < numNodes - 1; ++i) {
        devices = p2p.Install(nodes.Get(i), nodes.Get(i + 1));
    }

    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> interfaces;
    for (uint32_t i = 0; i < numNodes - 1; ++i) {
        std::ostringstream subnet;
        subnet << "10.1." << i << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfaces.push_back(address.Assign(devices));
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    if (printRoutingTables) {
        Simulator::Schedule(Seconds(routingTablePrintInterval), &Ipv4GlobalRoutingHelper::PrintRoutingTableAllAt, Seconds(routingTablePrintInterval));
    }

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps;
    for (uint32_t i = 0; i < numNodes; ++i) {
        serverApps = echoServer.Install(nodes.Get(i));
        serverApps.Start(Seconds(1.0));
        serverApps.Stop(Seconds(duration));
    }

    for (uint32_t i = 0; i < numNodes; ++i) {
        uint32_t destNodeIndex = (i + 1) % numNodes;
        Ipv4Address destAddr = nodes.Get(destNodeIndex)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();
        UdpEchoClientHelper echoClient(destAddr, 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue((uint32_t)(duration / 4)));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(4.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApps = echoClient.Install(nodes.Get(i));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(duration));
    }

    if (!pcapFileName.empty()) {
        p2p.EnablePcapAll(pcapFileName);
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor;
    if (enableFlowMonitor) {
        monitor = flowmon.InstallAll();
    }

    if (!animFile.empty()) {
        AnimationInterface anim(animFile);
        anim.EnablePacketMetadata(true);
    }

    Simulator::Stop(Seconds(duration));
    Simulator::Run();

    if (enableFlowMonitor) {
        monitor->CheckForLostPackets();
        monitor->SerializeToXmlFile(flowMonitorOutputFile, false, false);
    }

    Simulator::Destroy();
    return 0;
}