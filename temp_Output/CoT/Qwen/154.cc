#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessNetworkSimulation");

class PacketLogger {
public:
    PacketLogger(const std::string& filename) {
        m_ofstream.open(filename);
        m_ofstream << "Source,Destination,PacketSize,TransmissionTime,ReceptionTime\n";
    }

    ~PacketLogger() {
        m_ofstream.close();
    }

    void Log(Packet const* packet, const Address &from, const Address &to, double txTime, double rxTime) {
        std::ostringstream oss;
        Ipv4Address fromIp, toIp;
        if (InetSocketAddress::IsMatchingType(from)) {
            fromIp = InetSocketAddress::ConvertFrom(from).GetIpv4();
        } else {
            return;
        }
        if (InetSocketAddress::IsMatchingType(to)) {
            toIp = InetSocketAddress::ConvertFrom(to).GetIpv4();
        } else {
            return;
        }

        int16_t srcNodeId = -1, dstNodeId = -1;
        for (uint32_t i = 0; i < NodeList::GetNNodes(); ++i) {
            Ptr<Node> node = NodeList::GetNode(i);
            Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
            for (uint32_t j = 0; j < ipv4->GetNInterfaces(); ++j) {
                if (ipv4->GetAddress(j, 0).GetLocal() == fromIp) {
                    srcNodeId = node->GetId();
                }
                if (ipv4->GetAddress(j, 0).GetLocal() == toIp) {
                    dstNodeId = node->GetId();
                }
            }
        }

        if (srcNodeId != -1 && dstNodeId != -1) {
            m_ofstream << srcNodeId << "," << dstNodeId << "," << packet->GetSize() << ","
                       << txTime << "," << rxTime << "\n";
        }
    }

private:
    std::ofstream m_ofstream;
};

void
RxTrace(Ptr<PacketLogger> logger, Ptr<const Packet> packet, const Address &from, const Address &to)
{
    static auto lastRxTime = 0.0;
    auto txIt = Config::LookupGlobalValue("Simulator::Now", DoubleValue(0.0));
    double txTime = txIt.GetDouble();
    double rxTime = Simulator::Now().GetSeconds();
    logger->Log(packet, from, to, txTime, rxTime);
}

int main(int argc, char *argv[]) {
    std::string csvFile = "packet_data.csv";
    double simulationTime = 10.0;

    NodeContainer nodes;
    nodes.Create(5);

    YansWifiPhyHelper phy;
    WifiHelper wifi;
    WifiMacHelper mac;

    phy.Set("ChannelWidth", UintegerValue(20));
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    mac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(20.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(5),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 9;
    PacketLogger packetLogger(csvFile);

    // Server on node 4
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(4));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime));

    // Clients on other nodes sending to node 4
    Config::SetGlobal("Simulator::Now", DoubleValue(0.0));
    for (uint32_t i = 0; i < 4; ++i) {
        UdpEchoClientHelper echoClient(interfaces.GetAddress(4), port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(100));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
        echoClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApps = echoClient.Install(nodes.Get(i));
        clientApps.Start(Seconds(1.0));
        clientApps.Stop(Seconds(simulationTime));
    }

    // Hook into the socket RX of the server
    Config::ConnectWithoutContext("/NodeList/4/ApplicationList/0/$ns3::UdpEchoServer/Rx", MakeBoundCallback(&RxTrace, &packetLogger));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}