#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiInterferenceSimulation");

class InterfererHelper : public SpectrumPhy {
public:
    static TypeId GetTypeId();
    InterfererHelper(Ptr<SpectrumChannel> channel, double txPowerW);
    virtual void SetDevice(Ptr<NetDevice>);
    virtual void SetMobility(Ptr<MobilityModel>);
    virtual void StartTx(Ptr<Packet> p);

private:
    Ptr<MobilityModel> m_mobility;
    Ptr<AntennaModel> m_antenna;
    double m_txPowerW;
};

TypeId
InterfererHelper::GetTypeId() {
    static TypeId tid = TypeId("InterfererHelper")
                            .SetParent<SpectrumPhy>()
                            .AddConstructor<InterfererHelper>();
    return tid;
}

InterfererHelper::InterfererHelper(Ptr<SpectrumChannel> channel, double txPowerW)
    : m_txPowerW(txPowerW) {
    SetChannel(channel);
    m_antenna = CreateObject<IsotropicAntennaModel>();
    SetAntenna(m_antenna);
}

void InterfererHelper::SetDevice(Ptr<NetDevice>) {}

void InterfererHelper::SetMobility(Ptr<MobilityModel> mobility) {
    m_mobility = mobility;
}

void InterfererHelper::StartTx(Ptr<Packet> p) {
    NS_ASSERT_MSG(false, "This is an interferer that does not send packets via this method");
}

void SendInterference(Ptr<SpectrumValue> txPsd, Time duration, Ptr<InterfererHelper> interferer) {
    interferer->StartTx(ConstantPositionMobilityModel().GetTxSpectrumModel());
}

int main(int argc, char *argv[]) {
    double simTime = 10.0;
    double distance = 5.0;
    uint8_t mcsIndex = 4;
    uint16_t channelWidth = 20;
    bool shortGuardInterval = false;
    std::string wifiStandard = "802.11n";
    std::string phyMode = "OfdmRate24Mbps";
    double waveformPower = -60; // dBm
    bool pcapTracing = false;
    bool useTcp = true;
    std::string errorModelType = "ns3::NistErrorRateModel";

    CommandLine cmd(__FILE__);
    cmd.AddValue("simTime", "Total simulation time (seconds)", simTime);
    cmd.AddValue("distance", "Distance between nodes (meters)", distance);
    cmd.AddValue("mcsIndex", "Wi-Fi Modulation and Coding Scheme index", mcsIndex);
    cmd.AddValue("channelWidth", "Wi-Fi channel width in MHz", channelWidth);
    cmd.AddValue("shortGuardInterval", "Use short guard interval", shortGuardInterval);
    cmd.AddValue("wifiStandard", "Wi-Fi standard (e.g., 802.11n)", wifiStandard);
    cmd.AddValue("waveformPower", "Interfering waveform power in dBm", waveformPower);
    cmd.AddValue("pcapTracing", "Enable PCAP tracing", pcapTracing);
    cmd.AddValue("useTcp", "Use TCP instead of UDP", useTcp);
    cmd.AddValue("errorModel", "Error model type", errorModelType);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode",
                       StringValue(phyMode));
    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("9999"));
    Config::SetDefault("ns3::WifiPhy::ErrorRateModel", StringValue(errorModelType));

    NodeContainer staNodes;
    staNodes.Create(1);

    NodeContainer apNode;
    apNode.Create(1);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(0),
                                  "GridWidth", UintegerValue(1),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);
    mobility.Install(staNodes);

    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiChannel.AddPropagationLoss("ns3::LogDistancePropagationLossModel");
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");

    WifiHelper wifi;
    wifi.SetStandard(WifiStandardsMap.find(wifiStandard)->second);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(wifiChannel.Create(), mac, staNodes);

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevices = wifi.Install(wifiChannel.Create(), mac, apNode);

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevices);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    if (useTcp) {
        PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9));
        ApplicationContainer sinkApp = sink.Install(apNode.Get(0));
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(simTime));

        OnOffHelper client("ns3::TcpSocketFactory", Address(InetSocketAddress(staInterfaces.GetAddress(0), 9)));
        client.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        client.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        client.SetAttribute("DataRate", DataRateValue(DataRate("1000kbps")));
        client.SetAttribute("PacketSize", UintegerValue(1000));

        ApplicationContainer clientApp = client.Install(staNodes.Get(0));
        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(simTime));
    } else {
        UdpServerHelper server;
        ApplicationContainer serverApp = server.Install(apNode.Get(0));
        serverApp.Start(Seconds(0.0));
        serverApp.Stop(Seconds(simTime));

        UdpClientHelper client(staInterfaces.GetAddress(0), 9);
        client.SetAttribute("MaxPackets", UintegerValue(4294967295u));
        client.SetAttribute("Interval", TimeValue(Seconds(0.1)));
        client.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = client.Install(staNodes.Get(0));
        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(simTime));
    }

    if (pcapTracing) {
        wifi.EnablePcap("access-point", apDevices.Get(0));
        wifi.EnablePcap("station", staDevices.Get(0));
    }

    Config::Connect("/NodeList/*/DeviceList/*/Phy/RxTrace", MakeCallback([](std::string path, Ptr<const Packet> packet, uint16_t channelFreqMhz,
                                                                          uint16_t channelNumber, uint32_t rate, bool isShortPreamble, double signalDbm, double noiseDbm) {
        NS_LOG_INFO("RxPacket: Signal=" << signalDbm << "dBm Noise=" << noiseDbm << "dBm SNR=" << (signalDbm - noiseDbm));
    }));

    Config::Connect("/NodeList/*/DeviceList/*/Phy/TxTrace", MakeCallback([](std::string path, Ptr<const Packet> packet, uint16_t channelFreqMhz,
                                                                          uint16_t channelNumber, uint32_t rate, bool isShortPreamble, double powerDbm) {
        NS_LOG_INFO("TxPacket: Power=" << powerDbm << "dBm");
    }));

    Config::Connect("/NodeList/*/ApplicationList/*/SocketList/*/Tx", MakeCallback([](std::string path, Ptr<const Packet> packet, const Address &) {
        NS_LOG_INFO("Sent packet of size " << packet->GetSize());
    }));

    Config::Connect("/NodeList/*/ApplicationList/*/SocketList/*/Rx", MakeCallback([](std::string path, Ptr<const Packet> packet, const Address &, uint64_t bytes, uint8_t ttl) {
        NS_LOG_INFO("Received packet of size " << packet->GetSize());
    }));

    ConfigStore inputConfig;
    inputConfig.ConfigureDefaults();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}