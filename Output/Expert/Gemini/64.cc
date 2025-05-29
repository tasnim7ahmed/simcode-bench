#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpStarServer");

int main(int argc, char* argv[]) {
    CommandLine cmd;
    int nNodes = 9;
    uint32_t packetSize = 1472;
    std::string dataRate = "10Mbps";

    cmd.AddValue("nNodes", "Number of nodes in the star topology", nNodes);
    cmd.AddValue("packetSize", "Packet size for CBR traffic", packetSize);
    cmd.AddValue("dataRate", "Data rate for CBR traffic", dataRate);
    cmd.Parse(argc, argv);

    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(nNodes);

    NodeContainer hubNode = NodeContainer(nodes.Get(0));
    NodeContainer armNodes;

    for (int i = 1; i < nNodes; ++i) {
        armNodes.Add(nodes.Get(i));
    }

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer hubDevices;
    NetDeviceContainer armDevices;

    for (int i = 1; i < nNodes; ++i) {
        NetDeviceContainer link = pointToPoint.Install(hubNode.Get(0), armNodes.Get(i - 1));
        hubDevices.Add(link.Get(0));
        armDevices.Add(link.Get(1));
    }

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer hubInterfaces;
    Ipv4InterfaceContainer armInterfaces;

    for (uint32_t i = 0; i < armDevices.GetN(); ++i) {
        NetDeviceContainer devices;
        devices.Add(hubDevices.Get(i));
        devices.Add(armDevices.Get(i));
        Ipv4InterfaceContainer interfaces = address.Assign(devices);
        hubInterfaces.Add(interfaces.Get(0));
        armInterfaces.Add(interfaces.Get(1));
        address.NewNetwork();
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 50000;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(hubNode);
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    for (int i = 1; i < nNodes; ++i) {
        UdpClientHelper client(armInterfaces.GetAddress(i - 1), port);
        client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
        client.SetAttribute("Interval", TimeValue(Time("0.00001s")));
        client.SetAttribute("PacketSize", UintegerValue(packetSize));

        ApplicationContainer clientApp = client.Install(armNodes.Get(i - 1));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));
    }

    Simulator::Stop(Seconds(10.0));

    AsciiTraceHelper ascii;
    pointToPoint.EnableAsciiAll(ascii.CreateFileStream("tcp-star-server.tr"));

    for (uint32_t i = 0; i < armDevices.GetN(); ++i) {
        pointToPoint.EnablePcap("tcp-star-server-" + std::to_string(i + 1) + "-" + std::to_string(0), hubDevices.Get(i), false);
        pointToPoint.EnablePcap("tcp-star-server-" + std::to_string(i + 1) + "-" + std::to_string(1), armDevices.Get(i), false);
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::ostream& os = std::cout;
    monitor->SerializeToXmlFile("tcp-star-server.flowmon", true, true);

    Simulator::Destroy();
    return 0;
}