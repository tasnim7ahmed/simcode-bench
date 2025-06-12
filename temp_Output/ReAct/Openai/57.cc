#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    uint32_t nSpokes = 8;
    NodeContainer hubNode;
    hubNode.Create(1);
    NodeContainer spokeNodes;
    spokeNodes.Create(nSpokes);

    // Internet stack
    NodeContainer allNodes;
    allNodes.Add(hubNode);
    allNodes.Add(spokeNodes);
    InternetStackHelper stack;
    stack.Install(allNodes);

    // Point-to-Point configuration
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Keep devices and interfaces
    std::vector<NetDeviceContainer> deviceContainers(nSpokes);
    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> interfaceContainers(nSpokes);

    // Setup links and assign IP addresses
    for (uint32_t i = 0; i < nSpokes; ++i) {
        NodeContainer pair(hubNode.Get(0), spokeNodes.Get(i));
        deviceContainers[i] = p2p.Install(pair);

        std::ostringstream subnet;
        subnet << "10.1." << i+1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfaceContainers[i] = address.Assign(deviceContainers[i]);

        // Enable pcap tracing for each device
        p2p.EnablePcapAll("star-" + std::to_string(i), false);
    }

    // Enable static routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Packet sink on the hub
    uint16_t sinkPort = 50000;
    Address sinkAddress(InetSocketAddress(interfaceContainers[0].GetAddress(0), sinkPort));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = sinkHelper.Install(hubNode.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // OnOffApplication on each spoke sending to the hub
    for (uint32_t i = 0; i < nSpokes; ++i) {
        Address remoteAddr(InetSocketAddress(interfaceContainers[i].GetAddress(0), sinkPort));
        OnOffHelper client("ns3::TcpSocketFactory", remoteAddr);
        client.SetAttribute("PacketSize", UintegerValue(137));
        client.SetAttribute("DataRate", StringValue("14kbps"));
        client.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
        client.SetAttribute("StopTime", TimeValue(Seconds(10.0)));
        client.Install(spokeNodes.Get(i));
    }

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}