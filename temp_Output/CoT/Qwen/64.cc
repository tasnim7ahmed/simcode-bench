#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-header.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpStarServer");

int main(int argc, char *argv[]) {
    uint32_t nNodes = 9;
    uint32_t packetSize = 512;
    DataRate dataRate("1Mbps");
    TimeInterval interval(Seconds(0.1));

    CommandLine cmd;
    cmd.AddValue("nNodes", "Number of nodes (including hub)", nNodes);
    cmd.AddValue("packetSize", "Packet size in bytes", packetSize);
    cmd.AddValue("dataRate", "Data rate for CBR traffic", dataRate);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer hubNode;
    hubNode.Create(1);

    NodeContainer armNodes;
    armNodes.Create(nNodes - 1);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", DataRateValue(dataRate));
    p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    NetDeviceContainer hubDevices;
    NetDeviceContainer armDevices[nNodes - 1];

    for (uint32_t i = 0; i < nNodes - 1; ++i) {
        NetDeviceContainer devices = p2p.Install(hubNode.Get(0), armNodes.Get(i));
        hubDevices.Add(devices.Get(0));
        armDevices[i].Add(devices.Get(1));
    }

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces[nNodes];

    uint32_t net = 0x0A000000;
    uint8_t mask = 24;

    for (uint32_t i = 0; i < nNodes - 1; ++i) {
        std::ostringstream subnet;
        subnet << std::hex << (net + (i << mask));
        address.SetBase(subnet.str().c_str(), "/24");
        interfaces[i] = address.Assign(p2p.Install(NodeContainer(hubNode.Get(0), armNodes.Get(i))));
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(packetSize));

    uint16_t port = 5000;
    for (uint32_t i = 0; i < nNodes - 1; ++i) {
        BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(interfaces[i].GetAddress(0), port));
        source.SetAttribute("MaxBytes", UintegerValue(0));
        ApplicationContainer sourceApp = source.Install(armNodes.Get(i));
        sourceApp.Start(Seconds(0.0));
        sourceApp.Stop(Seconds(10.0));

        PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer sinkApp = sink.Install(hubNode.Get(0));
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(10.0));
        port++;
    }

    AsciiTraceHelper asciiTraceHelper;
    p2p.EnableAsciiAll(asciiTraceHelper.CreateFileStream("tcp-star-server.tr"));

    for (uint32_t i = 0; i < nNodes - 1; ++i) {
        std::ostringstream oss;
        oss << "tcp-star-server-" << armNodes.Get(i)->GetId() << "-" << 0 << ".pcap";
        p2p.EnablePcap(oss.str().c_str(), armDevices[i], true, true);
    }

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}