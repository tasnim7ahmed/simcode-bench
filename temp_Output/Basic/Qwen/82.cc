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

    CommandLine cmd;
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
    NetDeviceContainer devices;
    for (uint32_t i = 0; i < 3; ++i) {
        devices = pointToPoint.Install(nodes.Get(i), nodes.Get(i + 1));
        tch.Install(devices);
    }

    InternetStackHelper stack;
    if (useIpv6) {
        stack.SetIpv4StackInstall(false);
        stack.SetIpv6StackInstall(true);
    } else {
        stack.SetIpv4StackInstall(true);
        stack.SetIpv6StackInstall(false);
    }
    stack.Install(nodes);

    Ipv4AddressHelper ipv4;
    Ipv6AddressHelper ipv6;
    std::vector<Ipv4InterfaceContainer> ipv4Interfaces;
    std::vector<Ipv6InterfaceContainer> ipv6Interfaces;

    for (uint32_t i = 0; i < 3; ++i) {
        std::ostringstream subnet;
        subnet << "10.1." << i + 1 << ".0";
        ipv4.SetBase(subnet.str().c_str(), "255.255.255.0");
        ipv4Interfaces.push_back(ipv4.Assign(devices));

        std::ostringstream ipv6Subnet;
        ipv6Subnet << "2001:" << std::hex << i + 1 << "::";
        ipv6.SetBase(ipv6Subnet.str().c_str(), Ipv6Prefix(64));
        ipv6Interfaces.push_back(ipv6.Assign(devices));
    }

    if (!useIpv6) {
        Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    }

    uint16_t port = 9;
    UdpEchoServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    Time interPacketInterval = Seconds(1.0);
    UdpEchoClientHelper client;
    if (useIpv6) {
        client = UdpEchoClientHelper(ipv6Interfaces[0].GetAddress(1, 1), port);
    } else {
        client = UdpEchoClientHelper(ipv4Interfaces[0].GetAddress(1), port);
    }
    client.SetAttribute("MaxPackets", UintegerValue(10));
    client.SetAttribute("Interval", TimeValue(interPacketInterval));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> queueStream = asciiTraceHelper.CreateFileStream("udp-echo.tr");
    for (uint32_t i = 0; i < 3; ++i) {
        devices.Get(0)->GetChannel()->AddPacketPrinter([queueStream](Ptr<const Packet> packet) {
            *queueStream->GetStream() << "Received packet at time: " << Simulator::Now().GetSeconds() << "s" << std::endl;
        });
        devices.Get(1)->GetChannel()->AddPacketPrinter([queueStream](Ptr<const Packet> packet) {
            *queueStream->GetStream() << "Received packet at time: " << Simulator::Now().GetSeconds() << "s" << std::endl;
        });

        devices.Get(0)->GetObject<PointToPointNetDevice>()->TraceConnectWithoutContext("TxQueueLength", MakeBoundCallback(&AsciiTraceHelper::DefaultEnqueueSink, queueStream));
        devices.Get(1)->GetObject<PointToPointNetDevice>()->TraceConnectWithoutContext("TxQueueLength", MakeBoundCallback(&AsciiTraceHelper::DefaultEnqueueSink, queueStream));
    }

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}