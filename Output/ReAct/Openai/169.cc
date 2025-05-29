#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

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

    // Packet sink (TCP server) on node 1
    uint16_t sinkPort = 50000;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", sinkAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // BulkSend (TCP client) on node 0
    BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", sinkAddress);
    bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(0)); // Unlimited data
    ApplicationContainer clientApp = bulkSendHelper.Install(nodes.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    pointToPoint.EnablePcapAll("p2p-tcp");

    Simulator::Stop(Seconds(10.0));

    Simulator::Run();

    // Throughput at the server
    Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinkApp.Get(0));
    double throughput = (sink->GetTotalRx() * 8.0) / (9.0 * 1000000.0); // Mbit/s, data sent during 1-10s (9 sec)
    std::cout << "Average throughput at server: " << throughput << " Mbit/s" << std::endl;

    Simulator::Destroy();
    return 0;
}