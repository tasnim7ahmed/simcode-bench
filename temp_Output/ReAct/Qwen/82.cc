#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

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

    QueueBase::QueueType queueType = QueueBase::QUEUE_DISC;
    DropTailQueue<Packet>::GetTypeId();
    pointToPoint.SetQueue("ns3::DropTailQueue<Packet>");

    NetDeviceContainer devices[6];
    devices[0] = pointToPoint.Install(nodes.Get(0), nodes.Get(1));
    devices[1] = pointToPoint.Install(nodes.Get(1), nodes.Get(2));
    devices[2] = pointToPoint.Install(nodes.Get(1), nodes.Get(3));
    devices[3] = pointToPoint.Install(nodes.Get(0), nodes.Get(2));
    devices[4] = pointToPoint.Install(nodes.Get(0), nodes.Get(3));
    devices[5] = pointToPoint.Install(nodes.Get(2), nodes.Get(3));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces[6];

    for (int i = 0; i < 6; ++i) {
        std::ostringstream subnet;
        subnet << "10.1." << i << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfaces[i] = address.Assign(devices[i]);
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;
    UdpEchoServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    Time interval = Seconds(1.0);
    uint32_t packetSize = 1024;
    UdpEchoClientHelper clientHelper(interfaces[0].GetAddress(1), port);
    clientHelper.SetAttribute("MaxPackets", UintegerValue(5));
    clientHelper.SetAttribute("Interval", TimeValue(interval));
    clientHelper.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps = clientHelper.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> queueStream = asciiTraceHelper.CreateFileStream("udp-echo.tr");
    for (int i = 0; i < 6; ++i) {
        pointToPoint.EnableAsciiQueueTraces(queueStream);
    }

    pointToPoint.EnablePcapAll("udp-echo");

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}