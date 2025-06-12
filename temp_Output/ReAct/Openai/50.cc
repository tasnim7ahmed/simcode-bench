#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiMcsTest");

int
main(int argc, char *argv[])
{
    double simulationTime = 10.0;
    double distance = 10.0;
    std::string phyType = "Yans"; // or Spectrum
    std::string wifiStandard = "802.11ac";
    std::string errorModelType = "Yans";
    std::string dataRate = "";
    uint32_t channelWidth = 20;
    bool shortGuardInterval = false;
    uint32_t payloadSize = 1472;
    std::string apMcs = ""; // e.g., "9"
    std::string staMcs = ""; // e.g., "9"
    uint32_t mcsStart = 0;
    uint32_t mcsEnd = 9;

    CommandLine cmd(__FILE__);
    cmd.AddValue("simulationTime", "Duration of simulation in seconds", simulationTime);
    cmd.AddValue("distance", "Distance between AP and STA (meters)", distance);
    cmd.AddValue("phyType", "Wi-Fi PHY type: Yans or Spectrum", phyType);
    cmd.AddValue("wifiStandard", "Wi-Fi standard (e.g. 802.11ac, 802.11n)", wifiStandard);
    cmd.AddValue("channelWidth", "Wi-Fi channel width (MHz)", channelWidth);
    cmd.AddValue("shortGuardInterval", "Enable Short Guard Interval", shortGuardInterval);
    cmd.AddValue("errorModelType", "Error model type (Yans or Nist)", errorModelType);
    cmd.AddValue("payloadSize", "UDP payload size in bytes", payloadSize);
    cmd.AddValue("mcsStart", "Starting MCS index", mcsStart);
    cmd.AddValue("mcsEnd", "Ending MCS index (inclusive)", mcsEnd);
    cmd.Parse(argc, argv);

    WifiStandard standard = WIFI_STANDARD_80211ac;
    if (wifiStandard == "802.11n") {
        standard = WIFI_STANDARD_80211n;
    } else if (wifiStandard == "802.11ac") {
        standard = WIFI_STANDARD_80211ac;
    } else if (wifiStandard == "802.11ax") {
        standard = WIFI_STANDARD_80211ax;
    }

    NodeContainer wifiStaNode;
    wifiStaNode.Create(1);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    YansWifiChannelHelper yansChannel = YansWifiChannelHelper::Default();
    Ptr<YansWifiChannel> channel = yansChannel.Create();

    Ptr<Channel> spectrumChannel;
    if (phyType == "Spectrum") {
        SpectrumWifiPhyHelper phy = SpectrumWifiPhyHelper::Default();
        SpectrumChannelHelper schan = SpectrumChannelHelper::Default();
        spectrumChannel = schan.Create();
    }

    WifiHelper wifi;
    wifi.SetStandard(standard);

    WifiMacHelper mac;
    NetDeviceContainer staDevices, apDevices;

    Ptr<YansWifiPhy> yansPhy;
    Ptr<SpectrumWifiPhy> specPhy;

    for (uint32_t mcs = mcsStart; mcs <= mcsEnd; ++mcs)
    {
        Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
        positionAlloc->Add(Vector(0.0, 0.0, 0.0));
        positionAlloc->Add(Vector(distance, 0.0, 0.0));

        MobilityHelper mobility;
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.SetPositionAllocator(positionAlloc);

        mobility.Install(wifiStaNode);
        mobility.Install(wifiApNode);

        NetDeviceContainer tempStaDevices;
        NetDeviceContainer tempApDevices;

        // PHY CONFIGURATION
        if (phyType == "Spectrum")
        {
            SpectrumWifiPhyHelper phy = SpectrumWifiPhyHelper::Default();
            SpectrumChannelHelper schan = SpectrumChannelHelper::Default();
            spectrumChannel = schan.Create();
            phy.SetChannel(spectrumChannel);
            phy.Set("ChannelWidth", UintegerValue(channelWidth));
            phy.Set("ShortGuardEnabled", BooleanValue(shortGuardInterval));
            specPhy = DynamicCast<SpectrumWifiPhy>(phy.Create(wifiStaNode.Get(0), CreateObject<WifiPhy>()));
        }
        else // Yans
        {
            YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
            phy.SetChannel(channel);
            phy.Set("ChannelWidth", UintegerValue(channelWidth));
            phy.Set("ShortGuardEnabled", BooleanValue(shortGuardInterval));
            // Error Model
            if (errorModelType == "Nist") {
                phy.SetErrorRateModel("ns3::NistErrorRateModel");
            } else {
                phy.SetErrorRateModel("ns3::YansErrorRateModel");
            }
            
            YansWifiPhyHelper phy1 = phy;
            phy1.SetDeviceType(phy1.STATION); 
            phy1.Set("MuMimoDetection", BooleanValue(false));
            phy1.Set("MuMimoGroupSize", UintegerValue(1));
            tempStaDevices = wifi.Install(phy1, mac, wifiStaNode);
            YansWifiPhyHelper phy2 = phy;
            phy2.SetDeviceType(phy2.ACCESS_POINT);
            phy2.Set("MuMimoDetection", BooleanValue(false));
            phy2.Set("MuMimoGroupSize", UintegerValue(1));
            tempApDevices = wifi.Install(phy2, mac, wifiApNode);
        }

        // MAC CONFIGURATION
        Ssid ssid = Ssid("ns3-wifi");
        mac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false));
        tempStaDevices = wifi.Install(YansWifiPhyHelper::Default(), mac, wifiStaNode);

        mac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid));
        tempApDevices = wifi.Install(YansWifiPhyHelper::Default(), mac, wifiApNode);

        // MCS configuration per device
        std::ostringstream oss;
        oss << mcs;
        std::string mcs_str = oss.str();
        if (standard == WIFI_STANDARD_80211ac)
        {
            Config::Set("/NodeList/0/DeviceList/0/$ns3::WifiNetDevice/Phy/TxMcs", UintegerValue(mcs));
            Config::Set("/NodeList/1/DeviceList/0/$ns3::WifiNetDevice/Phy/TxMcs", UintegerValue(mcs));
        }
        else
        {
            Config::Set("/NodeList/0/DeviceList/0/$ns3::WifiNetDevice/Phy/TxMcs", UintegerValue(mcs));
            Config::Set("/NodeList/1/DeviceList/0/$ns3::WifiNetDevice/Phy/TxMcs", UintegerValue(mcs));
        }

        // Internet stack
        InternetStackHelper stack;
        stack.Install(wifiApNode);
        stack.Install(wifiStaNode);

        Ipv4AddressHelper address;
        address.SetBase("192.168.1.0", "255.255.255.0");

        Ipv4InterfaceContainer staInterface, apInterface;
        staInterface = address.Assign(tempStaDevices);
        apInterface = address.Assign(tempApDevices);

        // UDP App: from STA to AP
        uint16_t port = 9;
        UdpServerHelper server(port);
        ApplicationContainer serverApp = server.Install(wifiApNode.Get(0));
        serverApp.Start(Seconds(0.0));
        serverApp.Stop(Seconds(simulationTime));

        UdpClientHelper client(apInterface.GetAddress(0), port);
        client.SetAttribute("MaxPackets", UintegerValue(4294967295u));
        client.SetAttribute("Interval", TimeValue(MicroSeconds(100)));
        client.SetAttribute("PacketSize", UintegerValue(payloadSize));
        ApplicationContainer clientApp = client.Install(wifiStaNode.Get(0));
        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(simulationTime));

        FlowMonitorHelper flowmon;
        Ptr<FlowMonitor> monitor = flowmon.InstallAll();

        Simulator::Stop(Seconds(simulationTime));
        Simulator::Run();

        uint64_t rxBytes = 0;
        double throughput = 0;
        Ptr<UdpServer> udpServerApp = DynamicCast<UdpServer>(serverApp.Get(0));
        if (udpServerApp)
        {
            rxBytes = udpServerApp->GetReceived() * payloadSize;
            throughput = (rxBytes * 8.0) / (simulationTime - 1.0) / 1e6; // omit 1s app start
        }

        std::cout << "MCS Index: " << mcs
                  << ", Channel Width: " << channelWidth << " MHz"
                  << ", Short GI: " << (shortGuardInterval ? "Yes" : "No")
                  << ", Throughput: " << throughput << " Mbps" << std::endl;

        Simulator::Destroy();
    }

    return 0;
}