#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("PacketDatasetSimulation");

class PacketLogger {
public:
    PacketLogger(const std::string& filename) {
        m_ofstream.open(filename);
        m_ofstream << "Source,Destination,Size,TransmissionTime,ReceptionTime\n";
    }

    ~PacketLogger() {
        m_ofstream.close();
    }

    void LogTx(Ptr<const Packet> packet, const Address &from) {
        Ipv4Header ipHeader;
        packet->PeekHeader(ipHeader);
        Ipv4Address src = ipHeader.GetSource();
        Ipv4Address dst = ipHeader.GetDestination();

        uint32_t size = packet->GetSize();
        double txTime = Simulator::Now().GetSeconds();

        std::map<Ptr<Socket>, uint32_t>::iterator it = m_txMap.find(Ptr<Socket>(nullptr)); // dummy key
        if (it == m_txMap.end()) {
            m_txMap[ptr] = size;
        } else {
            it->second = size;
        }
        m_ofstream << GetNodeId(src) << "," << GetNodeId(dst) << "," << size << "," << txTime << ",\n";
    }

    void LogRx(Ptr<const Packet> packet, const Address &) {
        Ipv4Header ipHeader;
        packet->PeekHeader(ipHeader);
        Ipv4Address src = ipHeader.GetSource();
        Ipv4Address dst = ipHeader.GetDestination();

        double rxTime = Simulator::Now().GetSeconds();

        std::map<Ptr<Socket>, uint32_t>::iterator it = m_txMap.find(Ptr<Socket>(nullptr)); // dummy key
        uint32_t size = packet->GetSize(); // fallback

        m_ofstream.seekp(-2, std::ios_base::cur);  // overwrite the comma and newline
        m_ofstream << size << "," << rxTime << "\n";
    }

private:
    std::ofstream m_ofstream;
    std::map<Ptr<Socket>, uint32_t> m_txMap;

    uint32_t GetNodeId(Ipv4Address addr) {
        for (uint32_t i = 0; i < 5; ++i) {
            if (Names::Find<Node>("Node" + std::to_string(i))->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal() == addr) {
                return i;
            }
        }
        return 99;
    }
};

int main(int argc, char *argv[]) {
    std::string csvFile = "packet_dataset.csv";

    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("100"));

    NodeContainer nodes;
    nodes.Create(5);

    for (uint32_t i = 0; i < 5; ++i) {
        Names::Add("Node" + std::to_string(i), nodes.Get(i));
    }

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    mac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    UdpServerHelper server(9);
    ApplicationContainer serverApp = server.Install(nodes.Get(4));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    UdpClientHelper clientHelper(interfaces.GetAddress(4), 9);
    clientHelper.SetAttribute("MaxPackets", UintegerValue(20));
    clientHelper.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < 4; ++i) {
        clientApps.Add(clientHelper.Install(nodes.Get(i)));
    }

    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(20.0),
                                  "DeltaY", DoubleValue(20.0),
                                  "GridWidth", UintegerValue(5),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    PacketLogger logger(csvFile);

    Config::Connect("/NodeList/*/ApplicationList/*/$ns3::UdpClient/Tx", MakeCallback(&PacketLogger::LogTx, &logger));
    Config::Connect("/NodeList/*/ApplicationList/*/$ns3::UdpServer/Rx", MakeCallback(&PacketLogger::LogRx, &logger));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}