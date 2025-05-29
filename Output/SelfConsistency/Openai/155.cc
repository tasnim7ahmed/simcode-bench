#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("StarTopologyDataset");

struct PacketInfo
{
    uint32_t packetId;
    uint32_t srcNode;
    uint32_t dstNode;
    uint32_t packetSize;
    double sendTime;
    double recvTime;
};

std::map<uint32_t, PacketInfo> packetInfoMap;
std::vector<PacketInfo> packetLogs;

void
TxCallback(Ptr<const Packet> packet, const Address& address, uint32_t srcNode, uint32_t dstNode)
{
    uint32_t packetId = packet->GetUid();
    PacketInfo info;
    info.packetId = packetId;
    info.srcNode = srcNode;
    info.dstNode = dstNode;
    info.packetSize = packet->GetSize();
    info.sendTime = Simulator::Now().GetSeconds();
    info.recvTime = -1.0;

    packetInfoMap[packetId] = info;
}

void
RxCallback(Ptr<const Packet> packet, const Address& address, uint32_t dstNode)
{
    uint32_t packetId = packet->GetUid();
    auto it = packetInfoMap.find(packetId);
    if (it != packetInfoMap.end())
    {
        it->second.recvTime = Simulator::Now().GetSeconds();
        packetLogs.push_back(it->second);
        packetInfoMap.erase(it);
    }
}

int main(int argc, char *argv[])
{
    // Enable logging if needed
    // LogComponentEnable("StarTopologyDataset", LOG_LEVEL_INFO);

    // Configuration
    uint32_t nLeaf = 4;
    uint32_t nTotal = nLeaf + 1;
    uint32_t packetSize = 1024;
    Time interPacketInterval = MilliSeconds(100);
    uint32_t numPackets = 20;

    // Create nodes
    NodeContainer centralNode;
    centralNode.Create(1);
    NodeContainer leafNodes;
    leafNodes.Create(nLeaf);

    NodeContainer allNodes;
    allNodes.Add(centralNode);
    allNodes.Add(leafNodes);

    // Create point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    std::vector<NetDeviceContainer> deviceContainers;
    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        NetDeviceContainer devices = p2p.Install(centralNode.Get(0), leafNodes.Get(i));
        deviceContainers.push_back(devices);
    }

    // Install Internet Stack
    InternetStackHelper stack;
    stack.Install(allNodes);

    // Assign IP Addresses
    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> interfaceContainers;
    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.1." << i+1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        Ipv4InterfaceContainer interfaces = address.Assign(deviceContainers[i]);
        interfaceContainers.push_back(interfaces);
    }

    // Install applications
    uint16_t basePort = 5000;

    for (uint32_t i = 0; i < nLeaf; ++i)
    {
        uint16_t port = basePort + i;

        // Install a PacketSink on the leaf node
        Address sinkAddress(InetSocketAddress(interfaceContainers[i].GetAddress(1), port));
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkAddress);
        ApplicationContainer sinkApp = sinkHelper.Install(leafNodes.Get(i));
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(20.0));

        // Install an OnOff application on the central node
        OnOffHelper clientHelper("ns3::TcpSocketFactory", sinkAddress);
        clientHelper.SetAttribute("DataRate", StringValue("2Mbps"));
        clientHelper.SetAttribute("PacketSize", UintegerValue(packetSize));
        clientHelper.SetAttribute("MaxBytes", UintegerValue(0));
        clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

        ApplicationContainer clientApp = clientHelper.Install(centralNode.Get(0));
        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(19.0));

        // Trace packet transmissions from central node to each leaf node
        Ptr<Node> srcAppNode = centralNode.Get(0);
        Ptr<Node> dstAppNode = leafNodes.Get(i);

        Ptr<Ipv4> ipv4Src = srcAppNode->GetObject<Ipv4>();
        uint32_t ifIdx = 1 + i; // central node's interface to leaf i
        Ptr<NetDevice> srcDevice = srcAppNode->GetDevice(ifIdx);

        // Attach Tx callback to device at central node
        srcDevice->TraceConnect("PhyTxEnd", std::bind(&TxCallback, std::placeholders::_1, std::placeholders::_2, 0, i+1));

        // Attach Rx callback to device at destination
        Ptr<Ipv4> ipv4Dst = dstAppNode->GetObject<Ipv4>();
        Ptr<NetDevice> dstDevice = dstAppNode->GetDevice(1); // device 0 = loopback, device 1 is the p2p dev
        dstDevice->TraceConnect("PhyRxEnd", std::bind(&RxCallback, std::placeholders::_1, std::placeholders::_2, i+1));
    }

    // Run simulation
    Simulator::Stop(Seconds(20.0));
    Simulator::Run();

    // Write CSV file
    std::ofstream outFile("star_topology_dataset.csv");
    outFile << "src_node,dst_node,packet_size,tx_time,rx_time\n";
    for (const auto& info : packetLogs)
    {
        if (info.recvTime < 0)
            continue; // Only log packets actually received

        outFile << info.srcNode << ","
                << info.dstNode << ","
                << info.packetSize << ","
                << info.sendTime << ","
                << info.recvTime << "\n";
    }
    outFile.close();

    Simulator::Destroy();
    return 0;
}