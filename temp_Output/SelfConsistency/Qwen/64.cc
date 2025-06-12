#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpStarServerExample");

int main(int argc, char *argv[])
{
    uint32_t numNodes = 9;
    uint32_t packetSize = 1024;
    DataRate dataRate("1Mbps");

    CommandLine cmd(__FILE__);
    cmd.AddValue("numNodes", "Number of nodes in the star topology (including hub).", numNodes);
    cmd.AddValue("packetSize", "Size of CBR packets.", packetSize);
    cmd.AddValue("dataRate", "Data rate of CBR traffic.", dataRate);
    cmd.Parse(argc, argv);

    if (numNodes < 2)
    {
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
    pointToPoint.SetDeviceAttribute("DataRate", DataRateValue(dataRate));
    pointToPoint.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NetDeviceContainer hubDevices;
    NetDeviceContainer armDevices;

    for (uint32_t i = 0; i < armNodes.GetN(); ++i)
    {
        NetDeviceContainer link = pointToPoint.Install(hubNode.Get(0), armNodes.Get(i));
        hubDevices.Add(link.Get(0));
        armDevices.Add(link.Get(1));
    }

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> interfaces;

    for (uint32_t i = 0; i < hubDevices.GetN(); ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << i << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        Ipv4InterfaceContainer interfacePair = address.Assign(NetDeviceContainer(hubDevices.Get(i), armDevices.Get(i)));
        interfaces.push_back(interfacePair);
    }

    // Install TCP receiver on the hub node
    uint16_t sinkPort = 8080;
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = sinkHelper.Install(hubNode);
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Install CBR over TCP on each arm node
    for (uint32_t i = 0; i < armNodes.GetN(); ++i)
    {
        InetSocketAddress remoteAddress(interfaces[i].GetAddress(0), sinkPort); // Hub side IP
        remoteAddress.SetTos(0xb8); // IPTOS_LOWDELAY | IPTOS_THROUGHPUT

        OnOffHelper client("ns3::TcpSocketFactory", remoteAddress);
        client.SetConstantRate(DataRate(dataRate.GetBitRate()), packetSize);
        client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

        ApplicationContainer clientApp = client.Install(armNodes.Get(i));
        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(10.0));
    }

    AsciiTraceHelper asciiTraceHelper;
    pointToPoint.EnableAsciiAll(asciiTraceHelper.CreateFileStream("tcp-star-server.tr"));

    for (uint32_t i = 0; i < hubDevices.GetN(); ++i)
    {
        std::ostringstream oss;
        oss << "tcp-star-server-" << numNodes << "-" << i << ".pcap";
        pointToPoint.EnablePcap(oss.str().c_str(), hubDevices.Get(i), false);
    }

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}