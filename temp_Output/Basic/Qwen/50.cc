#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiMcsThroughputTest");

class ThroughputSink : public Application {
public:
    static TypeId GetTypeId(void) {
        static TypeId tid = TypeId("ThroughputSink")
                            .SetParent<Application>()
                            .AddConstructor<ThroughputSink>();
        return tid;
    }

    ThroughputSink() : m_totalRx(0) {}
    virtual ~ThroughputSink() {}

    void SetChannelWidth(uint16_t width) { m_channelWidth = width; }
    void SetMcs(uint8_t mcs) { m_mcs = mcs; }
    void SetGuardInterval(uint16_t gi) { m_guardInterval = gi; }

private:
    virtual void StartApplication() {
        if (m_socket == nullptr) {
            TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
            m_socket = Socket::CreateSocket(GetNode(), tid);
            InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 9);
            m_socket->Bind(local);
            m_socket->SetRecvCallback(MakeCallback(&ThroughputSink::Receive, this));
        }
    }

    virtual void StopApplication() {
        if (m_socket) {
            m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
            m_socket->Close();
        }
    }

    void Receive(Ptr<Socket> socket) {
        Ptr<Packet> packet;
        Address from;
        while ((packet = socket->RecvFrom(from))) {
            m_totalRx += packet->GetSize();
        }
    }

    virtual void DoDispose() override {
        m_socket = nullptr;
        Application::DoDispose();
    }

    Ptr<Socket> m_socket;
    uint64_t m_totalRx;
    uint16_t m_channelWidth;
    uint8_t m_mcs;
    uint16_t m_guardInterval;
};

int main(int argc, char *argv[]) {
    uint32_t simulationTime = 10;
    double distance = 1.0;
    std::string phyMode("Spectrum");
    std::string errorModel("nistic");
    uint16_t channelWidth = 20;
    uint16_t guardInterval = 800;
    uint8_t mcsValue = 3;

    CommandLine cmd(__FILE__);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("distance", "Distance between nodes in meters", distance);
    cmd.AddValue("phyMode", "Phy mode: Yans or Spectrum", phyMode);
    cmd.AddValue("errorModel", "Error model: default or nistic", errorModel);
    cmd.AddValue("channelWidth", "Channel width in MHz", channelWidth);
    cmd.AddValue("guardInterval", "Guard interval in ns", guardInterval);
    cmd.AddValue("mcsValue", "MCS index value", mcsValue);
    cmd.Parse(argc, argv);

    NodeContainer wifiStaNode;
    wifiStaNode.Create(1);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    YansWifiPhyHelper phyYans;
    SpectrumWifiPhyHelper phySpectrum;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ac);

    if (phyMode == "Yans") {
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        phyYans.SetChannel(channel.Create());
        wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                     "DataMode", StringValue(wifi.GetPhyLayerType()),
                                     "ControlMode", StringValue(wifi.GetPhyLayerType()));
        wifi.Install(phyYans, wifiStaNode);
        wifi.Install(phyYans, wifiApNode);
    } else {
        phySpectrum = SpectrumWifiPhyHelper::Default();
        Ptr<MultiModelSpectrumChannel> specChannel = CreateObject<MultiModelSpectrumChannel>();
        phySpectrum.SetChannel(specChannel);
        wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                     "DataMode", StringValue("HeMcs" + std::to_string(mcsValue)),
                                     "ControlMode", StringValue("HeMcs" + std::to_string(mcsValue)));
        wifi.Install(phySpectrum, wifiStaNode);
        wifi.Install(phySpectrum, wifiApNode);
    }

    if (errorModel == "default") {
        Config::SetDefault("ns3::WifiPhy::ErrorRateModel", PointerValue(CreateObject<NistErrorRateModel>()));
    } else {
        Config::SetDefault("ns3::WifiPhy::ErrorRateModel", PointerValue(CreateObject<DsssErrorRateModel>()));
    }

    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(999999));
    Config::SetDefault("ns3::WifiRemoteStationManager::FragmentationThreshold", UintegerValue(999999));

    WifiMacHelper mac;
    Ssid ssid = Ssid("throughput-test");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    mac.Install(wifiStaNode);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    mac.Install(wifiApNode);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNode);
    mobility.Install(wifiApNode);

    InternetStackHelper stack;
    stack.Install(wifiStaNode);
    stack.Install(wifiApNode);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterface;
    Ipv4InterfaceContainer apInterface;
    staInterface = address.Assign(wifiStaNode.Get(0)->GetObject<Ipv4>());
    apInterface = address.Assign(wifiApNode.Get(0)->GetObject<Ipv4>());

    UdpServerHelper server(9);
    ApplicationContainer sinkApp = server.Install(wifiStaNode.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simulationTime));

    UdpClientHelper client(staInterface.GetAddress(0), 9);
    client.SetAttribute("MaxPackets", UintegerValue(4294967295));
    client.SetAttribute("Interval", TimeValue(Seconds(0.001)));
    client.SetAttribute("PacketSize", UintegerValue(1472));
    ApplicationContainer clientApp = client.Install(wifiApNode.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(simulationTime));

    ThroughputSink *sink = new ThroughputSink();
    sink->SetChannelWidth(channelWidth);
    sink->SetMcs(mcsValue);
    sink->SetGuardInterval(guardInterval);
    ApplicationContainer customSink;
    customSink.Add(sink);
    customSink.Start(Seconds(0));
    customSink.Stop(Seconds(simulationTime));

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    double throughput = static_cast<double>(sink->m_totalRx * 8) / (simulationTime * 1000000.0);
    std::cout << "MCS=" << static_cast<uint32_t>(mcsValue)
              << ", ChannelWidth=" << channelWidth << "MHz"
              << ", GuardInterval=" << guardInterval << "ns"
              << ", Throughput=" << throughput << " Mbps" << std::endl;

    Simulator::Destroy();
    delete sink;
    return 0;
}