#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SingleNodeUdpEchoExample");

int
main(int argc, char *argv[])
{
    uint32_t packetSize = 1024;
    uint32_t numPackets = 10;
    double interval = 1.0; // seconds

    CommandLine cmd;
    cmd.AddValue("packetSize", "Size of the packets", packetSize);
    cmd.AddValue("numPackets", "Number of packets", numPackets);
    cmd.AddValue("interval", "Interval between packets (seconds)", interval);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = p2p.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t echoPort = 9;

    UdpEchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(0.5));
    serverApps.Stop(Seconds(20.0));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), echoPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(numPackets));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(interval)));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(20.0));

    Config::Connect("/NodeList/*/ApplicationList/*/$ns3::UdpEchoClient/Tx", 
        MakeCallback([](std::string context, Ptr<const Packet> p){
            NS_LOG_INFO("EchoClient: Packet TX, size=" << p->GetSize());
        })
    );

    Config::Connect("/NodeList/*/ApplicationList/*/$ns3::UdpEchoServer/Rx", 
        MakeCallback([](std::string context, Ptr<const Packet> p, const Address & addr){
            NS_LOG_INFO("EchoServer: Packet RX, size=" << p->GetSize());
        })
    );

    Simulator::Stop(Seconds(21.0));
    LogComponentEnable("SingleNodeUdpEchoExample", LOG_LEVEL_INFO);
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}