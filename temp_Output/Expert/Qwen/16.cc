#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/propagation-module.h"
#include "ns3/applications-module.h"
#include "ns3/packet-socket-helper.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Experiment");

class Experiment {
public:
    Experiment();
    ~Experiment();

    void SetPosition(Ptr<Node> node, Vector position);
    Vector GetPosition(Ptr<Node> node);
    void Move(Ptr<Node> node, Vector newPosition, Time moveTime);
    void ReceivePacket(Ptr<Socket> socket);
    void SetupPacketReception(Ptr<Node> node);

    void Run(std::string rateManagerType, std::string dataRate, std::string fileNamePrefix);

private:
    NodeContainer nodes;
    Gnuplot2dDataset throughputDataset;
};

Experiment::Experiment() {
    throughputDataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);
}

Experiment::~Experiment() {
    Simulator::Destroy();
}

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

void Experiment::Move(Ptr<Node> node, Vector newPosition, Time moveTime) {
    Simulator::Schedule(moveTime, &MobilityModel::SetPosition, node->GetObject<MobilityModel>(), newPosition);
}

void Experiment::ReceivePacket(Ptr<Socket> socket) {
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(65535, 0, from))) {
        uint32_t size = packet->GetSize();
        static uint64_t totalBytes = 0;
        totalBytes += size;
        double throughput = (totalBytes * 8.0) / (Simulator::Now().GetSeconds());
        throughputDataset.Add(Simulator::Now().GetSeconds(), throughput);
    }
}

void Experiment::SetupPacketReception(Ptr<Node> node) {
    TypeId tid = TypeId::LookupByName("ns3::PacketSocketFactory");
    Ptr<Socket> recvSocket = Socket::CreateSocket(node, tid);
    PacketSocketAddress local;
    local.SetSingleDevice(node->GetDevice(0)->GetIfIndex());
    local.SetProtocol(1);
    recvSocket->Bind(local);
    recvSocket->SetRecvCallback(MakeCallback(&Experiment::ReceivePacket, this));
}

void Experiment::Run(std::string rateManagerType, std::string dataRate, std::string fileNamePrefix) {
    nodes.Create(2);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::" + rateManagerType, "DataMode", StringValue(dataRate), "ControlMode", StringValue(dataRate));

    YansWifiPhyHelper phy;
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
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
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    PacketSocketHelper packetSocket;
    packetSocket.Install(nodes);

    SetupPacketReception(nodes.Get(1));

    OnOffHelper onoff("ns3::PacketSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 0));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate(dataRate)));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer apps = onoff.Install(nodes.Get(0));
    apps.Start(Seconds(0.0));
    apps.Stop(Seconds(10.0));

    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/State/RxOk", MakeCallback([](std::string path, Ptr<const Packet> packet, double snr, WifiTxVector txVector, std::vector<WifiPreamblePart> preambleParts) {
        static uint64_t totalRxBytes = 0;
        totalRxBytes += packet->GetSize();
    }));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    Gnuplot gnuplot(fileNamePrefix + "-throughput.plt");
    gnuplot.SetTitle("Throughput vs Time");
    gnuplot.SetTerminal("png");
    gnuplot.SetOutputFilename((fileNamePrefix + "-throughput.png").c_str());
    gnuplot.AddDataset(throughputDataset);
    std::ofstream plotFile((fileNamePrefix + "-throughput.plt").c_str());
    gnuplot.GenerateOutput(plotFile);
    plotFile.close();

    nodes.Clear();
    throughputDataset = Gnuplot2dDataset();
}

int main(int argc, char *argv[]) {
    std::string rateManagerType = "MinstrelWifiManager";
    std::string dataRate = "OfdmRate6Mbps";

    CommandLine cmd(__FILE__);
    cmd.AddValue("rateManager", "WiFi Rate Control Manager Type", rateManagerType);
    cmd.AddValue("dataRate", "Data Rate for WiFi Transmission", dataRate);
    cmd.Parse(argc, argv);

    Experiment experiment;
    experiment.Run(rateManagerType, dataRate, "experiment-output");

    return 0;
}