#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include <vector>
#include <string>
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiHtTosAggregationExample");

struct FlowData
{
    uint64_t rxBytes;
    Time    lastRxTime;
};

static std::map<uint32_t, FlowData> flowStats;

void
RxCallback(Ptr<const Packet> packet, const Address &address, uint32_t flowId)
{
    uint32_t pktSize = packet->GetSize();
    flowStats[flowId].rxBytes += pktSize;
    flowStats[flowId].lastRxTime = Simulator::Now();
}

int
main(int argc, char *argv[])
{
    uint32_t nSta = 4;
    uint8_t mcs = 7;
    uint32_t channelWidth = 40;
    std::string guardInterval = "Short";
    double distance = 15.0;
    bool useRtsCts = false;

    CommandLine cmd;
    cmd.AddValue("nSta", "Number of WiFi stations", nSta);
    cmd.AddValue("mcs", "HT MCS index (0-7)", mcs);
    cmd.AddValue("channelWidth", "Channel width in MHz (20 or 40)", channelWidth);
    cmd.AddValue("guardInterval", "Guard interval: Short or Long", guardInterval);
    cmd.AddValue("distance", "Distance between AP and each STA (meters)", distance);
    cmd.AddValue("useRtsCts", "Enable RTS/CTS for data frames", useRtsCts);
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer wifiStaNodes, wifiApNode;
    wifiStaNodes.Create(nSta);
    wifiApNode.Create(1);

    // Configure PHY and channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());
    phy.Set("ChannelWidth", UintegerValue(channelWidth));
    phy.Set("ShortGuardEnabled", BooleanValue(guardInterval == "Short" || guardInterval == "short"));

    // Configure WiFi (IEEE 802.11n HT 2.4GHz)
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n_2_4GHZ);

    WifiMacHelper mac;
    Ssid ssid = Ssid("wifi-ht-tos-example");

    // HT configuration
    HtWifiMacHelper htMac;
    WifiMode mcsMode = WifiMode("HtMcs" + std::to_string(mcs));
    WifiRemoteStationManagerHelper manager;
    manager.Set("DataMode", StringValue(mcsMode.GetUniqueName()));
    manager.Set("ControlMode", StringValue("HtMcs0"));
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue(mcsMode.GetUniqueName()),
                                 "ControlMode", StringValue("HtMcs0"),
                                 "RtsCtsThreshold", UintegerValue(useRtsCts ? 0 : 2347));

    // Install on stations
    NetDeviceContainer staDevices;
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    staDevices = wifi.Install(phy, mac, wifiStaNodes);

    // Install on AP
    NetDeviceContainer apDevice;
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    apDevice = wifi.Install(phy, mac, wifiApNode);

    // Mobility
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();

    // AP at origin
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    for (uint32_t i = 0; i < nSta; ++i)
    {
        double angle = (i * 2.0 * M_PI) / static_cast<double>(nSta);
        double x = std::cos(angle) * distance;
        double y = std::sin(angle) * distance;
        positionAlloc->Add(Vector(x, y, 0.0));
    }

    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);
    mobility.Install(wifiStaNodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces, apInterface;
    staInterfaces = address.Assign(staDevices);
    apInterface = address.Assign(apDevice);

    // Application: each station sends UDP to AP with different TOS
    uint16_t port_base = 9000;
    double appStart = 1.0, appStop = 11.0;
    std::vector<ApplicationContainer> clients, servers;

    // Use 4 different TOS (DiffServ Codepoint): 0x00, 0x20, 0x40, 0xb8 (BE, CS1, CS2, EF)
    std::vector<uint8_t> tosValues = {0x00, 0x20, 0x40, 0xb8};
    uint32_t nFlows = nSta;
    if (nFlows > tosValues.size()) nFlows = tosValues.size();

    // Create UDP servers at AP for each station
    for (uint32_t i = 0; i < nSta; ++i)
    {
        uint16_t port = port_base + i;
        UdpServerHelper server(port);
        servers.push_back(server.Install(wifiApNode.Get(0)));
        servers.back().Start(Seconds(appStart));
        servers.back().Stop(Seconds(appStop));

        // Install UDP client on STA
        UdpClientHelper client(apInterface.GetAddress(0), port);
        client.SetAttribute("MaxPackets", UintegerValue(1000000));
        client.SetAttribute("Interval", TimeValue(MilliSeconds(1)));
        client.SetAttribute("PacketSize", UintegerValue(1200));
        client.SetAttribute("TypeOfService", UintegerValue(tosValues[i % tosValues.size()]));

        clients.push_back(client.Install(wifiStaNodes.Get(i)));
        clients.back().Start(Seconds(appStart + 0.1));
        clients.back().Stop(Seconds(appStop));
    }

    // Throughput statistics
    std::vector<Ptr<UdpServer>> serverApps;
    for (uint32_t i = 0; i < servers.size(); ++i) {
        serverApps.push_back(DynamicCast<UdpServer>(servers[i].Get(0)));
    }

    Simulator::Stop(Seconds(appStop + 0.5));
    Simulator::Run();

    double totalRxBytes = 0.0;
    double duration = appStop - appStart;

    std::cout << "Flow Summary per TOS:" << std::endl;
    for (uint32_t i = 0; i < nSta; ++i)
    {
        uint64_t rx = serverApps[i]->GetReceived();
        // Each packet is 1200 bytes
        uint64_t rxBytes = rx * 1200;
        totalRxBytes += rxBytes;
        double throughput = (rxBytes * 8.0) / 1e6 / duration;
        std::cout << "  STA " << i
                  << "  TOS=0x" << std::hex << std::setfill('0') << std::setw(2) << (uint32_t)tosValues[i % tosValues.size()]
                  << std::dec << "  RX=" << rxBytes
                  << " bytes  Throughput=" << std::fixed << std::setprecision(3) << throughput << " Mbps"
                  << std::endl;
    }
    double aggThroughput = (totalRxBytes * 8.0) / 1e6 / duration;
    std::cout << "Aggregated UDP Throughput: " << aggThroughput << " Mbps" << std::endl;

    Simulator::Destroy();
    return 0;
}