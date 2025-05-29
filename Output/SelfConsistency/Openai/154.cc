#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include <fstream>
#include <iostream>

using namespace ns3;

// Global CSV output stream
std::ofstream gCsvFile;

// Store transmission times (by packet UID)
std::map<uint64_t, double> gTxTimes;

// Helper function to log packet transmission
void
TxTrace(Ptr<const Packet> packet, const Address & address)
{
    uint64_t uid = packet->GetUid();
    double now = Simulator::Now().GetSeconds();
    gTxTimes[uid] = now;
}

// Helper function to log packet reception and write to CSV
void
RxTrace(Ptr<const Packet> packet, const Address & address)
{
    uint64_t uid = packet->GetUid();
    double rxTime = Simulator::Now().GetSeconds();
    auto txIt = gTxTimes.find(uid);
    double txTime = txIt != gTxTimes.end() ? txIt->second : -1.0;

    // For getting IP addresses
    InetSocketAddress srcAddr = InetSocketAddress::ConvertFrom(address);
    std::string srcIp = srcAddr.GetIpv4().ToString();

    uint32_t size = packet->GetSize();

    // The "from" node cannot be directly known in Rx trace, so we'll log via socket/app
    // For this simulation, each client talks to only one server, so we can log app-port
    // For simplicity, output placeholder values: we'll log only UID-based tracking,
    // but below, we'll use application-level arguments to tag packet transmissions.

    uint32_t srcNode = 0; // Will be passed via context
    uint32_t dstNode = 4; // Central server

    // Write to CSV
    gCsvFile << srcNode << "," << dstNode << "," << size << "," << txTime << "," << rxTime << "\n";
}

// Custom Application to tag outgoing packets with node id, to facilitate source logging
class UdpAppWithNodeId : public Application
{
public:
    UdpAppWithNodeId() {}
    virtual ~UdpAppWithNodeId() {}
    void Setup(Ptr<Socket> socket, Address address, uint32_t packetSize,
               uint32_t nPackets, Time pktInterval, uint32_t nodeId)
    {
        m_socket = socket;
        m_peer = address;
        m_packetSize = packetSize;
        m_nPackets = nPackets;
        m_pktInterval = pktInterval;
        m_sent = 0;
        m_nodeId = nodeId;
    }

private:
    virtual void StartApplication(void)
    {
        m_socket->Bind();
        m_socket->Connect(m_peer);
        SendPacket();
    }

    virtual void StopApplication(void)
    {
        if (m_socket)
        {
            m_socket->Close();
        }
    }

    void SendPacket()
    {
        Ptr<Packet> packet = Create<Packet>(m_packetSize);
        m_socket->Send(packet);

        // Log transmit event with explicit source node
        uint64_t uid = packet->GetUid();
        double now = Simulator::Now().GetSeconds();
        gTxTimes[uid] = now;

        // Pass srcNode info for later matching in Rx
        m_uid2NodeId[uid] = m_nodeId;

        ++m_sent;
        if (m_sent < m_nPackets)
        {
            Simulator::Schedule(m_pktInterval, &UdpAppWithNodeId::SendPacket, this);
        }
    }

public:
    // Map accessible to main() to help with UID -> nodeID at reception
    std::map<uint64_t, uint32_t> m_uid2NodeId;

private:
    Ptr<Socket> m_socket;
    Address m_peer;
    uint32_t m_packetSize;
    uint32_t m_nPackets;
    Time m_pktInterval;
    uint32_t m_sent;
    uint32_t m_nodeId;
};

int
main(int argc, char *argv[])
{
    uint32_t nNodes = 5;
    double simulationTime = 10.0; // seconds
    uint32_t packetSize = 512; // bytes
    uint32_t nPackets = 30;
    double interval = 0.3;

    // Open CSV file and write header
    gCsvFile.open("packet_dataset.csv");
    gCsvFile << "SrcNode,DestNode,PacketSize,TxTime,RxTime\n";

    // Create nodes
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(nNodes - 1); // nodes 0-3 (clients)
    NodeContainer wifiApNode;
    wifiApNode.Create(1); // node 4 (server)

    // WiFi channel and devices
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-80211b");

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    // Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
        "MinX", DoubleValue(0.0),
        "MinY", DoubleValue(0.0),
        "DeltaX", DoubleValue(5.0),
        "DeltaY", DoubleValue(5.0),
        "GridWidth", UintegerValue(3),
        "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);
    mobility.Install(wifiApNode);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiStaNodes);
    stack.Install(wifiApNode);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

    // Install UDP server on AP node (node 4)
    uint16_t port = 5000;
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(wifiApNode.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime + 1));

    // Attach Rx trace - after creation of server socket (binds to node 4)
    // Get the UdpServer's underlying socket
    // Instead, log Rx at the NetDevice (promiscuous receive for best generality)
    Ptr<Node> serverNode = wifiApNode.Get(0);
    Ptr<NetDevice> netDev = serverNode->GetDevice(0);
    netDev->TraceConnectWithoutContext("PhyRxEnd", MakeCallback([](Ptr<const Packet> packet) {
        uint64_t uid = packet->GetUid();
        double rxTime = Simulator::Now().GetSeconds();
        auto it = gTxTimes.find(uid);
        double txTime = it != gTxTimes.end() ? it->second : -1.0;

        // Look up source node info (from all UdpAppWithNodeId apps)
        static std::vector<UdpAppWithNodeId*> appsWithId;
        if (appsWithId.empty()) {
            // Populate list: all applications from all nodes
            for (NodeList::Iterator n = NodeList::Begin(); n != NodeList::End(); ++n) {
                Ptr<Node> aNode = *n;
                for (uint32_t i = 0; i < aNode->GetNApplications(); ++i) {
                    Ptr<UdpAppWithNodeId> app = DynamicCast<UdpAppWithNodeId>(aNode->GetApplication(i));
                    if (app) {
                        appsWithId.push_back(app.get());
                    }
                }
            }
        }
        uint32_t srcNode = 0;
        for (auto app : appsWithId) {
            auto search = app->m_uid2NodeId.find(uid);
            if (search != app->m_uid2NodeId.end()) {
                srcNode = search->second;
                break;
            }
        }
        uint32_t dstNode = 4;
        uint32_t size = packet->GetSize();

        gCsvFile << srcNode << "," << dstNode << "," << size << "," << txTime << "," << rxTime << "\n";
    }));

    // Install UDP client applications on all clients
    std::vector<Ptr<UdpAppWithNodeId>> clientApps;
    for (uint32_t i = 0; i < nNodes - 1; ++i)
    {
        Ptr<Node> clientNode = wifiStaNodes.Get(i);

        Ptr<Socket> udpSocket = Socket::CreateSocket(clientNode, UdpSocketFactory::GetTypeId());
        Address serverAddress(InetSocketAddress(apInterface.GetAddress(0), port)); // server is node 4
        Ptr<UdpAppWithNodeId> app = CreateObject<UdpAppWithNodeId>();
        app->Setup(udpSocket, serverAddress, packetSize, nPackets, Seconds(interval), i);
        clientNode->AddApplication(app);
        app->SetStartTime(Seconds(1.0));
        app->SetStopTime(Seconds(simulationTime));
        clientApps.push_back(app);
    }

    phy.EnablePcap("wifi-packet-transmission", apDevice.Get(0));

    Simulator::Stop(Seconds(simulationTime + 1));
    Simulator::Run();

    gCsvFile.close();
    Simulator::Destroy();

    return 0;
}