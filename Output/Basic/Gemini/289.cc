#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/tcp-header.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpClientServer");

int main(int argc, char* argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponent::SetFilter("TcpClientServer", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 5000;
    Address serverAddress(InetSocketAddress(interfaces.GetAddress(1), port));

    TcpEchoServerHelper echoServerHelper(port);
    ApplicationContainer serverApps = echoServerHelper.Install(nodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    BulkSendHelper source("ns3::TcpSocketFactory", serverAddress);
    source.SetAttribute("SendSize", UintegerValue(1024));
    source.SetAttribute("MaxBytes", UintegerValue(12500000)); // 5Mbps for 10 seconds ~ 6250000 Bytes, increased for buffer
    ApplicationContainer sourceApps = source.Install(nodes.Get(0));
    sourceApps.Start(Seconds(2.0));
    sourceApps.Stop(Seconds(10.0));

    pointToPoint.EnablePcapAll("tcp-client-server");

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}