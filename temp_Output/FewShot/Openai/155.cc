#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include <fstream>
#include <sstream>
#include <map>

using namespace ns3;

struct PacketInfo
{
    uint32_t srcNode;
    uint32_t dstNode;
    uint32_t size;
    double txTime;
};

std::ofstream g_csvFile;
std::map<uint64_t, PacketInfo> g_packetInfos;

void
TxTrace(Ptr<const Packet> packet, const Address &address, uint32_t nodeId, uint32_t dstId)
{
    uint64_t uid = packet->GetUid();
    PacketInfo info;
    info.srcNode = nodeId;
    info.dstNode = dstId;
    info.size = packet->GetSize();
    info.txTime = Simulator::Now().GetSeconds();
    g_packetInfos[uid] = info;
}

void
RxTrace(Ptr<const Packet> packet, const Address &address, uint32_t nodeId)
{
    uint64_t uid = packet->GetUid();
    auto it = g_packetInfos.find(uid);
    if (it != g_packetInfos.end())
    {
        g_csvFile << it->second.srcNode << ","
                  << it->second.dstNode << ","
                  << it->second.size << ","
                  << std::fixed << std::setprecision(6) << it->second.txTime << ","
                  << Simulator::Now().GetSeconds()
                  << std::endl;
        g_packetInfos.erase(it);
    }
}

int main(int argc, char *argv[])
{
    // Open CSV file and write header
    g_csvFile.open("star_tcp_dataset.csv", std::ios::out);
    g_csvFile << "src_node,dst_node,packet_size,tx_time,rx_time" << std::endl;

    NodeContainer centerNode;
    centerNode.Create(1);

    NodeContainer spokes;
    spokes.Create(4);

    NodeContainer allNodes;
    allNodes.Add(centerNode);
    allNodes.Add(spokes);

    // Install internet stack
    InternetStackHelper stack;
    stack.Install(allNodes);

    // Create star topology with point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    std::vector<NetDeviceContainer> devices(4);
    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> interfaces(4);

    char baseAddr[] = "10.1.1.0";
    for (uint32_t i = 0; i < 4; ++i)
    {
        devices[i] = p2p.Install(NodeContainer(centerNode.Get(0), spokes.Get(i)));

        std::ostringstream subnet;
        subnet << "10.1." << (i+1) << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfaces[i] = address.Assign(devices[i]);
    }

    // Install TCP server applications on spoke nodes
    uint16_t portBase = 5000;
    std::vector<ApplicationContainer> serverApps(4);
    for (uint32_t i = 0; i < 4; ++i)
    {
        Address bindAddress(InetSocketAddress(Ipv4Address::GetAny(), portBase + i));
        PacketSinkHelper sink("ns3::TcpSocketFactory", bindAddress);
        serverApps[i] = sink.Install(spokes.Get(i));
        serverApps[i].Start(Seconds(0.0));
        serverApps[i].Stop(Seconds(12.0));

        // Trace receptions
        Ptr<PacketSink> sinkPtr = DynamicCast<PacketSink>(serverApps[i].Get(0));
        sinkPtr->TraceConnectWithoutContext("RxWithAddresses",
            MakeBoundCallback(&RxTrace, spokes.Get(i)->GetId()));
    }

    // Install TCP client applications on center node, one per spoke
    std::vector<ApplicationContainer> clientApps(4);
    for (uint32_t i = 0; i < 4; ++i)
    {
        Address serverAddress(InetSocketAddress(interfaces[i].GetAddress(1), portBase + i));
        OnOffHelper client("ns3::TcpSocketFactory", serverAddress);
        client.SetAttribute("DataRate", StringValue("2Mbps"));
        client.SetAttribute("PacketSize", UintegerValue(512));
        client.SetAttribute("MaxBytes", UintegerValue(0)); // unlimited for simulation time
        client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        clientApps[i] = client.Install(centerNode.Get(0));
        clientApps[i].Start(Seconds(1.0 + i));
        clientApps[i].Stop(Seconds(11.0));

        // Trace transmissions
        Ptr<Node> node = centerNode.Get(0);
        Ptr<Application> app = clientApps[i].Get(0);
        Ptr<Socket> srcSocket = Socket::CreateSocket(node, TcpSocketFactory::GetTypeId());
        app->TraceConnectWithoutContext("TxWithAddresses",
            MakeBoundCallback(&TxTrace, node->GetId(), spokes.Get(i)->GetId()));
    }

    // Enable IP routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(12.0));
    Simulator::Run();
    Simulator::Destroy();

    g_csvFile.close();
    return 0;
}