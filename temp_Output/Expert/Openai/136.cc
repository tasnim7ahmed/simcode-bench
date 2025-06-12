#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UdpEchoSimulation");

int main(int argc, char *argv[])
{
    uint32_t packetSize = 1024;
    uint32_t numPackets = 10;
    double interval = 1.0; // seconds

    CommandLine cmd;
    cmd.AddValue("packetSize", "Size of UDP packets in bytes", packetSize);
    cmd.AddValue("numPackets", "Number of packets to send", numPackets);
    cmd.AddValue("interval", "Interval between packets (seconds)", interval);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t echoPort = 9;

    UdpEchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApp = echoServer.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(20.0));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), echoPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(numPackets));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(interval)));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApp = echoClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(20.0));

    Config::Connect("/NodeList/0/ApplicationList/0/$ns3::UdpEchoClient/Tx", 
        MakeCallback([] (Ptr<const Packet> packet) {
            NS_LOG_INFO ("Client sent " << packet->GetSize() << " bytes");
        })
    );
    Config::Connect("/NodeList/1/ApplicationList/0/$ns3::UdpEchoServer/Rx",
        MakeCallback([] (Ptr<const Packet> packet, const Address &) {
            NS_LOG_INFO ("Server received " << packet->GetSize() << " bytes");
        })
    );

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}