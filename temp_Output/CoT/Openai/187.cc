#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/animation-interface.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("FatTreeDataCenter");

int main (int argc, char *argv[])
{
    uint32_t k = 4; // Fat-tree parameter: number of ports per switch
    uint32_t pods = k;
    uint32_t aggSwitchesPerPod = k / 2;
    uint32_t edgeSwitchesPerPod = k / 2;
    uint32_t coreSwitches = (k / 2) * (k / 2);
    uint32_t serversPerEdge = k / 2;
    uint32_t numServers = pods * edgeSwitchesPerPod * serversPerEdge;
    uint32_t numEdgeSwitches = pods * edgeSwitchesPerPod;
    uint32_t numAggSwitches = pods * aggSwitchesPerPod;

    // Create nodes
    NodeContainer coreSwitchesContainer;
    coreSwitchesContainer.Create (coreSwitches);

    std::vector<NodeContainer> aggSwitchesContainers(pods);
    std::vector<NodeContainer> edgeSwitchesContainers(pods);
    std::vector<std::vector<NodeContainer>> serversContainers(pods, std::vector<NodeContainer>(edgeSwitchesPerPod));

    for (uint32_t p = 0; p < pods; ++p)
    {
        aggSwitchesContainers[p].Create(aggSwitchesPerPod);
        edgeSwitchesContainers[p].Create(edgeSwitchesPerPod);

        for (uint32_t e = 0; e < edgeSwitchesPerPod; ++e)
        {
            serversContainers[p][e].Create(serversPerEdge);
        }
    }

    InternetStackHelper internet;
    for (uint32_t c = 0; c < coreSwitches; ++c)
        internet.Install(coreSwitchesContainer.Get(c));
    for (uint32_t p = 0; p < pods; ++p)
    {
        for (uint32_t a = 0; a < aggSwitchesPerPod; ++a)
            internet.Install(aggSwitchesContainers[p].Get(a));
        for (uint32_t e = 0; e < edgeSwitchesPerPod; ++e)
        {
            internet.Install(edgeSwitchesContainers[p].Get(e));
            for (uint32_t s = 0; s < serversPerEdge; ++s)
                internet.Install(serversContainers[p][e].Get(s));
        }
    }

    PointToPointHelper p2pCoreAgg;
    p2pCoreAgg.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
    p2pCoreAgg.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper p2pAggEdge;
    p2pAggEdge.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
    p2pAggEdge.SetChannelAttribute("Delay", StringValue("1ms"));

    PointToPointHelper p2pEdgeServer;
    p2pEdgeServer.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    p2pEdgeServer.SetChannelAttribute("Delay", StringValue("0.5ms"));

    // Track all interfaces for IP assignment
    std::vector<NetDeviceContainer> allDevices;
    // Track all node pairs for animation description
    std::vector<std::pair<Ptr<Node>, Ptr<Node>>> allLinks;

    // Connect core <-> aggregation
    uint32_t groupSize = k/2;
    for (uint32_t i = 0; i < coreSwitches; ++i)
    {
        uint32_t group = i / groupSize;
        for (uint32_t pod = 0; pod < pods; ++pod)
        {
            NetDeviceContainer link = p2pCoreAgg.Install(coreSwitchesContainer.Get(i), aggSwitchesContainers[pod].Get(group));
            allDevices.push_back(link);
            allLinks.push_back({coreSwitchesContainer.Get(i), aggSwitchesContainers[pod].Get(group)});
        }
    }

    // Connect aggregation <-> edge within each pod
    for (uint32_t pod = 0; pod < pods; ++pod)
    {
        for (uint32_t agg = 0; agg < aggSwitchesPerPod; ++agg)
        {
            for (uint32_t edge = 0; edge < edgeSwitchesPerPod; ++edge)
            {
                NetDeviceContainer link = p2pAggEdge.Install(aggSwitchesContainers[pod].Get(agg), edgeSwitchesContainers[pod].Get(edge));
                allDevices.push_back(link);
                allLinks.push_back({aggSwitchesContainers[pod].Get(agg), edgeSwitchesContainers[pod].Get(edge)});
            }
        }
    }

    // Connect edge <-> server within each pod
    for (uint32_t pod = 0; pod < pods; ++pod)
    {
        for (uint32_t edge = 0; edge < edgeSwitchesPerPod; ++edge)
        {
            for (uint32_t s = 0; s < serversPerEdge; ++s)
            {
                NetDeviceContainer link = p2pEdgeServer.Install(edgeSwitchesContainers[pod].Get(edge), serversContainers[pod][edge].Get(s));
                allDevices.push_back(link);
                allLinks.push_back({edgeSwitchesContainers[pod].Get(edge), serversContainers[pod][edge].Get(s)});
            }
        }
    }

    // Assign IP addresses
    Ipv4AddressHelper address;
    uint32_t subnet = 0;
    std::vector<Ipv4InterfaceContainer> allInterfaces;
    for (auto &devCont : allDevices)
    {
        std::ostringstream subnetStr;
        subnetStr << "10." << ((subnet >> 8) & 255) << "." << (subnet & 255) << ".0";
        address.SetBase(subnetStr.str().c_str(), "255.255.255.0");
        allInterfaces.push_back(address.Assign(devCont));
        subnet++;
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Setup TCP applications between two random servers in different pods
    Ptr<Node> srcServer = serversContainers[0][0].Get(0); // server in pod 0, edge 0, server 0
    Ptr<Node> dstServer = serversContainers[pods-1][edgeSwitchesPerPod-1].Get(serversPerEdge-1); // last server

    // Find their IP addresses
    Ipv4Address dstAddr;
    for (auto &iface : allInterfaces)
    {
        if (iface.GetN() > 1 && iface.Get(1).first == dstServer)
        {
            dstAddr = iface.GetAddress(1);
            break;
        }
    }
    if (dstAddr == Ipv4Address())
    {
        // fallback: brute-force search
        Ptr<Ipv4> ipv4 = dstServer->GetObject<Ipv4>();
        dstAddr = ipv4->GetAddress(1,0).GetLocal();
    }

    uint16_t servPort = 50000;
    Address bindAddress(InetSocketAddress(Ipv4Address::GetAny(), servPort));
    Address remoteAddress(InetSocketAddress(dstAddr, servPort));

    // Install packet sink on dstServer
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), servPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(dstServer);
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    // Install TCP BulkSend on srcServer
    BulkSendHelper bulkSend("ns3::TcpSocketFactory", InetSocketAddress(dstAddr, servPort));
    bulkSend.SetAttribute("MaxBytes", UintegerValue(0)); // Send as much as possible
    ApplicationContainer clientApps = bulkSend.Install(srcServer);
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(9.0));

    // Set up NetAnim positions
    AnimationInterface anim("fattree-netanim.xml");
    uint32_t nodeId = 0;
    double y_offset = 0.0, X_SPACING = 30.0, Y_SPACING = 30.0;

    // Place core switches
    for (uint32_t i = 0; i < coreSwitches; ++i, ++nodeId)
        anim.SetConstantPosition(coreSwitchesContainer.Get(i), X_SPACING*(i+2), Y_SPACING);

    nodeId = 0;
    // Place aggregation switches
    for (uint32_t pod = 0; pod < pods; ++pod)
        for (uint32_t agg = 0; agg < aggSwitchesPerPod; ++agg, ++nodeId)
            anim.SetConstantPosition(aggSwitchesContainers[pod].Get(agg), X_SPACING*(nodeId+1), Y_SPACING*2);

    nodeId = 0;
    // Place edge switches
    for (uint32_t pod = 0; pod < pods; ++pod)
        for (uint32_t edge = 0; edge < edgeSwitchesPerPod; ++edge, ++nodeId)
            anim.SetConstantPosition(edgeSwitchesContainers[pod].Get(edge), X_SPACING*(nodeId+1), Y_SPACING*3);

    nodeId = 0;
    // Place servers
    for (uint32_t pod = 0; pod < pods; ++pod)
    {
        for (uint32_t edge = 0; edge < edgeSwitchesPerPod; ++edge)
        {
            for (uint32_t s = 0; s < serversPerEdge; ++s, ++nodeId)
            {
                anim.SetConstantPosition(serversContainers[pod][edge].Get(s), X_SPACING*(nodeId+1), Y_SPACING*4);
            }
        }
    }

    Simulator::Stop(Seconds(10.5));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}