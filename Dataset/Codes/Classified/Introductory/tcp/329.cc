#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpSimpleExample");

int main(int argc, char *argv[])
{
    // Enable log components
    LogComponentEnable("TcpSimpleExample", LOG_LEVEL_INFO);

    // Create nodes for the network
    NodeContainer nodes;
    nodes.Create(3); // Three nodes: Two clients and one server

    // Set up point-to-point link between nodes
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes.Get(0), nodes.Get(1)); // Connection between node 0 and 1
    pointToPoint.Install(nodes.Get(1), nodes.Get(2));           // Connection between node 1 and 2

    // Install the internet stack (TCP/IP stack)
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to the devices
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces;
    interfaces = ipv4.Assign(devices);

    // Set up the server application (Node 2)
    uint16_t port = 9; // Server port
    TcpServerHelper tcpServer(port);
    ApplicationContainer serverApp = tcpServer.Install(nodes.Get(2)); // Install on Node 2 (Server)
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up the client application (Node 0 and Node 1)
    BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(2), port));
    bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(0));              // Unlimited data transfer
    ApplicationContainer clientApp0 = bulkSendHelper.Install(nodes.Get(0)); // Install on Node 0 (Client 1)
    ApplicationContainer clientApp1 = bulkSendHelper.Install(nodes.Get(1)); // Install on Node 1 (Client 2)

    clientApp0.Start(Seconds(2.0));
    clientApp0.Stop(Seconds(10.0));
    clientApp1.Start(Seconds(3.0));
    clientApp1.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
