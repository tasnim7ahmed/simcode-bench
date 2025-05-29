#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 9;

    // Install BulkSendApplication on node 0
    BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
    source.SetAttribute("MaxBytes", UintegerValue(0));
    ApplicationContainer sourceApps = source.Install(nodes.Get(0));
    sourceApps.Start(Seconds(0.0));
    sourceApps.Stop(Seconds(10.0));

    // Install PacketSinkApplication on node 1
    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = sink.Install(nodes.Get(1));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    pointToPoint.EnablePcapAll("tcp-p2p-trace", true);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    Ptr<PacketSink> sink1 = DynamicCast<PacketSink>(sinkApps.Get(0));
    double totalBytesReceived = sink1->GetTotalRx();
    double throughput = totalBytesReceived * 8 / (10.0 * 1000000.0); // Mbps

    std::cout << "Total Bytes Received: " << totalBytesReceived << std::endl;
    std::cout << "Average Throughput: " << throughput << " Mbps" << std::endl;

    Simulator::Destroy();
    return 0;
}