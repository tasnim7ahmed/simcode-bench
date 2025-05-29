#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleTcpExample");

int main(int argc, char *argv[])
{
    // Enable logging for BulkSendApplication and PacketSink
    LogComponentEnable("BulkSendApplication", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Set up point-to-point connection attributes
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // Create net devices
    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Configure TCP server: PacketSink on node 1 (receiver)
    uint16_t port = 8080;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), port));

    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(1));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    // Configure TCP client: BulkSendApplication on node 0 (sender)
    BulkSendHelper clientHelper("ns3::TcpSocketFactory", sinkAddress);
    clientHelper.SetAttribute("MaxBytes", UintegerValue(1024 * 10)); // 10 packets * 1024 bytes = 10240 bytes
    clientHelper.SetAttribute("SendSize", UintegerValue(1024)); // Send packets of 1024 bytes

    ApplicationContainer clientApp = clientHelper.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Limit send rate by setting socket send delay (workaround for BulkSend - one way is to schedule start/stop)
    // But we want 10 packets, one every second, so let's use OnOffApplication instead
    // We'll remove BulkSend and use OnOffApplication for precise control

    clientApp.Stop(Seconds(2.0)); // Cancel previous send app

    OnOffHelper onoff("ns3::TcpSocketFactory", sinkAddress);
    onoff.SetAttribute("DataRate", StringValue("10Mbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));
    onoff.SetAttribute("MaxBytes", UintegerValue(1024 * 10));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer onoffApp = onoff.Install(nodes.Get(0));
    onoffApp.Start(Seconds(2.0));
    onoffApp.Stop(Seconds(12.0)); // Allow for all 10 packets at 1s interval

    // Schedule packet sending manually for precise interval
    Ptr<Socket> srcSocket = Socket::CreateSocket(nodes.Get(0), TcpSocketFactory::GetTypeId());
    srcSocket->Connect(sinkAddress);

    for (uint32_t i = 0; i < 10; ++i)
    {
        Simulator::Schedule(Seconds(2.0 + i * 1.0), [srcSocket]()
        {
            Ptr<Packet> pkt = Create<Packet>(1024);
            srcSocket->Send(pkt);
        });
    }

    // Stop simulation at 12s to ensure all packets have been sent and received
    Simulator::Stop(Seconds(12.0));

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}