#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/trace-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpEchoSimulation");

int main(int argc, char *argv[]) {
    bool useIpv6 = false;
    CommandLine cmd(__FILE__);
    cmd.AddValue("useIpv6", "Use IPv6 if true, IPv4 otherwise", useIpv6);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(4);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices[6];
    devices[0] = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
    devices[1] = pointToPoint.Install(nodes.Get(1), nodes.Get(2));
    devices[2] = pointToPoint.Install(nodes.Get(1), nodes.Get(3));

    QueueBaseHelper queue("DropTailQueue", "MaxSize", StringValue("100p"));
    for (int i = 0; i < 3; ++i) {
        devices[i].Get(0)->SetQueue(queue.Create<Queue<Packet>>());
        devices[i].Get(1)->SetQueue(queue.Create<Queue<Packet>>());
    }

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces[6];
    address.SetBase("10.1.1.0", "255.255.255.0");
    interfaces[0] = address.Assign(devices[0]);
    address.NewNetwork();
    interfaces[1] = address.Assign(devices[1]);
    address.NewNetwork();
    interfaces[2] = address.Assign(devices[2]);

    if (useIpv6) {
        Ipv6AddressHelper address6;
        address6.SetBase("2001:db8::", Ipv6Prefix(64));
        interfaces[3] = address6.Assign(devices[0]);
        address6.NewNetwork();
        interfaces[4] = address6.Assign(devices[1]);
        address6.NewNetwork();
        interfaces[5] = address6.Assign(devices[2]);

        Ipv6GlobalRoutingHelper::PopulateRoutingTables();
    } else {
        Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    }

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    uint32_t packetSize = 1024;
    uint32_t maxPackets = 5;
    Time interval = Seconds(1.0);

    UdpEchoClientHelper echoClient;
    if (useIpv6) {
        echoClient = UdpEchoClientHelper(interfaces[3].GetAddress(1, 1), 9);
    } else {
        echoClient = UdpEchoClientHelper(interfaces[0].Get(1).GetAddress(), 9);
    }
    echoClient.SetAttribute("MaxPackets", UintegerValue(maxPackets));
    echoClient.SetAttribute("Interval", TimeValue(interval));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> queueStream = asciiTraceHelper.CreateFileStream("udp-echo.tr");
    pointToPoint.EnableQueueTraces(queueStream);
    pointToPoint.EnablePcapAll("udp-echo");

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}