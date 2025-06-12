#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpEchoClientServerSimulation");

int main(int argc, char *argv[])
{
    uint32_t packetSize = 1024;
    uint32_t numPackets = 10;
    double interval = 1.0; // seconds
    std::string dataRate = "5Mbps";
    std::string delay = "2ms";

    CommandLine cmd(__FILE__);
    cmd.AddValue("packetSize", "Size of each packet in bytes", packetSize);
    cmd.AddValue("numPackets", "Number of packets to send", numPackets);
    cmd.AddValue("interval", "Interval between packets in seconds", interval);
    cmd.AddValue("dataRate", "Data rate of the link", dataRate);
    cmd.AddValue("delay", "Propagation delay of the link", delay);
    cmd.Parse(argc, argv);

    Time interPacketInterval = Seconds(interval);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue(dataRate));
    pointToPoint.SetChannelAttribute("Delay", StringValue(delay));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(numPackets));
    echoClient.SetAttribute("Interval", TimeValue(interPacketInterval));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("udp-echo.tr");
    pointToPoint.EnableAsciiAll(stream);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}