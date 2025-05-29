#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging if needed
    // LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);

    // Set simulation parameters
    uint32_t numSpokes = 8;
    double simTime = 10.0;

    // Create nodes
    NodeContainer hubNode;
    hubNode.Create(1);
    NodeContainer spokeNodes;
    spokeNodes.Create(numSpokes);

    // Create point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Create NetDevice containers for each spoke-hub link
    std::vector<NetDeviceContainer> deviceContainers(numSpokes);
    for (uint32_t i = 0; i < numSpokes; ++i)
    {
        NodeContainer pair(hubNode.Get(0), spokeNodes.Get(i));
        deviceContainers[i] = p2p.Install(pair);
    }

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(hubNode);
    stack.Install(spokeNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> interfaceContainers(numSpokes);
    for (uint32_t i = 0; i < numSpokes; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << i + 1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfaceContainers[i] = address.Assign(deviceContainers[i]);
    }

    // Install PacketSinkApplication on the hub node
    uint16_t sinkPort = 50000;
    Address sinkAddress(InetSocketAddress(interfaceContainers[0].GetAddress(0), sinkPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(hubNode.Get(0));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(simTime));

    // Configure OnOffApplication attributes
    OnOffHelper onOffHelper("ns3::TcpSocketFactory", Address());
    onOffHelper.SetAttribute("PacketSize", UintegerValue(137));
    onOffHelper.SetAttribute("DataRate", DataRateValue(DataRate("14kbps")));
    onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    // Install OnOffApplications on spoke nodes, targeting the hub node's IP on their respective subnet
    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < numSpokes; ++i)
    {
        Address remoteAddress(InetSocketAddress(interfaceContainers[i].GetAddress(0), sinkPort));
        onOffHelper.SetAttribute("Remote", AddressValue(remoteAddress));
        clientApps.Add(onOffHelper.Install(spokeNodes.Get(i)));
    }
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simTime));

    // Enable static global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Enable pcap tracing on all devices
    for (uint32_t i = 0; i < numSpokes; ++i)
    {
        p2p.EnablePcapAll("star-topology", false);
    }

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}