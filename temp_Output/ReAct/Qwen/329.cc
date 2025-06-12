#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-header.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpThreeNodeSimulation");

int main(int argc, char *argv[]) {
    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(3);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices02 = pointToPoint.Install(nodes.Get(0), nodes.Get(2));
    NetDeviceContainer devices12 = pointToPoint.Install(nodes.Get(1), nodes.Get(2));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces02 = address.Assign(devices02);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces12 = address.Assign(devices12);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;

    // Server on Node 2
    TcpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(nodes.Get(2));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(10.0));

    // Client 0 (Node 0) sending to server at Node 2
    TcpClientHelper client0(interfaces02.GetAddress(1), port);
    client0.SetAttribute("MaxBytes", UintegerValue(0)); // infinite data
    ApplicationContainer clientApp0 = client0.Install(nodes.Get(0));
    clientApp0.Start(Seconds(2.0));
    clientApp0.Stop(Seconds(10.0));

    // Client 1 (Node 1) sending to server at Node 2
    TcpClientHelper client1(interfaces12.GetAddress(1), port);
    client1.SetAttribute("MaxBytes", UintegerValue(0)); // infinite data
    ApplicationContainer clientApp1 = client1.Install(nodes.Get(1));
    clientApp1.Start(Seconds(3.0));
    clientApp1.Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}