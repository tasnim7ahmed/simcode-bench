#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/tcp-header.h"
#include "ns3/ipv4-header.h"

using namespace ns3;

std::ofstream g_csvFile;

void PacketLogger(Ptr<const Packet> packet, const Address &from, const Address &to) {
    static uint32_t packetId = 0;
    Ipv4Header ipHeader;
    TcpHeader tcpHeader;
    uint8_t protocol = 0;
    packet->PeekHeader(ipHeader);
    packet->RemoveHeader(tcpHeader); // To get TCP header (after IP)
    Address srcAddr = from;
    Address dstAddr = to;
    std::string srcIp = Ipv4Address::ConvertFrom(ipHeader.GetSource()).SerializeToString();
    std::string dstIp = Ipv4Address::ConvertFrom(ipHeader.GetDestination()).SerializeToString();
    uint16_t srcPort = 0, dstPort = 0;

    if (InetSocketAddress::IsMatchingType(from)) {
        srcPort = InetSocketAddress::ConvertFrom(from).GetPort();
    }
    if (InetSocketAddress::IsMatchingType(to)) {
        dstPort = InetSocketAddress::ConvertFrom(to).GetPort();
    }

    g_csvFile << packetId++ << ","
              << srcIp << ":" << srcPort << ","
              << dstIp << ":" << dstPort << ","
              << packet->GetSize() << ","
              << Simulator::Now().GetSeconds() << ","
              << "\n";
}

int main(int argc, char *argv[]) {
    // Open CSV file and write header
    g_csvFile.open("star_topology_dataset.csv");
    g_csvFile << "PacketID,SrcIP:Port,DstIP:Port,Size,TransmissionTime,ReceptionTime\n";

    // Create a star topology with 5 nodes (node 0 is central)
    NodeContainer centralNode;
    centralNode.Create(1);

    NodeContainer peripheralNodes;
    peripheralNodes.Create(4);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("5ms"));

    NetDeviceContainer devices[4];
    for (int i = 0; i < 4; ++i) {
        NodeContainer pair = NodeContainer(centralNode, peripheralNodes.Get(i));
        devices[i] = p2p.Install(pair);
    }

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfaces[4];

    for (int i = 0; i < 4; ++i) {
        std::ostringstream subnet;
        subnet << "10.0." << i + 1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfaces[i] = address.Assign(devices[i]);
    }

    uint16_t port = 5000;

    // Install sink applications on peripheral nodes (TCP servers)
    for (uint32_t i = 0; i < 4; ++i) {
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory",
                                    InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer sinkApp = sinkHelper.Install(peripheralNodes.Get(i));
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(10.0));
    }

    // Install client applications on central node (TCP clients)
    for (uint32_t i = 0; i < 4; ++i) {
        BulkSendHelper clientHelper("ns3::TcpSocketFactory",
                                    InetSocketAddress(interfaces[i].GetAddress(1), port));
        clientHelper.SetAttribute("MaxBytes", UintegerValue(1024 * 1024)); // Send 1 MB of data
        ApplicationContainer clientApp = clientHelper.Install(centralNode.Get(0));
        clientApp.Start(Seconds(1.0 + i * 0.5)); // Staggered start times
        clientApp.Stop(Seconds(10.0));
    }

    // Capture packets on the central node's outgoing interfaces
    for (int i = 0; i < 4; ++i) {
        centralNode.Get(0)->GetDevice(i)->TraceConnectWithoutContext("PhyTxEnd", MakeCallback(&PacketLogger));
    }

    // Run simulation
    Simulator::Run();
    Simulator::Destroy();
    g_csvFile.close();
    return 0;
}