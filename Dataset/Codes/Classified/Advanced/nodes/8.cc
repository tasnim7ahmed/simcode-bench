#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarTopologyExample");

int main(int argc, char *argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer starNodes;
    starNodes.Create(4); // Create 4 nodes in a star topology

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer device1, device2, device3;
    device1.Add(pointToPoint.Install(starNodes.Get(0), starNodes.Get(1)));
    device2.Add(pointToPoint.Install(starNodes.Get(0), starNodes.Get(2)));
    device3.Add(pointToPoint.Install(starNodes.Get(0), starNodes.Get(3)));

    InternetStackHelper stack;
    stack.Install(starNodes);

    Ipv4AddressHelper address1, address2, address3;
    address1.SetBase("192.9.39.0", "255.255.255.252");
    address2.SetBase("192.9.39.4", "255.255.255.252");
    address3.SetBase("192.9.39.8", "255.255.255.252");

    Ipv4InterfaceContainer interface1, interface2, interface3;
    interface1 = address1.Assign(device1);
    interface2 = address2.Assign(device2);
    interface3 = address3.Assign(device3);

    UdpEchoServerHelper echoServer(9);

    // Server 0, Client 1
    ApplicationContainer serverApps01 = echoServer.Install(starNodes.Get(0));
    serverApps01.Start(Seconds(1.0));
    serverApps01.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient01(interface1.GetAddress(0), 9);
    echoClient01.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient01.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient01.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp01 = echoClient01.Install(starNodes.Get(1));
    clientApp01.Start(Seconds(2.0));
    clientApp01.Stop(Seconds(10.0));

    // Server 1, Client 0
    ApplicationContainer serverApps10 = echoServer.Install(starNodes.Get(1));
    serverApps10.Start(Seconds(11.0));
    serverApps10.Stop(Seconds(20.0));

    UdpEchoClientHelper echoClient10(interface1.GetAddress(1), 9);
    echoClient10.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient10.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient10.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp10 = echoClient10.Install(starNodes.Get(0));
    clientApp10.Start(Seconds(12.0));
    clientApp10.Stop(Seconds(20.0));

    // Server 0, Client 2
    ApplicationContainer serverApps02 = echoServer.Install(starNodes.Get(0));
    serverApps02.Start(Seconds(21.0));
    serverApps02.Stop(Seconds(30.0));

    UdpEchoClientHelper echoClient02(interface2.GetAddress(0), 9);
    echoClient02.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient02.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient02.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp02 = echoClient02.Install(starNodes.Get(2));
    clientApp02.Start(Seconds(22.0));
    clientApp02.Stop(Seconds(30.0));

    // Server 2, Client 0
    ApplicationContainer serverApps20 = echoServer.Install(starNodes.Get(2));
    serverApps20.Start(Seconds(31.0));
    serverApps20.Stop(Seconds(40.0));

    UdpEchoClientHelper echoClient20(interface2.GetAddress(1), 9);
    echoClient20.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient20.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient20.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp20 = echoClient20.Install(starNodes.Get(0));
    clientApp20.Start(Seconds(32.0));
    clientApp20.Stop(Seconds(40.0));

    // Server 0, Client 3
    ApplicationContainer serverApps03 = echoServer.Install(starNodes.Get(0));
    serverApps03.Start(Seconds(41.0));
    serverApps03.Stop(Seconds(50.0));

    UdpEchoClientHelper echoClient03(interface3.GetAddress(0), 9);
    echoClient03.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient03.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient03.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp03 = echoClient03.Install(starNodes.Get(3));
    clientApp03.Start(Seconds(42.0));
    clientApp03.Stop(Seconds(50.0));

    // Server 3, Client 0
    ApplicationContainer serverApps30 = echoServer.Install(starNodes.Get(3));
    serverApps30.Start(Seconds(51.0));
    serverApps30.Stop(Seconds(60.0));

    UdpEchoClientHelper echoClient30(interface3.GetAddress(1), 9);
    echoClient30.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient30.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient30.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp30 = echoClient30.Install(starNodes.Get(0));
    clientApp30.Start(Seconds(52.0));
    clientApp30.Stop(Seconds(60.0));

    AnimationInterface anim("StarTopologyExample.xml");

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

