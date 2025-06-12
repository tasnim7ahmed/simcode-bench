#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    uint32_t nSpokes = 8;
    NodeContainer hubNode;
    hubNode.Create(1);

    NodeContainer spokeNodes;
    spokeNodes.Create(nSpokes);

    NodeContainer starNodes;
    starNodes.Add(hubNode);
    starNodes.Add(spokeNodes);

    // Create point-to-point links for each spoke
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    std::vector<NetDeviceContainer> deviceContainers(nSpokes);
    std::vector<NodeContainer> nodePairs(nSpokes);

    for (uint32_t i = 0; i < nSpokes; ++i)
    {
        nodePairs[i] = NodeContainer(hubNode.Get(0), spokeNodes.Get(i));
        deviceContainers[i] = p2p.Install(nodePairs[i]);
    }

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(starNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> interfaceContainers(nSpokes);

    for (uint32_t i = 0; i < nSpokes; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << i+1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfaceContainers[i] = address.Assign(deviceContainers[i]);
    }

    // Install PacketSink on the hub node (port 50000)
    uint16_t sinkPort = 50000;
    Address sinkAddress(InetSocketAddress(interfaceContainers[0].GetAddress(0), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApp = packetSinkHelper.Install(hubNode.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Install OnOff applications on each spoke, targeting the hub's interface per spoke
    OnOffHelper onOffHelper("ns3::TcpSocketFactory", Address());
    onOffHelper.SetAttribute("PacketSize", UintegerValue(137));
    onOffHelper.SetAttribute("DataRate", StringValue("14kbps"));
    onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < nSpokes; ++i)
    {
        AddressValue remoteAddress(InetSocketAddress(interfaceContainers[i].GetAddress(0), sinkPort));
        onOffHelper.SetAttribute("Remote", remoteAddress);
        ApplicationContainer tmpApp = onOffHelper.Install(spokeNodes.Get(i));
        tmpApp.Start(Seconds(1.0));
        tmpApp.Stop(Seconds(10.0));
        clientApps.Add(tmpApp);
    }

    // Enable static global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Enable pcap tracing for all devices
    for (uint32_t i = 0; i < nSpokes; ++i)
    {
        p2p.EnablePcapAll("star-topology", false);
    }

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}