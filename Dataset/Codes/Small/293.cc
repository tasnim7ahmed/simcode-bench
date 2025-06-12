#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpConnectionExample");

int main(int argc, char *argv[])
{
    // Set default values for command line arguments
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("TcpConnectionExample", LOG_LEVEL_INFO);

    // Create the nodes
    NodeContainer nodes;
    nodes.Create(2); // Create two nodes

    // Set up point-to-point link between the two nodes
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));
    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Install the Internet stack on the nodes
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses to the devices
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Set up the TCP server on the second node
    uint16_t port = 8080;
    TcpServerHelper tcpServer(port);
    ApplicationContainer serverApp = tcpServer.Install(nodes.Get(1)); // Second node as server
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up the TCP client on the first node
    TcpClientHelper tcpClient(interfaces.GetAddress(1), port); // Connecting to the second node
    tcpClient.SetAttribute("MaxPackets", UintegerValue(100));
    tcpClient.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
    tcpClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = tcpClient.Install(nodes.Get(0)); // First node as client
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
