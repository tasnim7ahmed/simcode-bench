#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleTcpBulkTransfer");

uint64_t totalBytesReceived = 0;

void RxSinkCallback(Ptr<const Packet> packet, const Address &)
{
    totalBytesReceived += packet->GetSize();
}

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("SimpleTcpBulkTransfer", LOG_LEVEL_INFO);

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

    uint16_t sinkPort = 50000;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(1), sinkPort));

    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    Ptr<Application> app = sinkApp.Get(0);
    app->GetObject<PacketSink>()->TraceConnectWithoutContext("Rx", MakeCallback(&RxSinkCallback));

    BulkSendHelper source("ns3::TcpSocketFactory", sinkAddress);
    source.SetAttribute("MaxBytes", UintegerValue(0)); // 0 means unlimited

    ApplicationContainer sourceApp = source.Install(nodes.Get(0));
    sourceApp.Start(Seconds(1.0));
    sourceApp.Stop(Seconds(10.0));

    pointToPoint.EnablePcapAll("simple-tcp-bulk-transfer");

    Simulator::Stop(Seconds(10.0));

    Simulator::Run();

    NS_LOG_UNCOND("Total bytes received: " << totalBytesReceived);

    Simulator::Destroy();
    return 0;
}