#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpStarServerSimulation");

int
main(int argc, char *argv[])
{
    uint32_t numNodes = 9;
    std::string rate("2048bps");
    std::string packetSize("512");

    CommandLine cmd(__FILE__);
    cmd.AddValue("numNodes", "Number of nodes in the star topology (including hub).", numNodes);
    cmd.AddValue("rate", "Data rate for CBR traffic.", rate);
    cmd.AddValue("packetSize", "Packet size for CBR traffic.", packetSize);
    cmd.Parse(argc, argv);

    if (numNodes < 2)
    {
        NS_ABORT_MSG("Number of nodes must be at least 2 (hub + one arm node).");
    }

    uint32_t numArmNodes = numNodes - 1;

    NodeContainer hub;
    hub.Create(1);

    NodeContainer armNodes;
    armNodes.Create(numArmNodes);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer hubDevices;
    NetDeviceContainer armDevices;

    for (uint32_t i = 0; i < numArmNodes; ++i)
    {
        NetDeviceContainer ndc = p2p.Install(hub.Get(0), armNodes.Get(i));
        hubDevices.Add(ndc.Get(0));
        armDevices.Add(ndc.Get(1));
    }

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    char ipBase[20];
    for (uint32_t i = 0; i < numArmNodes; ++i)
    {
        sprintf(ipBase, "10.1.%d.0", i);
        address.SetBase(ipBase, "255.255.255.0");
        address.Assign(p2p.Install(hub.Get(0), armNodes.Get(i)));
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 50000;
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApp = sinkHelper.Install(hub.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(20.0));

    OnOffHelper clientHelper("ns3::TcpSocketFactory", InetSocketAddress());
    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    clientHelper.SetAttribute("DataRate", DataRateValue(DataRate(rate)));
    clientHelper.SetAttribute("PacketSize", UintegerValue(std::stoi(packetSize)));

    ApplicationContainer clientApps;

    for (uint32_t i = 0; i < numArmNodes; ++i)
    {
        Address remoteAddress(InetSocketAddress(Ipv4Address("10.1.0.1"), port));
        clientHelper.SetAttribute("Remote", AddressValue(remoteAddress));
        clientApps = clientHelper.Install(armNodes.Get(i));
        clientApps.Start(Seconds(1.0));
        clientApps.Stop(Seconds(20.0));
    }

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("tcp-star-server.tr");
    p2p.EnableAsciiAll(stream);

    for (uint32_t i = 0; i < hubDevices.GetN(); ++i)
    {
        std::ostringstream oss;
        oss << "tcp-star-server-" << hub.Get(0)->GetId() << "-" << i << ".pcap";
        p2p.EnablePcap(oss.str().c_str(), hubDevices.Get(i), true, true);
    }

    for (uint32_t i = 0; i < armDevices.GetN(); ++i)
    {
        std::ostringstream oss;
        oss << "tcp-star-server-" << armNodes.Get(i)->GetId() << "-" << i << ".pcap";
        p2p.EnablePcap(oss.str().c_str(), armDevices.Get(i), true, true);
    }

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}