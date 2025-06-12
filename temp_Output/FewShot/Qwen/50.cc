#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiMcsThroughputSimulation");

int main(int argc, char *argv[]) {
    uint32_t payloadSize = 1472; // UDP payload size in bytes
    Time simulationTime = Seconds(10);
    double distance = 1.0; // meters
    std::string phyType = "Yans";
    std::string errorModel = "ns3::NistErrorRateModel";
    bool enableQos = false;
    uint16_t channelWidth = 20;
    uint8_t mcsIndex = 3;
    uint32_t maxMcsIndex = 7;
    uint16_t guardInterval = 800; // nanoseconds

    CommandLine cmd(__FILE__);
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("simulationTime", "Total simulation time", simulationTime);
    cmd.AddValue("distance", "Distance between nodes (m)", distance);
    cmd.AddValue("phyType", "Phy type: Yans or Spectrum", phyType);
    cmd.AddValue("errorModel", "Error model type", errorModel);
    cmd.AddValue("enableQos", "Enable QoS (for HT/VHT)", enableQos);
    cmd.AddValue("channelWidth", "Channel width (MHz)", channelWidth);
    cmd.AddValue("guardInterval", "Guard interval (ns)", guardInterval);
    cmd.Parse(argc, argv);

    // Set default MCS values for throughput test
    NodeContainer wifiStaNode;
    wifiStaNode.Create(1);
    Ptr<Node> apNode = CreateObject<Node>();

    NodeContainer staNode;
    staNode.Add(wifiStaNode.Get(0));
    staNode.Add(apNode);

    // Setup mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(staNode);

    // Setup Wi-Fi
    WifiMacHelper wifiMac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("HtMcs" + std::to_string(mcsIndex)),
                                 "ControlMode", StringValue("HtMcs0"));

    if (phyType == "Spectrum") {
        wifi.SetPhy("ns3::SpectrumWifiPhy",
                    "ChannelSettings", StringValue("{0, 0, 0, 0}"),
                    "TxPowerStart", DoubleValue(10),
                    "TxPowerEnd", DoubleValue(10),
                    "TxPowerLevels", UintegerValue(1),
                    "RxGain", DoubleValue(0),
                    "CcaEdThreshold", DoubleValue(-62),
                    "ErrorRateModel", StringValue(errorModel));
    } else {
        YansWifiPhyHelper wifiPhy;
        wifiPhy.Set("TxPowerStart", DoubleValue(10));
        wifiPhy.Set("TxPowerEnd", DoubleValue(10));
        wifiPhy.Set("TxPowerLevels", UintegerValue(1));
        wifiPhy.Set("RxGain", DoubleValue(0));
        wifiPhy.Set("CcaEdThreshold", DoubleValue(-62));
        wifiPhy.SetErrorRateModel(StringValue(errorModel));
        wifi.SetPhy(wifiPhy);
    }

    wifi.SetChannel(ChannelHelper::Default());

    // Setup MAC
    Ssid ssid = Ssid("wifi-mcs-test");
    wifiMac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid),
                    "BeaconInterval", TimeValue(Seconds(2.5)));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(wifi.GetPhy(), wifiMac, apNode);

    wifiMac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices;
    staDevices = wifi.Install(wifi.GetPhy(), wifiMac, wifiStaNode);

    // Install IP stack
    InternetStackHelper stack;
    stack.Install(staNode);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterface;
    staInterface = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevice);

    // Setup applications
    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(apNode);
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(simulationTime);

    UdpClientHelper client(apInterface.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(100)));
    client.SetAttribute("PacketSize", UintegerValue(payloadSize));

    ApplicationContainer clientApp = client.Install(wifiStaNode.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(simulationTime);

    // Enable ASCII and PCAP tracing
    AsciiTraceHelper ascii;
    wifi.EnableAsciiAll(ascii.CreateFileStream("wifi-mcs-throughput.tr"));
    wifi.EnablePcapAll("wifi-mcs-throughput");

    // Throughput stats
    std::cout << "MCS\tChannelWidth(MHz)\tThroughput(Mbps)\tModulation" << std::endl;

    for (uint32_t mcs = 0; mcs <= maxMcsIndex; ++mcs) {
        Config::Set("/NodeList/0/DeviceList/0/$ns3::WifiNetDevice/RemoteStationManager/DataMode",
                    StringValue("HtMcs" + std::to_string(mcs)));

        Simulator::Run();

        double rxBytes = StaticCast<UdpServer>(serverApp.Get(0))->GetReceived();
        double throughput = (rxBytes * 8) / (simulationTime.GetSeconds() * 1e6);

        WifiMode mode = WifiModeFactory::Get().CreateWifiMode("HtMcs" + std::to_string(mcs));
        std::string modulation = mode.GetModulationType() == WIFI_MOD_CLASS_HT ? "HT" : "Unknown";

        std::cout << (uint32_t)mcs << "\t" << channelWidth << "\t\t" << throughput << "\t\t" << modulation << std::endl;

        // Reset received count
        StaticCast<UdpServer>(serverApp.Get(0))->Reset();
    }

    Simulator::Destroy();
    return 0;
}