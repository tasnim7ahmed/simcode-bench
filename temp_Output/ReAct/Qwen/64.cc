#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpStarServer");

int main(int argc, char *argv[]) {
    uint32_t numNodes = 8;
    uint32_t packetSize = 1024;
    DataRate dataRate("1Mbps");
    TimeValue interPacketInterval = TimeValue(MilliSeconds(200));

    CommandLine cmd(__FILE__);
    cmd.AddValue("numNodes", "Number of nodes (arm nodes)", numNodes);
    cmd.AddValue("packetSize", "Size of CBR packets", packetSize);
    cmd.AddValue("dataRate", "Data rate for CBR traffic", dataRate);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(packetSize));

    NodeContainer hubNode;
    hubNode.Create(1);

    NodeContainer armNodes;
    armNodes.Create(numNodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", DataRateValue(dataRate));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer hubDevices;
    NetDeviceContainer armDevices;

    for (uint32_t i = 0; i < numNodes; ++i) {
        NetDeviceContainer ndc = p2p.Install(hubNode.Get(0), armNodes.Get(i));
        hubDevices.Add(ndc.Get(0));
        armDevices.Add(ndc.Get(1));
    }

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces;

    for (uint32_t i = 0; i < numNodes; ++i) {
        std::ostringstream subnet;
        subnet << "10.1." << i << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfaces = address.Assign(hubDevices.Get(i));
        interfaces.Add(address.Assign(armDevices.Get(i)));
    }

    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), 50000));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(hubNode);
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(20.0));

    OnOffHelper onoff("ns3::TcpSocketFactory", Address());
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(dataRate));
    onoff.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps;

    for (uint32_t i = 0; i < numNodes; ++i) {
        AddressValue remoteAddress(InetSocketAddress(interfaces.GetAddress(0), 50000));
        onoff.SetAttribute("Remote", remoteAddress);
        clientApps = onoff.Install(armNodes.Get(i));
        clientApps.Start(Seconds(1.0 + i * 0.1));
        clientApps.Stop(Seconds(20.0));
    }

    AsciiTraceHelper asciiTraceHelper;
    p2p.EnableAsciiAll(asciiTraceHelper.CreateFileStream("tcp-star-server.tr"));

    for (uint32_t i = 0; i < hubDevices.GetN(); ++i) {
        std::ostringstream oss;
        oss << "tcp-star-server-" << hubNode.Get(0)->GetId() << "-" << i << ".pcap";
        p2p.EnablePcap(oss.str().c_str(), hubDevices.Get(i), true, true);
    }

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}