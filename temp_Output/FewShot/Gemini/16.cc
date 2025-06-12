#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/gnuplot.h"
#include "ns3/command-line.h"
#include "ns3/log.h"
#include "ns3/packet-socket-factory.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdhocWifiExperiment");

class Experiment {
public:
    Experiment();
    ~Experiment();

    void SetPosition(uint32_t nodeIndex, Vector position);
    Vector GetPosition(uint32_t nodeIndex);
    void MoveNode(uint32_t nodeIndex, Vector newPosition);
    void ReceivePacket(Ptr<Socket> socket);
    void SetupPacketReceptionSocket(Ptr<Node> node);
    void Run(std::string wifiRate, std::string rtsCtsThreshold, std::string fragmentationThreshold, std::string managerName, std::string experimentName);

private:
    NodeContainer nodes;
    NetDeviceContainer devices;
    Ipv4InterfaceContainer interfaces;
    ApplicationContainer appContainer;
    std::map<uint32_t, Vector> nodePositions;
    std::string currentExperimentName;
};

Experiment::Experiment() {}

Experiment::~Experiment() {}

void Experiment::SetPosition(uint32_t nodeIndex, Vector position) {
    nodePositions[nodeIndex] = position;
}

Vector Experiment::GetPosition(uint32_t nodeIndex) {
    return nodePositions[nodeIndex];
}

void Experiment::MoveNode(uint32_t nodeIndex, Vector newPosition) {
    nodePositions[nodeIndex] = newPosition;
    Ptr<Node> node = nodes.Get(nodeIndex);
    Ptr<MobilityModel> mobility = node->GetObject<MobilityModel>();
    mobility->SetPosition(newPosition);
}

void Experiment::ReceivePacket(Ptr<Socket> socket) {
    Address from;
    Ptr<Packet> packet = socket->RecvFrom(from);
    NS_LOG_INFO("Received packet from " << from << " with size " << packet->GetSize());
}

void Experiment::SetupPacketReceptionSocket(Ptr<Node> node) {
    TypeId tid = TypeId::LookupByName("ns3::PacketSocketFactory");
    Ptr<Socket> socket = Socket::CreateSocket(node, tid);
    InetSocketAddress local;
    local.SetPort(12345);
    socket->Bind(local);
    socket->SetRecvCallback(MakeCallback(&Experiment::ReceivePacket, this));
}

void Experiment::Run(std::string wifiRate, std::string rtsCtsThreshold, std::string fragmentationThreshold, std::string managerName, std::string experimentName) {
    currentExperimentName = experimentName;
    nodes.Create(2);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("adhoc-network");
    mac.SetType("ns3::AdhocWifiMac",
                "Ssid", SsidValue(ssid));

    devices = wifi.Install(phy, mac, nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    interfaces = address.Assign(devices);

    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        Ptr<Node> node = nodes.Get(i);
        Ptr<ConstantPositionMobilityModel> mobility = CreateObject<ConstantPositionMobilityModel>();
        node->AggregateObject(mobility);
        SetPosition(i, Vector(i * 10.0, 0.0, 0.0));
        mobility->SetPosition(GetPosition(i));
        SetupPacketReceptionSocket(node);
    }

    OnOffHelper onOffHelper("ns3::UdpSocketFactory", Address(InetSocketAddress(interfaces.GetAddress(1), 12345)));
    onOffHelper.SetConstantRate(DataRate("500kbps"));
    appContainer = onOffHelper.Install(nodes.Get(0));
    appContainer.Start(Seconds(1.0));
    appContainer.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    Gnuplot2dDataset dataset;
    dataset.SetTitle("Throughput vs Time");
    dataset.SetStyle(Gnuplot2dDataset::LINES);

    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
      double x = Simulator::Now().GetSeconds();
      double y = i;
      dataset.Add(x,y);
    }

    Gnuplot plot(experimentName + ".png");
    plot.AddDataset(dataset);
    plot.GenerateOutput();

    Simulator::Destroy();
}

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    std::vector<std::string> wifiRates = {"DsssRate1Mbps", "DsssRate2Mbps"};
    std::vector<std::string> rtsCtsThresholds = {"0", "2347"};
    std::vector<std::string> fragmentationThresholds = {"2346", "2347"};
    std::vector<std::string> managerNames = {"AarfWifiManager", "AmrrWifiManager"};

    for (const auto& wifiRate : wifiRates) {
        for (const auto& rtsCtsThreshold : rtsCtsThresholds) {
            for (const auto& fragmentationThreshold : fragmentationThresholds) {
                 for(const auto& managerName : managerNames) {
                        std::string experimentName = "wifi_" + wifiRate + "_rts_" + rtsCtsThreshold + "_frag_" + fragmentationThreshold + "_" + managerName;
                        Experiment experiment;
                        experiment.Run(wifiRate, rtsCtsThreshold, fragmentationThreshold, managerName, experimentName);
                 }
            }
        }
    }
    return 0;
}