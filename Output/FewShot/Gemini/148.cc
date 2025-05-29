#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    NodeContainer nodes;
    nodes.Create(4);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices01 = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devices02 = pointToPoint.Install(nodes.Get(0), nodes.Get(2));
    NetDeviceContainer devices03 = pointToPoint.Install(nodes.Get(0), nodes.Get(3));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces01 = address.Assign(devices01);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces02 = address.Assign(devices02);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces03 = address.Assign(devices03);
    
    uint16_t port = 9;

    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(nodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient1(interfaces01.GetAddress(0), port);
    echoClient1.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient1.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient1.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp1 = echoClient1.Install(nodes.Get(1));
    clientApp1.Start(Seconds(2.0));
    clientApp1.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient2(interfaces02.GetAddress(0), port);
    echoClient2.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp2 = echoClient2.Install(nodes.Get(2));
    clientApp2.Start(Seconds(3.0));
    clientApp2.Stop(Seconds(10.0));
    
    UdpEchoClientHelper echoClient3(interfaces03.GetAddress(0), port);
    echoClient3.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient3.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient3.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp3 = echoClient3.Install(nodes.Get(3));
    clientApp3.Start(Seconds(4.0));
    clientApp3.Stop(Seconds(10.0));

    pointToPoint.EnablePcapAll("branch");

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}