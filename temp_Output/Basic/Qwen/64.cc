#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpStarServer");

int main(int argc, char *argv[]) {
    uint32_t numNodes = 9;
    uint32_t packetSize = 512;
    std::string dataRate = "1Mbps";

    CommandLine cmd(__FILE__);
    cmd.AddValue("numNodes", "Number of nodes in the star topology (including hub).", numNodes);
    cmd.AddValue("packetSize", "Packet size for CBR traffic.", packetSize);
    cmd.AddValue("dataRate", "Data rate for CBR traffic.", dataRate);
    cmd.Parse(argc, argv);

    if (numNodes < 2) {
        NS_LOG_ERROR("Number of nodes must be at least 2.");
        return 1;
    }

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer hubNode;
    hubNode.Create(1);

    NodeContainer armNodes;
    armNodes.Create(numNodes - 1);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue(dataRate));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer hubDevices;
    NetDeviceContainer armDevices;

    for (uint32_t i = 0; i < armNodes.GetN(); ++i) {
        NetDeviceContainer container = pointToPoint.Install(hubNode.Get(0), armNodes.Get(i));
        hubDevices.Add(container.Get(0));
        armDevices.Add(container.Get(1));
    }

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    char subnet[32];
    for (uint32_t i = 0; i < armNodes.GetN(); ++i) {
        sprintf(subnet, "10.1.%d.0", i);
        address.SetBase(subnet, "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(hubDevices.Get(i));
        interfaces.Add(address.Assign(armDevices.Get(i)));
    }

    uint16_t sinkPort = 8080;

    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = sink.Install(hubNode.Get(0));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    OnOffHelper onoff("ns3::TcpSocketFactory", Address());
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate(dataRate)));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < armNodes.GetN(); ++i) {
        AddressValue remoteAddress(InetSocketAddress(sinkApps.Get(0)->GetIpv4()->GetAddress(1, 0).GetLocal(), sinkPort));
        onoff.SetAttribute("Remote", remoteAddress);
        clientApps = onoff.Install(armNodes.Get(i));
        clientApps.Start(Seconds(1.0));
        clientApps.Stop(Seconds(10.0));
    }

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("tcp-star-server.tr");
    pointToPoint.EnableAsciiAll(stream);
    pointToPoint.EnablePcapAll("tcp-star-server");

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}