#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpStarServer");

int main(int argc, char* argv[]) {
    uint32_t nNodes = 9;
    uint32_t packetSize = 1024;
    std::string dataRate = "1Mbps";

    CommandLine cmd;
    cmd.AddValue("nNodes", "Number of nodes", nNodes);
    cmd.AddValue("packetSize", "Packet size", packetSize);
    cmd.AddValue("dataRate", "Data rate", dataRate);
    cmd.Parse(argc, argv);

    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(nNodes);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer devices;
    for (uint32_t i = 1; i < nNodes; ++i) {
        NetDeviceContainer link = pointToPoint.Install(nodes.Get(0), nodes.Get(i));
        devices.Add(link.Get(0));
        devices.Add(link.Get(1));
    }

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces;
    uint32_t deviceIndex = 0;
    for (uint32_t i = 1; i < nNodes; ++i) {
        interfaces.Add(address.AssignOne(devices.Get(deviceIndex)));
        interfaces.Add(address.AssignOne(devices.Get(deviceIndex + 1)));
        address.NewNetwork();
        deviceIndex += 2;
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;

    ApplicationContainer serverApp;
    BulkSendHelper server("ns3::TcpSocketFactory",
                                 Address(InetSocketAddress(interfaces.GetAddress(1), port)));
    serverApp = server.Install(nodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));


    ApplicationContainer clientApps;
    for (uint32_t i = 1; i < nNodes; ++i) {
        BulkSendHelper client("ns3::TcpSocketFactory",
                             Address(InetSocketAddress(interfaces.GetAddress((i*2)-1), port)));
        client.SetAttribute("MaxBytes", UintegerValue(0));
        client.SetAttribute("SendSize", UintegerValue(packetSize));
        client.SetAttribute("Remote", AddressValue(InetSocketAddress(interfaces.GetAddress(0), port)));

        clientApps.Add(client.Install(nodes.Get(i)));
        clientApps.Get(i-1)->SetStartTime(Seconds(2.0));
        clientApps.Get(i-1)->SetStopTime(Seconds(10.0));
    }

    Packet::EnablePcapAll("tcp-star-server");
    Simulator::Stop(Seconds(11.0));

    AsciiTraceHelper ascii;
    pointToPoint.EnableAsciiAll(ascii.CreateFileStream("tcp-star-server.tr"));

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}