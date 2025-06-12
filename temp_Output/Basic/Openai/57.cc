#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    uint32_t nSpokes = 8;
    NodeContainer hub;
    hub.Create(1);
    NodeContainer spokes;
    spokes.Create(nSpokes);

    NodeContainer starNodes;
    starNodes.Add(hub);
    starNodes.Add(spokes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    InternetStackHelper stack;
    stack.Install(starNodes);

    std::vector<NetDeviceContainer> deviceContainers(nSpokes);
    std::vector<Ipv4InterfaceContainer> interfaceContainers(nSpokes);

    Ipv4AddressHelper address;
    uint32_t subnetBase = 1;

    for (uint32_t i = 0; i < nSpokes; ++i)
    {
        NodeContainer pair;
        pair.Add(hub.Get(0));
        pair.Add(spokes.Get(i));
        deviceContainers[i] = p2p.Install(pair);

        std::ostringstream subnet;
        subnet << "10.1." << subnetBase + i << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfaceContainers[i] = address.Assign(deviceContainers[i]);

        p2p.EnablePcapAll("star-topology", false);
    }

    uint16_t port = 50000;

    Address hubLocalAddress(
        InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", hubLocalAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(hub.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    OnOffHelper onoff("ns3::TcpSocketFactory", Address());
    onoff.SetAttribute("PacketSize", UintegerValue(137));
    onoff.SetAttribute("DataRate", StringValue("14kbps"));
    onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    onoff.SetAttribute("StopTime", TimeValue(Seconds(10.0)));

    for (uint32_t i = 0; i < nSpokes; ++i)
    {
        AddressValue remoteAddress(
            InetSocketAddress(interfaceContainers[i].GetAddress(0), port));
        onoff.SetAttribute("Remote", remoteAddress);
        onoff.Install(spokes.Get(i));
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}