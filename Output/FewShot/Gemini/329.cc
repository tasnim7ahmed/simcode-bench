#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/error-model.h"

using namespace ns3;

int main(int argc, char *argv[]) {
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
    Ipv4InterfaceContainer interfaces12 = address.Assign(devices12);

    uint16_t port = 9;
    Address serverAddress(InetSocketAddress(interfaces02.GetAddress(1), port));

    TcpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(nodes.Get(2));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    TcpEchoClientHelper echoClient0(interfaces02.GetAddress(1), port);
    echoClient0.SetAttribute("MaxPackets", UintegerValue(100));
    echoClient0.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    echoClient0.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp0 = echoClient0.Install(nodes.Get(0));
    clientApp0.Start(Seconds(2.0));
    clientApp0.Stop(Seconds(10.0));

    TcpEchoClientHelper echoClient1(interfaces12.GetAddress(0), port);
    echoClient1.SetAttribute("MaxPackets", UintegerValue(100));
    echoClient1.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    echoClient1.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp1 = echoClient1.Install(nodes.Get(1));
    clientApp1.Start(Seconds(3.0));
    clientApp1.Stop(Seconds(10.0));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}