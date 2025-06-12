#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpConnectionExample");

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("TcpConnectionExample", LOG_LEVEL_INFO);

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Set up Point-to-Point link between nodes
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));
    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Install Internet stack on nodes
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses to the devices
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Set up TCP server on Node 1
    uint16_t port = 8080;
    TcpServerHelper tcpServer(port);
    ApplicationContainer serverApp = tcpServer.Install(nodes.Get(1)); // Node 1 as server
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up TCP client on Node 0
    TcpClientHelper tcpClient(interfaces.GetAddress(1), port);
    tcpClient.SetAttribute("MaxPackets", UintegerValue(100));
    tcpClient.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
    tcpClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = tcpClient.Install(nodes.Get(0)); // Node 0 as client
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Set up FlowMonitor to track throughput
    FlowMonitorHelper flowMonitorHelper;
    Ptr<FlowMonitor> flowMonitor = flowMonitorHelper.InstallAll();

    // Run simulation
    Simulator::Run();

    // Display FlowMonitor results
    flowMonitor->CheckForLostPackets();
    flowMonitor->SerializeToXmlFile("tcp_connection_flowmonitor.xml", true, true);

    Simulator::Destroy();

    return 0;
}
