#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    // Set up logging if desired
    // LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // 1. Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // 2. Configure the point-to-point link
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // 3. Install network devices
    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // 4. Stack: Internet
    InternetStackHelper stack;
    stack.Install(nodes);

    // 5. Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // 6. Create and setup the server application
    uint16_t port = 8080;
    Address serverAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", serverAddress);
    ApplicationContainer serverApps = packetSinkHelper.Install(nodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // 7. Create and setup the client application
    BulkSendHelper clientHelper("ns3::TcpSocketFactory",
                                InetSocketAddress(interfaces.GetAddress(0), port));
    clientHelper.SetAttribute("MaxBytes", UintegerValue(1000000));
    ApplicationContainer clientApps = clientHelper.Install(nodes.Get(1));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // 8. Routing (populate routing tables)
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // 9. Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}