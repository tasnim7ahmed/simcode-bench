#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet-sink.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

static uint64_t totalTxBytes = 0;
static uint64_t totalRxBytes = 0;

void
TxTrace(Ptr<const Packet> packet)
{
    totalTxBytes += packet->GetSize();
}

void
RxTrace(Ptr<const Packet> packet, const Address &address)
{
    totalRxBytes += packet->GetSize();
}

int
main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Set up point-to-point link
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up PacketSink TCP server on node 1
    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Set up TCP client on node 0
    OnOffHelper clientHelper("ns3::TcpSocketFactory", sinkAddress);
    clientHelper.SetAttribute("DataRate", StringValue("2Mbps"));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
    clientHelper.SetAttribute("MaxBytes", UintegerValue(10240));
    ApplicationContainer clientApp = clientHelper.Install(nodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(9.0));

    // Enable packet tracing on device 0 (client)
    devices.Get(0)->TraceConnectWithoutContext("PhyTxEnd", MakeCallback(&TxTrace));

    // Enable packet tracing on PacketSink (server)
    Ptr<Application> sinkAppPtr = sinkApp.Get(0);
    Ptr<PacketSink> packetSink = DynamicCast<PacketSink>(sinkAppPtr);
    packetSink->TraceConnectWithoutContext("Rx", MakeCallback(&RxTrace));

    // Enable ASCII and PCAP tracing
    AsciiTraceHelper ascii;
    pointToPoint.EnableAsciiAll(ascii.CreateFileStream("tcp-p2p.tr"));
    pointToPoint.EnablePcapAll("tcp-p2p");

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    std::cout << "Total bytes sent: " << totalTxBytes << std::endl;
    std::cout << "Total bytes received: " << totalRxBytes << std::endl;

    Simulator::Destroy();
    return 0;
}