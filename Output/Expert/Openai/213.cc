#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

uint64_t totalBytesSent = 0;
uint64_t totalBytesReceived = 0;

void TxTrace(Ptr<const Packet> packet)
{
    totalBytesSent += packet->GetSize();
}

void RxTrace(Ptr<const Packet> packet, const Address &address)
{
    totalBytesReceived += packet->GetSize();
}

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices = p2p.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 50000;
    Address serverAddress(InetSocketAddress(interfaces.GetAddress(1), port));

    // TCP Server (PacketSink) setup
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = sinkHelper.Install(nodes.Get(1));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    // TCP Client (OnOffApplication) setup
    OnOffHelper clientHelper("ns3::TcpSocketFactory", serverAddress);
    clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
    clientHelper.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    clientHelper.SetAttribute("StopTime", TimeValue(Seconds(9.0)));
    ApplicationContainer clientApps = clientHelper.Install(nodes.Get(0));

    // Packet tracing
    devices.Get(0)->TraceConnectWithoutContext("PhyTxEnd", MakeCallback(&TxTrace));
    devices.Get(1)->TraceConnectWithoutContext("PhyRxEnd", MakeCallback(&RxTrace));

    // Ascii and pcap tracing
    AsciiTraceHelper ascii;
    p2p.EnableAsciiAll(ascii.CreateFileStream("tcp-p2p.tr"));
    p2p.EnablePcapAll("tcp-p2p");

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApps.Get(0));
    uint64_t appBytesReceived = sink ? sink->GetTotalRx() : 0;

    std::cout << "Total bytes sent: " << totalBytesSent << std::endl;
    std::cout << "Total bytes received (packet trace): " << totalBytesReceived << std::endl;
    std::cout << "Total bytes received (application): " << appBytesReceived << std::endl;

    Simulator::Destroy();
    return 0;
}