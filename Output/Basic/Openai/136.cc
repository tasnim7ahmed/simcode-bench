#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpEchoExample");

void TxCallback(Ptr<const Packet> packet)
{
    NS_LOG_INFO("Packet transmitted: size=" << packet->GetSize() << " bytes");
}

void RxCallback(Ptr<const Packet> packet, const Address &address)
{
    NS_LOG_INFO("Packet received: size=" << packet->GetSize() << " bytes");
}

int main(int argc, char *argv[])
{
    uint32_t packetSize = 1024; // bytes
    uint32_t numPackets = 10;
    double interval = 1.0; // seconds

    CommandLine cmd;
    cmd.AddValue("packetSize", "Size of each packet sent", packetSize);
    cmd.AddValue("numPackets", "Number of packets to send", numPackets);
    cmd.AddValue("interval", "Interval between packets (s)", interval);
    cmd.Parse(argc, argv);

    LogComponentEnable("UdpEchoExample", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    uint16_t echoPort = 9;

    UdpEchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(1.0 + numPackets * interval + 2.0));

    Ptr<UdpEchoServer> server = DynamicCast<UdpEchoServer>(serverApps.Get(0));
    server->TraceConnectWithoutContext("Rx", MakeCallback(&RxCallback));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), echoPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(numPackets));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(interval)));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(2.0 + numPackets * interval + 2.0));

    Ptr<Application> app = clientApps.Get(0);
    Ptr<UdpEchoClient> client = DynamicCast<UdpEchoClient>(app);
    client->TraceConnectWithoutContext("Tx", MakeCallback(&TxCallback));

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}