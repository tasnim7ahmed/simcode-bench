#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiAdHocSimulation");

class AdHocPacketSender : public Application {
public:
    static TypeId GetTypeId() {
        static TypeId tid = TypeId("AdHocPacketSender")
                                .SetParent<Application>()
                                .AddConstructor<AdHocPacketSender>();
        return tid;
    }

    AdHocPacketSender() : m_socket(nullptr), m_peer(), m_packetSize(1000) {}
    virtual ~AdHocPacketSender();

protected:
    void DoStart() override {
        m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
        m_socket->Bind();
        m_socket->Connect(m_peer);
        Ptr<Packet> packet = Create<Packet>(m_packetSize);
        m_socket->Send(packet);
        NS_LOG_INFO("Sent packet of size " << m_packetSize << " bytes");
    }

    void DoDispose() override {
        if (m_socket) {
            m_socket->Close();
            m_socket = nullptr;
        }
        Application::DoDispose();
    }

public:
    void SetRemote(Address addr) { m_peer = addr; }
    void SetPacketSize(uint32_t size) { m_packetSize = size; }

private:
    Ptr<Socket> m_socket;
    Address m_peer;
    uint32_t m_packetSize;
};

TypeId AdHocPacketSender::GetTypeId() { return TypeId(); }

int main(int argc, char *argv[]) {
    std::string phyMode("DsssRate1Mbps");
    double rss = -80.0; // dBm
    bool verbose = false;
    bool pcapTracing = false;
    uint32_t packetSize = 1000;
    std::string rtsThresholdStr = "2347";

    CommandLine cmd(__FILE__);
    cmd.AddValue("phyMode", "Wifi Phy mode to use for devices", phyMode);
    cmd.AddValue("rss", "Received Signal Strength (dBm)", rss);
    cmd.AddValue("verbose", "Enable verbose logging", verbose);
    cmd.AddValue("pcap", "Enable PCAP tracing", pcapTracing);
    cmd.AddValue("packetSize", "Size of packet to send", packetSize);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("WifiPhy", LOG_LEVEL_ALL);
        LogComponentEnable("WifiMac", LOG_LEVEL_ALL);
        LogComponentEnable("AdHocPacketSender", LOG_LEVEL_INFO);
    }

    NodeContainer nodes;
    nodes.Create(2);

    YansWifiPhyHelper wifiPhy;
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.Set("RxGain", DoubleValue(0));
    wifiPhy.Set("TxGain", DoubleValue(0));
    wifiPhy.Set("RxNoiseFigure", DoubleValue(10));
    wifiPhy.Set("CcaEdThreshold", DoubleValue(-95));
    wifiPhy.Set("EnergyDetectionThreshold", DoubleValue(rss - 1));
    wifiPhy.Set("FixedRss", BooleanValue(true));
    wifiPhy.Set("FixedRssValue", DoubleValue(rss));
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ip;
    ip.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ip.Assign(devices);

    uint16_t port = 9;
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), port));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("1kbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
    onoff.SetAttribute("MaxBytes", UintegerValue(packetSize));

    ApplicationContainer app = onoff.Install(nodes.Get(0));
    app.Start(Seconds(1.0));
    app.Stop(Seconds(2.0));

    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    app = sink.Install(nodes.Get(1));
    app.Start(Seconds(0.0));

    if (pcapTracing) {
        wifiPhy.EnablePcapAll("wifi-ad-hoc-sim", false);
    }

    Simulator::Stop(Seconds(3.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}