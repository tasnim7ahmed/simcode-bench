#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/udp-server-client-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/config.h"
#include "ns3/string.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("PacketDatasetSimulation");

class PacketLogger {
public:
    PacketLogger(const std::string& filename) {
        m_file.open(filename);
        m_file << "Source,Destination,Size,TransmissionTime,ReceptionTime\n";
    }

    ~PacketLogger() {
        if (m_file.is_open()) {
            m_file.close();
        }
    }

    void Log(uint32_t src, uint32_t dst, uint32_t size, double tx, double rx) {
        m_file << src << "," << dst << "," << size << "," << tx << "," << rx << "\n";
    }

private:
    std::ofstream m_file;
};

static void TxTrace(Ptr<Packet> packet, const Address& from, const Address& to, uint32_t srcId, uint32_t dstId, PacketLogger* logger) {
    auto it = logger->m_pendingPackets.find(packet);
    if (it == logger->m_pendingPackets.end()) {
        logger->m_pendingPackets[packet] = std::make_pair(srcId, Simulator::Now().GetSeconds());
    }
}

static void RxTrace(Ptr<Packet> packet, const Address& from, const Address& to, uint32_t srcId, uint32_t dstId, PacketLogger* logger) {
    auto it = logger->m_pendingPackets.find(packet);
    if (it != logger->m_pendingPackets.end()) {
        uint32_t size = packet->GetSize();
        double tx = it->second.second;
        double rx = Simulator::Now().GetSeconds();
        logger->Log(it->second.first, dstId, size, tx, rx);
        logger->m_pendingPackets.erase(it);
    }
}

int main(int argc, char *argv[]) {
    std::string csvFile = "packet_dataset.csv";

    NodeContainer nodes;
    nodes.Create(5);

    YansWifiPhyHelper phy;
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("OfdmRate6Mbps"));

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(5),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    PacketLogger logger(csvFile);

    ApplicationContainer serverApps;
    UdpServerHelper server(9);
    serverApps.Add(server.Install(nodes.Get(4)));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    for (uint32_t i = 0; i < 4; ++i) {
        UdpClientHelper client(interfaces.GetAddress(4), 9);
        client.SetAttribute("MaxPackets", UintegerValue(10));
        client.SetAttribute("Interval", TimeValue(Seconds(0.5)));
        client.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = client.Install(nodes.Get(i));
        clientApp.Start(Seconds(2.0 + i * 0.5));
        clientApp.Stop(Seconds(10.0));

        Config::Connect("/NodeList/"+std::to_string(i)+"/ApplicationList/0/Tx", MakeBoundCallback(&TxTrace, &logger));
        Config::Connect("/NodeList/"+std::to_string(i)+"/ApplicationList/0/Rx", MakeBoundCallback(&RxTrace, &logger));
    }

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}