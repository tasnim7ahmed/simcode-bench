#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include <fstream>
#include <string>
#include <iomanip>
#include <map>

using namespace ns3;

struct PacketRecord {
    uint32_t srcNode;
    uint32_t dstNode;
    uint32_t size;
    double   sendTime;
    double   recvTime;
};

std::map<uint64_t, PacketRecord> packetRecords;
std::ofstream csvFile;

void
TxTrace(Ptr<const Packet> packet, const Address& srcAddr, uint32_t nodeId, uint32_t dstNode)
{
    uint64_t uid = packet->GetUid();
    PacketRecord rec;
    rec.srcNode = nodeId;
    rec.dstNode = dstNode;
    rec.size = packet->GetSize();
    rec.sendTime = Simulator::Now().GetSeconds();
    rec.recvTime = -1.0;
    packetRecords[uid] = rec;
}

void
RxTrace(Ptr<const Packet> packet, const Address& addr, uint32_t dstNode)
{
    uint64_t uid = packet->GetUid();

    auto it = packetRecords.find(uid);
    if (it != packetRecords.end()) {
        it->second.recvTime = Simulator::Now().GetSeconds();
        // Write to CSV
        csvFile << it->second.srcNode << ","
                << it->second.dstNode << ","
                << it->second.size << ","
                << std::fixed << std::setprecision(6) << it->second.sendTime << ","
                << std::fixed << std::setprecision(6) << it->second.recvTime << std::endl;
        packetRecords.erase(it);
    }
}

int
main(int argc, char* argv[])
{
    uint32_t numNodes = 5;
    double simTime = 10.0;
    uint16_t port = 4000;

    // Open CSV file and write header
    csvFile.open("packet_dataset.csv");
    csvFile << "src_node,dst_node,packet_size,tx_time,rx_time" << std::endl;

    NodeContainer nodes;
    nodes.Create(numNodes);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    for (uint32_t i = 0; i < numNodes - 1; ++i)
        staDevices.Add(wifi.Install(phy, mac, nodes.Get(i)));

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, nodes.Get(numNodes - 1));

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    for (uint32_t i = 0; i < numNodes - 1; ++i)
        positionAlloc->Add(Vector(10 * i, 0.0, 0.0));
    positionAlloc->Add(Vector(20.0, 20.0, 0.0)); // Server node
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    NetDeviceContainer allDevices;
    for (uint32_t i = 0; i < staDevices.GetN(); ++i)
        allDevices.Add(staDevices.Get(i));
    allDevices.Add(apDevice.Get(0));
    Ipv4InterfaceContainer interfaces = address.Assign(allDevices);

    // UDP Server on node 4
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(nodes.Get(numNodes - 1));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simTime));

    // UDP Clients on nodes 0-3
    for (uint32_t i = 0; i < numNodes - 1; ++i) {
        UdpClientHelper client(interfaces.GetAddress(numNodes - 1), port);
        client.SetAttribute("MaxPackets", UintegerValue(100));
        client.SetAttribute("Interval", TimeValue(Seconds(0.2 + 0.1 * i)));
        client.SetAttribute("PacketSize", UintegerValue(512 + 16 * i));

        ApplicationContainer clientApp = client.Install(nodes.Get(i));
        clientApp.Start(Seconds(1.0 + 0.2 * i));
        clientApp.Stop(Seconds(simTime));
    }

    // Connect Tx trace from UDP clients
    for (uint32_t i = 0; i < numNodes - 1; ++i) {
        Ptr<Node> n = nodes.Get(i);
        Ptr<Ipv4> ipv4 = n->GetObject<Ipv4>();
        for (uint32_t j = 0; j < n->GetNApplications(); ++j) {
            Ptr<Application> app = n->GetApplication(j);
            Ptr<UdpClient> udpClient = DynamicCast<UdpClient>(app);
            if (udpClient) {
                udpClient->TraceConnectWithoutContext(
                    "Tx", MakeBoundCallback(&TxTrace, i, (uint32_t)(numNodes - 1)));
            }
        }
    }

    // Connect Rx trace from UDP server
    Ptr<Node> srvNode = nodes.Get(numNodes - 1);
    for (uint32_t j = 0; j < srvNode->GetNApplications(); ++j) {
        Ptr<Application> app = srvNode->GetApplication(j);
        Ptr<UdpServer> udpServer = DynamicCast<UdpServer>(app);
        if (udpServer) {
            udpServer->TraceConnectWithoutContext(
                "Rx", MakeBoundCallback(&RxTrace, (uint32_t)(numNodes - 1)));
        }
    }

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    if (csvFile.is_open()) {
        csvFile.close();
    }
    Simulator::Destroy();

    return 0;
}