#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiInterferenceSimulation");

class InterfererHelper : public SpectrumInterferenceHelper
{
public:
  static TypeId GetTypeId(void);
};

TypeId
InterfererHelper::GetTypeId(void)
{
  static TypeId tid = TypeId("InterfererHelper")
                          .SetParent<SpectrumInterferenceHelper>()
                          .AddConstructor<InterfererHelper>();
  return tid;
}

int main(int argc, char *argv[])
{
    double simulationTime = 10.0;
    double distance = 10.0;
    uint8_t mcsIndex = 4;
    uint16_t channelWidth = 20;
    bool shortGuardInterval = false;
    double txPowerW = 0.1;
    std::string wifiType = "ns3::YansWifiPhy";
    std::string errorModel = "ns3::NistErrorRateModel";
    bool pcapTracing = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("simulationTime", "Total simulation time (seconds)", simulationTime);
    cmd.AddValue("distance", "Distance between nodes (meters)", distance);
    cmd.AddValue("mcsIndex", "Wi-Fi MCS index", mcsIndex);
    cmd.AddValue("channelWidth", "Channel width in MHz", channelWidth);
    cmd.AddValue("shortGuardInterval", "Use short guard interval", shortGuardInterval);
    cmd.AddValue("txPowerW", "Transmitter power in Watts", txPowerW);
    cmd.AddValue("wifiType", "Wi-Fi PHY type (Yans or Spectrum)", wifiType);
    cmd.AddValue("errorModel", "Error rate model", errorModel);
    cmd.AddValue("pcap", "Enable PCAP tracing", pcapTracing);
    cmd.Parse(argc, argv);

    NodeContainer wifiStaNode;
    wifiStaNode.Create(1);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    YansWifiPhyHelper phy;
    WifiMacHelper mac;
    WifiHelper wifi;

    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("HtMcs" + std::to_string(mcsIndex)),
                                 "ControlMode", StringValue("HtMcs" + std::to_string(mcsIndex)));

    if (wifiType == "ns3::SpectrumWifiPhy")
    {
        Ptr<MultiModelSpectrumChannel> specChan = CreateObject<MultiModelSpectrumChannel>();
        Ptr<FriisPropagationLossModel> lossModel = CreateObject<FriisPropagationLossModel>();
        specChan->AddPropagationLossModel(lossModel);
        Ptr<ConstantSpeedPropagationDelayModel> delayModel = CreateObject<ConstantSpeedPropagationDelayModel>();
        specChan->SetPropagationDelayModel(delayModel);

        phy.SetChannel(specChan);
        phy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
    }
    else
    {
        YansWifiChannelHelper chan = YansWifiChannelHelper::Default();
        phy.SetChannel(chan.Create());
    }

    phy.Set("TxPowerStart", DoubleValue(10 * log10(txPowerW) + 30));
    phy.Set("TxPowerEnd", DoubleValue(10 * log10(txPowerW) + 30));
    phy.Set("RxGain", DoubleValue(0));
    phy.Set("CcaEdThreshold", DoubleValue(-60.0));
    phy.Set("ErrorRateModel", StringValue(errorModel));

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(Ssid("interference-network")),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevice;
    staDevice = wifi.Install(phy, mac, wifiStaNode);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(Ssid("interference-network")));

    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(wifiStaNode);
    mobility.Install(wifiApNode);

    InternetStackHelper stack;
    stack.Install(wifiStaNode);
    stack.Install(wifiApNode);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterface;
    staInterface = address.Assign(staDevice);
    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign(apDevice);

    uint16_t port = 9;
    OnOffHelper onoffUdp("ns3::UdpSocketFactory", InetSocketAddress(apInterface.GetAddress(0), port));
    onoffUdp.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoffUdp.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoffUdp.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    onoffUdp.SetAttribute("PacketSize", UintegerValue(1000));

    ApplicationContainer udpApp = onoffUdp.Install(wifiStaNode);
    udpApp.Start(Seconds(1.0));
    udpApp.Stop(Seconds(simulationTime - 0.1));

    PacketSinkHelper sinkUdp("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkUdpApp = sinkUdp.Install(wifiApNode);
    sinkUdpApp.Start(Seconds(0.0));
    sinkUdpApp.Stop(Seconds(simulationTime));

    OnOffHelper onoffTcp("ns3::TcpSocketFactory", InetSocketAddress(apInterface.GetAddress(0), port + 1));
    onoffTcp.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoffTcp.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoffTcp.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    onoffTcp.SetAttribute("PacketSize", UintegerValue(1000));

    ApplicationContainer tcpApp = onoffTcp.Install(wifiStaNode);
    tcpApp.Start(Seconds(1.0));
    tcpApp.Stop(Seconds(simulationTime - 0.1));

    PacketSinkHelper sinkTcp("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port + 1));
    ApplicationContainer sinkTcpApp = sinkTcp.Install(wifiApNode);
    sinkTcpApp.Start(Seconds(0.0));
    sinkTcpApp.Stop(Seconds(simulationTime));

    if (pcapTracing)
    {
        phy.EnablePcap("wifi-interference-sta", staDevice.Get(0));
        phy.EnablePcap("wifi-interference-ap", apDevice.Get(0));
    }

    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/State/RxOk",
                    MakeCallback([](std::string path, Ptr<const Packet> packet, const WifiConstPsduMap &psdus,
                                    RxSignalInfo rxSignalInfo, WifiTxVector txVector, std::vector<bool> statusPerMpdu) {
                        NS_LOG_UNCOND("Received packet: Power=" << rxSignalInfo.rssi << " dBm, SNR=" << rxSignalInfo.snr << " dB, TXVECTOR=" << txVector);
                    }));

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    uint64_t totalBytesUdp = DynamicCast<PacketSink>(sinkUdpApp.Get(0))->GetTotalRx();
    uint64_t totalBytesTcp = DynamicCast<PacketSink>(sinkTcpApp.Get(0))->GetTotalRx();
    NS_LOG_UNCOND("UDP Throughput: " << totalBytesUdp * 8.0 / simulationTime / 1e6 << " Mbps");
    NS_LOG_UNCOND("TCP Throughput: " << totalBytesTcp * 8.0 / simulationTime / 1e6 << " Mbps");

    Simulator::Destroy();
    return 0;
}