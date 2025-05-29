#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/queue-disc-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Configure point-to-point link
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Install NetDevices
    NetDeviceContainer devices = pointToPoint.Install(nodes);

    // Install Rate limiting queue
    Ptr<Queue<Packet>> queue = CreateObject<DropTailQueue<Packet>>();
    queue->SetAttribute("MaxPackets", UintegerValue(5));
    Ptr<PointToPointNetDevice> nd = DynamicCast<PointToPointNetDevice>(devices.Get(0));
    nd->SetQueue(queue);

    // Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up UDP Server
    uint16_t port = 4000;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    // Set up UDP Client to generate aggressive traffic
    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(100));
    echoClient.SetAttribute("Interval", TimeValue(MilliSeconds(10))); // 100 packets/sec
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    // NetAnim setup
    AnimationInterface anim("packet-loss-demo.xml");
    anim.SetConstantPosition(nodes.Get(0), 0.0, 50.0);
    anim.SetConstantPosition(nodes.Get(1), 100.0, 50.0);

    // Track packet drops at the queue
    Ptr<DropTailQueue<Packet>> dtq = DynamicCast<DropTailQueue<Packet>>(nd->GetQueue());
    dtq->TraceConnectWithoutContext("Drop", MakeCallback([](Ptr<const Packet> packet){
        std::cout << "Packet dropped at queue at time " << Simulator::Now().GetSeconds() << "s" << std::endl;
    }));

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}