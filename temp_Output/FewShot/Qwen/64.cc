#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpStarNetwork");

int main(int argc, char *argv[]) {
    uint32_t numNodes = 8;
    std::string dataRate = "1Mbps";
    std::string packetSize = "1024";

    CommandLine cmd(__FILE__);
    cmd.AddValue("numNodes", "Number of nodes to place in the star", numNodes);
    cmd.AddValue("dataRate", "Data rate for the links", dataRate);
    cmd.AddValue("packetSize", "Packet size for CBR traffic", packetSize);
    cmd.Parse(argc, argv);

    // Create central hub node and arm nodes
    NodeContainer hub;
    hub.Create(1);

    NodeContainer arms;
    arms.Create(numNodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(dataRate));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer hubDevices;

    // Connect each arm node to the hub
    for (uint32_t i = 0; i < numNodes; ++i) {
        NetDeviceContainer link = p2p.Install(hub.Get(0), arms.Get(i));
        hubDevices.Add(link.Get(0)); // Collect hub side devices
    }

    // Install Internet stack
    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("10.1.0.0", "255.255.255.0");

    Ipv4InterfaceContainer hubInterfaces;
    for (uint32_t i = 0; i < hubDevices.GetN(); ++i) {
        Ipv4InterfaceContainer interfaces = address.Assign(hubDevices.Get(i));
        hubInterfaces.Add(interfaces);
        address.NewNetwork();
    }

    // Assign IP addresses to arm nodes
    for (uint32_t i = 0; i < numNodes; ++i) {
        Ptr<Node> arm = arms.Get(i);
        Ptr<NetDevice> dev = arm->GetDevice(0);
        Ipv4InterfaceContainer interfaces = address.Assign(dev);
        address.NewNetwork();
    }

    // Set up TCP receivers (on hub)
    uint16_t port = 50000;
    ApplicationContainer sinkApps;
    for (uint32_t i = 0; i < numNodes; ++i) {
        Address hubLocalAddress(hubInterfaces.GetAddress(i));
        PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(hubLocalAddress, port + i));
        ApplicationContainer sinkApp = packetSinkHelper.Install(hub.Get(0));
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(10.0));
        sinkApps.Add(sinkApp);
    }

    // Set up CBR senders (on arm nodes)
    ApplicationContainer cbrApps;
    for (uint32_t i = 0; i < numNodes; ++i) {
        TypeId tid = TypeId::LookupByName("ns3::TcpSocketFactory");
        InetSocketAddress destAddr(hubInterfaces.GetAddress(i), port + i);
        CbrClientHelper cbr(tid, destAddr);
        cbr.SetAttribute("PacketSize", UintegerValue(std::stoi(packetSize)));
        cbr.SetAttribute("Interval", TimeValue(Seconds(0.1))); // Fixed interval for simplicity

        ApplicationContainer clientApp = cbr.Install(arms.Get(i));
        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(10.0));
        cbrApps.Add(clientApp);
    }

    // Enable PCAP tracing
    p2p.EnablePcapAll("tcp-star-server");

    // Enable ASCII tracing
    AsciiTraceHelper asciiTraceHelper;
    p2p.EnableAsciiAll(asciiTraceHelper.CreateFileStream("tcp-star-server.tr"));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}