#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-server.h"
#include "ns3/tcp-client.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpSimpleExample");

int main(int argc, char *argv[])
{
    // Enable logging for TCP client and server
    LogComponentEnable("TcpClient", LOG_LEVEL_INFO);
    LogComponentEnable("TcpServer", LOG_LEVEL_INFO);

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Setup point-to-point link with 10Mbps data rate and 5ms delay
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", DataRateValue(DataRate("10Mbps")));
    pointToPoint.SetChannelAttribute("Delay", TimeValue(MilliSeconds(5)));

    // Install network devices on the nodes
    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Install Internet stack on all nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses to the devices
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up TCP server on Node 2 (port 50000)
    TcpServerHelper server(50000);
    ApplicationContainer serverApps = server.Install(nodes.Get(1));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    // Set up TCP client on Node 1 connecting to Node 2's IP and port 50000
    TcpClientHelper client(interfaces.GetAddress(1), 50000);
    client.SetAttribute("MaxBytes", UintegerValue(0)); // Send continuously until stopped

    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    // Start simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}