#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpEchoLanSimulation");

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

    TrafficControlHelper tch;
    tch.SetRootQueueDisc("ns3::DropTailQueueDisc");
    QueueDiscContainer qdiscs = tch.Install(nodes.Get(0)->GetDevice(0));

    NetDeviceContainer devices[6];
    devices[0] = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
    devices[1] = pointToPoint.Install(nodes.Get(0), nodes.Get(2));
    devices[2] = pointToPoint.Install(nodes.Get(0), nodes.Get(3));
    devices[3] = pointToPoint.Install(nodes.Get(1), nodes.Get(2));
    devices[4] = pointToPoint.Install(nodes.Get(1), nodes.Get(3));
    devices[5] = pointToPoint.Install(nodes.Get(2), nodes.Get(3));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces[6];

    for (int i = 0; i < 6; ++i) {
        std::ostringstream subnet;
        subnet << "10.1." << i + 1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfaces[i] = address.Assign(devices[i]);
    }

    if (useIpv6) {
        Ipv6AddressHelper address6;
        Ipv6InterfaceContainer interfaces6[6];

        for (int i = 0; i < 6; ++i) {
            std::ostringstream subnet6;
            subnet6 << "2001:db8::" << i + 1 << "/64";
            address6.SetBase(subnet6.str().c_str());
            interfaces6[i] = address6.Assign(devices[i]);
        }
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    uint32_t packetSize = 1024;
    uint32_t maxPacketCount = 20;
    Time interPacketInterval = Seconds(1.0);

    UdpEchoClientHelper echoClient(interfaces[0].GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(maxPacketCount));
    echoClient.SetAttribute("Interval", TimeValue(interPacketInterval));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> queueStream = asciiTraceHelper.CreateFileStream("udp-echo.tr");
    qdiscs.Get(0)->TraceConnectWithoutContext("Enqueue", MakeBoundCallback(&QueueDisc::AsciiEnqueueEvent, queueStream));
    qdiscs.Get(0)->TraceConnectWithoutContext("Dequeue", MakeBoundCallback(&QueueDisc::AsciiDequeueEvent, queueStream));
    qdiscs.Get(0)->TraceConnectWithoutContext("Drop", MakeBoundCallback(&QueueDisc::AsciiDropEvent, queueStream));

    pointToPoint.EnablePcapAll("udp-echo");

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}