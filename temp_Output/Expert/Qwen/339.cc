#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-header.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpClientServerSimulation");

int main(int argc, char *argv[]) {
    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

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

    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(1));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    AddressValue remoteAddress(InetSocketAddress(interfaces.GetAddress(1), port));
    BulkSendHelper clientHelper("ns3::TcpSocketFactory", remoteAddress);
    clientHelper.SetAttribute("MaxBytes", UintegerValue(100 * 512));
    ApplicationContainer clientApp = clientHelper.Install(nodes.Get(0));
    clientApp.Start(Seconds(0.0));
    clientApp.Stop(Seconds(10.0));

    Config::Set("/NodeList/0/ApplicationList/0/StartTime", StringValue("0s"));
    Config::Set("/NodeList/0/ApplicationList/0/StopTime", StringValue("10s"));

    Simulator::Schedule(Seconds(0.0), &BulkSendApplication::SendPacket, DynamicCast<BulkSendApplication>(clientApp.Get(0)));
    for (double t = 0.1; t < 10.0; t += 0.1) {
        Simulator::Schedule(Seconds(t), &BulkSendApplication::SendPacket, DynamicCast<BulkSendApplication>(clientApp.Get(0)));
    }

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}