#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/config-store.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiMcsThroughputSimulation");

uint64_t g_bytesReceived = 0;

void
UdpRxCallback(Ptr<const Packet> packet, const Address &)
{
    g_bytesReceived += packet->GetSize();
}

int main(int argc, char *argv[])
{
    // Default simulation parameters
    double simulationTime = 10.0;     // seconds
    double distance = 5.0;            // meters
    std::string phyType = "YansWifiPhy"; // "YansWifiPhy" or "SpectrumWifiPhy"
    uint32_t channelWidth = 20;       // MHz
    std::string gi = "long";          // "long" or "short"
    std::string wifiStandard = "802.11ac"; // "802.11n", "802.11ac", etc
    std::string errorModelType = "ns3::NistErrorRateModel";
    uint32_t udpPayloadSize = 1472;   // bytes
    std::string dataRate = "200Mbps"; // Application data rate

    // Accept user parameters from command line
    CommandLine cmd(__FILE__);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("distance", "Distance between STA and AP in meters", distance);
    cmd.AddValue("phyType", "Physical layer type: YansWifiPhy or SpectrumWifiPhy", phyType);
    cmd.AddValue("channelWidth", "Channel width in MHz", channelWidth);
    cmd.AddValue("guardInterval", "Guard interval: long or short", gi);
    cmd.AddValue("wifiStandard", "Wi-Fi standard: 802.11n, 802.11ac, 802.11ax, etc", wifiStandard);
    cmd.AddValue("errorModelType", "Error rate model type", errorModelType);
    cmd.AddValue("udpPayloadSize", "UDP payload size in bytes", udpPayloadSize);
    cmd.AddValue("dataRate", "Application data rate", dataRate);
    cmd.Parse(argc, argv);

    // Map wifiStandard string to WifiStandard enum
    WifiStandard standard = WIFI_STANDARD_80211ac;
    if (wifiStandard == "802.11ac")
        standard = WIFI_STANDARD_80211ac;
    else if (wifiStandard == "802.11n")
        standard = WIFI_STANDARD_80211n;
    else if (wifiStandard == "802.11ax")
        standard = WIFI_STANDARD_80211ax;
    else if (wifiStandard == "802.11a")
        standard = WIFI_STANDARD_80211a;
    else if (wifiStandard == "802.11g")
        standard = WIFI_STANDARD_80211g;
    else if (wifiStandard == "802.11b")
        standard = WIFI_STANDARD_80211b;

    // MCS indices supported for the chosen standard (simplified set for demo)
    std::vector<uint8_t> mcsIndexes;
    if (standard == WIFI_STANDARD_80211n) {
        for (uint8_t mcs = 0; mcs <= 7; ++mcs) mcsIndexes.push_back(mcs); // MCS 0-7
    } else if (standard == WIFI_STANDARD_80211ac || standard == WIFI_STANDARD_80211ax) {
        for (uint8_t mcs = 0; mcs <= 9; ++mcs) mcsIndexes.push_back(mcs); // MCS 0-9
    } else if (standard == WIFI_STANDARD_80211a || standard == WIFI_STANDARD_80211g) {
        for (uint8_t mcs = 0; mcs <= 7; ++mcs) mcsIndexes.push_back(mcs); // Rates encoded as legacy MCS
    } else if (standard == WIFI_STANDARD_80211b) {
        for (uint8_t mcs = 0; mcs < 4; ++mcs) mcsIndexes.push_back(mcs); // 1,2,5.5,11 Mbps
    }

    // Record stats for each MCS
    struct McsResult {
        uint8_t mcsIdx;
        uint32_t channelWidth;
        std::string gi;
        double throughputMbps;
    };
    std::vector<McsResult> results;

    for (uint8_t mcsIdx : mcsIndexes)
    {
        // Reset Bytes RX stats
        g_bytesReceived = 0;

        NodeContainer wifiStaNode, wifiApNode;
        wifiStaNode.Create(1);
        wifiApNode.Create(1);

        // Create Wifi channel/phy helpers
        Ptr<YansWifiChannel> yansChannel;
        Ptr<YansWifiPhy> yansPhy;
        Ptr<SpectrumWifiPhy> spectrumPhy;
        Ptr<WifiChannel> channel;

        WifiHelper wifi;
        wifi.SetStandard(standard);

        // Error rate model
        wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                    "DataMode", StringValue(""),
                                    "ControlMode", StringValue(""));

        NetDeviceContainer apDevice, staDevice;
        Ptr<WifiPhy> phy;

        if (phyType == "YansWifiPhy")
        {
            YansWifiChannelHelper channelHelper = YansWifiChannelHelper::Default();
            yansChannel = channelHelper.Create();
            YansWifiPhyHelper phyHelper = YansWifiPhyHelper::Default();
            phyHelper.SetChannel(yansChannel);
            phyHelper.Set("ChannelWidth", UintegerValue(channelWidth));
            phyHelper.SetErrorRateModel(errorModelType);
            phy = phyHelper.GetPhy(); // NULL, but not used directly below

            WifiMacHelper mac;
            Ssid ssid = Ssid("wifi-mcs-ssid");

            phyHelper.Set("ShortGuardEnabled", BooleanValue(gi == "short"));

            mac.SetType("ns3::StaWifiMac",
                        "Ssid", SsidValue(ssid),
                        "ActiveProbing", BooleanValue(false));
            staDevice = wifi.Install(phyHelper, mac, wifiStaNode);

            mac.SetType("ns3::ApWifiMac",
                        "Ssid", SsidValue(ssid));
            apDevice = wifi.Install(phyHelper, mac, wifiApNode);
        }
        else // SpectrumWifiPhy
        {
            WifiSpectrumChannelHelper channelHelper = WifiSpectrumChannelHelper::Default();
            channel = channelHelper.Create();
            SpectrumWifiPhyHelper phyHelper = SpectrumWifiPhyHelper::Default();
            phyHelper.SetChannel(channel);
            phyHelper.Set("ChannelWidth", UintegerValue(channelWidth));
            phyHelper.SetErrorRateModel(errorModelType);
            phyHelper.Set("ShortGuardEnabled", BooleanValue(gi == "short"));

            WifiMacHelper mac;
            Ssid ssid = Ssid("wifi-mcs-ssid");

            mac.SetType("ns3::StaWifiMac",
                        "Ssid", SsidValue(ssid),
                        "ActiveProbing", BooleanValue(false));
            staDevice = wifi.Install(phyHelper, mac, wifiStaNode);

            mac.SetType("ns3::ApWifiMac",
                        "Ssid", SsidValue(ssid));
            apDevice = wifi.Install(phyHelper, mac, wifiApNode);
        }

        // Set the MCS/configure rates
        std::ostringstream mode;
        if (standard == WIFI_STANDARD_80211n) {
            mode << "HtMcs" << unsigned(mcsIdx);
        } else if (standard == WIFI_STANDARD_80211ac) {
            mode << "VhtMcs" << unsigned(mcsIdx);
        } else if (standard == WIFI_STANDARD_80211ax) {
            mode << "HeMcs" << unsigned(mcsIdx);
        } else if (standard == WIFI_STANDARD_80211a || standard == WIFI_STANDARD_80211g) {
            static const char *legacyRates[] = {"OfdmRate6Mbps", "OfdmRate9Mbps", "OfdmRate12Mbps","OfdmRate18Mbps","OfdmRate24Mbps","OfdmRate36Mbps","OfdmRate48Mbps","OfdmRate54Mbps"};
            mode << legacyRates[std::min<int>(mcsIdx,7)];
        } else if (standard == WIFI_STANDARD_80211b) {
            static const char *legacyRates[] = {"DsssRate1Mbps", "DsssRate2Mbps", "DsssRate5_5Mbps", "DsssRate11Mbps"};
            mode << legacyRates[std::min<int>(mcsIdx,3)];
        }

        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/DataMode",
                    StringValue(mode.str()));
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/RemoteStationManager/ControlMode",
                    StringValue(mode.str()));

        // Mobility model: STA at (0,0,0), AP at (distance,0,0)
        MobilityHelper mobility;
        Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
        positionAlloc->Add(Vector(0.0, 0.0, 0.0));                  // STA
        positionAlloc->Add(Vector(distance, 0.0, 0.0));              // AP
        mobility.SetPositionAllocator(positionAlloc);
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(wifiStaNode);
        mobility.Install(wifiApNode);

        // Install TCP/IP stack
        InternetStackHelper stack;
        stack.Install(wifiStaNode);
        stack.Install(wifiApNode);

        // Assign IP addresses
        Ipv4AddressHelper address;
        address.SetBase("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer staIf, apIf;
        staIf = address.Assign(staDevice);
        apIf  = address.Assign(apDevice);

        // UDP application: send UDP packets from STA to AP
        uint16_t port = 5000;
        // UDP Server (receiver on AP)
        UdpServerHelper server(port);
        ApplicationContainer serverApp = server.Install(wifiApNode.Get(0));
        serverApp.Start(Seconds(0.0));
        serverApp.Stop(Seconds(simulationTime + 1));

        // UDP Client (transmitter on STA)
        UdpClientHelper client(apIf.GetAddress(0), port);
        client.SetAttribute("MaxPackets", UintegerValue(4294967295u));
        client.SetAttribute("PacketSize", UintegerValue(udpPayloadSize));
        client.SetAttribute("Interval", TimeValue(Seconds((double)udpPayloadSize * 8 / std::stod(dataRate.substr(0, dataRate.find("M"))) / 1e6)));
        client.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
        client.SetAttribute("StopTime", TimeValue(Seconds(simulationTime)));

        ApplicationContainer clientApp = client.Install(wifiStaNode.Get(0));
        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(simulationTime));

        // Connect to Rx trace for throughput
        g_bytesReceived = 0;
        Ptr<UdpServer> udpServer = DynamicCast<UdpServer>(serverApp.Get(0));
        Config::ConnectWithoutContext(
            "/NodeList/" + std::to_string(wifiApNode.Get(0)->GetId()) + "/ApplicationList/0/$ns3::UdpServer/Rx",
            MakeCallback(&UdpRxCallback));

        Simulator::Stop(Seconds(simulationTime + 1));

        Simulator::Run();

        double throughput = g_bytesReceived * 8.0 / (simulationTime - 1.0) / 1e6; // Mbps

        McsResult result;
        result.mcsIdx = mcsIdx;
        result.channelWidth = channelWidth;
        result.gi = gi;
        result.throughputMbps = throughput;
        results.push_back(result);

        Simulator::Destroy();

        Config::Reset();
    }

    // Output results
    std::cout << "MCS/Rate,ChannelWidth,GuardInterval,Throughput(Mbps)" << std::endl;
    for (const auto &r : results) {
        std::cout << unsigned(r.mcsIdx) << "," << r.channelWidth << "," << r.gi << "," << r.throughputMbps << std::endl;
    }

    return 0;
}