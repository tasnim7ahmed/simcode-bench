#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/packet-socket-helper.h"
#include "ns3/packet-socket-address.h"
#include "ns3/onoff-application.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Experiment");

class Experiment {
public:
    Experiment();
    ~Experiment();

    void SetPosition(Ptr<Node> node, Vector position);
    Vector GetPosition(Ptr<Node> node);
    void MoveNode(Ptr<Node> node, Vector destination, double speed);
    void ReceivePacket(Ptr<Socket> socket);
    void SetupPacketReception(Ptr<Node> node);

    void Run(std::string rate, std::string managerType, std::string fileNamePrefix);

private:
    Gnuplot2dDataset m_throughputDataset;
};

Experiment::Experiment() {
    m_throughputDataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);
}

Experiment::~Experiment() {}

void Experiment::SetPosition(Ptr<Node> node, Vector position) {
    Ptr<MobilityModel> mobility = node->GetObject<MobilityModel>();
    if (mobility) {
        mobility->SetPosition(position);
    }
}

Vector Experiment::GetPosition(Ptr<Node> node) {
    Ptr<MobilityModel> mobility = node->GetObject<MobilityModel>();
    return mobility ? mobility->GetPosition() : Vector();
}

void Experiment::MoveNode(Ptr<Node> node, Vector destination, double speed) {
    Ptr<MobilityModel> mobility = node->GetObject<ConstantVelocityMobilityModel>();
    if (!mobility) {
        mobility = CreateObject<ConstantVelocityMobilityModel>();
        node->AggregateObject(mobility);
    }

    Vector currentPosition = GetPosition(node);
    Vector velocityVector = destination - currentPosition;
    double distance = velocityVector.GetLength();
    if (distance > 0) {
        velocityVector /= distance;
        velocityVector *= speed;
        mobility->SetAttribute("Velocity", VectorValue(velocityVector));
    }
}

void Experiment::ReceivePacket(Ptr<Socket> socket) {
    Ptr<Packet> packet;
    while ((packet = socket->Recv())) {
        NS_LOG_INFO("Received packet of size " << packet->GetSize());
    }
}

void Experiment::SetupPacketReception(Ptr<Node> node) {
    PacketSocketHelper packetSocketHelper;
    packetSocketHelper.Install(node);

    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    PacketSocketAddress address(Ipv4Address::GetAny(), 80, 0);

    Ptr<Socket> recvSocket = Socket::CreateSocket(node, tid);
    recvSocket->Bind(address);
    recvSocket->SetRecvCallback(MakeCallback(&Experiment::ReceivePacket, this));
}

void Experiment::Run(std::string rate, std::string managerType, std::string fileNamePrefix) {
    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(9000));

    if (managerType == "Aarf") {
        Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue(rate));
        Config::SetDefault("ns3::WifiRemoteStationManager::DefaultTxMode", StringValue(rate));
    } else if (managerType == "Minstrel") {
        Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue(rate));
        Config::SetDefault("ns3::WifiRemoteStationManager::DefaultTxMode", StringValue(rate));
        Config::SetDefault("ns3::WifiRemoteStationManager::TypeId", TypeIdValue(ns3::MinstrelWifiManager::GetTypeId()));
    }

    NodeContainer nodes;
    nodes.Create(2);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::" + managerType + "WifiManager");

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
    mac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    SetupPacketReception(nodes.Get(1));

    OnOffHelper onOff("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(1), 80));
    onOff.SetAttribute("DataRate", DataRateValue(DataRate(rate)));
    onOff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onOff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer apps = onOff.Install(nodes.Get(0));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    AsciiTraceHelper asciiTraceHelper;
    wifi.EnableAsciiAll(asciiTraceHelper.CreateFileStream(fileNamePrefix + "-wifi.tr"));

    double simDuration = 10.0;
    Simulator::Stop(Seconds(simDuration));

    double throughputInterval = 1.0;
    uint64_t previousRxBytes = 0;
    Gnuplot gnuplot(fileNamePrefix + "-throughput.plt");
    gnuplot.AddDataset(m_throughputDataset);
    gnuplot.SetTitle("Throughput over Time");
    gnuplot.SetXLabel("Time (s)");
    gnuplot.SetYLabel("Throughput (Mbps)");

    auto checkThroughput = [&]() {
        uint64_t totalRxBytes = 0;
        for (uint32_t i = 0; i < interfaces.GetN(); ++i) {
            totalRxBytes += DynamicCast<UdpServer>(apps.Get(i))->GetReceived();
        }
        double diff = (totalRxBytes - previousRxBytes) * 8.0 / (throughputInterval * 1e6); // Mbps
        m_throughputDataset.Add(Simulator::Now().GetSeconds(), diff);
        previousRxBytes = totalRxBytes;
        Simulator::Schedule(Seconds(throughputInterval), checkThroughput);
    };

    Simulator::Schedule(Seconds(throughputInterval), checkThroughput);

    Simulator::Run();

    std::ofstream pltFile((fileNamePrefix + ".plt").c_str());
    gnuplot.GenerateOutput(pltFile);

    std::ofstream datFile((fileNamePrefix + ".dat").c_str());
    m_throughputDataset.Write(datFile);

    Simulator::Destroy();
}

int main(int argc, char *argv[]) {
    std::vector<std::pair<std::string, std::string>> experiments = {
        {"10000000bps", "Aarf"},
        {"10000000bps", "Minstrel"},
        {"20000000bps", "Aarf"},
        {"20000000bps", "Minstrel"}
    };

    CommandLine cmd;
    cmd.Parse(argc, argv);

    for (const auto &exp : experiments) {
        Experiment experiment;
        std::string rate = exp.first;
        std::string manager = exp.second;
        std::string fileNamePrefix = "experiment-" + rate + "-" + manager;
        experiment.Run(rate, manager, fileNamePrefix);
    }

    return 0;
}