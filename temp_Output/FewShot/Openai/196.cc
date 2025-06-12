#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/applications-module.h"
#include "ns3/ssid.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiUdpStatsExample");

class StaStats : public Object
{
public:
    StaStats() : m_rxPackets(0), m_rxBytes(0) {}

    void RxCallback(Ptr<const Packet> packet, const Address &)
    {
        m_rxPackets++;
        m_rxBytes += packet->GetSize();
    }

    uint32_t GetRxPackets() const { return m_rxPackets; }
    uint64_t GetRxBytes() const { return m_rxBytes; }

private:
    uint32_t m_rxPackets;
    uint64_t m_rxBytes;
};

void
PrintStaThroughput(Ptr<StaStats> stats, double appStart, double appStop, uint32_t staId)
{
    double duration = appStop - appStart;
    double throughput = (stats->GetRxBytes() * 8.0) / duration / 1e6; // Mbps
    std::cout << "STA" << staId << "   Received Packets: " << stats->GetRxPackets()
              << ", Received Bytes: " << stats->GetRxBytes()
              << ", Throughput: " << throughput << " Mbps" << std::endl;
}

int main(int argc, char *argv[])
{
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    uint32_t nSta = 2;
    double simulationTime = 10.0;
    double appStart = 1.0;
    double appStop = 9.0;

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(nSta);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ac);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("VhtMcs7"), "ControlMode", StringValue("VhtMcs0"));

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-80211ac");

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevices = wifi.Install(phy, mac, wifiApNode);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(5.0),
                                 "DeltaY", DoubleValue(10.0),
                                 "GridWidth", UintegerValue(3),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    mobility.Install(wifiStaNodes);
    mobility.Install(wifiApNode);

    InternetStackHelper stack;
    stack.Install(wifiStaNodes);
    stack.Install(wifiApNode);

    Ipv4AddressHelper address;
    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevices);

    // UDP Server on AP (for each station)
    uint16_t basePort = 9000;
    ApplicationContainer serverApps;
    for (uint32_t i = 0; i < nSta; ++i)
    {
        UdpServerHelper server(basePort + i);
        ApplicationContainer serverApp = server.Install(wifiApNode.Get(0));
        serverApp.Start(Seconds(appStart));
        serverApp.Stop(Seconds(appStop));
        serverApps.Add(serverApp);
    }

    // UDP Client on each STA, targeting AP
    ApplicationContainer clientApps;
    std::vector<Ptr<StaStats>> staStats(nSta);

    for (uint32_t i = 0; i < nSta; ++i)
    {
        UdpClientHelper client(apInterface.GetAddress(0), basePort + i);
        client.SetAttribute("MaxPackets", UintegerValue(1000));
        client.SetAttribute("Interval", TimeValue(Seconds(0.01)));
        client.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApp = client.Install(wifiStaNodes.Get(i));
        clientApp.Start(Seconds(appStart));
        clientApp.Stop(Seconds(appStop));
        clientApps.Add(clientApp);

        // Set up statistics gathering (listen for server response)
        staStats[i] = CreateObject<StaStats>();
        Ptr<UdpServer> udpServer = DynamicCast<UdpServer>(serverApps.Get(i));
        if (udpServer)
        {
            udpServer->TraceConnectWithoutContext("Rx", MakeCallback(&StaStats::RxCallback, staStats[i]));
        }
    }

    // Enable pcap tracing
    phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    phy.EnablePcap("wifi-ap", apDevices.Get(0), true);
    for (uint32_t i = 0; i < staDevices.GetN(); ++i)
    {
        phy.EnablePcap("wifi-sta" + std::to_string(i), staDevices.Get(i), true);
    }

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Print stats and throughput
    std::cout << "==== Per-STA Results ====" << std::endl;
    for (uint32_t i = 0; i < nSta; ++i)
    {
        PrintStaThroughput(staStats[i], appStart, appStop, i);
    }

    Simulator::Destroy();
    return 0;
}